%{
#include "conffile.h"
#include "conffile.tab.h"
#define YYSTYPE ROUTER_YYSTYPE

#define YY_DECL int router_yylex \
	(YYSTYPE *yylval_param, ROUTER_YYLTYPE *llocp, yyscan_t yyscanner, router *rtr)
#define YY_USER_ACTION llocp->first_line = llocp->last_line = yylineno; \
	llocp->first_column = yycolumn; llocp->last_column = yycolumn+yyleng-1; \
	yycolumn += yyleng;
#define YY_USER_INIT yycolumn = 0; yylineno = 0;

int identstate = 0;
%}

%option nostdinit
%option yylineno
%option noyywrap
%option prefix="router_yy"
%option reentrant bison-bridge

%option noinput
%option nounput

%x cl ma re ag st in qu
%x idcl idma

%%

	char string_buf[1024];
	char *strbuf;

cluster				{
						identstate = cl;
						BEGIN(idcl);
						return crCLUSTER;
					}
<cl>forward			{
						identstate = idcl;
						BEGIN(idcl);
						return crFORWARD;
					}
<cl>any_of			{
						identstate = idcl;
						BEGIN(idcl);
						return crANY_OF;
					}
<cl>failover		{
						identstate = idcl;
						BEGIN(idcl);
						return crFAILOVER;
					}
<idcl>useall			return crUSEALL;
<cl>carbon_ch		{
						identstate = idcl;
						BEGIN(idcl);
						return crCARBON_CH;
					}
<cl>fnv1a_ch		{
						identstate = idcl;
						BEGIN(idcl);
						return crFNV1A_CH;
					}
<cl>jump_fnv1a_ch	{
						identstate = idcl;
						BEGIN(idcl);
						return crJUMP_FNV1A_CH;
					}
<idcl>replication		return crREPLICATION;
<cl>file			{
						identstate = idcl;
						BEGIN(idcl);
						return crFILE;
					}
<idcl>ip				return crIP;
<idcl>proto				return crPROTO;
<idcl>udp				return crUDP;
<idcl>tcp				return crTCP;
<idcl>=					return '=';

match				{
						identstate = idma;
						BEGIN(idma);
						return crMATCH;
					}
<idma>\*				return '*';
<idma>validate		{
						identstate = ma;
						BEGIN(idma);
						return crVALIDATE;
					}
<ma>else				return crELSE;
<ma>log					return crLOG;
<ma>drop				return crDROP;
<idma>send			{
						BEGIN(ma);
						return crSEND;
					}
<ma>to				{
						identstate = idcl;
						BEGIN(idcl);
						return crTO;
					}
<idcl>blackhole			return crBLACKHOLE;
<idcl>stop				return crSTOP;

rewrite				return crREWRITE;
into				return crINTO;

aggregate			return crAGGREGATE;
every				return crEVERY;
seconds				return crSECONDS;
expire				return crEXPIRE;
after				return crAFTER;
timestamp			return crTIMESTAMP;
at					return crAT;
start				return crSTART;
middle				return crMIDDLE;
end					return crEND;
of					return crOF;
bucket				return crBUCKET;
compute				return crCOMPUTE;
summary				return crSUM;
sum					return crSUM;
count				return crCOUNT;
cnt					return crCOUNT;
maximum				return crMAX;
max					return crMAX;
minimum				return crMIN;
min					return crMIN;
average				return crAVERAGE;
avg					return crAVERAGE;
median				return crMEDIAN;
percentile[0-9][0-9]?	return crPERCENTILE;
variance			return crVARIANCE;
stddev				return crSTDDEV;
write				return crWRITE;

statistics			return crSTATISTICS;

include				return crINCLUDE;

	/* handle quoted strings */
<idcl,idma>\"		{
						strbuf = string_buf;
						BEGIN(qu);
					}
<qu>{
	\"				{
						if (strbuf == string_buf) {
							router_yyerror(llocp, NULL, rtr, "empty string");
							yyterminate();
						}
						BEGIN(identstate);
						*strbuf = '\0';
						yylval_param->crSTRING = ra_strdup(rtr, string_buf);
						return crSTRING;
					}
	\n				{
						router_yyerror(llocp, NULL, rtr, "unterminated string");
						yyterminate();
					}
	\\.  			{
						*strbuf++ = yytext[1];
						if (strbuf == string_buf + sizeof(string_buf)) {
							router_yyerror(llocp, NULL, rtr,
									"string too large");
							yyterminate();
						}
					}
	[^\\\n\"]+		{
						char *yptr = yytext;
						while (*yptr) {
							*strbuf++ = *yptr++;
							if (strbuf == string_buf + sizeof(string_buf)) {
								router_yyerror(llocp, NULL, rtr,
										"string too large");
								yyterminate();
							}
						}
					}
}

<*>#.*\n			{
						/* ignore for now
						yylval_param->crCOMMENT = ra_strdup(rtr, yytext);
						return crCOMMENT;
						*/
						yycolumn = 0;
						;
					}

<*>[ \t\r]+			; /* whitespace */
<*>\n+				yycolumn = 0;
<idcl>[0-9]+		{
						yylval_param->crINTVAL = atoi(yytext);
						return crINTVAL;
					}
<idcl,idma>[a-zA-Z0-9[\]:/._+-]+	{
						yylval_param->crSTRING = ra_strdup(rtr, yytext);
						BEGIN(identstate);
						return crSTRING;
					}
<*>;				{ BEGIN(INITIAL); return ';'; }
<*>.				{
						router_yyerror(llocp, NULL, rtr,
								"unexpected character");
						yyterminate();
					}

%%
