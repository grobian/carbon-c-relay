/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         ROUTER_YYSTYPE
#define YYLTYPE         ROUTER_YYLTYPE
/* Substitute the variable and function names.  */
#define yyparse         router_yyparse
#define yylex           router_yylex
#define yyerror         router_yyerror
#define yydebug         router_yydebug
#define yynerrs         router_yynerrs

/* First part of user prologue.  */
#line 1 "conffile.y"

#include "allocator.h"
#include "conffile.h"
#include "conffile.tab.h"
#include "aggregator.h"
#include "receptor.h"
#include "router.h"
#include "config.h"

int router_yylex(ROUTER_YYSTYPE *, ROUTER_YYLTYPE *, void *, router *, allocator *, allocator *);

#line 90 "conffile.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "conffile.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_crCLUSTER = 3,                  /* crCLUSTER  */
  YYSYMBOL_crFORWARD = 4,                  /* crFORWARD  */
  YYSYMBOL_crANY_OF = 5,                   /* crANY_OF  */
  YYSYMBOL_crFAILOVER = 6,                 /* crFAILOVER  */
  YYSYMBOL_crCARBON_CH = 7,                /* crCARBON_CH  */
  YYSYMBOL_crFNV1A_CH = 8,                 /* crFNV1A_CH  */
  YYSYMBOL_crJUMP_FNV1A_CH = 9,            /* crJUMP_FNV1A_CH  */
  YYSYMBOL_crFILE = 10,                    /* crFILE  */
  YYSYMBOL_crIP = 11,                      /* crIP  */
  YYSYMBOL_crREPLICATION = 12,             /* crREPLICATION  */
  YYSYMBOL_crDYNAMIC = 13,                 /* crDYNAMIC  */
  YYSYMBOL_crPROTO = 14,                   /* crPROTO  */
  YYSYMBOL_crUSEALL = 15,                  /* crUSEALL  */
  YYSYMBOL_crUDP = 16,                     /* crUDP  */
  YYSYMBOL_crTCP = 17,                     /* crTCP  */
  YYSYMBOL_crMATCH = 18,                   /* crMATCH  */
  YYSYMBOL_crVALIDATE = 19,                /* crVALIDATE  */
  YYSYMBOL_crELSE = 20,                    /* crELSE  */
  YYSYMBOL_crLOG = 21,                     /* crLOG  */
  YYSYMBOL_crDROP = 22,                    /* crDROP  */
  YYSYMBOL_crROUTE = 23,                   /* crROUTE  */
  YYSYMBOL_crUSING = 24,                   /* crUSING  */
  YYSYMBOL_crSEND = 25,                    /* crSEND  */
  YYSYMBOL_crTO = 26,                      /* crTO  */
  YYSYMBOL_crBLACKHOLE = 27,               /* crBLACKHOLE  */
  YYSYMBOL_crSTOP = 28,                    /* crSTOP  */
  YYSYMBOL_crREWRITE = 29,                 /* crREWRITE  */
  YYSYMBOL_crINTO = 30,                    /* crINTO  */
  YYSYMBOL_crAGGREGATE = 31,               /* crAGGREGATE  */
  YYSYMBOL_crEVERY = 32,                   /* crEVERY  */
  YYSYMBOL_crSECONDS = 33,                 /* crSECONDS  */
  YYSYMBOL_crEXPIRE = 34,                  /* crEXPIRE  */
  YYSYMBOL_crAFTER = 35,                   /* crAFTER  */
  YYSYMBOL_crTIMESTAMP = 36,               /* crTIMESTAMP  */
  YYSYMBOL_crAT = 37,                      /* crAT  */
  YYSYMBOL_crSTART = 38,                   /* crSTART  */
  YYSYMBOL_crMIDDLE = 39,                  /* crMIDDLE  */
  YYSYMBOL_crEND = 40,                     /* crEND  */
  YYSYMBOL_crOF = 41,                      /* crOF  */
  YYSYMBOL_crBUCKET = 42,                  /* crBUCKET  */
  YYSYMBOL_crCOMPUTE = 43,                 /* crCOMPUTE  */
  YYSYMBOL_crSUM = 44,                     /* crSUM  */
  YYSYMBOL_crCOUNT = 45,                   /* crCOUNT  */
  YYSYMBOL_crMAX = 46,                     /* crMAX  */
  YYSYMBOL_crMIN = 47,                     /* crMIN  */
  YYSYMBOL_crAVERAGE = 48,                 /* crAVERAGE  */
  YYSYMBOL_crMEDIAN = 49,                  /* crMEDIAN  */
  YYSYMBOL_crVARIANCE = 50,                /* crVARIANCE  */
  YYSYMBOL_crSTDDEV = 51,                  /* crSTDDEV  */
  YYSYMBOL_crPERCENTILE = 52,              /* crPERCENTILE  */
  YYSYMBOL_crWRITE = 53,                   /* crWRITE  */
  YYSYMBOL_crSTATISTICS = 54,              /* crSTATISTICS  */
  YYSYMBOL_crSUBMIT = 55,                  /* crSUBMIT  */
  YYSYMBOL_crRESET = 56,                   /* crRESET  */
  YYSYMBOL_crCOUNTERS = 57,                /* crCOUNTERS  */
  YYSYMBOL_crINTERVAL = 58,                /* crINTERVAL  */
  YYSYMBOL_crPREFIX = 59,                  /* crPREFIX  */
  YYSYMBOL_crWITH = 60,                    /* crWITH  */
  YYSYMBOL_crLISTEN = 61,                  /* crLISTEN  */
  YYSYMBOL_crTYPE = 62,                    /* crTYPE  */
  YYSYMBOL_crLINEMODE = 63,                /* crLINEMODE  */
  YYSYMBOL_crSYSLOGMODE = 64,              /* crSYSLOGMODE  */
  YYSYMBOL_crTRANSPORT = 65,               /* crTRANSPORT  */
  YYSYMBOL_crPLAIN = 66,                   /* crPLAIN  */
  YYSYMBOL_crGZIP = 67,                    /* crGZIP  */
  YYSYMBOL_crLZ4 = 68,                     /* crLZ4  */
  YYSYMBOL_crSNAPPY = 69,                  /* crSNAPPY  */
  YYSYMBOL_crSSL = 70,                     /* crSSL  */
  YYSYMBOL_crUNIX = 71,                    /* crUNIX  */
  YYSYMBOL_crPROTOMIN = 72,                /* crPROTOMIN  */
  YYSYMBOL_crPROTOMAX = 73,                /* crPROTOMAX  */
  YYSYMBOL_crSSL3 = 74,                    /* crSSL3  */
  YYSYMBOL_crTLS1_0 = 75,                  /* crTLS1_0  */
  YYSYMBOL_crTLS1_1 = 76,                  /* crTLS1_1  */
  YYSYMBOL_crTLS1_2 = 77,                  /* crTLS1_2  */
  YYSYMBOL_crTLS1_3 = 78,                  /* crTLS1_3  */
  YYSYMBOL_crINCLUDE = 79,                 /* crINCLUDE  */
  YYSYMBOL_crCOMMENT = 80,                 /* crCOMMENT  */
  YYSYMBOL_crSTRING = 81,                  /* crSTRING  */
  YYSYMBOL_crUNEXPECTED = 82,              /* crUNEXPECTED  */
  YYSYMBOL_crINTVAL = 83,                  /* crINTVAL  */
  YYSYMBOL_84_ = 84,                       /* ';'  */
  YYSYMBOL_85_ = 85,                       /* '='  */
  YYSYMBOL_86_ = 86,                       /* '*'  */
  YYSYMBOL_YYACCEPT = 87,                  /* $accept  */
  YYSYMBOL_stmts = 88,                     /* stmts  */
  YYSYMBOL_opt_stmt = 89,                  /* opt_stmt  */
  YYSYMBOL_stmt = 90,                      /* stmt  */
  YYSYMBOL_command = 91,                   /* command  */
  YYSYMBOL_cluster = 92,                   /* cluster  */
  YYSYMBOL_cluster_type = 93,              /* cluster_type  */
  YYSYMBOL_cluster_useall = 94,            /* cluster_useall  */
  YYSYMBOL_cluster_opt_useall = 95,        /* cluster_opt_useall  */
  YYSYMBOL_cluster_ch = 96,                /* cluster_ch  */
  YYSYMBOL_cluster_opt_repl = 97,          /* cluster_opt_repl  */
  YYSYMBOL_cluster_opt_dynamic = 98,       /* cluster_opt_dynamic  */
  YYSYMBOL_cluster_file = 99,              /* cluster_file  */
  YYSYMBOL_cluster_paths = 100,            /* cluster_paths  */
  YYSYMBOL_cluster_opt_path = 101,         /* cluster_opt_path  */
  YYSYMBOL_cluster_path = 102,             /* cluster_path  */
  YYSYMBOL_cluster_hosts = 103,            /* cluster_hosts  */
  YYSYMBOL_cluster_opt_host = 104,         /* cluster_opt_host  */
  YYSYMBOL_cluster_host = 105,             /* cluster_host  */
  YYSYMBOL_cluster_opt_instance = 106,     /* cluster_opt_instance  */
  YYSYMBOL_cluster_opt_proto = 107,        /* cluster_opt_proto  */
  YYSYMBOL_cluster_opt_type = 108,         /* cluster_opt_type  */
  YYSYMBOL_cluster_opt_transport = 109,    /* cluster_opt_transport  */
  YYSYMBOL_cluster_transport_trans = 110,  /* cluster_transport_trans  */
  YYSYMBOL_cluster_transport_opt_ssl = 111, /* cluster_transport_opt_ssl  */
  YYSYMBOL_match = 112,                    /* match  */
  YYSYMBOL_match_exprs = 113,              /* match_exprs  */
  YYSYMBOL_match_exprs2 = 114,             /* match_exprs2  */
  YYSYMBOL_match_opt_expr = 115,           /* match_opt_expr  */
  YYSYMBOL_match_expr = 116,               /* match_expr  */
  YYSYMBOL_match_opt_validate = 117,       /* match_opt_validate  */
  YYSYMBOL_match_log_or_drop = 118,        /* match_log_or_drop  */
  YYSYMBOL_match_opt_route = 119,          /* match_opt_route  */
  YYSYMBOL_match_opt_send_to = 120,        /* match_opt_send_to  */
  YYSYMBOL_match_send_to = 121,            /* match_send_to  */
  YYSYMBOL_match_dsts = 122,               /* match_dsts  */
  YYSYMBOL_match_dsts2 = 123,              /* match_dsts2  */
  YYSYMBOL_match_opt_dst = 124,            /* match_opt_dst  */
  YYSYMBOL_match_dst = 125,                /* match_dst  */
  YYSYMBOL_match_opt_stop = 126,           /* match_opt_stop  */
  YYSYMBOL_rewrite = 127,                  /* rewrite  */
  YYSYMBOL_aggregate = 128,                /* aggregate  */
  YYSYMBOL_aggregate_opt_timestamp = 129,  /* aggregate_opt_timestamp  */
  YYSYMBOL_aggregate_ts_when = 130,        /* aggregate_ts_when  */
  YYSYMBOL_aggregate_computes = 131,       /* aggregate_computes  */
  YYSYMBOL_aggregate_opt_compute = 132,    /* aggregate_opt_compute  */
  YYSYMBOL_aggregate_compute = 133,        /* aggregate_compute  */
  YYSYMBOL_aggregate_comp_type = 134,      /* aggregate_comp_type  */
  YYSYMBOL_aggregate_opt_send_to = 135,    /* aggregate_opt_send_to  */
  YYSYMBOL_send = 136,                     /* send  */
  YYSYMBOL_statistics = 137,               /* statistics  */
  YYSYMBOL_statistics_opt_interval = 138,  /* statistics_opt_interval  */
  YYSYMBOL_statistics_opt_counters = 139,  /* statistics_opt_counters  */
  YYSYMBOL_statistics_opt_prefix = 140,    /* statistics_opt_prefix  */
  YYSYMBOL_listen = 141,                   /* listen  */
  YYSYMBOL_listener = 142,                 /* listener  */
  YYSYMBOL_transport_opt_ssl = 143,        /* transport_opt_ssl  */
  YYSYMBOL_transport_opt_ssl_protos = 144, /* transport_opt_ssl_protos  */
  YYSYMBOL_transport_ssl_proto = 145,      /* transport_ssl_proto  */
  YYSYMBOL_transport_ssl_prototype = 146,  /* transport_ssl_prototype  */
  YYSYMBOL_transport_ssl_protover = 147,   /* transport_ssl_protover  */
  YYSYMBOL_transport_mode_trans = 148,     /* transport_mode_trans  */
  YYSYMBOL_transport_mode = 149,           /* transport_mode  */
  YYSYMBOL_receptors = 150,                /* receptors  */
  YYSYMBOL_opt_receptor = 151,             /* opt_receptor  */
  YYSYMBOL_receptor = 152,                 /* receptor  */
  YYSYMBOL_rcptr_proto = 153,              /* rcptr_proto  */
  YYSYMBOL_include = 154                   /* include  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined ROUTER_YYLTYPE_IS_TRIVIAL && ROUTER_YYLTYPE_IS_TRIVIAL \
             && defined ROUTER_YYSTYPE_IS_TRIVIAL && ROUTER_YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  35
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   149

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  87
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  68
/* YYNRULES -- Number of rules.  */
#define YYNRULES  138
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  203

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   338


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,    86,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    84,
       2,    85,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83
};

#if ROUTER_YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   142,   142,   145,   146,   149,   152,   153,   154,   155,
     156,   157,   158,   159,   163,   233,   274,   276,   280,   281,
     282,   285,   286,   289,   290,   291,   294,   295,   298,   299,
     302,   303,   306,   308,   309,   311,   331,   333,   334,   336,
     358,   359,   360,   370,   371,   372,   375,   376,   377,   380,
     381,   391,   392,   402,   412,   423,   424,   439,   494,   506,
     509,   511,   512,   515,   534,   535,   554,   555,   558,   559,
     562,   563,   566,   569,   579,   582,   584,   585,   588,   604,
     605,   610,   655,   746,   747,   752,   753,   754,   757,   761,
     762,   764,   779,   780,   781,   782,   783,   784,   785,   795,
     796,   799,   800,   805,   820,   845,   846,   857,   858,   861,
     862,   867,   898,   923,   926,   947,   950,   955,   968,   969,
     971,   972,   973,   974,   975,   978,   988,  1005,  1022,  1042,
    1051,  1062,  1065,  1066,  1069,  1106,  1128,  1129,  1134
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "crCLUSTER",
  "crFORWARD", "crANY_OF", "crFAILOVER", "crCARBON_CH", "crFNV1A_CH",
  "crJUMP_FNV1A_CH", "crFILE", "crIP", "crREPLICATION", "crDYNAMIC",
  "crPROTO", "crUSEALL", "crUDP", "crTCP", "crMATCH", "crVALIDATE",
  "crELSE", "crLOG", "crDROP", "crROUTE", "crUSING", "crSEND", "crTO",
  "crBLACKHOLE", "crSTOP", "crREWRITE", "crINTO", "crAGGREGATE", "crEVERY",
  "crSECONDS", "crEXPIRE", "crAFTER", "crTIMESTAMP", "crAT", "crSTART",
  "crMIDDLE", "crEND", "crOF", "crBUCKET", "crCOMPUTE", "crSUM", "crCOUNT",
  "crMAX", "crMIN", "crAVERAGE", "crMEDIAN", "crVARIANCE", "crSTDDEV",
  "crPERCENTILE", "crWRITE", "crSTATISTICS", "crSUBMIT", "crRESET",
  "crCOUNTERS", "crINTERVAL", "crPREFIX", "crWITH", "crLISTEN", "crTYPE",
  "crLINEMODE", "crSYSLOGMODE", "crTRANSPORT", "crPLAIN", "crGZIP",
  "crLZ4", "crSNAPPY", "crSSL", "crUNIX", "crPROTOMIN", "crPROTOMAX",
  "crSSL3", "crTLS1_0", "crTLS1_1", "crTLS1_2", "crTLS1_3", "crINCLUDE",
  "crCOMMENT", "crSTRING", "crUNEXPECTED", "crINTVAL", "';'", "'='", "'*'",
  "$accept", "stmts", "opt_stmt", "stmt", "command", "cluster",
  "cluster_type", "cluster_useall", "cluster_opt_useall", "cluster_ch",
  "cluster_opt_repl", "cluster_opt_dynamic", "cluster_file",
  "cluster_paths", "cluster_opt_path", "cluster_path", "cluster_hosts",
  "cluster_opt_host", "cluster_host", "cluster_opt_instance",
  "cluster_opt_proto", "cluster_opt_type", "cluster_opt_transport",
  "cluster_transport_trans", "cluster_transport_opt_ssl", "match",
  "match_exprs", "match_exprs2", "match_opt_expr", "match_expr",
  "match_opt_validate", "match_log_or_drop", "match_opt_route",
  "match_opt_send_to", "match_send_to", "match_dsts", "match_dsts2",
  "match_opt_dst", "match_dst", "match_opt_stop", "rewrite", "aggregate",
  "aggregate_opt_timestamp", "aggregate_ts_when", "aggregate_computes",
  "aggregate_opt_compute", "aggregate_compute", "aggregate_comp_type",
  "aggregate_opt_send_to", "send", "statistics", "statistics_opt_interval",
  "statistics_opt_counters", "statistics_opt_prefix", "listen", "listener",
  "transport_opt_ssl", "transport_opt_ssl_protos", "transport_ssl_proto",
  "transport_ssl_prototype", "transport_ssl_protover",
  "transport_mode_trans", "transport_mode", "receptors", "opt_receptor",
  "receptor", "rcptr_proto", "include", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-101)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      -2,   -75,   -74,   -40,   -59,   -53,   -25,   -11,   -15,    67,
    -101,    -2,   -16,  -101,  -101,  -101,  -101,  -101,  -101,  -101,
    -101,    36,  -101,  -101,    50,  -101,   -53,    44,    41,    40,
      42,    17,    12,  -101,  -101,  -101,  -101,  -101,  -101,  -101,
    -101,  -101,  -101,  -101,    65,    -3,    64,    68,     2,     4,
      58,  -101,  -101,   -23,     5,    -1,     6,    27,    28,    23,
    -101,     7,  -101,    -3,  -101,  -101,     8,    77,  -101,  -101,
       2,    73,    71,    72,  -101,  -101,    70,  -101,    15,  -101,
      66,    69,    74,    43,    72,   -19,    30,    20,   -68,    90,
    -101,  -101,  -101,  -101,  -101,  -101,  -101,     3,    24,    80,
      70,  -101,  -101,  -101,  -101,  -101,    76,  -101,    49,    31,
    -101,    70,  -101,  -101,  -101,  -101,    32,  -101,    94,  -101,
      20,  -101,  -101,    37,    52,  -101,  -101,  -101,  -101,   -23,
    -101,    81,  -101,  -101,  -101,   -17,   -14,  -101,  -101,  -101,
    -101,     1,    46,  -101,    34,  -101,  -101,  -101,   -17,   -57,
    -101,  -101,  -101,  -101,  -101,  -101,    -6,  -101,    45,    85,
    -101,  -101,  -101,  -101,  -101,  -101,  -101,  -101,  -101,  -101,
    -101,  -101,  -101,    83,    84,    79,   -30,   -13,    72,    79,
    -101,  -101,  -101,    82,  -101,  -101,  -101,  -101,  -101,  -101,
    -101,  -101,  -101,    75,    70,  -101,  -101,    78,    98,  -101,
    -101,    48,  -101
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     0,     0,     0,     0,   105,     0,     0,     0,
       2,     3,     0,     6,     7,     8,     9,    10,    11,    12,
      13,     0,    63,    58,    64,    59,    61,     0,     0,     0,
       0,   107,     0,   111,   138,     1,     4,     5,    18,    19,
      20,    23,    24,    25,    31,     0,    21,    26,     0,     0,
      68,    62,    60,     0,     0,     0,     0,     0,   109,   129,
      30,    40,    14,    37,    22,    16,     0,    28,    35,    15,
      33,     0,     0,    70,    73,    78,    79,    74,    76,    81,
       0,     0,     0,     0,   101,     0,   113,     0,     0,    43,
      38,    36,    27,    29,    17,    32,    34,     0,     0,     0,
      79,    71,    80,   103,    77,    75,     0,   106,     0,     0,
     102,    79,   125,   126,   127,   128,     0,   130,     0,   112,
     132,    41,    42,     0,    46,    66,    67,    65,    69,     0,
      57,     0,   108,   110,   104,   115,     0,   133,   131,    44,
      45,     0,    49,    72,     0,   118,   119,   114,   115,     0,
     137,   136,   135,   134,    47,    48,     0,    39,    55,     0,
     116,   120,   121,   122,   123,   124,   117,    51,    52,    53,
      54,    56,    50,    83,     0,     0,     0,     0,   101,    89,
      85,    86,    87,     0,    92,    93,    94,    95,    96,    97,
      99,   100,    98,     0,    79,    90,    88,     0,     0,    82,
      84,     0,    91
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -101,  -101,   114,  -101,  -101,  -101,  -101,  -101,  -101,  -101,
    -101,  -101,  -101,  -101,  -101,    56,    86,  -101,  -101,  -101,
    -101,  -101,  -101,  -101,  -101,  -101,  -101,     0,  -101,  -101,
    -101,  -101,  -101,  -101,    54,     9,    53,  -101,  -101,  -100,
    -101,  -101,  -101,  -101,   -49,  -101,  -101,  -101,   -46,  -101,
    -101,  -101,  -101,  -101,  -101,  -101,  -101,   -12,  -101,  -101,
    -101,  -101,  -101,    13,  -101,  -101,  -101,  -101
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     9,    10,    11,    12,    13,    45,    46,    65,    47,
      67,    94,    48,    69,    95,    70,    62,    91,    63,    89,
     124,   142,   157,   158,   172,    14,    24,    25,    52,    26,
      50,   127,    73,   100,   110,    76,    77,   105,    78,   103,
      15,    16,   175,   183,   178,   196,   179,   193,   111,    17,
      18,    31,    58,    84,    19,    33,   117,   147,   148,   149,
     166,    86,    87,   119,   138,   120,   153,    20
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
     130,     1,   150,   151,    74,    29,    21,    22,   180,   181,
     182,   134,    23,   121,    27,   122,     2,   161,   162,   163,
     164,   165,    28,     3,   125,   126,    51,     4,    22,     5,
      30,   184,   185,   186,   187,   188,   189,   190,   191,   192,
      38,    39,    40,    41,    42,    43,    44,   112,   113,   114,
     115,    32,     6,   139,   140,   145,   146,   152,    75,     7,
     167,   168,   169,   170,   154,   155,    34,    35,    37,    49,
      53,    54,    55,    57,    56,    59,    60,     8,    61,    64,
      66,    72,    80,    68,    82,    71,    79,    83,    85,    81,
      93,    92,    88,    97,   199,    98,    75,    99,   102,   106,
     116,   118,   107,   109,   123,   128,   129,   132,   136,   108,
     131,   156,   133,   135,   141,   171,   144,   159,   173,   174,
     200,   176,   177,   197,   201,    36,    96,   101,   198,   202,
     195,   104,   194,   137,     0,     0,   160,     0,   143,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    90
};

static const yytype_int16 yycheck[] =
{
     100,     3,    16,    17,    27,     5,    81,    81,    38,    39,
      40,   111,    86,    81,    54,    83,    18,    74,    75,    76,
      77,    78,    81,    25,    21,    22,    26,    29,    81,    31,
      55,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       4,     5,     6,     7,     8,     9,    10,    66,    67,    68,
      69,    62,    54,    16,    17,    72,    73,    71,    81,    61,
      66,    67,    68,    69,    63,    64,    81,     0,    84,    19,
      26,    30,    32,    56,    32,    63,    11,    79,    81,    15,
      12,    23,    83,    81,    57,    81,    81,    59,    65,    83,
      13,    83,    85,    20,   194,    24,    81,    25,    28,    33,
      70,    81,    33,    60,    14,    81,    26,    58,    14,    35,
      34,    65,    81,    81,    62,    70,    35,    83,    33,    36,
      42,    37,    43,    41,    26,    11,    70,    73,    53,    81,
     179,    78,   178,   120,    -1,    -1,   148,    -1,   129,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    63
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,    18,    25,    29,    31,    54,    61,    79,    88,
      89,    90,    91,    92,   112,   127,   128,   136,   137,   141,
     154,    81,    81,    86,   113,   114,   116,    54,    81,   114,
      55,   138,    62,   142,    81,     0,    89,    84,     4,     5,
       6,     7,     8,     9,    10,    93,    94,    96,    99,    19,
     117,   114,   115,    26,    30,    32,    32,    56,   139,    63,
      11,    81,   103,   105,    15,    95,    12,    97,    81,   100,
     102,    81,    23,   119,    27,    81,   122,   123,   125,    81,
      83,    83,    57,    59,   140,    65,   148,   149,    85,   106,
     103,   104,    83,    13,    98,   101,   102,    20,    24,    25,
     120,   121,    28,   126,   123,   124,    33,    33,    35,    60,
     121,   135,    66,    67,    68,    69,    70,   143,    81,   150,
     152,    81,    83,    14,   107,    21,    22,   118,    81,    26,
     126,    34,    58,    81,   126,    81,    14,   150,   151,    16,
      17,    62,   108,   122,    35,    72,    73,   144,   145,   146,
      16,    17,    71,   153,    63,    64,    65,   109,   110,    83,
     144,    74,    75,    76,    77,    78,   147,    66,    67,    68,
      69,    70,   111,    33,    36,   129,    37,    43,   131,   133,
      38,    39,    40,   130,    44,    45,    46,    47,    48,    49,
      50,    51,    52,   134,   135,   131,   132,    41,    53,   126,
      42,    26,    81
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    87,    88,    89,    89,    90,    91,    91,    91,    91,
      91,    91,    91,    91,    92,    92,    93,    93,    94,    94,
      94,    95,    95,    96,    96,    96,    97,    97,    98,    98,
      99,    99,   100,   101,   101,   102,   103,   104,   104,   105,
     106,   106,   106,   107,   107,   107,   108,   108,   108,   109,
     109,   110,   110,   110,   110,   111,   111,   112,   113,   113,
     114,   115,   115,   116,   117,   117,   118,   118,   119,   119,
     120,   120,   121,   122,   122,   123,   124,   124,   125,   126,
     126,   127,   128,   129,   129,   130,   130,   130,   131,   132,
     132,   133,   134,   134,   134,   134,   134,   134,   134,   134,
     134,   135,   135,   136,   137,   138,   138,   139,   139,   140,
     140,   141,   142,   143,   143,   144,   144,   145,   146,   146,
     147,   147,   147,   147,   147,   148,   148,   148,   148,   149,
     149,   150,   151,   151,   152,   152,   153,   153,   154
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     4,     4,     2,     3,     1,     1,
       1,     0,     1,     1,     1,     1,     0,     2,     0,     1,
       2,     1,     2,     0,     1,     1,     2,     0,     1,     5,
       0,     2,     2,     0,     2,     2,     0,     2,     2,     0,
       2,     2,     2,     2,     2,     0,     1,     6,     1,     1,
       2,     0,     1,     1,     0,     4,     1,     1,     0,     3,
       0,     1,     3,     1,     1,     2,     0,     1,     1,     0,
       1,     4,    13,     0,     5,     1,     1,     1,     2,     0,
       1,     5,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     0,     1,     5,     6,     0,     4,     0,     4,     0,
       3,     2,     4,     0,     3,     0,     2,     2,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     2,     2,     0,
       2,     2,     0,     1,     3,     3,     1,     1,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = ROUTER_YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == ROUTER_YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, yyscanner, rtr, ralloc, palloc, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use ROUTER_YYerror or ROUTER_YYUNDEF. */
#define YYERRCODE ROUTER_YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if ROUTER_YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined ROUTER_YYLTYPE_IS_TRIVIAL && ROUTER_YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, yyscanner, rtr, ralloc, palloc); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (yyscanner);
  YY_USE (rtr);
  YY_USE (ralloc);
  YY_USE (palloc);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, yyscanner, rtr, ralloc, palloc);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), yyscanner, rtr, ralloc, palloc);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, yyscanner, rtr, ralloc, palloc); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !ROUTER_YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !ROUTER_YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (yyscanner);
  YY_USE (rtr);
  YY_USE (ralloc);
  YY_USE (palloc);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined ROUTER_YYLTYPE_IS_TRIVIAL && ROUTER_YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = ROUTER_YYEMPTY; /* Cause a token to be read.  */

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == ROUTER_YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, &yylloc, yyscanner, rtr, ralloc, palloc);
    }

  if (yychar <= ROUTER_YYEOF)
    {
      yychar = ROUTER_YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == ROUTER_YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = ROUTER_YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = ROUTER_YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 14: /* cluster: crCLUSTER crSTRING cluster_type cluster_hosts  */
#line 164 "conffile.y"
           {
	   	struct _clhost *w;
		char *err;
		int srvcnt;
		int replcnt;

		/* count number of servers for ch_new */
		for (srvcnt = 0, w = (yyvsp[0].cluster_hosts); w != NULL; w = w->next, srvcnt++)
			;

		if (((yyval.cluster) = ra_malloc(ralloc, sizeof(cluster))) == NULL) {
			logerr("malloc failed for cluster '%s'\n", (yyvsp[-2].crSTRING));
			YYABORT;
		}
		(yyval.cluster)->name = ra_strdup(ralloc, (yyvsp[-2].crSTRING));
		(yyval.cluster)->next = NULL;
		(yyval.cluster)->type = (yyvsp[-1].cluster_type).t;
		switch ((yyval.cluster)->type) {
			case CARBON_CH:
			case FNV1A_CH:
			case JUMP_CH:
				(yyval.cluster)->members.ch = ra_malloc(ralloc, sizeof(chashring));
				if ((yyval.cluster)->members.ch == NULL) {
					logerr("malloc failed for ch in cluster '%s'\n", (yyvsp[-2].crSTRING));
					YYABORT;
				}
				replcnt = (yyvsp[-1].cluster_type).ival / 10;
				(yyval.cluster)->isdynamic = (yyvsp[-1].cluster_type).ival - (replcnt * 10) == 2;
				if (replcnt < 1 || replcnt > 255) {
					router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
						"replication count must be between 1 and 255");
					YYERROR;
				}
				(yyval.cluster)->members.ch->repl_factor = (unsigned char)replcnt;
				(yyval.cluster)->members.ch->ring = ch_new(ralloc,
					(yyval.cluster)->type == CARBON_CH ? CARBON :
					(yyval.cluster)->type == FNV1A_CH ? FNV1a :
					JUMP_FNV1a, srvcnt);
				(yyval.cluster)->members.ch->servers = NULL;
				(yyvsp[-1].cluster_type).ival = 0;  /* hack, avoid triggering use_all */
				break;
			case FORWARD:
				(yyval.cluster)->members.forward = NULL;
				break;
			case ANYOF:
			case FAILOVER:
				(yyval.cluster)->members.anyof = NULL;
				break;
			default:
				logerr("unknown cluster type %zd!\n", (ssize_t)(yyval.cluster)->type);
				YYABORT;
		}
		
		for (w = (yyvsp[0].cluster_hosts); w != NULL; w = w->next) {
			err = router_add_server(rtr, w->ip, w->port, w->inst,
					w->type, w->trnsp, w->proto,
					w->saddr, w->hint, (char)(yyvsp[-1].cluster_type).ival, (yyval.cluster));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}
		}

		err = router_add_cluster(rtr, (yyval.cluster));
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
	   }
#line 1867 "conffile.tab.c"
    break;

  case 15: /* cluster: crCLUSTER crSTRING cluster_file cluster_paths  */
#line 234 "conffile.y"
           {
	   	struct _clhost *w;
		char *err;

		if (((yyval.cluster) = ra_malloc(ralloc, sizeof(cluster))) == NULL) {
			logerr("malloc failed for cluster '%s'\n", (yyvsp[-2].crSTRING));
			YYABORT;
		}
		(yyval.cluster)->name = ra_strdup(ralloc, (yyvsp[-2].crSTRING));
		(yyval.cluster)->next = NULL;
		(yyval.cluster)->type = (yyvsp[-1].cluster_file).t;
		switch ((yyval.cluster)->type) {
			case FILELOG:
			case FILELOGIP:
				(yyval.cluster)->members.forward = NULL;
				break;
			default:
				logerr("unknown cluster type %zd!\n", (ssize_t)(yyval.cluster)->type);
				YYABORT;
		}
		
		for (w = (yyvsp[0].cluster_paths); w != NULL; w = w->next) {
			err = router_add_server(rtr, w->ip, w->port, w->inst,
					T_LINEMODE, W_PLAIN, w->proto,
					w->saddr, w->hint, (char)(yyvsp[-1].cluster_file).ival, (yyval.cluster));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}
		}

		err = router_add_cluster(rtr, (yyval.cluster));
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
	   }
#line 1909 "conffile.tab.c"
    break;

  case 16: /* cluster_type: cluster_useall cluster_opt_useall  */
#line 275 "conffile.y"
                          { (yyval.cluster_type).t = (yyvsp[-1].cluster_useall); (yyval.cluster_type).ival = (yyvsp[0].cluster_opt_useall); }
#line 1915 "conffile.tab.c"
    break;

  case 17: /* cluster_type: cluster_ch cluster_opt_repl cluster_opt_dynamic  */
#line 277 "conffile.y"
                          { (yyval.cluster_type).t = (yyvsp[-2].cluster_ch); (yyval.cluster_type).ival = ((yyvsp[0].cluster_opt_dynamic) * 2) + ((yyvsp[-1].cluster_opt_repl) * 10); }
#line 1921 "conffile.tab.c"
    break;

  case 18: /* cluster_useall: crFORWARD  */
#line 280 "conffile.y"
                           { (yyval.cluster_useall) = FORWARD; }
#line 1927 "conffile.tab.c"
    break;

  case 19: /* cluster_useall: crANY_OF  */
#line 281 "conffile.y"
                                       { (yyval.cluster_useall) = ANYOF; }
#line 1933 "conffile.tab.c"
    break;

  case 20: /* cluster_useall: crFAILOVER  */
#line 282 "conffile.y"
                                       { (yyval.cluster_useall) = FAILOVER; }
#line 1939 "conffile.tab.c"
    break;

  case 21: /* cluster_opt_useall: %empty  */
#line 285 "conffile.y"
                             { (yyval.cluster_opt_useall) = 0; }
#line 1945 "conffile.tab.c"
    break;

  case 22: /* cluster_opt_useall: crUSEALL  */
#line 286 "conffile.y"
                                             { (yyval.cluster_opt_useall) = 1; }
#line 1951 "conffile.tab.c"
    break;

  case 23: /* cluster_ch: crCARBON_CH  */
#line 289 "conffile.y"
                            { (yyval.cluster_ch) = CARBON_CH; }
#line 1957 "conffile.tab.c"
    break;

  case 24: /* cluster_ch: crFNV1A_CH  */
#line 290 "conffile.y"
                                    { (yyval.cluster_ch) = FNV1A_CH; }
#line 1963 "conffile.tab.c"
    break;

  case 25: /* cluster_ch: crJUMP_FNV1A_CH  */
#line 291 "conffile.y"
                                    { (yyval.cluster_ch) = JUMP_CH; }
#line 1969 "conffile.tab.c"
    break;

  case 26: /* cluster_opt_repl: %empty  */
#line 294 "conffile.y"
                                              { (yyval.cluster_opt_repl) = 1; }
#line 1975 "conffile.tab.c"
    break;

  case 27: /* cluster_opt_repl: crREPLICATION crINTVAL  */
#line 295 "conffile.y"
                                                              { (yyval.cluster_opt_repl) = (yyvsp[0].crINTVAL); }
#line 1981 "conffile.tab.c"
    break;

  case 28: /* cluster_opt_dynamic: %empty  */
#line 298 "conffile.y"
                               { (yyval.cluster_opt_dynamic) = 0; }
#line 1987 "conffile.tab.c"
    break;

  case 29: /* cluster_opt_dynamic: crDYNAMIC  */
#line 299 "conffile.y"
                                               { (yyval.cluster_opt_dynamic) = 1; }
#line 1993 "conffile.tab.c"
    break;

  case 30: /* cluster_file: crFILE crIP  */
#line 302 "conffile.y"
                          { (yyval.cluster_file).t = FILELOGIP; (yyval.cluster_file).ival = 0; }
#line 1999 "conffile.tab.c"
    break;

  case 31: /* cluster_file: crFILE  */
#line 303 "conffile.y"
                                      { (yyval.cluster_file).t = FILELOG; (yyval.cluster_file).ival = 0; }
#line 2005 "conffile.tab.c"
    break;

  case 32: /* cluster_paths: cluster_path cluster_opt_path  */
#line 306 "conffile.y"
                                                   { (yyvsp[-1].cluster_path)->next = (yyvsp[0].cluster_opt_path); (yyval.cluster_paths) = (yyvsp[-1].cluster_path); }
#line 2011 "conffile.tab.c"
    break;

  case 33: /* cluster_opt_path: %empty  */
#line 308 "conffile.y"
                               { (yyval.cluster_opt_path) = NULL; }
#line 2017 "conffile.tab.c"
    break;

  case 34: /* cluster_opt_path: cluster_path  */
#line 309 "conffile.y"
                                               { (yyval.cluster_opt_path) = (yyvsp[0].cluster_path); }
#line 2023 "conffile.tab.c"
    break;

  case 35: /* cluster_path: crSTRING  */
#line 312 "conffile.y"
                        {
				struct _clhost *ret = ra_malloc(palloc, sizeof(struct _clhost));
				char *err = router_validate_path(rtr, (yyvsp[0].crSTRING));
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
				ret->ip = (yyvsp[0].crSTRING);
				ret->port = GRAPHITE_PORT;
				ret->saddr = NULL;
				ret->hint = NULL;
				ret->inst = NULL;
				ret->proto = CON_FILE;
				ret->next = NULL;
				(yyval.cluster_path) = ret;
			}
#line 2045 "conffile.tab.c"
    break;

  case 36: /* cluster_hosts: cluster_host cluster_opt_host  */
#line 331 "conffile.y"
                                                   { (yyvsp[-1].cluster_host)->next = (yyvsp[0].cluster_opt_host); (yyval.cluster_hosts) = (yyvsp[-1].cluster_host); }
#line 2051 "conffile.tab.c"
    break;

  case 37: /* cluster_opt_host: %empty  */
#line 333 "conffile.y"
                                { (yyval.cluster_opt_host) = NULL; }
#line 2057 "conffile.tab.c"
    break;

  case 38: /* cluster_opt_host: cluster_hosts  */
#line 334 "conffile.y"
                                                { (yyval.cluster_opt_host) = (yyvsp[0].cluster_hosts); }
#line 2063 "conffile.tab.c"
    break;

  case 39: /* cluster_host: crSTRING cluster_opt_instance cluster_opt_proto cluster_opt_type cluster_opt_transport  */
#line 339 "conffile.y"
                        {
			  	struct _clhost *ret = ra_malloc(palloc, sizeof(struct _clhost));
				char *err = router_validate_address(
						rtr,
						&(ret->ip), &(ret->port), &(ret->saddr), &(ret->hint),
						(yyvsp[-4].crSTRING), (yyvsp[-2].cluster_opt_proto));
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
				ret->inst = (yyvsp[-3].cluster_opt_instance);
				ret->proto = (yyvsp[-2].cluster_opt_proto);
				ret->type = (yyvsp[-1].cluster_opt_type);
				ret->trnsp = (yyvsp[0].cluster_opt_transport);
				ret->next = NULL;
				(yyval.cluster_host) = ret;
			  }
#line 2086 "conffile.tab.c"
    break;

  case 40: /* cluster_opt_instance: %empty  */
#line 358 "conffile.y"
                                         { (yyval.cluster_opt_instance) = NULL; }
#line 2092 "conffile.tab.c"
    break;

  case 41: /* cluster_opt_instance: '=' crSTRING  */
#line 359 "conffile.y"
                                                             { (yyval.cluster_opt_instance) = (yyvsp[0].crSTRING); }
#line 2098 "conffile.tab.c"
    break;

  case 42: /* cluster_opt_instance: '=' crINTVAL  */
#line 361 "conffile.y"
                                        {
						(yyval.cluster_opt_instance) = ra_malloc(palloc, sizeof(char) * 12);
						if ((yyval.cluster_opt_instance) == NULL) {
							logerr("out of memory\n");
							YYABORT;
						}
						snprintf((yyval.cluster_opt_instance), 12, "%d", (yyvsp[0].crINTVAL));
					}
#line 2111 "conffile.tab.c"
    break;

  case 43: /* cluster_opt_proto: %empty  */
#line 370 "conffile.y"
                                 { (yyval.cluster_opt_proto) = CON_TCP; }
#line 2117 "conffile.tab.c"
    break;

  case 44: /* cluster_opt_proto: crPROTO crUDP  */
#line 371 "conffile.y"
                                                 { (yyval.cluster_opt_proto) = CON_UDP; }
#line 2123 "conffile.tab.c"
    break;

  case 45: /* cluster_opt_proto: crPROTO crTCP  */
#line 372 "conffile.y"
                                                 { (yyval.cluster_opt_proto) = CON_TCP; }
#line 2129 "conffile.tab.c"
    break;

  case 46: /* cluster_opt_type: %empty  */
#line 375 "conffile.y"
                                      { (yyval.cluster_opt_type) = T_LINEMODE; }
#line 2135 "conffile.tab.c"
    break;

  case 47: /* cluster_opt_type: crTYPE crLINEMODE  */
#line 376 "conffile.y"
                                                      { (yyval.cluster_opt_type) = T_LINEMODE; }
#line 2141 "conffile.tab.c"
    break;

  case 48: /* cluster_opt_type: crTYPE crSYSLOGMODE  */
#line 377 "conffile.y"
                                                      { (yyval.cluster_opt_type) = T_SYSLOGMODE; }
#line 2147 "conffile.tab.c"
    break;

  case 49: /* cluster_opt_transport: %empty  */
#line 380 "conffile.y"
                                      { (yyval.cluster_opt_transport) = W_PLAIN; }
#line 2153 "conffile.tab.c"
    break;

  case 50: /* cluster_opt_transport: cluster_transport_trans cluster_transport_opt_ssl  */
#line 383 "conffile.y"
                                         {
					 	if ((yyvsp[0].cluster_transport_opt_ssl) == W_PLAIN) {
							(yyval.cluster_opt_transport) = (yyvsp[-1].cluster_transport_trans);
						} else {
							(yyval.cluster_opt_transport) = (yyvsp[-1].cluster_transport_trans) | (yyvsp[0].cluster_transport_opt_ssl);
						}
					 }
#line 2165 "conffile.tab.c"
    break;

  case 51: /* cluster_transport_trans: crTRANSPORT crPLAIN  */
#line 391 "conffile.y"
                                              { (yyval.cluster_transport_trans) = W_PLAIN; }
#line 2171 "conffile.tab.c"
    break;

  case 52: /* cluster_transport_trans: crTRANSPORT crGZIP  */
#line 392 "conffile.y"
                                                                  {
#ifdef HAVE_GZIP
							(yyval.cluster_transport_trans) = W_GZIP;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature gzip not compiled in");
							YYERROR;
#endif
					    }
#line 2186 "conffile.tab.c"
    break;

  case 53: /* cluster_transport_trans: crTRANSPORT crLZ4  */
#line 402 "conffile.y"
                                                                   {
#ifdef HAVE_LZ4
							(yyval.cluster_transport_trans) = W_LZ4;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature lz4 not compiled in");
							YYERROR;
#endif
					    }
#line 2201 "conffile.tab.c"
    break;

  case 54: /* cluster_transport_trans: crTRANSPORT crSNAPPY  */
#line 412 "conffile.y"
                                                                      {
#ifdef HAVE_SNAPPY
							(yyval.cluster_transport_trans) = W_SNAPPY;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature snappy not compiled in");
							YYERROR;
#endif
					    }
#line 2216 "conffile.tab.c"
    break;

  case 55: /* cluster_transport_opt_ssl: %empty  */
#line 423 "conffile.y"
                           { (yyval.cluster_transport_opt_ssl) = W_PLAIN; }
#line 2222 "conffile.tab.c"
    break;

  case 56: /* cluster_transport_opt_ssl: crSSL  */
#line 425 "conffile.y"
                                                 {
#ifdef HAVE_SSL
							(yyval.cluster_transport_opt_ssl) = W_SSL;
#else
							router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc,
								"feature ssl not compiled in");
							YYERROR;
#endif
					     }
#line 2237 "conffile.tab.c"
    break;

  case 57: /* match: crMATCH match_exprs match_opt_validate match_opt_route match_opt_send_to match_opt_stop  */
#line 441 "conffile.y"
         {
	 	/* each expr comes with an allocated route, populate it */
		struct _maexpr *we;
		destinations *d = NULL;
		char *err;

		if ((yyvsp[-3].match_opt_validate) != NULL) {
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
			d->cl->members.validation->rule = (yyvsp[-3].match_opt_validate)->r;
			d->cl->members.validation->action = (yyvsp[-3].match_opt_validate)->drop ? VAL_DROP : VAL_LOG;
		}
		/* add destinations to the chain */
		if (d != NULL) {
			d->next = (yyvsp[-1].match_opt_send_to);
		} else {
			d = (yyvsp[-1].match_opt_send_to);
		}
		/* replace with copy on the router allocator */
		if ((yyvsp[-2].match_opt_route) != NULL)
			(yyvsp[-2].match_opt_route) = ra_strdup(ralloc, (yyvsp[-2].match_opt_route));
		for (we = (yyvsp[-4].match_exprs); we != NULL; we = we->next) {
			we->r->next = NULL;
			we->r->dests = d;
			we->r->masq = (yyvsp[-2].match_opt_route);
			we->r->stop = (yyvsp[-1].match_opt_send_to) == NULL ? 0 :
					(yyvsp[-1].match_opt_send_to)->cl->type == BLACKHOLE ? 1 : (yyvsp[0].match_opt_stop);
			err = router_add_route(rtr, we->r);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
		}
	 }
#line 2293 "conffile.tab.c"
    break;

  case 58: /* match_exprs: '*'  */
#line 495 "conffile.y"
                   {
			if (((yyval.match_exprs) = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	(yyval.match_exprs)->r = NULL;
			if (router_validate_expression(rtr, &((yyval.match_exprs)->r), "*") != NULL)
				YYABORT;
			(yyval.match_exprs)->drop = 0;
			(yyval.match_exprs)->next = NULL;
		   }
#line 2309 "conffile.tab.c"
    break;

  case 59: /* match_exprs: match_exprs2  */
#line 506 "conffile.y"
                                  { (yyval.match_exprs) = (yyvsp[0].match_exprs2); }
#line 2315 "conffile.tab.c"
    break;

  case 60: /* match_exprs2: match_expr match_opt_expr  */
#line 509 "conffile.y"
                                              { (yyvsp[-1].match_expr)->next = (yyvsp[0].match_opt_expr); (yyval.match_exprs2) = (yyvsp[-1].match_expr); }
#line 2321 "conffile.tab.c"
    break;

  case 61: /* match_opt_expr: %empty  */
#line 511 "conffile.y"
                             { (yyval.match_opt_expr) = NULL; }
#line 2327 "conffile.tab.c"
    break;

  case 62: /* match_opt_expr: match_exprs2  */
#line 512 "conffile.y"
                                         { (yyval.match_opt_expr) = (yyvsp[0].match_exprs2); }
#line 2333 "conffile.tab.c"
    break;

  case 63: /* match_expr: crSTRING  */
#line 516 "conffile.y"
                  {
			char *err;
			if (((yyval.match_expr) = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	(yyval.match_expr)->r = NULL;
		  	err = router_validate_expression(rtr, &((yyval.match_expr)->r), (yyvsp[0].crSTRING));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
			(yyval.match_expr)->drop = 0;
			(yyval.match_expr)->next = NULL;
		  }
#line 2354 "conffile.tab.c"
    break;

  case 64: /* match_opt_validate: %empty  */
#line 534 "conffile.y"
                    { (yyval.match_opt_validate) = NULL; }
#line 2360 "conffile.tab.c"
    break;

  case 65: /* match_opt_validate: crVALIDATE crSTRING crELSE match_log_or_drop  */
#line 536 "conffile.y"
                                  {
					char *err;
					if (((yyval.match_opt_validate) = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
						logerr("out of memory\n");
						YYABORT;
					}
					(yyval.match_opt_validate)->r = NULL;
					err = router_validate_expression(rtr, &((yyval.match_opt_validate)->r), (yyvsp[-2].crSTRING));
					if (err != NULL) {
						router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc, err);
						YYERROR;
					}
					(yyval.match_opt_validate)->drop = (yyvsp[0].match_log_or_drop);
					(yyval.match_opt_validate)->next = NULL;
				  }
#line 2381 "conffile.tab.c"
    break;

  case 66: /* match_log_or_drop: crLOG  */
#line 554 "conffile.y"
                          { (yyval.match_log_or_drop) = 0; }
#line 2387 "conffile.tab.c"
    break;

  case 67: /* match_log_or_drop: crDROP  */
#line 555 "conffile.y"
                                          { (yyval.match_log_or_drop) = 1; }
#line 2393 "conffile.tab.c"
    break;

  case 68: /* match_opt_route: %empty  */
#line 558 "conffile.y"
                 { (yyval.match_opt_route) = NULL; }
#line 2399 "conffile.tab.c"
    break;

  case 69: /* match_opt_route: crROUTE crUSING crSTRING  */
#line 559 "conffile.y"
                                                            { (yyval.match_opt_route) = (yyvsp[0].crSTRING); }
#line 2405 "conffile.tab.c"
    break;

  case 70: /* match_opt_send_to: %empty  */
#line 562 "conffile.y"
                   { (yyval.match_opt_send_to) = NULL; }
#line 2411 "conffile.tab.c"
    break;

  case 71: /* match_opt_send_to: match_send_to  */
#line 563 "conffile.y"
                                                 { (yyval.match_opt_send_to) = (yyvsp[0].match_send_to); }
#line 2417 "conffile.tab.c"
    break;

  case 72: /* match_send_to: crSEND crTO match_dsts  */
#line 566 "conffile.y"
                                            { (yyval.match_send_to) = (yyvsp[0].match_dsts); }
#line 2423 "conffile.tab.c"
    break;

  case 73: /* match_dsts: crBLACKHOLE  */
#line 570 "conffile.y"
                  {
			if (((yyval.match_dsts) = ra_malloc(ralloc, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			if (router_validate_cluster(rtr, &((yyval.match_dsts)->cl), "blackhole") != NULL)
				YYABORT;
			(yyval.match_dsts)->next = NULL;
		  }
#line 2437 "conffile.tab.c"
    break;

  case 74: /* match_dsts: match_dsts2  */
#line 579 "conffile.y"
                                { (yyval.match_dsts) = (yyvsp[0].match_dsts2); }
#line 2443 "conffile.tab.c"
    break;

  case 75: /* match_dsts2: match_dst match_opt_dst  */
#line 582 "conffile.y"
                                           { (yyvsp[-1].match_dst)->next = (yyvsp[0].match_opt_dst); (yyval.match_dsts2) = (yyvsp[-1].match_dst); }
#line 2449 "conffile.tab.c"
    break;

  case 76: /* match_opt_dst: %empty  */
#line 584 "conffile.y"
                           { (yyval.match_opt_dst) = NULL; }
#line 2455 "conffile.tab.c"
    break;

  case 77: /* match_opt_dst: match_dsts2  */
#line 585 "conffile.y"
                                       { (yyval.match_opt_dst) = (yyvsp[0].match_dsts2); }
#line 2461 "conffile.tab.c"
    break;

  case 78: /* match_dst: crSTRING  */
#line 589 "conffile.y"
                 {
			char *err;
			if (((yyval.match_dst) = ra_malloc(ralloc, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			err = router_validate_cluster(rtr, &((yyval.match_dst)->cl), (yyvsp[0].crSTRING));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}
			(yyval.match_dst)->next = NULL;
		 }
#line 2479 "conffile.tab.c"
    break;

  case 79: /* match_opt_stop: %empty  */
#line 604 "conffile.y"
                       { (yyval.match_opt_stop) = 0; }
#line 2485 "conffile.tab.c"
    break;

  case 80: /* match_opt_stop: crSTOP  */
#line 605 "conffile.y"
                                   { (yyval.match_opt_stop) = 1; }
#line 2491 "conffile.tab.c"
    break;

  case 81: /* rewrite: crREWRITE crSTRING crINTO crSTRING  */
#line 611 "conffile.y"
           {
		char *err;
		route *r = NULL;
		cluster *cl;

		err = router_validate_expression(rtr, &r, (yyvsp[-2].crSTRING));
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
		cl->members.replacement = ra_strdup(ralloc, (yyvsp[0].crSTRING));
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
#line 2536 "conffile.tab.c"
    break;

  case 82: /* aggregate: crAGGREGATE match_exprs2 crEVERY crINTVAL crSECONDS crEXPIRE crAFTER crINTVAL crSECONDS aggregate_opt_timestamp aggregate_computes aggregate_opt_send_to match_opt_stop  */
#line 661 "conffile.y"
                 {
		 	cluster *w;
			aggregator *a;
			destinations *d;
			struct _agcomp *acw;
			struct _maexpr *we;
			char *err;

			if ((yyvsp[-9].crINTVAL) <= 0) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
					"interval must be > 0");
				YYERROR;
			}
			if ((yyvsp[-5].crINTVAL) <= 0) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
					"expire must be > 0");
				YYERROR;
			}
			if ((yyvsp[-5].crINTVAL) <= (yyvsp[-9].crINTVAL)) {
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

			a = aggregator_new((yyvsp[-9].crINTVAL), (yyvsp[-5].crINTVAL), (yyvsp[-3].aggregate_opt_timestamp));
			if (a == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			if ((err = router_add_aggregator(rtr, a)) != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}

			w->members.aggregation = a;
			
			for (acw = (yyvsp[-2].aggregate_computes); acw != NULL; acw = acw->next) {
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
			d->next = (yyvsp[-1].aggregate_opt_send_to);

			for (we = (yyvsp[-11].match_exprs2); we != NULL; we = we->next) {
				we->r->next = NULL;
				we->r->dests = d;
				we->r->stop = (yyvsp[0].match_opt_stop);
				err = router_add_route(rtr, we->r);
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
			}

			if ((yyvsp[-1].aggregate_opt_send_to) != NULL)
				router_add_stubroute(rtr, AGGRSTUB, w, (yyvsp[-1].aggregate_opt_send_to));
		 }
#line 2624 "conffile.tab.c"
    break;

  case 83: /* aggregate_opt_timestamp: %empty  */
#line 746 "conffile.y"
                         { (yyval.aggregate_opt_timestamp) = TS_END; }
#line 2630 "conffile.tab.c"
    break;

  case 84: /* aggregate_opt_timestamp: crTIMESTAMP crAT aggregate_ts_when crOF crBUCKET  */
#line 749 "conffile.y"
                                           { (yyval.aggregate_opt_timestamp) = (yyvsp[-2].aggregate_ts_when); }
#line 2636 "conffile.tab.c"
    break;

  case 85: /* aggregate_ts_when: crSTART  */
#line 752 "conffile.y"
                            { (yyval.aggregate_ts_when) = TS_START; }
#line 2642 "conffile.tab.c"
    break;

  case 86: /* aggregate_ts_when: crMIDDLE  */
#line 753 "conffile.y"
                                            { (yyval.aggregate_ts_when) = TS_MIDDLE; }
#line 2648 "conffile.tab.c"
    break;

  case 87: /* aggregate_ts_when: crEND  */
#line 754 "conffile.y"
                                            { (yyval.aggregate_ts_when) = TS_END; }
#line 2654 "conffile.tab.c"
    break;

  case 88: /* aggregate_computes: aggregate_compute aggregate_opt_compute  */
#line 758 "conffile.y"
                                  { (yyvsp[-1].aggregate_compute)->next = (yyvsp[0].aggregate_opt_compute); (yyval.aggregate_computes) = (yyvsp[-1].aggregate_compute); }
#line 2660 "conffile.tab.c"
    break;

  case 89: /* aggregate_opt_compute: %empty  */
#line 761 "conffile.y"
                                          { (yyval.aggregate_opt_compute) = NULL; }
#line 2666 "conffile.tab.c"
    break;

  case 90: /* aggregate_opt_compute: aggregate_computes  */
#line 762 "conffile.y"
                                                              { (yyval.aggregate_opt_compute) = (yyvsp[0].aggregate_computes); }
#line 2672 "conffile.tab.c"
    break;

  case 91: /* aggregate_compute: crCOMPUTE aggregate_comp_type crWRITE crTO crSTRING  */
#line 766 "conffile.y"
                                 {
					(yyval.aggregate_compute) = ra_malloc(palloc, sizeof(struct _agcomp));
					if ((yyval.aggregate_compute) == NULL) {
						logerr("malloc failed\n");
						YYABORT;
					}
				 	(yyval.aggregate_compute)->ctype = (yyvsp[-3].aggregate_comp_type).ctype;
					(yyval.aggregate_compute)->pctl = (yyvsp[-3].aggregate_comp_type).pctl;
					(yyval.aggregate_compute)->metric = (yyvsp[0].crSTRING);
					(yyval.aggregate_compute)->next = NULL;
				 }
#line 2688 "conffile.tab.c"
    break;

  case 92: /* aggregate_comp_type: crSUM  */
#line 779 "conffile.y"
                                  { (yyval.aggregate_comp_type).ctype = SUM; }
#line 2694 "conffile.tab.c"
    break;

  case 93: /* aggregate_comp_type: crCOUNT  */
#line 780 "conffile.y"
                                                  { (yyval.aggregate_comp_type).ctype = CNT; }
#line 2700 "conffile.tab.c"
    break;

  case 94: /* aggregate_comp_type: crMAX  */
#line 781 "conffile.y"
                                                  { (yyval.aggregate_comp_type).ctype = MAX; }
#line 2706 "conffile.tab.c"
    break;

  case 95: /* aggregate_comp_type: crMIN  */
#line 782 "conffile.y"
                                                  { (yyval.aggregate_comp_type).ctype = MIN; }
#line 2712 "conffile.tab.c"
    break;

  case 96: /* aggregate_comp_type: crAVERAGE  */
#line 783 "conffile.y"
                                                  { (yyval.aggregate_comp_type).ctype = AVG; }
#line 2718 "conffile.tab.c"
    break;

  case 97: /* aggregate_comp_type: crMEDIAN  */
#line 784 "conffile.y"
                                                  { (yyval.aggregate_comp_type).ctype = MEDN; }
#line 2724 "conffile.tab.c"
    break;

  case 98: /* aggregate_comp_type: crPERCENTILE  */
#line 786 "conffile.y"
                                   {
				    if ((yyvsp[0].crPERCENTILE) < 1 || (yyvsp[0].crPERCENTILE) > 99) {
						router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
							"percentile<x>: value x must be between 1 and 99");
						YYERROR;
					}
				   	(yyval.aggregate_comp_type).ctype = PCTL;
					(yyval.aggregate_comp_type).pctl = (unsigned char)(yyvsp[0].crPERCENTILE);
				   }
#line 2738 "conffile.tab.c"
    break;

  case 99: /* aggregate_comp_type: crVARIANCE  */
#line 795 "conffile.y"
                                                  { (yyval.aggregate_comp_type).ctype = VAR; }
#line 2744 "conffile.tab.c"
    break;

  case 100: /* aggregate_comp_type: crSTDDEV  */
#line 796 "conffile.y"
                                                  { (yyval.aggregate_comp_type).ctype = SDEV; }
#line 2750 "conffile.tab.c"
    break;

  case 101: /* aggregate_opt_send_to: %empty  */
#line 799 "conffile.y"
                                     { (yyval.aggregate_opt_send_to) = NULL; }
#line 2756 "conffile.tab.c"
    break;

  case 102: /* aggregate_opt_send_to: match_send_to  */
#line 800 "conffile.y"
                                                         { (yyval.aggregate_opt_send_to) = (yyvsp[0].match_send_to); }
#line 2762 "conffile.tab.c"
    break;

  case 103: /* send: crSEND crSTATISTICS crTO match_dsts match_opt_stop  */
#line 806 "conffile.y"
        {
		char *err = router_set_statistics(rtr, (yyvsp[-1].match_dsts));
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
		logerr("warning: 'send statistics to ...' is deprecated and will be "
				"removed in a future version, use 'statistics send to ...' "
				"instead\n");
	}
#line 2777 "conffile.tab.c"
    break;

  case 104: /* statistics: crSTATISTICS statistics_opt_interval statistics_opt_counters statistics_opt_prefix aggregate_opt_send_to match_opt_stop  */
#line 826 "conffile.y"
                  {
		  	char *err;
		  	err = router_set_collectorvals(rtr, (yyvsp[-4].statistics_opt_interval), (yyvsp[-2].statistics_opt_prefix), (yyvsp[-3].statistics_opt_counters));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}

			if ((yyvsp[-1].aggregate_opt_send_to) != NULL) {
				err = router_set_statistics(rtr, (yyvsp[-1].aggregate_opt_send_to));
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
			}
		  }
#line 2799 "conffile.tab.c"
    break;

  case 105: /* statistics_opt_interval: %empty  */
#line 845 "conffile.y"
                         { (yyval.statistics_opt_interval) = -1; }
#line 2805 "conffile.tab.c"
    break;

  case 106: /* statistics_opt_interval: crSUBMIT crEVERY crINTVAL crSECONDS  */
#line 847 "conffile.y"
                                           {
					   	if ((yyvsp[-1].crINTVAL) <= 0) {
							router_yyerror(&yylloc, yyscanner, rtr,
									ralloc, palloc, "interval must be > 0");
							YYERROR;
						}
						(yyval.statistics_opt_interval) = (yyvsp[-1].crINTVAL);
					   }
#line 2818 "conffile.tab.c"
    break;

  case 107: /* statistics_opt_counters: %empty  */
#line 857 "conffile.y"
                                                               { (yyval.statistics_opt_counters) = CUM; }
#line 2824 "conffile.tab.c"
    break;

  case 108: /* statistics_opt_counters: crRESET crCOUNTERS crAFTER crINTERVAL  */
#line 858 "conffile.y"
                                                                                   { (yyval.statistics_opt_counters) = SUB; }
#line 2830 "conffile.tab.c"
    break;

  case 109: /* statistics_opt_prefix: %empty  */
#line 861 "conffile.y"
                                                        { (yyval.statistics_opt_prefix) = NULL; }
#line 2836 "conffile.tab.c"
    break;

  case 110: /* statistics_opt_prefix: crPREFIX crWITH crSTRING  */
#line 862 "conffile.y"
                                                                            { (yyval.statistics_opt_prefix) = (yyvsp[0].crSTRING); }
#line 2842 "conffile.tab.c"
    break;

  case 111: /* listen: crLISTEN listener  */
#line 868 "conffile.y"
          {
	  	struct _rcptr *walk;
		char *err;
		struct _rcptr_sslprotos *prwalk;
		int protomin;
		int protomax;

		for (walk = (yyvsp[0].listener)->rcptr; walk != NULL; walk = walk->next) {
			protomin = 0;
			protomax = 0;
			prwalk = (yyvsp[0].listener)->transport->protos;
			for (; prwalk != NULL; prwalk = prwalk->next) {
				if (prwalk->prtype == _rp_PROTOMIN)
					protomin = prwalk->prver;
				else if (prwalk->prtype == _rp_PROTOMAX)
					protomax = prwalk->prver;
			}
			err = router_add_listener(rtr, (yyvsp[0].listener)->type,
				(yyvsp[0].listener)->transport->mode, (yyvsp[0].listener)->transport->pemcert,
				protomin, protomax,
				walk->ctype, walk->ip, walk->port, walk->saddr);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
		}
	  }
#line 2875 "conffile.tab.c"
    break;

  case 112: /* listener: crTYPE crLINEMODE transport_mode receptors  */
#line 899 "conffile.y"
                {
			if (((yyval.listener) = ra_malloc(palloc, sizeof(struct _lsnr))) == NULL) {
				logerr("malloc failed\n");
				YYABORT;
			}
			(yyval.listener)->type = T_LINEMODE;
			(yyval.listener)->transport = (yyvsp[-1].transport_mode);
			(yyval.listener)->rcptr = (yyvsp[0].receptors);
			if ((yyvsp[-1].transport_mode)->mode != W_PLAIN) {
				struct _rcptr *walk;

				for (walk = (yyvsp[0].receptors); walk != NULL; walk = walk->next) {
					if (walk->ctype == CON_UDP) {
						router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
							"cannot use UDP transport for "
							"compressed/encrypted stream");
						YYERROR;
					}
				}
			}
		}
#line 2901 "conffile.tab.c"
    break;

  case 113: /* transport_opt_ssl: %empty  */
#line 923 "conffile.y"
                                 {
				 	(yyval.transport_opt_ssl) = NULL;
				 }
#line 2909 "conffile.tab.c"
    break;

  case 114: /* transport_opt_ssl: crSSL crSTRING transport_opt_ssl_protos  */
#line 927 "conffile.y"
                                 {
#ifdef HAVE_SSL
					if (((yyval.transport_opt_ssl) = ra_malloc(palloc,
							sizeof(struct _rcptr_trsp))) == NULL)
					{
						logerr("malloc failed\n");
						YYABORT;
					}
					(yyval.transport_opt_ssl)->mode = W_SSL;
					(yyval.transport_opt_ssl)->pemcert = ra_strdup(ralloc, (yyvsp[-1].crSTRING));
					(yyval.transport_opt_ssl)->protos = (yyvsp[0].transport_opt_ssl_protos);
#else
					router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc,
						"feature ssl not compiled in");
					YYERROR;
#endif
				 }
#line 2932 "conffile.tab.c"
    break;

  case 115: /* transport_opt_ssl_protos: %empty  */
#line 947 "conffile.y"
                                                {
							(yyval.transport_opt_ssl_protos) = NULL;
						}
#line 2940 "conffile.tab.c"
    break;

  case 116: /* transport_opt_ssl_protos: transport_ssl_proto transport_opt_ssl_protos  */
#line 951 "conffile.y"
                                                {
							(yyvsp[-1].transport_ssl_proto)->next = (yyvsp[0].transport_opt_ssl_protos); (yyval.transport_opt_ssl_protos) = (yyvsp[-1].transport_ssl_proto);
						}
#line 2948 "conffile.tab.c"
    break;

  case 117: /* transport_ssl_proto: transport_ssl_prototype transport_ssl_protover  */
#line 956 "conffile.y"
                                   {
						if (((yyval.transport_ssl_proto) = ra_malloc(palloc,
								sizeof(struct _rcptr_sslprotos))) == NULL)
						{
							logerr("malloc failed\n");
							YYABORT;
						}
						(yyval.transport_ssl_proto)->prtype = (yyvsp[-1].transport_ssl_prototype);
						(yyval.transport_ssl_proto)->prver = (yyvsp[0].transport_ssl_protover);
						(yyval.transport_ssl_proto)->next = NULL;
				   }
#line 2964 "conffile.tab.c"
    break;

  case 118: /* transport_ssl_prototype: crPROTOMIN  */
#line 968 "conffile.y"
                                     { (yyval.transport_ssl_prototype) = _rp_PROTOMIN; }
#line 2970 "conffile.tab.c"
    break;

  case 119: /* transport_ssl_prototype: crPROTOMAX  */
#line 969 "conffile.y"
                                                         { (yyval.transport_ssl_prototype) = _rp_PROTOMAX; }
#line 2976 "conffile.tab.c"
    break;

  case 120: /* transport_ssl_protover: crSSL3  */
#line 971 "conffile.y"
                                  { (yyval.transport_ssl_protover) = _rp_SSL3;   }
#line 2982 "conffile.tab.c"
    break;

  case 121: /* transport_ssl_protover: crTLS1_0  */
#line 972 "conffile.y"
                                                      { (yyval.transport_ssl_protover) = _rp_TLS1_0; }
#line 2988 "conffile.tab.c"
    break;

  case 122: /* transport_ssl_protover: crTLS1_1  */
#line 973 "conffile.y"
                                                      { (yyval.transport_ssl_protover) = _rp_TLS1_1; }
#line 2994 "conffile.tab.c"
    break;

  case 123: /* transport_ssl_protover: crTLS1_2  */
#line 974 "conffile.y"
                                                      { (yyval.transport_ssl_protover) = _rp_TLS1_2; }
#line 3000 "conffile.tab.c"
    break;

  case 124: /* transport_ssl_protover: crTLS1_3  */
#line 975 "conffile.y"
                                                      { (yyval.transport_ssl_protover) = _rp_TLS1_3; }
#line 3006 "conffile.tab.c"
    break;

  case 125: /* transport_mode_trans: crTRANSPORT crPLAIN  */
#line 979 "conffile.y"
                                        {
						if (((yyval.transport_mode_trans) = ra_malloc(palloc,
								sizeof(struct _rcptr_trsp))) == NULL)
						{
							logerr("malloc failed\n");
							YYABORT;
						}
						(yyval.transport_mode_trans)->mode = W_PLAIN;
					}
#line 3020 "conffile.tab.c"
    break;

  case 126: /* transport_mode_trans: crTRANSPORT crGZIP  */
#line 989 "conffile.y"
                                        {
#ifdef HAVE_GZIP
						if (((yyval.transport_mode_trans) = ra_malloc(palloc,
								sizeof(struct _rcptr_trsp))) == NULL)
						{
							logerr("malloc failed\n");
							YYABORT;
						}
						(yyval.transport_mode_trans)->mode = W_GZIP;
#else
						router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc,
							"feature gzip not compiled in");
						YYERROR;
#endif
					}
#line 3041 "conffile.tab.c"
    break;

  case 127: /* transport_mode_trans: crTRANSPORT crLZ4  */
#line 1006 "conffile.y"
                                        {
#ifdef HAVE_LZ4
						if (((yyval.transport_mode_trans) = ra_malloc(palloc,
								sizeof(struct _rcptr_trsp))) == NULL)
						{
							logerr("malloc failed\n");
							YYABORT;
						}
						(yyval.transport_mode_trans)->mode = W_LZ4;
#else
						router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc,
							"feature lz4 not compiled in");
						YYERROR;
#endif
					}
#line 3062 "conffile.tab.c"
    break;

  case 128: /* transport_mode_trans: crTRANSPORT crSNAPPY  */
#line 1023 "conffile.y"
                                        {
#ifdef HAVE_SNAPPY
						if (((yyval.transport_mode_trans) = ra_malloc(palloc,
								sizeof(struct _rcptr_trsp))) == NULL)
						{
							logerr("malloc failed\n");
							YYABORT;
						}
						(yyval.transport_mode_trans)->mode = W_SNAPPY;
#else
						router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc,
							"feature snappy not compiled in");
						YYERROR;
#endif
					}
#line 3083 "conffile.tab.c"
    break;

  case 129: /* transport_mode: %empty  */
#line 1042 "conffile.y"
                          { 
				if (((yyval.transport_mode) = ra_malloc(palloc,
						sizeof(struct _rcptr_trsp))) == NULL)
				{
					logerr("malloc failed\n");
					YYABORT;
				}
				(yyval.transport_mode)->mode = W_PLAIN;
			  }
#line 3097 "conffile.tab.c"
    break;

  case 130: /* transport_mode: transport_mode_trans transport_opt_ssl  */
#line 1052 "conffile.y"
                          {
			  	if ((yyvsp[0].transport_opt_ssl) == NULL) {
					(yyval.transport_mode) = (yyvsp[-1].transport_mode_trans);
				} else {
					(yyval.transport_mode) = (yyvsp[0].transport_opt_ssl);
					(yyval.transport_mode)->mode |= (yyvsp[-1].transport_mode_trans)->mode;
				}
			  }
#line 3110 "conffile.tab.c"
    break;

  case 131: /* receptors: receptor opt_receptor  */
#line 1062 "conffile.y"
                                       { (yyvsp[-1].receptor)->next = (yyvsp[0].opt_receptor); (yyval.receptors) = (yyvsp[-1].receptor); }
#line 3116 "conffile.tab.c"
    break;

  case 132: /* opt_receptor: %empty  */
#line 1065 "conffile.y"
                        { (yyval.opt_receptor) = NULL; }
#line 3122 "conffile.tab.c"
    break;

  case 133: /* opt_receptor: receptors  */
#line 1066 "conffile.y"
                                    { (yyval.opt_receptor) = (yyvsp[0].receptors);   }
#line 3128 "conffile.tab.c"
    break;

  case 134: /* receptor: crSTRING crPROTO rcptr_proto  */
#line 1070 "conffile.y"
                {
			char *err;
			void *hint = NULL;
			char *w;
			char bcip[24];

			if (((yyval.receptor) = ra_malloc(palloc, sizeof(struct _rcptr))) == NULL) {
				logerr("malloc failed\n");
				YYABORT;
			}
			(yyval.receptor)->ctype = (yyvsp[0].rcptr_proto);

			/* find out if this is just a port */
			for (w = (yyvsp[-2].crSTRING); *w != '\0'; w++)
				if (*w < '0' || *w > '9')
					break;
			if (*w == '\0') {
				snprintf(bcip, sizeof(bcip), ":%s", (yyvsp[-2].crSTRING));
				(yyvsp[-2].crSTRING) = bcip;
			}

			err = router_validate_address(
					rtr,
					&((yyval.receptor)->ip), &((yyval.receptor)->port), &((yyval.receptor)->saddr), &hint,
					(yyvsp[-2].crSTRING), (yyvsp[0].rcptr_proto));
			/* help some static analysis tools to see bcip isn't going
			 * out of scope */
			(yyvsp[-2].crSTRING) = NULL;
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
			free(hint);
			(yyval.receptor)->next = NULL;
		}
#line 3169 "conffile.tab.c"
    break;

  case 135: /* receptor: crSTRING crPROTO crUNIX  */
#line 1107 "conffile.y"
                {
			char *err;

			if (((yyval.receptor) = ra_malloc(palloc, sizeof(struct _rcptr))) == NULL) {
				logerr("malloc failed\n");
				YYABORT;
			}
			(yyval.receptor)->ctype = CON_UNIX;
			(yyval.receptor)->ip = (yyvsp[-2].crSTRING);
			(yyval.receptor)->port = 0;
			(yyval.receptor)->saddr = NULL;
			err = router_validate_path(rtr, (yyvsp[-2].crSTRING));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
			(yyval.receptor)->next = NULL;
		}
#line 3193 "conffile.tab.c"
    break;

  case 136: /* rcptr_proto: crTCP  */
#line 1128 "conffile.y"
                   { (yyval.rcptr_proto) = CON_TCP; }
#line 3199 "conffile.tab.c"
    break;

  case 137: /* rcptr_proto: crUDP  */
#line 1129 "conffile.y"
                           { (yyval.rcptr_proto) = CON_UDP; }
#line 3205 "conffile.tab.c"
    break;

  case 138: /* include: crINCLUDE crSTRING  */
#line 1135 "conffile.y"
           {
	   	if (router_readconfig(rtr, (yyvsp[0].crSTRING), 0, 0, 0, 0, 0, 0, 0) == NULL)
			YYERROR;
	   }
#line 3214 "conffile.tab.c"
    break;


#line 3218 "conffile.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == ROUTER_YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, yyscanner, rtr, ralloc, palloc, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= ROUTER_YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == ROUTER_YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, yyscanner, rtr, ralloc, palloc);
          yychar = ROUTER_YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, yyscanner, rtr, ralloc, palloc);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, yyscanner, rtr, ralloc, palloc, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != ROUTER_YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, yyscanner, rtr, ralloc, palloc);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, yyscanner, rtr, ralloc, palloc);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

