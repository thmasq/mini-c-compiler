%{
#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include "parser.h"
#include <string.h>
#include <stdlib.h>

int line_number = 1;
int column = 0;

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

// Update column tracking
void count_chars() {
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            column = 0;
            line_number++;
        } else if (yytext[i] == '\t') {
            column += 8 - (column % 8);
        } else {
            column++;
        }
    }
}

// Process character literals
char process_char_literal(const char *text) {
    if (text[1] == '\\') {
        // Escape sequence
        switch (text[2]) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case 'b': return '\b';
            case 'f': return '\f';
            case 'a': return '\a';
            case 'v': return '\v';
            case '\\': return '\\';
            case '\'': return '\'';
            case '\"': return '\"';
            case '0': return '\0';
            default: return text[2];
        }
    } else {
        return text[1];
    }
}

// Process string literals (basic - doesn't handle all escape sequences)
char *process_string_literal(const char *text) {
    int len = strlen(text);
    char *result = malloc(len - 1); // remove quotes
    int j = 0;
    
    for (int i = 1; i < len - 1; i++) { // skip quotes
        if (text[i] == '\\' && i + 1 < len - 1) {
            // Handle escape sequences
            switch (text[i + 1]) {
                case 'n': result[j++] = '\n'; i++; break;
                case 't': result[j++] = '\t'; i++; break;
                case 'r': result[j++] = '\r'; i++; break;
                case 'b': result[j++] = '\b'; i++; break;
                case 'f': result[j++] = '\f'; i++; break;
                case 'a': result[j++] = '\a'; i++; break;
                case 'v': result[j++] = '\v'; i++; break;
                case '\\': result[j++] = '\\'; i++; break;
                case '\'': result[j++] = '\''; i++; break;
                case '\"': result[j++] = '\"'; i++; break;
                case '0': result[j++] = '\0'; i++; break;
                default: 
                    result[j++] = '\\';
                    result[j++] = text[i + 1];
                    i++;
                    break;
            }
        } else {
            result[j++] = text[i];
        }
    }
    result[j] = '\0';
    return result;
}
%}

%option noyywrap
%option nounput
%option noinput

/* Definitions for common patterns */
D               [0-9]
L               [a-zA-Z_]
H               [a-fA-F0-9]
E               ([Ee][+-]?{D}+)
P               ([Pp][+-]?{D}+)
FS              (f|F|l|L)
IS              ((u|U)|(u|U)?(l|L|ll|LL)|(l|L|ll|LL)(u|U))

%%

"/*"                    { /* Multi-line comment */
                          int c;
                          while ((c = input()) != 0) {
                              if (c == '\n') {
                                  line_number++;
                                  column = 0;
                              } else {
                                  column++;
                              }
                              if (c == '*') {
                                  if ((c = input()) == '/') {
                                      column++;
                                      break;
                                  } else {
                                      unput(c);
                                  }
                              }
                          }
                        }

"//".*$                 { /* Single-line comment */ }

/* Keywords */
"auto"                  { count_chars(); return AUTO; }
"_Bool"                 { count_chars(); return BOOL; }
"break"                 { count_chars(); return BREAK; }
"case"                  { count_chars(); return CASE; }
"char"                  { count_chars(); return CHAR; }
"_Complex"              { count_chars(); return COMPLEX; }
"const"                 { count_chars(); return CONST; }
"continue"              { count_chars(); return CONTINUE; }
"default"               { count_chars(); return DEFAULT; }
"do"                    { count_chars(); return DO; }
"double"                { count_chars(); return DOUBLE; }
"else"                  { count_chars(); return ELSE; }
"enum"                  { count_chars(); return ENUM; }
"extern"                { count_chars(); return EXTERN; }
"float"                 { count_chars(); return FLOAT; }
"for"                   { count_chars(); return FOR; }
"goto"                  { count_chars(); return GOTO; }
"if"                    { count_chars(); return IF; }
"_Imaginary"            { count_chars(); return IMAGINARY; }
"inline"                { count_chars(); return INLINE; }
"int"                   { count_chars(); return INT; }
"long"                  { count_chars(); return LONG; }
"register"              { count_chars(); return REGISTER; }
"restrict"              { count_chars(); return RESTRICT; }
"return"                { count_chars(); return RETURN; }
"short"                 { count_chars(); return SHORT; }
"signed"                { count_chars(); return SIGNED; }
"sizeof"                { count_chars(); return SIZEOF; }
"static"                { count_chars(); return STATIC; }
"struct"                { count_chars(); return STRUCT; }
"switch"                { count_chars(); return SWITCH; }
"typedef"               { count_chars(); return TYPEDEF; }
"union"                 { count_chars(); return UNION; }
"unsigned"              { count_chars(); return UNSIGNED; }
"void"                  { count_chars(); return VOID; }
"volatile"              { count_chars(); return VOLATILE; }
"while"                 { count_chars(); return WHILE; }

/* Identifiers */
{L}({L}|{D})*           { count_chars(); 
                          yylval.string = string_duplicate(yytext); 
                          return IDENTIFIER; 
                        }

/* Integer constants */
0[xX]{H}+{IS}?          { count_chars(); 
                          yylval.number = (int)strtol(yytext, NULL, 16); 
                          return CONSTANT; 
                        }
0[0-7]*{IS}?            { count_chars(); 
                          yylval.number = (int)strtol(yytext, NULL, 8); 
                          return CONSTANT; 
                        }
[1-9]{D}*{IS}?          { count_chars(); 
                          yylval.number = atoi(yytext); 
                          return CONSTANT; 
                        }

/* Character constants */
L?'(\\.|[^\\'\n])+'     { count_chars(); 
                          yylval.character = process_char_literal(yytext); 
                          return CONSTANT; 
                        }

/* Floating point constants */
{D}+{E}{FS}?            { count_chars(); 
                          yylval.number = (int)atof(yytext); 
                          return CONSTANT; 
                        }
{D}*"."{D}+{E}?{FS}?    { count_chars(); 
                          yylval.number = (int)atof(yytext); 
                          return CONSTANT; 
                        }
{D}+"."{D}*{E}?{FS}?    { count_chars(); 
                          yylval.number = (int)atof(yytext); 
                          return CONSTANT; 
                        }
0[xX]{H}+{P}{FS}?       { count_chars(); 
                          yylval.number = (int)strtod(yytext, NULL); 
                          return CONSTANT; 
                        }
0[xX]{H}*"."{H}+{P}{FS}? { count_chars(); 
                          yylval.number = (int)strtod(yytext, NULL); 
                          return CONSTANT; 
                        }
0[xX]{H}+"."{H}*{P}{FS}? { count_chars(); 
                          yylval.number = (int)strtod(yytext, NULL); 
                          return CONSTANT; 
                        }

/* String literals */
L?\"(\\.|[^\\"\n])*\"   { count_chars(); 
                          yylval.string = process_string_literal(yytext); 
                          return STRING_LITERAL; 
                        }

/* Multi-character operators */
"..."                   { count_chars(); return ELLIPSIS; }
">>="                   { count_chars(); return RIGHT_ASSIGN; }
"<<="                   { count_chars(); return LEFT_ASSIGN; }
"+="                    { count_chars(); return ADD_ASSIGN; }
"-="                    { count_chars(); return SUB_ASSIGN; }
"*="                    { count_chars(); return MUL_ASSIGN; }
"/="                    { count_chars(); return DIV_ASSIGN; }
"%="                    { count_chars(); return MOD_ASSIGN; }
"&="                    { count_chars(); return AND_ASSIGN; }
"^="                    { count_chars(); return XOR_ASSIGN; }
"|="                    { count_chars(); return OR_ASSIGN; }
">>"                    { count_chars(); return RIGHT_OP; }
"<<"                    { count_chars(); return LEFT_OP; }
"++"                    { count_chars(); return INC_OP; }
"--"                    { count_chars(); return DEC_OP; }
"->"                    { count_chars(); return PTR_OP; }
"&&"                    { count_chars(); return AND_OP; }
"||"                    { count_chars(); return OR_OP; }
"<="                    { count_chars(); return LE_OP; }
">="                    { count_chars(); return GE_OP; }
"=="                    { count_chars(); return EQ_OP; }
"!="                    { count_chars(); return NE_OP; }

/* Single character tokens */
";"                     { count_chars(); return SEMICOLON; }
("{"|"<%")              { count_chars(); return LBRACE; }
("}"|"%>")              { count_chars(); return RBRACE; }
","                     { count_chars(); return COMMA; }
":"                     { count_chars(); return COLON; }
"="                     { count_chars(); return ASSIGN; }
"("                     { count_chars(); return LPAREN; }
")"                     { count_chars(); return RPAREN; }
("["|"<:")              { count_chars(); return LBRACKET; }
("]"|":>")              { count_chars(); return RBRACKET; }
"."                     { count_chars(); return DOT; }
"&"                     { count_chars(); return AMPERSAND; }
"!"                     { count_chars(); return EXCLAMATION; }
"~"                     { count_chars(); return TILDE; }
"-"                     { count_chars(); return MINUS; }
"+"                     { count_chars(); return PLUS; }
"*"                     { count_chars(); return ASTERISK; }
"/"                     { count_chars(); return SLASH; }
"%"                     { count_chars(); return PERCENT; }
"<"                     { count_chars(); return LESS_THAN; }
">"                     { count_chars(); return GREATER_THAN; }
"^"                     { count_chars(); return CARET; }
"|"                     { count_chars(); return PIPE; }
"?"                     { count_chars(); return QUESTION; }

/* Whitespace */
[ \t\v\f]               { count_chars(); }
\n                      { line_number++; column = 0; }

/* Catch-all for unrecognized characters */
.                       { 
                          fprintf(stderr, "Unexpected character '%c' at line %d, column %d\n", 
                                  yytext[0], line_number, column); 
                          count_chars();
                          return yytext[0];
                        }

%%

/* Additional functions */
void reset_lexer() {
    line_number = 1;
    column = 0;
}

int get_line_number() {
    return line_number;
}

int get_column() {
    return column;
}
