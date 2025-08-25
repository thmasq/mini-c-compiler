#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AST Node Types
typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_COMPOUND_STMT,
    AST_DECLARATION,
    AST_ASSIGNMENT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_RETURN_STMT,
    AST_CALL,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_IDENTIFIER,
    AST_NUMBER,
    AST_PARAMETER,
    AST_EXPR_STMT
} ast_node_type_t;

// Binary operators
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE
} binary_op_t;

// Unary operators
typedef enum {
    OP_NEG, OP_NOT
} unary_op_t;

// Forward declaration
struct ast_node;

// AST Node structure
typedef struct ast_node {
    ast_node_type_t type;
    union {
        struct {
            struct ast_node **functions;
            int func_count;
        } program;
        
        struct {
            char *name;
            char *return_type;
            struct ast_node **params;
            int param_count;
            struct ast_node *body;
        } function;
        
        struct {
            struct ast_node **statements;
            int stmt_count;
        } compound;
        
        struct {
            char *type;
            char *name;
            struct ast_node *init;
        } declaration;
        
        struct {
            char *name;
            struct ast_node *value;
        } assignment;
        
        struct {
            struct ast_node *condition;
            struct ast_node *then_stmt;
            struct ast_node *else_stmt;
        } if_stmt;
        
        struct {
            struct ast_node *condition;
            struct ast_node *body;
        } while_stmt;
        
        struct {
            struct ast_node *value;
        } return_stmt;
        
        struct {
            char *name;
            struct ast_node **args;
            int arg_count;
        } call;
        
        struct {
            binary_op_t op;
            struct ast_node *left;
            struct ast_node *right;
        } binary_op;
        
        struct {
            unary_op_t op;
            struct ast_node *operand;
        } unary_op;
        
        struct {
            char *name;
        } identifier;
        
        struct {
            int value;
        } number;
        
        struct {
            char *type;
            char *name;
        } parameter;
        
        struct {
            struct ast_node *expr;
        } expr_stmt;
    } data;
} ast_node_t;

// Function prototypes
ast_node_t *create_program(ast_node_t **functions, int func_count);
ast_node_t *create_function(char *name, char *return_type, ast_node_t **params, int param_count, ast_node_t *body);
ast_node_t *create_compound_stmt(ast_node_t **statements, int stmt_count);
ast_node_t *create_declaration(char *type, char *name, ast_node_t *init);
ast_node_t *create_assignment(char *name, ast_node_t *value);
ast_node_t *create_if_stmt(ast_node_t *condition, ast_node_t *then_stmt, ast_node_t *else_stmt);
ast_node_t *create_while_stmt(ast_node_t *condition, ast_node_t *body);
ast_node_t *create_return_stmt(ast_node_t *value);
ast_node_t *create_call(char *name, ast_node_t **args, int arg_count);
ast_node_t *create_binary_op(binary_op_t op, ast_node_t *left, ast_node_t *right);
ast_node_t *create_unary_op(unary_op_t op, ast_node_t *operand);
ast_node_t *create_identifier(char *name);
ast_node_t *create_number(int value);
ast_node_t *create_parameter(char *type, char *name);
ast_node_t *create_expr_stmt(ast_node_t *expr);

void free_ast(ast_node_t *node);
void generate_llvm_ir(ast_node_t *ast, FILE *output);

extern FILE *yyin;
extern int yyparse();
extern ast_node_t *ast_root;

#endif
