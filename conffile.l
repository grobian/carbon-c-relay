%{
#include "allocator.h"
#include "conffile.h"
#include "conffile.tab.h"
#define YYSTYPE ROUTER_YYSTYPE

#define YY_DECL int router_yylex \
	(YYSTYPE *yylval_param, ROUTER_YYLTYPE *llocp, yyscan_t yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
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

%x cl ma re ag se st li qu
%x idcl idma idag idin idli

IDCHR	[a-zA-Z0-9[\](){}|\\^?*$&%<>:/,._+-]

%%

	char string_buf[1024];
	char *strbuf = NULL;

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
<ma,idma>route		{
						BEGIN(ma);
						return crROUTE;
					}
<ma>using			{	
						identstate = ma;
						BEGIN(idma);
						return crUSING;
					}
<ma,idma>send		{
						BEGIN(ma);
						return crSEND;
					}
<ma>to				{
						identstate = idcl;
						BEGIN(idcl);
						return crTO;
					}
<idcl>blackhole			return crBLACKHOLE;
<ma,idcl>stop			return crSTOP;

rewrite				{
						identstate = re;
						BEGIN(idma);
						return crREWRITE;
					}
<re>into			{
						identstate = re;
						BEGIN(idma);
						return crINTO;
					}

aggregate			{
						identstate = idag;
						BEGIN(idag);
						return crAGGREGATE;
					}
<idag>every			{
						BEGIN(ag);
						return crEVERY;
					}
<ag>seconds				return crSECONDS;
<ag>expire				return crEXPIRE;
<ag>after				return crAFTER;
<ag>timestamp			return crTIMESTAMP;
<ag>at					return crAT;
<ag>start				return crSTART;
<ag>middle				return crMIDDLE;
<ag>end					return crEND;
<ag>of					return crOF;
<ag>bucket				return crBUCKET;
<ag>compute				return crCOMPUTE;
<ag>summary				return crSUM;
<ag>sum					return crSUM;
<ag>count				return crCOUNT;
<ag>cnt					return crCOUNT;
<ag>maximum				return crMAX;
<ag>max					return crMAX;
<ag>minimum				return crMIN;
<ag>min					return crMIN;
<ag>average				return crAVERAGE;
<ag>avg					return crAVERAGE;
<ag>median				return crMEDIAN;
<ag>percentile[0-9]* {
						yylval_param->crINTVAL =
							atoi(yytext + strlen("percentile"));
						return crPERCENTILE;
					}
<ag>variance			return crVARIANCE;
<ag>stddev				return crSTDDEV;
<ag>write				return crWRITE;
<ag>to				{
						identstate = ag;
						BEGIN(idag);
						return crTO;
					}
<ag>send			{
						/* reuse the match logic for send to ... stop */
						BEGIN(ma);
						return crSEND;
					}

send				{
						BEGIN(se);
						return crSEND;
					}
<se>statistics		{
						/* reuse the match logic for send to ... stop */
						BEGIN(ma);
						return crSTATISTICS;
					}

statistics			{
						BEGIN(st);
						return crSTATISTICS;
					}
<st>submit				return crSUBMIT;
<st>every				return crEVERY;
<st>seconds				return crSECONDS;
<st>reset				return crRESET;
<st>counters			return crCOUNTERS;
<st>after				return crAFTER;
<st>interval			return crINTERVAL;
<st>prefix				return crPREFIX;
<st>with			{
						identstate = st;
						BEGIN(idag);
						return crWITH;
					}
<st>send			{
						/* reuse the match logic for send to ... stop */
						BEGIN(ma);
						return crSEND;
					}

listen				{
						BEGIN(li);
						return crLISTEN;
					}
<li>linemode		{
						identstate = li;
						BEGIN(idli);
						return crLINEMODE;
					}
<idli>gzip				return crGZIP;
<idli>bzip2				return crBZIP2;
<idli>lzma				return crLZMA;
<idli>ssl				return crSSL;
<li>proto				return crPROTO;
<li>udp				{
						identstate = li;
						BEGIN(idli);
						return crUDP;
					}
<li>tcp				{
						identstate = li;
						BEGIN(idli);
						return crTCP;
					}
<li>unix			{
						identstate = li;
						BEGIN(idli);
						return crUNIX;
					}

include				{
						identstate = INITIAL;
						BEGIN(idin);
						return crINCLUDE;
					}

	/* handle quoted strings */
<idcl,idma,idag,idin,idli>\"	{
						strbuf = string_buf;
						BEGIN(qu);
					}
<qu>{
	\"				{
						if (strbuf == string_buf) {
							router_yyerror(llocp, NULL, rtr, ralloc, palloc,
									"empty string");
							yyterminate();
						}
						BEGIN(identstate);
						*strbuf = '\0';
						yylval_param->crSTRING = ra_strdup(palloc, string_buf);
						return crSTRING;
					}
	\n				{
						router_yyerror(llocp, NULL, rtr, ralloc, palloc,
								"unterminated string");
						yyterminate();
					}
	<<EOF>>			{
						router_yyerror(llocp, NULL, rtr, ralloc, palloc,
								"unterminated string");
						yyterminate();
					}
	\\.  			{
						*strbuf++ = yytext[1];
						if (strbuf == string_buf + sizeof(string_buf)) {
							router_yyerror(llocp, NULL, rtr, ralloc, palloc,
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
										ralloc, palloc, "string too large");
								yyterminate();
							}
						}
					}
}

<*>#.*\n			{
						/* ignore for now
						yylval_param->crCOMMENT = ra_strdup(palloc, yytext);
						return crCOMMENT;
						*/
						yycolumn = 0;
						;
					}

<*>[ \t\r]+			; /* whitespace */
<*>\n+				yycolumn = 0;
<idcl,ag,st>[0-9]+	{
						yylval_param->crINTVAL = atoi(yytext);
						return crINTVAL;
					}
<idcl,idma,idag,idin,idli>{IDCHR}+(\\[ ]{IDCHR}+)*	{
						yylval_param->crSTRING = ra_strdup(palloc, yytext);
						BEGIN(identstate);
						return crSTRING;
					}
<*>;				{ BEGIN(INITIAL); return ';'; }
<*>.				{
						/* feed this back to the parser, it will give a
						 * nice error message */
						yylval_param->crSTRING = ra_strdup(palloc, yytext);
						return crSTRING;
					}

%%
