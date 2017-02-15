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
%}

%option nostdinit
%option yylineno
%option noyywrap
%option prefix="router_yy"
%option reentrant bison-bridge

%option noinput
%option nounput

%x cl ma re ag st in qu

%%

	char string_buf[1024];
	char *strbuf;
	int strretstate;

cluster				{ BEGIN(cl); return crCLUSTER; }
<cl>forward				return crFORWARD;
<cl>any_of				return crANY_OF;
<cl>failover			return crFAILOVER;
<cl>carbon_ch			return crCARBON_CH;
<cl>fnv1a_ch			return crFNV1A_CH;
<cl>jump_fnv1a_ch		return crJUMP_FNV1A_CH;
<cl>useall				return crUSEALL;
<cl>replication			return crREPLICATION;
<cl>proto				return crPROTO;
<cl>udp					return crUDP;
<cl>tcp					return crTCP;
<cl>file				return crFILE;
<cl>ip					return crIP;
<cl>=					return '=';

match				return crMATCH;
validate			return crVALIDATE;
else				return crELSE;
log					return crLOG;
drop				return crDROP;
send				return crSEND;
to					return crTO;
stop				return crSTOP;

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
<INITIAL,cl>\"					{
						strbuf = string_buf;
						strretstate = yyg->yy_start;
						BEGIN(qu);
					}
<qu>{
	\"				{
						if (strbuf == string_buf) {
							router_yyerror(llocp, NULL, rtr, "empty string");
							yyterminate();
						}
						BEGIN(strretstate);
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
						;
					}

<*>[ \t\r\n]+		; /* whitespace */
[0-9]+				{
						yylval_param->crINTVAL = atoi(yytext);
						return crINTVAL;
					}
<*>[a-zA-Z0-9:/._+-]+	{
						yylval_param->crSTRING = ra_strdup(rtr, yytext);
						return crSTRING;
					}
<*>;				{ BEGIN(INITIAL); return ';'; }
<*>.				{
						router_yyerror(llocp, NULL, rtr,
								"unexpected character");
						yyterminate();
					}

%%
