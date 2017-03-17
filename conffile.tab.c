/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

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


/* Copy the first part of user declarations.  */
#line 1 "conffile.y" /* yacc.c:339  */

#include "allocator.h"
#include "conffile.h"
#include "conffile.tab.h"
#include "aggregator.h"

int router_yylex(ROUTER_YYSTYPE *, ROUTER_YYLTYPE *, void *, router *, allocator *, allocator *);

#line 83 "conffile.tab.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* In a future release of Bison, this section will be replaced
   by #include "conffile.tab.h".  */
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
#line 9 "conffile.y" /* yacc.c:355  */

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

#line 148 "conffile.tab.c" /* yacc.c:355  */

/* Token type.  */
#ifndef ROUTER_YYTOKENTYPE
# define ROUTER_YYTOKENTYPE
  enum router_yytokentype
  {
    crCLUSTER = 258,
    crFORWARD = 259,
    crANY_OF = 260,
    crFAILOVER = 261,
    crCARBON_CH = 262,
    crFNV1A_CH = 263,
    crJUMP_FNV1A_CH = 264,
    crFILE = 265,
    crIP = 266,
    crREPLICATION = 267,
    crPROTO = 268,
    crUSEALL = 269,
    crUDP = 270,
    crTCP = 271,
    crMATCH = 272,
    crVALIDATE = 273,
    crELSE = 274,
    crLOG = 275,
    crDROP = 276,
    crSEND = 277,
    crTO = 278,
    crBLACKHOLE = 279,
    crSTOP = 280,
    crREWRITE = 281,
    crINTO = 282,
    crAGGREGATE = 283,
    crEVERY = 284,
    crSECONDS = 285,
    crEXPIRE = 286,
    crAFTER = 287,
    crTIMESTAMP = 288,
    crAT = 289,
    crSTART = 290,
    crMIDDLE = 291,
    crEND = 292,
    crOF = 293,
    crBUCKET = 294,
    crCOMPUTE = 295,
    crSUM = 296,
    crCOUNT = 297,
    crMAX = 298,
    crMIN = 299,
    crAVERAGE = 300,
    crMEDIAN = 301,
    crVARIANCE = 302,
    crSTDDEV = 303,
    crPERCENTILE = 304,
    crWRITE = 305,
    crSTATISTICS = 306,
    crSUBMIT = 307,
    crRESET = 308,
    crCOUNTERS = 309,
    crINTERVAL = 310,
    crPREFIX = 311,
    crWITH = 312,
    crINCLUDE = 313,
    crCOMMENT = 314,
    crSTRING = 315,
    crINTVAL = 316
  };
#endif

/* Value type.  */
#if ! defined ROUTER_YYSTYPE && ! defined ROUTER_YYSTYPE_IS_DECLARED

union ROUTER_YYSTYPE
{

  /* crCOMMENT  */
  char * crCOMMENT;
  /* crSTRING  */
  char * crSTRING;
  /* cluster_opt_instance  */
  char * cluster_opt_instance;
  /* statistics_opt_prefix  */
  char * statistics_opt_prefix;
  /* cluster  */
  cluster * cluster;
  /* statistics_opt_counters  */
  col_mode statistics_opt_counters;
  /* match_opt_send_to  */
  destinations * match_opt_send_to;
  /* match_send_to  */
  destinations * match_send_to;
  /* match_dsts  */
  destinations * match_dsts;
  /* match_dsts2  */
  destinations * match_dsts2;
  /* match_opt_dst  */
  destinations * match_opt_dst;
  /* match_dst  */
  destinations * match_dst;
  /* aggregate_opt_send_to  */
  destinations * aggregate_opt_send_to;
  /* aggregate_opt_timestamp  */
  enum _aggr_timestamp aggregate_opt_timestamp;
  /* aggregate_ts_when  */
  enum _aggr_timestamp aggregate_ts_when;
  /* cluster_useall  */
  enum clusttype cluster_useall;
  /* cluster_ch  */
  enum clusttype cluster_ch;
  /* crPERCENTILE  */
  int crPERCENTILE;
  /* crINTVAL  */
  int crINTVAL;
  /* cluster_opt_useall  */
  int cluster_opt_useall;
  /* cluster_opt_repl  */
  int cluster_opt_repl;
  /* match_log_or_drop  */
  int match_log_or_drop;
  /* match_opt_stop  */
  int match_opt_stop;
  /* statistics_opt_interval  */
  int statistics_opt_interval;
  /* cluster_opt_proto  */
  serv_ctype cluster_opt_proto;
  /* aggregate_comp_type  */
  struct _agcomp aggregate_comp_type;
  /* aggregate_computes  */
  struct _agcomp * aggregate_computes;
  /* aggregate_opt_compute  */
  struct _agcomp * aggregate_opt_compute;
  /* aggregate_compute  */
  struct _agcomp * aggregate_compute;
  /* cluster_paths  */
  struct _clhost * cluster_paths;
  /* cluster_opt_path  */
  struct _clhost * cluster_opt_path;
  /* cluster_path  */
  struct _clhost * cluster_path;
  /* cluster_hosts  */
  struct _clhost * cluster_hosts;
  /* cluster_opt_host  */
  struct _clhost * cluster_opt_host;
  /* cluster_host  */
  struct _clhost * cluster_host;
  /* cluster_type  */
  struct _clust cluster_type;
  /* cluster_file  */
  struct _clust cluster_file;
  /* match_exprs  */
  struct _maexpr * match_exprs;
  /* match_exprs2  */
  struct _maexpr * match_exprs2;
  /* match_opt_expr  */
  struct _maexpr * match_opt_expr;
  /* match_expr  */
  struct _maexpr * match_expr;
  /* match_exprs_subst  */
  struct _maexpr * match_exprs_subst;
  /* match_subst_opt_expr  */
  struct _maexpr * match_subst_opt_expr;
  /* match_subst_expr  */
  struct _maexpr * match_subst_expr;
  /* match_opt_validate  */
  struct _maexpr * match_opt_validate;
#line 313 "conffile.tab.c" /* yacc.c:355  */
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

/* Copy the second part of user declarations.  */

#line 343 "conffile.tab.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

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

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
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


#if ! defined yyoverflow || YYERROR_VERBOSE

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
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined ROUTER_YYLTYPE_IS_TRIVIAL && ROUTER_YYLTYPE_IS_TRIVIAL \
             && defined ROUTER_YYSTYPE_IS_TRIVIAL && ROUTER_YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
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
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  33
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   102

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  65
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  52
/* YYNRULES -- Number of rules.  */
#define YYNRULES  98
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  150

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   316

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,    64,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    62,
       2,    63,     2,     2,     2,     2,     2,     2,     2,     2,
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
      55,    56,    57,    58,    59,    60,    61
};

#if ROUTER_YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    92,    92,    95,    96,    99,   102,   103,   104,   105,
     106,   107,   108,   112,   171,   211,   213,   217,   218,   219,
     222,   223,   226,   227,   228,   231,   232,   235,   236,   239,
     241,   242,   244,   264,   266,   267,   269,   287,   288,   290,
     291,   292,   297,   348,   360,   363,   365,   366,   369,   388,
     391,   392,   395,   414,   415,   434,   435,   438,   439,   442,
     445,   455,   458,   460,   461,   464,   480,   481,   486,   531,
     622,   623,   628,   629,   630,   633,   637,   638,   640,   655,
     656,   657,   658,   659,   660,   661,   671,   672,   675,   676,
     681,   696,   721,   722,   733,   734,   737,   738,   743
};
#endif

#if ROUTER_YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "crCLUSTER", "crFORWARD", "crANY_OF",
  "crFAILOVER", "crCARBON_CH", "crFNV1A_CH", "crJUMP_FNV1A_CH", "crFILE",
  "crIP", "crREPLICATION", "crPROTO", "crUSEALL", "crUDP", "crTCP",
  "crMATCH", "crVALIDATE", "crELSE", "crLOG", "crDROP", "crSEND", "crTO",
  "crBLACKHOLE", "crSTOP", "crREWRITE", "crINTO", "crAGGREGATE", "crEVERY",
  "crSECONDS", "crEXPIRE", "crAFTER", "crTIMESTAMP", "crAT", "crSTART",
  "crMIDDLE", "crEND", "crOF", "crBUCKET", "crCOMPUTE", "crSUM", "crCOUNT",
  "crMAX", "crMIN", "crAVERAGE", "crMEDIAN", "crVARIANCE", "crSTDDEV",
  "crPERCENTILE", "crWRITE", "crSTATISTICS", "crSUBMIT", "crRESET",
  "crCOUNTERS", "crINTERVAL", "crPREFIX", "crWITH", "crINCLUDE",
  "crCOMMENT", "crSTRING", "crINTVAL", "';'", "'='", "'*'", "$accept",
  "stmts", "opt_stmt", "stmt", "command", "cluster", "cluster_type",
  "cluster_useall", "cluster_opt_useall", "cluster_ch", "cluster_opt_repl",
  "cluster_file", "cluster_paths", "cluster_opt_path", "cluster_path",
  "cluster_hosts", "cluster_opt_host", "cluster_host",
  "cluster_opt_instance", "cluster_opt_proto", "match", "match_exprs",
  "match_exprs2", "match_opt_expr", "match_expr", "match_exprs_subst",
  "match_subst_opt_expr", "match_subst_expr", "match_opt_validate",
  "match_log_or_drop", "match_opt_send_to", "match_send_to", "match_dsts",
  "match_dsts2", "match_opt_dst", "match_dst", "match_opt_stop", "rewrite",
  "aggregate", "aggregate_opt_timestamp", "aggregate_ts_when",
  "aggregate_computes", "aggregate_opt_compute", "aggregate_compute",
  "aggregate_comp_type", "aggregate_opt_send_to", "send", "statistics",
  "statistics_opt_interval", "statistics_opt_counters",
  "statistics_opt_prefix", "include", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,    59,    61,    42
};
# endif

#define YYPACT_NINF -77

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-77)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      -2,   -37,   -48,   -26,   -33,   -24,   -15,   -21,    40,   -77,
      -2,   -20,   -77,   -77,   -77,   -77,   -77,   -77,   -77,    25,
     -77,   -77,    23,   -77,   -17,    21,    18,   -77,    17,   -24,
      19,    -6,   -77,   -77,   -77,   -77,   -77,   -77,   -77,   -77,
     -77,   -77,    39,    -9,    38,    41,    -5,    -3,    32,   -77,
     -77,   -22,    -1,     0,   -77,   -77,     1,     4,     7,   -77,
       3,   -77,    -9,   -77,   -77,     8,   -77,   -77,   -77,    -5,
      45,    37,    42,   -77,   -77,   -77,    42,   -77,    10,   -77,
      43,    44,    36,    14,    32,    12,    62,   -77,   -77,   -77,
     -77,   -77,    -7,   -22,   -77,   -77,   -77,   -77,   -77,    46,
     -77,    24,    16,   -77,    42,   -77,     6,   -77,   -77,   -77,
     -77,   -77,    48,   -77,   -77,   -77,   -77,   -77,    20,    52,
      50,    51,    47,   -18,   -38,    32,    47,   -77,   -77,   -77,
      53,   -77,   -77,   -77,   -77,   -77,   -77,   -77,   -77,   -77,
      28,    42,   -77,   -77,    49,    61,   -77,   -77,    26,   -77
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     0,     0,     0,     0,    92,     0,     0,     2,
       3,     0,     6,     7,     8,     9,    10,    11,    12,     0,
      48,    43,    53,    44,    46,     0,     0,    52,     0,    50,
       0,    94,    98,     1,     4,     5,    17,    18,    19,    22,
      23,    24,    28,     0,    20,    25,     0,     0,    57,    47,
      45,     0,     0,     0,    51,    49,     0,     0,    96,    27,
      37,    13,    34,    21,    15,     0,    16,    32,    14,    30,
       0,     0,    66,    58,    60,    65,    66,    61,    63,    68,
       0,     0,     0,     0,    88,     0,    39,    35,    33,    26,
      29,    31,     0,     0,    67,    42,    90,    64,    62,     0,
      93,     0,     0,    89,    66,    38,     0,    36,    55,    56,
      54,    59,     0,    95,    97,    91,    40,    41,     0,     0,
      70,     0,     0,     0,     0,    88,    76,    72,    73,    74,
       0,    79,    80,    81,    82,    83,    84,    86,    87,    85,
       0,    66,    77,    75,     0,     0,    69,    71,     0,    78
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -77,   -77,    79,   -77,   -77,   -77,   -77,   -77,   -77,   -77,
     -77,   -77,   -77,   -77,    27,    30,   -77,   -77,   -77,   -77,
     -77,   -77,    66,   -77,   -77,    64,   -77,   -77,   -77,   -77,
     -77,    54,     2,    22,   -77,   -77,   -76,   -77,   -77,   -77,
     -77,   -32,   -77,   -77,   -77,   -28,   -77,   -77,   -77,   -77,
     -77,   -77
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     8,     9,    10,    11,    12,    43,    44,    64,    45,
      66,    46,    68,    90,    69,    61,    88,    62,    86,   107,
      13,    22,    23,    50,    24,    28,    55,    29,    48,   110,
      72,   103,    76,    77,    98,    78,    95,    14,    15,   122,
     130,   125,   143,   126,   140,   104,    16,    17,    31,    58,
      84,    18
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      96,     1,    74,   131,   132,   133,   134,   135,   136,   137,
     138,   139,    20,   108,   109,     2,    21,   127,   128,   129,
       3,   116,   117,    19,     4,    25,     5,    26,   115,    36,
      37,    38,    39,    40,    41,    42,    27,    30,    75,    32,
      33,    47,    35,    20,    51,    52,    53,    57,    56,     6,
      59,    60,    63,    65,    71,    67,     7,    70,    82,    79,
      93,    80,    81,    83,    92,   146,    85,    94,   101,    89,
      75,   102,   105,    99,   100,   106,   114,   112,   145,   113,
     118,   119,   120,   121,   148,   123,   149,   124,   147,    34,
      49,   144,    87,    54,   142,   111,    91,   141,     0,     0,
      97,     0,    73
};

static const yytype_int16 yycheck[] =
{
      76,     3,    24,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    60,    20,    21,    17,    64,    35,    36,    37,
      22,    15,    16,    60,    26,    51,    28,    60,   104,     4,
       5,     6,     7,     8,     9,    10,    60,    52,    60,    60,
       0,    18,    62,    60,    23,    27,    29,    53,    29,    51,
      11,    60,    14,    12,    22,    60,    58,    60,    54,    60,
      23,    61,    61,    56,    19,   141,    63,    25,    32,    61,
      60,    57,    60,    30,    30,    13,    60,    31,    50,    55,
      32,    61,    30,    33,    23,    34,    60,    40,    39,    10,
      24,    38,    62,    29,   126,    93,    69,   125,    -1,    -1,
      78,    -1,    48
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,    17,    22,    26,    28,    51,    58,    66,    67,
      68,    69,    70,    85,   102,   103,   111,   112,   116,    60,
      60,    64,    86,    87,    89,    51,    60,    60,    90,    92,
      52,   113,    60,     0,    67,    62,     4,     5,     6,     7,
       8,     9,    10,    71,    72,    74,    76,    18,    93,    87,
      88,    23,    27,    29,    90,    91,    29,    53,   114,    11,
      60,    80,    82,    14,    73,    12,    75,    60,    77,    79,
      60,    22,    95,    96,    24,    60,    97,    98,   100,    60,
      61,    61,    54,    56,   115,    63,    83,    80,    81,    61,
      78,    79,    19,    23,    25,   101,   101,    98,    99,    30,
      30,    32,    57,    96,   110,    60,    13,    84,    20,    21,
      94,    97,    31,    55,    60,   101,    15,    16,    32,    61,
      30,    33,   104,    34,    40,   106,   108,    35,    36,    37,
     105,    41,    42,    43,    44,    45,    46,    47,    48,    49,
     109,   110,   106,   107,    38,    50,   101,    39,    23,    60
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    65,    66,    67,    67,    68,    69,    69,    69,    69,
      69,    69,    69,    70,    70,    71,    71,    72,    72,    72,
      73,    73,    74,    74,    74,    75,    75,    76,    76,    77,
      78,    78,    79,    80,    81,    81,    82,    83,    83,    84,
      84,    84,    85,    86,    86,    87,    88,    88,    89,    90,
      91,    91,    92,    93,    93,    94,    94,    95,    95,    96,
      97,    97,    98,    99,    99,   100,   101,   101,   102,   103,
     104,   104,   105,   105,   105,   106,   107,   107,   108,   109,
     109,   109,   109,   109,   109,   109,   109,   109,   110,   110,
     111,   112,   113,   113,   114,   114,   115,   115,   116
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     2,     2,     1,     1,     1,     1,
       1,     1,     1,     4,     4,     2,     2,     1,     1,     1,
       0,     1,     1,     1,     1,     0,     2,     2,     1,     2,
       0,     1,     1,     2,     0,     1,     3,     0,     2,     0,
       2,     2,     5,     1,     1,     2,     0,     1,     1,     2,
       0,     1,     1,     0,     4,     1,     1,     0,     1,     3,
       1,     1,     2,     0,     1,     1,     0,     1,     4,    13,
       0,     5,     1,     1,     1,     2,     0,     1,     5,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     0,     1,
       5,     6,     0,     4,     0,     4,     0,     3,     2
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
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

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256


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


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined ROUTER_YYLTYPE_IS_TRIVIAL && ROUTER_YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static unsigned
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  unsigned res = 0;
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

#  define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, Location, yyscanner, rtr, ralloc, palloc); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (yylocationp);
  YYUSE (yyscanner);
  YYUSE (rtr);
  YYUSE (ralloc);
  YYUSE (palloc);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, yyscanner, rtr, ralloc, palloc);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &yyvsp[(yyi + 1) - (yynrhs)]
                       , &(yylsp[(yyi + 1) - (yynrhs)])                       , yyscanner, rtr, ralloc, palloc);
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
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
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


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
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
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
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
            /* Fall through.  */
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

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
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
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
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
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
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
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (yyscanner);
  YYUSE (rtr);
  YYUSE (ralloc);
  YYUSE (palloc);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void *yyscanner, router *rtr, allocator *ralloc, allocator *palloc)
{
/* The lookahead symbol.  */
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
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.
       'yyls': related to locations.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yylsp = yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  yylsp[0] = yylloc;
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yyls1, yysize * sizeof (*yylsp),
                    &yystacksize);

        yyls = yyls1;
        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

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

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, &yylloc, yyscanner, rtr, ralloc, palloc);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
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

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;
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
| yyreduce -- Do a reduction.  |
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

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 13:
#line 113 "conffile.y" /* yacc.c:1646  */
    {
	   	struct _clhost *w;
		char *err;

		if (((*(cluster **)(&yyval)) = ra_malloc(ralloc, sizeof(cluster))) == NULL) {
			logerr("malloc failed for cluster '%s'\n", (*(char **)(&yyvsp[-2])));
			YYABORT;
		}
		(*(cluster **)(&yyval))->name = ra_strdup(ralloc, (*(char **)(&yyvsp[-2])));
		(*(cluster **)(&yyval))->next = NULL;
		(*(cluster **)(&yyval))->type = (*(struct _clust*)(&yyvsp[-1])).t;
		switch ((*(cluster **)(&yyval))->type) {
			case CARBON_CH:
			case FNV1A_CH:
			case JUMP_CH:
				(*(cluster **)(&yyval))->members.ch = ra_malloc(ralloc, sizeof(chashring));
				if ((*(cluster **)(&yyval))->members.ch == NULL) {
					logerr("malloc failed for ch in cluster '%s'\n", (*(char **)(&yyvsp[-2])));
					YYABORT;
				}
				if ((*(struct _clust*)(&yyvsp[-1])).ival < 1 || (*(struct _clust*)(&yyvsp[-1])).ival > 255) {
					router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
						"replication count must be between 1 and 255");
					YYERROR;
				}
				(*(cluster **)(&yyval))->members.ch->repl_factor = (unsigned char)(*(struct _clust*)(&yyvsp[-1])).ival;
				(*(cluster **)(&yyval))->members.ch->ring = ch_new(
					(*(cluster **)(&yyval))->type == CARBON_CH ? CARBON :
					(*(cluster **)(&yyval))->type == FNV1A_CH ? FNV1a :
					JUMP_FNV1a);
				break;
			case FORWARD:
				(*(cluster **)(&yyval))->members.forward = NULL;
				break;
			case ANYOF:
			case FAILOVER:
				(*(cluster **)(&yyval))->members.anyof = NULL;
				break;
			default:
				logerr("unknown cluster type %zd!\n", (*(cluster **)(&yyval))->type);
				YYABORT;
		}
		
		for (w = (*(struct _clhost **)(&yyvsp[0])); w != NULL; w = w->next) {
			err = router_add_server(rtr, w->ip, w->port, w->inst,
					w->proto, w->saddr, w->hint, (*(struct _clust*)(&yyvsp[-1])).ival, (*(cluster **)(&yyval)));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}
		}

		err = router_add_cluster(rtr, (*(cluster **)(&yyval)));
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
	   }
#line 1708 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 14:
#line 172 "conffile.y" /* yacc.c:1646  */
    {
	   	struct _clhost *w;
		char *err;

		if (((*(cluster **)(&yyval)) = ra_malloc(ralloc, sizeof(cluster))) == NULL) {
			logerr("malloc failed for cluster '%s'\n", (*(char **)(&yyvsp[-2])));
			YYABORT;
		}
		(*(cluster **)(&yyval))->name = ra_strdup(ralloc, (*(char **)(&yyvsp[-2])));
		(*(cluster **)(&yyval))->next = NULL;
		(*(cluster **)(&yyval))->type = (*(struct _clust*)(&yyvsp[-1])).t;
		switch ((*(cluster **)(&yyval))->type) {
			case FILELOG:
			case FILELOGIP:
				(*(cluster **)(&yyval))->members.forward = NULL;
				break;
			default:
				logerr("unknown cluster type %zd!\n", (*(cluster **)(&yyval))->type);
				YYABORT;
		}
		
		for (w = (*(struct _clhost **)(&yyvsp[0])); w != NULL; w = w->next) {
			err = router_add_server(rtr, w->ip, w->port, w->inst,
					w->proto, w->saddr, w->hint, (*(struct _clust*)(&yyvsp[-1])).ival, (*(cluster **)(&yyval)));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}
		}

		err = router_add_cluster(rtr, (*(cluster **)(&yyval)));
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
	   }
#line 1749 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 15:
#line 212 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clust*)(&yyval)).t = (*(enum clusttype*)(&yyvsp[-1])); (*(struct _clust*)(&yyval)).ival = (*(int*)(&yyvsp[0])); }
#line 1755 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 16:
#line 214 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clust*)(&yyval)).t = (*(enum clusttype*)(&yyvsp[-1])); (*(struct _clust*)(&yyval)).ival = (*(int*)(&yyvsp[0])); }
#line 1761 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 17:
#line 217 "conffile.y" /* yacc.c:1646  */
    { (*(enum clusttype*)(&yyval)) = FORWARD; }
#line 1767 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 18:
#line 218 "conffile.y" /* yacc.c:1646  */
    { (*(enum clusttype*)(&yyval)) = ANYOF; }
#line 1773 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 19:
#line 219 "conffile.y" /* yacc.c:1646  */
    { (*(enum clusttype*)(&yyval)) = FAILOVER; }
#line 1779 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 20:
#line 222 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = 0; }
#line 1785 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 21:
#line 223 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = 1; }
#line 1791 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 22:
#line 226 "conffile.y" /* yacc.c:1646  */
    { (*(enum clusttype*)(&yyval)) = CARBON_CH; }
#line 1797 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 23:
#line 227 "conffile.y" /* yacc.c:1646  */
    { (*(enum clusttype*)(&yyval)) = FNV1A_CH; }
#line 1803 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 24:
#line 228 "conffile.y" /* yacc.c:1646  */
    { (*(enum clusttype*)(&yyval)) = JUMP_CH; }
#line 1809 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 25:
#line 231 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = 1; }
#line 1815 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 26:
#line 232 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = (*(int*)(&yyvsp[0])); }
#line 1821 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 27:
#line 235 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clust*)(&yyval)).t = FILELOGIP; (*(struct _clust*)(&yyval)).ival = 0; }
#line 1827 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 28:
#line 236 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clust*)(&yyval)).t = FILELOG; (*(struct _clust*)(&yyval)).ival = 0; }
#line 1833 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 29:
#line 239 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clhost **)(&yyvsp[-1]))->next = (*(struct _clhost **)(&yyvsp[0])); (*(struct _clhost **)(&yyval)) = (*(struct _clhost **)(&yyvsp[-1])); }
#line 1839 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 30:
#line 241 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clhost **)(&yyval)) = NULL; }
#line 1845 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 31:
#line 242 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clhost **)(&yyval)) = (*(struct _clhost **)(&yyvsp[0])); }
#line 1851 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 32:
#line 245 "conffile.y" /* yacc.c:1646  */
    {
				struct _clhost *ret = ra_malloc(palloc, sizeof(struct _clhost));
				char *err = router_validate_path(rtr, (*(char **)(&yyvsp[0])));
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
				ret->ip = (*(char **)(&yyvsp[0]));
				ret->port = 2003;
				ret->saddr = NULL;
				ret->hint = NULL;
				ret->inst = NULL;
				ret->proto = CON_FILE;
				ret->next = NULL;
				(*(struct _clhost **)(&yyval)) = ret;
			}
#line 1873 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 33:
#line 264 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clhost **)(&yyvsp[-1]))->next = (*(struct _clhost **)(&yyvsp[0])); (*(struct _clhost **)(&yyval)) = (*(struct _clhost **)(&yyvsp[-1])); }
#line 1879 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 34:
#line 266 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clhost **)(&yyval)) = NULL; }
#line 1885 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 35:
#line 267 "conffile.y" /* yacc.c:1646  */
    { (*(struct _clhost **)(&yyval)) = (*(struct _clhost **)(&yyvsp[0])); }
#line 1891 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 36:
#line 270 "conffile.y" /* yacc.c:1646  */
    {
			  	struct _clhost *ret = ra_malloc(palloc, sizeof(struct _clhost));
				char *err = router_validate_address(
						rtr,
						&(ret->ip), &(ret->port), &(ret->saddr), &(ret->hint),
						(*(char **)(&yyvsp[-2])), (*(serv_ctype*)(&yyvsp[0])));
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
				ret->inst = (*(char **)(&yyvsp[-1]));
				ret->proto = (*(serv_ctype*)(&yyvsp[0]));
				ret->next = NULL;
				(*(struct _clhost **)(&yyval)) = ret;
			  }
#line 1912 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 37:
#line 287 "conffile.y" /* yacc.c:1646  */
    { (*(char **)(&yyval)) = NULL; }
#line 1918 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 38:
#line 288 "conffile.y" /* yacc.c:1646  */
    { (*(char **)(&yyval)) = (*(char **)(&yyvsp[0])); }
#line 1924 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 39:
#line 290 "conffile.y" /* yacc.c:1646  */
    { (*(serv_ctype*)(&yyval)) = CON_TCP; }
#line 1930 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 40:
#line 291 "conffile.y" /* yacc.c:1646  */
    { (*(serv_ctype*)(&yyval)) = CON_UDP; }
#line 1936 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 41:
#line 292 "conffile.y" /* yacc.c:1646  */
    { (*(serv_ctype*)(&yyval)) = CON_TCP; }
#line 1942 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 42:
#line 299 "conffile.y" /* yacc.c:1646  */
    {
	 	/* each expr comes with an allocated route, populate it */
		struct _maexpr *we;
		destinations *d = NULL;
		char *err;

		if ((*(struct _maexpr **)(&yyvsp[-2])) != NULL) {
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
			d->cl->members.validation->rule = (*(struct _maexpr **)(&yyvsp[-2]))->r;
			d->cl->members.validation->action = (*(struct _maexpr **)(&yyvsp[-2]))->drop ? VAL_DROP : VAL_LOG;
		}
		/* add destinations to the chain */
		if (d != NULL) {
			d->next = (*(destinations **)(&yyvsp[-1]));
		} else {
			d = (*(destinations **)(&yyvsp[-1]));
		}
		for (we = (*(struct _maexpr **)(&yyvsp[-3])); we != NULL; we = we->next) {
			we->r->next = NULL;
			we->r->dests = d;
			we->r->stop = (*(destinations **)(&yyvsp[-1])) == NULL ? 0 :
					(*(destinations **)(&yyvsp[-1]))->cl->type == BLACKHOLE ? 1 : (*(int*)(&yyvsp[0]));
			err = router_add_route(rtr, we->r);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
		}
	 }
#line 1994 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 43:
#line 349 "conffile.y" /* yacc.c:1646  */
    {
			if (((*(struct _maexpr **)(&yyval)) = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	(*(struct _maexpr **)(&yyval))->r = NULL;
			if (router_validate_expression(rtr, &((*(struct _maexpr **)(&yyval))->r), "*", 0) != NULL)
				YYABORT;
			(*(struct _maexpr **)(&yyval))->drop = 0;
			(*(struct _maexpr **)(&yyval))->next = NULL;
		   }
#line 2010 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 44:
#line 360 "conffile.y" /* yacc.c:1646  */
    { (*(struct _maexpr **)(&yyval)) = (*(struct _maexpr **)(&yyvsp[0])); }
#line 2016 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 45:
#line 363 "conffile.y" /* yacc.c:1646  */
    { (*(struct _maexpr **)(&yyvsp[-1]))->next = (*(struct _maexpr **)(&yyvsp[0])); (*(struct _maexpr **)(&yyval)) = (*(struct _maexpr **)(&yyvsp[-1])); }
#line 2022 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 46:
#line 365 "conffile.y" /* yacc.c:1646  */
    { (*(struct _maexpr **)(&yyval)) = NULL; }
#line 2028 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 47:
#line 366 "conffile.y" /* yacc.c:1646  */
    { (*(struct _maexpr **)(&yyval)) = (*(struct _maexpr **)(&yyvsp[0])); }
#line 2034 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 48:
#line 370 "conffile.y" /* yacc.c:1646  */
    {
			char *err;
			if (((*(struct _maexpr **)(&yyval)) = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	(*(struct _maexpr **)(&yyval))->r = NULL;
		  	err = router_validate_expression(rtr, &((*(struct _maexpr **)(&yyval))->r), (*(char **)(&yyvsp[0])), 0);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
			(*(struct _maexpr **)(&yyval))->drop = 0;
			(*(struct _maexpr **)(&yyval))->next = NULL;
		  }
#line 2055 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 49:
#line 389 "conffile.y" /* yacc.c:1646  */
    { (*(struct _maexpr **)(&yyvsp[-1]))->next = (*(struct _maexpr **)(&yyvsp[0])); (*(struct _maexpr **)(&yyval)) = (*(struct _maexpr **)(&yyvsp[-1])); }
#line 2061 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 50:
#line 391 "conffile.y" /* yacc.c:1646  */
    { (*(struct _maexpr **)(&yyval)) = NULL; }
#line 2067 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 51:
#line 392 "conffile.y" /* yacc.c:1646  */
    { (*(struct _maexpr **)(&yyval)) = (*(struct _maexpr **)(&yyvsp[0])); }
#line 2073 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 52:
#line 396 "conffile.y" /* yacc.c:1646  */
    {
			char *err;
			if (((*(struct _maexpr **)(&yyval)) = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
		   	(*(struct _maexpr **)(&yyval))->r = NULL;
		  	err = router_validate_expression(rtr, &((*(struct _maexpr **)(&yyval))->r), (*(char **)(&yyvsp[0])), 1);
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr,
						ralloc, palloc, err);
				YYERROR;
			}
			(*(struct _maexpr **)(&yyval))->drop = 0;
			(*(struct _maexpr **)(&yyval))->next = NULL;
		  }
#line 2094 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 53:
#line 414 "conffile.y" /* yacc.c:1646  */
    { (*(struct _maexpr **)(&yyval)) = NULL; }
#line 2100 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 54:
#line 416 "conffile.y" /* yacc.c:1646  */
    {
					char *err;
					if (((*(struct _maexpr **)(&yyval)) = ra_malloc(palloc, sizeof(struct _maexpr))) == NULL) {
						logerr("out of memory\n");
						YYABORT;
					}
					(*(struct _maexpr **)(&yyval))->r = NULL;
					err = router_validate_expression(rtr, &((*(struct _maexpr **)(&yyval))->r), (*(char **)(&yyvsp[-2])), 0);
					if (err != NULL) {
						router_yyerror(&yylloc, yyscanner, rtr,
								ralloc, palloc, err);
						YYERROR;
					}
					(*(struct _maexpr **)(&yyval))->drop = (*(int*)(&yyvsp[0]));
					(*(struct _maexpr **)(&yyval))->next = NULL;
				  }
#line 2121 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 55:
#line 434 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = 0; }
#line 2127 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 56:
#line 435 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = 1; }
#line 2133 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 57:
#line 438 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyval)) = NULL; }
#line 2139 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 58:
#line 439 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyval)) = (*(destinations **)(&yyvsp[0])); }
#line 2145 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 59:
#line 442 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyval)) = (*(destinations **)(&yyvsp[0])); }
#line 2151 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 60:
#line 446 "conffile.y" /* yacc.c:1646  */
    {
			if (((*(destinations **)(&yyval)) = ra_malloc(ralloc, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			if (router_validate_cluster(rtr, &((*(destinations **)(&yyval))->cl), "blackhole") != NULL)
				YYABORT;
			(*(destinations **)(&yyval))->next = NULL;
		  }
#line 2165 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 61:
#line 455 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyval)) = (*(destinations **)(&yyvsp[0])); }
#line 2171 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 62:
#line 458 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyvsp[-1]))->next = (*(destinations **)(&yyvsp[0])); (*(destinations **)(&yyval)) = (*(destinations **)(&yyvsp[-1])); }
#line 2177 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 63:
#line 460 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyval)) = NULL; }
#line 2183 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 64:
#line 461 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyval)) = (*(destinations **)(&yyvsp[0])); }
#line 2189 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 65:
#line 465 "conffile.y" /* yacc.c:1646  */
    {
			char *err;
			if (((*(destinations **)(&yyval)) = ra_malloc(ralloc, sizeof(destinations))) == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			err = router_validate_cluster(rtr, &((*(destinations **)(&yyval))->cl), (*(char **)(&yyvsp[0])));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}
			(*(destinations **)(&yyval))->next = NULL;
		 }
#line 2207 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 66:
#line 480 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = 0; }
#line 2213 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 67:
#line 481 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = 1; }
#line 2219 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 68:
#line 487 "conffile.y" /* yacc.c:1646  */
    {
		char *err;
		route *r = NULL;
		cluster *cl;

		err = router_validate_expression(rtr, &r, (*(char **)(&yyvsp[-2])), 1);
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
		cl->members.replacement = ra_strdup(ralloc, (*(char **)(&yyvsp[0])));
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
#line 2264 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 69:
#line 537 "conffile.y" /* yacc.c:1646  */
    {
		 	cluster *w;
			aggregator *a;
			destinations *d;
			struct _agcomp *acw;
			struct _maexpr *we;
			char *err;

			if ((*(int*)(&yyvsp[-9])) <= 0) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
					"interval must be > 0");
				YYERROR;
			}
			if ((*(int*)(&yyvsp[-5])) <= 0) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
					"expire must be > 0");
				YYERROR;
			}
			if ((*(int*)(&yyvsp[-5])) < (*(int*)(&yyvsp[-9]))) {
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

			a = aggregator_new((*(int*)(&yyvsp[-9])), (*(int*)(&yyvsp[-5])), (*(enum _aggr_timestamp*)(&yyvsp[-3])));
			if (a == NULL) {
				logerr("out of memory\n");
				YYABORT;
			}
			if ((err = router_add_aggregator(rtr, a)) != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}

			w->members.aggregation = a;
			
			for (acw = (*(struct _agcomp **)(&yyvsp[-2])); acw != NULL; acw = acw->next) {
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
			d->next = (*(destinations **)(&yyvsp[-1]));

			for (we = (*(struct _maexpr **)(&yyvsp[-11])); we != NULL; we = we->next) {
				we->r->next = NULL;
				we->r->dests = d;
				we->r->stop = (*(int*)(&yyvsp[0]));
				err = router_add_route(rtr, we->r);
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
			}

			if ((*(destinations **)(&yyvsp[-1])) != NULL)
				router_add_stubroute(rtr, AGGRSTUB, w, (*(destinations **)(&yyvsp[-1])));
		 }
#line 2352 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 70:
#line 622 "conffile.y" /* yacc.c:1646  */
    { (*(enum _aggr_timestamp*)(&yyval)) = TS_END; }
#line 2358 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 71:
#line 625 "conffile.y" /* yacc.c:1646  */
    { (*(enum _aggr_timestamp*)(&yyval)) = (*(enum _aggr_timestamp*)(&yyvsp[-2])); }
#line 2364 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 72:
#line 628 "conffile.y" /* yacc.c:1646  */
    { (*(enum _aggr_timestamp*)(&yyval)) = TS_START; }
#line 2370 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 73:
#line 629 "conffile.y" /* yacc.c:1646  */
    { (*(enum _aggr_timestamp*)(&yyval)) = TS_MIDDLE; }
#line 2376 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 74:
#line 630 "conffile.y" /* yacc.c:1646  */
    { (*(enum _aggr_timestamp*)(&yyval)) = TS_END; }
#line 2382 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 75:
#line 634 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp **)(&yyvsp[-1]))->next = (*(struct _agcomp **)(&yyvsp[0])); (*(struct _agcomp **)(&yyval)) = (*(struct _agcomp **)(&yyvsp[-1])); }
#line 2388 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 76:
#line 637 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp **)(&yyval)) = NULL; }
#line 2394 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 77:
#line 638 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp **)(&yyval)) = (*(struct _agcomp **)(&yyvsp[0])); }
#line 2400 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 78:
#line 642 "conffile.y" /* yacc.c:1646  */
    {
					(*(struct _agcomp **)(&yyval)) = ra_malloc(palloc, sizeof(struct _agcomp));
					if ((*(struct _agcomp **)(&yyval)) == NULL) {
						logerr("malloc failed\n");
						YYABORT;
					}
				 	(*(struct _agcomp **)(&yyval))->ctype = (*(struct _agcomp*)(&yyvsp[-3])).ctype;
					(*(struct _agcomp **)(&yyval))->pctl = (*(struct _agcomp*)(&yyvsp[-3])).pctl;
					(*(struct _agcomp **)(&yyval))->metric = (*(char **)(&yyvsp[0]));
					(*(struct _agcomp **)(&yyval))->next = NULL;
				 }
#line 2416 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 79:
#line 655 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp*)(&yyval)).ctype = SUM; }
#line 2422 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 80:
#line 656 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp*)(&yyval)).ctype = CNT; }
#line 2428 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 81:
#line 657 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp*)(&yyval)).ctype = MAX; }
#line 2434 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 82:
#line 658 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp*)(&yyval)).ctype = MIN; }
#line 2440 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 83:
#line 659 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp*)(&yyval)).ctype = AVG; }
#line 2446 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 84:
#line 660 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp*)(&yyval)).ctype = MEDN; }
#line 2452 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 85:
#line 662 "conffile.y" /* yacc.c:1646  */
    {
				    if ((*(int*)(&yyvsp[0])) < 1 || (*(int*)(&yyvsp[0])) > 99) {
						router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc,
							"percentile<x>: value x must be between 1 and 99");
						YYERROR;
					}
				   	(*(struct _agcomp*)(&yyval)).ctype = PCTL;
					(*(struct _agcomp*)(&yyval)).pctl = (unsigned char)(*(int*)(&yyvsp[0]));
				   }
#line 2466 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 86:
#line 671 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp*)(&yyval)).ctype = VAR; }
#line 2472 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 87:
#line 672 "conffile.y" /* yacc.c:1646  */
    { (*(struct _agcomp*)(&yyval)).ctype = SDEV; }
#line 2478 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 88:
#line 675 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyval)) = NULL; }
#line 2484 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 89:
#line 676 "conffile.y" /* yacc.c:1646  */
    { (*(destinations **)(&yyval)) = (*(destinations **)(&yyvsp[0])); }
#line 2490 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 90:
#line 682 "conffile.y" /* yacc.c:1646  */
    {
		char *err = router_set_statistics(rtr, (*(destinations **)(&yyvsp[-1])));
		if (err != NULL) {
			router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
			YYERROR;
		}
		logerr("warning: 'send statistics to ...' is deprecated and will be "
				"removed in a future version, use 'statistics send to ...' "
				"instead\n");
	}
#line 2505 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 91:
#line 702 "conffile.y" /* yacc.c:1646  */
    {
		  	char *err;
		  	err = router_set_collectorvals(rtr, (*(int*)(&yyvsp[-4])), (*(char **)(&yyvsp[-2])), (*(col_mode*)(&yyvsp[-3])));
			if (err != NULL) {
				router_yyerror(&yylloc, yyscanner, rtr, ralloc, palloc, err);
				YYERROR;
			}

			if ((*(destinations **)(&yyvsp[-1])) != NULL) {
				err = router_set_statistics(rtr, (*(destinations **)(&yyvsp[-1])));
				if (err != NULL) {
					router_yyerror(&yylloc, yyscanner, rtr,
							ralloc, palloc, err);
					YYERROR;
				}
			}
		  }
#line 2527 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 92:
#line 721 "conffile.y" /* yacc.c:1646  */
    { (*(int*)(&yyval)) = 0; }
#line 2533 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 93:
#line 723 "conffile.y" /* yacc.c:1646  */
    {
					   	if ((*(int*)(&yyvsp[-1])) <= 0) {
							router_yyerror(&yylloc, yyscanner, rtr,
									ralloc, palloc, "interval must be > 0");
							YYERROR;
						}
						(*(int*)(&yyval)) = (*(int*)(&yyvsp[-1]));
					   }
#line 2546 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 94:
#line 733 "conffile.y" /* yacc.c:1646  */
    { (*(col_mode*)(&yyval)) = CUM; }
#line 2552 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 95:
#line 734 "conffile.y" /* yacc.c:1646  */
    { (*(col_mode*)(&yyval)) = SUB; }
#line 2558 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 96:
#line 737 "conffile.y" /* yacc.c:1646  */
    { (*(char **)(&yyval)) = NULL; }
#line 2564 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 97:
#line 738 "conffile.y" /* yacc.c:1646  */
    { (*(char **)(&yyval)) = (*(char **)(&yyvsp[0])); }
#line 2570 "conffile.tab.c" /* yacc.c:1646  */
    break;

  case 98:
#line 744 "conffile.y" /* yacc.c:1646  */
    {
	   	router_readconfig(rtr, (*(char **)(&yyvsp[0])), 0, 0, 0, 0, 0);
	   }
#line 2578 "conffile.tab.c" /* yacc.c:1646  */
    break;


#line 2582 "conffile.tab.c" /* yacc.c:1646  */
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
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (&yylloc, yyscanner, rtr, ralloc, palloc, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (&yylloc, yyscanner, rtr, ralloc, palloc, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, yyscanner, rtr, ralloc, palloc);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  yyerror_range[1] = yylsp[1-yylen];
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

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
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
                  yystos[yystate], yyvsp, yylsp, yyscanner, rtr, ralloc, palloc);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, yyscanner, rtr, ralloc, palloc, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
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
                  yystos[*yyssp], yyvsp, yylsp, yyscanner, rtr, ralloc, palloc);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
