%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nodes.h"

extern std::string outfilename;
extern int errorcount;
int yyerror(const char *s);
int yylex(void);
%}

%define parse.error verbose

%union {
  char *str;
  int itg;
  double flt;
  Node *node;
}

%token TOK_PRINT TOK_ATTR TOK_INC TOK_DEC TOK_IF TOK_ELSE TOK_WHILE
%token TOK_EQUAL TOK_DIFF TOK_GREATER TOK_LESS TOK_GREATER_EQUAL TOK_LESS_EQUAL
%token TOK_AND TOK_OR TOK_NOT
%token TOK_DECL_NUM TOK_DECL_STR
%token TOK_IDENT TOK_FLOAT TOK_INT TOK_STR

%type<str> TOK_IDENT
%type<itg> TOK_INT
%type<flt> TOK_FLOAT
%type<str> TOK_STR
%type<node> stmts stmt expr term factor
%type<node> relational
%type<node> logical

%start program

%%

program : stmts {
  Program *p = new Program();
  p->addChild($stmts);

  PrintTree pt;
  pt.print(p);

  CheckVars ck;
  ck.check(p);

  if (errorcount > 0) {
    printf("%d error(s) found.\n", errorcount);
    exit(errorcount);
  } else {
    CodeGen cg;
    cg.generate(p, outfilename);
  }
}

stmts : stmts[ss] stmt {
  $ss->addChild($stmt);
  $$ = $ss;
}

stmts : stmt {
  Stmts *stmts = new Stmts();
  stmts->addChild($stmt);
  $$ = stmts;
}

stmt : TOK_DECL_NUM TOK_IDENT[id] ';' {
  $$ = new Decl($id, Decl::NUM);
}

stmt : TOK_DECL_STR TOK_IDENT[id] ';' {
  $$ = new Decl($id, Decl::STR);
}

stmt : TOK_DECL_NUM TOK_IDENT[id] TOK_ATTR expr ';' {
  Node *node = new Node();
  node->addChild(new Decl($id, Decl::NUM));
  node->addChild(new Attr($id, $expr));
  $$ = node;
}

stmt : TOK_DECL_STR TOK_IDENT[id] TOK_ATTR TOK_STR[str] ';' {
  Node *node = new Node();
  node->addChild(new Decl($id, Decl::STR));
  node->addChild(new Attr($id, new Str($str)));
  $$ = node;
}

stmt : TOK_IDENT[id] TOK_ATTR expr ';' {
  $$ = new Attr($id, $expr);
}

stmt : TOK_IDENT[id] TOK_ATTR TOK_STR[str] ';' {
  $$ = new Attr($id, new Str($str));
}

stmt : TOK_IDENT[id] TOK_INC ';' {
  $$ = new Inc($id);
}

stmt : TOK_IDENT[id] TOK_DEC ';' {
  $$ = new Dec($id);
}

stmt : TOK_PRINT expr ';' {
  $$ = new Print($expr);
}

stmt : TOK_PRINT TOK_STR[str] ';' {
  $$ = new Print(new Str($str));
}

stmt : TOK_WHILE logical '{' stmts '}' {
  $$ = new While($logical, $stmts);
}

stmt : TOK_IF logical '{' stmts '}' {
  $$ = new If($logical, $stmts);
}

stmt : TOK_IF logical '{' stmts[ifStmts] '}' TOK_ELSE '{' stmts[elseStmts] '}' {
  $$ = new IfElse($logical, $ifStmts, $elseStmts);
}

logical : relational[lr] TOK_AND relational[rr] {
  $$ = new Logical($lr, Logical::AND, $rr);
}

logical : relational[lr] TOK_OR relational[rr] {
  $$ = new Logical($lr, Logical::OR, $rr);
}

logical : TOK_NOT relational {
  $$ = new Logical($relational, Logical::NOT);
}

logical : relational {
  $$ = $relational;
}

relational : expr[le] TOK_EQUAL expr[re] {
  $$ = new Relational($le, Relational::EQUAL, $re);
}

relational : expr[le] TOK_DIFF expr[re] {
  $$ = new Relational($le, Relational::DIFF, $re);
}

relational : expr[le] TOK_GREATER expr[re] {
  $$ = new Relational($le, Relational::GREATER, $re);
}

relational : expr[le] TOK_LESS expr[re] {
  $$ = new Relational($le, Relational::LESS, $re);
}

relational : expr[le] TOK_GREATER_EQUAL expr[re] {
  $$ = new Relational($le, Relational::GREATER_EQUAL, $re);
}

relational : expr[le] TOK_LESS_EQUAL expr[re] {
  $$ = new Relational($le, Relational::LESS_EQUAL, $re);
}

relational : '(' logical ')' {
  $$ = $logical;
}

expr : expr[ex] '+' term {
  $$ = new Arit($ex, $term, '+');
}

expr : expr[ex] '-' term {
  $$ = new Arit($ex, $term, '-');
}

expr : term {
  $$ = $term;
}

term : term[te] '*' factor {
  $$ = new Arit($te, $factor, '*');
}

term : term[te] '/' factor {
  $$ = new Arit($te, $factor, '/');
}

term : factor {
  $$ = $factor;
}

factor : '(' expr ')' {
  $$ = $expr;
}

factor : TOK_INT[itg] {
  $$ = new Int($itg);
}

factor : TOK_FLOAT[flt] {
  $$ = new Float($flt);
}

factor : TOK_IDENT[id] {
  $$ = new Ident($id);
}

%%

extern int yylineno;

int yyerror(const char *s) {
  printf("error on line %d: %s\n", yylineno, s);
  errorcount++;
  return 1;
}

