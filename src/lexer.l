%{
#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include "parser.h"
#include <string.h>
#include <stdlib.h>

int line_number = 1;

// Portable string duplication function
static char *string_duplicate(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);
    if (!copy) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    strcpy(copy, str);
    return copy;
}
%}

%option noyywrap
%option nounput
%option noinput

%%

"int"           { return INT; }
"char"          { return CHAR; }
"void"          { return VOID; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"return"        { return RETURN; }

[a-zA-Z_][a-zA-Z0-9_]*  { 
    yylval.string = string_duplicate(yytext); 
    return IDENTIFIER; 
}

[0-9]+          { 
    yylval.number = atoi(yytext); 
    return NUMBER; 
}

"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return MULTIPLY; }
"/"             { return DIVIDE; }
"="             { return ASSIGN; }
"=="            { return EQ; }
"!="            { return NE; }
"<"             { return LT; }
"<="            { return LE; }
">"             { return GT; }
">="            { return GE; }
"!"             { return NOT; }

"("             { return LPAREN; }
")"             { return RPAREN; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
";"             { return SEMICOLON; }
","             { return COMMA; }

[ \t]+          { /* ignore whitespace */ }
\n              { line_number++; }

"//".*          { /* ignore single-line comments */ }

"/*"([^*]|\*+[^*/])*\*+"/"  { /* ignore multi-line comments */ }

.               { 
    fprintf(stderr, "Unexpected character: %c at line %d\n", yytext[0], line_number); 
    return yytext[0];
}

%%
