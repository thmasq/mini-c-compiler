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
    AST_ASSIGN_EXPR,
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
    AST_ARRAY_DECL,
    AST_ARRAY_INDEX,
    AST_ADDRESS_OF,
    AST_DEREFERENCE,
    AST_POINTER_TYPE,
    AST_ARRAY_TYPE
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

// Type information structure
typedef struct type_info {
    char *base_type;
    int pointer_level;
    int is_array;
    struct ast_node *array_size;
    struct type_info *element_type;
} type_info_t;

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
            type_info_t *return_type;
            struct ast_node **params;
            int param_count;
            struct ast_node *body;
        } function;
        
        struct {
            struct ast_node **statements;
            int stmt_count;
        } compound;
        
        struct {
            type_info_t *type;
            char *name;
            struct ast_node *init;
        } declaration;
        
        struct {
            char *name;
            struct ast_node *value;
        } assignment;
        
        struct {
            struct ast_node *lvalue;
            struct ast_node *rvalue;
        } assign_expr;
        
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
            type_info_t *type;
            char *name;
        } parameter;
        
        struct {
            struct ast_node *expr;
        } expr_stmt;
        
        struct {
            type_info_t *type;
            char *name;
            struct ast_node *size;
        } array_decl;
        
        struct {
            struct ast_node *array;
            struct ast_node *index;
        } array_index;
        
        struct {
            struct ast_node *operand;
        } address_of;
        
        struct {
            struct ast_node *operand;
        } dereference;
        
        struct {
            type_info_t *base_type;
        } pointer_type;
        
        struct {
            type_info_t *element_type;
            struct ast_node *size;
        } array_type;
    } data;
} ast_node_t;

// Type creation functions
type_info_t *create_basic_type(const char *type_name);
type_info_t *create_pointer_type(type_info_t *base_type);
type_info_t *create_array_type(type_info_t *element_type, ast_node_t *size);
type_info_t *copy_type_info(type_info_t *original);
void free_type_info(type_info_t *type);
char *type_to_string(type_info_t *type);
char *type_to_llvm_string(type_info_t *type);

// Function prototypes
ast_node_t *create_program(ast_node_t **functions, int func_count);
ast_node_t *create_function(char *name, type_info_t *return_type, ast_node_t **params, int param_count, ast_node_t *body);
ast_node_t *create_compound_stmt(ast_node_t **statements, int stmt_count);
ast_node_t *create_declaration(type_info_t *type, char *name, ast_node_t *init);
ast_node_t *create_assignment(char *name, ast_node_t *value);
ast_node_t *create_assign_expr(ast_node_t *lvalue, ast_node_t *rvalue);
ast_node_t *create_if_stmt(ast_node_t *condition, ast_node_t *then_stmt, ast_node_t *else_stmt);
ast_node_t *create_while_stmt(ast_node_t *condition, ast_node_t *body);
ast_node_t *create_return_stmt(ast_node_t *value);
ast_node_t *create_call(char *name, ast_node_t **args, int arg_count);
ast_node_t *create_binary_op(binary_op_t op, ast_node_t *left, ast_node_t *right);
ast_node_t *create_unary_op(unary_op_t op, ast_node_t *operand);
ast_node_t *create_identifier(char *name);
ast_node_t *create_number(int value);
ast_node_t *create_parameter(type_info_t *type, char *name);
ast_node_t *create_expr_stmt(ast_node_t *expr);

// New creation functions for pointers and arrays
ast_node_t *create_array_decl(type_info_t *type, char *name, ast_node_t *size);
ast_node_t *create_array_index(ast_node_t *array, ast_node_t *index);
ast_node_t *create_address_of(ast_node_t *operand);
ast_node_t *create_dereference(ast_node_t *operand);

void free_ast(ast_node_t *node);
void generate_llvm_ir(ast_node_t *ast, FILE *output);

extern FILE *yyin;
extern int yyparse();
extern ast_node_t *ast_root;

#endif
