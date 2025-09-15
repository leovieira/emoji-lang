%{
#include <stdio.h>
#include <stdlib.h>

extern int errorcount;

class Node;
#include "emoji.tab.h"

char *gen_str(const char *token, int length) {
  char *str = (char*) malloc(length + 1);
  if (!str) { /* handle memory allocation error */ }
  memcpy(str, token, length);
  str[length] = '\0';
  return str;
}
%}

%option yylineno

A [a-zA-Z_]
D [0-9]

%%

[ \t\n\r]			{ /* ignore spaces */ }

"✏️"				{ return TOK_PRINT; }
"👈"				{ return TOK_ATTR; }
"⏩"				{ return TOK_INC; }
"⏪"				{ return TOK_DEC; }
"🤔"				{ return TOK_IF; }
"🙃"				{ return TOK_ELSE; }
"🔁"				{ return TOK_WHILE; }

"➕"				{ return '+'; }
"➖"				{ return '-'; }
"✖️"				{ return '*'; }
"➗"				{ return '/'; }

"✅"				{ return TOK_EQUAL; }
"❗✅"				{ return TOK_DIFF; }
"👆"				{ return TOK_GREATER; }
"👇"				{ return TOK_LESS; }
"👆✅"				{ return TOK_GREATER_EQUAL; }
"👇✅"				{ return TOK_LESS_EQUAL; }

"⚖️"				{ return TOK_AND; }
"✌️"				{ return TOK_OR; }
"❗"				{ return TOK_NOT; }

";"				{ return ';'; }
"("				{ return '('; }
")"				{ return ')'; }
"⤵️"				{ return '{'; }
"⤴️"				{ return '}'; }

"🔢"				{ return TOK_DECL_NUM; }
"🔡"				{ return TOK_DECL_STR; }

{A}({A}|{D})*			{ yylval.str = strndup(yytext, yyleng); return TOK_IDENT; }
{D}*\.{D}+			{ yylval.flt = atof(yytext); return TOK_FLOAT; }
{D}+				{ yylval.itg = atoi(yytext); return TOK_INT; }
\"([^\\\"]|\\.)*\"		{ yylval.str = gen_str(yytext + 1, yyleng - 2); return TOK_STR; }

.  {
  printf("Error: Invalid character %c\n", yytext[0]);
  errorcount++;
}

%%

int yywrap() {
  return 1;
}

