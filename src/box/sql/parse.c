/*
** 2000-05-29
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Driver template for the LEMON parser generator.
**
** The "lemon" program processes an LALR(1) input grammar file, then uses
** this template to construct a parser.  The "lemon" program inserts text
** at each "%%" line.  Also, any "P-a-r-s-e" identifer prefix (without the
** interstitial "-" characters) contained in this template is changed into
** the value of the %name directive from the grammar.  Otherwise, the content
** of this template is copied straight through into the generate parser
** source file.
**
** The following is the concatenation of all %include directives from the
** input grammar file:
*/
#include <stdio.h>
#include <stdbool.h>
/************ Begin %include sections from the grammar ************************/
#line 52 "parse.y"

#include "sqliteInt.h"

/*
** Disable all error recovery processing in the parser push-down
** automaton.
*/
#define YYNOERRORRECOVERY 1

/*
** Make yytestcase() the same as testcase()
*/
#define yytestcase(X) testcase(X)

/*
** Indicate that sqlite3ParserFree() will never be called with a null
** pointer.
*/
#define YYPARSEFREENEVERNULL 1

/*
** Alternative datatype for the argument to the malloc() routine passed
** into sqlite3ParserAlloc().  The default is size_t.
*/
#define YYMALLOCARGTYPE  u64

/*
** An instance of this structure holds information about the
** LIMIT clause of a SELECT statement.
*/
struct LimitVal {
  Expr *pLimit;    /* The LIMIT expression.  NULL if there is no limit */
  Expr *pOffset;   /* The OFFSET expression.  NULL if there is none */
};

/*
** An instance of the following structure describes the event of a
** TRIGGER.  "a" is the event type, one of TK_UPDATE, TK_INSERT,
** TK_DELETE, or TK_INSTEAD.  If the event is of the form
**
**      UPDATE ON (a,b,c)
**
** Then the "b" IdList records the list "a,b,c".
*/
struct TrigEvent { int a; IdList * b; };

/*
** Disable lookaside memory allocation for objects that might be
** shared across database connections.
*/
static void disableLookaside(Parse *pParse){
  pParse->disableLookaside++;
  pParse->db->lookaside.bDisable++;
}

#line 409 "parse.y"

  /*
  ** For a compound SELECT statement, make sure p->pPrior->pNext==p for
  ** all elements in the list.  And make sure list length does not exceed
  ** SQLITE_LIMIT_COMPOUND_SELECT.
  */
  static void parserDoubleLinkSelect(Parse *pParse, Select *p){
    if( p->pPrior ){
      Select *pNext = 0, *pLoop;
      int mxSelect, cnt = 0;
      for(pLoop=p; pLoop; pNext=pLoop, pLoop=pLoop->pPrior, cnt++){
        pLoop->pNext = pNext;
        pLoop->selFlags |= SF_Compound;
      }
      if( (p->selFlags & SF_MultiValue)==0 && 
        (mxSelect = pParse->db->aLimit[SQLITE_LIMIT_COMPOUND_SELECT])>0 &&
        cnt>mxSelect
      ){
        sqlite3ErrorMsg(pParse, "Too many UNION or EXCEPT or INTERSECT operations");
      }
    }
  }
#line 846 "parse.y"

  /* This is a utility routine used to set the ExprSpan.zStart and
  ** ExprSpan.zEnd values of pOut so that the span covers the complete
  ** range of text beginning with pStart and going to the end of pEnd.
  */
  static void spanSet(ExprSpan *pOut, Token *pStart, Token *pEnd){
    pOut->zStart = pStart->z;
    pOut->zEnd = &pEnd->z[pEnd->n];
  }

  /* Construct a new Expr object from a single identifier.  Use the
  ** new Expr to populate pOut.  Set the span of pOut to be the identifier
  ** that created the expression.
  */
  static void spanExpr(ExprSpan *pOut, Parse *pParse, int op, Token t){
    Expr *p = sqlite3DbMallocRawNN(pParse->db, sizeof(Expr)+t.n+1);
    if( p ){
      memset(p, 0, sizeof(Expr));
      p->op = (u8)op;
      p->flags = EP_Leaf;
      p->iAgg = -1;
      p->u.zToken = (char*)&p[1];
      memcpy(p->u.zToken, t.z, t.n);
      p->u.zToken[t.n] = 0;
      if( sqlite3Isquote(p->u.zToken[0]) ){
        if( p->u.zToken[0]=='"' ) p->flags |= EP_DblQuoted;
        sqlite3Dequote(p->u.zToken);
      }
#if SQLITE_MAX_EXPR_DEPTH>0
      p->nHeight = 1;
#endif  
    }
    pOut->pExpr = p;
    pOut->zStart = t.z;
    pOut->zEnd = &t.z[t.n];
  }
#line 963 "parse.y"

  /* This routine constructs a binary expression node out of two ExprSpan
  ** objects and uses the result to populate a new ExprSpan object.
  */
  static void spanBinaryExpr(
    Parse *pParse,      /* The parsing context.  Errors accumulate here */
    int op,             /* The binary operation */
    ExprSpan *pLeft,    /* The left operand, and output */
    ExprSpan *pRight    /* The right operand */
  ){
    pLeft->pExpr = sqlite3PExpr(pParse, op, pLeft->pExpr, pRight->pExpr);
    pLeft->zEnd = pRight->zEnd;
  }

  /* If doNot is true, then add a TK_NOT Expr-node wrapper around the
  ** outside of *ppExpr.
  */
  static void exprNot(Parse *pParse, int doNot, ExprSpan *pSpan){
    if( doNot ){
      pSpan->pExpr = sqlite3PExpr(pParse, TK_NOT, pSpan->pExpr, 0);
    }
  }
#line 1037 "parse.y"

  /* Construct an expression node for a unary postfix operator
  */
  static void spanUnaryPostfix(
    Parse *pParse,         /* Parsing context to record errors */
    int op,                /* The operator */
    ExprSpan *pOperand,    /* The operand, and output */
    Token *pPostOp         /* The operand token for setting the span */
  ){
    pOperand->pExpr = sqlite3PExpr(pParse, op, pOperand->pExpr, 0);
    pOperand->zEnd = &pPostOp->z[pPostOp->n];
  }                           
#line 1054 "parse.y"

  /* A routine to convert a binary TK_IS or TK_ISNOT expression into a
  ** unary TK_ISNULL or TK_NOTNULL expression. */
  static void binaryToUnaryIfNull(Parse *pParse, Expr *pY, Expr *pA, int op){
    sqlite3 *db = pParse->db;
    if( pA && pY && pY->op==TK_NULL ){
      pA->op = (u8)op;
      sqlite3ExprDelete(db, pA->pRight);
      pA->pRight = 0;
    }
  }
#line 1082 "parse.y"

  /* Construct an expression node for a unary prefix operator
  */
  static void spanUnaryPrefix(
    ExprSpan *pOut,        /* Write the new expression node here */
    Parse *pParse,         /* Parsing context to record errors */
    int op,                /* The operator */
    ExprSpan *pOperand,    /* The operand */
    Token *pPreOp         /* The operand token for setting the span */
  ){
    pOut->zStart = pPreOp->z;
    pOut->pExpr = sqlite3PExpr(pParse, op, pOperand->pExpr, 0);
    pOut->zEnd = pOperand->zEnd;
  }
#line 1287 "parse.y"

  /* Add a single new term to an ExprList that is used to store a
  ** list of identifiers.  Report an error if the ID list contains
  ** a COLLATE clause or an ASC or DESC keyword, except ignore the
  ** error while parsing a legacy schema.
  */
  static ExprList *parserAddExprIdListTerm(
    Parse *pParse,
    ExprList *pPrior,
    Token *pIdToken,
    int hasCollate,
    int sortOrder
  ){
    ExprList *p = sqlite3ExprListAppend(pParse, pPrior, 0);
    if( (hasCollate || sortOrder!=SQLITE_SO_UNDEFINED)
        && pParse->db->init.busy==0
    ){
      sqlite3ErrorMsg(pParse, "syntax error after column name \"%.*s\"",
                         pIdToken->n, pIdToken->z);
    }
    sqlite3ExprListSetName(pParse, p, pIdToken, 1);
    return p;
  }
#line 232 "parse.c"
/**************** End of %include directives **********************************/
/* These constants specify the various numeric values for terminal symbols
** in a format understandable to "makeheaders".  This section is blank unless
** "lemon" is run with the "-m" command-line option.
***************** Begin makeheaders token definitions *************************/
/**************** End makeheaders token definitions ***************************/

/* The next sections is a series of control #defines.
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used to store the integer codes
**                       that represent terminal and non-terminal symbols.
**                       "unsigned char" is used if there are fewer than
**                       256 symbols.  Larger types otherwise.
**    YYNOCODE           is a number of type YYCODETYPE that is not used for
**                       any terminal or nonterminal symbol.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       (also known as: "terminal symbols") have fall-back
**                       values which should be used if the original symbol
**                       would not parse.  This permits keywords to sometimes
**                       be used as identifiers, for example.
**    YYACTIONTYPE       is the data type used for "action codes" - numbers
**                       that indicate what to do in response to the next
**                       token.
**    sqlite3ParserTOKENTYPE     is the data type used for minor type for terminal
**                       symbols.  Background: A "minor type" is a semantic
**                       value associated with a terminal or non-terminal
**                       symbols.  For example, for an "ID" terminal symbol,
**                       the minor type might be the name of the identifier.
**                       Each non-terminal can have a different minor type.
**                       Terminal symbols all have the same minor type, though.
**                       This macros defines the minor type for terminal 
**                       symbols.
**    YYMINORTYPE        is the data type used for all minor types.
**                       This is typically a union of many types, one of
**                       which is sqlite3ParserTOKENTYPE.  The entry in the union
**                       for terminal symbols is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    sqlite3ParserARG_SDECL     A static variable declaration for the %extra_argument
**    sqlite3ParserARG_PDECL     A parameter declaration for the %extra_argument
**    sqlite3ParserARG_STORE     Code to store %extra_argument into yypParser
**    sqlite3ParserARG_FETCH     Code to extract %extra_argument from yypParser
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YY_MAX_SHIFT       Maximum value for shift actions
**    YY_MIN_SHIFTREDUCE Minimum value for shift-reduce actions
**    YY_MAX_SHIFTREDUCE Maximum value for shift-reduce actions
**    YY_MIN_REDUCE      Maximum value for reduce actions
**    YY_ERROR_ACTION    The yy_action[] code for syntax error
**    YY_ACCEPT_ACTION   The yy_action[] code for accept
**    YY_NO_ACTION       The yy_action[] code for no-op
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/************* Begin control #defines *****************************************/
#define YYCODETYPE unsigned char
#define YYNOCODE 234
#define YYACTIONTYPE unsigned short int
#define YYWILDCARD 78
#define sqlite3ParserTOKENTYPE Token
typedef union {
  int yyinit;
  sqlite3ParserTOKENTYPE yy0;
  struct TrigEvent yy60;
  SrcList* yy73;
  ExprSpan yy200;
  TriggerStep* yy251;
  ExprList* yy272;
  Expr* yy294;
  Select* yy309;
  int yy322;
  struct {int value; int mask;} yy369;
  struct LimitVal yy400;
  IdList* yy436;
  With* yy445;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define sqlite3ParserARG_SDECL Parse *pParse;
#define sqlite3ParserARG_PDECL ,Parse *pParse
#define sqlite3ParserARG_FETCH Parse *pParse = yypParser->pParse
#define sqlite3ParserARG_STORE yypParser->pParse = pParse
#define YYFALLBACK 1
#define YYNSTATE             423
#define YYNRULE              305
#define YY_MAX_SHIFT         422
#define YY_MIN_SHIFTREDUCE   619
#define YY_MAX_SHIFTREDUCE   923
#define YY_MIN_REDUCE        924
#define YY_MAX_REDUCE        1228
#define YY_ERROR_ACTION      1229
#define YY_ACCEPT_ACTION     1230
#define YY_NO_ACTION         1231
/************* End control #defines *******************************************/

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N <= YY_MAX_SHIFT             Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   N between YY_MIN_SHIFTREDUCE       Shift to an arbitrary state then
**     and YY_MAX_SHIFTREDUCE           reduce by rule N-YY_MIN_SHIFTREDUCE.
**
**   N between YY_MIN_REDUCE            Reduce by rule N-YY_MIN_REDUCE
**     and YY_MAX_REDUCE
**
**   N == YY_ERROR_ACTION               A syntax error has occurred.
**
**   N == YY_ACCEPT_ACTION              The parser accepts its input.
**
**   N == YY_NO_ACTION                  No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as either:
**
**    (A)   N = yy_action[ yy_shift_ofst[S] + X ]
**    (B)   N = yy_default[S]
**
** The (A) formula is preferred.  The B formula is used instead if:
**    (1)  The yy_shift_ofst[S]+X value is out of range, or
**    (2)  yy_lookahead[yy_shift_ofst[S]+X] is not equal to X, or
**    (3)  yy_shift_ofst[S] equal YY_SHIFT_USE_DFLT.
** (Implementation note: YY_SHIFT_USE_DFLT is chosen so that
** YY_SHIFT_USE_DFLT+X will be out of range for all possible lookaheads X.
** Hence only tests (1) and (2) need to be evaluated.)
**
** The formulas above are for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
**
*********** Begin parsing tables **********************************************/
#define YY_ACTTAB_COUNT (1445)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    91,   92,  295,   82,  790,  790,  802,  805,  794,  794,
 /*    10 */    89,   89,   90,   90,   90,   90,  724,   88,   88,   88,
 /*    20 */    88,   87,   87,   86,   86,   86,   85,  317,   90,   90,
 /*    30 */    90,   90,   83,   88,   88,   88,   88,   87,   87,   86,
 /*    40 */    86,   86,   85,  317,  191,  165,  309,  909,   90,   90,
 /*    50 */    90,   90,  186,   88,   88,   88,   88,   87,   87,   86,
 /*    60 */    86,   86,   85,  317,   87,   87,   86,   86,   86,   85,
 /*    70 */   317,  306,  724,  317,  909,   86,   86,   86,   85,  317,
 /*    80 */    91,   92,  295,   82,  790,  790,  802,  805,  794,  794,
 /*    90 */    89,   89,   90,   90,   90,   90,  123,   88,   88,   88,
 /*   100 */    88,   87,   87,   86,   86,   86,   85,  317,   88,   88,
 /*   110 */    88,   88,   87,   87,   86,   86,   86,   85,  317,  897,
 /*   120 */   199,  157,  261,  371,  256,  370,  187,  658,  791,  791,
 /*   130 */   803,  806,  297,  739,  254, 1181, 1181,  740,  648,   91,
 /*   140 */    92,  295,   82,  790,  790,  802,  805,  794,  794,   89,
 /*   150 */    89,   90,   90,   90,   90,  651,   88,   88,   88,   88,
 /*   160 */    87,   87,   86,   86,   86,   85,  317,  123,  723,   91,
 /*   170 */    92,  295,   82,  790,  790,  802,  805,  794,  794,   89,
 /*   180 */    89,   90,   90,   90,   90,   67,   88,   88,   88,   88,
 /*   190 */    87,   87,   86,   86,   86,   85,  317,  761,  710,  345,
 /*   200 */   421,  421,  795,   93,  620,  320,  229,  231,  140,   84,
 /*   210 */    81,  166,  251,  697,  650,  244,  342,  243,   91,   92,
 /*   220 */   295,   82,  790,  790,  802,  805,  794,  794,   89,   89,
 /*   230 */    90,   90,   90,   90,  761,   88,   88,   88,   88,   87,
 /*   240 */    87,   86,   86,   86,   85,  317,   22, 1230,  422,    3,
 /*   250 */    85,  317,  244,  332,  232,   74,  780,   72,  773,  123,
 /*   260 */   738,  656,  767,  213,  228,  378,  383,   91,   92,  295,
 /*   270 */    82,  790,  790,  802,  805,  794,  794,   89,   89,   90,
 /*   280 */    90,   90,   90,  347,   88,   88,   88,   88,   87,   87,
 /*   290 */    86,   86,   86,   85,  317,  772,  772,  774,  182,  111,
 /*   300 */   737,  368,  365,  364,  315,  314,   84,   81,  166,  377,
 /*   310 */   649,  702,  702,  363,  188,  163,   91,   92,  295,   82,
 /*   320 */   790,  790,  802,  805,  794,  794,   89,   89,   90,   90,
 /*   330 */    90,   90,  416,   88,   88,   88,   88,   87,   87,   86,
 /*   340 */    86,   86,   85,  317,   66,  416,   84,   81,  166,  345,
 /*   350 */    47,   47,   84,   81,  166,  413,  413,  413,  329,  781,
 /*   360 */   316,  316,  316,   48,   48,   91,   92,  295,   82,  790,
 /*   370 */   790,  802,  805,  794,  794,   89,   89,   90,   90,   90,
 /*   380 */    90,  661,   88,   88,   88,   88,   87,   87,   86,   86,
 /*   390 */    86,   85,  317,  382,  917,  765,  917,  146,  394,  379,
 /*   400 */   684,  663,  901,  271,  848,  848,  337,  290,  766,  684,
 /*   410 */   123,    9,    9,  217,   91,   92,  295,   82,  790,  790,
 /*   420 */   802,  805,  794,  794,   89,   89,   90,   90,   90,   90,
 /*   430 */   415,   88,   88,   88,   88,   87,   87,   86,   86,   86,
 /*   440 */    85,  317,   91,   92,  295,   82,  790,  790,  802,  805,
 /*   450 */   794,  794,   89,   89,   90,   90,   90,   90,  336,   88,
 /*   460 */    88,   88,   88,   87,   87,   86,   86,   86,   85,  317,
 /*   470 */    91,   92,  295,   82,  790,  790,  802,  805,  794,  794,
 /*   480 */    89,   89,   90,   90,   90,   90,  192,   88,   88,   88,
 /*   490 */    88,   87,   87,   86,   86,   86,   85,  317,   91,   92,
 /*   500 */   295,   82,  790,  790,  802,  805,  794,  794,   89,   89,
 /*   510 */    90,   90,   90,   90,  147,   88,   88,   88,   88,   87,
 /*   520 */    87,   86,   86,   86,   85,  317,  185,  184,  183,  228,
 /*   530 */   378,  388,   70,  295,   82,  790,  790,  802,  805,  794,
 /*   540 */   794,   89,   89,   90,   90,   90,   90,  164,   88,   88,
 /*   550 */    88,   88,   87,   87,   86,   86,   86,   85,  317,  272,
 /*   560 */    73,  689,  689,  687,  687,  270,   91,   80,  295,   82,
 /*   570 */   790,  790,  802,  805,  794,  794,   89,   89,   90,   90,
 /*   580 */    90,   90,  723,   88,   88,   88,   88,   87,   87,   86,
 /*   590 */    86,   86,   85,  317,   92,  295,   82,  790,  790,  802,
 /*   600 */   805,  794,  794,   89,   89,   90,   90,   90,   90,   78,
 /*   610 */    88,   88,   88,   88,   87,   87,   86,   86,   86,   85,
 /*   620 */   317,  323,  640,  298,  416,  641,  641,  351,   75,   76,
 /*   630 */   212,  894,  641,  641,  289,   77,  873,  287,  286,  285,
 /*   640 */   202,  283,   48,   48,  633,  315,  314,  409,  411,    2,
 /*   650 */  1124,  717,  642,  892,  318,  318,  861,  168,  894,  642,
 /*   660 */   892,  182,  211,  211,  368,  365,  364,  758,  321,  864,
 /*   670 */   172,  641,  641,  400,  381,  170,  363,  394,  384,  780,
 /*   680 */   726,  418,  417,  765,  865,  767,  416,   78,  946,  111,
 /*   690 */   351,  211,  211,  866,  690,  395,  416,  296,  642,  892,
 /*   700 */   111,  916,  691,  381,   48,   48,   75,   76,  914,  872,
 /*   710 */   915,  216,  343,   77,   48,   48,  330,  223,  772,  772,
 /*   720 */   774,  775,  414,   18,  842,   23,  411,    2,  111,  416,
 /*   730 */   341,  324,  318,  318,  123,  917,  123,  917,  375,  394,
 /*   740 */   396,  356,  641,  641,  353,  227,  300,   30,   30,  394,
 /*   750 */   393,  400,  641,  641,  416,   54,  137,  780,  761,  418,
 /*   760 */   417,  641,  641,  767,  723,   78,  641,  641,  870,  642,
 /*   770 */   892,  111,   48,   48,  242,  325,  244,  342,  243,  642,
 /*   780 */   892,  109,  376,  151,   75,   76,  711, 1205,  642,  892,
 /*   790 */   679,   77,  208,  642,  892,  724,  772,  772,  774,  775,
 /*   800 */   414,   18,  712,  214,  411,    2,  146,  394,  374,  351,
 /*   810 */   318,  318,  282,  138,  123,  839,  291,   24,  641,  641,
 /*   820 */   123,  725,  641,  641,  165,  143,  641,  641,  191,  400,
 /*   830 */   667,  909,  327,  845,    1,  780,  844,  418,  417,  641,
 /*   840 */   641,  767,  416,   78,  335,  642,  892,  327,  326,  642,
 /*   850 */   892,  724,  416,  642,  892,  360,  299,  186,  909,  765,
 /*   860 */    48,   48,   75,   76,  861,  833,  642,  892,  416,   77,
 /*   870 */    10,   10,  416,  206,  772,  772,  774,  775,  414,   18,
 /*   880 */   416,  864,  411,    2,  303,  385,   10,   10,  318,  318,
 /*   890 */    10,   10,  416,  218,  249,  310,  865,  111,   48,   48,
 /*   900 */   305,  412,  298,  723,  219,  866,  399,  400,  327,  707,
 /*   910 */    10,   10,  328,  780,  706,  418,  417,  765,  108,  767,
 /*   920 */   343,   68,  301,  646,  391,  248,  856,  835,  837,  855,
 /*   930 */     5,  386,  191,  389,  146,  909,  825,  297,  352,  237,
 /*   940 */    75,   76,  215,  254,  294,  670,  354,   77,  159,  158,
 /*   950 */   646,  355,  772,  772,  774,  775,  414,   18,  671,  273,
 /*   960 */   411,    2,  909,  220,  416,  220,  318,  318,  416,   95,
 /*   970 */   236,  416,  723,  723,  652,  652,   20,  240,  307,  369,
 /*   980 */   302,  723,   34,   34,  416,  400,   35,   35,  835,   36,
 /*   990 */    36,  780,  304,  418,  417,  398,  416,  767,  416,  780,
 /*  1000 */   416,  773,   37,   37,  349,  767,  416,   19,  416,  723,
 /*  1010 */   247,  867,  333,  723,   38,   38,   26,   26,   27,   27,
 /*  1020 */   696,  235,  234,  416,   29,   29,   39,   39,  416,  666,
 /*  1030 */   772,  772,  774,  775,  414,   18,  416,  272,  772,  772,
 /*  1040 */   774,   40,   40,  416,  632,  416,   41,   41,  692,  416,
 /*  1050 */   350,  416,  239,  416,   11,   11,  416,  250,  669,  668,
 /*  1060 */   416,   42,   42,   97,   97,  416,  665,   43,   43,   44,
 /*  1070 */    44,   31,   31,  416,   45,   45,  699,  416,   46,   46,
 /*  1080 */   416,  765,  416,   32,   32,  416,  763,  381,  416,  190,
 /*  1090 */   416,  113,  113,  416,  373,  114,  114,  416,  115,  115,
 /*  1100 */    52,   52,  416,   33,   33,  416,   98,   98,   49,   49,
 /*  1110 */   416,   99,   99,  416,  273,  100,  100,  859,  680,  416,
 /*  1120 */    96,   96,  416,  112,  112,  405,  695,  416,  110,  110,
 /*  1130 */   416,  104,  104,  416,  637,  416,  160,  103,  103,  416,
 /*  1140 */   101,  101,  416,  402,  311,  102,  102,  313,   51,   51,
 /*  1150 */   111,   53,   53,   50,   50,  406,  410,   25,   25,  111,
 /*  1160 */    28,   28,  739,  210,  163,  707,  740,  111,  908,  344,
 /*  1170 */   706,  346,  190,  676,  190,  245,  111,  361,   66,  252,
 /*  1180 */   195,  260,   66,  704,  659,  832,   69,  733,  111,  111,
 /*  1190 */   190,  828,  776,  259,  195,  841,  840,  841,  840,   64,
 /*  1200 */   644,  664,  677,  107,  255,  731,  764,  713,  397,  274,
 /*  1210 */   205,  659,  832,  275,  771,  647,  639,  630,  629,  776,
 /*  1220 */   631,  886,  263,  155,  340,  265,  267,  148,  753,    7,
 /*  1230 */   241,  331,  233,  858,  348,  269,  401,  663,  280,  156,
 /*  1240 */   889,  366,  161,  258,  923,  124,  830,  829,  135,  145,
 /*  1250 */   121,  334,   64,  339,  359,  843,   55,  238,  174,  149,
 /*  1260 */   144,  178,  126,  357,  179,  180,  292,  372,  128,  129,
 /*  1270 */   130,  131,  683,  139,  760,  750,  308,  682,  681,  674,
 /*  1280 */   661,  860,  824,  655,  387,  293,  654,   63,  257,    6,
 /*  1290 */   653,   71,  899,  312,  673,   94,  392,   65,  721,  390,
 /*  1300 */   200,  636,  262,  404,   21,  887,  722,  264,  419,  720,
 /*  1310 */   266,  408,  719,  201,  204,  268,  203,  224,  625,  420,
 /*  1320 */   627,  116,  626,  623,  106,  288,  221,  117,  118,  622,
 /*  1330 */   319,  230,  167,  322,  105,  169,  838,  836,  171,  277,
 /*  1340 */   703,  276,  278,  759,  810,  279,  125,  119,  173,  127,
 /*  1350 */   693,  846,  190,  132,  854,  919,  338,  133,  134,  136,
 /*  1360 */   225,  226,   56,   57,   58,   59,  120,  857,  175,  176,
 /*  1370 */   853,    8,  177,   12,  246,  635,  259,  150,  358,  181,
 /*  1380 */   141,  362,   60,   13,  367,  672,  253,   14,   61,  222,
 /*  1390 */   779,  778,  122,  808,  701,   62,  619,   15,  162,  705,
 /*  1400 */   812,    4,  189,  380,  207,  209,  142,  727,  732,   69,
 /*  1410 */    16,   66,  823,   17,  809,  807,  863,  197, 1186,  926,
 /*  1420 */   862,  193,  194,  403,  879,  152,  284,  880,  196,  153,
 /*  1430 */   407,  811,  154,  926,  777,  645,   79,  198,  281, 1197,
 /*  1440 */   926,  926,  926,  926,  947,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*    10 */    15,   16,   17,   18,   19,   20,   51,   22,   23,   24,
 /*    20 */    25,   26,   27,   28,   29,   30,   31,   32,   17,   18,
 /*    30 */    19,   20,   21,   22,   23,   24,   25,   26,   27,   28,
 /*    40 */    29,   30,   31,   32,   49,   80,    7,   52,   17,   18,
 /*    50 */    19,   20,    9,   22,   23,   24,   25,   26,   27,   28,
 /*    60 */    29,   30,   31,   32,   26,   27,   28,   29,   30,   31,
 /*    70 */    32,   32,  107,   32,   79,   28,   29,   30,   31,   32,
 /*    80 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*    90 */    15,   16,   17,   18,   19,   20,  134,   22,   23,   24,
 /*   100 */    25,   26,   27,   28,   29,   30,   31,   32,   22,   23,
 /*   110 */    24,   25,   26,   27,   28,   29,   30,   31,   32,  161,
 /*   120 */    81,   82,   83,   84,   85,   86,   87,  169,    9,   10,
 /*   130 */    11,   12,   89,   58,   95,  102,  103,   62,  162,    5,
 /*   140 */     6,    7,    8,    9,   10,   11,   12,   13,   14,   15,
 /*   150 */    16,   17,   18,   19,   20,  162,   22,   23,   24,   25,
 /*   160 */    26,   27,   28,   29,   30,   31,   32,  134,  144,    5,
 /*   170 */     6,    7,    8,    9,   10,   11,   12,   13,   14,   15,
 /*   180 */    16,   17,   18,   19,   20,   51,   22,   23,   24,   25,
 /*   190 */    26,   27,   28,   29,   30,   31,   32,   72,  200,  144,
 /*   200 */   140,  141,   83,   69,    1,    2,  146,  183,  148,  211,
 /*   210 */   212,  213,   48,  153,  162,   90,   91,   92,    5,    6,
 /*   220 */     7,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   230 */    17,   18,   19,   20,   72,   22,   23,   24,   25,   26,
 /*   240 */    27,   28,   29,   30,   31,   32,  186,  137,  138,  139,
 /*   250 */    31,   32,   90,   91,   92,  122,   77,  124,   79,  134,
 /*   260 */   165,   48,   83,  208,  102,  103,  153,    5,    6,    7,
 /*   270 */     8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
 /*   280 */    18,   19,   20,  144,   22,   23,   24,   25,   26,   27,
 /*   290 */    28,   29,   30,   31,   32,  116,  117,  118,   81,  186,
 /*   300 */   165,   84,   85,   86,   26,   27,  211,  212,  213,   98,
 /*   310 */    48,  100,  101,   96,  201,  202,    5,    6,    7,    8,
 /*   320 */     9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
 /*   330 */    19,   20,  144,   22,   23,   24,   25,   26,   27,   28,
 /*   340 */    29,   30,   31,   32,   51,  144,  211,  212,  213,  144,
 /*   350 */   162,  163,  211,  212,  213,  158,  159,  160,   82,   48,
 /*   360 */   158,  159,  160,  162,  163,    5,    6,    7,    8,    9,
 /*   370 */    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
 /*   380 */    20,   88,   22,   23,   24,   25,   26,   27,   28,   29,
 /*   390 */    30,   31,   32,  144,  116,  144,  118,  144,  197,  198,
 /*   400 */   169,  170,  175,  215,   90,   91,   92,  154,   48,  178,
 /*   410 */   134,  162,  163,  208,    5,    6,    7,    8,    9,   10,
 /*   420 */    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
 /*   430 */   144,   22,   23,   24,   25,   26,   27,   28,   29,   30,
 /*   440 */    31,   32,    5,    6,    7,    8,    9,   10,   11,   12,
 /*   450 */    13,   14,   15,   16,   17,   18,   19,   20,  207,   22,
 /*   460 */    23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
 /*   470 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*   480 */    15,   16,   17,   18,   19,   20,  144,   22,   23,   24,
 /*   490 */    25,   26,   27,   28,   29,   30,   31,   32,    5,    6,
 /*   500 */     7,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   510 */    17,   18,   19,   20,   49,   22,   23,   24,   25,   26,
 /*   520 */    27,   28,   29,   30,   31,   32,   90,   91,   92,  102,
 /*   530 */   103,  144,  123,    7,    8,    9,   10,   11,   12,   13,
 /*   540 */    14,   15,   16,   17,   18,   19,   20,  144,   22,   23,
 /*   550 */    24,   25,   26,   27,   28,   29,   30,   31,   32,  144,
 /*   560 */   123,  180,  181,  180,  181,  144,    5,    6,    7,    8,
 /*   570 */     9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
 /*   580 */    19,   20,  144,   22,   23,   24,   25,   26,   27,   28,
 /*   590 */    29,   30,   31,   32,    6,    7,    8,    9,   10,   11,
 /*   600 */    12,   13,   14,   15,   16,   17,   18,   19,   20,    7,
 /*   610 */    22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
 /*   620 */    32,  183,  156,  157,  144,   52,   53,  144,   26,   27,
 /*   630 */    47,   52,   52,   53,   34,   33,  144,   37,   38,   39,
 /*   640 */    40,   41,  162,  163,   44,   26,   27,  232,   46,   47,
 /*   650 */    48,  203,   79,   80,   52,   53,  153,   57,   79,   79,
 /*   660 */    80,   81,  184,  185,   84,   85,   86,  153,  230,   39,
 /*   670 */    70,   52,   53,   71,  196,   75,   96,  197,  198,   77,
 /*   680 */   107,   79,   80,  144,   54,   83,  144,    7,  105,  186,
 /*   690 */   144,  184,  185,   63,   64,  153,  144,   97,   79,   80,
 /*   700 */   186,   82,   72,  196,  162,  163,   26,   27,   89,  144,
 /*   710 */    91,  228,  209,   33,  162,  163,  209,  200,  116,  117,
 /*   720 */   118,  119,  120,  121,   38,  222,   46,   47,  186,  144,
 /*   730 */   227,  131,   52,   53,  134,  116,  134,  118,  153,  197,
 /*   740 */   198,  218,   52,   53,  221,  189,  207,  162,  163,  197,
 /*   750 */   198,   71,   52,   53,  144,  199,   47,   77,   72,   79,
 /*   760 */    80,   52,   53,   83,  144,    7,   52,   53,  144,   79,
 /*   770 */    80,  186,  162,  163,  228,  144,   90,   91,   92,   79,
 /*   780 */    80,   47,  197,   49,   26,   27,   28,   48,   79,   80,
 /*   790 */    51,   33,   48,   79,   80,   51,  116,  117,  118,  119,
 /*   800 */   120,  121,   28,  183,   46,   47,  144,  197,  198,  144,
 /*   810 */    52,   53,  150,   47,  134,  144,  154,   47,   52,   53,
 /*   820 */   134,  107,   52,   53,   80,  135,   52,   53,   49,   71,
 /*   830 */   171,   52,  144,   56,   47,   77,   59,   79,   80,   52,
 /*   840 */    53,   83,  144,    7,   67,   79,   80,  159,  160,   79,
 /*   850 */    80,  107,  144,   79,   80,    7,  144,    9,   79,  144,
 /*   860 */   162,  163,   26,   27,  153,  144,   79,   80,  144,   33,
 /*   870 */   162,  163,  144,  144,  116,  117,  118,  119,  120,  121,
 /*   880 */   144,   39,   46,   47,  176,    7,  162,  163,   52,   53,
 /*   890 */   162,  163,  144,  228,   43,  197,   54,  186,  162,  163,
 /*   900 */   176,  156,  157,  144,  176,   63,   64,   71,  220,   99,
 /*   910 */   162,  163,  144,   77,  104,   79,   80,  144,   47,   83,
 /*   920 */   209,    7,  207,   52,  176,   74,  144,  159,  160,  144,
 /*   930 */    47,   53,   49,  197,  144,   52,   85,   89,  227,   43,
 /*   940 */    26,   27,  183,   95,  154,   60,  144,   33,   26,   27,
 /*   950 */    79,  144,  116,  117,  118,  119,  120,  121,   73,  144,
 /*   960 */    46,   47,   79,  173,  144,  175,   52,   53,  144,   47,
 /*   970 */    74,  144,  144,  144,   52,   53,   16,  126,   93,   94,
 /*   980 */   207,  144,  162,  163,  144,   71,  162,  163,  220,  162,
 /*   990 */   163,   77,  177,   79,   80,  181,  144,   83,  144,   77,
 /*  1000 */   144,   79,  162,  163,    7,   83,  144,   47,  144,  144,
 /*  1010 */   144,  183,  183,  144,  162,  163,  162,  163,  162,  163,
 /*  1020 */   183,  125,  126,  144,  162,  163,  162,  163,  144,  171,
 /*  1030 */   116,  117,  118,  119,  120,  121,  144,  144,  116,  117,
 /*  1040 */   118,  162,  163,  144,  144,  144,  162,  163,  183,  144,
 /*  1050 */    53,  144,  183,  144,  162,  163,  144,  144,   82,   83,
 /*  1060 */   144,  162,  163,  162,  163,  144,  171,  162,  163,  162,
 /*  1070 */   163,  162,  163,  144,  162,  163,  185,  144,  162,  163,
 /*  1080 */   144,  144,  144,  162,  163,  144,   48,  196,  144,   51,
 /*  1090 */   144,  162,  163,  144,   28,  162,  163,  144,  162,  163,
 /*  1100 */   162,  163,  144,  162,  163,  144,  162,  163,  162,  163,
 /*  1110 */   144,  162,  163,  144,  144,  162,  163,  153,  144,  144,
 /*  1120 */   162,  163,  144,  162,  163,  232,  153,  144,  162,  163,
 /*  1130 */   144,  162,  163,  144,  153,  144,   51,  162,  163,  144,
 /*  1140 */   162,  163,  144,  153,  207,  162,  163,  177,  162,  163,
 /*  1150 */   186,  162,  163,  162,  163,  153,  153,  162,  163,  186,
 /*  1160 */   162,  163,   58,  201,  202,   99,   62,  186,   51,   48,
 /*  1170 */   104,   48,   51,   36,   51,   48,  186,   48,   51,   48,
 /*  1180 */    51,   83,   51,   48,   52,   52,   51,   48,  186,  186,
 /*  1190 */    51,   48,   52,   95,   51,  116,  116,  118,  118,  114,
 /*  1200 */    48,  144,   65,   51,  144,  144,  144,  144,  144,  144,
 /*  1210 */   223,   79,   79,  144,  144,  144,  144,  144,  144,   79,
 /*  1220 */   144,  144,  200,  106,  224,  200,  200,  187,  191,  188,
 /*  1230 */   229,  204,  204,  191,  229,  204,  217,  170,  190,  188,
 /*  1240 */   147,  166,  174,  165,  133,  231,  165,  165,   47,  210,
 /*  1250 */     5,   45,  114,  128,   45,  226,  122,  225,  149,  210,
 /*  1260 */    47,  149,  179,  167,  149,  149,  167,   89,  182,  182,
 /*  1270 */   182,  182,  164,  179,  179,  191,   66,  164,  164,  172,
 /*  1280 */    88,  191,  191,  164,  109,  167,  166,   89,  164,   47,
 /*  1290 */   164,  122,  164,   32,  172,  113,  110,  112,  206,  111,
 /*  1300 */    50,  152,  205,  167,   51,   40,  206,  205,  151,  206,
 /*  1310 */   205,  167,  206,  145,   35,  205,  145,  216,   36,  143,
 /*  1320 */   143,  155,  143,  143,  168,  142,  168,  155,  155,    4,
 /*  1330 */     3,  132,   42,   76,   43,   89,   48,   48,  105,  193,
 /*  1340 */   195,  194,  192,  103,  214,  191,  115,   93,   89,  106,
 /*  1350 */    46,  127,   51,  127,    1,  130,  129,   89,  106,  115,
 /*  1360 */   219,  219,   16,   16,   16,   16,   93,   53,  108,  105,
 /*  1370 */     1,   34,   89,   47,  125,   46,   95,   49,    7,   87,
 /*  1380 */    47,   68,   47,   47,   68,   55,   48,   47,   47,   68,
 /*  1390 */    48,   48,   61,   48,   99,   51,    1,   47,  105,   48,
 /*  1400 */    38,   47,  108,   51,   48,   48,   47,  107,   53,   51,
 /*  1410 */   108,   51,   48,  108,   48,   48,   48,  105,    0,  233,
 /*  1420 */    48,   51,   47,   49,   48,   47,   42,   48,   51,   47,
 /*  1430 */    49,   48,   47,  233,   48,   48,   47,  105,   48,  105,
 /*  1440 */   233,  233,  233,  233,  105,
};
#define YY_SHIFT_USE_DFLT (1445)
#define YY_SHIFT_COUNT    (422)
#define YY_SHIFT_MIN      (-38)
#define YY_SHIFT_MAX      (1418)
static const short yy_shift_ofst[] = {
 /*     0 */   203,  602,  680,  600,  836,  836,  836,  836,  125,   -5,
 /*    10 */    75,   75,  836,  836,  836,  836,  836,  836,  836,  619,
 /*    20 */   619,  580,  162,  686,   33,  134,  164,  213,  262,  311,
 /*    30 */   360,  409,  437,  465,  493,  493,  493,  493,  493,  493,
 /*    40 */   493,  493,  493,  493,  493,  493,  493,  493,  493,  561,
 /*    50 */   493,  588,  526,  526,  758,  836,  836,  836,  836,  836,
 /*    60 */   836,  836,  836,  836,  836,  836,  836,  836,  836,  836,
 /*    70 */   836,  836,  836,  836,  836,  836,  836,  836,  836,  836,
 /*    80 */   836,  836,  914,  836,  836,  836,  836,  836,  836,  836,
 /*    90 */   836,  836,  836,  836,  836,  836,   11,   31,   31,   31,
 /*   100 */    31,   31,   86,   38,   47,  700,  848,  278,  278,  700,
 /*   110 */   219,  427,   41, 1445, 1445, 1445,   39,   39,   39,  709,
 /*   120 */   709,  630,  630,  690,  700,  700,  700,  700,  700,  700,
 /*   130 */   700,  700,  700,  700,  700,  700,  700,  700,  700,  700,
 /*   140 */   851,  700,  700,  700,  700,  276,  579,  579,  427,  -38,
 /*   150 */   -38,  -38,  -38,  -38,  -38, 1445, 1445,  922,  179,  179,
 /*   160 */   766,  217,  774,  770,  573,  714,  787,  700,  700,  700,
 /*   170 */   700,  700,  700,  700,  700,  700,  700,  700,  700,  700,
 /*   180 */   700,  700,  700,  885,  885,  885,  700,  700,  744,  700,
 /*   190 */   700,  700,  883,  700,  842,  700,  700,  700,  700,  700,
 /*   200 */   700,  700,  700,  700,  700,  314,  777,  779,  779,  779,
 /*   210 */   -35,  211, 1066, 1085,  878,  878,  997, 1085,  997,  293,
 /*   220 */   739,   43, 1104,  878,  133, 1104, 1104, 1117,  810,  734,
 /*   230 */  1111, 1201, 1245, 1138, 1206, 1206, 1206, 1206, 1134, 1125,
 /*   240 */  1209, 1138, 1201, 1245, 1245, 1138, 1209, 1213, 1209, 1209,
 /*   250 */  1213, 1178, 1178, 1178, 1210, 1213, 1178, 1192, 1178, 1210,
 /*   260 */  1178, 1178, 1175, 1198, 1175, 1198, 1175, 1198, 1175, 1198,
 /*   270 */  1242, 1169, 1213, 1261, 1261, 1213, 1182, 1186, 1185, 1188,
 /*   280 */  1138, 1250, 1253, 1265, 1265, 1279, 1279, 1279, 1279, 1282,
 /*   290 */  1445, 1445, 1445, 1445, 1445,  119,  896,  436,  871,  960,
 /*   300 */  1038, 1121, 1123, 1127, 1129, 1131, 1132,  976, 1137, 1098,
 /*   310 */  1135, 1139, 1133, 1143, 1079, 1080, 1152, 1140,  583, 1325,
 /*   320 */  1327, 1199, 1290, 1257, 1291, 1246, 1288, 1289, 1233, 1240,
 /*   330 */  1231, 1254, 1243, 1259, 1304, 1224, 1301, 1226, 1225, 1227,
 /*   340 */  1268, 1353, 1252, 1244, 1346, 1347, 1348, 1349, 1273, 1314,
 /*   350 */  1260, 1264, 1369, 1337, 1326, 1283, 1249, 1328, 1329, 1371,
 /*   360 */  1281, 1292, 1333, 1313, 1335, 1336, 1338, 1340, 1316, 1330,
 /*   370 */  1341, 1321, 1331, 1342, 1343, 1345, 1344, 1295, 1350, 1351,
 /*   380 */  1354, 1352, 1293, 1356, 1357, 1355, 1294, 1359, 1300, 1358,
 /*   390 */  1302, 1360, 1305, 1364, 1358, 1366, 1367, 1368, 1370, 1372,
 /*   400 */  1375, 1362, 1376, 1378, 1374, 1377, 1379, 1382, 1381, 1377,
 /*   410 */  1383, 1385, 1386, 1387, 1389, 1312, 1332, 1334, 1339, 1390,
 /*   420 */  1384, 1395, 1418,
};
#define YY_REDUCE_USE_DFLT (-43)
#define YY_REDUCE_COUNT (294)
#define YY_REDUCE_MIN   (-42)
#define YY_REDUCE_MAX   (1183)
static const short yy_reduce_ofst[] = {
 /*     0 */   110,  542,  585,   60,  201,  480,  552,  610,  503,   -2,
 /*    10 */    95,  135,  708,  724,  728,  698,  736,  748,  188,  688,
 /*    20 */   768,  790,  507,  711,  113,  141,  141,  141,  141,  141,
 /*    30 */   141,  141,  141,  141,  141,  141,  141,  141,  141,  141,
 /*    40 */   141,  141,  141,  141,  141,  141,  141,  141,  141,  141,
 /*    50 */   141,  141,  141,  141,  249,  820,  824,  827,  840,  852,
 /*    60 */   854,  856,  862,  864,  879,  884,  892,  899,  901,  905,
 /*    70 */   907,  909,  912,  916,  921,  929,  933,  936,  938,  941,
 /*    80 */   944,  946,  949,  953,  958,  961,  966,  969,  975,  978,
 /*    90 */   983,  986,  989,  991,  995,  998,  141,  141,  141,  141,
 /*   100 */   141,  141,  141,  141,  141,  438,  231,  197,  202,  662,
 /*   110 */   141,  478,  141,  141,  141,  141,  -42,  -42,  -42,   55,
 /*   120 */   205,  381,  383,  415,  253,   24,  620,  759,  828,  829,
 /*   130 */   837,  865,  251,  869,  483,  539,  546,  715,  773,  665,
 /*   140 */   523,  815,  937,  893,  970,  514,  466,  745,  891,  964,
 /*   150 */   973,  981,  990, 1002, 1003,  962,  556,  -24,   -7,   52,
 /*   160 */   139,  227,  286,  342,  387,  403,  421,  492,  565,  624,
 /*   170 */   631,  671,  712,  721,  729,  782,  785,  802,  807,  866,
 /*   180 */   900,  913,  974,  659,  858,  895, 1057, 1060,  448, 1061,
 /*   190 */  1062, 1063,  517, 1064,  814, 1065, 1069, 1070,  286, 1071,
 /*   200 */  1072, 1073, 1074, 1076, 1077, 1000,  987, 1022, 1025, 1026,
 /*   210 */   448, 1040, 1041, 1037, 1027, 1028, 1001, 1042, 1005, 1075,
 /*   220 */  1068, 1067, 1078, 1031, 1019, 1081, 1082, 1048, 1051, 1093,
 /*   230 */  1014, 1039, 1083, 1084, 1086, 1087, 1088, 1089, 1029, 1032,
 /*   240 */  1109, 1090, 1049, 1094, 1095, 1091, 1112, 1096, 1115, 1116,
 /*   250 */  1099, 1108, 1113, 1114, 1107, 1118, 1119, 1120, 1124, 1122,
 /*   260 */  1126, 1128, 1092, 1097, 1100, 1102, 1103, 1105, 1106, 1110,
 /*   270 */  1130, 1101, 1136, 1141, 1142, 1144, 1145, 1147, 1146, 1150,
 /*   280 */  1154, 1149, 1157, 1168, 1171, 1176, 1177, 1179, 1180, 1183,
 /*   290 */  1166, 1172, 1156, 1158, 1173,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */  1187, 1181, 1181, 1181, 1124, 1124, 1124, 1124, 1181, 1019,
 /*    10 */  1046, 1046, 1229, 1229, 1229, 1229, 1229, 1229, 1123, 1229,
 /*    20 */  1229, 1229, 1229, 1181, 1023, 1052, 1229, 1229, 1229, 1125,
 /*    30 */  1126, 1229, 1229, 1229, 1157, 1062, 1061, 1060, 1059, 1033,
 /*    40 */  1057, 1050, 1054, 1125, 1119, 1120, 1118, 1122, 1126, 1229,
 /*    50 */  1053, 1088, 1103, 1087, 1229, 1229, 1229, 1229, 1229, 1229,
 /*    60 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*    70 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*    80 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*    90 */  1229, 1229, 1229, 1229, 1229, 1229, 1097, 1102, 1109, 1101,
 /*   100 */  1098, 1090, 1089, 1091, 1092, 1229,  990, 1229, 1229, 1229,
 /*   110 */  1093, 1229, 1094, 1106, 1105, 1104, 1179, 1196, 1195, 1229,
 /*   120 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   130 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   140 */  1131, 1229, 1229, 1229, 1229, 1181,  948,  948, 1229, 1181,
 /*   150 */  1181, 1181, 1181, 1181, 1181, 1023, 1014, 1229, 1229, 1229,
 /*   160 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1176, 1229,
 /*   170 */  1173, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   180 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   190 */  1229, 1229, 1019, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   200 */  1229, 1229, 1229, 1229, 1190, 1229, 1152, 1019, 1019, 1019,
 /*   210 */  1021, 1003, 1013, 1056, 1035, 1035, 1226, 1056, 1226,  965,
 /*   220 */  1208,  962, 1046, 1035, 1121, 1046, 1046, 1020, 1013, 1229,
 /*   230 */  1227, 1067,  993, 1056,  999,  999,  999,  999, 1156, 1223,
 /*   240 */   939, 1056, 1067,  993,  993, 1056,  939, 1132,  939,  939,
 /*   250 */  1132,  991,  991,  991,  980, 1132,  991,  965,  991,  980,
 /*   260 */   991,  991, 1039, 1034, 1039, 1034, 1039, 1034, 1039, 1034,
 /*   270 */  1127, 1229, 1132, 1136, 1136, 1132, 1051, 1040, 1049, 1047,
 /*   280 */  1056,  943,  983, 1193, 1193, 1189, 1189, 1189, 1189,  929,
 /*   290 */  1203, 1203,  967,  967, 1203, 1229, 1229, 1229, 1198, 1139,
 /*   300 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   310 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1073, 1229,
 /*   320 */   926, 1229, 1229, 1180, 1229, 1174, 1229, 1229, 1218, 1229,
 /*   330 */  1229, 1229, 1229, 1229, 1229, 1229, 1155, 1154, 1229, 1229,
 /*   340 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   350 */  1229, 1225, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   360 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   370 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1005, 1229, 1229,
 /*   380 */  1229, 1212, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1048,
 /*   390 */  1229, 1041, 1229, 1229, 1216, 1229, 1229, 1229, 1229, 1229,
 /*   400 */  1229, 1229, 1229, 1229, 1229, 1183, 1229, 1229, 1229, 1182,
 /*   410 */  1229, 1229, 1229, 1229, 1229, 1075, 1229, 1074, 1078, 1229,
 /*   420 */   933, 1229, 1229,
};
/********** End of lemon-generated parsing tables *****************************/

/* The next table maps tokens (terminal symbols) into fallback tokens.  
** If a construct like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
**
** This feature can be used, for example, to cause some keywords in a language
** to revert to identifiers if they keyword does not apply in the context where
** it appears.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
    0,  /*          $ => nothing */
    0,  /*       SEMI => nothing */
   52,  /*    EXPLAIN => ID */
   52,  /*      QUERY => ID */
   52,  /*       PLAN => ID */
    0,  /*         OR => nothing */
    0,  /*        AND => nothing */
    0,  /*        NOT => nothing */
    0,  /*         IS => nothing */
   52,  /*      MATCH => ID */
    0,  /*    LIKE_KW => nothing */
    0,  /*    BETWEEN => nothing */
    0,  /*         IN => nothing */
    0,  /*     ISNULL => nothing */
    0,  /*    NOTNULL => nothing */
    0,  /*         NE => nothing */
    0,  /*         EQ => nothing */
    0,  /*         GT => nothing */
    0,  /*         LE => nothing */
    0,  /*         LT => nothing */
    0,  /*         GE => nothing */
    0,  /*     ESCAPE => nothing */
    0,  /*     BITAND => nothing */
    0,  /*      BITOR => nothing */
    0,  /*     LSHIFT => nothing */
    0,  /*     RSHIFT => nothing */
    0,  /*       PLUS => nothing */
    0,  /*      MINUS => nothing */
    0,  /*       STAR => nothing */
    0,  /*      SLASH => nothing */
    0,  /*        REM => nothing */
    0,  /*     CONCAT => nothing */
    0,  /*    COLLATE => nothing */
    0,  /*     BITNOT => nothing */
    0,  /*      BEGIN => nothing */
    0,  /* TRANSACTION => nothing */
   52,  /*   DEFERRED => ID */
    0,  /*     COMMIT => nothing */
    0,  /*        END => nothing */
    0,  /*   ROLLBACK => nothing */
    0,  /*  SAVEPOINT => nothing */
   52,  /*    RELEASE => ID */
    0,  /*         TO => nothing */
    0,  /*      TABLE => nothing */
    0,  /*     CREATE => nothing */
   52,  /*         IF => ID */
    0,  /*     EXISTS => nothing */
    0,  /*         LP => nothing */
    0,  /*         RP => nothing */
    0,  /*         AS => nothing */
    0,  /*    WITHOUT => nothing */
    0,  /*      COMMA => nothing */
    0,  /*         ID => nothing */
    0,  /*    INDEXED => nothing */
   52,  /*      ABORT => ID */
   52,  /*     ACTION => ID */
   52,  /*      AFTER => ID */
   52,  /*    ANALYZE => ID */
   52,  /*        ASC => ID */
   52,  /*     BEFORE => ID */
   52,  /*    CASCADE => ID */
   52,  /*   CONFLICT => ID */
   52,  /*       DESC => ID */
   52,  /*       FAIL => ID */
   52,  /*     IGNORE => ID */
   52,  /*  IMMEDIATE => ID */
   52,  /*  INITIALLY => ID */
   52,  /*    INSTEAD => ID */
   52,  /*        KEY => ID */
   52,  /*     OFFSET => ID */
   52,  /*     PRAGMA => ID */
   52,  /*      RAISE => ID */
   52,  /*    REPLACE => ID */
   52,  /*   RESTRICT => ID */
   52,  /*       VIEW => ID */
   52,  /*    REINDEX => ID */
   52,  /*     RENAME => ID */
   52,  /*   CTIME_KW => ID */
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
**
** After the "shift" half of a SHIFTREDUCE action, the stateno field
** actually contains the reduce action for the second half of the
** SHIFTREDUCE.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number, or reduce action in SHIFTREDUCE */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  yyStackEntry *yytos;          /* Pointer to top element of the stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyhwm;                    /* High-water mark of the stack */
#endif
#ifndef YYNOERRORRECOVERY
  int yyerrcnt;                 /* Shifts left before out of the error */
#endif
  bool is_fallback_failed;      /* Shows if fallback failed or not */
  sqlite3ParserARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
  yyStackEntry yystk0;          /* First stack entry */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3ParserTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "SEMI",          "EXPLAIN",       "QUERY",       
  "PLAN",          "OR",            "AND",           "NOT",         
  "IS",            "MATCH",         "LIKE_KW",       "BETWEEN",     
  "IN",            "ISNULL",        "NOTNULL",       "NE",          
  "EQ",            "GT",            "LE",            "LT",          
  "GE",            "ESCAPE",        "BITAND",        "BITOR",       
  "LSHIFT",        "RSHIFT",        "PLUS",          "MINUS",       
  "STAR",          "SLASH",         "REM",           "CONCAT",      
  "COLLATE",       "BITNOT",        "BEGIN",         "TRANSACTION", 
  "DEFERRED",      "COMMIT",        "END",           "ROLLBACK",    
  "SAVEPOINT",     "RELEASE",       "TO",            "TABLE",       
  "CREATE",        "IF",            "EXISTS",        "LP",          
  "RP",            "AS",            "WITHOUT",       "COMMA",       
  "ID",            "INDEXED",       "ABORT",         "ACTION",      
  "AFTER",         "ANALYZE",       "ASC",           "BEFORE",      
  "CASCADE",       "CONFLICT",      "DESC",          "FAIL",        
  "IGNORE",        "IMMEDIATE",     "INITIALLY",     "INSTEAD",     
  "KEY",           "OFFSET",        "PRAGMA",        "RAISE",       
  "REPLACE",       "RESTRICT",      "VIEW",          "REINDEX",     
  "RENAME",        "CTIME_KW",      "ANY",           "STRING",      
  "JOIN_KW",       "CONSTRAINT",    "DEFAULT",       "NULL",        
  "PRIMARY",       "UNIQUE",        "CHECK",         "REFERENCES",  
  "AUTOINCR",      "ON",            "INSERT",        "DELETE",      
  "UPDATE",        "SET",           "NO",            "DEFERRABLE",  
  "FOREIGN",       "DROP",          "UNION",         "ALL",         
  "EXCEPT",        "INTERSECT",     "SELECT",        "VALUES",      
  "DISTINCT",      "DOT",           "FROM",          "JOIN",        
  "BY",            "USING",         "ORDER",         "GROUP",       
  "HAVING",        "LIMIT",         "WHERE",         "INTO",        
  "FLOAT",         "BLOB",          "INTEGER",       "VARIABLE",    
  "CAST",          "CASE",          "WHEN",          "THEN",        
  "ELSE",          "INDEX",         "TRIGGER",       "OF",          
  "FOR",           "EACH",          "ROW",           "ALTER",       
  "ADD",           "COLUMNKW",      "WITH",          "RECURSIVE",   
  "error",         "input",         "ecmd",          "explain",     
  "cmdx",          "cmd",           "transtype",     "trans_opt",   
  "nm",            "savepoint_opt",  "create_table",  "create_table_args",
  "createkw",      "ifnotexists",   "columnlist",    "conslist_opt",
  "table_options",  "select",        "columnname",    "carglist",    
  "typetoken",     "typename",      "signed",        "plus_num",    
  "minus_num",     "ccons",         "term",          "expr",        
  "onconf",        "sortorder",     "autoinc",       "eidlist_opt", 
  "refargs",       "defer_subclause",  "refarg",        "refact",      
  "init_deferred_pred_opt",  "conslist",      "tconscomma",    "tcons",       
  "sortlist",      "eidlist",       "defer_subclause_opt",  "orconf",      
  "resolvetype",   "raisetype",     "ifexists",      "fullname",    
  "selectnowith",  "oneselect",     "with",          "multiselect_op",
  "distinct",      "selcollist",    "from",          "where_opt",   
  "groupby_opt",   "having_opt",    "orderby_opt",   "limit_opt",   
  "values",        "nexprlist",     "exprlist",      "sclp",        
  "as",            "seltablist",    "stl_prefix",    "joinop",      
  "indexed_opt",   "on_opt",        "using_opt",     "idlist",      
  "setlist",       "insert_cmd",    "idlist_opt",    "likeop",      
  "between_op",    "in_op",         "paren_exprlist",  "case_operand",
  "case_exprlist",  "case_else",     "uniqueflag",    "collate",     
  "nmnum",         "trigger_decl",  "trigger_cmd_list",  "trigger_time",
  "trigger_event",  "foreach_clause",  "when_clause",   "trigger_cmd", 
  "trnm",          "tridxby",       "add_column_fullname",  "kwcolumn_opt",
  "wqlist",      
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "ecmd ::= explain cmdx SEMI",
 /*   1 */ "ecmd ::= SEMI",
 /*   2 */ "explain ::= EXPLAIN",
 /*   3 */ "explain ::= EXPLAIN QUERY PLAN",
 /*   4 */ "cmd ::= BEGIN transtype trans_opt",
 /*   5 */ "transtype ::=",
 /*   6 */ "transtype ::= DEFERRED",
 /*   7 */ "cmd ::= COMMIT trans_opt",
 /*   8 */ "cmd ::= END trans_opt",
 /*   9 */ "cmd ::= ROLLBACK trans_opt",
 /*  10 */ "cmd ::= SAVEPOINT nm",
 /*  11 */ "cmd ::= RELEASE savepoint_opt nm",
 /*  12 */ "cmd ::= ROLLBACK trans_opt TO savepoint_opt nm",
 /*  13 */ "create_table ::= createkw TABLE ifnotexists nm",
 /*  14 */ "createkw ::= CREATE",
 /*  15 */ "ifnotexists ::=",
 /*  16 */ "ifnotexists ::= IF NOT EXISTS",
 /*  17 */ "create_table_args ::= LP columnlist conslist_opt RP table_options",
 /*  18 */ "create_table_args ::= AS select",
 /*  19 */ "table_options ::=",
 /*  20 */ "table_options ::= WITHOUT nm",
 /*  21 */ "columnname ::= nm typetoken",
 /*  22 */ "nm ::= ID|INDEXED",
 /*  23 */ "nm ::= STRING",
 /*  24 */ "typetoken ::=",
 /*  25 */ "typetoken ::= typename LP signed RP",
 /*  26 */ "typetoken ::= typename LP signed COMMA signed RP",
 /*  27 */ "typename ::= typename ID|STRING",
 /*  28 */ "ccons ::= CONSTRAINT nm",
 /*  29 */ "ccons ::= DEFAULT term",
 /*  30 */ "ccons ::= DEFAULT LP expr RP",
 /*  31 */ "ccons ::= DEFAULT PLUS term",
 /*  32 */ "ccons ::= DEFAULT MINUS term",
 /*  33 */ "ccons ::= DEFAULT ID|INDEXED",
 /*  34 */ "ccons ::= NOT NULL onconf",
 /*  35 */ "ccons ::= PRIMARY KEY sortorder onconf autoinc",
 /*  36 */ "ccons ::= UNIQUE onconf",
 /*  37 */ "ccons ::= CHECK LP expr RP",
 /*  38 */ "ccons ::= REFERENCES nm eidlist_opt refargs",
 /*  39 */ "ccons ::= defer_subclause",
 /*  40 */ "ccons ::= COLLATE ID|STRING",
 /*  41 */ "autoinc ::=",
 /*  42 */ "autoinc ::= AUTOINCR",
 /*  43 */ "refargs ::=",
 /*  44 */ "refargs ::= refargs refarg",
 /*  45 */ "refarg ::= MATCH nm",
 /*  46 */ "refarg ::= ON INSERT refact",
 /*  47 */ "refarg ::= ON DELETE refact",
 /*  48 */ "refarg ::= ON UPDATE refact",
 /*  49 */ "refact ::= SET NULL",
 /*  50 */ "refact ::= SET DEFAULT",
 /*  51 */ "refact ::= CASCADE",
 /*  52 */ "refact ::= RESTRICT",
 /*  53 */ "refact ::= NO ACTION",
 /*  54 */ "defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt",
 /*  55 */ "defer_subclause ::= DEFERRABLE init_deferred_pred_opt",
 /*  56 */ "init_deferred_pred_opt ::=",
 /*  57 */ "init_deferred_pred_opt ::= INITIALLY DEFERRED",
 /*  58 */ "init_deferred_pred_opt ::= INITIALLY IMMEDIATE",
 /*  59 */ "conslist_opt ::=",
 /*  60 */ "tconscomma ::= COMMA",
 /*  61 */ "tcons ::= CONSTRAINT nm",
 /*  62 */ "tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf",
 /*  63 */ "tcons ::= UNIQUE LP sortlist RP onconf",
 /*  64 */ "tcons ::= CHECK LP expr RP onconf",
 /*  65 */ "tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt",
 /*  66 */ "defer_subclause_opt ::=",
 /*  67 */ "onconf ::=",
 /*  68 */ "onconf ::= ON CONFLICT resolvetype",
 /*  69 */ "orconf ::=",
 /*  70 */ "orconf ::= OR resolvetype",
 /*  71 */ "resolvetype ::= IGNORE",
 /*  72 */ "resolvetype ::= REPLACE",
 /*  73 */ "cmd ::= DROP TABLE ifexists fullname",
 /*  74 */ "ifexists ::= IF EXISTS",
 /*  75 */ "ifexists ::=",
 /*  76 */ "cmd ::= createkw VIEW ifnotexists nm eidlist_opt AS select",
 /*  77 */ "cmd ::= DROP VIEW ifexists fullname",
 /*  78 */ "cmd ::= select",
 /*  79 */ "select ::= with selectnowith",
 /*  80 */ "selectnowith ::= selectnowith multiselect_op oneselect",
 /*  81 */ "multiselect_op ::= UNION",
 /*  82 */ "multiselect_op ::= UNION ALL",
 /*  83 */ "multiselect_op ::= EXCEPT|INTERSECT",
 /*  84 */ "oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt",
 /*  85 */ "values ::= VALUES LP nexprlist RP",
 /*  86 */ "values ::= values COMMA LP exprlist RP",
 /*  87 */ "distinct ::= DISTINCT",
 /*  88 */ "distinct ::= ALL",
 /*  89 */ "distinct ::=",
 /*  90 */ "sclp ::=",
 /*  91 */ "selcollist ::= sclp expr as",
 /*  92 */ "selcollist ::= sclp STAR",
 /*  93 */ "selcollist ::= sclp nm DOT STAR",
 /*  94 */ "as ::= AS nm",
 /*  95 */ "as ::=",
 /*  96 */ "from ::=",
 /*  97 */ "from ::= FROM seltablist",
 /*  98 */ "stl_prefix ::= seltablist joinop",
 /*  99 */ "stl_prefix ::=",
 /* 100 */ "seltablist ::= stl_prefix nm as indexed_opt on_opt using_opt",
 /* 101 */ "seltablist ::= stl_prefix nm LP exprlist RP as on_opt using_opt",
 /* 102 */ "seltablist ::= stl_prefix LP select RP as on_opt using_opt",
 /* 103 */ "seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt",
 /* 104 */ "fullname ::= nm",
 /* 105 */ "joinop ::= COMMA|JOIN",
 /* 106 */ "joinop ::= JOIN_KW JOIN",
 /* 107 */ "joinop ::= JOIN_KW nm JOIN",
 /* 108 */ "joinop ::= JOIN_KW nm nm JOIN",
 /* 109 */ "on_opt ::= ON expr",
 /* 110 */ "on_opt ::=",
 /* 111 */ "indexed_opt ::=",
 /* 112 */ "indexed_opt ::= INDEXED BY nm",
 /* 113 */ "indexed_opt ::= NOT INDEXED",
 /* 114 */ "using_opt ::= USING LP idlist RP",
 /* 115 */ "using_opt ::=",
 /* 116 */ "orderby_opt ::=",
 /* 117 */ "orderby_opt ::= ORDER BY sortlist",
 /* 118 */ "sortlist ::= sortlist COMMA expr sortorder",
 /* 119 */ "sortlist ::= expr sortorder",
 /* 120 */ "sortorder ::= ASC",
 /* 121 */ "sortorder ::= DESC",
 /* 122 */ "sortorder ::=",
 /* 123 */ "groupby_opt ::=",
 /* 124 */ "groupby_opt ::= GROUP BY nexprlist",
 /* 125 */ "having_opt ::=",
 /* 126 */ "having_opt ::= HAVING expr",
 /* 127 */ "limit_opt ::=",
 /* 128 */ "limit_opt ::= LIMIT expr",
 /* 129 */ "limit_opt ::= LIMIT expr OFFSET expr",
 /* 130 */ "limit_opt ::= LIMIT expr COMMA expr",
 /* 131 */ "cmd ::= with DELETE FROM fullname indexed_opt where_opt",
 /* 132 */ "where_opt ::=",
 /* 133 */ "where_opt ::= WHERE expr",
 /* 134 */ "cmd ::= with UPDATE orconf fullname indexed_opt SET setlist where_opt",
 /* 135 */ "setlist ::= setlist COMMA nm EQ expr",
 /* 136 */ "setlist ::= setlist COMMA LP idlist RP EQ expr",
 /* 137 */ "setlist ::= nm EQ expr",
 /* 138 */ "setlist ::= LP idlist RP EQ expr",
 /* 139 */ "cmd ::= with insert_cmd INTO fullname idlist_opt select",
 /* 140 */ "cmd ::= with insert_cmd INTO fullname idlist_opt DEFAULT VALUES",
 /* 141 */ "insert_cmd ::= INSERT orconf",
 /* 142 */ "insert_cmd ::= REPLACE",
 /* 143 */ "idlist_opt ::=",
 /* 144 */ "idlist_opt ::= LP idlist RP",
 /* 145 */ "idlist ::= idlist COMMA nm",
 /* 146 */ "idlist ::= nm",
 /* 147 */ "expr ::= LP expr RP",
 /* 148 */ "term ::= NULL",
 /* 149 */ "expr ::= ID|INDEXED",
 /* 150 */ "expr ::= JOIN_KW",
 /* 151 */ "expr ::= nm DOT nm",
 /* 152 */ "expr ::= nm DOT nm DOT nm",
 /* 153 */ "term ::= FLOAT|BLOB",
 /* 154 */ "term ::= STRING",
 /* 155 */ "term ::= INTEGER",
 /* 156 */ "expr ::= VARIABLE",
 /* 157 */ "expr ::= expr COLLATE ID|STRING",
 /* 158 */ "expr ::= CAST LP expr AS typetoken RP",
 /* 159 */ "expr ::= ID|INDEXED LP distinct exprlist RP",
 /* 160 */ "expr ::= ID|INDEXED LP STAR RP",
 /* 161 */ "term ::= CTIME_KW",
 /* 162 */ "expr ::= LP nexprlist COMMA expr RP",
 /* 163 */ "expr ::= expr AND expr",
 /* 164 */ "expr ::= expr OR expr",
 /* 165 */ "expr ::= expr LT|GT|GE|LE expr",
 /* 166 */ "expr ::= expr EQ|NE expr",
 /* 167 */ "expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr",
 /* 168 */ "expr ::= expr PLUS|MINUS expr",
 /* 169 */ "expr ::= expr STAR|SLASH|REM expr",
 /* 170 */ "expr ::= expr CONCAT expr",
 /* 171 */ "likeop ::= LIKE_KW|MATCH",
 /* 172 */ "likeop ::= NOT LIKE_KW|MATCH",
 /* 173 */ "expr ::= expr likeop expr",
 /* 174 */ "expr ::= expr likeop expr ESCAPE expr",
 /* 175 */ "expr ::= expr ISNULL|NOTNULL",
 /* 176 */ "expr ::= expr NOT NULL",
 /* 177 */ "expr ::= expr IS expr",
 /* 178 */ "expr ::= expr IS NOT expr",
 /* 179 */ "expr ::= NOT expr",
 /* 180 */ "expr ::= BITNOT expr",
 /* 181 */ "expr ::= MINUS expr",
 /* 182 */ "expr ::= PLUS expr",
 /* 183 */ "between_op ::= BETWEEN",
 /* 184 */ "between_op ::= NOT BETWEEN",
 /* 185 */ "expr ::= expr between_op expr AND expr",
 /* 186 */ "in_op ::= IN",
 /* 187 */ "in_op ::= NOT IN",
 /* 188 */ "expr ::= expr in_op LP exprlist RP",
 /* 189 */ "expr ::= LP select RP",
 /* 190 */ "expr ::= expr in_op LP select RP",
 /* 191 */ "expr ::= expr in_op nm paren_exprlist",
 /* 192 */ "expr ::= EXISTS LP select RP",
 /* 193 */ "expr ::= CASE case_operand case_exprlist case_else END",
 /* 194 */ "case_exprlist ::= case_exprlist WHEN expr THEN expr",
 /* 195 */ "case_exprlist ::= WHEN expr THEN expr",
 /* 196 */ "case_else ::= ELSE expr",
 /* 197 */ "case_else ::=",
 /* 198 */ "case_operand ::= expr",
 /* 199 */ "case_operand ::=",
 /* 200 */ "exprlist ::=",
 /* 201 */ "nexprlist ::= nexprlist COMMA expr",
 /* 202 */ "nexprlist ::= expr",
 /* 203 */ "paren_exprlist ::=",
 /* 204 */ "paren_exprlist ::= LP exprlist RP",
 /* 205 */ "cmd ::= createkw uniqueflag INDEX ifnotexists nm ON nm LP sortlist RP where_opt",
 /* 206 */ "uniqueflag ::= UNIQUE",
 /* 207 */ "uniqueflag ::=",
 /* 208 */ "eidlist_opt ::=",
 /* 209 */ "eidlist_opt ::= LP eidlist RP",
 /* 210 */ "eidlist ::= eidlist COMMA nm collate sortorder",
 /* 211 */ "eidlist ::= nm collate sortorder",
 /* 212 */ "collate ::=",
 /* 213 */ "collate ::= COLLATE ID|STRING",
 /* 214 */ "cmd ::= DROP INDEX ifexists fullname ON nm",
 /* 215 */ "cmd ::= PRAGMA nm",
 /* 216 */ "cmd ::= PRAGMA nm EQ nmnum",
 /* 217 */ "cmd ::= PRAGMA nm LP nmnum RP",
 /* 218 */ "cmd ::= PRAGMA nm EQ minus_num",
 /* 219 */ "cmd ::= PRAGMA nm LP minus_num RP",
 /* 220 */ "cmd ::= PRAGMA nm EQ nm DOT nm",
 /* 221 */ "plus_num ::= PLUS INTEGER|FLOAT",
 /* 222 */ "minus_num ::= MINUS INTEGER|FLOAT",
 /* 223 */ "cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END",
 /* 224 */ "trigger_decl ::= TRIGGER ifnotexists nm trigger_time trigger_event ON fullname foreach_clause when_clause",
 /* 225 */ "trigger_time ::= BEFORE",
 /* 226 */ "trigger_time ::= AFTER",
 /* 227 */ "trigger_time ::= INSTEAD OF",
 /* 228 */ "trigger_time ::=",
 /* 229 */ "trigger_event ::= DELETE|INSERT",
 /* 230 */ "trigger_event ::= UPDATE",
 /* 231 */ "trigger_event ::= UPDATE OF idlist",
 /* 232 */ "when_clause ::=",
 /* 233 */ "when_clause ::= WHEN expr",
 /* 234 */ "trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI",
 /* 235 */ "trigger_cmd_list ::= trigger_cmd SEMI",
 /* 236 */ "trnm ::= nm DOT nm",
 /* 237 */ "tridxby ::= INDEXED BY nm",
 /* 238 */ "tridxby ::= NOT INDEXED",
 /* 239 */ "trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt",
 /* 240 */ "trigger_cmd ::= insert_cmd INTO trnm idlist_opt select",
 /* 241 */ "trigger_cmd ::= DELETE FROM trnm tridxby where_opt",
 /* 242 */ "trigger_cmd ::= select",
 /* 243 */ "expr ::= RAISE LP IGNORE RP",
 /* 244 */ "expr ::= RAISE LP raisetype COMMA nm RP",
 /* 245 */ "raisetype ::= ROLLBACK",
 /* 246 */ "raisetype ::= ABORT",
 /* 247 */ "raisetype ::= FAIL",
 /* 248 */ "cmd ::= DROP TRIGGER ifexists fullname",
 /* 249 */ "cmd ::= REINDEX",
 /* 250 */ "cmd ::= REINDEX nm",
 /* 251 */ "cmd ::= REINDEX nm ON nm",
 /* 252 */ "cmd ::= ANALYZE",
 /* 253 */ "cmd ::= ANALYZE nm",
 /* 254 */ "cmd ::= ALTER TABLE fullname RENAME TO nm",
 /* 255 */ "cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist",
 /* 256 */ "add_column_fullname ::= fullname",
 /* 257 */ "with ::=",
 /* 258 */ "with ::= WITH wqlist",
 /* 259 */ "with ::= WITH RECURSIVE wqlist",
 /* 260 */ "wqlist ::= nm eidlist_opt AS LP select RP",
 /* 261 */ "wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP",
 /* 262 */ "input ::= ecmd",
 /* 263 */ "explain ::=",
 /* 264 */ "cmdx ::= cmd",
 /* 265 */ "trans_opt ::=",
 /* 266 */ "trans_opt ::= TRANSACTION",
 /* 267 */ "trans_opt ::= TRANSACTION nm",
 /* 268 */ "savepoint_opt ::= SAVEPOINT",
 /* 269 */ "savepoint_opt ::=",
 /* 270 */ "cmd ::= create_table create_table_args",
 /* 271 */ "columnlist ::= columnlist COMMA columnname carglist",
 /* 272 */ "columnlist ::= columnname carglist",
 /* 273 */ "nm ::= JOIN_KW",
 /* 274 */ "typetoken ::= typename",
 /* 275 */ "typename ::= ID|STRING",
 /* 276 */ "signed ::= plus_num",
 /* 277 */ "signed ::= minus_num",
 /* 278 */ "carglist ::= carglist ccons",
 /* 279 */ "carglist ::=",
 /* 280 */ "ccons ::= NULL onconf",
 /* 281 */ "conslist_opt ::= COMMA conslist",
 /* 282 */ "conslist ::= conslist tconscomma tcons",
 /* 283 */ "conslist ::= tcons",
 /* 284 */ "tconscomma ::=",
 /* 285 */ "defer_subclause_opt ::= defer_subclause",
 /* 286 */ "resolvetype ::= raisetype",
 /* 287 */ "selectnowith ::= oneselect",
 /* 288 */ "oneselect ::= values",
 /* 289 */ "sclp ::= selcollist COMMA",
 /* 290 */ "as ::= ID|STRING",
 /* 291 */ "expr ::= term",
 /* 292 */ "exprlist ::= nexprlist",
 /* 293 */ "nmnum ::= plus_num",
 /* 294 */ "nmnum ::= nm",
 /* 295 */ "nmnum ::= ON",
 /* 296 */ "nmnum ::= DELETE",
 /* 297 */ "nmnum ::= DEFAULT",
 /* 298 */ "plus_num ::= INTEGER|FLOAT",
 /* 299 */ "foreach_clause ::=",
 /* 300 */ "foreach_clause ::= FOR EACH ROW",
 /* 301 */ "trnm ::= nm",
 /* 302 */ "tridxby ::=",
 /* 303 */ "kwcolumn_opt ::=",
 /* 304 */ "kwcolumn_opt ::= COLUMNKW",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.  Return the number
** of errors.  Return 0 on success.
*/
static int yyGrowStack(yyParser *p){
  int newSize;
  int idx;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  idx = p->yytos ? (int)(p->yytos - p->yystack) : 0;
  if( p->yystack==&p->yystk0 ){
    pNew = malloc(newSize*sizeof(pNew[0]));
    if( pNew ) pNew[0] = p->yystk0;
  }else{
    pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  }
  if( pNew ){
    p->yystack = pNew;
    p->yytos = &p->yystack[idx];
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows from %d to %d entries.\n",
              yyTracePrompt, p->yystksz, newSize);
    }
#endif
    p->yystksz = newSize;
  }
  return pNew==0; 
}
#endif

/* Datatype of the argument to the memory allocated passed as the
** second argument to sqlite3ParserAlloc() below.  This can be changed by
** putting an appropriate #define in the %include section of the input
** grammar.
*/
#ifndef YYMALLOCARGTYPE
# define YYMALLOCARGTYPE size_t
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to sqlite3Parser and sqlite3ParserFree.
*/
void *sqlite3ParserAlloc(void *(*mallocProc)(YYMALLOCARGTYPE)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (YYMALLOCARGTYPE)sizeof(yyParser) );
  if( pParser ){
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyhwm = 0;
    pParser->is_fallback_failed = false;
#endif
#if YYSTACKDEPTH<=0
    pParser->yytos = NULL;
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    if( yyGrowStack(pParser) ){
      pParser->yystack = &pParser->yystk0;
      pParser->yystksz = 1;
    }
#endif
#ifndef YYNOERRORRECOVERY
    pParser->yyerrcnt = -1;
#endif
    pParser->yytos = pParser->yystack;
    pParser->yystack[0].stateno = 0;
    pParser->yystack[0].major = 0;
  }
  return pParser;
}

/* The following function deletes the "minor type" or semantic value
** associated with a symbol.  The symbol can be either a terminal
** or nonterminal. "yymajor" is the symbol code, and "yypminor" is
** a pointer to the value to be deleted.  The code used to do the 
** deletions is derived from the %destructor and/or %token_destructor
** directives of the input grammar.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  sqlite3ParserARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are *not* used
    ** inside the C code.
    */
/********* Begin destructor definitions ***************************************/
    case 153: /* select */
    case 184: /* selectnowith */
    case 185: /* oneselect */
    case 196: /* values */
{
#line 403 "parse.y"
sqlite3SelectDelete(pParse->db, (yypminor->yy309));
#line 1496 "parse.c"
}
      break;
    case 162: /* term */
    case 163: /* expr */
{
#line 844 "parse.y"
sqlite3ExprDelete(pParse->db, (yypminor->yy200).pExpr);
#line 1504 "parse.c"
}
      break;
    case 167: /* eidlist_opt */
    case 176: /* sortlist */
    case 177: /* eidlist */
    case 189: /* selcollist */
    case 192: /* groupby_opt */
    case 194: /* orderby_opt */
    case 197: /* nexprlist */
    case 198: /* exprlist */
    case 199: /* sclp */
    case 208: /* setlist */
    case 214: /* paren_exprlist */
    case 216: /* case_exprlist */
{
#line 1285 "parse.y"
sqlite3ExprListDelete(pParse->db, (yypminor->yy272));
#line 1522 "parse.c"
}
      break;
    case 183: /* fullname */
    case 190: /* from */
    case 201: /* seltablist */
    case 202: /* stl_prefix */
{
#line 631 "parse.y"
sqlite3SrcListDelete(pParse->db, (yypminor->yy73));
#line 1532 "parse.c"
}
      break;
    case 186: /* with */
    case 232: /* wqlist */
{
#line 1529 "parse.y"
sqlite3WithDelete(pParse->db, (yypminor->yy445));
#line 1540 "parse.c"
}
      break;
    case 191: /* where_opt */
    case 193: /* having_opt */
    case 205: /* on_opt */
    case 215: /* case_operand */
    case 217: /* case_else */
    case 226: /* when_clause */
{
#line 753 "parse.y"
sqlite3ExprDelete(pParse->db, (yypminor->yy294));
#line 1552 "parse.c"
}
      break;
    case 206: /* using_opt */
    case 207: /* idlist */
    case 210: /* idlist_opt */
{
#line 665 "parse.y"
sqlite3IdListDelete(pParse->db, (yypminor->yy436));
#line 1561 "parse.c"
}
      break;
    case 222: /* trigger_cmd_list */
    case 227: /* trigger_cmd */
{
#line 1404 "parse.y"
sqlite3DeleteTriggerStep(pParse->db, (yypminor->yy251));
#line 1569 "parse.c"
}
      break;
    case 224: /* trigger_event */
{
#line 1390 "parse.y"
sqlite3IdListDelete(pParse->db, (yypminor->yy60).b);
#line 1576 "parse.c"
}
      break;
/********* End destructor definitions *****************************************/
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
*/
static void yy_pop_parser_stack(yyParser *pParser){
  yyStackEntry *yytos;
  assert( pParser->yytos!=0 );
  assert( pParser->yytos > pParser->yystack );
  yytos = pParser->yytos--;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yy_destructor(pParser, yytos->major, &yytos->minor);
}

/* 
** Deallocate and destroy a parser.  Destructors are called for
** all stack elements before shutting the parser down.
**
** If the YYPARSEFREENEVERNULL macro exists (for example because it
** is defined in a %include section of the input grammar) then it is
** assumed that the input pointer is never NULL.
*/
void sqlite3ParserFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
#ifndef YYPARSEFREENEVERNULL
  if( pParser==0 ) return;
#endif
  while( pParser->yytos>pParser->yystack ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  if( pParser->yystack!=&pParser->yystk0 ) free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int sqlite3ParserStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyhwm;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
*/
static unsigned int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yytos->stateno;
 
  if( stateno>=YY_MIN_REDUCE ) return stateno;
  assert( stateno <= YY_SHIFT_COUNT );
  do{
    i = yy_shift_ofst[stateno];
    assert( iLookAhead!=YYNOCODE );
    i += iLookAhead;
    if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback = -1;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        assert( yyFallback[iFallback]==0 ); /* Fallback loop must terminate */
        iLookAhead = iFallback;
        continue;
      } else if ( iFallback==0 ) {
        pParser->is_fallback_failed = true;
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( 
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD && iLookAhead>0
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead],
               yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
      return yy_default[stateno];
    }else{
      return yy_action[i];
    }
  }while(1);
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser){
   sqlite3ParserARG_FETCH;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
/******** Begin %stack_overflow code ******************************************/
#line 41 "parse.y"

  sqlite3ErrorMsg(pParse, "parser stack overflow");
#line 1751 "parse.c"
/******** End %stack_overflow code ********************************************/
   sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Print tracing information for a SHIFT action
*/
#ifndef NDEBUG
static void yyTraceShift(yyParser *yypParser, int yyNewState){
  if( yyTraceFILE ){
    if( yyNewState<YYNSTATE ){
      fprintf(yyTraceFILE,"%sShift '%s', go to state %d\n",
         yyTracePrompt,yyTokenName[yypParser->yytos->major],
         yyNewState);
    }else{
      fprintf(yyTraceFILE,"%sShift '%s'\n",
         yyTracePrompt,yyTokenName[yypParser->yytos->major]);
    }
  }
}
#else
# define yyTraceShift(X,Y)
#endif

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  sqlite3ParserTOKENTYPE yyMinor        /* The minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yytos++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
    yypParser->yyhwm++;
    assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack) );
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yytos>=&yypParser->yystack[YYSTACKDEPTH] ){
    yypParser->yytos--;
    yyStackOverflow(yypParser);
    return;
  }
#else
  if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz] ){
    if( yyGrowStack(yypParser) ){
      yypParser->yytos--;
      yyStackOverflow(yypParser);
      return;
    }
  }
#endif
  if( yyNewState > YY_MAX_SHIFT ){
    yyNewState += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
  }
  yytos = yypParser->yytos;
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor.yy0 = yyMinor;
  yyTraceShift(yypParser, yyNewState);
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 138, 3 },
  { 138, 1 },
  { 139, 1 },
  { 139, 3 },
  { 141, 3 },
  { 142, 0 },
  { 142, 1 },
  { 141, 2 },
  { 141, 2 },
  { 141, 2 },
  { 141, 2 },
  { 141, 3 },
  { 141, 5 },
  { 146, 4 },
  { 148, 1 },
  { 149, 0 },
  { 149, 3 },
  { 147, 5 },
  { 147, 2 },
  { 152, 0 },
  { 152, 2 },
  { 154, 2 },
  { 144, 1 },
  { 144, 1 },
  { 156, 0 },
  { 156, 4 },
  { 156, 6 },
  { 157, 2 },
  { 161, 2 },
  { 161, 2 },
  { 161, 4 },
  { 161, 3 },
  { 161, 3 },
  { 161, 2 },
  { 161, 3 },
  { 161, 5 },
  { 161, 2 },
  { 161, 4 },
  { 161, 4 },
  { 161, 1 },
  { 161, 2 },
  { 166, 0 },
  { 166, 1 },
  { 168, 0 },
  { 168, 2 },
  { 170, 2 },
  { 170, 3 },
  { 170, 3 },
  { 170, 3 },
  { 171, 2 },
  { 171, 2 },
  { 171, 1 },
  { 171, 1 },
  { 171, 2 },
  { 169, 3 },
  { 169, 2 },
  { 172, 0 },
  { 172, 2 },
  { 172, 2 },
  { 151, 0 },
  { 174, 1 },
  { 175, 2 },
  { 175, 7 },
  { 175, 5 },
  { 175, 5 },
  { 175, 10 },
  { 178, 0 },
  { 164, 0 },
  { 164, 3 },
  { 179, 0 },
  { 179, 2 },
  { 180, 1 },
  { 180, 1 },
  { 141, 4 },
  { 182, 2 },
  { 182, 0 },
  { 141, 7 },
  { 141, 4 },
  { 141, 1 },
  { 153, 2 },
  { 184, 3 },
  { 187, 1 },
  { 187, 2 },
  { 187, 1 },
  { 185, 9 },
  { 196, 4 },
  { 196, 5 },
  { 188, 1 },
  { 188, 1 },
  { 188, 0 },
  { 199, 0 },
  { 189, 3 },
  { 189, 2 },
  { 189, 4 },
  { 200, 2 },
  { 200, 0 },
  { 190, 0 },
  { 190, 2 },
  { 202, 2 },
  { 202, 0 },
  { 201, 6 },
  { 201, 8 },
  { 201, 7 },
  { 201, 7 },
  { 183, 1 },
  { 203, 1 },
  { 203, 2 },
  { 203, 3 },
  { 203, 4 },
  { 205, 2 },
  { 205, 0 },
  { 204, 0 },
  { 204, 3 },
  { 204, 2 },
  { 206, 4 },
  { 206, 0 },
  { 194, 0 },
  { 194, 3 },
  { 176, 4 },
  { 176, 2 },
  { 165, 1 },
  { 165, 1 },
  { 165, 0 },
  { 192, 0 },
  { 192, 3 },
  { 193, 0 },
  { 193, 2 },
  { 195, 0 },
  { 195, 2 },
  { 195, 4 },
  { 195, 4 },
  { 141, 6 },
  { 191, 0 },
  { 191, 2 },
  { 141, 8 },
  { 208, 5 },
  { 208, 7 },
  { 208, 3 },
  { 208, 5 },
  { 141, 6 },
  { 141, 7 },
  { 209, 2 },
  { 209, 1 },
  { 210, 0 },
  { 210, 3 },
  { 207, 3 },
  { 207, 1 },
  { 163, 3 },
  { 162, 1 },
  { 163, 1 },
  { 163, 1 },
  { 163, 3 },
  { 163, 5 },
  { 162, 1 },
  { 162, 1 },
  { 162, 1 },
  { 163, 1 },
  { 163, 3 },
  { 163, 6 },
  { 163, 5 },
  { 163, 4 },
  { 162, 1 },
  { 163, 5 },
  { 163, 3 },
  { 163, 3 },
  { 163, 3 },
  { 163, 3 },
  { 163, 3 },
  { 163, 3 },
  { 163, 3 },
  { 163, 3 },
  { 211, 1 },
  { 211, 2 },
  { 163, 3 },
  { 163, 5 },
  { 163, 2 },
  { 163, 3 },
  { 163, 3 },
  { 163, 4 },
  { 163, 2 },
  { 163, 2 },
  { 163, 2 },
  { 163, 2 },
  { 212, 1 },
  { 212, 2 },
  { 163, 5 },
  { 213, 1 },
  { 213, 2 },
  { 163, 5 },
  { 163, 3 },
  { 163, 5 },
  { 163, 4 },
  { 163, 4 },
  { 163, 5 },
  { 216, 5 },
  { 216, 4 },
  { 217, 2 },
  { 217, 0 },
  { 215, 1 },
  { 215, 0 },
  { 198, 0 },
  { 197, 3 },
  { 197, 1 },
  { 214, 0 },
  { 214, 3 },
  { 141, 11 },
  { 218, 1 },
  { 218, 0 },
  { 167, 0 },
  { 167, 3 },
  { 177, 5 },
  { 177, 3 },
  { 219, 0 },
  { 219, 2 },
  { 141, 6 },
  { 141, 2 },
  { 141, 4 },
  { 141, 5 },
  { 141, 4 },
  { 141, 5 },
  { 141, 6 },
  { 159, 2 },
  { 160, 2 },
  { 141, 5 },
  { 221, 9 },
  { 223, 1 },
  { 223, 1 },
  { 223, 2 },
  { 223, 0 },
  { 224, 1 },
  { 224, 1 },
  { 224, 3 },
  { 226, 0 },
  { 226, 2 },
  { 222, 3 },
  { 222, 2 },
  { 228, 3 },
  { 229, 3 },
  { 229, 2 },
  { 227, 7 },
  { 227, 5 },
  { 227, 5 },
  { 227, 1 },
  { 163, 4 },
  { 163, 6 },
  { 181, 1 },
  { 181, 1 },
  { 181, 1 },
  { 141, 4 },
  { 141, 1 },
  { 141, 2 },
  { 141, 4 },
  { 141, 1 },
  { 141, 2 },
  { 141, 6 },
  { 141, 7 },
  { 230, 1 },
  { 186, 0 },
  { 186, 2 },
  { 186, 3 },
  { 232, 6 },
  { 232, 8 },
  { 137, 1 },
  { 139, 0 },
  { 140, 1 },
  { 143, 0 },
  { 143, 1 },
  { 143, 2 },
  { 145, 1 },
  { 145, 0 },
  { 141, 2 },
  { 150, 4 },
  { 150, 2 },
  { 144, 1 },
  { 156, 1 },
  { 157, 1 },
  { 158, 1 },
  { 158, 1 },
  { 155, 2 },
  { 155, 0 },
  { 161, 2 },
  { 151, 2 },
  { 173, 3 },
  { 173, 1 },
  { 174, 0 },
  { 178, 1 },
  { 180, 1 },
  { 184, 1 },
  { 185, 1 },
  { 199, 2 },
  { 200, 1 },
  { 163, 1 },
  { 198, 1 },
  { 220, 1 },
  { 220, 1 },
  { 220, 1 },
  { 220, 1 },
  { 220, 1 },
  { 159, 1 },
  { 225, 0 },
  { 225, 3 },
  { 228, 1 },
  { 229, 0 },
  { 231, 0 },
  { 231, 1 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  unsigned int yyruleno        /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  sqlite3ParserARG_FETCH;
  yymsp = yypParser->yytos;
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    yysize = yyRuleInfo[yyruleno].nrhs;
    fprintf(yyTraceFILE, "%sReduce [%s], go to state %d.\n", yyTracePrompt,
      yyRuleName[yyruleno], yymsp[-yysize].stateno);
  }
#endif /* NDEBUG */

  /* Check that the stack is large enough to grow by a single entry
  ** if the RHS of the rule is empty.  This ensures that there is room
  ** enough on the stack to push the LHS value */
  if( yyRuleInfo[yyruleno].nrhs==0 ){
#ifdef YYTRACKMAXSTACKDEPTH
    if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
      yypParser->yyhwm++;
      assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack));
    }
#endif
#if YYSTACKDEPTH>0 
    if( yypParser->yytos>=&yypParser->yystack[YYSTACKDEPTH-1] ){
      yyStackOverflow(yypParser);
      return;
    }
#else
    if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz-1] ){
      if( yyGrowStack(yypParser) ){
        yyStackOverflow(yypParser);
        return;
      }
      yymsp = yypParser->yytos;
    }
#endif
  }

  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
/********** Begin reduce actions **********************************************/
        YYMINORTYPE yylhsminor;
      case 0: /* ecmd ::= explain cmdx SEMI */
#line 111 "parse.y"
{ sqlite3FinishCoding(pParse); }
#line 2196 "parse.c"
        break;
      case 1: /* ecmd ::= SEMI */
#line 112 "parse.y"
{
  sqlite3ErrorMsg(pParse, "syntax error: empty request");
}
#line 2203 "parse.c"
        break;
      case 2: /* explain ::= EXPLAIN */
#line 117 "parse.y"
{ pParse->explain = 1; }
#line 2208 "parse.c"
        break;
      case 3: /* explain ::= EXPLAIN QUERY PLAN */
#line 118 "parse.y"
{ pParse->explain = 2; }
#line 2213 "parse.c"
        break;
      case 4: /* cmd ::= BEGIN transtype trans_opt */
#line 150 "parse.y"
{sqlite3BeginTransaction(pParse, yymsp[-1].minor.yy322);}
#line 2218 "parse.c"
        break;
      case 5: /* transtype ::= */
#line 155 "parse.y"
{yymsp[1].minor.yy322 = TK_DEFERRED;}
#line 2223 "parse.c"
        break;
      case 6: /* transtype ::= DEFERRED */
#line 156 "parse.y"
{yymsp[0].minor.yy322 = yymsp[0].major; /*A-overwrites-X*/}
#line 2228 "parse.c"
        break;
      case 7: /* cmd ::= COMMIT trans_opt */
      case 8: /* cmd ::= END trans_opt */ yytestcase(yyruleno==8);
#line 157 "parse.y"
{sqlite3CommitTransaction(pParse);}
#line 2234 "parse.c"
        break;
      case 9: /* cmd ::= ROLLBACK trans_opt */
#line 159 "parse.y"
{sqlite3RollbackTransaction(pParse);}
#line 2239 "parse.c"
        break;
      case 10: /* cmd ::= SAVEPOINT nm */
#line 163 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_BEGIN, &yymsp[0].minor.yy0);
}
#line 2246 "parse.c"
        break;
      case 11: /* cmd ::= RELEASE savepoint_opt nm */
#line 166 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_RELEASE, &yymsp[0].minor.yy0);
}
#line 2253 "parse.c"
        break;
      case 12: /* cmd ::= ROLLBACK trans_opt TO savepoint_opt nm */
#line 169 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_ROLLBACK, &yymsp[0].minor.yy0);
}
#line 2260 "parse.c"
        break;
      case 13: /* create_table ::= createkw TABLE ifnotexists nm */
#line 176 "parse.y"
{
   sqlite3StartTable(pParse,&yymsp[0].minor.yy0,yymsp[-1].minor.yy322);
}
#line 2267 "parse.c"
        break;
      case 14: /* createkw ::= CREATE */
#line 179 "parse.y"
{disableLookaside(pParse);}
#line 2272 "parse.c"
        break;
      case 15: /* ifnotexists ::= */
      case 19: /* table_options ::= */ yytestcase(yyruleno==19);
      case 41: /* autoinc ::= */ yytestcase(yyruleno==41);
      case 56: /* init_deferred_pred_opt ::= */ yytestcase(yyruleno==56);
      case 66: /* defer_subclause_opt ::= */ yytestcase(yyruleno==66);
      case 75: /* ifexists ::= */ yytestcase(yyruleno==75);
      case 89: /* distinct ::= */ yytestcase(yyruleno==89);
      case 212: /* collate ::= */ yytestcase(yyruleno==212);
#line 182 "parse.y"
{yymsp[1].minor.yy322 = 0;}
#line 2284 "parse.c"
        break;
      case 16: /* ifnotexists ::= IF NOT EXISTS */
#line 183 "parse.y"
{yymsp[-2].minor.yy322 = 1;}
#line 2289 "parse.c"
        break;
      case 17: /* create_table_args ::= LP columnlist conslist_opt RP table_options */
#line 185 "parse.y"
{
  sqlite3EndTable(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,yymsp[0].minor.yy322,0);
}
#line 2296 "parse.c"
        break;
      case 18: /* create_table_args ::= AS select */
#line 188 "parse.y"
{
  sqlite3EndTable(pParse,0,0,0,yymsp[0].minor.yy309);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy309);
}
#line 2304 "parse.c"
        break;
      case 20: /* table_options ::= WITHOUT nm */
#line 194 "parse.y"
{
  if( yymsp[0].minor.yy0.n==5 && sqlite3_strnicmp(yymsp[0].minor.yy0.z,"rowid",5)==0 ){
    yymsp[-1].minor.yy322 = TF_WithoutRowid | TF_NoVisibleRowid;
  }else{
    yymsp[-1].minor.yy322 = 0;
    sqlite3ErrorMsg(pParse, "unknown table option: %.*s", yymsp[0].minor.yy0.n, yymsp[0].minor.yy0.z);
  }
}
#line 2316 "parse.c"
        break;
      case 21: /* columnname ::= nm typetoken */
#line 204 "parse.y"
{sqlite3AddColumn(pParse,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy0);}
#line 2321 "parse.c"
        break;
      case 22: /* nm ::= ID|INDEXED */
      case 23: /* nm ::= STRING */ yytestcase(yyruleno==23);
#line 236 "parse.y"
{
  if(yymsp[0].minor.yy0.isReserved) {
    sqlite3ErrorMsg(pParse, "keyword \"%T\" is reserved", &yymsp[0].minor.yy0);
  }
}
#line 2331 "parse.c"
        break;
      case 24: /* typetoken ::= */
      case 59: /* conslist_opt ::= */ yytestcase(yyruleno==59);
      case 95: /* as ::= */ yytestcase(yyruleno==95);
#line 253 "parse.y"
{yymsp[1].minor.yy0.n = 0; yymsp[1].minor.yy0.z = 0;}
#line 2338 "parse.c"
        break;
      case 25: /* typetoken ::= typename LP signed RP */
#line 255 "parse.y"
{
  yymsp[-3].minor.yy0.n = (int)(&yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-3].minor.yy0.z);
}
#line 2345 "parse.c"
        break;
      case 26: /* typetoken ::= typename LP signed COMMA signed RP */
#line 258 "parse.y"
{
  yymsp[-5].minor.yy0.n = (int)(&yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-5].minor.yy0.z);
}
#line 2352 "parse.c"
        break;
      case 27: /* typename ::= typename ID|STRING */
#line 263 "parse.y"
{yymsp[-1].minor.yy0.n=yymsp[0].minor.yy0.n+(int)(yymsp[0].minor.yy0.z-yymsp[-1].minor.yy0.z);}
#line 2357 "parse.c"
        break;
      case 28: /* ccons ::= CONSTRAINT nm */
      case 61: /* tcons ::= CONSTRAINT nm */ yytestcase(yyruleno==61);
#line 272 "parse.y"
{pParse->constraintName = yymsp[0].minor.yy0;}
#line 2363 "parse.c"
        break;
      case 29: /* ccons ::= DEFAULT term */
      case 31: /* ccons ::= DEFAULT PLUS term */ yytestcase(yyruleno==31);
#line 273 "parse.y"
{sqlite3AddDefaultValue(pParse,&yymsp[0].minor.yy200);}
#line 2369 "parse.c"
        break;
      case 30: /* ccons ::= DEFAULT LP expr RP */
#line 274 "parse.y"
{sqlite3AddDefaultValue(pParse,&yymsp[-1].minor.yy200);}
#line 2374 "parse.c"
        break;
      case 32: /* ccons ::= DEFAULT MINUS term */
#line 276 "parse.y"
{
  ExprSpan v;
  v.pExpr = sqlite3PExpr(pParse, TK_UMINUS, yymsp[0].minor.yy200.pExpr, 0);
  v.zStart = yymsp[-1].minor.yy0.z;
  v.zEnd = yymsp[0].minor.yy200.zEnd;
  sqlite3AddDefaultValue(pParse,&v);
}
#line 2385 "parse.c"
        break;
      case 33: /* ccons ::= DEFAULT ID|INDEXED */
#line 283 "parse.y"
{
  ExprSpan v;
  spanExpr(&v, pParse, TK_STRING, yymsp[0].minor.yy0);
  sqlite3AddDefaultValue(pParse,&v);
}
#line 2394 "parse.c"
        break;
      case 34: /* ccons ::= NOT NULL onconf */
#line 293 "parse.y"
{sqlite3AddNotNull(pParse, yymsp[0].minor.yy322);}
#line 2399 "parse.c"
        break;
      case 35: /* ccons ::= PRIMARY KEY sortorder onconf autoinc */
#line 295 "parse.y"
{sqlite3AddPrimaryKey(pParse,0,yymsp[-1].minor.yy322,yymsp[0].minor.yy322,yymsp[-2].minor.yy322);}
#line 2404 "parse.c"
        break;
      case 36: /* ccons ::= UNIQUE onconf */
#line 296 "parse.y"
{sqlite3CreateIndex(pParse,0,0,0,yymsp[0].minor.yy322,0,0,0,0,
                                   SQLITE_IDXTYPE_UNIQUE);}
#line 2410 "parse.c"
        break;
      case 37: /* ccons ::= CHECK LP expr RP */
#line 298 "parse.y"
{sqlite3AddCheckConstraint(pParse,yymsp[-1].minor.yy200.pExpr);}
#line 2415 "parse.c"
        break;
      case 38: /* ccons ::= REFERENCES nm eidlist_opt refargs */
#line 300 "parse.y"
{sqlite3CreateForeignKey(pParse,0,&yymsp[-2].minor.yy0,yymsp[-1].minor.yy272,yymsp[0].minor.yy322);}
#line 2420 "parse.c"
        break;
      case 39: /* ccons ::= defer_subclause */
#line 301 "parse.y"
{sqlite3DeferForeignKey(pParse,yymsp[0].minor.yy322);}
#line 2425 "parse.c"
        break;
      case 40: /* ccons ::= COLLATE ID|STRING */
#line 302 "parse.y"
{sqlite3AddCollateType(pParse, &yymsp[0].minor.yy0);}
#line 2430 "parse.c"
        break;
      case 42: /* autoinc ::= AUTOINCR */
#line 307 "parse.y"
{yymsp[0].minor.yy322 = 1;}
#line 2435 "parse.c"
        break;
      case 43: /* refargs ::= */
#line 315 "parse.y"
{ yymsp[1].minor.yy322 = OE_None*0x0101; /* EV: R-19803-45884 */}
#line 2440 "parse.c"
        break;
      case 44: /* refargs ::= refargs refarg */
#line 316 "parse.y"
{ yymsp[-1].minor.yy322 = (yymsp[-1].minor.yy322 & ~yymsp[0].minor.yy369.mask) | yymsp[0].minor.yy369.value; }
#line 2445 "parse.c"
        break;
      case 45: /* refarg ::= MATCH nm */
#line 318 "parse.y"
{ yymsp[-1].minor.yy369.value = 0;     yymsp[-1].minor.yy369.mask = 0x000000; }
#line 2450 "parse.c"
        break;
      case 46: /* refarg ::= ON INSERT refact */
#line 319 "parse.y"
{ yymsp[-2].minor.yy369.value = 0;     yymsp[-2].minor.yy369.mask = 0x000000; }
#line 2455 "parse.c"
        break;
      case 47: /* refarg ::= ON DELETE refact */
#line 320 "parse.y"
{ yymsp[-2].minor.yy369.value = yymsp[0].minor.yy322;     yymsp[-2].minor.yy369.mask = 0x0000ff; }
#line 2460 "parse.c"
        break;
      case 48: /* refarg ::= ON UPDATE refact */
#line 321 "parse.y"
{ yymsp[-2].minor.yy369.value = yymsp[0].minor.yy322<<8;  yymsp[-2].minor.yy369.mask = 0x00ff00; }
#line 2465 "parse.c"
        break;
      case 49: /* refact ::= SET NULL */
#line 323 "parse.y"
{ yymsp[-1].minor.yy322 = OE_SetNull;  /* EV: R-33326-45252 */}
#line 2470 "parse.c"
        break;
      case 50: /* refact ::= SET DEFAULT */
#line 324 "parse.y"
{ yymsp[-1].minor.yy322 = OE_SetDflt;  /* EV: R-33326-45252 */}
#line 2475 "parse.c"
        break;
      case 51: /* refact ::= CASCADE */
#line 325 "parse.y"
{ yymsp[0].minor.yy322 = OE_Cascade;  /* EV: R-33326-45252 */}
#line 2480 "parse.c"
        break;
      case 52: /* refact ::= RESTRICT */
#line 326 "parse.y"
{ yymsp[0].minor.yy322 = OE_Restrict; /* EV: R-33326-45252 */}
#line 2485 "parse.c"
        break;
      case 53: /* refact ::= NO ACTION */
#line 327 "parse.y"
{ yymsp[-1].minor.yy322 = OE_None;     /* EV: R-33326-45252 */}
#line 2490 "parse.c"
        break;
      case 54: /* defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt */
#line 329 "parse.y"
{yymsp[-2].minor.yy322 = 0;}
#line 2495 "parse.c"
        break;
      case 55: /* defer_subclause ::= DEFERRABLE init_deferred_pred_opt */
      case 70: /* orconf ::= OR resolvetype */ yytestcase(yyruleno==70);
      case 141: /* insert_cmd ::= INSERT orconf */ yytestcase(yyruleno==141);
#line 330 "parse.y"
{yymsp[-1].minor.yy322 = yymsp[0].minor.yy322;}
#line 2502 "parse.c"
        break;
      case 57: /* init_deferred_pred_opt ::= INITIALLY DEFERRED */
      case 74: /* ifexists ::= IF EXISTS */ yytestcase(yyruleno==74);
      case 184: /* between_op ::= NOT BETWEEN */ yytestcase(yyruleno==184);
      case 187: /* in_op ::= NOT IN */ yytestcase(yyruleno==187);
      case 213: /* collate ::= COLLATE ID|STRING */ yytestcase(yyruleno==213);
#line 333 "parse.y"
{yymsp[-1].minor.yy322 = 1;}
#line 2511 "parse.c"
        break;
      case 58: /* init_deferred_pred_opt ::= INITIALLY IMMEDIATE */
#line 334 "parse.y"
{yymsp[-1].minor.yy322 = 0;}
#line 2516 "parse.c"
        break;
      case 60: /* tconscomma ::= COMMA */
#line 340 "parse.y"
{pParse->constraintName.n = 0;}
#line 2521 "parse.c"
        break;
      case 62: /* tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf */
#line 344 "parse.y"
{sqlite3AddPrimaryKey(pParse,yymsp[-3].minor.yy272,yymsp[0].minor.yy322,yymsp[-2].minor.yy322,0);}
#line 2526 "parse.c"
        break;
      case 63: /* tcons ::= UNIQUE LP sortlist RP onconf */
#line 346 "parse.y"
{sqlite3CreateIndex(pParse,0,0,yymsp[-2].minor.yy272,yymsp[0].minor.yy322,0,0,0,0,
                                       SQLITE_IDXTYPE_UNIQUE);}
#line 2532 "parse.c"
        break;
      case 64: /* tcons ::= CHECK LP expr RP onconf */
#line 349 "parse.y"
{sqlite3AddCheckConstraint(pParse,yymsp[-2].minor.yy200.pExpr);}
#line 2537 "parse.c"
        break;
      case 65: /* tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt */
#line 351 "parse.y"
{
    sqlite3CreateForeignKey(pParse, yymsp[-6].minor.yy272, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy272, yymsp[-1].minor.yy322);
    sqlite3DeferForeignKey(pParse, yymsp[0].minor.yy322);
}
#line 2545 "parse.c"
        break;
      case 67: /* onconf ::= */
      case 69: /* orconf ::= */ yytestcase(yyruleno==69);
#line 365 "parse.y"
{yymsp[1].minor.yy322 = OE_Default;}
#line 2551 "parse.c"
        break;
      case 68: /* onconf ::= ON CONFLICT resolvetype */
#line 366 "parse.y"
{yymsp[-2].minor.yy322 = yymsp[0].minor.yy322;}
#line 2556 "parse.c"
        break;
      case 71: /* resolvetype ::= IGNORE */
#line 370 "parse.y"
{yymsp[0].minor.yy322 = OE_Ignore;}
#line 2561 "parse.c"
        break;
      case 72: /* resolvetype ::= REPLACE */
      case 142: /* insert_cmd ::= REPLACE */ yytestcase(yyruleno==142);
#line 371 "parse.y"
{yymsp[0].minor.yy322 = OE_Replace;}
#line 2567 "parse.c"
        break;
      case 73: /* cmd ::= DROP TABLE ifexists fullname */
#line 375 "parse.y"
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy73, 0, yymsp[-1].minor.yy322);
}
#line 2574 "parse.c"
        break;
      case 76: /* cmd ::= createkw VIEW ifnotexists nm eidlist_opt AS select */
#line 386 "parse.y"
{
  sqlite3CreateView(pParse, &yymsp[-6].minor.yy0, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy272, yymsp[0].minor.yy309, yymsp[-4].minor.yy322);
}
#line 2581 "parse.c"
        break;
      case 77: /* cmd ::= DROP VIEW ifexists fullname */
#line 389 "parse.y"
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy73, 1, yymsp[-1].minor.yy322);
}
#line 2588 "parse.c"
        break;
      case 78: /* cmd ::= select */
#line 396 "parse.y"
{
  SelectDest dest = {SRT_Output, 0, 0, 0, 0, 0};
  sqlite3Select(pParse, yymsp[0].minor.yy309, &dest);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy309);
}
#line 2597 "parse.c"
        break;
      case 79: /* select ::= with selectnowith */
#line 433 "parse.y"
{
  Select *p = yymsp[0].minor.yy309;
  if( p ){
    p->pWith = yymsp[-1].minor.yy445;
    parserDoubleLinkSelect(pParse, p);
  }else{
    sqlite3WithDelete(pParse->db, yymsp[-1].minor.yy445);
  }
  yymsp[-1].minor.yy309 = p; /*A-overwrites-W*/
}
#line 2611 "parse.c"
        break;
      case 80: /* selectnowith ::= selectnowith multiselect_op oneselect */
#line 446 "parse.y"
{
  Select *pRhs = yymsp[0].minor.yy309;
  Select *pLhs = yymsp[-2].minor.yy309;
  if( pRhs && pRhs->pPrior ){
    SrcList *pFrom;
    Token x;
    x.n = 0;
    parserDoubleLinkSelect(pParse, pRhs);
    pFrom = sqlite3SrcListAppendFromTerm(pParse,0,0,0,&x,pRhs,0,0);
    pRhs = sqlite3SelectNew(pParse,0,pFrom,0,0,0,0,0,0,0);
  }
  if( pRhs ){
    pRhs->op = (u8)yymsp[-1].minor.yy322;
    pRhs->pPrior = pLhs;
    if( ALWAYS(pLhs) ) pLhs->selFlags &= ~SF_MultiValue;
    pRhs->selFlags &= ~SF_MultiValue;
    if( yymsp[-1].minor.yy322!=TK_ALL ) pParse->hasCompound = 1;
  }else{
    sqlite3SelectDelete(pParse->db, pLhs);
  }
  yymsp[-2].minor.yy309 = pRhs;
}
#line 2637 "parse.c"
        break;
      case 81: /* multiselect_op ::= UNION */
      case 83: /* multiselect_op ::= EXCEPT|INTERSECT */ yytestcase(yyruleno==83);
#line 469 "parse.y"
{yymsp[0].minor.yy322 = yymsp[0].major; /*A-overwrites-OP*/}
#line 2643 "parse.c"
        break;
      case 82: /* multiselect_op ::= UNION ALL */
#line 470 "parse.y"
{yymsp[-1].minor.yy322 = TK_ALL;}
#line 2648 "parse.c"
        break;
      case 84: /* oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt */
#line 474 "parse.y"
{
#if SELECTTRACE_ENABLED
  Token s = yymsp[-8].minor.yy0; /*A-overwrites-S*/
#endif
  yymsp[-8].minor.yy309 = sqlite3SelectNew(pParse,yymsp[-6].minor.yy272,yymsp[-5].minor.yy73,yymsp[-4].minor.yy294,yymsp[-3].minor.yy272,yymsp[-2].minor.yy294,yymsp[-1].minor.yy272,yymsp[-7].minor.yy322,yymsp[0].minor.yy400.pLimit,yymsp[0].minor.yy400.pOffset);
#if SELECTTRACE_ENABLED
  /* Populate the Select.zSelName[] string that is used to help with
  ** query planner debugging, to differentiate between multiple Select
  ** objects in a complex query.
  **
  ** If the SELECT keyword is immediately followed by a C-style comment
  ** then extract the first few alphanumeric characters from within that
  ** comment to be the zSelName value.  Otherwise, the label is #N where
  ** is an integer that is incremented with each SELECT statement seen.
  */
  if( yymsp[-8].minor.yy309!=0 ){
    const char *z = s.z+6;
    int i;
    sqlite3_snprintf(sizeof(yymsp[-8].minor.yy309->zSelName), yymsp[-8].minor.yy309->zSelName, "#%d",
                     ++pParse->nSelect);
    while( z[0]==' ' ) z++;
    if( z[0]=='/' && z[1]=='*' ){
      z += 2;
      while( z[0]==' ' ) z++;
      for(i=0; sqlite3Isalnum(z[i]); i++){}
      sqlite3_snprintf(sizeof(yymsp[-8].minor.yy309->zSelName), yymsp[-8].minor.yy309->zSelName, "%.*s", i, z);
    }
  }
#endif /* SELECTRACE_ENABLED */
}
#line 2682 "parse.c"
        break;
      case 85: /* values ::= VALUES LP nexprlist RP */
#line 508 "parse.y"
{
  yymsp[-3].minor.yy309 = sqlite3SelectNew(pParse,yymsp[-1].minor.yy272,0,0,0,0,0,SF_Values,0,0);
}
#line 2689 "parse.c"
        break;
      case 86: /* values ::= values COMMA LP exprlist RP */
#line 511 "parse.y"
{
  Select *pRight, *pLeft = yymsp[-4].minor.yy309;
  pRight = sqlite3SelectNew(pParse,yymsp[-1].minor.yy272,0,0,0,0,0,SF_Values|SF_MultiValue,0,0);
  if( ALWAYS(pLeft) ) pLeft->selFlags &= ~SF_MultiValue;
  if( pRight ){
    pRight->op = TK_ALL;
    pRight->pPrior = pLeft;
    yymsp[-4].minor.yy309 = pRight;
  }else{
    yymsp[-4].minor.yy309 = pLeft;
  }
}
#line 2705 "parse.c"
        break;
      case 87: /* distinct ::= DISTINCT */
#line 528 "parse.y"
{yymsp[0].minor.yy322 = SF_Distinct;}
#line 2710 "parse.c"
        break;
      case 88: /* distinct ::= ALL */
#line 529 "parse.y"
{yymsp[0].minor.yy322 = SF_All;}
#line 2715 "parse.c"
        break;
      case 90: /* sclp ::= */
      case 116: /* orderby_opt ::= */ yytestcase(yyruleno==116);
      case 123: /* groupby_opt ::= */ yytestcase(yyruleno==123);
      case 200: /* exprlist ::= */ yytestcase(yyruleno==200);
      case 203: /* paren_exprlist ::= */ yytestcase(yyruleno==203);
      case 208: /* eidlist_opt ::= */ yytestcase(yyruleno==208);
#line 542 "parse.y"
{yymsp[1].minor.yy272 = 0;}
#line 2725 "parse.c"
        break;
      case 91: /* selcollist ::= sclp expr as */
#line 543 "parse.y"
{
   yymsp[-2].minor.yy272 = sqlite3ExprListAppend(pParse, yymsp[-2].minor.yy272, yymsp[-1].minor.yy200.pExpr);
   if( yymsp[0].minor.yy0.n>0 ) sqlite3ExprListSetName(pParse, yymsp[-2].minor.yy272, &yymsp[0].minor.yy0, 1);
   sqlite3ExprListSetSpan(pParse,yymsp[-2].minor.yy272,&yymsp[-1].minor.yy200);
}
#line 2734 "parse.c"
        break;
      case 92: /* selcollist ::= sclp STAR */
#line 548 "parse.y"
{
  Expr *p = sqlite3Expr(pParse->db, TK_ASTERISK, 0);
  yymsp[-1].minor.yy272 = sqlite3ExprListAppend(pParse, yymsp[-1].minor.yy272, p);
}
#line 2742 "parse.c"
        break;
      case 93: /* selcollist ::= sclp nm DOT STAR */
#line 552 "parse.y"
{
  Expr *pRight = sqlite3PExpr(pParse, TK_ASTERISK, 0, 0);
  Expr *pLeft = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *pDot = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight);
  yymsp[-3].minor.yy272 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy272, pDot);
}
#line 2752 "parse.c"
        break;
      case 94: /* as ::= AS nm */
      case 221: /* plus_num ::= PLUS INTEGER|FLOAT */ yytestcase(yyruleno==221);
      case 222: /* minus_num ::= MINUS INTEGER|FLOAT */ yytestcase(yyruleno==222);
#line 563 "parse.y"
{yymsp[-1].minor.yy0 = yymsp[0].minor.yy0;}
#line 2759 "parse.c"
        break;
      case 96: /* from ::= */
#line 577 "parse.y"
{yymsp[1].minor.yy73 = sqlite3DbMallocZero(pParse->db, sizeof(*yymsp[1].minor.yy73));}
#line 2764 "parse.c"
        break;
      case 97: /* from ::= FROM seltablist */
#line 578 "parse.y"
{
  yymsp[-1].minor.yy73 = yymsp[0].minor.yy73;
  sqlite3SrcListShiftJoinType(yymsp[-1].minor.yy73);
}
#line 2772 "parse.c"
        break;
      case 98: /* stl_prefix ::= seltablist joinop */
#line 586 "parse.y"
{
   if( ALWAYS(yymsp[-1].minor.yy73 && yymsp[-1].minor.yy73->nSrc>0) ) yymsp[-1].minor.yy73->a[yymsp[-1].minor.yy73->nSrc-1].fg.jointype = (u8)yymsp[0].minor.yy322;
}
#line 2779 "parse.c"
        break;
      case 99: /* stl_prefix ::= */
#line 589 "parse.y"
{yymsp[1].minor.yy73 = 0;}
#line 2784 "parse.c"
        break;
      case 100: /* seltablist ::= stl_prefix nm as indexed_opt on_opt using_opt */
#line 591 "parse.y"
{
  yymsp[-5].minor.yy73 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-5].minor.yy73,&yymsp[-4].minor.yy0,0,&yymsp[-3].minor.yy0,0,yymsp[-1].minor.yy294,yymsp[0].minor.yy436);
  sqlite3SrcListIndexedBy(pParse, yymsp[-5].minor.yy73, &yymsp[-2].minor.yy0);
}
#line 2792 "parse.c"
        break;
      case 101: /* seltablist ::= stl_prefix nm LP exprlist RP as on_opt using_opt */
#line 596 "parse.y"
{
  yymsp[-7].minor.yy73 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-7].minor.yy73,&yymsp[-6].minor.yy0,0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy294,yymsp[0].minor.yy436);
  sqlite3SrcListFuncArgs(pParse, yymsp[-7].minor.yy73, yymsp[-4].minor.yy272);
}
#line 2800 "parse.c"
        break;
      case 102: /* seltablist ::= stl_prefix LP select RP as on_opt using_opt */
#line 602 "parse.y"
{
    yymsp[-6].minor.yy73 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy73,0,0,&yymsp[-2].minor.yy0,yymsp[-4].minor.yy309,yymsp[-1].minor.yy294,yymsp[0].minor.yy436);
  }
#line 2807 "parse.c"
        break;
      case 103: /* seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt */
#line 606 "parse.y"
{
    if( yymsp[-6].minor.yy73==0 && yymsp[-2].minor.yy0.n==0 && yymsp[-1].minor.yy294==0 && yymsp[0].minor.yy436==0 ){
      yymsp[-6].minor.yy73 = yymsp[-4].minor.yy73;
    }else if( yymsp[-4].minor.yy73->nSrc==1 ){
      yymsp[-6].minor.yy73 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy73,0,0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy294,yymsp[0].minor.yy436);
      if( yymsp[-6].minor.yy73 ){
        struct SrcList_item *pNew = &yymsp[-6].minor.yy73->a[yymsp[-6].minor.yy73->nSrc-1];
        struct SrcList_item *pOld = yymsp[-4].minor.yy73->a;
        pNew->zName = pOld->zName;
        pNew->zDatabase = pOld->zDatabase;
        pNew->pSelect = pOld->pSelect;
        pOld->zName = pOld->zDatabase = 0;
        pOld->pSelect = 0;
      }
      sqlite3SrcListDelete(pParse->db, yymsp[-4].minor.yy73);
    }else{
      Select *pSubquery;
      sqlite3SrcListShiftJoinType(yymsp[-4].minor.yy73);
      pSubquery = sqlite3SelectNew(pParse,0,yymsp[-4].minor.yy73,0,0,0,0,SF_NestedFrom,0,0);
      yymsp[-6].minor.yy73 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy73,0,0,&yymsp[-2].minor.yy0,pSubquery,yymsp[-1].minor.yy294,yymsp[0].minor.yy436);
    }
  }
#line 2833 "parse.c"
        break;
      case 104: /* fullname ::= nm */
#line 633 "parse.y"
{yymsp[0].minor.yy73 = sqlite3SrcListAppend(pParse->db,0,&yymsp[0].minor.yy0,0); /*A-overwrites-X*/}
#line 2838 "parse.c"
        break;
      case 105: /* joinop ::= COMMA|JOIN */
#line 636 "parse.y"
{ yymsp[0].minor.yy322 = JT_INNER; }
#line 2843 "parse.c"
        break;
      case 106: /* joinop ::= JOIN_KW JOIN */
#line 638 "parse.y"
{yymsp[-1].minor.yy322 = sqlite3JoinType(pParse,&yymsp[-1].minor.yy0,0,0);  /*X-overwrites-A*/}
#line 2848 "parse.c"
        break;
      case 107: /* joinop ::= JOIN_KW nm JOIN */
#line 640 "parse.y"
{yymsp[-2].minor.yy322 = sqlite3JoinType(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,0); /*X-overwrites-A*/}
#line 2853 "parse.c"
        break;
      case 108: /* joinop ::= JOIN_KW nm nm JOIN */
#line 642 "parse.y"
{yymsp[-3].minor.yy322 = sqlite3JoinType(pParse,&yymsp[-3].minor.yy0,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0);/*X-overwrites-A*/}
#line 2858 "parse.c"
        break;
      case 109: /* on_opt ::= ON expr */
      case 126: /* having_opt ::= HAVING expr */ yytestcase(yyruleno==126);
      case 133: /* where_opt ::= WHERE expr */ yytestcase(yyruleno==133);
      case 196: /* case_else ::= ELSE expr */ yytestcase(yyruleno==196);
#line 646 "parse.y"
{yymsp[-1].minor.yy294 = yymsp[0].minor.yy200.pExpr;}
#line 2866 "parse.c"
        break;
      case 110: /* on_opt ::= */
      case 125: /* having_opt ::= */ yytestcase(yyruleno==125);
      case 132: /* where_opt ::= */ yytestcase(yyruleno==132);
      case 197: /* case_else ::= */ yytestcase(yyruleno==197);
      case 199: /* case_operand ::= */ yytestcase(yyruleno==199);
#line 647 "parse.y"
{yymsp[1].minor.yy294 = 0;}
#line 2875 "parse.c"
        break;
      case 111: /* indexed_opt ::= */
#line 660 "parse.y"
{yymsp[1].minor.yy0.z=0; yymsp[1].minor.yy0.n=0;}
#line 2880 "parse.c"
        break;
      case 112: /* indexed_opt ::= INDEXED BY nm */
#line 661 "parse.y"
{yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;}
#line 2885 "parse.c"
        break;
      case 113: /* indexed_opt ::= NOT INDEXED */
#line 662 "parse.y"
{yymsp[-1].minor.yy0.z=0; yymsp[-1].minor.yy0.n=1;}
#line 2890 "parse.c"
        break;
      case 114: /* using_opt ::= USING LP idlist RP */
#line 666 "parse.y"
{yymsp[-3].minor.yy436 = yymsp[-1].minor.yy436;}
#line 2895 "parse.c"
        break;
      case 115: /* using_opt ::= */
      case 143: /* idlist_opt ::= */ yytestcase(yyruleno==143);
#line 667 "parse.y"
{yymsp[1].minor.yy436 = 0;}
#line 2901 "parse.c"
        break;
      case 117: /* orderby_opt ::= ORDER BY sortlist */
      case 124: /* groupby_opt ::= GROUP BY nexprlist */ yytestcase(yyruleno==124);
#line 681 "parse.y"
{yymsp[-2].minor.yy272 = yymsp[0].minor.yy272;}
#line 2907 "parse.c"
        break;
      case 118: /* sortlist ::= sortlist COMMA expr sortorder */
#line 682 "parse.y"
{
  yymsp[-3].minor.yy272 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy272,yymsp[-1].minor.yy200.pExpr);
  sqlite3ExprListSetSortOrder(yymsp[-3].minor.yy272,yymsp[0].minor.yy322);
}
#line 2915 "parse.c"
        break;
      case 119: /* sortlist ::= expr sortorder */
#line 686 "parse.y"
{
  yymsp[-1].minor.yy272 = sqlite3ExprListAppend(pParse,0,yymsp[-1].minor.yy200.pExpr); /*A-overwrites-Y*/
  sqlite3ExprListSetSortOrder(yymsp[-1].minor.yy272,yymsp[0].minor.yy322);
}
#line 2923 "parse.c"
        break;
      case 120: /* sortorder ::= ASC */
#line 693 "parse.y"
{yymsp[0].minor.yy322 = SQLITE_SO_ASC;}
#line 2928 "parse.c"
        break;
      case 121: /* sortorder ::= DESC */
#line 694 "parse.y"
{yymsp[0].minor.yy322 = SQLITE_SO_DESC;}
#line 2933 "parse.c"
        break;
      case 122: /* sortorder ::= */
#line 695 "parse.y"
{yymsp[1].minor.yy322 = SQLITE_SO_UNDEFINED;}
#line 2938 "parse.c"
        break;
      case 127: /* limit_opt ::= */
#line 720 "parse.y"
{yymsp[1].minor.yy400.pLimit = 0; yymsp[1].minor.yy400.pOffset = 0;}
#line 2943 "parse.c"
        break;
      case 128: /* limit_opt ::= LIMIT expr */
#line 721 "parse.y"
{yymsp[-1].minor.yy400.pLimit = yymsp[0].minor.yy200.pExpr; yymsp[-1].minor.yy400.pOffset = 0;}
#line 2948 "parse.c"
        break;
      case 129: /* limit_opt ::= LIMIT expr OFFSET expr */
#line 723 "parse.y"
{yymsp[-3].minor.yy400.pLimit = yymsp[-2].minor.yy200.pExpr; yymsp[-3].minor.yy400.pOffset = yymsp[0].minor.yy200.pExpr;}
#line 2953 "parse.c"
        break;
      case 130: /* limit_opt ::= LIMIT expr COMMA expr */
#line 725 "parse.y"
{yymsp[-3].minor.yy400.pOffset = yymsp[-2].minor.yy200.pExpr; yymsp[-3].minor.yy400.pLimit = yymsp[0].minor.yy200.pExpr;}
#line 2958 "parse.c"
        break;
      case 131: /* cmd ::= with DELETE FROM fullname indexed_opt where_opt */
#line 742 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-5].minor.yy445, 1);
  sqlite3SrcListIndexedBy(pParse, yymsp[-2].minor.yy73, &yymsp[-1].minor.yy0);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3DeleteFrom(pParse,yymsp[-2].minor.yy73,yymsp[0].minor.yy294);
}
#line 2970 "parse.c"
        break;
      case 134: /* cmd ::= with UPDATE orconf fullname indexed_opt SET setlist where_opt */
#line 775 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-7].minor.yy445, 1);
  sqlite3SrcListIndexedBy(pParse, yymsp[-4].minor.yy73, &yymsp[-3].minor.yy0);
  sqlite3ExprListCheckLength(pParse,yymsp[-1].minor.yy272,"set list"); 
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Update(pParse,yymsp[-4].minor.yy73,yymsp[-1].minor.yy272,yymsp[0].minor.yy294,yymsp[-5].minor.yy322);
}
#line 2983 "parse.c"
        break;
      case 135: /* setlist ::= setlist COMMA nm EQ expr */
#line 789 "parse.y"
{
  yymsp[-4].minor.yy272 = sqlite3ExprListAppend(pParse, yymsp[-4].minor.yy272, yymsp[0].minor.yy200.pExpr);
  sqlite3ExprListSetName(pParse, yymsp[-4].minor.yy272, &yymsp[-2].minor.yy0, 1);
}
#line 2991 "parse.c"
        break;
      case 136: /* setlist ::= setlist COMMA LP idlist RP EQ expr */
#line 793 "parse.y"
{
  yymsp[-6].minor.yy272 = sqlite3ExprListAppendVector(pParse, yymsp[-6].minor.yy272, yymsp[-3].minor.yy436, yymsp[0].minor.yy200.pExpr);
}
#line 2998 "parse.c"
        break;
      case 137: /* setlist ::= nm EQ expr */
#line 796 "parse.y"
{
  yylhsminor.yy272 = sqlite3ExprListAppend(pParse, 0, yymsp[0].minor.yy200.pExpr);
  sqlite3ExprListSetName(pParse, yylhsminor.yy272, &yymsp[-2].minor.yy0, 1);
}
#line 3006 "parse.c"
  yymsp[-2].minor.yy272 = yylhsminor.yy272;
        break;
      case 138: /* setlist ::= LP idlist RP EQ expr */
#line 800 "parse.y"
{
  yymsp[-4].minor.yy272 = sqlite3ExprListAppendVector(pParse, 0, yymsp[-3].minor.yy436, yymsp[0].minor.yy200.pExpr);
}
#line 3014 "parse.c"
        break;
      case 139: /* cmd ::= with insert_cmd INTO fullname idlist_opt select */
#line 806 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-5].minor.yy445, 1);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Insert(pParse, yymsp[-2].minor.yy73, yymsp[0].minor.yy309, yymsp[-1].minor.yy436, yymsp[-4].minor.yy322);
}
#line 3025 "parse.c"
        break;
      case 140: /* cmd ::= with insert_cmd INTO fullname idlist_opt DEFAULT VALUES */
#line 814 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-6].minor.yy445, 1);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Insert(pParse, yymsp[-3].minor.yy73, 0, yymsp[-2].minor.yy436, yymsp[-5].minor.yy322);
}
#line 3036 "parse.c"
        break;
      case 144: /* idlist_opt ::= LP idlist RP */
#line 832 "parse.y"
{yymsp[-2].minor.yy436 = yymsp[-1].minor.yy436;}
#line 3041 "parse.c"
        break;
      case 145: /* idlist ::= idlist COMMA nm */
#line 834 "parse.y"
{yymsp[-2].minor.yy436 = sqlite3IdListAppend(pParse->db,yymsp[-2].minor.yy436,&yymsp[0].minor.yy0);}
#line 3046 "parse.c"
        break;
      case 146: /* idlist ::= nm */
#line 836 "parse.y"
{yymsp[0].minor.yy436 = sqlite3IdListAppend(pParse->db,0,&yymsp[0].minor.yy0); /*A-overwrites-Y*/}
#line 3051 "parse.c"
        break;
      case 147: /* expr ::= LP expr RP */
#line 886 "parse.y"
{spanSet(&yymsp[-2].minor.yy200,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/  yymsp[-2].minor.yy200.pExpr = yymsp[-1].minor.yy200.pExpr;}
#line 3056 "parse.c"
        break;
      case 148: /* term ::= NULL */
      case 153: /* term ::= FLOAT|BLOB */ yytestcase(yyruleno==153);
      case 154: /* term ::= STRING */ yytestcase(yyruleno==154);
#line 887 "parse.y"
{spanExpr(&yymsp[0].minor.yy200,pParse,yymsp[0].major,yymsp[0].minor.yy0);/*A-overwrites-X*/}
#line 3063 "parse.c"
        break;
      case 149: /* expr ::= ID|INDEXED */
      case 150: /* expr ::= JOIN_KW */ yytestcase(yyruleno==150);
#line 888 "parse.y"
{spanExpr(&yymsp[0].minor.yy200,pParse,TK_ID,yymsp[0].minor.yy0); /*A-overwrites-X*/}
#line 3069 "parse.c"
        break;
      case 151: /* expr ::= nm DOT nm */
#line 890 "parse.y"
{
  Expr *temp1 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *temp2 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[0].minor.yy0, 1);
  spanSet(&yymsp[-2].minor.yy200,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-2].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_DOT, temp1, temp2);
}
#line 3079 "parse.c"
        break;
      case 152: /* expr ::= nm DOT nm DOT nm */
#line 896 "parse.y"
{
  Expr *temp1 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-4].minor.yy0, 1);
  Expr *temp2 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *temp3 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[0].minor.yy0, 1);
  Expr *temp4 = sqlite3PExpr(pParse, TK_DOT, temp2, temp3);
  spanSet(&yymsp[-4].minor.yy200,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-4].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_DOT, temp1, temp4);
}
#line 3091 "parse.c"
        break;
      case 155: /* term ::= INTEGER */
#line 906 "parse.y"
{
  yylhsminor.yy200.pExpr = sqlite3ExprAlloc(pParse->db, TK_INTEGER, &yymsp[0].minor.yy0, 1);
  yylhsminor.yy200.zStart = yymsp[0].minor.yy0.z;
  yylhsminor.yy200.zEnd = yymsp[0].minor.yy0.z + yymsp[0].minor.yy0.n;
  if( yylhsminor.yy200.pExpr ) yylhsminor.yy200.pExpr->flags |= EP_Leaf;
}
#line 3101 "parse.c"
  yymsp[0].minor.yy200 = yylhsminor.yy200;
        break;
      case 156: /* expr ::= VARIABLE */
#line 912 "parse.y"
{
  if( !(yymsp[0].minor.yy0.z[0]=='#' && sqlite3Isdigit(yymsp[0].minor.yy0.z[1])) ){
    u32 n = yymsp[0].minor.yy0.n;
    spanExpr(&yymsp[0].minor.yy200, pParse, TK_VARIABLE, yymsp[0].minor.yy0);
    sqlite3ExprAssignVarNumber(pParse, yymsp[0].minor.yy200.pExpr, n);
  }else{
    /* When doing a nested parse, one can include terms in an expression
    ** that look like this:   #1 #2 ...  These terms refer to registers
    ** in the virtual machine.  #N is the N-th register. */
    Token t = yymsp[0].minor.yy0; /*A-overwrites-X*/
    assert( t.n>=2 );
    spanSet(&yymsp[0].minor.yy200, &t, &t);
    if( pParse->nested==0 ){
      sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &t);
      yymsp[0].minor.yy200.pExpr = 0;
    }else{
      yymsp[0].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_REGISTER, 0, 0);
      if( yymsp[0].minor.yy200.pExpr ) sqlite3GetInt32(&t.z[1], &yymsp[0].minor.yy200.pExpr->iTable);
    }
  }
}
#line 3127 "parse.c"
        break;
      case 157: /* expr ::= expr COLLATE ID|STRING */
#line 933 "parse.y"
{
  yymsp[-2].minor.yy200.pExpr = sqlite3ExprAddCollateToken(pParse, yymsp[-2].minor.yy200.pExpr, &yymsp[0].minor.yy0, 1);
  yymsp[-2].minor.yy200.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
}
#line 3135 "parse.c"
        break;
      case 158: /* expr ::= CAST LP expr AS typetoken RP */
#line 938 "parse.y"
{
  spanSet(&yymsp[-5].minor.yy200,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-5].minor.yy200.pExpr = sqlite3ExprAlloc(pParse->db, TK_CAST, &yymsp[-1].minor.yy0, 1);
  sqlite3ExprAttachSubtrees(pParse->db, yymsp[-5].minor.yy200.pExpr, yymsp[-3].minor.yy200.pExpr, 0);
}
#line 3144 "parse.c"
        break;
      case 159: /* expr ::= ID|INDEXED LP distinct exprlist RP */
#line 944 "parse.y"
{
  if( yymsp[-1].minor.yy272 && yymsp[-1].minor.yy272->nExpr>pParse->db->aLimit[SQLITE_LIMIT_FUNCTION_ARG] ){
    sqlite3ErrorMsg(pParse, "too many arguments on function %T", &yymsp[-4].minor.yy0);
  }
  yylhsminor.yy200.pExpr = sqlite3ExprFunction(pParse, yymsp[-1].minor.yy272, &yymsp[-4].minor.yy0);
  spanSet(&yylhsminor.yy200,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);
  if( yymsp[-2].minor.yy322==SF_Distinct && yylhsminor.yy200.pExpr ){
    yylhsminor.yy200.pExpr->flags |= EP_Distinct;
  }
}
#line 3158 "parse.c"
  yymsp[-4].minor.yy200 = yylhsminor.yy200;
        break;
      case 160: /* expr ::= ID|INDEXED LP STAR RP */
#line 954 "parse.y"
{
  yylhsminor.yy200.pExpr = sqlite3ExprFunction(pParse, 0, &yymsp[-3].minor.yy0);
  spanSet(&yylhsminor.yy200,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);
}
#line 3167 "parse.c"
  yymsp[-3].minor.yy200 = yylhsminor.yy200;
        break;
      case 161: /* term ::= CTIME_KW */
#line 958 "parse.y"
{
  yylhsminor.yy200.pExpr = sqlite3ExprFunction(pParse, 0, &yymsp[0].minor.yy0);
  spanSet(&yylhsminor.yy200, &yymsp[0].minor.yy0, &yymsp[0].minor.yy0);
}
#line 3176 "parse.c"
  yymsp[0].minor.yy200 = yylhsminor.yy200;
        break;
      case 162: /* expr ::= LP nexprlist COMMA expr RP */
#line 987 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(pParse, yymsp[-3].minor.yy272, yymsp[-1].minor.yy200.pExpr);
  yylhsminor.yy200.pExpr = sqlite3PExpr(pParse, TK_VECTOR, 0, 0);
  if( yylhsminor.yy200.pExpr ){
    yylhsminor.yy200.pExpr->x.pList = pList;
    spanSet(&yylhsminor.yy200, &yymsp[-4].minor.yy0, &yymsp[0].minor.yy0);
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  }
}
#line 3191 "parse.c"
  yymsp[-4].minor.yy200 = yylhsminor.yy200;
        break;
      case 163: /* expr ::= expr AND expr */
      case 164: /* expr ::= expr OR expr */ yytestcase(yyruleno==164);
      case 165: /* expr ::= expr LT|GT|GE|LE expr */ yytestcase(yyruleno==165);
      case 166: /* expr ::= expr EQ|NE expr */ yytestcase(yyruleno==166);
      case 167: /* expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr */ yytestcase(yyruleno==167);
      case 168: /* expr ::= expr PLUS|MINUS expr */ yytestcase(yyruleno==168);
      case 169: /* expr ::= expr STAR|SLASH|REM expr */ yytestcase(yyruleno==169);
      case 170: /* expr ::= expr CONCAT expr */ yytestcase(yyruleno==170);
#line 998 "parse.y"
{spanBinaryExpr(pParse,yymsp[-1].major,&yymsp[-2].minor.yy200,&yymsp[0].minor.yy200);}
#line 3204 "parse.c"
        break;
      case 171: /* likeop ::= LIKE_KW|MATCH */
#line 1011 "parse.y"
{yymsp[0].minor.yy0=yymsp[0].minor.yy0;/*A-overwrites-X*/}
#line 3209 "parse.c"
        break;
      case 172: /* likeop ::= NOT LIKE_KW|MATCH */
#line 1012 "parse.y"
{yymsp[-1].minor.yy0=yymsp[0].minor.yy0; yymsp[-1].minor.yy0.n|=0x80000000; /*yymsp[-1].minor.yy0-overwrite-yymsp[0].minor.yy0*/}
#line 3214 "parse.c"
        break;
      case 173: /* expr ::= expr likeop expr */
#line 1013 "parse.y"
{
  ExprList *pList;
  int bNot = yymsp[-1].minor.yy0.n & 0x80000000;
  yymsp[-1].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[0].minor.yy200.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-2].minor.yy200.pExpr);
  yymsp[-2].minor.yy200.pExpr = sqlite3ExprFunction(pParse, pList, &yymsp[-1].minor.yy0);
  exprNot(pParse, bNot, &yymsp[-2].minor.yy200);
  yymsp[-2].minor.yy200.zEnd = yymsp[0].minor.yy200.zEnd;
  if( yymsp[-2].minor.yy200.pExpr ) yymsp[-2].minor.yy200.pExpr->flags |= EP_InfixFunc;
}
#line 3229 "parse.c"
        break;
      case 174: /* expr ::= expr likeop expr ESCAPE expr */
#line 1024 "parse.y"
{
  ExprList *pList;
  int bNot = yymsp[-3].minor.yy0.n & 0x80000000;
  yymsp[-3].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy200.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-4].minor.yy200.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy200.pExpr);
  yymsp[-4].minor.yy200.pExpr = sqlite3ExprFunction(pParse, pList, &yymsp[-3].minor.yy0);
  exprNot(pParse, bNot, &yymsp[-4].minor.yy200);
  yymsp[-4].minor.yy200.zEnd = yymsp[0].minor.yy200.zEnd;
  if( yymsp[-4].minor.yy200.pExpr ) yymsp[-4].minor.yy200.pExpr->flags |= EP_InfixFunc;
}
#line 3245 "parse.c"
        break;
      case 175: /* expr ::= expr ISNULL|NOTNULL */
#line 1051 "parse.y"
{spanUnaryPostfix(pParse,yymsp[0].major,&yymsp[-1].minor.yy200,&yymsp[0].minor.yy0);}
#line 3250 "parse.c"
        break;
      case 176: /* expr ::= expr NOT NULL */
#line 1052 "parse.y"
{spanUnaryPostfix(pParse,TK_NOTNULL,&yymsp[-2].minor.yy200,&yymsp[0].minor.yy0);}
#line 3255 "parse.c"
        break;
      case 177: /* expr ::= expr IS expr */
#line 1073 "parse.y"
{
  spanBinaryExpr(pParse,TK_IS,&yymsp[-2].minor.yy200,&yymsp[0].minor.yy200);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy200.pExpr, yymsp[-2].minor.yy200.pExpr, TK_ISNULL);
}
#line 3263 "parse.c"
        break;
      case 178: /* expr ::= expr IS NOT expr */
#line 1077 "parse.y"
{
  spanBinaryExpr(pParse,TK_ISNOT,&yymsp[-3].minor.yy200,&yymsp[0].minor.yy200);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy200.pExpr, yymsp[-3].minor.yy200.pExpr, TK_NOTNULL);
}
#line 3271 "parse.c"
        break;
      case 179: /* expr ::= NOT expr */
      case 180: /* expr ::= BITNOT expr */ yytestcase(yyruleno==180);
#line 1101 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy200,pParse,yymsp[-1].major,&yymsp[0].minor.yy200,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3277 "parse.c"
        break;
      case 181: /* expr ::= MINUS expr */
#line 1105 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy200,pParse,TK_UMINUS,&yymsp[0].minor.yy200,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3282 "parse.c"
        break;
      case 182: /* expr ::= PLUS expr */
#line 1107 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy200,pParse,TK_UPLUS,&yymsp[0].minor.yy200,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3287 "parse.c"
        break;
      case 183: /* between_op ::= BETWEEN */
      case 186: /* in_op ::= IN */ yytestcase(yyruleno==186);
#line 1110 "parse.y"
{yymsp[0].minor.yy322 = 0;}
#line 3293 "parse.c"
        break;
      case 185: /* expr ::= expr between_op expr AND expr */
#line 1112 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy200.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy200.pExpr);
  yymsp[-4].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_BETWEEN, yymsp[-4].minor.yy200.pExpr, 0);
  if( yymsp[-4].minor.yy200.pExpr ){
    yymsp[-4].minor.yy200.pExpr->x.pList = pList;
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  } 
  exprNot(pParse, yymsp[-3].minor.yy322, &yymsp[-4].minor.yy200);
  yymsp[-4].minor.yy200.zEnd = yymsp[0].minor.yy200.zEnd;
}
#line 3309 "parse.c"
        break;
      case 188: /* expr ::= expr in_op LP exprlist RP */
#line 1128 "parse.y"
{
    if( yymsp[-1].minor.yy272==0 ){
      /* Expressions of the form
      **
      **      expr1 IN ()
      **      expr1 NOT IN ()
      **
      ** simplify to constants 0 (false) and 1 (true), respectively,
      ** regardless of the value of expr1.
      */
      sqlite3ExprDelete(pParse->db, yymsp[-4].minor.yy200.pExpr);
      yymsp[-4].minor.yy200.pExpr = sqlite3ExprAlloc(pParse->db, TK_INTEGER,&sqlite3IntTokens[yymsp[-3].minor.yy322],1);
    }else if( yymsp[-1].minor.yy272->nExpr==1 ){
      /* Expressions of the form:
      **
      **      expr1 IN (?1)
      **      expr1 NOT IN (?2)
      **
      ** with exactly one value on the RHS can be simplified to something
      ** like this:
      **
      **      expr1 == ?1
      **      expr1 <> ?2
      **
      ** But, the RHS of the == or <> is marked with the EP_Generic flag
      ** so that it may not contribute to the computation of comparison
      ** affinity or the collating sequence to use for comparison.  Otherwise,
      ** the semantics would be subtly different from IN or NOT IN.
      */
      Expr *pRHS = yymsp[-1].minor.yy272->a[0].pExpr;
      yymsp[-1].minor.yy272->a[0].pExpr = 0;
      sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy272);
      /* pRHS cannot be NULL because a malloc error would have been detected
      ** before now and control would have never reached this point */
      if( ALWAYS(pRHS) ){
        pRHS->flags &= ~EP_Collate;
        pRHS->flags |= EP_Generic;
      }
      yymsp[-4].minor.yy200.pExpr = sqlite3PExpr(pParse, yymsp[-3].minor.yy322 ? TK_NE : TK_EQ, yymsp[-4].minor.yy200.pExpr, pRHS);
    }else{
      yymsp[-4].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy200.pExpr, 0);
      if( yymsp[-4].minor.yy200.pExpr ){
        yymsp[-4].minor.yy200.pExpr->x.pList = yymsp[-1].minor.yy272;
        sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy200.pExpr);
      }else{
        sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy272);
      }
      exprNot(pParse, yymsp[-3].minor.yy322, &yymsp[-4].minor.yy200);
    }
    yymsp[-4].minor.yy200.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
  }
#line 3364 "parse.c"
        break;
      case 189: /* expr ::= LP select RP */
#line 1179 "parse.y"
{
    spanSet(&yymsp[-2].minor.yy200,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/
    yymsp[-2].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_SELECT, 0, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-2].minor.yy200.pExpr, yymsp[-1].minor.yy309);
  }
#line 3373 "parse.c"
        break;
      case 190: /* expr ::= expr in_op LP select RP */
#line 1184 "parse.y"
{
    yymsp[-4].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy200.pExpr, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-4].minor.yy200.pExpr, yymsp[-1].minor.yy309);
    exprNot(pParse, yymsp[-3].minor.yy322, &yymsp[-4].minor.yy200);
    yymsp[-4].minor.yy200.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
  }
#line 3383 "parse.c"
        break;
      case 191: /* expr ::= expr in_op nm paren_exprlist */
#line 1190 "parse.y"
{
    SrcList *pSrc = sqlite3SrcListAppend(pParse->db, 0,&yymsp[-1].minor.yy0,0);
    Select *pSelect = sqlite3SelectNew(pParse, 0,pSrc,0,0,0,0,0,0,0);
    if( yymsp[0].minor.yy272 )  sqlite3SrcListFuncArgs(pParse, pSelect ? pSrc : 0, yymsp[0].minor.yy272);
    yymsp[-3].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-3].minor.yy200.pExpr, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-3].minor.yy200.pExpr, pSelect);
    exprNot(pParse, yymsp[-2].minor.yy322, &yymsp[-3].minor.yy200);
    yymsp[-3].minor.yy200.zEnd = &yymsp[-1].minor.yy0.z[yymsp[-1].minor.yy0.n];
  }
#line 3396 "parse.c"
        break;
      case 192: /* expr ::= EXISTS LP select RP */
#line 1199 "parse.y"
{
    Expr *p;
    spanSet(&yymsp[-3].minor.yy200,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/
    p = yymsp[-3].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_EXISTS, 0, 0);
    sqlite3PExprAddSelect(pParse, p, yymsp[-1].minor.yy309);
  }
#line 3406 "parse.c"
        break;
      case 193: /* expr ::= CASE case_operand case_exprlist case_else END */
#line 1208 "parse.y"
{
  spanSet(&yymsp[-4].minor.yy200,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-C*/
  yymsp[-4].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_CASE, yymsp[-3].minor.yy294, 0);
  if( yymsp[-4].minor.yy200.pExpr ){
    yymsp[-4].minor.yy200.pExpr->x.pList = yymsp[-1].minor.yy294 ? sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy272,yymsp[-1].minor.yy294) : yymsp[-2].minor.yy272;
    sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy200.pExpr);
  }else{
    sqlite3ExprListDelete(pParse->db, yymsp[-2].minor.yy272);
    sqlite3ExprDelete(pParse->db, yymsp[-1].minor.yy294);
  }
}
#line 3421 "parse.c"
        break;
      case 194: /* case_exprlist ::= case_exprlist WHEN expr THEN expr */
#line 1221 "parse.y"
{
  yymsp[-4].minor.yy272 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy272, yymsp[-2].minor.yy200.pExpr);
  yymsp[-4].minor.yy272 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy272, yymsp[0].minor.yy200.pExpr);
}
#line 3429 "parse.c"
        break;
      case 195: /* case_exprlist ::= WHEN expr THEN expr */
#line 1225 "parse.y"
{
  yymsp[-3].minor.yy272 = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy200.pExpr);
  yymsp[-3].minor.yy272 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy272, yymsp[0].minor.yy200.pExpr);
}
#line 3437 "parse.c"
        break;
      case 198: /* case_operand ::= expr */
#line 1235 "parse.y"
{yymsp[0].minor.yy294 = yymsp[0].minor.yy200.pExpr; /*A-overwrites-X*/}
#line 3442 "parse.c"
        break;
      case 201: /* nexprlist ::= nexprlist COMMA expr */
#line 1246 "parse.y"
{yymsp[-2].minor.yy272 = sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy272,yymsp[0].minor.yy200.pExpr);}
#line 3447 "parse.c"
        break;
      case 202: /* nexprlist ::= expr */
#line 1248 "parse.y"
{yymsp[0].minor.yy272 = sqlite3ExprListAppend(pParse,0,yymsp[0].minor.yy200.pExpr); /*A-overwrites-Y*/}
#line 3452 "parse.c"
        break;
      case 204: /* paren_exprlist ::= LP exprlist RP */
      case 209: /* eidlist_opt ::= LP eidlist RP */ yytestcase(yyruleno==209);
#line 1256 "parse.y"
{yymsp[-2].minor.yy272 = yymsp[-1].minor.yy272;}
#line 3458 "parse.c"
        break;
      case 205: /* cmd ::= createkw uniqueflag INDEX ifnotexists nm ON nm LP sortlist RP where_opt */
#line 1263 "parse.y"
{
  sqlite3CreateIndex(pParse, &yymsp[-6].minor.yy0, 
                     sqlite3SrcListAppend(pParse->db,0,&yymsp[-4].minor.yy0,0), yymsp[-2].minor.yy272, yymsp[-9].minor.yy322,
                      &yymsp[-10].minor.yy0, yymsp[0].minor.yy294, SQLITE_SO_ASC, yymsp[-7].minor.yy322, SQLITE_IDXTYPE_APPDEF);
}
#line 3467 "parse.c"
        break;
      case 206: /* uniqueflag ::= UNIQUE */
      case 246: /* raisetype ::= ABORT */ yytestcase(yyruleno==246);
#line 1270 "parse.y"
{yymsp[0].minor.yy322 = OE_Abort;}
#line 3473 "parse.c"
        break;
      case 207: /* uniqueflag ::= */
#line 1271 "parse.y"
{yymsp[1].minor.yy322 = OE_None;}
#line 3478 "parse.c"
        break;
      case 210: /* eidlist ::= eidlist COMMA nm collate sortorder */
#line 1314 "parse.y"
{
  yymsp[-4].minor.yy272 = parserAddExprIdListTerm(pParse, yymsp[-4].minor.yy272, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy322, yymsp[0].minor.yy322);
}
#line 3485 "parse.c"
        break;
      case 211: /* eidlist ::= nm collate sortorder */
#line 1317 "parse.y"
{
  yymsp[-2].minor.yy272 = parserAddExprIdListTerm(pParse, 0, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy322, yymsp[0].minor.yy322); /*A-overwrites-Y*/
}
#line 3492 "parse.c"
        break;
      case 214: /* cmd ::= DROP INDEX ifexists fullname ON nm */
#line 1328 "parse.y"
{
    sqlite3DropIndex(pParse, yymsp[-2].minor.yy73, &yymsp[0].minor.yy0, yymsp[-3].minor.yy322);
}
#line 3499 "parse.c"
        break;
      case 215: /* cmd ::= PRAGMA nm */
#line 1335 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[0].minor.yy0,0,0,0,0);
}
#line 3506 "parse.c"
        break;
      case 216: /* cmd ::= PRAGMA nm EQ nmnum */
#line 1338 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-2].minor.yy0,0,&yymsp[0].minor.yy0,0,0);
}
#line 3513 "parse.c"
        break;
      case 217: /* cmd ::= PRAGMA nm LP nmnum RP */
#line 1341 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,0,&yymsp[-1].minor.yy0,0,0);
}
#line 3520 "parse.c"
        break;
      case 218: /* cmd ::= PRAGMA nm EQ minus_num */
#line 1344 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-2].minor.yy0,0,&yymsp[0].minor.yy0,0,1);
}
#line 3527 "parse.c"
        break;
      case 219: /* cmd ::= PRAGMA nm LP minus_num RP */
#line 1347 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,0,&yymsp[-1].minor.yy0,0,1);
}
#line 3534 "parse.c"
        break;
      case 220: /* cmd ::= PRAGMA nm EQ nm DOT nm */
#line 1350 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-4].minor.yy0,0,&yymsp[0].minor.yy0,&yymsp[-2].minor.yy0,0);
}
#line 3541 "parse.c"
        break;
      case 223: /* cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END */
#line 1369 "parse.y"
{
  Token all;
  all.z = yymsp[-3].minor.yy0.z;
  all.n = (int)(yymsp[0].minor.yy0.z - yymsp[-3].minor.yy0.z) + yymsp[0].minor.yy0.n;
  sqlite3FinishTrigger(pParse, yymsp[-1].minor.yy251, &all);
}
#line 3551 "parse.c"
        break;
      case 224: /* trigger_decl ::= TRIGGER ifnotexists nm trigger_time trigger_event ON fullname foreach_clause when_clause */
#line 1378 "parse.y"
{
  sqlite3BeginTrigger(pParse, &yymsp[-6].minor.yy0, yymsp[-5].minor.yy322, yymsp[-4].minor.yy60.a, yymsp[-4].minor.yy60.b, yymsp[-2].minor.yy73, yymsp[0].minor.yy294, yymsp[-7].minor.yy322);
  yymsp[-8].minor.yy0 = yymsp[-6].minor.yy0; /*yymsp[-8].minor.yy0-overwrites-T*/
}
#line 3559 "parse.c"
        break;
      case 225: /* trigger_time ::= BEFORE */
#line 1384 "parse.y"
{ yymsp[0].minor.yy322 = TK_BEFORE; }
#line 3564 "parse.c"
        break;
      case 226: /* trigger_time ::= AFTER */
#line 1385 "parse.y"
{ yymsp[0].minor.yy322 = TK_AFTER;  }
#line 3569 "parse.c"
        break;
      case 227: /* trigger_time ::= INSTEAD OF */
#line 1386 "parse.y"
{ yymsp[-1].minor.yy322 = TK_INSTEAD;}
#line 3574 "parse.c"
        break;
      case 228: /* trigger_time ::= */
#line 1387 "parse.y"
{ yymsp[1].minor.yy322 = TK_BEFORE; }
#line 3579 "parse.c"
        break;
      case 229: /* trigger_event ::= DELETE|INSERT */
      case 230: /* trigger_event ::= UPDATE */ yytestcase(yyruleno==230);
#line 1391 "parse.y"
{yymsp[0].minor.yy60.a = yymsp[0].major; /*A-overwrites-X*/ yymsp[0].minor.yy60.b = 0;}
#line 3585 "parse.c"
        break;
      case 231: /* trigger_event ::= UPDATE OF idlist */
#line 1393 "parse.y"
{yymsp[-2].minor.yy60.a = TK_UPDATE; yymsp[-2].minor.yy60.b = yymsp[0].minor.yy436;}
#line 3590 "parse.c"
        break;
      case 232: /* when_clause ::= */
#line 1400 "parse.y"
{ yymsp[1].minor.yy294 = 0; }
#line 3595 "parse.c"
        break;
      case 233: /* when_clause ::= WHEN expr */
#line 1401 "parse.y"
{ yymsp[-1].minor.yy294 = yymsp[0].minor.yy200.pExpr; }
#line 3600 "parse.c"
        break;
      case 234: /* trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI */
#line 1405 "parse.y"
{
  assert( yymsp[-2].minor.yy251!=0 );
  yymsp[-2].minor.yy251->pLast->pNext = yymsp[-1].minor.yy251;
  yymsp[-2].minor.yy251->pLast = yymsp[-1].minor.yy251;
}
#line 3609 "parse.c"
        break;
      case 235: /* trigger_cmd_list ::= trigger_cmd SEMI */
#line 1410 "parse.y"
{ 
  assert( yymsp[-1].minor.yy251!=0 );
  yymsp[-1].minor.yy251->pLast = yymsp[-1].minor.yy251;
}
#line 3617 "parse.c"
        break;
      case 236: /* trnm ::= nm DOT nm */
#line 1421 "parse.y"
{
  yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;
  sqlite3ErrorMsg(pParse, 
        "qualified table names are not allowed on INSERT, UPDATE, and DELETE "
        "statements within triggers");
}
#line 3627 "parse.c"
        break;
      case 237: /* tridxby ::= INDEXED BY nm */
#line 1433 "parse.y"
{
  sqlite3ErrorMsg(pParse,
        "the INDEXED BY clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
#line 3636 "parse.c"
        break;
      case 238: /* tridxby ::= NOT INDEXED */
#line 1438 "parse.y"
{
  sqlite3ErrorMsg(pParse,
        "the NOT INDEXED clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
#line 3645 "parse.c"
        break;
      case 239: /* trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt */
#line 1451 "parse.y"
{yymsp[-6].minor.yy251 = sqlite3TriggerUpdateStep(pParse->db, &yymsp[-4].minor.yy0, yymsp[-1].minor.yy272, yymsp[0].minor.yy294, yymsp[-5].minor.yy322);}
#line 3650 "parse.c"
        break;
      case 240: /* trigger_cmd ::= insert_cmd INTO trnm idlist_opt select */
#line 1455 "parse.y"
{yymsp[-4].minor.yy251 = sqlite3TriggerInsertStep(pParse->db, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy436, yymsp[0].minor.yy309, yymsp[-4].minor.yy322);/*A-overwrites-R*/}
#line 3655 "parse.c"
        break;
      case 241: /* trigger_cmd ::= DELETE FROM trnm tridxby where_opt */
#line 1459 "parse.y"
{yymsp[-4].minor.yy251 = sqlite3TriggerDeleteStep(pParse->db, &yymsp[-2].minor.yy0, yymsp[0].minor.yy294);}
#line 3660 "parse.c"
        break;
      case 242: /* trigger_cmd ::= select */
#line 1463 "parse.y"
{yymsp[0].minor.yy251 = sqlite3TriggerSelectStep(pParse->db, yymsp[0].minor.yy309); /*A-overwrites-X*/}
#line 3665 "parse.c"
        break;
      case 243: /* expr ::= RAISE LP IGNORE RP */
#line 1466 "parse.y"
{
  spanSet(&yymsp[-3].minor.yy200,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-X*/
  yymsp[-3].minor.yy200.pExpr = sqlite3PExpr(pParse, TK_RAISE, 0, 0); 
  if( yymsp[-3].minor.yy200.pExpr ){
    yymsp[-3].minor.yy200.pExpr->affinity = OE_Ignore;
  }
}
#line 3676 "parse.c"
        break;
      case 244: /* expr ::= RAISE LP raisetype COMMA nm RP */
#line 1473 "parse.y"
{
  spanSet(&yymsp[-5].minor.yy200,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-X*/
  yymsp[-5].minor.yy200.pExpr = sqlite3ExprAlloc(pParse->db, TK_RAISE, &yymsp[-1].minor.yy0, 1); 
  if( yymsp[-5].minor.yy200.pExpr ) {
    yymsp[-5].minor.yy200.pExpr->affinity = (char)yymsp[-3].minor.yy322;
  }
}
#line 3687 "parse.c"
        break;
      case 245: /* raisetype ::= ROLLBACK */
#line 1483 "parse.y"
{yymsp[0].minor.yy322 = OE_Rollback;}
#line 3692 "parse.c"
        break;
      case 247: /* raisetype ::= FAIL */
#line 1485 "parse.y"
{yymsp[0].minor.yy322 = OE_Fail;}
#line 3697 "parse.c"
        break;
      case 248: /* cmd ::= DROP TRIGGER ifexists fullname */
#line 1490 "parse.y"
{
  sqlite3DropTrigger(pParse,yymsp[0].minor.yy73,yymsp[-1].minor.yy322);
}
#line 3704 "parse.c"
        break;
      case 249: /* cmd ::= REINDEX */
#line 1497 "parse.y"
{sqlite3Reindex(pParse, 0, 0);}
#line 3709 "parse.c"
        break;
      case 250: /* cmd ::= REINDEX nm */
#line 1498 "parse.y"
{sqlite3Reindex(pParse, &yymsp[0].minor.yy0, 0);}
#line 3714 "parse.c"
        break;
      case 251: /* cmd ::= REINDEX nm ON nm */
#line 1499 "parse.y"
{sqlite3Reindex(pParse, &yymsp[-2].minor.yy0, &yymsp[0].minor.yy0);}
#line 3719 "parse.c"
        break;
      case 252: /* cmd ::= ANALYZE */
#line 1504 "parse.y"
{sqlite3Analyze(pParse, 0);}
#line 3724 "parse.c"
        break;
      case 253: /* cmd ::= ANALYZE nm */
#line 1505 "parse.y"
{sqlite3Analyze(pParse, &yymsp[0].minor.yy0);}
#line 3729 "parse.c"
        break;
      case 254: /* cmd ::= ALTER TABLE fullname RENAME TO nm */
#line 1510 "parse.y"
{
  sqlite3AlterRenameTable(pParse,yymsp[-3].minor.yy73,&yymsp[0].minor.yy0);
}
#line 3736 "parse.c"
        break;
      case 255: /* cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist */
#line 1514 "parse.y"
{
  yymsp[-1].minor.yy0.n = (int)(pParse->sLastToken.z-yymsp[-1].minor.yy0.z) + pParse->sLastToken.n;
  sqlite3AlterFinishAddColumn(pParse, &yymsp[-1].minor.yy0);
}
#line 3744 "parse.c"
        break;
      case 256: /* add_column_fullname ::= fullname */
#line 1518 "parse.y"
{
  disableLookaside(pParse);
  sqlite3AlterBeginAddColumn(pParse, yymsp[0].minor.yy73);
}
#line 3752 "parse.c"
        break;
      case 257: /* with ::= */
#line 1532 "parse.y"
{yymsp[1].minor.yy445 = 0;}
#line 3757 "parse.c"
        break;
      case 258: /* with ::= WITH wqlist */
#line 1534 "parse.y"
{ yymsp[-1].minor.yy445 = yymsp[0].minor.yy445; }
#line 3762 "parse.c"
        break;
      case 259: /* with ::= WITH RECURSIVE wqlist */
#line 1535 "parse.y"
{ yymsp[-2].minor.yy445 = yymsp[0].minor.yy445; }
#line 3767 "parse.c"
        break;
      case 260: /* wqlist ::= nm eidlist_opt AS LP select RP */
#line 1537 "parse.y"
{
  yymsp[-5].minor.yy445 = sqlite3WithAdd(pParse, 0, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy272, yymsp[-1].minor.yy309); /*A-overwrites-X*/
}
#line 3774 "parse.c"
        break;
      case 261: /* wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP */
#line 1540 "parse.y"
{
  yymsp[-7].minor.yy445 = sqlite3WithAdd(pParse, yymsp[-7].minor.yy445, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy272, yymsp[-1].minor.yy309);
}
#line 3781 "parse.c"
        break;
      default:
      /* (262) input ::= ecmd */ yytestcase(yyruleno==262);
      /* (263) explain ::= */ yytestcase(yyruleno==263);
      /* (264) cmdx ::= cmd (OPTIMIZED OUT) */ assert(yyruleno!=264);
      /* (265) trans_opt ::= */ yytestcase(yyruleno==265);
      /* (266) trans_opt ::= TRANSACTION */ yytestcase(yyruleno==266);
      /* (267) trans_opt ::= TRANSACTION nm */ yytestcase(yyruleno==267);
      /* (268) savepoint_opt ::= SAVEPOINT */ yytestcase(yyruleno==268);
      /* (269) savepoint_opt ::= */ yytestcase(yyruleno==269);
      /* (270) cmd ::= create_table create_table_args */ yytestcase(yyruleno==270);
      /* (271) columnlist ::= columnlist COMMA columnname carglist */ yytestcase(yyruleno==271);
      /* (272) columnlist ::= columnname carglist */ yytestcase(yyruleno==272);
      /* (273) nm ::= JOIN_KW */ yytestcase(yyruleno==273);
      /* (274) typetoken ::= typename */ yytestcase(yyruleno==274);
      /* (275) typename ::= ID|STRING */ yytestcase(yyruleno==275);
      /* (276) signed ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=276);
      /* (277) signed ::= minus_num (OPTIMIZED OUT) */ assert(yyruleno!=277);
      /* (278) carglist ::= carglist ccons */ yytestcase(yyruleno==278);
      /* (279) carglist ::= */ yytestcase(yyruleno==279);
      /* (280) ccons ::= NULL onconf */ yytestcase(yyruleno==280);
      /* (281) conslist_opt ::= COMMA conslist */ yytestcase(yyruleno==281);
      /* (282) conslist ::= conslist tconscomma tcons */ yytestcase(yyruleno==282);
      /* (283) conslist ::= tcons (OPTIMIZED OUT) */ assert(yyruleno!=283);
      /* (284) tconscomma ::= */ yytestcase(yyruleno==284);
      /* (285) defer_subclause_opt ::= defer_subclause (OPTIMIZED OUT) */ assert(yyruleno!=285);
      /* (286) resolvetype ::= raisetype (OPTIMIZED OUT) */ assert(yyruleno!=286);
      /* (287) selectnowith ::= oneselect (OPTIMIZED OUT) */ assert(yyruleno!=287);
      /* (288) oneselect ::= values */ yytestcase(yyruleno==288);
      /* (289) sclp ::= selcollist COMMA */ yytestcase(yyruleno==289);
      /* (290) as ::= ID|STRING */ yytestcase(yyruleno==290);
      /* (291) expr ::= term (OPTIMIZED OUT) */ assert(yyruleno!=291);
      /* (292) exprlist ::= nexprlist */ yytestcase(yyruleno==292);
      /* (293) nmnum ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=293);
      /* (294) nmnum ::= nm */ yytestcase(yyruleno==294);
      /* (295) nmnum ::= ON */ yytestcase(yyruleno==295);
      /* (296) nmnum ::= DELETE */ yytestcase(yyruleno==296);
      /* (297) nmnum ::= DEFAULT */ yytestcase(yyruleno==297);
      /* (298) plus_num ::= INTEGER|FLOAT */ yytestcase(yyruleno==298);
      /* (299) foreach_clause ::= */ yytestcase(yyruleno==299);
      /* (300) foreach_clause ::= FOR EACH ROW */ yytestcase(yyruleno==300);
      /* (301) trnm ::= nm */ yytestcase(yyruleno==301);
      /* (302) tridxby ::= */ yytestcase(yyruleno==302);
      /* (303) kwcolumn_opt ::= */ yytestcase(yyruleno==303);
      /* (304) kwcolumn_opt ::= COLUMNKW */ yytestcase(yyruleno==304);
        break;
/********** End reduce actions ************************************************/
  };
  assert( yyruleno<sizeof(yyRuleInfo)/sizeof(yyRuleInfo[0]) );
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact <= YY_MAX_SHIFTREDUCE ){
    if( yyact>YY_MAX_SHIFT ){
      yyact += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
    }
    yymsp -= yysize-1;
    yypParser->yytos = yymsp;
    yymsp->stateno = (YYACTIONTYPE)yyact;
    yymsp->major = (YYCODETYPE)yygoto;
    yyTraceShift(yypParser, yyact);
  }else{
    assert( yyact == YY_ACCEPT_ACTION );
    yypParser->yytos -= yysize;
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
/************ Begin %parse_failure code ***************************************/
/************ End %parse_failure code *****************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  sqlite3ParserTOKENTYPE yyminor         /* The minor type of the error token */
){
  sqlite3ParserARG_FETCH;
#define TOKEN yyminor
/************ Begin %syntax_error code ****************************************/
#line 32 "parse.y"

  UNUSED_PARAMETER(yymajor);  /* Silence some compiler warnings */
  assert( TOKEN.z[0] );  /* The tokenizer always gives us a token */
  if (yypParser->is_fallback_failed && TOKEN.isReserved) {
    sqlite3ErrorMsg(pParse, "keyword \"%T\" is reserved", &TOKEN);
  } else {
    sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &TOKEN);
  }
#line 3892 "parse.c"
/************ End %syntax_error code ******************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  assert( yypParser->yytos==yypParser->yystack );
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
/*********** Begin %parse_accept code *****************************************/
/*********** End %parse_accept code *******************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "sqlite3ParserAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3Parser(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  sqlite3ParserTOKENTYPE yyminor       /* The value for the token */
  sqlite3ParserARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  unsigned int yyact;   /* The parser action. */
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  int yyendofinput;     /* True if we are at the end of input */
#endif
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  yypParser = (yyParser*)yyp;
  assert( yypParser->yytos!=0 );
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  yyendofinput = (yymajor==0);
#endif
  sqlite3ParserARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput '%s'\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact <= YY_MAX_SHIFTREDUCE ){
      yy_shift(yypParser,yyact,yymajor,yyminor);
#ifndef YYNOERRORRECOVERY
      yypParser->yyerrcnt--;
#endif
      yymajor = YYNOCODE;
    }else if( yyact <= YY_MAX_REDUCE ){
      yy_reduce(yypParser,yyact-YY_MIN_REDUCE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
      yyminorunion.yy0 = yyminor;
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminor);
      }
      yymx = yypParser->yytos->major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor, &yyminorunion);
        yymajor = YYNOCODE;
      }else{
        while( yypParser->yytos >= yypParser->yystack
            && yymx != YYERRORSYMBOL
            && (yyact = yy_find_reduce_action(
                        yypParser->yytos->stateno,
                        YYERRORSYMBOL)) >= YY_MIN_REDUCE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yytos < yypParser->yystack || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
          yypParser->yyerrcnt = -1;
#endif
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          yy_shift(yypParser,yyact,YYERRORSYMBOL,yyminor);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor, yyminor);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor, yyminor);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
        yypParser->yyerrcnt = -1;
#endif
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yytos>yypParser->yystack );
#ifndef NDEBUG
  if( yyTraceFILE ){
    yyStackEntry *i;
    char cDiv = '[';
    fprintf(yyTraceFILE,"%sReturn. Stack=",yyTracePrompt);
    for(i=&yypParser->yystack[1]; i<=yypParser->yytos; i++){
      fprintf(yyTraceFILE,"%c%s", cDiv, yyTokenName[i->major]);
      cDiv = ' ';
    }
    fprintf(yyTraceFILE,"]\n");
  }
#endif
  return;
}
