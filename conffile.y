%{
#include "conffile.h"
#include "conffile.tab.h"

int router_yylex(ROUTER_YYSTYPE *, ROUTER_YYLTYPE *, void *, router *);
%}

%define api.prefix {router_yy}
%define api.pure full
%locations
%define parse.error verbose
%define api.value.type union
%param {void *s} {router *r}

%token crCLUSTER
%token crFORWARD crANY_OF crFAILOVER crCARBON_CH crFNV1A_CH crJUMP_FNV1A_CH
%token crUSEALL crREPLICATION crPROTO crUDP crTCP crFILE crIP

%token crMATCH
%token crVALIDATE crELSE crLOG crDROP crSEND crTO crSTOP

%token crREWRITE
%token crINTO

%token crAGGREGATE
%token crEVERY crSECONDS crEXPIRE crAFTER crTIMESTAMP crAT
%token crSTART crMIDDLE crEND crOF crBUCKET
%token crCOMPUTE crSUM crCOUNT crMAX crMIN crAVERAGE crMEDIAN
%token crPERCENTILE crVARIANCE crSTDDEV
%token crWRITE

%token crSTATISTICS

%token crINCLUDE

%token <char *> crCOMMENT crSTRING
%token <int> crINTVAL

%%

commands:
		| commands command ';'
		;

command:
	     cluster
/*	   | match
	   | rewrite
	   | aggregate
	   | send
	   | include */
	   ;

cluster: crCLUSTER crSTRING[name] cluster_type cluster_hosts
	   | crCLUSTER crSTRING[name] cluster_file cluster_paths
	   ;

cluster_type:
			  cluster_useall cluster_opt_useall
			| cluster_ch cluster_opt_repl
			;

cluster_useall: crFORWARD  { /*$$ = FORWARD;*/ }
			  | crANY_OF   { /*$$ = ANYOF;*/ }
			  | crFAILOVER { /*$$ = FAILOVER;*/ }
			  ;

cluster_opt_useall:          { /*$$ = 0;*/ }
				  | crUSEALL { /*$$ = 1;*/ }
				  ;

cluster_ch: crCARBON_CH | crFNV1A_CH | crJUMP_FNV1A_CH
		  ;

cluster_opt_repl:
				| crREPLICATION crINTVAL
				;

cluster_file: crFILE cluster_opt_ip
			;

cluster_opt_ip:
			  | crIP
			  ;

cluster_paths: cluster_path cluster_opt_path
			 ;
cluster_opt_path:
				| cluster_path
				;
cluster_path: crSTRING
			;

cluster_hosts: cluster_host cluster_opt_host
			 ;
cluster_opt_host:
				| cluster_host
				;
cluster_host: crSTRING cluster_opt_instance cluster_opt_proto
			;
cluster_opt_instance:
					| '=' crSTRING
					;
cluster_opt_proto:
				 | crPROTO cluster_udp_tcp
				 ;
cluster_udp_tcp: crUDP | crTCP
			   ;
