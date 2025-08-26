#define _POSIX_C_SOURCE 200809L
#include "ast.h"

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

// Type system functions
type_info_t *create_basic_type(const char *type_name) {
    type_info_t *type = malloc(sizeof(type_info_t));
    if (!type) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    type->base_type = string_duplicate(type_name);
    type->pointer_level = 0;
    type->is_array = 0;
    type->array_size = NULL;
    type->element_type = NULL;
    return type;
}

type_info_t *create_pointer_type(type_info_t *base_type) {
    type_info_t *type = malloc(sizeof(type_info_t));
    if (!type) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    // Copy the base type info and increment pointer level
    type->base_type = string_duplicate(base_type->base_type);
    type->pointer_level = base_type->pointer_level + 1;
    type->is_array = 0;
    type->array_size = NULL;
    type->element_type = base_type;
    return type;
}

type_info_t *create_array_type(type_info_t *element_type, ast_node_t *size) {
    type_info_t *type = malloc(sizeof(type_info_t));
    if (!type) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    type->base_type = string_duplicate(element_type->base_type);
    type->pointer_level = element_type->pointer_level;
    type->is_array = 1;
    type->array_size = size;
    type->element_type = element_type;
    return type;
}

void free_type_info(type_info_t *type) {
    if (!type) return;
    
    free(type->base_type);
    if (type->element_type && type->element_type != type) {
        free_type_info(type->element_type);
    }
    free(type);
}

// Deep copy function for type_info_t
type_info_t *copy_type_info(type_info_t *original) {
    if (!original) return NULL;
    
    type_info_t *copy = malloc(sizeof(type_info_t));
    if (!copy) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    copy->base_type = string_duplicate(original->base_type);
    copy->pointer_level = original->pointer_level;
    copy->is_array = original->is_array;
    copy->array_size = original->array_size;
    
    // Deep copy element_type to avoid double-free
    copy->element_type = copy_type_info(original->element_type);
    
    return copy;
}

char *type_to_string(type_info_t *type) {
    if (!type) return string_duplicate("void");
    
    char *result = malloc(256);
    if (!result) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    strcpy(result, type->base_type);
    
    // Add pointer asterisks
    for (int i = 0; i < type->pointer_level; i++) {
        strcat(result, "*");
    }
    
    if (type->is_array) {
        strcat(result, "[]");
    }
    
    return result;
}

char *type_to_llvm_string(type_info_t *type) {
    if (!type) return string_duplicate("void");
    
    char *result = malloc(256);
    if (!result) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    // Convert base type
    if (strcmp(type->base_type, "int") == 0) {
        strcpy(result, "i32");
    } else if (strcmp(type->base_type, "char") == 0) {
        strcpy(result, "i8");
    } else if (strcmp(type->base_type, "void") == 0) {
        strcpy(result, "void");
    } else {
        strcpy(result, "i32");
    }
    
    // Add pointer asterisks
    for (int i = 0; i < type->pointer_level; i++) {
        strcat(result, "*");
    }
    
    return result;
}

ast_node_t *create_program(ast_node_t **functions, int func_count) {
    ast_node_t *node = create_node(AST_PROGRAM);
    node->data.program.functions = functions;
    node->data.program.func_count = func_count;
    return node;
}

ast_node_t *create_function(char *name, type_info_t *return_type, ast_node_t **params, int param_count, ast_node_t *body) {
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

ast_node_t *create_declaration(type_info_t *type, char *name, ast_node_t *init) {
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

ast_node_t *create_assign_expr(ast_node_t *lvalue, ast_node_t *rvalue) {
    ast_node_t *node = create_node(AST_ASSIGN_EXPR);
    node->data.assign_expr.lvalue = lvalue;
    node->data.assign_expr.rvalue = rvalue;
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

ast_node_t *create_parameter(type_info_t *type, char *name) {
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

// New creation functions for pointers and arrays
ast_node_t *create_array_decl(type_info_t *type, char *name, ast_node_t *size) {
    ast_node_t *node = create_node(AST_ARRAY_DECL);
    node->data.array_decl.type = type;
    node->data.array_decl.name = name;
    node->data.array_decl.size = size;
    return node;
}

ast_node_t *create_array_index(ast_node_t *array, ast_node_t *index) {
    ast_node_t *node = create_node(AST_ARRAY_INDEX);
    node->data.array_index.array = array;
    node->data.array_index.index = index;
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
            free_type_info(node->data.function.return_type);
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
            free_type_info(node->data.declaration.type);
            free(node->data.declaration.name);
            free_ast(node->data.declaration.init);
            break;
            
        case AST_ASSIGNMENT:
            free(node->data.assignment.name);
            free_ast(node->data.assignment.value);
            break;
            
        case AST_ASSIGN_EXPR:
            free_ast(node->data.assign_expr.lvalue);
            free_ast(node->data.assign_expr.rvalue);
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
            free_type_info(node->data.parameter.type);
            free(node->data.parameter.name);
            break;
            
        case AST_EXPR_STMT:
            free_ast(node->data.expr_stmt.expr);
            break;
            
        // New cases for pointers and arrays
        case AST_ARRAY_DECL:
            free_type_info(node->data.array_decl.type);
            free(node->data.array_decl.name);
            free_ast(node->data.array_decl.size);
            break;
            
        case AST_ARRAY_INDEX:
            free_ast(node->data.array_index.array);
            free_ast(node->data.array_index.index);
            break;
            
        case AST_ADDRESS_OF:
            free_ast(node->data.address_of.operand);
            break;
            
        case AST_DEREFERENCE:
            free_ast(node->data.dereference.operand);
            break;
            
        case AST_POINTER_TYPE:
            free_type_info(node->data.pointer_type.base_type);
            break;
            
        case AST_ARRAY_TYPE:
            free_type_info(node->data.array_type.element_type);
            free_ast(node->data.array_type.size);
            break;
            
        case AST_NUMBER:
            // No dynamic memory to free
            break;
    }
    
    free(node);
}
