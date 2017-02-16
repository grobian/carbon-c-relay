%{
#include "conffile.h"
#include "conffile.tab.h"

int router_yylex(ROUTER_YYSTYPE *, ROUTER_YYLTYPE *, void *, router *);
%}
%code requires {
struct _clust {
	enum clusttype t;
	int ival;
};
struct _clhost {
	char *ip;
	int port;
	char *inst;
	int proto;
	void *saddr;
	void *hint;
	struct _clhost *next;
};
}

%define api.prefix {router_yy}
%define api.pure full
%locations
%define parse.error verbose
%define api.value.type union
%param {void *yyscanner} {router *rtr}

%token crCLUSTER
%token crFORWARD crANY_OF crFAILOVER crCARBON_CH crFNV1A_CH crJUMP_FNV1A_CH
	crFILE crIP crREPLICATION crPROTO crUSEALL crUDP crTCP
%type <enum clusttype> cluster_useall cluster_ch cluster_file
%type <struct _clust> cluster_type
%type <int> cluster_opt_repl cluster_opt_useall
%type <serv_ctype> cluster_opt_proto
%type <char *> cluster_opt_instance
%type <cluster *> cluster
%type <struct _clhost *> cluster_host cluster_hosts cluster_opt_host

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

cluster: crCLUSTER crSTRING[name] cluster_type[type] cluster_hosts[servers]
	   {
	   	struct _clhost *w;
		char *err;

		if (($$ = ra_malloc(rtr, sizeof(cluster))) == NULL) {
			logerr("malloc failed for cluster '%s'\n", $name);
			YYABORT;
		}
		$$->name = ra_strdup(rtr, $name);
		$$->next = NULL;
		$$->type = $type.t;
		switch ($$->type) {
			case CARBON_CH:
			case FNV1A_CH:
			case JUMP_CH:
				$$->members.ch = ra_malloc(rtr, sizeof(chashring));
				if ($$->members.ch == NULL) {
					logerr("malloc failed for ch in cluster '%s'\n", $name);
					YYABORT;
				}
				if ($type.ival < 1 || $type.ival > 255) {
					router_yyerror(&yylloc, yyscanner, rtr,
						"replication count must be between 1 and 255");
					YYERROR;
				}
				$$->members.ch->repl_factor = (unsigned char)$type.ival;
				$$->members.ch->ring = ch_new(
					$$->type == CARBON_CH ? CARBON :
					$$->type == FNV1A_CH ? FNV1a :
					JUMP_FNV1a);
				break;
			case FORWARD:
				$$->members.forward = NULL;
				break;
			case ANYOF:
			case FAILOVER:
				$$->members.anyof = NULL;
				break;
			default:
				logerr("unknown cluster type %zd!\n", $$->type);
				YYABORT;
		}
		
		for (w = $servers; w != NULL; w = w->next) {
			err = router_add_server(rtr, w->ip, w->port, w->inst,
					w->proto, w->saddr, w->hint, $type.ival, $$);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, err);
				YYERROR;
			}
		}

		err = router_add_cluster(rtr, $$);
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, err);
			YYERROR;
		}
	   }
	   | crCLUSTER crSTRING[name] cluster_file[type] cluster_paths[paths]
	   ;

cluster_type:
			  cluster_useall[type] cluster_opt_useall[use]
			  { $$.t = $type; $$.ival = $use; }
			| cluster_ch[type] cluster_opt_repl[repl]
			  { $$.t = $type; $$.ival = $repl; }
			;

cluster_useall: crFORWARD  { $$ = FORWARD; }
			  | crANY_OF   { $$ = ANYOF; }
			  | crFAILOVER { $$ = FAILOVER; }
			  ;

cluster_opt_useall:          { $$ = 0; }
				  | crUSEALL { $$ = 1; }
				  ;

cluster_ch: crCARBON_CH     { $$ = CARBON_CH; }
		  | crFNV1A_CH      { $$ = FNV1A_CH; }
		  | crJUMP_FNV1A_CH { $$ = JUMP_CH; }
		  ;

cluster_opt_repl:                             { $$ = 1; }
				| crREPLICATION crINTVAL[cnt] { $$ = $cnt; }
				;

cluster_file: crFILE crIP { $$ = FILELOGIP; }
			| crFILE      { $$ = FILELOG; }
			;

cluster_paths: cluster_path cluster_opt_path
			 ;
cluster_opt_path:
				| cluster_path
				;
cluster_path: crSTRING
			;

cluster_hosts: cluster_host[l] cluster_opt_host[r] { $l->next = $r; $$ = $l; }
			 ;
cluster_opt_host:               { $$ = NULL; }
				| cluster_hosts { $$ = $1; }
				;
cluster_host: crSTRING[ip] cluster_opt_instance[inst] cluster_opt_proto[prot]
			  {
			  	struct _clhost *ret = ra_malloc(rtr, sizeof(struct _clhost));
				char *err = router_validate_address(
						rtr,
						&(ret->ip), &(ret->port), &(ret->saddr), &(ret->hint),
						$ip, $prot);
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr, err);
					YYERROR;
				}
				ret->inst = $inst;
				ret->proto = $prot;
				ret->next = NULL;
				$$ = ret;
			  }
			;
cluster_opt_instance:                    { $$ = NULL; }
					| '=' crSTRING[inst] { $$ = $inst; }
					;
cluster_opt_proto:               { $$ = CON_TCP; }
				 | crPROTO crUDP { $$ = CON_UDP; }
				 | crPROTO crTCP { $$ = CON_TCP; }
				 ;
