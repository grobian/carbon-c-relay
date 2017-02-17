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
struct _maexpr {
	route *r;
	char drop;
	struct _maexpr *next;
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
%type <enum clusttype> cluster_useall cluster_ch
%type <struct _clust> cluster_type cluster_file
%type <int> cluster_opt_repl cluster_opt_useall
%type <serv_ctype> cluster_opt_proto
%type <char *> cluster_opt_instance
%type <cluster *> cluster
%type <struct _clhost *> cluster_host cluster_hosts cluster_opt_host
	cluster_path cluster_paths cluster_opt_path

%token crMATCH
%token crVALIDATE crELSE crLOG crDROP crSEND crTO crBLACKHOLE crSTOP
%type <destinations *> match_dst match_opt_dst match_dsts
	match_dsts2 match_send_to
%type <int> match_opt_stop match_log_or_drop
%type <struct _maexpr *> match_opt_validate match_expr match_opt_expr
	match_exprs match_exprs2

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

stmts: stmt opt_stmt
	 ;

opt_stmt:
		| stmts
		;

stmt: command ';'
	;

command: cluster
	   | match
	   | rewrite /*
	   | aggregate
	   | send
	   | include */
	   ;

/*** {{{ BEGIN cluster ***/
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
			case FILELOG:
			case FILELOGIP:
				$$->members.forward = NULL;
				break;
			default:
				logerr("unknown cluster type %zd!\n", $$->type);
				YYABORT;
		}
		
		for (w = $paths; w != NULL; w = w->next) {
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

cluster_file: crFILE crIP { $$.t = FILELOGIP; $$.ival = 0; }
			| crFILE      { $$.t = FILELOG; $$.ival = 0; }
			;

cluster_paths: cluster_path[l] cluster_opt_path[r] { $l->next = $r; $$ = $l; }
			 ;
cluster_opt_path:              { $$ = NULL; }
				| cluster_path { $$ = $1; }
				;
cluster_path: crSTRING[path]
			{
				struct _clhost *ret = ra_malloc(rtr, sizeof(struct _clhost));
				char *err = router_validate_path(rtr, $path);
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr, err);
					YYERROR;
				}
				ret->ip = $path;
				ret->port = 2003;
				ret->saddr = NULL;
				ret->hint = NULL;
				ret->inst = NULL;
				ret->proto = CON_FILE;
				ret->next = NULL;
				$$ = ret;
			}
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
/*** }}} END cluster ***/

/*** {{{ BEGIN match ***/
match: crMATCH match_exprs[exprs] match_opt_validate[val] match_send_to[dsts]
	 match_opt_stop[stop]
	 {
	 	/* each expr comes with an allocated route, populate it */
		struct _maexpr *we;
		destinations *d = NULL;
		char *err;

		if ($val != NULL) {
			/* optional validate clause */
			if ((d = ra_malloc(rtr, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			d->next = NULL;
			if ((d->cl = ra_malloc(rtr, sizeof(cluster))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			d->cl->name = NULL;
			d->cl->type = VALIDATION;
			d->cl->next = NULL;
			d->cl->members.validation = ra_malloc(rtr, sizeof(validate));
			if (d->cl->members.validation == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			d->cl->members.validation->rule = $val->r;
			d->cl->members.validation->action = $val->drop ? VAL_DROP : VAL_LOG;
		}
		/* add destinations to the chain */
		if (d != NULL) {
			d->next = $dsts;
		} else {
			d = $dsts;
		}
		for (we = $exprs; we != NULL; we = we->next) {
			we->r->next = NULL;
			we->r->dests = d;
			we->r->stop = $dsts->cl->type == BLACKHOLE ? 1 : $stop;
			err = router_add_route(rtr, we->r);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, err);
				YYERROR;
			}
		}
	 }
	 ;

match_exprs: '*'
		   {
			if (($$ = ra_malloc(rtr, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	$$->r = NULL;
			if (router_validate_expression(rtr, &($$->r), "*") != NULL)
				YYABORT;
			$$->drop = 0;
			$$->next = NULL;
		   }
		   | match_exprs2 { $$ = $1; }
		   ;

match_exprs2: match_expr[l] match_opt_expr[r] { $l->next = $r; $$ = $l; }

match_opt_expr:              { $$ = NULL; }
			  | match_exprs2 { $$ = $1; }
			  ;

match_expr: crSTRING[expr]
		  {
			char *err;
			if (($$ = ra_malloc(rtr, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	$$->r = NULL;
		  	err = router_validate_expression(rtr, &($$->r), $expr);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, err);
				YYERROR;
			}
			$$->drop = 0;
			$$->next = NULL;
		  }
		  ;

match_opt_validate: { $$ = NULL; }
				  | crVALIDATE crSTRING[expr] crELSE match_log_or_drop[drop]
				  {
					char *err;
					if (($$ = ra_malloc(rtr, sizeof(struct _maexpr))) == NULL) {
						logerr("out of memory\n");
						YYABORT;
					}
					$$->r = NULL;
					err = router_validate_expression(rtr, &($$->r), $expr);
					if (err != NULL) {
						router_yyerror(&yylloc, yyscanner, rtr, err);
						YYERROR;
					}
					$$->drop = $drop;
					$$->next = NULL;
				  }
				  ;

match_log_or_drop: crLOG  { $$ = 0; }
				 | crDROP { $$ = 1; }
				 ;

match_send_to: crSEND crTO match_dsts[dsts] { $$ = $dsts; }
			 ;

match_dsts: crBLACKHOLE
		  {
			if (($$ = ra_malloc(rtr, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			if (router_validate_cluster(rtr, &($$->cl), "blackhole") != NULL)
				YYABORT;
			$$->next = NULL;
		  }
		  | match_dsts2 { $$ = $1; }
		  ;

match_dsts2: match_dst[l] match_opt_dst[r] { $l->next = $r; $$ = $l; }

match_opt_dst:             { $$ = NULL; }
			 | match_dsts2 { $$ = $1; }
			 ;

match_dst: crSTRING[cluster]
		 {
			char *err;
			if (($$ = ra_malloc(rtr, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			err = router_validate_cluster(rtr, &($$->cl), $1);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, err);
				YYERROR;
			}
			$$->next = NULL;
		 }
		 ;

match_opt_stop:        { $$ = 0; }
			  | crSTOP { $$ = 1; }
			  ;
/*** }}} END match ***/

/*** {{{ BEGIN rewrite ***/
rewrite: crREWRITE crSTRING[expr] crINTO crSTRING[replacement]
	   {
		char *err;
		route *r = NULL;
		cluster *cl;

		err = router_validate_expression(rtr, &r, $expr);
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, err);
			YYERROR;
		}
		
		cl = ra_malloc(rtr, sizeof(cluster));
		if (cl == NULL) {
			logerr("out of memory\n");
			YYABORT;
		}
		cl->type = REWRITE;
		cl->name = NULL;
		cl->members.replacement = ra_strdup(rtr, $replacement);
		cl->next = NULL;
		if (cl->members.replacement == NULL) {
			logerr("out of memory");
			YYABORT;
		}
		r->dests = ra_malloc(rtr, sizeof(destinations));
		if (r->dests == NULL) {
			logerr("out of memory");
			YYABORT;
		}
		r->dests->cl = cl;
		r->dests->next = NULL;
		r->stop = 0;
		r->next = NULL;

		err = router_add_route(rtr, r);
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, err);
			YYERROR;
		}
	   }
	   ;
/*** }}} END rewrite ***/

/* vim: set ts=4 sw=4 foldmethod=marker: */
