%{
#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yylex();
extern int yyparse();
extern FILE *yyin;
extern int line_number;

void yyerror(const char *s);

ast_node_t *ast_root = NULL;

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

%union {
    int number;
    char *string;
    ast_node_t *node;
    ast_node_t **node_list;
    struct {
        ast_node_t **nodes;
        int count;
    } node_array;
}

%token <number> NUMBER
%token <string> IDENTIFIER
%token INT CHAR VOID
%token IF ELSE WHILE RETURN
%token PLUS MINUS MULTIPLY DIVIDE MODULO
%token ASSIGN EQ NE LT LE GT GE NOT
%token LPAREN RPAREN LBRACE RBRACE SEMICOLON COMMA

%type <node> program function declaration statement compound_statement
%type <node> expression primary_expression assignment_statement
%type <node> if_statement while_statement return_statement
%type <node> parameter expression_statement
%type <string> type_specifier
%type <node_array> function_list statement_list parameter_list argument_list

%left EQ NE LT LE GT GE
%left PLUS MINUS
%left MULTIPLY DIVIDE MODULO
%right NOT UMINUS
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

program:
    function_list {
        ast_root = create_program($1.nodes, $1.count);
        $$ = ast_root;
    }
    ;

function_list:
    function {
        $$.nodes = malloc(sizeof(ast_node_t*));
        $$.nodes[0] = $1;
        $$.count = 1;
    }
    | function_list function {
        $$.count = $1.count + 1;
        $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
        $$.nodes[$$.count - 1] = $2;
    }
    ;

function:
    type_specifier IDENTIFIER LPAREN parameter_list RPAREN compound_statement {
        $$ = create_function($2, $1, $4.nodes, $4.count, $6);
    }
    | type_specifier IDENTIFIER LPAREN RPAREN compound_statement {
        $$ = create_function($2, $1, NULL, 0, $5);
    }
    ;

parameter_list:
    parameter {
        $$.nodes = malloc(sizeof(ast_node_t*));
        $$.nodes[0] = $1;
        $$.count = 1;
    }
    | parameter_list COMMA parameter {
        $$.count = $1.count + 1;
        $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
        $$.nodes[$$.count - 1] = $3;
    }
    ;

parameter:
    type_specifier IDENTIFIER {
        $$ = create_parameter($1, $2);
    }
    ;

type_specifier:
    INT { $$ = string_duplicate("int"); }
    | CHAR { $$ = string_duplicate("char"); }
    | VOID { $$ = string_duplicate("void"); }
    ;

compound_statement:
    LBRACE statement_list RBRACE {
        $$ = create_compound_stmt($2.nodes, $2.count);
    }
    | LBRACE RBRACE {
        $$ = create_compound_stmt(NULL, 0);
    }
    ;

statement_list:
    statement {
        $$.nodes = malloc(sizeof(ast_node_t*));
        $$.nodes[0] = $1;
        $$.count = 1;
    }
    | statement_list statement {
        $$.count = $1.count + 1;
        $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
        $$.nodes[$$.count - 1] = $2;
    }
    ;

statement:
    declaration
    | assignment_statement
    | if_statement
    | while_statement
    | return_statement
    | expression_statement
    | compound_statement
    ;

declaration:
    type_specifier IDENTIFIER SEMICOLON {
        $$ = create_declaration($1, $2, NULL);
    }
    | type_specifier IDENTIFIER ASSIGN expression SEMICOLON {
        $$ = create_declaration($1, $2, $4);
    }
    ;

assignment_statement:
    IDENTIFIER ASSIGN expression SEMICOLON {
        $$ = create_assignment($1, $3);
    }
    ;

if_statement:
    IF LPAREN expression RPAREN statement %prec LOWER_THAN_ELSE {
        $$ = create_if_stmt($3, $5, NULL);
    }
    | IF LPAREN expression RPAREN statement ELSE statement {
        $$ = create_if_stmt($3, $5, $7);
    }
    ;

while_statement:
    WHILE LPAREN expression RPAREN statement {
        $$ = create_while_stmt($3, $5);
    }
    ;

return_statement:
    RETURN expression SEMICOLON {
        $$ = create_return_stmt($2);
    }
    | RETURN SEMICOLON {
        $$ = create_return_stmt(NULL);
    }
    ;

expression_statement:
    expression SEMICOLON {
        $$ = create_expr_stmt($1);
    }
    ;

expression:
    primary_expression
    | expression PLUS expression {
        $$ = create_binary_op(OP_ADD, $1, $3);
    }
    | expression MINUS expression {
        $$ = create_binary_op(OP_SUB, $1, $3);
    }
    | expression MULTIPLY expression {
        $$ = create_binary_op(OP_MUL, $1, $3);
    }
    | expression DIVIDE expression {
        $$ = create_binary_op(OP_DIV, $1, $3);
    }
    | expression MODULO expression {
        $$ = create_binary_op(OP_MOD, $1, $3);
    }
    | expression EQ expression {
        $$ = create_binary_op(OP_EQ, $1, $3);
    }
    | expression NE expression {
        $$ = create_binary_op(OP_NE, $1, $3);
    }
    | expression LT expression {
        $$ = create_binary_op(OP_LT, $1, $3);
    }
    | expression LE expression {
        $$ = create_binary_op(OP_LE, $1, $3);
    }
    | expression GT expression {
        $$ = create_binary_op(OP_GT, $1, $3);
    }
    | expression GE expression {
        $$ = create_binary_op(OP_GE, $1, $3);
    }
    | MINUS expression %prec UMINUS {
        $$ = create_unary_op(OP_NEG, $2);
    }
    | NOT expression {
        $$ = create_unary_op(OP_NOT, $2);
    }
    | LPAREN expression RPAREN {
        $$ = $2;
    }
    ;

primary_expression:
    IDENTIFIER {
        $$ = create_identifier($1);
    }
    | NUMBER {
        $$ = create_number($1);
    }
    | IDENTIFIER LPAREN argument_list RPAREN {
        $$ = create_call($1, $3.nodes, $3.count);
    }
    | IDENTIFIER LPAREN RPAREN {
        $$ = create_call($1, NULL, 0);
    }
    ;

argument_list:
    expression {
        $$.nodes = malloc(sizeof(ast_node_t*));
        $$.nodes[0] = $1;
        $$.count = 1;
    }
    | argument_list COMMA expression {
        $$.count = $1.count + 1;
        $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
        $$.nodes[$$.count - 1] = $3;
    }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Parse error at line %d: %s\n", line_number, s);
}
