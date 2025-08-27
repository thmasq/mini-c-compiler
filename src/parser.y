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

// Helper to create type_info from type_specifier and declarator
static type_info_t make_type_info(type_info_t base_type, declarator_t decl) {
    type_info_t result = base_type;
    result.pointer_level = decl.pointer_level;
    result.is_array = decl.is_array;
    result.array_size = decl.array_size;
    if (decl.array_size && decl.array_size->type != AST_NUMBER) {
        result.is_vla = 1;
    } else {
        result.is_vla = 0;
    }
    return result;
}
%}

%union {
    int number;
    char *string;
    ast_node_t *node;
    ast_node_t **node_list;
    type_info_t type_info;
    declarator_t declarator;
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
%token ASSIGN EQ NE LT LE GT GE 
%token NOT LAND LOR AMPERSAND PIPE CARET TILDE LSHIFT RSHIFT
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET SEMICOLON COMMA

%type <node> program function declaration statement compound_statement
%type <node> expression primary_expression assignment_statement
%type <node> if_statement while_statement return_statement
%type <node> parameter expression_statement
%type <type_info> type_specifier
%type <declarator> declarator pointer_declarator direct_declarator
%type <number> pointer
%type <node_array> function_list statement_list parameter_list argument_list

// Operator precedence and associativity (lowest to highest precedence)
%right ASSIGN
%left LOR
%left LAND
%left PIPE
%left CARET
%left AMPERSAND
%left EQ NE
%left LT LE GT GE
%left LSHIFT RSHIFT
%left PLUS MINUS
%left MULTIPLY DIVIDE MODULO
%right NOT UMINUS USTAR UAMPERSAND UTILDE
%left LBRACKET
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
    type_specifier declarator LPAREN parameter_list RPAREN compound_statement {
        type_info_t func_type = make_type_info($1, $2);
        $$ = create_function($2.name, func_type, $4.nodes, $4.count, $6);
    }
    | type_specifier declarator LPAREN RPAREN compound_statement {
        type_info_t func_type = make_type_info($1, $2);
        $$ = create_function($2.name, func_type, NULL, 0, $5);
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
    type_specifier declarator {
        type_info_t param_type = make_type_info($1, $2);
        $$ = create_parameter(param_type, $2.name);
    }
    ;

type_specifier:
    INT { 
        $$ = create_type_info(string_duplicate("int"), 0, 0, NULL); 
    }
    | CHAR { 
        $$ = create_type_info(string_duplicate("char"), 0, 0, NULL); 
    }
    | VOID { 
        $$ = create_type_info(string_duplicate("void"), 0, 0, NULL); 
    }
    ;

declarator:
    pointer_declarator
    | direct_declarator
    ;

pointer_declarator:
    pointer direct_declarator {
        $$ = $2;
        $$.pointer_level = $1;
    }
    ;

pointer:
    MULTIPLY { $$ = 1; }
    | pointer MULTIPLY { $$ = $1 + 1; }
    ;

direct_declarator:
    IDENTIFIER {
        $$ = make_declarator($1, 0, 0, NULL);
    }
    | direct_declarator LBRACKET RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = NULL;
    }
    | direct_declarator LBRACKET expression RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = $3;
    }
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
    type_specifier declarator SEMICOLON {
        type_info_t var_type = make_type_info($1, $2);
        if ($2.is_array) {
            $$ = create_array_declaration(var_type, $2.name, $2.array_size);
        } else {
            $$ = create_declaration(var_type, $2.name, NULL);
        }
    }
    | type_specifier declarator ASSIGN expression SEMICOLON {
        type_info_t var_type = make_type_info($1, $2);
        if ($2.is_array) {
            // Arrays with initializers - for simplicity, ignore initializer for now
            $$ = create_array_declaration(var_type, $2.name, $2.array_size);
        } else {
            $$ = create_declaration(var_type, $2.name, $4);
        }
    }
    | type_specifier declarator ASSIGN LBRACE argument_list RBRACE SEMICOLON {
        type_info_t var_type = make_type_info($1, $2);
        // Array initialization - for simplicity, ignore initializer for now
        if ($2.is_array) {
            $$ = create_array_declaration(var_type, $2.name, $2.array_size);
        } else {
            $$ = create_declaration(var_type, $2.name, NULL);
        }
    }
    ;

assignment_statement:
    IDENTIFIER ASSIGN expression SEMICOLON {
        $$ = create_assignment($1, $3);
    }
    | expression LBRACKET expression RBRACKET ASSIGN expression SEMICOLON {
        // Handle array[index] = value
        ast_node_t *array_access = create_array_access($1, $3);
        $$ = create_assignment_to_lvalue(array_access, $6);
    }
    | MULTIPLY primary_expression ASSIGN expression SEMICOLON {
        // Handle *pointer = value
        ast_node_t *deref = create_dereference($2);
        $$ = create_assignment_to_lvalue(deref, $4);
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
    // Arithmetic operators
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
    // Comparison operators
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
    // Shift operators
    | expression LSHIFT expression {
        $$ = create_binary_op(OP_LSHIFT, $1, $3);
    }
    | expression RSHIFT expression {
        $$ = create_binary_op(OP_RSHIFT, $1, $3);
    }
    // Bitwise operators
    | expression AMPERSAND expression {
        $$ = create_binary_op(OP_BAND, $1, $3);
    }
    | expression PIPE expression {
        $$ = create_binary_op(OP_BOR, $1, $3);
    }
    | expression CARET expression {
        $$ = create_binary_op(OP_BXOR, $1, $3);
    }
    // Logical operators
    | expression LAND expression {
        $$ = create_binary_op(OP_LAND, $1, $3);
    }
    | expression LOR expression {
        $$ = create_binary_op(OP_LOR, $1, $3);
    }
    // Unary operators
    | MINUS expression %prec UMINUS {
        $$ = create_unary_op(OP_NEG, $2);
    }
    | NOT expression {
        $$ = create_unary_op(OP_NOT, $2);
    }
    | TILDE expression %prec UTILDE {
        $$ = create_unary_op(OP_BNOT, $2);
    }
    | AMPERSAND expression %prec UAMPERSAND {
        $$ = create_address_of($2);
    }
    | MULTIPLY expression %prec USTAR {
        $$ = create_dereference($2);
    }
    | expression LBRACKET expression RBRACKET {
        $$ = create_array_access($1, $3);
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
