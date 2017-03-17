%{
#include "allocator.h"
#include "conffile.h"
#include "conffile.tab.h"
#include "aggregator.h"

int router_yylex(ROUTER_YYSTYPE *, ROUTER_YYLTYPE *, void *, router *, allocator *, allocator *);
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
struct _agcomp {
	enum _aggr_compute_type ctype;
	unsigned char pctl;
	char *metric;
	struct _agcomp *next;
};
}

%define api.prefix {router_yy}
%define api.pure full
%locations
%define parse.error verbose
%define api.value.type union
%param {void *yyscanner} {router *rtr} {allocator *ralloc} {allocator *palloc}

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
	match_dsts2 match_opt_send_to match_send_to aggregate_opt_send_to
%type <int> match_opt_stop match_log_or_drop
%type <struct _maexpr *> match_opt_validate match_expr match_opt_expr
	match_exprs match_exprs_subst match_exprs2 match_subst_expr
	match_subst_opt_expr 

%token crREWRITE
%token crINTO

%token crAGGREGATE
%token crEVERY crSECONDS crEXPIRE crAFTER crTIMESTAMP crAT
%token crSTART crMIDDLE crEND crOF crBUCKET
%token crCOMPUTE crSUM crCOUNT crMAX crMIN crAVERAGE crMEDIAN
%token crVARIANCE crSTDDEV
%token <int> crPERCENTILE
%token crWRITE
%type <enum _aggr_timestamp> aggregate_ts_when aggregate_opt_timestamp
%type <struct _agcomp> aggregate_comp_type
%type <struct _agcomp *> aggregate_compute aggregate_computes
	aggregate_opt_compute

%token crSTATISTICS
%token crSUBMIT crRESET crCOUNTERS crINTERVAL crPREFIX crWITH
%type <int> statistics_opt_interval
%type <col_mode> statistics_opt_counters
%type <char *> statistics_opt_prefix

%token crINCLUDE

%token <char *> crCOMMENT crSTRING
%token <int> crINTVAL

%%

stmts: opt_stmt
	 ;

opt_stmt:
		| stmt opt_stmt
		;

stmt: command ';'
	;

command: cluster
	   | match
	   | rewrite
	   | aggregate
	   | send
	   | statistics
	   | include
	   ;

/*** {{{ BEGIN cluster ***/
cluster: crCLUSTER crSTRING[name] cluster_type[type] cluster_hosts[servers]
	   {
	   	struct _clhost *w;
		char *err;

		if (($$ = ra_malloc(ralloc, sizeof(cluster))) == NULL) {
			logerr("malloc failed for cluster '%s'\n", $name);
			YYABORT;
		}
		$$->name = ra_strdup(ralloc, $name);
		$$->next = NULL;
		$$->type = $type.t;
		switch ($$->type) {
			case CARBON_CH:
			case FNV1A_CH:
			case JUMP_CH:
				$$->members.ch = ra_malloc(ralloc, sizeof(chashring));
				if ($$->members.ch == NULL) {
					logerr("malloc failed for ch in cluster '%s'\n", $name);
					YYABORT;
				}
				if ($type.ival < 1 || $type.ival > 255) {
					router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
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
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}
		}

		err = router_add_cluster(rtr, $$);
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
	   }
	   | crCLUSTER crSTRING[name] cluster_file[type] cluster_paths[paths]
	   {
	   	struct _clhost *w;
		char *err;

		if (($$ = ra_malloc(ralloc, sizeof(cluster))) == NULL) {
			logerr("malloc failed for cluster '%s'\n", $name);
			YYABORT;
		}
		$$->name = ra_strdup(ralloc, $name);
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
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}
		}

		err = router_add_cluster(rtr, $$);
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
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
				struct _clhost *ret = ra_malloc(palloc, sizeof(struct _clhost));
				char *err = router_validate_path(rtr, $path);
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
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
			  	struct _clhost *ret = ra_malloc(palloc, sizeof(struct _clhost));
				char *err = router_validate_address(
						rtr,
						&(ret->ip), &(ret->port), &(ret->saddr), &(ret->hint),
						$ip, $prot);
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
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
match: crMATCH match_exprs[exprs] match_opt_validate[val]
	 match_opt_send_to[dsts] match_opt_stop[stop]
	 {
	 	/* each expr comes with an allocated route, populate it */
		struct _maexpr *we;
		destinations *d = NULL;
		char *err;

		if ($val != NULL) {
			/* optional validate clause */
			if ((d = ra_malloc(ralloc, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			d->next = NULL;
			if ((d->cl = ra_malloc(ralloc, sizeof(cluster))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			d->cl->name = NULL;
			d->cl->type = VALIDATION;
			d->cl->next = NULL;
			d->cl->members.validation = ra_malloc(ralloc, sizeof(validate));
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
			we->r->stop = $dsts == NULL ? 0 :
					$dsts->cl->type == BLACKHOLE ? 1 : $stop;
			err = router_add_route(rtr, we->r);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
		}
	 }
	 ;

match_exprs: '*'
		   {
			if (($$ = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	$$->r = NULL;
			if (router_validate_expression(rtr, &($$->r), "*", 0) != NULL)
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
			if (($$ = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	$$->r = NULL;
		  	err = router_validate_expression(rtr, &($$->r), $expr, 0);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
			$$->drop = 0;
			$$->next = NULL;
		  }
		  ;

match_exprs_subst: match_subst_expr[l] match_subst_opt_expr[r]
				 { $l->next = $r; $$ = $l; }

match_subst_opt_expr:                   { $$ = NULL; }
					| match_exprs_subst { $$ = $1; }
					;

match_subst_expr: crSTRING[expr]
		  {
			char *err;
			if (($$ = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	$$->r = NULL;
		  	err = router_validate_expression(rtr, &($$->r), $expr, 1);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
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
					if (($$ = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
						logerr("out of memory\n");
						YYABORT;
					}
					$$->r = NULL;
					err = router_validate_expression(rtr, &($$->r), $expr, 0);
					if (err != NULL) {
						router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc, err);
						YYERROR;
					}
					$$->drop = $drop;
					$$->next = NULL;
				  }
				  ;

match_log_or_drop: crLOG  { $$ = 0; }
				 | crDROP { $$ = 1; }
				 ;

match_opt_send_to: { $$ = NULL; }
				 | match_send_to { $$ = $1; }
				 ;

match_send_to: crSEND crTO match_dsts[dsts] { $$ = $dsts; }
			 ;

match_dsts: crBLACKHOLE
		  {
			if (($$ = ra_malloc(ralloc, sizeof(destinations))) == NULL) {
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
			if (($$ = ra_malloc(ralloc, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			err = router_validate_cluster(rtr, &($$->cl), $1);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
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

		err = router_validate_expression(rtr, &r, $expr, 1);
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
		
		cl = ra_malloc(ralloc, sizeof(cluster));
		if (cl == NULL) {
			logerr("out of memory\n");
			YYABORT;
		}
		cl->type = REWRITE;
		cl->name = NULL;
		cl->members.replacement = ra_strdup(ralloc, $replacement);
		cl->next = NULL;
		if (cl->members.replacement == NULL) {
			logerr("out of memory\n");
			YYABORT;
		}
		r->dests = ra_malloc(ralloc, sizeof(destinations));
		if (r->dests == NULL) {
			logerr("out of memory\n");
			YYABORT;
		}
		r->dests->cl = cl;
		r->dests->next = NULL;
		r->stop = 0;
		r->next = NULL;

		err = router_add_route(rtr, r);
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
	   }
	   ;
/*** }}} END rewrite ***/

/*** {{{ BEGIN aggregate ***/
aggregate: crAGGREGATE match_exprs_subst[exprs] crEVERY crINTVAL[intv] crSECONDS
		 crEXPIRE crAFTER crINTVAL[expire] crSECONDS
		 aggregate_opt_timestamp[tswhen]
		 aggregate_computes[computes]
		 aggregate_opt_send_to[dsts]
		 match_opt_stop[stop]
		 {
		 	cluster *w;
			aggregator *a;
			destinations *d;
			struct _agcomp *acw;
			struct _maexpr *we;
			char *err;

			if ($intv <= 0) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
					"interval must be > 0");
				YYERROR;
			}
			if ($expire <= 0) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
					"expire must be > 0");
				YYERROR;
			}
			if ($expire < $intv) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
					"expire must be greater than interval");
				YYERROR;
			}

			w = ra_malloc(ralloc, sizeof(cluster));
			if (w == NULL) {
				logerr("malloc failed for aggregate\n");
				YYABORT;
			}
			w->name = NULL;
			w->type = AGGREGATION;
			w->next = NULL;

			a = aggregator_new($intv, $expire, $tswhen);
			if (a == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			if ((err = router_add_aggregator(rtr, a)) != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}

			w->members.aggregation = a;
			
			for (acw = $computes; acw != NULL; acw = acw->next) {
				if (aggregator_add_compute(a,
							acw->metric, acw->ctype, acw->pctl) != 0)
				{
					logerr("out of memory\n");
					YYABORT;
				}
			}

			err = router_add_cluster(rtr, w);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}

			d = ra_malloc(ralloc, sizeof(destinations));
			if (d == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			d->cl = w;
			d->next = $dsts;

			for (we = $exprs; we != NULL; we = we->next) {
				we->r->next = NULL;
				we->r->dests = d;
				we->r->stop = $stop;
				err = router_add_route(rtr, we->r);
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
			}

			if ($dsts != NULL)
				router_add_stubroute(rtr, AGGRSTUB, w, $dsts);
		 }
		 ;

aggregate_opt_timestamp: { $$ = TS_END; }
					   | crTIMESTAMP crAT aggregate_ts_when[tswhen] crOF
					   crBUCKET
					   { $$ = $tswhen; }
					   ;

aggregate_ts_when: crSTART  { $$ = TS_START; }
				 | crMIDDLE { $$ = TS_MIDDLE; }
				 | crEND    { $$ = TS_END; }
				 ;

aggregate_computes: aggregate_compute[l] aggregate_opt_compute[r]
				  { $l->next = $r; $$ = $l; }
				  ;

aggregate_opt_compute:                    { $$ = NULL; }
					 | aggregate_computes { $$ = $1; }

aggregate_compute: crCOMPUTE aggregate_comp_type[type] crWRITE crTO
				 crSTRING[metric]
				 {
					$$ = ra_malloc(palloc, sizeof(struct _agcomp));
					if ($$ == NULL) {
						logerr("malloc failed\n");
						YYABORT;
					}
				 	$$->ctype = $type.ctype;
					$$->pctl = $type.pctl;
					$$->metric = $metric;
					$$->next = NULL;
				 }
				 ;

aggregate_comp_type: crSUM        { $$.ctype = SUM; }
				   | crCOUNT      { $$.ctype = CNT; }
				   | crMAX        { $$.ctype = MAX; }
				   | crMIN        { $$.ctype = MIN; }
				   | crAVERAGE    { $$.ctype = AVG; }
				   | crMEDIAN     { $$.ctype = MEDN; }
				   | crPERCENTILE
				   {
				    if ($1 < 1 || $1 > 99) {
						router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
							"percentile<x>: value x must be between 1 and 99");
						YYERROR;
					}
				   	$$.ctype = PCTL;
					$$.pctl = (unsigned char)$1;
				   }
				   | crVARIANCE   { $$.ctype = VAR; }
				   | crSTDDEV     { $$.ctype = SDEV; }
				   ;

aggregate_opt_send_to:               { $$ = NULL; }
					 | match_send_to { $$ = $1; }
					 ;
/*** }}} END aggregate ***/

/*** {{{ BEGIN send ***/
send: crSEND crSTATISTICS crTO match_dsts[dsts] match_opt_stop[stop]
	{
		char *err = router_set_statistics(rtr, $dsts);
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
		logerr("warning: 'send statistics to ...' is deprecated and will be "
				"removed in a future version, use 'statistics send to ...' "
				"instead\n");
	}
	;
/*** }}} END send ***/

/*** {{{ BEGIN statistics ***/
statistics: crSTATISTICS
		  statistics_opt_interval[interval]
		  statistics_opt_counters[counters]
		  statistics_opt_prefix[prefix]
		  aggregate_opt_send_to[dsts]
		  match_opt_stop[stop]
		  {
		  	char *err;
		  	err = router_set_collectorvals(rtr, $interval, $prefix, $counters);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}

			if ($dsts != NULL) {
				err = router_set_statistics(rtr, $dsts);
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
			}
		  }
		  ;

statistics_opt_interval: { $$ = 0; }
					   | crSUBMIT crEVERY crINTVAL[intv] crSECONDS
					   {
					   	if ($intv <= 0) {
							router_yyerror(&yylloc, yyscanner, rtr,
									ralloc, palloc, "interval must be > 0");
							YYERROR;
						}
						$$ = $intv;
					   }
					   ;

statistics_opt_counters:                                       { $$ = CUM; }
					   | crRESET crCOUNTERS crAFTER crINTERVAL { $$ = SUB; }
					   ;

statistics_opt_prefix:                                  { $$ = NULL; }
					 | crPREFIX crWITH crSTRING[prefix] { $$ = $prefix; }
					 ;
/*** }}} END statistics ***/

/*** {{{ BEGIN include ***/
include: crINCLUDE crSTRING[path]
	   {
	   	router_readconfig(rtr, $path, 0, 0, 0, 0, 0);
	   }
	   ;
/*** }}} END include ***/

/* vim: set ts=4 sw=4 foldmethod=marker: */
