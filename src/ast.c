#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include "symbol_table.h"

// Helper function to create a new AST node
static ast_node_t *create_node(ast_node_type_t type) {
    ast_node_t *node = malloc(sizeof(ast_node_t));
    if (!node) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    node->type = type;
    node->line_number = 0;
    node->column = 0;
    return node;
}

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

// Type info creation and management
type_info_t create_type_info(char *base_type, int pointer_level, int is_array, ast_node_t *array_size) {
    type_info_t type_info;
    type_info.base_type = base_type;
    type_info.pointer_level = pointer_level;
    type_info.is_array = is_array;
    type_info.is_vla = (is_array && array_size && array_size->type != AST_NUMBER);
    type_info.is_function = 0;
    type_info.is_struct = 0;
    type_info.is_union = 0;
    type_info.is_enum = 0;
    type_info.is_incomplete = 0;
    type_info.storage_class = STORAGE_NONE;
    type_info.qualifiers = QUAL_NONE;
    type_info.array_size = array_size;
    type_info.param_types = NULL;
    type_info.param_count = 0;
    type_info.is_variadic = 0;
    return type_info;
}

declarator_t make_declarator(char *name, int pointer_level, int is_array, ast_node_t *array_size) {
    declarator_t decl;
    decl.name = name;
    decl.pointer_level = pointer_level;
    decl.is_array = is_array;
    decl.is_function = 0;
    decl.array_size = array_size;
    decl.params = NULL;
    decl.param_count = 0;
    decl.is_variadic = 0;
    return decl;
}

// Program and function creation
ast_node_t *create_program(ast_node_t **declarations, int decl_count) {
    ast_node_t *node = create_node(AST_PROGRAM);
    node->data.program.declarations = declarations;
    node->data.program.decl_count = decl_count;
    return node;
}

ast_node_t *create_function(char *name, type_info_t return_type, ast_node_t **params, int param_count, ast_node_t *body) {
    ast_node_t *node = create_node(AST_FUNCTION);
    node->data.function.name = name;
    node->data.function.return_type = return_type;
    node->data.function.params = params;
    node->data.function.param_count = param_count;
    node->data.function.body = body;
    node->data.function.storage_class = return_type.storage_class;
    node->data.function.is_variadic = return_type.is_variadic;
    node->data.function.is_defined = (body != NULL);
    return node;
}

// Statement creation
ast_node_t *create_compound_stmt(ast_node_t **statements, int stmt_count) {
    ast_node_t *node = create_node(AST_COMPOUND_STMT);
    node->data.compound.statements = statements;
    node->data.compound.stmt_count = stmt_count;
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

ast_node_t *create_for_stmt(ast_node_t *init, ast_node_t *condition, ast_node_t *update, ast_node_t *body) {
    ast_node_t *node = create_node(AST_FOR_STMT);
    node->data.for_stmt.init = init;
    node->data.for_stmt.condition = condition;
    node->data.for_stmt.update = update;
    node->data.for_stmt.body = body;
    return node;
}

ast_node_t *create_do_while_stmt(ast_node_t *body, ast_node_t *condition) {
    ast_node_t *node = create_node(AST_DO_WHILE_STMT);
    node->data.do_while_stmt.body = body;
    node->data.do_while_stmt.condition = condition;
    return node;
}

ast_node_t *create_switch_stmt(ast_node_t *expression, ast_node_t *body) {
    ast_node_t *node = create_node(AST_SWITCH_STMT);
    node->data.switch_stmt.expression = expression;
    node->data.switch_stmt.body = body;
    node->data.switch_stmt.cases = NULL;
    node->data.switch_stmt.default_label = NULL;
    node->data.switch_stmt.break_label = NULL;
    return node;
}

ast_node_t *create_case_stmt(ast_node_t *value, ast_node_t *statement) {
    ast_node_t *node = create_node(AST_CASE_STMT);
    node->data.case_stmt.value = value;
    node->data.case_stmt.statement = statement;
    node->data.case_stmt.label_name = NULL;
    return node;
}

ast_node_t *create_default_stmt(ast_node_t *statement) {
    ast_node_t *node = create_node(AST_DEFAULT_STMT);
    node->data.default_stmt.statement = statement;
    node->data.default_stmt.label_name = NULL;
    return node;
}

ast_node_t *create_break_stmt(void) {
    return create_node(AST_BREAK_STMT);
}

ast_node_t *create_continue_stmt(void) {
    return create_node(AST_CONTINUE_STMT);
}

ast_node_t *create_goto_stmt(char *label) {
    ast_node_t *node = create_node(AST_GOTO_STMT);
    node->data.goto_stmt.label = label;
    return node;
}

ast_node_t *create_label_stmt(char *label, ast_node_t *statement) {
    ast_node_t *node = create_node(AST_LABEL_STMT);
    node->data.label_stmt.label = label;
    node->data.label_stmt.statement = statement;
    return node;
}

ast_node_t *create_return_stmt(ast_node_t *value) {
    ast_node_t *node = create_node(AST_RETURN_STMT);
    node->data.return_stmt.value = value;
    return node;
}

ast_node_t *create_expr_stmt(ast_node_t *expr) {
    ast_node_t *node = create_node(AST_EXPR_STMT);
    node->data.expr_stmt.expr = expr;
    return node;
}

ast_node_t *create_empty_stmt(void) {
    return create_node(AST_EMPTY_STMT);
}

// Declaration creation
ast_node_t *create_declaration(type_info_t type_info, char *name, ast_node_t *init) {
    if (type_info.is_array) {
        ast_node_t *node = create_node(AST_ARRAY_DECL);
        node->data.array_decl.type_info = type_info;         
        node->data.array_decl.name = name;
        node->data.array_decl.size = type_info.array_size;   
        node->data.array_decl.is_vla = type_info.is_vla ||
        (type_info.array_size && type_info.array_size->type != AST_NUMBER);
        return node;
    }

    ast_node_t *node = create_node(AST_DECLARATION);
    node->data.declaration.type_info = type_info;
    node->data.declaration.name = name;
    node->data.declaration.init = init;
    node->data.declaration.is_parameter = 0;
    return node;
}


ast_node_t *create_array_declaration(type_info_t type_info, char *name, ast_node_t *size) {
    ast_node_t *node = create_node(AST_ARRAY_DECL);
    node->data.array_decl.type_info = type_info;
    node->data.array_decl.name = name;
    node->data.array_decl.size = size;
    node->data.array_decl.is_vla = (size && size->type != AST_NUMBER);
    return node;
}

ast_node_t *create_struct_declaration(char *name, member_info_t *members, int is_definition) {
    ast_node_t *node = create_node(AST_STRUCT_DECL);
    node->data.struct_decl.name = name;
    node->data.struct_decl.members = members;
    node->data.struct_decl.member_count = 0;
    node->data.struct_decl.is_definition = is_definition;
    node->data.struct_decl.size = 0;
    node->data.struct_decl.alignment = 1;
    
    // Count members
    member_info_t *m = members;
    while (m) {
        node->data.struct_decl.member_count++;
        m = m->next;
    }
    
    return node;
}

ast_node_t *create_union_declaration(char *name, member_info_t *members, int is_definition) {
    ast_node_t *node = create_node(AST_UNION_DECL);
    node->data.union_decl.name = name;
    node->data.union_decl.members = members;
    node->data.union_decl.member_count = 0;
    node->data.union_decl.is_definition = is_definition;
    node->data.union_decl.size = 0;
    node->data.union_decl.alignment = 1;
    
    // Count members
    member_info_t *m = members;
    while (m) {
        node->data.union_decl.member_count++;
        m = m->next;
    }
    
    return node;
}

ast_node_t *create_enum_declaration(char *name, enum_value_t *values, int is_definition) {
    ast_node_t *node = create_node(AST_ENUM_DECL);
    node->data.enum_decl.name = name;
    node->data.enum_decl.values = values;
    node->data.enum_decl.value_count = 0;
    node->data.enum_decl.is_definition = is_definition;
    node->data.enum_decl.next_value = 0;
    
    // Count values
    enum_value_t *v = values;
    while (v) {
        node->data.enum_decl.value_count++;
        v = v->next;
    }
    
    return node;
}

ast_node_t *create_typedef(type_info_t type, char *name) {
    ast_node_t *node = create_node(AST_TYPEDEF);
    node->data.typedef_decl.type = type;
    node->data.typedef_decl.name = name;
    return node;
}

// Expression creation
ast_node_t *create_assignment(char *name, ast_node_t *value) {
    ast_node_t *node = create_node(AST_ASSIGNMENT);
    node->data.assignment.name = name;
    node->data.assignment.lvalue = NULL;
    node->data.assignment.value = value;
    node->data.assignment.op = OP_ASSIGN;
    return node;
}

ast_node_t *create_assignment_to_lvalue(ast_node_t *lvalue, ast_node_t *value) {
    ast_node_t *node = create_node(AST_ASSIGNMENT);
    node->data.assignment.name = NULL;
    node->data.assignment.lvalue = lvalue;
    node->data.assignment.value = value;
    node->data.assignment.op = OP_ASSIGN;
    return node;
}

ast_node_t *create_compound_assignment(ast_node_t *lvalue, binary_op_t op, ast_node_t *value) {
    ast_node_t *node = create_node(AST_ASSIGNMENT);
    node->data.assignment.name = NULL;
    node->data.assignment.lvalue = lvalue;
    node->data.assignment.value = value;
    node->data.assignment.op = op;
    return node;
}

ast_node_t *create_call(char *name, ast_node_t **args, int arg_count) {
    ast_node_t *node = create_node(AST_CALL);
    node->data.call.name = name;
    node->data.call.args = args;
    node->data.call.arg_count = arg_count;
    // Return type will be filled in during type checking
    node->data.call.return_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    return node;
}

ast_node_t *create_binary_op(binary_op_t op, ast_node_t *left, ast_node_t *right) {
    ast_node_t *node = create_node(AST_BINARY_OP);
    node->data.binary_op.op = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    // Result type will be filled in during type checking
    node->data.binary_op.result_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    return node;
}

ast_node_t *create_unary_op(unary_op_t op, ast_node_t *operand) {
    ast_node_t *node = create_node(AST_UNARY_OP);
    node->data.unary_op.op = op;
    node->data.unary_op.operand = operand;
    // Result type will be filled in during type checking
    node->data.unary_op.result_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    return node;
}

ast_node_t *create_conditional(ast_node_t *condition, ast_node_t *true_expr, ast_node_t *false_expr) {
    ast_node_t *node = create_node(AST_CONDITIONAL);
    node->data.conditional.condition = condition;
    node->data.conditional.true_expr = true_expr;
    node->data.conditional.false_expr = false_expr;
    // Result type will be filled in during type checking
    node->data.conditional.result_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    return node;
}

ast_node_t *create_cast(type_info_t target_type, ast_node_t *expression) {
    ast_node_t *node = create_node(AST_CAST);
    node->data.cast.target_type = target_type;
    node->data.cast.expression = expression;
    return node;
}

ast_node_t *create_sizeof_expr(ast_node_t *operand) {
    ast_node_t *node = create_node(AST_SIZEOF);
    node->data.sizeof_op.operand = operand;
    node->data.sizeof_op.is_type = 0;
    node->data.sizeof_op.size_value = 0; // Will be computed later
    return node;
}

ast_node_t *create_sizeof_type(type_info_t type) {
    ast_node_t *node = create_node(AST_SIZEOF);
    node->data.sizeof_op.operand = NULL;
    node->data.sizeof_op.is_type = 1;
    node->data.sizeof_op.size_value = 0; // Will be computed later
    return node;
}

// Primary expressions
ast_node_t *create_identifier(char *name) {
    ast_node_t *node = create_node(AST_IDENTIFIER);
    node->data.identifier.name = name;
    // Type will be filled in during symbol resolution
    node->data.identifier.type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    return node;
}

ast_node_t *create_number(int value) {
    ast_node_t *node = create_node(AST_NUMBER);
    node->data.number.value = value;
    return node;
}

ast_node_t *create_string_literal(char *value) {
    ast_node_t *node = create_node(AST_STRING_LITERAL);
    node->data.string_literal.value = value;
    node->data.string_literal.length = value ? strlen(value) : 0;
    return node;
}

ast_node_t *create_character(char value) {
    ast_node_t *node = create_node(AST_CHARACTER);
    node->data.character.value = value;
    return node;
}

ast_node_t *create_parameter(type_info_t type_info, char *name) {
    ast_node_t *node = create_node(AST_PARAMETER);
    node->data.parameter.type_info = type_info;
    node->data.parameter.name = name;
    return node;
}

// Pointer and array operations
ast_node_t *create_address_of(ast_node_t *operand) {
    ast_node_t *node = create_node(AST_ADDRESS_OF);
    node->data.address_of.operand = operand;
    // Result type will be filled in during type checking
    node->data.address_of.result_type = create_type_info(string_duplicate("int"), 1, 0, NULL);
    return node;
}

ast_node_t *create_dereference(ast_node_t *operand) {
    ast_node_t *node = create_node(AST_DEREFERENCE);
    node->data.dereference.operand = operand;
    // Result type will be filled in during type checking
    node->data.dereference.result_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    return node;
}

ast_node_t *create_array_access(ast_node_t *array, ast_node_t *index) {
    ast_node_t *node = create_node(AST_ARRAY_ACCESS);
    node->data.array_access.array = array;
    node->data.array_access.index = index;
    // Element type will be filled in during type checking
    node->data.array_access.element_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    return node;
}

ast_node_t *create_member_access(ast_node_t *object, char *member) {
    ast_node_t *node = create_node(AST_MEMBER_ACCESS);
    node->data.member_access.object = object;
    node->data.member_access.member = member;
    // Member type and offset will be filled in during type checking
    node->data.member_access.member_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    node->data.member_access.member_offset = 0;
    return node;
}

ast_node_t *create_ptr_member_access(ast_node_t *object, char *member) {
    ast_node_t *node = create_node(AST_PTR_MEMBER_ACCESS);
    node->data.ptr_member_access.object = object;
    node->data.ptr_member_access.member = member;
    // Member type and offset will be filled in during type checking
    node->data.ptr_member_access.member_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    node->data.ptr_member_access.member_offset = 0;
    return node;
}

// Initializers
ast_node_t *create_initializer_list(ast_node_t **values, int count) {
    ast_node_t *node = create_node(AST_INITIALIZER_LIST);
    node->data.initializer_list.values = values;
    node->data.initializer_list.count = count;
    // Element type will be determined during type checking
    node->data.initializer_list.element_type = create_type_info(string_duplicate("int"), 0, 0, NULL);
    return node;
}

// Helper functions for struct/union/enum
member_info_t *create_member_info(char *name, type_info_t type, int bit_field_size) {
    member_info_t *member = malloc(sizeof(member_info_t));
    if (!member) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    member->name = name;
    member->type = type;
    member->bit_field_size = bit_field_size;
    member->bit_field_expr = NULL;
    member->next = NULL;
    return member;
}

enum_value_t *create_enum_value(char *name, int value) {
    enum_value_t *enum_val = malloc(sizeof(enum_value_t));
    if (!enum_val) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    enum_val->name = name;
    enum_val->value = value;
    enum_val->value_expr = NULL;
    enum_val->next = NULL;
    return enum_val;
}

case_label_t *create_case_label(ast_node_t *value, char *label_name) {
    case_label_t *label = malloc(sizeof(case_label_t));
    if (!label) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    label->value = value;
    label->label_name = label_name;
    label->next = NULL;
    return label;
}

// Type system functions
type_info_t merge_declaration_specifiers(type_info_t base, declarator_t declarator) {
    type_info_t result = base;
    result.pointer_level += declarator.pointer_level;   
    result.is_array = declarator.is_array;
    result.is_function = declarator.is_function;
    result.array_size = declarator.array_size;
    result.is_vla = declarator.is_array && declarator.array_size && 
                    declarator.array_size->type != AST_NUMBER;
    
    if (declarator.is_function) {
        result.param_count = declarator.param_count;
        result.is_variadic = declarator.is_variadic;
    }
    
    return result;
}

int is_integer_type(type_info_t *type) {
    if (!type || !type->base_type) return 0;
    if (type->pointer_level > 0) return 0;
    if (type->is_array) return 0;
    
    return (strcmp(type->base_type, "char") == 0 ||
            strcmp(type->base_type, "short") == 0 ||
            strcmp(type->base_type, "int") == 0 ||
            strcmp(type->base_type, "long") == 0 ||
            strcmp(type->base_type, "_Bool") == 0 ||
            strstr(type->base_type, "signed") != NULL ||
            strstr(type->base_type, "unsigned") != NULL ||
            type->is_enum);
}

int is_floating_type(type_info_t *type) {
    if (!type || !type->base_type) return 0;
    if (type->pointer_level > 0) return 0;
    if (type->is_array) return 0;
    
    return (strcmp(type->base_type, "float") == 0 ||
            strcmp(type->base_type, "double") == 0);
}

int is_arithmetic_type(type_info_t *type) {
    return is_integer_type(type) || is_floating_type(type);
}

int is_pointer_type(type_info_t *type) {
    return type && type->pointer_level > 0;
}

int is_array_type(type_info_t *type) {
    return type && type->is_array;
}

int is_function_type(type_info_t *type) {
    return type && type->is_function;
}

int is_struct_type(type_info_t *type) {
    return type && type->is_struct;
}

int is_union_type(type_info_t *type) {
    return type && type->is_union;
}

int is_enum_type(type_info_t *type) {
    return type && type->is_enum;
}

// Type conversion and promotion
type_info_t perform_usual_arithmetic_conversions(type_info_t *type1, type_info_t *type2) {
    // Simplified version - promote to larger type
    if (is_floating_type(type1) || is_floating_type(type2)) {
        if (strcmp(type1->base_type, "double") == 0 || strcmp(type2->base_type, "double") == 0) {
            return create_type_info(string_duplicate("double"), 0, 0, NULL);
        }
        return create_type_info(string_duplicate("float"), 0, 0, NULL);
    }
    
    if (is_integer_type(type1) && is_integer_type(type2)) {
        // Promote to int or larger
        if (strcmp(type1->base_type, "long") == 0 || strcmp(type2->base_type, "long") == 0) {
            return create_type_info(string_duplicate("long"), 0, 0, NULL);
        }
        return create_type_info(string_duplicate("int"), 0, 0, NULL);
    }
    
    // Default to int
    return create_type_info(string_duplicate("int"), 0, 0, NULL);
}

type_info_t perform_integer_promotions(type_info_t *type) {
    if (!is_integer_type(type)) {
        return *type; // No promotion
    }
    
    // Promote char and short to int
    if (strcmp(type->base_type, "char") == 0 || strcmp(type->base_type, "short") == 0) {
        return create_type_info(string_duplicate("int"), 0, 0, NULL);
    }
    
    return *type; // No promotion needed
}

int can_convert_to(type_info_t *from, type_info_t *to) {
    // Simplified type conversion rules
    if (!from || !to) return 0;
    
    // Identical types
    if (from->pointer_level == to->pointer_level &&
        from->is_array == to->is_array &&
        strcmp(from->base_type, to->base_type) == 0) {
        return 1;
    }
    
    // Arithmetic conversions
    if (is_arithmetic_type(from) && is_arithmetic_type(to)) {
        return 1;
    }
    
    // Pointer conversions
    if (is_pointer_type(from) && is_pointer_type(to)) {
        // void* is compatible with any pointer
        if (strcmp(from->base_type, "void") == 0 || strcmp(to->base_type, "void") == 0) {
            return 1;
        }
        // Same pointed-to type
        return strcmp(from->base_type, to->base_type) == 0;
    }
    
    // Array to pointer decay
    if (is_array_type(from) && is_pointer_type(to)) {
        return strcmp(from->base_type, to->base_type) == 0;
    }
    
    return 0;
}

// Memory management functions
void free_type_info(type_info_t *type_info) {
    if (!type_info) return;
    
    if (type_info->base_type) {
        free(type_info->base_type);
        type_info->base_type = NULL;
    }
    if (type_info->param_types) {
        for (int i = 0; i < type_info->param_count; i++) {
            free_ast(type_info->param_types[i]);
        }
        free(type_info->param_types);
        type_info->param_types = NULL;
    }
    type_info->param_count = 0;
    // Don't free array_size - it's managed by AST
    type_info->array_size = NULL;
}

void free_member_info(member_info_t *member) {
    while (member) {
        member_info_t *next = member->next;
        free(member->name);
        free_type_info(&member->type);
        free_ast(member->bit_field_expr);
        free(member);
        member = next;
    }
}

void free_enum_value(enum_value_t *value) {
    while (value) {
        enum_value_t *next = value->next;
        free(value->name);
        free_ast(value->value_expr);
        free(value);
        value = next;
    }
}

void free_case_label(case_label_t *label) {
    while (label) {
        case_label_t *next = label->next;
        free_ast(label->value);
        free(label->label_name);
        free(label);
        label = next;
    }
}

void free_ast(ast_node_t *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_PROGRAM:
            for (int i = 0; i < node->data.program.decl_count; i++) {
                free_ast(node->data.program.declarations[i]);
            }
            free(node->data.program.declarations);
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
            
        case AST_FOR_STMT:
            free_ast(node->data.for_stmt.init);
            free_ast(node->data.for_stmt.condition);
            free_ast(node->data.for_stmt.update);
            free_ast(node->data.for_stmt.body);
            break;
            
        case AST_DO_WHILE_STMT:
            free_ast(node->data.do_while_stmt.body);
            free_ast(node->data.do_while_stmt.condition);
            break;
            
        case AST_SWITCH_STMT:
            free_ast(node->data.switch_stmt.expression);
            free_ast(node->data.switch_stmt.body);
            free_case_label(node->data.switch_stmt.cases);
            free(node->data.switch_stmt.default_label);
            free(node->data.switch_stmt.break_label);
            break;
            
        case AST_CASE_STMT:
            free_ast(node->data.case_stmt.value);
            free_ast(node->data.case_stmt.statement);
            free(node->data.case_stmt.label_name);
            break;
            
        case AST_DEFAULT_STMT:
            free_ast(node->data.default_stmt.statement);
            free(node->data.default_stmt.label_name);
            break;
            
        case AST_GOTO_STMT:
            free(node->data.goto_stmt.label);
            break;
            
        case AST_LABEL_STMT:
            free(node->data.label_stmt.label);
            free_ast(node->data.label_stmt.statement);
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
            free_type_info(&node->data.call.return_type);
            break;
            
        case AST_BINARY_OP:
            free_ast(node->data.binary_op.left);
            free_ast(node->data.binary_op.right);
            free_type_info(&node->data.binary_op.result_type);
            break;
            
        case AST_UNARY_OP:
            free_ast(node->data.unary_op.operand);
            free_type_info(&node->data.unary_op.result_type);
            break;
            
        case AST_CONDITIONAL:
            free_ast(node->data.conditional.condition);
            free_ast(node->data.conditional.true_expr);
            free_ast(node->data.conditional.false_expr);
            free_type_info(&node->data.conditional.result_type);
            break;
            
        case AST_CAST:
            free_type_info(&node->data.cast.target_type);
            free_ast(node->data.cast.expression);
            break;
            
        case AST_SIZEOF:
            if (!node->data.sizeof_op.is_type) {
                free_ast(node->data.sizeof_op.operand);
            }
            break;
            
        case AST_IDENTIFIER:
            free(node->data.identifier.name);
            free_type_info(&node->data.identifier.type);
            break;
            
        case AST_STRING_LITERAL:
            free(node->data.string_literal.value);
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
            free_type_info(&node->data.address_of.result_type);
            break;
            
        case AST_DEREFERENCE:
            free_ast(node->data.dereference.operand);
            free_type_info(&node->data.dereference.result_type);
            break;
            
        case AST_ARRAY_ACCESS:
            free_ast(node->data.array_access.array);
            free_ast(node->data.array_access.index);
            free_type_info(&node->data.array_access.element_type);
            break;
            
        case AST_ARRAY_DECL:
            free_type_info(&node->data.array_decl.type_info);
            free(node->data.array_decl.name);
            free_ast(node->data.array_decl.size);
            break;
            
        case AST_STRUCT_DECL:
            free(node->data.struct_decl.name);
            free_member_info(node->data.struct_decl.members);
            break;
            
        case AST_UNION_DECL:
            free(node->data.union_decl.name);
            free_member_info(node->data.union_decl.members);
            break;
            
        case AST_ENUM_DECL:
            free(node->data.enum_decl.name);
            free_enum_value(node->data.enum_decl.values);
            break;
            
        case AST_MEMBER_ACCESS:
            free_ast(node->data.member_access.object);
            free(node->data.member_access.member);
            free_type_info(&node->data.member_access.member_type);
            break;
            
        case AST_PTR_MEMBER_ACCESS:
            free_ast(node->data.ptr_member_access.object);
            free(node->data.ptr_member_access.member);
            free_type_info(&node->data.ptr_member_access.member_type);
            break;
            
        case AST_INITIALIZER_LIST:
            for (int i = 0; i < node->data.initializer_list.count; i++) {
                free_ast(node->data.initializer_list.values[i]);
            }
            free(node->data.initializer_list.values);
            free_type_info(&node->data.initializer_list.element_type);
            break;
            
        case AST_TYPEDEF:
            free_type_info(&node->data.typedef_decl.type);
            free(node->data.typedef_decl.name);
            break;
            
        case AST_NUMBER:
        case AST_CHARACTER:
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
        case AST_EMPTY_STMT:
        case AST_INCREMENT:
        case AST_DECREMENT:
            // No dynamic memory to free
            break;
    }
    
    free(node);
}

// Simplified type checking functions
int check_types(ast_node_t *ast, symbol_table_t *table) {
    if (!ast || !table) return 0;
    
    switch (ast->type) {
        case AST_PROGRAM:
            for (int i = 0; i < ast->data.program.decl_count; i++) {
                if (!check_types(ast->data.program.declarations[i], table)) {
                    return 0;
                }
            }
            return 1;
            
        case AST_FUNCTION:
            return check_types(ast->data.function.body, table);
            
        case AST_COMPOUND_STMT:
            for (int i = 0; i < ast->data.compound.stmt_count; i++) {
                if (!check_types(ast->data.compound.statements[i], table)) {
                    return 0;
                }
            }
            return 1;
            
        default:
            return check_expression_types(ast, table) && check_statement_types(ast, table);
    }
}

int check_expression_types(ast_node_t *expr, symbol_table_t *table) {
    if (!expr) return 1;
    
    // Simplified type checking - just verify identifiers exist
    if (expr->type == AST_IDENTIFIER) {
        symbol_t *sym = find_symbol(table, expr->data.identifier.name);
        if (!sym) {
            fprintf(stderr, "Undefined identifier: %s\n", expr->data.identifier.name);
            return 0;
        }
        expr->data.identifier.type = sym->type_info;
        return 1;
    }
    
    return 1; // For now, accept all other expressions
}

int check_statement_types(ast_node_t *stmt, symbol_table_t *table) {
    if (!stmt) return 1;
    
    // Simplified statement type checking
    return 1;
}

static const char* binop_str(binary_op_t op){
    switch(op){
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_EQ: return "==";
        case OP_NE: return "!=";
        case OP_LT: return "<";
        case OP_LE: return "<=";
        case OP_GT: return ">";
        case OP_GE: return ">=";
        case OP_LAND: return "&&";
        case OP_LOR: return "||";
        case OP_BAND: return "&";
        case OP_BOR:  return "|";
        case OP_BXOR: return "^";
        case OP_LSHIFT: return "<<";
        case OP_RSHIFT: return ">>";
        case OP_ASSIGN: return "=";
        case OP_ADD_ASSIGN: return "+=";
        case OP_SUB_ASSIGN: return "-=";
        case OP_MUL_ASSIGN: return "*=";
        case OP_DIV_ASSIGN: return "/=";
        case OP_MOD_ASSIGN: return "%=";
        case OP_LSHIFT_ASSIGN: return "<<=";
        case OP_RSHIFT_ASSIGN: return ">>=";
        case OP_BAND_ASSIGN: return "&=";
        case OP_BOR_ASSIGN:  return "|=";
        case OP_BXOR_ASSIGN: return "^=";
    }
    return "?";
}

static const char* unop_str(unary_op_t op){
    switch(op){
        case OP_NEG: return "neg";
        case OP_NOT: return "!";
        case OP_BNOT: return "~";
        case OP_PREINC: return "pre++";
        case OP_POSTINC: return "post++";
        case OP_PREDEC: return "pre--";
        case OP_POSTDEC: return "post--";
    }
    return "?";
}

static void indent_out(int n){
    for(int i=0;i<n;i++) putchar(' ');
}

static void print_ptr_stars(int n){
    for(int i=0;i<n;i++) putchar('*');
}

static void print_type_info(type_info_t t){
    const char *base = t.base_type ? t.base_type : "int";
    fputs(base, stdout);
    if (t.pointer_level > 0) print_ptr_stars(t.pointer_level);
}

void print_ast(ast_node_t *node, int indent){
    if(!node){ indent_out(indent); puts("(null)"); return; }

    switch(node->type){
        case AST_PROGRAM: {
            indent_out(indent); printf("Program (%d decls)\n", node->data.program.decl_count);
            for(int i=0;i<node->data.program.decl_count;i++)
                print_ast(node->data.program.declarations[i], indent+2);
        } break;

        case AST_FUNCTION: {
            indent_out(indent);
            printf("Function %s -> ", node->data.function.name ? node->data.function.name : "(anon)");
            print_type_info(node->data.function.return_type);
            putchar('\n');

            if(node->data.function.param_count>0){
                indent_out(indent+2); printf("Params (%d):\n", node->data.function.param_count);
                for(int i=0;i<node->data.function.param_count;i++)
                    print_ast(node->data.function.params[i], indent+4);
            }
            print_ast(node->data.function.body, indent+2);
        } break;

        case AST_PARAMETER: {
            indent_out(indent);
            printf("Param %s : ", node->data.parameter.name ? node->data.parameter.name : "(anon)");
            print_type_info(node->data.parameter.type_info);
            putchar('\n');
        } break;

        case AST_COMPOUND_STMT: {
            indent_out(indent); printf("{\n");
            for(int i=0;i<node->data.compound.stmt_count;i++)
                print_ast(node->data.compound.statements[i], indent+2);
            indent_out(indent); printf("}\n");
        } break;

        case AST_DECLARATION: {
            indent_out(indent);
            printf("Decl %s : ", node->data.declaration.name ? node->data.declaration.name : "(anon)");
            print_type_info(node->data.declaration.type_info);
            putchar('\n');

            if(node->data.declaration.init){
                indent_out(indent+2); puts("Init:");
                print_ast(node->data.declaration.init, indent+4);
            }
        } break;

        case AST_ASSIGNMENT: {
            indent_out(indent);
            if(node->data.assignment.name){
                printf("Assign %s %s\n", node->data.assignment.name, binop_str(node->data.assignment.op));
            }else{
                printf("Assign (lvalue) %s\n", binop_str(node->data.assignment.op));
                print_ast(node->data.assignment.lvalue, indent+2);
            }
            print_ast(node->data.assignment.value, indent+2);
        } break;

        case AST_RETURN_STMT: {
            indent_out(indent); puts("return");
            if(node->data.return_stmt.value)
                print_ast(node->data.return_stmt.value, indent+2);
        } break;

        case AST_IF_STMT: {
            indent_out(indent); puts("if");
            print_ast(node->data.if_stmt.condition, indent+2);
            indent_out(indent); puts("then");
            print_ast(node->data.if_stmt.then_stmt, indent+2);
            if(node->data.if_stmt.else_stmt){
                indent_out(indent); puts("else");
                print_ast(node->data.if_stmt.else_stmt, indent+2);
            }
        } break;

        case AST_WHILE_STMT:
            indent_out(indent); puts("while");
            print_ast(node->data.while_stmt.condition, indent+2);
            print_ast(node->data.while_stmt.body, indent+2);
            break;

        case AST_FOR_STMT:
            indent_out(indent); puts("for");
            if(node->data.for_stmt.init){ indent_out(indent+2); puts("init:"); print_ast(node->data.for_stmt.init, indent+4); }
            if(node->data.for_stmt.condition){ indent_out(indent+2); puts("cond:"); print_ast(node->data.for_stmt.condition, indent+4); }
            if(node->data.for_stmt.update){ indent_out(indent+2); puts("upd:"); print_ast(node->data.for_stmt.update, indent+4); }
            print_ast(node->data.for_stmt.body, indent+2);
            break;

        case AST_EXPR_STMT:
            indent_out(indent); puts("expr;");
            if(node->data.expr_stmt.expr)
                print_ast(node->data.expr_stmt.expr, indent+2);
            break;

        case AST_BINARY_OP:
            indent_out(indent); printf("(%s)\n", binop_str(node->data.binary_op.op));
            print_ast(node->data.binary_op.left, indent+2);
            print_ast(node->data.binary_op.right, indent+2);
            break;

        case AST_UNARY_OP:
            indent_out(indent); printf("(un %s)\n", unop_str(node->data.unary_op.op));
            print_ast(node->data.unary_op.operand, indent+2);
            break;

        case AST_IDENTIFIER:
            indent_out(indent); printf("id %s\n", node->data.identifier.name);
            break;

        case AST_NUMBER:
            indent_out(indent); printf("num %d\n", node->data.number.value);
            break;

        case AST_CALL:
            indent_out(indent); printf("call %s (%d args)\n", node->data.call.name, node->data.call.arg_count);
            for(int i=0;i<node->data.call.arg_count;i++)
                print_ast(node->data.call.args[i], indent+2);
            break;

        case AST_ARRAY_ACCESS:
            indent_out(indent); puts("array[]");
            print_ast(node->data.array_access.array, indent+2);
            print_ast(node->data.array_access.index, indent+2);
            break;

        case AST_ADDRESS_OF:
            indent_out(indent); puts("&");
            print_ast(node->data.address_of.operand, indent+2);
            break;

        case AST_DEREFERENCE:
            indent_out(indent); puts("*");
            print_ast(node->data.dereference.operand, indent+2);
            break;

        case AST_CONDITIONAL:
            indent_out(indent); puts("?:");
            print_ast(node->data.conditional.condition, indent+2);
            print_ast(node->data.conditional.true_expr, indent+2);
            print_ast(node->data.conditional.false_expr, indent+2);
            break;

        default:
            indent_out(indent); printf("(node %d)\n", node->type);
            break;
    }
}
