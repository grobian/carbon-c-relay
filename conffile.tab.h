/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_ROUTER_YY_CONFFILE_TAB_H_INCLUDED
# define YY_ROUTER_YY_CONFFILE_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef ROUTER_YYDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define ROUTER_YYDEBUG 1
#  else
#   define ROUTER_YYDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define ROUTER_YYDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined ROUTER_YYDEBUG */
#if ROUTER_YYDEBUG
extern int router_yydebug;
#endif
/* "%code requires" blocks.  */
#line 28 "conffile.y"

struct _clust {
	enum clusttype t;
	int ival;
};
struct _clhost {
	char *ip;
	unsigned short port;
	char *inst;
	int proto;
	con_type type;
	struct _rcptr_trsp_ssl *trnsp;
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
	unsigned short port;
	void *saddr;
	struct _rcptr *next;
};
enum _rcptr_sslprototype {
	_rp_PROTOMIN,
	_rp_PROTOMAX
};
struct _rcptr_sslprotos {
	enum _rcptr_sslprototype prtype;
	tlsprotover prver;
	struct _rcptr_sslprotos *next;
};
struct _rcptr_trsp_ssl {
	con_trnsp mode;
	char *mtlspemcert;
	char *mtlspemkey;
};
struct _rcptr_trsp {
	con_trnsp mode;
	char *pemcert;
	struct _rcptr_sslprotos *protos;
	char *ciphers;
	char *suites;
};

#line 119 "conffile.tab.h"

/* Token kinds.  */
#ifndef ROUTER_YYTOKENTYPE
# define ROUTER_YYTOKENTYPE
  enum router_yytokentype
  {
    ROUTER_YYEMPTY = -2,
    ROUTER_YYEOF = 0,              /* "end of file"  */
    ROUTER_YYerror = 256,          /* error  */
    ROUTER_YYUNDEF = 257,          /* "invalid token"  */
    crCLUSTER = 258,               /* crCLUSTER  */
    crFORWARD = 259,               /* crFORWARD  */
    crANY_OF = 260,                /* crANY_OF  */
    crFAILOVER = 261,              /* crFAILOVER  */
    crCARBON_CH = 262,             /* crCARBON_CH  */
    crFNV1A_CH = 263,              /* crFNV1A_CH  */
    crJUMP_FNV1A_CH = 264,         /* crJUMP_FNV1A_CH  */
    crFILE = 265,                  /* crFILE  */
    crIP = 266,                    /* crIP  */
    crREPLICATION = 267,           /* crREPLICATION  */
    crDYNAMIC = 268,               /* crDYNAMIC  */
    crPROTO = 269,                 /* crPROTO  */
    crUSEALL = 270,                /* crUSEALL  */
    crUDP = 271,                   /* crUDP  */
    crTCP = 272,                   /* crTCP  */
    crMATCH = 273,                 /* crMATCH  */
    crVALIDATE = 274,              /* crVALIDATE  */
    crELSE = 275,                  /* crELSE  */
    crLOG = 276,                   /* crLOG  */
    crDROP = 277,                  /* crDROP  */
    crROUTE = 278,                 /* crROUTE  */
    crUSING = 279,                 /* crUSING  */
    crSEND = 280,                  /* crSEND  */
    crTO = 281,                    /* crTO  */
    crBLACKHOLE = 282,             /* crBLACKHOLE  */
    crSTOP = 283,                  /* crSTOP  */
    crREWRITE = 284,               /* crREWRITE  */
    crINTO = 285,                  /* crINTO  */
    crAGGREGATE = 286,             /* crAGGREGATE  */
    crEVERY = 287,                 /* crEVERY  */
    crSECONDS = 288,               /* crSECONDS  */
    crEXPIRE = 289,                /* crEXPIRE  */
    crAFTER = 290,                 /* crAFTER  */
    crTIMESTAMP = 291,             /* crTIMESTAMP  */
    crAT = 292,                    /* crAT  */
    crSTART = 293,                 /* crSTART  */
    crMIDDLE = 294,                /* crMIDDLE  */
    crEND = 295,                   /* crEND  */
    crOF = 296,                    /* crOF  */
    crBUCKET = 297,                /* crBUCKET  */
    crCOMPUTE = 298,               /* crCOMPUTE  */
    crSUM = 299,                   /* crSUM  */
    crCOUNT = 300,                 /* crCOUNT  */
    crMAX = 301,                   /* crMAX  */
    crMIN = 302,                   /* crMIN  */
    crAVERAGE = 303,               /* crAVERAGE  */
    crMEDIAN = 304,                /* crMEDIAN  */
    crVARIANCE = 305,              /* crVARIANCE  */
    crSTDDEV = 306,                /* crSTDDEV  */
    crPERCENTILE = 307,            /* crPERCENTILE  */
    crWRITE = 308,                 /* crWRITE  */
    crSTATISTICS = 309,            /* crSTATISTICS  */
    crSUBMIT = 310,                /* crSUBMIT  */
    crRESET = 311,                 /* crRESET  */
    crCOUNTERS = 312,              /* crCOUNTERS  */
    crINTERVAL = 313,              /* crINTERVAL  */
    crPREFIX = 314,                /* crPREFIX  */
    crWITH = 315,                  /* crWITH  */
    crLISTEN = 316,                /* crLISTEN  */
    crTYPE = 317,                  /* crTYPE  */
    crLINEMODE = 318,              /* crLINEMODE  */
    crSYSLOGMODE = 319,            /* crSYSLOGMODE  */
    crTRANSPORT = 320,             /* crTRANSPORT  */
    crPLAIN = 321,                 /* crPLAIN  */
    crGZIP = 322,                  /* crGZIP  */
    crLZ4 = 323,                   /* crLZ4  */
    crSNAPPY = 324,                /* crSNAPPY  */
    crSSL = 325,                   /* crSSL  */
    crMTLS = 326,                  /* crMTLS  */
    crUNIX = 327,                  /* crUNIX  */
    crPROTOMIN = 328,              /* crPROTOMIN  */
    crPROTOMAX = 329,              /* crPROTOMAX  */
    crSSL3 = 330,                  /* crSSL3  */
    crTLS1_0 = 331,                /* crTLS1_0  */
    crTLS1_1 = 332,                /* crTLS1_1  */
    crTLS1_2 = 333,                /* crTLS1_2  */
    crTLS1_3 = 334,                /* crTLS1_3  */
    crCIPHERS = 335,               /* crCIPHERS  */
    crCIPHERSUITES = 336,          /* crCIPHERSUITES  */
    crINCLUDE = 337,               /* crINCLUDE  */
    crCOMMENT = 338,               /* crCOMMENT  */
    crSTRING = 339,                /* crSTRING  */
    crUNEXPECTED = 340,            /* crUNEXPECTED  */
    crINTVAL = 341                 /* crINTVAL  */
  };
  typedef enum router_yytokentype router_yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined ROUTER_YYSTYPE && ! defined ROUTER_YYSTYPE_IS_DECLARED
union ROUTER_YYSTYPE
{
  char * crCOMMENT;                        /* crCOMMENT  */
  char * crSTRING;                         /* crSTRING  */
  char * crUNEXPECTED;                     /* crUNEXPECTED  */
  char * cluster_opt_instance;             /* cluster_opt_instance  */
  char * match_opt_route;                  /* match_opt_route  */
  char * statistics_opt_prefix;            /* statistics_opt_prefix  */
  char * transport_opt_ssl_ciphers;        /* transport_opt_ssl_ciphers  */
  char * transport_opt_ssl_ciphersuites;   /* transport_opt_ssl_ciphersuites  */
  cluster * cluster;                       /* cluster  */
  col_mode statistics_opt_counters;        /* statistics_opt_counters  */
  con_proto cluster_opt_proto;             /* cluster_opt_proto  */
  con_proto rcptr_proto;                   /* rcptr_proto  */
  con_trnsp cluster_transport_trans;       /* cluster_transport_trans  */
  con_type cluster_opt_type;               /* cluster_opt_type  */
  con_type transport_ssl_or_mtls;          /* transport_ssl_or_mtls  */
  destinations * match_opt_send_to;        /* match_opt_send_to  */
  destinations * match_send_to;            /* match_send_to  */
  destinations * match_dsts;               /* match_dsts  */
  destinations * match_dsts2;              /* match_dsts2  */
  destinations * match_opt_dst;            /* match_opt_dst  */
  destinations * match_dst;                /* match_dst  */
  destinations * aggregate_opt_send_to;    /* aggregate_opt_send_to  */
  enum _aggr_timestamp aggregate_opt_timestamp; /* aggregate_opt_timestamp  */
  enum _aggr_timestamp aggregate_ts_when;  /* aggregate_ts_when  */
  enum _rcptr_sslprototype transport_ssl_prototype; /* transport_ssl_prototype  */
  enum clusttype cluster_useall;           /* cluster_useall  */
  enum clusttype cluster_ch;               /* cluster_ch  */
  int crPERCENTILE;                        /* crPERCENTILE  */
  int crINTVAL;                            /* crINTVAL  */
  int cluster_opt_useall;                  /* cluster_opt_useall  */
  int cluster_opt_repl;                    /* cluster_opt_repl  */
  int cluster_opt_dynamic;                 /* cluster_opt_dynamic  */
  int match_log_or_drop;                   /* match_log_or_drop  */
  int match_opt_stop;                      /* match_opt_stop  */
  int statistics_opt_interval;             /* statistics_opt_interval  */
  struct _agcomp aggregate_comp_type;      /* aggregate_comp_type  */
  struct _agcomp * aggregate_computes;     /* aggregate_computes  */
  struct _agcomp * aggregate_opt_compute;  /* aggregate_opt_compute  */
  struct _agcomp * aggregate_compute;      /* aggregate_compute  */
  struct _clhost * cluster_paths;          /* cluster_paths  */
  struct _clhost * cluster_opt_path;       /* cluster_opt_path  */
  struct _clhost * cluster_path;           /* cluster_path  */
  struct _clhost * cluster_hosts;          /* cluster_hosts  */
  struct _clhost * cluster_opt_host;       /* cluster_opt_host  */
  struct _clhost * cluster_host;           /* cluster_host  */
  struct _clust cluster_type;              /* cluster_type  */
  struct _clust cluster_file;              /* cluster_file  */
  struct _lsnr * listener;                 /* listener  */
  struct _maexpr * match_exprs;            /* match_exprs  */
  struct _maexpr * match_exprs2;           /* match_exprs2  */
  struct _maexpr * match_opt_expr;         /* match_opt_expr  */
  struct _maexpr * match_expr;             /* match_expr  */
  struct _maexpr * match_opt_validate;     /* match_opt_validate  */
  struct _rcptr * receptors;               /* receptors  */
  struct _rcptr * opt_receptor;            /* opt_receptor  */
  struct _rcptr * receptor;                /* receptor  */
  struct _rcptr_sslprotos * transport_opt_ssl_protos; /* transport_opt_ssl_protos  */
  struct _rcptr_sslprotos * transport_ssl_proto; /* transport_ssl_proto  */
  struct _rcptr_trsp * transport_opt_ssl;  /* transport_opt_ssl  */
  struct _rcptr_trsp * transport_mode_trans; /* transport_mode_trans  */
  struct _rcptr_trsp * transport_mode;     /* transport_mode  */
  struct _rcptr_trsp_ssl * cluster_opt_transport; /* cluster_opt_transport  */
  struct _rcptr_trsp_ssl * cluster_transport_opt_ssl; /* cluster_transport_opt_ssl  */
  tlsprotover transport_ssl_protover;      /* transport_ssl_protover  */

#line 287 "conffile.tab.h"

};
typedef union ROUTER_YYSTYPE ROUTER_YYSTYPE;
# define ROUTER_YYSTYPE_IS_TRIVIAL 1
# define ROUTER_YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined ROUTER_YYLTYPE && ! defined ROUTER_YYLTYPE_IS_DECLARED
typedef struct ROUTER_YYLTYPE ROUTER_YYLTYPE;
struct ROUTER_YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define ROUTER_YYLTYPE_IS_DECLARED 1
# define ROUTER_YYLTYPE_IS_TRIVIAL 1
#endif




int router_yyparse (void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc);


#endif /* !YY_ROUTER_YY_CONFFILE_TAB_H_INCLUDED  */
