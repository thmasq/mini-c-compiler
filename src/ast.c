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

type_info_t create_type_info(char *base_type, int pointer_level, int is_array, ast_node_t *array_size) {
    type_info_t type_info;
    type_info.base_type = base_type;
    type_info.pointer_level = pointer_level;
    type_info.is_array = is_array;
    type_info.array_size = array_size;
    type_info.is_vla = (is_array && array_size && array_size->type != AST_NUMBER);
    return type_info;
}

declarator_t make_declarator(char *name, int pointer_level, int is_array, ast_node_t *array_size) {
    declarator_t decl;
    decl.name = name;
    decl.pointer_level = pointer_level;
    decl.is_array = is_array;
    decl.array_size = array_size;
    return decl;
}

ast_node_t *create_program(ast_node_t **functions, int func_count) {
    ast_node_t *node = create_node(AST_PROGRAM);
    node->data.program.functions = functions;
    node->data.program.func_count = func_count;
    return node;
}

ast_node_t *create_function(char *name, type_info_t return_type, ast_node_t **params, int param_count, ast_node_t *body) {
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

ast_node_t *create_declaration(type_info_t type_info, char *name, ast_node_t *init) {
    ast_node_t *node = create_node(AST_DECLARATION);
    node->data.declaration.type_info = type_info;
    node->data.declaration.name = name;
    node->data.declaration.init = init;
    return node;
}

ast_node_t *create_assignment(char *name, ast_node_t *value) {
    ast_node_t *node = create_node(AST_ASSIGNMENT);
    node->data.assignment.name = name;
    node->data.assignment.lvalue = NULL;
    node->data.assignment.value = value;
    return node;
}

ast_node_t *create_assignment_to_lvalue(ast_node_t *lvalue, ast_node_t *value) {
    ast_node_t *node = create_node(AST_ASSIGNMENT);
    node->data.assignment.name = NULL;
    node->data.assignment.lvalue = lvalue;
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

ast_node_t *create_parameter(type_info_t type_info, char *name) {
    ast_node_t *node = create_node(AST_PARAMETER);
    node->data.parameter.type_info = type_info;
    node->data.parameter.name = name;
    return node;
}

ast_node_t *create_expr_stmt(ast_node_t *expr) {
    ast_node_t *node = create_node(AST_EXPR_STMT);
    node->data.expr_stmt.expr = expr;
    return node;
}

ast_node_t *create_address_of(ast_node_t *operand) {
    ast_node_t *node = create_node(AST_ADDRESS_OF);
    node->data.address_of.operand = operand;
    return node;
}

ast_node_t *create_dereference(ast_node_t *operand) {
    ast_node_t *node = create_node(AST_DEREFERENCE);
    node->data.dereference.operand = operand;
    return node;
}

ast_node_t *create_array_access(ast_node_t *array, ast_node_t *index) {
    ast_node_t *node = create_node(AST_ARRAY_ACCESS);
    node->data.array_access.array = array;
    node->data.array_access.index = index;
    return node;
}

ast_node_t *create_array_declaration(type_info_t type_info, char *name, ast_node_t *size) {
    ast_node_t *node = create_node(AST_ARRAY_DECL);
    node->data.array_decl.type_info = type_info;
    node->data.array_decl.name = name;
    node->data.array_decl.size = size;
    return node;
}

void free_type_info(type_info_t *type_info) {
    if (type_info->base_type) {
        free(type_info->base_type);
        type_info->base_type = NULL;
    }
    if (type_info->array_size) {
        free_ast(type_info->array_size);
        type_info->array_size = NULL;
    }
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
            free_type_info(&node->data.function.return_type);
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
            free_type_info(&node->data.declaration.type_info);
            free(node->data.declaration.name);
            free_ast(node->data.declaration.init);
            break;
            
        case AST_ASSIGNMENT:
            free(node->data.assignment.name);
            free_ast(node->data.assignment.lvalue);
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
            free_type_info(&node->data.parameter.type_info);
            free(node->data.parameter.name);
            break;
            
        case AST_EXPR_STMT:
            free_ast(node->data.expr_stmt.expr);
            break;
            
        case AST_ADDRESS_OF:
            free_ast(node->data.address_of.operand);
            break;
            
        case AST_DEREFERENCE:
            free_ast(node->data.dereference.operand);
            break;
            
        case AST_ARRAY_ACCESS:
            free_ast(node->data.array_access.array);
            free_ast(node->data.array_access.index);
            break;
            
        case AST_ARRAY_DECL:
            free_type_info(&node->data.array_decl.type_info);
            free(node->data.array_decl.name);
            free_ast(node->data.array_decl.size);
            break;
            
        case AST_NUMBER:
            // No dynamic memory to free
            break;
    }
    
    free(node);
}
