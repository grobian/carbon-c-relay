%{
#include "allocator.h"
#include "conffile.h"
#include "conffile.tab.h"
#include "aggregator.h"
#include "receptor.h"
#include "config.h"

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
	con_trnsp trnsp;
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
struct _lsnr {
	con_type type;
	struct _rcptr_trsp *transport;
	struct _rcptr *rcptr;
};
struct _rcptr {
	con_proto ctype;
	char *ip;
	int port;
	void *saddr;
	struct _rcptr *next;
};
struct _rcptr_trsp {
	con_trnsp mode;
	char *pemcert;
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
%type <con_proto> cluster_opt_proto
%type <char *> cluster_opt_instance
%type <cluster *> cluster
%type <struct _clhost *> cluster_host cluster_hosts cluster_opt_host
	cluster_path cluster_paths cluster_opt_path
%type <con_trnsp> cluster_opt_transport

%token crMATCH
%token crVALIDATE crELSE crLOG crDROP crROUTE crUSING
%token crSEND crTO crBLACKHOLE crSTOP
%type <destinations *> match_dst match_opt_dst match_dsts
	match_dsts2 match_opt_send_to match_send_to aggregate_opt_send_to
%type <int> match_opt_stop match_log_or_drop
%type <char *> match_opt_route
%type <struct _maexpr *> match_opt_validate match_expr match_opt_expr
	match_exprs match_exprs2

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

%token crLISTEN
%token crTYPE crLINEMODE crTRANSPORT crGZIP crLZ4 crSSL crUNIX
%type <con_proto> rcptr_proto
%type <struct _rcptr *> receptor opt_receptor receptors
%type <struct _rcptr_trsp *> transport_mode
%type <struct _lsnr *> listener

%token crINCLUDE

%token <char *> crCOMMENT crSTRING crUNEXPECTED
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
	   | listen
	   | include
	   ;

/*** {{{ BEGIN cluster ***/
cluster: crCLUSTER crSTRING[name] cluster_type[type] cluster_hosts[servers]
	   {
	   	struct _clhost *w;
		char *err;
		int srvcnt;

		/* count number of servers for ch_new */
		for (srvcnt = 0, w = $servers; w != NULL; w = w->next, srvcnt++)
			;

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
				$$->members.ch->ring = ch_new(ralloc,
					$$->type == CARBON_CH ? CARBON :
					$$->type == FNV1A_CH ? FNV1a :
					JUMP_FNV1a, srvcnt);
				$$->members.ch->servers = NULL;
				$type.ival = 0;  /* hack, avoid triggering use_all */
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
					T_LINEMODE, w->trnsp, w->proto,
					w->saddr, w->hint, $type.ival, $$);
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
					T_LINEMODE, W_PLAIN, w->proto,
					w->saddr, w->hint, $type.ival, $$);
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
				ret->port = GRAPHITE_PORT;
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
cluster_host: crSTRING[ip] cluster_opt_instance[inst]
			  cluster_opt_proto[prot] cluster_opt_type[type]
			  cluster_opt_transport[trnsp]
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
				ret->trnsp = $trnsp;
				ret->next = NULL;
				$$ = ret;
			  }
			;
cluster_opt_instance:                    { $$ = NULL; }
					| '=' crSTRING[inst] { $$ = $inst; }
					| '=' crINTVAL[inst]
					{
						$$ = ra_malloc(palloc, sizeof(char) * 12);
						if ($$ == NULL) {
							logerr("out of memory\n");
							YYABORT;
						}
						snprintf($$, 12, "%d", $inst);
					}
					;
cluster_opt_proto:               { $$ = CON_TCP; }
				 | crPROTO crUDP { $$ = CON_UDP; }
				 | crPROTO crTCP { $$ = CON_TCP; }
				 ;

cluster_opt_type:
				| crTYPE crLINEMODE
				;

cluster_opt_transport:                      { $$ = W_PLAIN; }
					 | crTRANSPORT crGZIP   {
#ifdef HAVE_GZIP
							$$ = W_GZIP;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature gzip not compiled in");
							YYERROR;
#endif
					 }
					 | crTRANSPORT crLZ4    {
#ifdef HAVE_LZ4
							$$ = W_LZ4;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature lz4 not compiled in");
							YYERROR;
#endif
					 }
					 | crTRANSPORT crSSL    {
#ifdef HAVE_SSL
							$$ = W_SSL;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature ssl not compiled in");
							YYERROR;
#endif
					 }
					 ;
/*** }}} END cluster ***/

/*** {{{ BEGIN match ***/
match: crMATCH match_exprs[exprs] match_opt_validate[val]
	 match_opt_route[masq] match_opt_send_to[dsts] match_opt_stop[stop]
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
		/* replace with copy on the router allocator */
		if ($masq != NULL)
			$masq = ra_strdup(ralloc, $masq);
		for (we = $exprs; we != NULL; we = we->next) {
			we->r->next = NULL;
			we->r->dests = d;
			we->r->masq = $masq;
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
			if (($$ = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	$$->r = NULL;
		  	err = router_validate_expression(rtr, &($$->r), $expr);
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
					err = router_validate_expression(rtr, &($$->r), $expr);
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

match_opt_route: { $$ = NULL; }
			   | crROUTE crUSING crSTRING[expr] { $$ = $expr; }
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

		err = router_validate_expression(rtr, &r, $expr);
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
aggregate: crAGGREGATE match_exprs2[exprs] crEVERY crINTVAL[intv] crSECONDS
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

statistics_opt_interval: { $$ = -1; }
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

/*** {{{ BEGIN listen ***/
listen: crLISTEN listener[lsnr]
	  {
	  	struct _rcptr *walk;
		char *err;

		for (walk = $lsnr->rcptr; walk != NULL; walk = walk->next) {
			err = router_add_listener(rtr, $lsnr->type,
				$lsnr->transport->mode, $lsnr->transport->pemcert,
				walk->ctype, walk->ip, walk->port, walk->saddr);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
		}
	  }
	  ;

listener: crTYPE crLINEMODE transport_mode[mode] receptors[ifaces]
		{
			if (($$ = ra_malloc(palloc, sizeof(struct _lsnr))) == NULL) {
				logerr("malloc failed\n");
				YYABORT;
			}
			$$->type = T_LINEMODE;
			$$->transport = $mode;
			$$->rcptr = $ifaces;
			if ($mode->mode != W_PLAIN) {
				struct _rcptr *walk;

				for (walk = $ifaces; walk != NULL; walk = walk->next) {
					if (walk->ctype == CON_UDP) {
						router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
							"cannot use UDP transport for "
							"compressed/encrypted stream");
						YYERROR;
					}
				}
			}
		}
		;

transport_mode:         {
							if (($$ = ra_malloc(palloc,
									sizeof(struct _rcptr_trsp))) == NULL)
							{
								logerr("malloc failed\n");
								YYABORT;
							}
							$$->mode = W_PLAIN;
						}
			  | crTRANSPORT crGZIP  {
#ifdef HAVE_GZIP
							if (($$ = ra_malloc(palloc,
									sizeof(struct _rcptr_trsp))) == NULL)
							{
								logerr("malloc failed\n");
								YYABORT;
							}
							$$->mode = W_GZIP;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature gzip not compiled in");
							YYERROR;
#endif
						}
			  | crTRANSPORT crLZ4 {
#ifdef HAVE_LZ4
							if (($$ = ra_malloc(palloc,
									sizeof(struct _rcptr_trsp))) == NULL)
							{
								logerr("malloc failed\n");
								YYABORT;
							}
							$$->mode = W_LZ4;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature lz4 not compiled in");
							YYERROR;
#endif
						}
			  | crTRANSPORT crSSL crSTRING[pemcert]  {
#ifdef HAVE_SSL
							if (($$ = ra_malloc(palloc,
									sizeof(struct _rcptr_trsp))) == NULL)
							{
								logerr("malloc failed\n");
								YYABORT;
							}
							$$->mode = W_SSL;
							$$->pemcert = ra_strdup(ralloc, $pemcert);
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature ssl not compiled in");
							YYERROR;
#endif
						}
			  ;

receptors: receptor[l] opt_receptor[r] { $l->next = $r; $$ = $l; }
		 ;

opt_receptor:           { $$ = NULL; }
			| receptors { $$ = $1;   }
			;

receptor: crSTRING[ip] crPROTO rcptr_proto[prot]
		{
			char *err;
			void *hint = NULL;
			char *w;
			char bcip[24];

			if (($$ = ra_malloc(palloc, sizeof(struct _rcptr))) == NULL) {
				logerr("malloc failed\n");
				YYABORT;
			}
			$$->ctype = $prot;

			/* find out if this is just a port */
			for (w = $ip; *w != '\0'; w++)
				if (*w < '0' || *w > '9')
					break;
			if (*w == '\0') {
				snprintf(bcip, sizeof(bcip), ":%s", $ip);
				$ip = bcip;
			}

			err = router_validate_address(
					rtr,
					&($$->ip), &($$->port), &($$->saddr), &hint,
					$ip, $prot);
			/* help some static analysis tools to see bcip isn't going
			 * out of scope */
			$ip = NULL;
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
			free(hint);
			$$->next = NULL;
		}
		| crSTRING[path] crPROTO crUNIX
		{
			char *err;

			if (($$ = ra_malloc(palloc, sizeof(struct _rcptr))) == NULL) {
				logerr("malloc failed\n");
				YYABORT;
			}
			$$->ctype = CON_UNIX;
			$$->ip = $path;
			$$->port = 0;
			$$->saddr = NULL;
			err = router_validate_path(rtr, $path);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
			$$->next = NULL;
		}
		;

rcptr_proto: crTCP { $$ = CON_TCP; }
		   | crUDP { $$ = CON_UDP; }
		   ;
/*** }}} END listen ***/

/*** {{{ BEGIN include ***/
include: crINCLUDE crSTRING[path]
	   {
	   	if (router_readconfig(rtr, $path, 0, 0, 0, 0, 0, 0, 0) == NULL)
			YYERROR;
	   }
	   ;
/*** }}} END include ***/

/* vim: set ts=4 sw=4 foldmethod=marker: */
