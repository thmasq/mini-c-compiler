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
    AST_EXPR_STMT,
    AST_ADDRESS_OF,
    AST_DEREFERENCE,
    AST_ARRAY_ACCESS,
    AST_ARRAY_DECL
} ast_node_type_t;

// Binary operators
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE
} binary_op_t;

// Unary operators
typedef enum {
    OP_NEG, OP_NOT
} unary_op_t;

// Forward declaration
struct ast_node;

// Structure to hold declarator information (used in parser)
typedef struct {
    char *name;
    int pointer_level;
    int is_array;
    struct ast_node *array_size;
} declarator_t;

// Type information structure
typedef struct {
    char *base_type;
    int pointer_level;
    int is_array;
    int is_vla;
    struct ast_node *array_size;
} type_info_t;

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
            type_info_t return_type;
            struct ast_node **params;
            int param_count;
            struct ast_node *body;
        } function;
        
        struct {
            struct ast_node **statements;
            int stmt_count;
        } compound;
        
        struct {
            type_info_t type_info;
            char *name;
            struct ast_node *init;
        } declaration;
        
        struct {
            char *name;
            struct ast_node *lvalue;
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
            type_info_t type_info;
            char *name;
        } parameter;
        
        struct {
            struct ast_node *expr;
        } expr_stmt;
        
        struct {
            struct ast_node *operand;
        } address_of;
        
        struct {
            struct ast_node *operand;
        } dereference;
        
        struct {
            struct ast_node *array;
            struct ast_node *index;
        } array_access;
        
        struct {
            type_info_t type_info;
            char *name;
            struct ast_node *size;
        } array_decl;
    } data;
} ast_node_t;

// Type creation helper
type_info_t create_type_info(char *base_type, int pointer_level, int is_array, ast_node_t *array_size);

// Helper for creating declarators (used in parser)
declarator_t make_declarator(char *name, int pointer_level, int is_array, ast_node_t *array_size);

// Function prototypes
ast_node_t *create_program(ast_node_t **functions, int func_count);
ast_node_t *create_function(char *name, type_info_t return_type, ast_node_t **params, int param_count, ast_node_t *body);
ast_node_t *create_compound_stmt(ast_node_t **statements, int stmt_count);
ast_node_t *create_declaration(type_info_t type_info, char *name, ast_node_t *init);
ast_node_t *create_assignment(char *name, ast_node_t *value);
ast_node_t *create_assignment_to_lvalue(ast_node_t *lvalue, ast_node_t *value);
ast_node_t *create_if_stmt(ast_node_t *condition, ast_node_t *then_stmt, ast_node_t *else_stmt);
ast_node_t *create_while_stmt(ast_node_t *condition, ast_node_t *body);
ast_node_t *create_return_stmt(ast_node_t *value);
ast_node_t *create_call(char *name, ast_node_t **args, int arg_count);
ast_node_t *create_binary_op(binary_op_t op, ast_node_t *left, ast_node_t *right);
ast_node_t *create_unary_op(unary_op_t op, ast_node_t *operand);
ast_node_t *create_identifier(char *name);
ast_node_t *create_number(int value);
ast_node_t *create_parameter(type_info_t type_info, char *name);
ast_node_t *create_expr_stmt(ast_node_t *expr);
ast_node_t *create_address_of(ast_node_t *operand);
ast_node_t *create_dereference(ast_node_t *operand);
ast_node_t *create_array_access(ast_node_t *array, ast_node_t *index);
ast_node_t *create_array_declaration(type_info_t type_info, char *name, ast_node_t *size);

void free_ast(ast_node_t *node);
void free_type_info(type_info_t *type_info);
void generate_llvm_ir(ast_node_t *ast, FILE *output);

extern FILE *yyin;
extern int yyparse();
extern ast_node_t *ast_root;

#endif
