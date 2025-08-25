#define _POSIX_C_SOURCE 200809L
#include "ast.h"

// Helper function to create a new AST node
static ast_node_t *create_node(ast_node_type_t type) {
    ast_node_t *node = malloc(sizeof(ast_node_t));
    if (!node) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    node->type = type;
    return node;
}

ast_node_t *create_program(ast_node_t **functions, int func_count) {
    ast_node_t *node = create_node(AST_PROGRAM);
    node->data.program.functions = functions;
    node->data.program.func_count = func_count;
    return node;
}

ast_node_t *create_function(char *name, char *return_type, ast_node_t **params, int param_count, ast_node_t *body) {
    ast_node_t *node = create_node(AST_FUNCTION);
    node->data.function.name = name;
    node->data.function.return_type = return_type;
    node->data.function.params = params;
    node->data.function.param_count = param_count;
    node->data.function.body = body;
    return node;
}

ast_node_t *create_compound_stmt(ast_node_t **statements, int stmt_count) {
    ast_node_t *node = create_node(AST_COMPOUND_STMT);
    node->data.compound.statements = statements;
    node->data.compound.stmt_count = stmt_count;
    return node;
}

ast_node_t *create_declaration(char *type, char *name, ast_node_t *init) {
    ast_node_t *node = create_node(AST_DECLARATION);
    node->data.declaration.type = type;
    node->data.declaration.name = name;
    node->data.declaration.init = init;
    return node;
}

ast_node_t *create_assignment(char *name, ast_node_t *value) {
    ast_node_t *node = create_node(AST_ASSIGNMENT);
    node->data.assignment.name = name;
    node->data.assignment.value = value;
    return node;
}

ast_node_t *create_if_stmt(ast_node_t *condition, ast_node_t *then_stmt, ast_node_t *else_stmt) {
    ast_node_t *node = create_node(AST_IF_STMT);
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_stmt = then_stmt;
    node->data.if_stmt.else_stmt = else_stmt;
    return node;
}

ast_node_t *create_while_stmt(ast_node_t *condition, ast_node_t *body) {
    ast_node_t *node = create_node(AST_WHILE_STMT);
    node->data.while_stmt.condition = condition;
    node->data.while_stmt.body = body;
    return node;
}

ast_node_t *create_return_stmt(ast_node_t *value) {
    ast_node_t *node = create_node(AST_RETURN_STMT);
    node->data.return_stmt.value = value;
    return node;
}

ast_node_t *create_call(char *name, ast_node_t **args, int arg_count) {
    ast_node_t *node = create_node(AST_CALL);
    node->data.call.name = name;
    node->data.call.args = args;
    node->data.call.arg_count = arg_count;
    return node;
}

ast_node_t *create_binary_op(binary_op_t op, ast_node_t *left, ast_node_t *right) {
    ast_node_t *node = create_node(AST_BINARY_OP);
    node->data.binary_op.op = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    return node;
}

ast_node_t *create_unary_op(unary_op_t op, ast_node_t *operand) {
    ast_node_t *node = create_node(AST_UNARY_OP);
    node->data.unary_op.op = op;
    node->data.unary_op.operand = operand;
    return node;
}

ast_node_t *create_identifier(char *name) {
    ast_node_t *node = create_node(AST_IDENTIFIER);
    node->data.identifier.name = name;
    return node;
}

ast_node_t *create_number(int value) {
    ast_node_t *node = create_node(AST_NUMBER);
    node->data.number.value = value;
    return node;
}

ast_node_t *create_parameter(char *type, char *name) {
    ast_node_t *node = create_node(AST_PARAMETER);
    node->data.parameter.type = type;
    node->data.parameter.name = name;
    return node;
}

ast_node_t *create_expr_stmt(ast_node_t *expr) {
    ast_node_t *node = create_node(AST_EXPR_STMT);
    node->data.expr_stmt.expr = expr;
    return node;
}

void free_ast(ast_node_t *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_PROGRAM:
            for (int i = 0; i < node->data.program.func_count; i++) {
                free_ast(node->data.program.functions[i]);
            }
            free(node->data.program.functions);
            break;
            
        case AST_FUNCTION:
            free(node->data.function.name);
            free(node->data.function.return_type);
            for (int i = 0; i < node->data.function.param_count; i++) {
                free_ast(node->data.function.params[i]);
            }
            free(node->data.function.params);
            free_ast(node->data.function.body);
            break;
            
        case AST_COMPOUND_STMT:
            for (int i = 0; i < node->data.compound.stmt_count; i++) {
                free_ast(node->data.compound.statements[i]);
            }
            free(node->data.compound.statements);
            break;
            
        case AST_DECLARATION:
            free(node->data.declaration.type);
            free(node->data.declaration.name);
            free_ast(node->data.declaration.init);
            break;
            
        case AST_ASSIGNMENT:
            free(node->data.assignment.name);
            free_ast(node->data.assignment.value);
            break;
            
        case AST_IF_STMT:
            free_ast(node->data.if_stmt.condition);
            free_ast(node->data.if_stmt.then_stmt);
            free_ast(node->data.if_stmt.else_stmt);
            break;
            
        case AST_WHILE_STMT:
            free_ast(node->data.while_stmt.condition);
            free_ast(node->data.while_stmt.body);
            break;
            
        case AST_RETURN_STMT:
            free_ast(node->data.return_stmt.value);
            break;
            
        case AST_CALL:
            free(node->data.call.name);
            for (int i = 0; i < node->data.call.arg_count; i++) {
                free_ast(node->data.call.args[i]);
            }
            free(node->data.call.args);
            break;
            
        case AST_BINARY_OP:
            free_ast(node->data.binary_op.left);
            free_ast(node->data.binary_op.right);
            break;
            
        case AST_UNARY_OP:
            free_ast(node->data.unary_op.operand);
            break;
            
        case AST_IDENTIFIER:
            free(node->data.identifier.name);
            break;
            
        case AST_PARAMETER:
            free(node->data.parameter.type);
            free(node->data.parameter.name);
            break;
            
        case AST_EXPR_STMT:
            free_ast(node->data.expr_stmt.expr);
            break;
            
        case AST_NUMBER:
            // No dynamic memory to free
            break;
    }
    
    free(node);
}
