#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Code generation context
typedef struct {
    FILE *output;
    symbol_table_t *symbol_table;
    int label_counter;
    int temp_counter;
    int string_counter;
    int in_return_block;
    
    // Control flow management
    char *current_break_label;
    char *current_continue_label;
    char *current_switch_end_label;
    
    // Function context
    char *current_function_name;
    type_info_t current_function_return_type;
    
    // String literals storage
    struct {
        char *content;
        int id;
    } *string_literals;
    int string_literal_count;
    
} codegen_context_t;

static codegen_context_t ctx;

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

static type_info_t deep_copy_type_info_codegen(const type_info_t *src) {
    type_info_t result;
    result.base_type = src->base_type ? string_duplicate(src->base_type) : NULL;
    result.pointer_level = src->pointer_level;
    result.is_array = src->is_array;
    result.is_vla = src->is_vla;
    result.is_function = src->is_function;
    result.is_struct = src->is_struct;
    result.is_union = src->is_union;
    result.is_enum = src->is_enum;
    result.is_incomplete = src->is_incomplete;
    result.storage_class = src->storage_class;
    result.qualifiers = src->qualifiers;
    result.array_size = NULL; // Don't copy AST nodes - they're managed separately
    result.param_types = NULL; // Don't copy param arrays - they're managed separately
    result.param_count = src->param_count;
    result.is_variadic = src->is_variadic;
    return result;
}

// Helper functions
static int get_next_temp() {
    return ++ctx.temp_counter;
}

static char *generate_label(const char *prefix) {
    char *label = malloc(64);
    snprintf(label, 64, "%s%d", prefix, ++ctx.label_counter);
    return label;
}

static int is_comparison_op(binary_op_t op) {
    return (op >= OP_EQ && op <= OP_GE);
}

// Convert C type to LLVM type string
static char *get_llvm_type_string(type_info_t *type_info) {
    char *result = malloc(256);
    if (!result) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    // Handle pointers first
    if (type_info->pointer_level > 0) {
        if (strcmp(type_info->base_type, "void") == 0) {
            strcpy(result, "i8");
        } else if (strcmp(type_info->base_type, "char") == 0) {
            strcpy(result, "i8");
        } else if (strcmp(type_info->base_type, "short") == 0) {
            strcpy(result, "i16");
        } else if (strcmp(type_info->base_type, "int") == 0) {
            strcpy(result, "i32");
        } else if (strcmp(type_info->base_type, "long") == 0) {
            strcpy(result, "i64");
        } else if (strcmp(type_info->base_type, "float") == 0) {
            strcpy(result, "float");
        } else if (strcmp(type_info->base_type, "double") == 0) {
            strcpy(result, "double");
        } else if (type_info->is_struct) {
            snprintf(result, 256, "%%struct.%s", type_info->base_type);
        } else if (type_info->is_union) {
            snprintf(result, 256, "%%union.%s", type_info->base_type);
        } else {
            strcpy(result, "i32"); // default
        }
        
        // Add pointer levels
        for (int i = 0; i < type_info->pointer_level; i++) {
            strcat(result, "*");
        }
        return result;
    }
    
    // Handle non-pointer types
    if (strcmp(type_info->base_type, "void") == 0) {
        strcpy(result, "void");
    } else if (strcmp(type_info->base_type, "char") == 0) {
        strcpy(result, "i8");
    } else if (strcmp(type_info->base_type, "short") == 0) {
        strcpy(result, "i16");
    } else if (strcmp(type_info->base_type, "int") == 0) {
        strcpy(result, "i32");
    } else if (strcmp(type_info->base_type, "long") == 0) {
        strcpy(result, "i64");
    } else if (strcmp(type_info->base_type, "float") == 0) {
        strcpy(result, "float");
    } else if (strcmp(type_info->base_type, "double") == 0) {
        strcpy(result, "double");
    } else if (strcmp(type_info->base_type, "_Bool") == 0) {
        strcpy(result, "i1");
    } else if (type_info->is_struct) {
        snprintf(result, 256, "%%struct.%s", type_info->base_type);
    } else if (type_info->is_union) {
        snprintf(result, 256, "%%union.%s", type_info->base_type);
    } else if (type_info->is_enum) {
        strcpy(result, "i32");
    } else {
        strcpy(result, "i32"); // default
    }
    
    return result;
}

// Get size of type in bytes
static size_t get_type_size(type_info_t *type_info) {
    return calculate_type_size(type_info, ctx.symbol_table);
}

// Generate struct type definition
static void generate_struct_type(symbol_t *struct_sym) {
    if (!struct_sym || struct_sym->sym_type != SYM_STRUCT) return;
    
    fprintf(ctx.output, "%%struct.%s = type { ", struct_sym->name);
    
    for (int i = 0; i < struct_sym->member_count; i++) {
        if (i > 0) fprintf(ctx.output, ", ");
        
        symbol_t *member = struct_sym->members[i];
        char *member_type = get_llvm_type_string(&member->type_info);
        
        if (member->type_info.is_array && !member->type_info.is_vla) {
            // Fixed-size array member
            if (member->type_info.array_size && member->type_info.array_size->type == AST_NUMBER) {
                int array_size = member->type_info.array_size->data.number.value;
                fprintf(ctx.output, "[%d x %s]", array_size, member_type);
            } else {
                fprintf(ctx.output, "%s*", member_type); // Pointer for incomplete arrays
            }
        } else {
            fprintf(ctx.output, "%s", member_type);
        }
        
        free(member_type);
    }
    
    fprintf(ctx.output, " }\n");
}

// Generate union type definition
static void generate_union_type(symbol_t *union_sym) {
    if (!union_sym || union_sym->sym_type != SYM_UNION) return;
    
    // For unions, we use the largest member type wrapped in a struct
    size_t max_size = 0;
    symbol_t *largest_member = NULL;
    
    for (int i = 0; i < union_sym->member_count; i++) {
        symbol_t *member = union_sym->members[i];
        if (member->size > max_size) {
            max_size = member->size;
            largest_member = member;
        }
    }
    
    if (largest_member) {
        char *member_type = get_llvm_type_string(&largest_member->type_info);
        fprintf(ctx.output, "%%union.%s = type { %s }\n", union_sym->name, member_type);
        free(member_type);
    } else {
        fprintf(ctx.output, "%%union.%s = type { i8 }\n", union_sym->name);
    }
}

// Store string literal and return its ID
static int store_string_literal(const char *content) {
    // Check if string already exists
    for (int i = 0; i < ctx.string_literal_count; i++) {
        if (strcmp(ctx.string_literals[i].content, content) == 0) {
            return ctx.string_literals[i].id;
        }
    }
    
    // Add new string literal
    ctx.string_literal_count++;
    ctx.string_literals = realloc(ctx.string_literals, 
                                 ctx.string_literal_count * sizeof(*ctx.string_literals));
    
    int id = ++ctx.string_counter;
    ctx.string_literals[ctx.string_literal_count - 1].content = string_duplicate(content);
    ctx.string_literals[ctx.string_literal_count - 1].id = id;
    
    return id;
}

// Generate global string constants
static void generate_string_constants() {
    for (int i = 0; i < ctx.string_literal_count; i++) {
        const char *content = ctx.string_literals[i].content;
        int id = ctx.string_literals[i].id;
        size_t len = strlen(content) + 1; // Include null terminator
        
        fprintf(ctx.output, "@.str%d = private unnamed_addr constant [%zu x i8] c\"", id, len);
        
        // Emit string with escape sequences
        for (const char *p = content; *p; p++) {
            switch (*p) {
                case '\n': fprintf(ctx.output, "\\0A"); break;
                case '\t': fprintf(ctx.output, "\\09"); break;
                case '\r': fprintf(ctx.output, "\\0D"); break;
                case '\\': fprintf(ctx.output, "\\\\"); break;
                case '"': fprintf(ctx.output, "\\22"); break;
                case '\0': fprintf(ctx.output, "\\00"); break;
                default:
                    if (*p >= 32 && *p <= 126) {
                        fprintf(ctx.output, "%c", *p);
                    } else {
                        fprintf(ctx.output, "\\%02X", (unsigned char)*p);
                    }
                    break;
            }
        }
        fprintf(ctx.output, "\\00\"\n");
    }
}

// Forward declarations
static int generate_expression(ast_node_t *node);
static void generate_statement(ast_node_t *node);
static void generate_compound_statement(ast_node_t *node);

// Generate expression and return temporary number or constant value
static int generate_expression(ast_node_t *node) {
    if (!node) return -1;
    
    switch (node->type) {
        case AST_NUMBER: {
            return node->data.number.value; // Return constant directly
        }
        
        case AST_CHARACTER: {
            return (int)node->data.character.value;
        }
        
        case AST_STRING_LITERAL: {
            int string_id = store_string_literal(node->data.string_literal.value);
            int temp = get_next_temp();
            size_t len = strlen(node->data.string_literal.value) + 1;
            
            fprintf(ctx.output, "  %%t%d = getelementptr [%zu x i8], [%zu x i8]* @.str%d, i32 0, i32 0\n",
                    temp, len, len, string_id);
            
            return temp;
        }
        
        case AST_IDENTIFIER: {
            symbol_t *sym = find_symbol(ctx.symbol_table, node->data.identifier.name);
            if (!sym) {
                fprintf(stderr, "Undefined variable: %s\n", node->data.identifier.name);
                return -1;
            }
            
            int temp = get_next_temp();
            char *type_str = get_llvm_type_string(&sym->type_info);
            
            // Handle different symbol types
            if (sym->sym_type == SYM_ENUM_CONSTANT) {
                // Enum constants are compile-time constants
                free(type_str);
                return sym->enum_value;
            }
            
            if (sym->type_info.is_array) {
                if (sym->is_parameter) {
                    fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s.addr\n", 
                            temp, type_str, type_str, sym->llvm_name);
                } else if (sym->type_info.is_vla) {
                    fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s\n", 
                            temp, type_str, type_str, sym->llvm_name);
                } else {
                    // Fixed array - get pointer to first element
                    char *element_type = get_llvm_type_string(&sym->type_info);
                    if (sym->type_info.pointer_level > 0) {
                        element_type[strlen(element_type) - 1] = '\0'; // Remove one *
                    }
                    fprintf(ctx.output, "  %%t%d = getelementptr [%zu x %s], [%zu x %s]* %%%s, i32 0, i32 0\n", 
                            temp, sym->size / get_type_size(&sym->type_info), element_type,
                            sym->size / get_type_size(&sym->type_info), element_type, sym->llvm_name);
                    free(element_type);
                }
            } else {
                // Regular variables
                if (sym->is_parameter) {
                    fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s.addr\n", 
                            temp, type_str, type_str, sym->llvm_name);
                } else {
                    fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s\n", 
                            temp, type_str, type_str, sym->llvm_name);
                }
            }
            
            free(type_str);
            return temp;
        }
        
        case AST_BINARY_OP: {
            // Handle short-circuit logical operators
            if (node->data.binary_op.op == OP_LAND || node->data.binary_op.op == OP_LOR) {
                char *left_label = generate_label("logical_left");
                char *right_label = generate_label("logical_right");
                char *end_label = generate_label("logical_end");
                int result_temp = get_next_temp();
                
                fprintf(ctx.output, "  %%t%d.addr = alloca i1\n", result_temp);
                
                int left = generate_expression(node->data.binary_op.left);
                int left_bool;
                
                if (node->data.binary_op.left->type == AST_BINARY_OP && 
                    is_comparison_op(node->data.binary_op.left->data.binary_op.op)) {
                    left_bool = left;
                } else {
                    left_bool = get_next_temp();
                    char left_str[32];
                    if (node->data.binary_op.left->type == AST_NUMBER) {
                        snprintf(left_str, sizeof(left_str), "%d", left);
                    } else {
                        snprintf(left_str, sizeof(left_str), "%%t%d", left);
                    }
                    fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", left_bool, left_str);
                }
                
                if (node->data.binary_op.op == OP_LAND) {
                    // AND: if left is false, result is false; otherwise evaluate right
                    fprintf(ctx.output, "  br i1 %%t%d, label %%%s, label %%%s\n", 
                            left_bool, right_label, left_label);
                    
                    fprintf(ctx.output, "%s:\n", left_label);
                    fprintf(ctx.output, "  store i1 false, i1* %%t%d.addr\n", result_temp);
                    fprintf(ctx.output, "  br label %%%s\n", end_label);
                } else {
                    // OR: if left is true, result is true; otherwise evaluate right
                    fprintf(ctx.output, "  br i1 %%t%d, label %%%s, label %%%s\n", 
                            left_bool, left_label, right_label);
                    
                    fprintf(ctx.output, "%s:\n", left_label);
                    fprintf(ctx.output, "  store i1 true, i1* %%t%d.addr\n", result_temp);
                    fprintf(ctx.output, "  br label %%%s\n", end_label);
                }
                
                fprintf(ctx.output, "%s:\n", right_label);
                int right = generate_expression(node->data.binary_op.right);
                int right_bool;
                
                if (node->data.binary_op.right->type == AST_BINARY_OP && 
                    is_comparison_op(node->data.binary_op.right->data.binary_op.op)) {
                    right_bool = right;
                } else {
                    right_bool = get_next_temp();
                    char right_str[32];
                    if (node->data.binary_op.right->type == AST_NUMBER) {
                        snprintf(right_str, sizeof(right_str), "%d", right);
                    } else {
                        snprintf(right_str, sizeof(right_str), "%%t%d", right);
                    }
                    fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", right_bool, right_str);
                }
                
                fprintf(ctx.output, "  store i1 %%t%d, i1* %%t%d.addr\n", right_bool, result_temp);
                fprintf(ctx.output, "  br label %%%s\n", end_label);
                
                fprintf(ctx.output, "%s:\n", end_label);
                int final_temp = get_next_temp();
                fprintf(ctx.output, "  %%t%d = load i1, i1* %%t%d.addr\n", final_temp, result_temp);
                
                // Convert to i32 for compatibility
                int result = get_next_temp();
                fprintf(ctx.output, "  %%t%d = zext i1 %%t%d to i32\n", result, final_temp);
                
                free(left_label);
                free(right_label);
                free(end_label);
                return result;
            }
            
            // Handle compound assignment operators
            if (node->data.binary_op.op >= OP_ADD_ASSIGN && node->data.binary_op.op <= OP_BXOR_ASSIGN) {
                // This should be handled in assignment statement, not here
                fprintf(stderr, "Compound assignment in expression context\n");
                return -1;
            }
            
            // Regular binary operations
            int left = generate_expression(node->data.binary_op.left);
            int right = generate_expression(node->data.binary_op.right);
            int temp = get_next_temp();
            
            const char *op_str = "";
            const char *result_type = "i32";
            
            switch (node->data.binary_op.op) {
                case OP_ADD: op_str = "add"; break;
                case OP_SUB: op_str = "sub"; break;
                case OP_MUL: op_str = "mul"; break;
                case OP_DIV: op_str = "sdiv"; break;
                case OP_MOD: op_str = "srem"; break;
                case OP_EQ: op_str = "icmp eq"; result_type = "i1"; break;
                case OP_NE: op_str = "icmp ne"; result_type = "i1"; break;
                case OP_LT: op_str = "icmp slt"; result_type = "i1"; break;
                case OP_LE: op_str = "icmp sle"; result_type = "i1"; break;
                case OP_GT: op_str = "icmp sgt"; result_type = "i1"; break;
                case OP_GE: op_str = "icmp sge"; result_type = "i1"; break;
                case OP_BAND: op_str = "and"; break;
                case OP_BOR: op_str = "or"; break;
                case OP_BXOR: op_str = "xor"; break;
                case OP_LSHIFT: op_str = "shl"; break;
                case OP_RSHIFT: op_str = "ashr"; break;
                default:
                    fprintf(stderr, "Unknown binary operator: %d\n", node->data.binary_op.op);
                    return -1;
            }
            
            char left_operand[32], right_operand[32];
            if (node->data.binary_op.left->type == AST_NUMBER) {
                snprintf(left_operand, sizeof(left_operand), "%d", left);
            } else {
                snprintf(left_operand, sizeof(left_operand), "%%t%d", left);
            }
            
            if (node->data.binary_op.right->type == AST_NUMBER) {
                snprintf(right_operand, sizeof(right_operand), "%d", right);
            } else {
                snprintf(right_operand, sizeof(right_operand), "%%t%d", right);
            }
            
            fprintf(ctx.output, "  %%t%d = %s %s %s, %s\n", 
                    temp, op_str, result_type, left_operand, right_operand);
            
            return temp;
        }

        case AST_ASSIGNMENT: {
            // Handle assignment as expression (returns the assigned value)
            int value = generate_expression(node->data.assignment.value);
            
            if (node->data.assignment.name) {
                // Simple identifier assignment
                symbol_t *sym = find_symbol(ctx.symbol_table, node->data.assignment.name);
                if (!sym) {
                    fprintf(stderr, "Undefined variable in assignment: %s\n", node->data.assignment.name);
                    return -1;
                }
                
                char *type_str = get_llvm_type_string(&sym->type_info);
                
                // Store the value
                if (sym->is_parameter) {
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store %s %d, %s* %%%s.addr\n", 
                                type_str, value, type_str, sym->llvm_name);
                    } else {
                        fprintf(ctx.output, "  store %s %%t%d, %s* %%%s.addr\n", 
                                type_str, value, type_str, sym->llvm_name);
                    }
                } else {
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store %s %d, %s* %%%s\n", 
                                type_str, value, type_str, sym->llvm_name);
                    } else {
                        fprintf(ctx.output, "  store %s %%t%d, %s* %%%s\n", 
                                type_str, value, type_str, sym->llvm_name);
                    }
                }
                
                free(type_str);
                
                // Return the assigned value
                return value;
                
            } else if (node->data.assignment.lvalue) {
                // Assignment to lvalue - similar to statement version but return value
                // Handle array access, dereference, etc.
                if (node->data.assignment.lvalue->type == AST_ARRAY_ACCESS) {
                    ast_node_t *array = node->data.assignment.lvalue->data.array_access.array;
                    ast_node_t *index = node->data.assignment.lvalue->data.array_access.index;
                    
                    if (array->type == AST_IDENTIFIER) {
                        symbol_t *sym = find_symbol(ctx.symbol_table, array->data.identifier.name);
                        if (!sym) {
                            fprintf(stderr, "Undefined array in assignment: %s\n", array->data.identifier.name);
                            return -1;
                        }
                        
                        int index_val = generate_expression(index);
                        int addr_temp = get_next_temp();
                        
                        char index_str[32];
                        if (index->type == AST_NUMBER) {
                            snprintf(index_str, sizeof(index_str), "%d", index_val);
                        } else {
                            snprintf(index_str, sizeof(index_str), "%%t%d", index_val);
                        }
                        
                        char *element_type = get_llvm_type_string(&node->data.assignment.lvalue->data.array_access.element_type);
                        
                        if (sym->type_info.is_array) {
                            if (sym->is_parameter) {
                                int ptr_temp = get_next_temp();
                                fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s.addr\n", 
                                        ptr_temp, element_type, element_type, sym->llvm_name);
                                fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                        addr_temp, element_type, element_type, ptr_temp, index_str);
                            } else if (sym->type_info.is_vla) {
                                int ptr_temp = get_next_temp();
                                fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s\n", 
                                        ptr_temp, element_type, element_type, sym->llvm_name);
                                fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                        addr_temp, element_type, element_type, ptr_temp, index_str);
                            } else {
                                size_t array_length = sym->size / get_type_size(&sym->type_info);
                                fprintf(ctx.output, "  %%t%d = getelementptr [%zu x %s], [%zu x %s]* %%%s, i32 0, i32 %s\n", 
                                        addr_temp, array_length, element_type, array_length, element_type, 
                                        sym->llvm_name, index_str);
                            }
                        }
                        
                        // Store value
                        if (node->data.assignment.value->type == AST_NUMBER) {
                            fprintf(ctx.output, "  store %s %d, %s* %%t%d\n", 
                                    element_type, value, element_type, addr_temp);
                        } else {
                            fprintf(ctx.output, "  store %s %%t%d, %s* %%t%d\n", 
                                    element_type, value, element_type, addr_temp);
                        }
                        
                        free(element_type);
                        return value;
                    }
                } else if (node->data.assignment.lvalue->type == AST_DEREFERENCE) {
                    // Handle *pointer = value
                    int ptr = generate_expression(node->data.assignment.lvalue->data.dereference.operand);
                    char *result_type = get_llvm_type_string(&node->data.assignment.lvalue->data.dereference.result_type);
                    
                    char ptr_str[32];
                    if (node->data.assignment.lvalue->data.dereference.operand->type == AST_NUMBER) {
                        snprintf(ptr_str, sizeof(ptr_str), "%d", ptr);
                    } else {
                        snprintf(ptr_str, sizeof(ptr_str), "%%t%d", ptr);
                    }
                    
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store %s %d, %s* %s\n", 
                                result_type, value, result_type, ptr_str);
                    } else {
                        fprintf(ctx.output, "  store %s %%t%d, %s* %s\n", 
                                result_type, value, result_type, ptr_str);
                    }
                    
                    free(result_type);
                    return value;
                }
            }
            
            return -1;
        }
        
        case AST_UNARY_OP: {
            // Handle increment/decrement operators
            if (node->data.unary_op.op >= OP_PREINC && node->data.unary_op.op <= OP_POSTDEC) {
                ast_node_t *operand = node->data.unary_op.operand;
                if (operand->type != AST_IDENTIFIER) {
                    fprintf(stderr, "Increment/decrement on non-lvalue\n");
                    return -1;
                }
                
                symbol_t *sym = find_symbol(ctx.symbol_table, operand->data.identifier.name);
                if (!sym) {
                    fprintf(stderr, "Undefined variable in increment/decrement: %s\n", 
                            operand->data.identifier.name);
                    return -1;
                }
                
                char *type_str = get_llvm_type_string(&sym->type_info);
                int old_val_temp = get_next_temp();
                int new_val_temp = get_next_temp();
                
                // Load current value
                if (sym->is_parameter) {
                    fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s.addr\n", 
                            old_val_temp, type_str, type_str, sym->llvm_name);
                } else {
                    fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s\n", 
                            old_val_temp, type_str, type_str, sym->llvm_name);
                }
                
                // Calculate new value
                const char *op_str = (node->data.unary_op.op == OP_PREINC || 
                                     node->data.unary_op.op == OP_POSTINC) ? "add" : "sub";
                fprintf(ctx.output, "  %%t%d = %s %s %%t%d, 1\n", 
                        new_val_temp, op_str, type_str, old_val_temp);
                
                // Store new value
                if (sym->is_parameter) {
                    fprintf(ctx.output, "  store %s %%t%d, %s* %%%s.addr\n", 
                            type_str, new_val_temp, type_str, sym->llvm_name);
                } else {
                    fprintf(ctx.output, "  store %s %%t%d, %s* %%%s\n", 
                            type_str, new_val_temp, type_str, sym->llvm_name);
                }
                
                free(type_str);
                
                // Return appropriate value
                if (node->data.unary_op.op == OP_PREINC || node->data.unary_op.op == OP_PREDEC) {
                    return new_val_temp;
                } else {
                    return old_val_temp;
                }
            }
            
            // Regular unary operators
            int operand = generate_expression(node->data.unary_op.operand);
            int temp = get_next_temp();
            
            char operand_str[32];
            if (node->data.unary_op.operand->type == AST_NUMBER) {
                snprintf(operand_str, sizeof(operand_str), "%d", operand);
            } else {
                snprintf(operand_str, sizeof(operand_str), "%%t%d", operand);
            }
            
            switch (node->data.unary_op.op) {
                case OP_NEG:
                    fprintf(ctx.output, "  %%t%d = sub i32 0, %s\n", temp, operand_str);
                    break;
                case OP_NOT:
                    fprintf(ctx.output, "  %%t%d = icmp eq i32 %s, 0\n", temp, operand_str);
                    break;
                case OP_BNOT:
                    fprintf(ctx.output, "  %%t%d = xor i32 %s, -1\n", temp, operand_str);
                    break;
                default:
                    fprintf(stderr, "Unknown unary operator: %d\n", node->data.unary_op.op);
                    return -1;
            }
            
            return temp;
        }
        
        case AST_CONDITIONAL: {
            char *true_label = generate_label("cond_true");
            char *false_label = generate_label("cond_false");
            char *end_label = generate_label("cond_end");
            int result_temp = get_next_temp();
            
            // Get result type from true expression
            char *result_type = get_llvm_type_string(&node->data.conditional.result_type);
            fprintf(ctx.output, "  %%t%d.addr = alloca %s\n", result_temp, result_type);
            
            // Evaluate condition
            int cond = generate_expression(node->data.conditional.condition);
            int bool_temp;
            
            if (node->data.conditional.condition->type == AST_BINARY_OP &&
                is_comparison_op(node->data.conditional.condition->data.binary_op.op)) {
                bool_temp = cond;
            } else {
                bool_temp = get_next_temp();
                char cond_str[32];
                if (node->data.conditional.condition->type == AST_NUMBER) {
                    snprintf(cond_str, sizeof(cond_str), "%d", cond);
                } else {
                    snprintf(cond_str, sizeof(cond_str), "%%t%d", cond);
                }
                fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", bool_temp, cond_str);
            }
            
            fprintf(ctx.output, "  br i1 %%t%d, label %%%s, label %%%s\n", 
                    bool_temp, true_label, false_label);
            
            // True branch
            fprintf(ctx.output, "%s:\n", true_label);
            int true_val = generate_expression(node->data.conditional.true_expr);
            if (node->data.conditional.true_expr->type == AST_NUMBER) {
                fprintf(ctx.output, "  store %s %d, %s* %%t%d.addr\n", 
                        result_type, true_val, result_type, result_temp);
            } else {
                fprintf(ctx.output, "  store %s %%t%d, %s* %%t%d.addr\n", 
                        result_type, true_val, result_type, result_temp);
            }
            fprintf(ctx.output, "  br label %%%s\n", end_label);
            
            // False branch
            fprintf(ctx.output, "%s:\n", false_label);
            int false_val = generate_expression(node->data.conditional.false_expr);
            if (node->data.conditional.false_expr->type == AST_NUMBER) {
                fprintf(ctx.output, "  store %s %d, %s* %%t%d.addr\n", 
                        result_type, false_val, result_type, result_temp);
            } else {
                fprintf(ctx.output, "  store %s %%t%d, %s* %%t%d.addr\n", 
                        result_type, false_val, result_type, result_temp);
            }
            fprintf(ctx.output, "  br label %%%s\n", end_label);
            
            // End - load result
            fprintf(ctx.output, "%s:\n", end_label);
            int final_temp = get_next_temp();
            fprintf(ctx.output, "  %%t%d = load %s, %s* %%t%d.addr\n", 
                    final_temp, result_type, result_type, result_temp);
            
            free(result_type);
            free(true_label);
            free(false_label);
            free(end_label);
            return final_temp;
        }
        
        case AST_SIZEOF: {
            // Return the computed size as a constant
            return (int)node->data.sizeof_op.size_value;
        }
        
        case AST_CAST: {
            int operand = generate_expression(node->data.cast.expression);
            int temp = get_next_temp();
            
            type_info_t source_type = get_expression_type(node->data.cast.expression, ctx.symbol_table);
            char *source_type_str = get_llvm_type_string(&source_type);
            char *target_type_str = get_llvm_type_string(&node->data.cast.target_type);
            
            // Handle different cast types
            if (strcmp(source_type_str, target_type_str) == 0) {
                // No cast needed
                free(source_type_str);
                free(target_type_str);
                return operand;
            }
            
            // Implement various cast operations
            char operand_str[32];
            if (node->data.cast.expression->type == AST_NUMBER) {
                snprintf(operand_str, sizeof(operand_str), "%d", operand);
            } else {
                snprintf(operand_str, sizeof(operand_str), "%%t%d", operand);
            }
            
            // Simple casting - extend for more complex cases
            if (strcmp(target_type_str, "i32") == 0 && strcmp(source_type_str, "i8") == 0) {
                fprintf(ctx.output, "  %%t%d = sext i8 %s to i32\n", temp, operand_str);
            } else if (strcmp(target_type_str, "i8") == 0 && strcmp(source_type_str, "i32") == 0) {
                fprintf(ctx.output, "  %%t%d = trunc i32 %s to i8\n", temp, operand_str);
            } else {
                // Default bitcast
                fprintf(ctx.output, "  %%t%d = bitcast %s %s to %s\n", 
                        temp, source_type_str, operand_str, target_type_str);
            }
            
            free(source_type_str);
            free(target_type_str);
            return temp;
        }
        
        case AST_ADDRESS_OF: {
            ast_node_t *operand = node->data.address_of.operand;
            
            if (operand->type == AST_IDENTIFIER) {
                symbol_t *sym = find_symbol(ctx.symbol_table, operand->data.identifier.name);
                if (!sym) {
                    fprintf(stderr, "Undefined variable in address-of: %s\n", 
                            operand->data.identifier.name);
                    return -1;
                }
                
                int temp = get_next_temp();
                char *var_type_str = get_llvm_type_string(&sym->type_info);
                
                if (sym->is_parameter) {
                    // For parameters, return address of .addr
                    fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%%s.addr, i32 0\n", 
                            temp, var_type_str, var_type_str, sym->llvm_name);
                } else {
                    // For local variables, return their address
                    fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%%s, i32 0\n", 
                            temp, var_type_str, var_type_str, sym->llvm_name);
                }
                
                free(var_type_str);
                return temp;
            }
            else if (operand->type == AST_ARRAY_ACCESS) {
                // Handle &array[index] - return address of element
                ast_node_t *array_node = operand->data.array_access.array;
                ast_node_t *index_node = operand->data.array_access.index;
                
                if (array_node->type == AST_IDENTIFIER) {
                    symbol_t *sym = find_symbol(ctx.symbol_table, array_node->data.identifier.name);
                    if (!sym) {
                        fprintf(stderr, "Undefined array in address-of: %s\n", 
                                array_node->data.identifier.name);
                        return -1;
                    }
                    
                    int index = generate_expression(index_node);
                    int addr_temp = get_next_temp();
                    
                    char index_str[32];
                    if (index_node->type == AST_NUMBER) {
                        snprintf(index_str, sizeof(index_str), "%d", index);
                    } else {
                        snprintf(index_str, sizeof(index_str), "%%t%d", index);
                    }
                    
                    char *element_type = get_llvm_type_string(&operand->data.array_access.element_type);
                    
                    if (sym->type_info.is_array) {
                        if (sym->is_parameter) {
                            int ptr_temp = get_next_temp();
                            fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s.addr\n", 
                                    ptr_temp, element_type, element_type, sym->llvm_name);
                            fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                    addr_temp, element_type, element_type, ptr_temp, index_str);
                        } else if (sym->type_info.is_vla) {
                            int ptr_temp = get_next_temp();
                            fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s\n", 
                                    ptr_temp, element_type, element_type, sym->llvm_name);
                            fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                    addr_temp, element_type, element_type, ptr_temp, index_str);
                        } else {
                            if (sym->type_info.array_size && sym->type_info.array_size->type == AST_NUMBER) {
                                size_t array_length = sym->type_info.array_size->data.number.value;
                                fprintf(ctx.output, "  %%t%d = getelementptr [%zu x %s], [%zu x %s]* %%%s, i32 0, i32 %s\n", 
                                        addr_temp, array_length, element_type, array_length, element_type, 
                                        sym->llvm_name, index_str);
                            } else {
                                fprintf(stderr, "Cannot take address of incomplete array element\n");
                                free(element_type);
                                return -1;
                            }
                        }
                    } else if (sym->type_info.pointer_level > 0) {
                        int ptr_temp = get_next_temp();
                        char *ptr_type = get_llvm_type_string(&sym->type_info);
                        fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s\n", 
                                ptr_temp, ptr_type, ptr_type, sym->llvm_name);
                        fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                addr_temp, element_type, element_type, ptr_temp, index_str);
                        free(ptr_type);
                    }
                    
                    free(element_type);
                    return addr_temp;
                }
            }
            else if (operand->type == AST_DEREFERENCE) {
                // Handle &(*ptr) - simplifies to ptr
                return generate_expression(operand->data.dereference.operand);
            }
            else if (operand->type == AST_MEMBER_ACCESS) {
                // Handle &obj.member
                ast_node_t *object = operand->data.member_access.object;
                const char *member_name = operand->data.member_access.member;
                
                if (object->type == AST_IDENTIFIER) {
                    symbol_t *obj_sym = find_symbol(ctx.symbol_table, object->data.identifier.name);
                    if (!obj_sym || (!obj_sym->type_info.is_struct && !obj_sym->type_info.is_union)) {
                        fprintf(stderr, "Member access on non-struct/union in address-of\n");
                        return -1;
                    }
                    
                    symbol_t *struct_sym = find_symbol(ctx.symbol_table, obj_sym->type_info.base_type);
                    if (!struct_sym) {
                        fprintf(stderr, "Unknown struct/union type: %s\n", obj_sym->type_info.base_type);
                        return -1;
                    }
                    
                    symbol_t *member = find_struct_member(struct_sym, member_name);
                    if (!member) {
                        fprintf(stderr, "Unknown member: %s\n", member_name);
                        return -1;
                    }
                    
                    int addr_temp = get_next_temp();
                    char *struct_type = get_llvm_type_string(&obj_sym->type_info);
                    
                    // Get address of member
                    fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%%s, i32 0, i32 %zu\n", 
                            addr_temp, struct_type, struct_type, obj_sym->llvm_name, 
                            member->offset);
                    
                    free(struct_type);
                    return addr_temp;
                }
            }
            
            fprintf(stderr, "Address-of on complex expression not fully implemented\n");
            return -1;
        }
        
        case AST_DEREFERENCE: {
            int ptr = generate_expression(node->data.dereference.operand);
            int temp = get_next_temp();
            
            char *result_type = get_llvm_type_string(&node->data.dereference.result_type);
            
            char ptr_str[32];
            if (node->data.dereference.operand->type == AST_NUMBER) {
                snprintf(ptr_str, sizeof(ptr_str), "%d", ptr);
            } else {
                snprintf(ptr_str, sizeof(ptr_str), "%%t%d", ptr);
            }
            
            fprintf(ctx.output, "  %%t%d = load %s, %s* %s\n", 
                    temp, result_type, result_type, ptr_str);
            
            free(result_type);
            return temp;
        }
        
case AST_ARRAY_ACCESS: {
            // array[index] is equivalent to *(array + index)
            ast_node_t *array_node = node->data.array_access.array;
            ast_node_t *index_node = node->data.array_access.index;
            
            if (array_node->type == AST_IDENTIFIER) {
                symbol_t *sym = find_symbol(ctx.symbol_table, array_node->data.identifier.name);
                if (!sym) {
                    fprintf(stderr, "Undefined array: %s\n", array_node->data.identifier.name);
                    return -1;
                }
                
                int index = generate_expression(index_node);
                int addr_temp = get_next_temp();
                int result_temp = get_next_temp();
                
                char index_str[32];
                if (index_node->type == AST_NUMBER) {
                    snprintf(index_str, sizeof(index_str), "%d", index);
                } else {
                    snprintf(index_str, sizeof(index_str), "%%t%d", index);
                }
                
                char *element_type = get_llvm_type_string(&node->data.array_access.element_type);
                
                // Handle different array types
                if (sym->type_info.is_array || sym->type_info.pointer_level > 0) {
                    if (sym->is_parameter) {
                        // Parameter array: load pointer first
                        int ptr_temp = get_next_temp();
                        char *param_type = get_llvm_type_string(&sym->type_info);
                        fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s.addr\n", 
                                ptr_temp, param_type, param_type, sym->llvm_name);
                        fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                addr_temp, element_type, element_type, ptr_temp, index_str);
                        free(param_type);
                    } else if (sym->type_info.is_vla) {
                        // VLA: load pointer first
                        int ptr_temp = get_next_temp();
                        fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s\n", 
                                ptr_temp, element_type, element_type, sym->llvm_name);
                        fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                addr_temp, element_type, element_type, ptr_temp, index_str);
                    } else if (sym->type_info.is_array) {
                        // Fixed array
                        if (sym->type_info.array_size && sym->type_info.array_size->type == AST_NUMBER) {
                            size_t array_length = sym->type_info.array_size->data.number.value;
                            fprintf(ctx.output, "  %%t%d = getelementptr [%zu x %s], [%zu x %s]* %%%s, i32 0, i32 %s\n", 
                                    addr_temp, array_length, element_type, array_length, element_type, 
                                    sym->llvm_name, index_str);
                        } else {
                            // Incomplete array - treat as pointer
                            int ptr_temp = get_next_temp();
                            char *ptr_type = get_llvm_type_string(&sym->type_info);
                            fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s\n", 
                                    ptr_temp, ptr_type, ptr_type, sym->llvm_name);
                            fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                    addr_temp, element_type, element_type, ptr_temp, index_str);
                            free(ptr_type);
                        }
                    } else if (sym->type_info.pointer_level > 0) {
                        // Pointer access
                        int ptr_temp = get_next_temp();
                        char *ptr_type = get_llvm_type_string(&sym->type_info);
                        fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s\n", 
                                ptr_temp, ptr_type, ptr_type, sym->llvm_name);
                        fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                addr_temp, element_type, element_type, ptr_temp, index_str);
                        free(ptr_type);
                    }
                } else {
                    fprintf(stderr, "Array access on non-array/pointer variable: %s\n", 
                            array_node->data.identifier.name);
                    free(element_type);
                    return -1;
                }
                
                // Load the value
                fprintf(ctx.output, "  %%t%d = load %s, %s* %%t%d\n", 
                        result_temp, element_type, element_type, addr_temp);
                
                free(element_type);
                return result_temp;
            }
            
            fprintf(stderr, "Complex array access not implemented\n");
            return -1;
        }
        
        case AST_MEMBER_ACCESS: {
            // obj.member
            ast_node_t *object = node->data.member_access.object;
            const char *member_name = node->data.member_access.member;
            
            if (object->type == AST_IDENTIFIER) {
                symbol_t *obj_sym = find_symbol(ctx.symbol_table, object->data.identifier.name);
                if (!obj_sym || (!obj_sym->type_info.is_struct && !obj_sym->type_info.is_union)) {
                    fprintf(stderr, "Member access on non-struct/union\n");
                    return -1;
                }
                
                symbol_t *struct_sym = find_symbol(ctx.symbol_table, obj_sym->type_info.base_type);
                if (!struct_sym) {
                    fprintf(stderr, "Unknown struct/union type: %s\n", obj_sym->type_info.base_type);
                    return -1;
                }
                
                symbol_t *member = find_struct_member(struct_sym, member_name);
                if (!member) {
                    fprintf(stderr, "Unknown member: %s\n", member_name);
                    return -1;
                }
                
                int addr_temp = get_next_temp();
                int result_temp = get_next_temp();
                
                char *struct_type = get_llvm_type_string(&obj_sym->type_info);
                char *member_type = get_llvm_type_string(&member->type_info);
                
                // Get address of member
                fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%%s, i32 0, i32 %zu\n", 
                        addr_temp, struct_type, struct_type, obj_sym->llvm_name, 
                        member->offset);
                
                // Load member value
                fprintf(ctx.output, "  %%t%d = load %s, %s* %%t%d\n", 
                        result_temp, member_type, member_type, addr_temp);
                
                free(struct_type);
                free(member_type);
                return result_temp;
            }
            
            fprintf(stderr, "Complex member access not implemented\n");
            return -1;
        }
        
        case AST_PTR_MEMBER_ACCESS: {
            // ptr->member
            ast_node_t *object = node->data.ptr_member_access.object;
            const char *member_name = node->data.ptr_member_access.member;
            
            int ptr = generate_expression(object);
            
            // Find the struct/union type
            type_info_t ptr_type = get_expression_type(object, ctx.symbol_table);
            if (ptr_type.pointer_level == 0 || (!ptr_type.is_struct && !ptr_type.is_union)) {
                fprintf(stderr, "Pointer member access on non-pointer-to-struct/union\n");
                return -1;
            }
            
            symbol_t *struct_sym = find_symbol(ctx.symbol_table, ptr_type.base_type);
            if (!struct_sym) {
                fprintf(stderr, "Unknown struct/union type: %s\n", ptr_type.base_type);
                return -1;
            }
            
            symbol_t *member = find_struct_member(struct_sym, member_name);
            if (!member) {
                fprintf(stderr, "Unknown member: %s\n", member_name);
                return -1;
            }
            
            int addr_temp = get_next_temp();
            int result_temp = get_next_temp();
            
            char *struct_type = get_llvm_type_string(&ptr_type);
            // Remove one level of pointer for the struct type
            struct_type[strlen(struct_type) - 1] = '\0';
            char *member_type = get_llvm_type_string(&member->type_info);
            
            char ptr_str[32];
            if (object->type == AST_NUMBER) {
                snprintf(ptr_str, sizeof(ptr_str), "%d", ptr);
            } else {
                snprintf(ptr_str, sizeof(ptr_str), "%%t%d", ptr);
            }
            
            // Get address of member
            fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %s, i32 0, i32 %zu\n", 
                    addr_temp, struct_type, struct_type, ptr_str, member->offset);
            
            // Load member value
            fprintf(ctx.output, "  %%t%d = load %s, %s* %%t%d\n", 
                    result_temp, member_type, member_type, addr_temp);
            
            free(struct_type);
            free(member_type);
            return result_temp;
        }
        
        case AST_CALL: {
            // Generate arguments
            int *arg_temps = NULL;
            if (node->data.call.arg_count > 0) {
                arg_temps = malloc(sizeof(int) * node->data.call.arg_count);
                for (int i = 0; i < node->data.call.arg_count; i++) {
                    arg_temps[i] = generate_expression(node->data.call.args[i]);
                }
            }
            
            int temp = get_next_temp();
            char *return_type = get_llvm_type_string(&node->data.call.return_type);
            
            fprintf(ctx.output, "  %%t%d = call %s @%s(", temp, return_type, node->data.call.name);
            
            for (int i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0) fprintf(ctx.output, ", ");
                
                // Determine argument type
                type_info_t arg_type = get_expression_type(node->data.call.args[i], ctx.symbol_table);
                char *arg_type_str = get_llvm_type_string(&arg_type);
                
                if (node->data.call.args[i]->type == AST_NUMBER) {
                    fprintf(ctx.output, "%s %d", arg_type_str, arg_temps[i]);
                } else {
                    fprintf(ctx.output, "%s %%t%d", arg_type_str, arg_temps[i]);
                }
                
                free(arg_type_str);
            }
            
            fprintf(ctx.output, ")\n");
            
            free(arg_temps);
            free(return_type);
            return temp;
        }
        
        default:
            fprintf(stderr, "Unknown expression type: %d\n", node->type);
            return -1;
    }
}

// Generate statement
static void generate_statement(ast_node_t *node) {
    if (!node || ctx.in_return_block) return;
    
    switch (node->type) {
        case AST_COMPOUND_STMT: {
            generate_compound_statement(node);
            break;
        }
        
        case AST_DECLARATION: {
            symbol_t *sym = add_symbol(ctx.symbol_table, node->data.declaration.name, 
                                     SYM_VARIABLE, node->data.declaration.type_info);
            if (!sym) {
                fprintf(stderr, "Failed to add symbol: %s\n", node->data.declaration.name);
                return;
            }
            
            char *type_str = get_llvm_type_string(&sym->type_info);
            fprintf(ctx.output, "  %%%s = alloca %s\n", sym->llvm_name, type_str);
            
            if (node->data.declaration.init) {
                int init_value = generate_expression(node->data.declaration.init);
                
                if (node->data.declaration.init->type == AST_NUMBER) {
                    fprintf(ctx.output, "  store %s %d, %s* %%%s\n", 
                            type_str, init_value, type_str, sym->llvm_name);
                } else {
                    fprintf(ctx.output, "  store %s %%t%d, %s* %%%s\n", 
                            type_str, init_value, type_str, sym->llvm_name);
                }
            }
            
            free(type_str);
            break;
        }
        
        case AST_ARRAY_DECL: {
            symbol_t *sym = add_symbol(ctx.symbol_table, node->data.array_decl.name, 
                                     SYM_VARIABLE, node->data.array_decl.type_info);
            if (!sym) return;
            
            if (node->data.array_decl.is_vla) {
                // Variable Length Array
                int size = generate_expression(node->data.array_decl.size);
                int temp = get_next_temp();
                
                char size_str[32];
                if (node->data.array_decl.size->type == AST_NUMBER) {
                    snprintf(size_str, sizeof(size_str), "%d", size);
                } else {
                    snprintf(size_str, sizeof(size_str), "%%t%d", size);
                }
                
                char *element_type = get_llvm_type_string(&node->data.array_decl.type_info);
                fprintf(ctx.output, "  %%t%d = alloca %s, i32 %s\n", 
                        temp, element_type, size_str);
                fprintf(ctx.output, "  %%%s = alloca %s*\n", 
                        sym->llvm_name, element_type);
                fprintf(ctx.output, "  store %s* %%t%d, %s** %%%s\n", 
                        element_type, temp, element_type, sym->llvm_name);
                free(element_type);
            } else {
                // Fixed size array
                if (node->data.array_decl.size && node->data.array_decl.size->type == AST_NUMBER) {
                    int array_size = node->data.array_decl.size->data.number.value;
                    char *element_type = get_llvm_type_string(&node->data.array_decl.type_info);
                    fprintf(ctx.output, "  %%%s = alloca [%d x %s]\n", 
                            sym->llvm_name, array_size, element_type);
                    free(element_type);
                }
            }
            break;
        }
        
        case AST_ASSIGNMENT: {
            if (node->data.assignment.name) {
                // Simple identifier assignment
                int value = generate_expression(node->data.assignment.value);
                
                symbol_t *sym = find_symbol(ctx.symbol_table, node->data.assignment.name);
                if (!sym) {
                    fprintf(stderr, "Undefined variable in assignment: %s\n", node->data.assignment.name);
                    return;
                }
                
                char *type_str = get_llvm_type_string(&sym->type_info);
                
                if (sym->is_parameter) {
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store %s %d, %s* %%%s.addr\n", 
                                type_str, value, type_str, sym->llvm_name);
                    } else {
                        fprintf(ctx.output, "  store %s %%t%d, %s* %%%s.addr\n", 
                                type_str, value, type_str, sym->llvm_name);
                    }
                } else {
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store %s %d, %s* %%%s\n", 
                                type_str, value, type_str, sym->llvm_name);
                    } else {
                        fprintf(ctx.output, "  store %s %%t%d, %s* %%%s\n", 
                                type_str, value, type_str, sym->llvm_name);
                    }
                }
                
                free(type_str);
            } else if (node->data.assignment.lvalue) {
                // Assignment to lvalue (array element, pointer dereference, etc.)
                int value = generate_expression(node->data.assignment.value);
                
                if (node->data.assignment.lvalue->type == AST_ARRAY_ACCESS) {
                    // Handle array[index] = value
                    ast_node_t *array = node->data.assignment.lvalue->data.array_access.array;
                    ast_node_t *index = node->data.assignment.lvalue->data.array_access.index;
                    
                    if (array->type == AST_IDENTIFIER) {
                        symbol_t *sym = find_symbol(ctx.symbol_table, array->data.identifier.name);
                        if (!sym) {
                            fprintf(stderr, "Undefined array in assignment: %s\n", array->data.identifier.name);
                            return;
                        }
                        
                        int index_val = generate_expression(index);
                        int addr_temp = get_next_temp();
                        
                        char index_str[32];
                        if (index->type == AST_NUMBER) {
                            snprintf(index_str, sizeof(index_str), "%d", index_val);
                        } else {
                            snprintf(index_str, sizeof(index_str), "%%t%d", index_val);
                        }
                        
                        char *element_type = get_llvm_type_string(&node->data.assignment.lvalue->data.array_access.element_type);
                        
                        if (sym->type_info.is_array) {
                            if (sym->is_parameter) {
                                int ptr_temp = get_next_temp();
                                fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s.addr\n", 
                                        ptr_temp, element_type, element_type, sym->llvm_name);
                                fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                        addr_temp, element_type, element_type, ptr_temp, index_str);
                            } else if (sym->type_info.is_vla) {
                                int ptr_temp = get_next_temp();
                                fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s\n", 
                                        ptr_temp, element_type, element_type, sym->llvm_name);
                                fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%t%d, i32 %s\n", 
                                        addr_temp, element_type, element_type, ptr_temp, index_str);
                            } else {
                                size_t array_length = sym->size / get_type_size(&sym->type_info);
                                fprintf(ctx.output, "  %%t%d = getelementptr [%zu x %s], [%zu x %s]* %%%s, i32 0, i32 %s\n", 
                                        addr_temp, array_length, element_type, array_length, element_type, 
                                        sym->llvm_name, index_str);
                            }
                        }
                        
                        // Store value
                        if (node->data.assignment.value->type == AST_NUMBER) {
                            fprintf(ctx.output, "  store %s %d, %s* %%t%d\n", 
                                    element_type, value, element_type, addr_temp);
                        } else {
                            fprintf(ctx.output, "  store %s %%t%d, %s* %%t%d\n", 
                                    element_type, value, element_type, addr_temp);
                        }
                        
                        free(element_type);
                    }
                } else if (node->data.assignment.lvalue->type == AST_DEREFERENCE) {
                    // Handle *pointer = value
                    int ptr = generate_expression(node->data.assignment.lvalue->data.dereference.operand);
                    char *result_type = get_llvm_type_string(&node->data.assignment.lvalue->data.dereference.result_type);
                    
                    char ptr_str[32];
                    if (node->data.assignment.lvalue->data.dereference.operand->type == AST_NUMBER) {
                        snprintf(ptr_str, sizeof(ptr_str), "%d", ptr);
                    } else {
                        snprintf(ptr_str, sizeof(ptr_str), "%%t%d", ptr);
                    }
                    
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store %s %d, %s* %s\n", 
                                result_type, value, result_type, ptr_str);
                    } else {
                        fprintf(ctx.output, "  store %s %%t%d, %s* %s\n", 
                                result_type, value, result_type, ptr_str);
                    }
                    
                    free(result_type);
                }
            }
            break;
        }
        
        case AST_IF_STMT: {
            char *then_label = generate_label("if_then");
            char *else_label = generate_label("if_else");
            char *end_label = generate_label("if_end");
            
            int cond = generate_expression(node->data.if_stmt.condition);
            int bool_temp;
            
            if (node->data.if_stmt.condition->type == AST_BINARY_OP &&
                is_comparison_op(node->data.if_stmt.condition->data.binary_op.op)) {
                bool_temp = cond;
            } else {
                bool_temp = get_next_temp();
                char cond_str[32];
                if (node->data.if_stmt.condition->type == AST_NUMBER) {
                    snprintf(cond_str, sizeof(cond_str), "%d", cond);
                } else {
                    snprintf(cond_str, sizeof(cond_str), "%%t%d", cond);
                }
                fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", bool_temp, cond_str);
            }
            
            if (node->data.if_stmt.else_stmt) {
                fprintf(ctx.output, "  br i1 %%t%d, label %%%s, label %%%s\n", 
                        bool_temp, then_label, else_label);
            } else {
                fprintf(ctx.output, "  br i1 %%t%d, label %%%s, label %%%s\n", 
                        bool_temp, then_label, end_label);
            }
            
            fprintf(ctx.output, "%s:\n", then_label);
            enter_scope(ctx.symbol_table);
            int prev_return_state = ctx.in_return_block;
            ctx.in_return_block = 0;
            generate_statement(node->data.if_stmt.then_stmt);
            if (!ctx.in_return_block) {
                fprintf(ctx.output, "  br label %%%s\n", end_label);
            }
            int then_returned = ctx.in_return_block;
            ctx.in_return_block = prev_return_state;
            exit_scope(ctx.symbol_table);
            
            if (node->data.if_stmt.else_stmt) {
                fprintf(ctx.output, "%s:\n", else_label);
                enter_scope(ctx.symbol_table);
                prev_return_state = ctx.in_return_block;
                ctx.in_return_block = 0;
                generate_statement(node->data.if_stmt.else_stmt);
                if (!ctx.in_return_block) {
                    fprintf(ctx.output, "  br label %%%s\n", end_label);
                }
                int else_returned = ctx.in_return_block;
                ctx.in_return_block = prev_return_state || (then_returned && else_returned);
                exit_scope(ctx.symbol_table);
            }
            
            fprintf(ctx.output, "%s:\n", end_label);
            
            free(then_label);
            free(else_label);
            free(end_label);
            break;
        }
        
        case AST_WHILE_STMT: {
            char *cond_label = generate_label("while_cond");
            char *body_label = generate_label("while_body");
            char *end_label = generate_label("while_end");
            
            // Save previous break/continue labels
            char *prev_break = ctx.current_break_label;
            char *prev_continue = ctx.current_continue_label;
            ctx.current_break_label = string_duplicate(end_label);
            ctx.current_continue_label = string_duplicate(cond_label);
            
            fprintf(ctx.output, "  br label %%%s\n", cond_label);
            fprintf(ctx.output, "%s:\n", cond_label);
            
            int cond = generate_expression(node->data.while_stmt.condition);
            int bool_temp;
            
            if (node->data.while_stmt.condition->type == AST_BINARY_OP &&
                is_comparison_op(node->data.while_stmt.condition->data.binary_op.op)) {
                bool_temp = cond;
            } else {
                bool_temp = get_next_temp();
                char cond_str[32];
                if (node->data.while_stmt.condition->type == AST_NUMBER) {
                    snprintf(cond_str, sizeof(cond_str), "%d", cond);
                } else {
                    snprintf(cond_str, sizeof(cond_str), "%%t%d", cond);
                }
                fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", bool_temp, cond_str);
            }
            
            fprintf(ctx.output, "  br i1 %%t%d, label %%%s, label %%%s\n", 
                    bool_temp, body_label, end_label);
            
            fprintf(ctx.output, "%s:\n", body_label);
            enter_scope(ctx.symbol_table);
            int prev_return_state = ctx.in_return_block;
            ctx.in_return_block = 0;
            generate_statement(node->data.while_stmt.body);
            if (!ctx.in_return_block) {
                fprintf(ctx.output, "  br label %%%s\n", cond_label);
            }
            ctx.in_return_block = prev_return_state;
            exit_scope(ctx.symbol_table);
            
            fprintf(ctx.output, "%s:\n", end_label);
            
            // Restore previous break/continue labels
            free(ctx.current_break_label);
            free(ctx.current_continue_label);
            ctx.current_break_label = prev_break;
            ctx.current_continue_label = prev_continue;
            
            free(cond_label);
            free(body_label);
            free(end_label);
            break;
        }
        
        case AST_FOR_STMT: {
            char *cond_label = generate_label("for_cond");
            char *body_label = generate_label("for_body");
            char *update_label = generate_label("for_update");
            char *end_label = generate_label("for_end");
            
            // Save previous break/continue labels
            char *prev_break = ctx.current_break_label;
            char *prev_continue = ctx.current_continue_label;
            ctx.current_break_label = string_duplicate(end_label);
            ctx.current_continue_label = string_duplicate(update_label);
            
            enter_scope(ctx.symbol_table);
            
            // Generate initialization
            if (node->data.for_stmt.init) {
                if (node->data.for_stmt.init->type == AST_DECLARATION ||
                    node->data.for_stmt.init->type == AST_ARRAY_DECL) {
                    generate_statement(node->data.for_stmt.init);
                } else {
                    generate_expression(node->data.for_stmt.init);
                }
            }
            
            fprintf(ctx.output, "  br label %%%s\n", cond_label);
            fprintf(ctx.output, "%s:\n", cond_label);
            
            // Generate condition
            if (node->data.for_stmt.condition) {
                int cond = generate_expression(node->data.for_stmt.condition);
                int bool_temp;
                
                if (node->data.for_stmt.condition->type == AST_BINARY_OP &&
                    is_comparison_op(node->data.for_stmt.condition->data.binary_op.op)) {
                    bool_temp = cond;
                } else {
                    bool_temp = get_next_temp();
                    char cond_str[32];
                    if (node->data.for_stmt.condition->type == AST_NUMBER) {
                        snprintf(cond_str, sizeof(cond_str), "%d", cond);
                    } else {
                        snprintf(cond_str, sizeof(cond_str), "%%t%d", cond);
                    }
                    fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", bool_temp, cond_str);
                }
                
                fprintf(ctx.output, "  br i1 %%t%d, label %%%s, label %%%s\n", 
                        bool_temp, body_label, end_label);
            } else {
                // No condition means infinite loop
                fprintf(ctx.output, "  br label %%%s\n", body_label);
            }
            
            // Generate body
            fprintf(ctx.output, "%s:\n", body_label);
            int prev_return_state = ctx.in_return_block;
            ctx.in_return_block = 0;
            generate_statement(node->data.for_stmt.body);
            if (!ctx.in_return_block) {
                fprintf(ctx.output, "  br label %%%s\n", update_label);
            }
            ctx.in_return_block = prev_return_state;
            
            // Generate update
            fprintf(ctx.output, "%s:\n", update_label);
            if (node->data.for_stmt.update) {
                generate_expression(node->data.for_stmt.update);
            }
            fprintf(ctx.output, "  br label %%%s\n", cond_label);
            
            fprintf(ctx.output, "%s:\n", end_label);
            
            exit_scope(ctx.symbol_table);
            
            // Restore previous break/continue labels
            free(ctx.current_break_label);
            free(ctx.current_continue_label);
            ctx.current_break_label = prev_break;
            ctx.current_continue_label = prev_continue;
            
            free(cond_label);
            free(body_label);
            free(update_label);
            free(end_label);
            break;
        }
        
        case AST_DO_WHILE_STMT: {
            char *body_label = generate_label("do_body");
            char *cond_label = generate_label("do_cond");
            char *end_label = generate_label("do_end");
            
            // Save previous break/continue labels
            char *prev_break = ctx.current_break_label;
            char *prev_continue = ctx.current_continue_label;
            ctx.current_break_label = string_duplicate(end_label);
            ctx.current_continue_label = string_duplicate(cond_label);
            
            fprintf(ctx.output, "  br label %%%s\n", body_label);
            fprintf(ctx.output, "%s:\n", body_label);
            
            enter_scope(ctx.symbol_table);
            int prev_return_state = ctx.in_return_block;
            ctx.in_return_block = 0;
            generate_statement(node->data.do_while_stmt.body);
            if (!ctx.in_return_block) {
                fprintf(ctx.output, "  br label %%%s\n", cond_label);
            }
            ctx.in_return_block = prev_return_state;
            exit_scope(ctx.symbol_table);
            
            fprintf(ctx.output, "%s:\n", cond_label);
            int cond = generate_expression(node->data.do_while_stmt.condition);
            int bool_temp;
            
            if (node->data.do_while_stmt.condition->type == AST_BINARY_OP &&
                is_comparison_op(node->data.do_while_stmt.condition->data.binary_op.op)) {
                bool_temp = cond;
            } else {
                bool_temp = get_next_temp();
                char cond_str[32];
                if (node->data.do_while_stmt.condition->type == AST_NUMBER) {
                    snprintf(cond_str, sizeof(cond_str), "%d", cond);
                } else {
                    snprintf(cond_str, sizeof(cond_str), "%%t%d", cond);
                }
                fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", bool_temp, cond_str);
            }
            
            fprintf(ctx.output, "  br i1 %%t%d, label %%%s, label %%%s\n", 
                    bool_temp, body_label, end_label);
            
            fprintf(ctx.output, "%s:\n", end_label);
            
            // Restore previous break/continue labels
            free(ctx.current_break_label);
            free(ctx.current_continue_label);
            ctx.current_break_label = prev_break;
            ctx.current_continue_label = prev_continue;
            
            free(body_label);
            free(cond_label);
            free(end_label);
            break;
        }
        
        case AST_SWITCH_STMT: {
            char *end_label = generate_label("switch_end");
            char *default_label = generate_label("switch_default");
            
            // Save previous break label
            char *prev_break = ctx.current_break_label;
            char *prev_switch_end = ctx.current_switch_end_label;
            ctx.current_break_label = string_duplicate(end_label);
            ctx.current_switch_end_label = string_duplicate(end_label);
            
            int switch_val = generate_expression(node->data.switch_stmt.expression);
            
            // For now, implement switch as a series of if-else statements
            // A more sophisticated implementation would use LLVM's switch instruction
            
            fprintf(ctx.output, "  br label %%%s\n", default_label);
            fprintf(ctx.output, "%s:\n", default_label);
            
            enter_scope(ctx.symbol_table);
            generate_statement(node->data.switch_stmt.body);
            if (!ctx.in_return_block) {
                fprintf(ctx.output, "  br label %%%s\n", end_label);
            }
            exit_scope(ctx.symbol_table);
            
            fprintf(ctx.output, "%s:\n", end_label);
            
            // Restore previous break label
            free(ctx.current_break_label);
            free(ctx.current_switch_end_label);
            ctx.current_break_label = prev_break;
            ctx.current_switch_end_label = prev_switch_end;
            
            free(end_label);
            free(default_label);
            break;
        }
        
        case AST_CASE_STMT:
        case AST_DEFAULT_STMT:
            // Case and default statements are handled within switch implementation
            // For now, just generate the statement part
            if (node->type == AST_CASE_STMT) {
                generate_statement(node->data.case_stmt.statement);
            } else {
                generate_statement(node->data.default_stmt.statement);
            }
            break;
        
        case AST_BREAK_STMT: {
            if (!ctx.current_break_label) {
                fprintf(stderr, "Break statement outside of loop or switch\n");
                return;
            }
            fprintf(ctx.output, "  br label %%%s\n", ctx.current_break_label);
            ctx.in_return_block = 1;
            break;
        }
        
        case AST_CONTINUE_STMT: {
            if (!ctx.current_continue_label) {
                fprintf(stderr, "Continue statement outside of loop\n");
                return;
            }
            fprintf(ctx.output, "  br label %%%s\n", ctx.current_continue_label);
            ctx.in_return_block = 1;
            break;
        }
        
        case AST_GOTO_STMT: {
            symbol_t *label_sym = find_label(ctx.symbol_table, node->data.goto_stmt.label);
            if (!label_sym) {
                // Forward declaration of label
                label_sym = add_label(ctx.symbol_table, node->data.goto_stmt.label);
                label_sym->label_defined = 0; // Mark as forward reference
            }
            fprintf(ctx.output, "  br label %%%s\n", node->data.goto_stmt.label);
            ctx.in_return_block = 1;
            break;
        }
        
        case AST_LABEL_STMT: {
            symbol_t *label_sym = find_label(ctx.symbol_table, node->data.label_stmt.label);
            if (!label_sym) {
                label_sym = add_label(ctx.symbol_table, node->data.label_stmt.label);
            }
            label_sym->label_defined = 1;
            
            if (ctx.in_return_block) {
                ctx.in_return_block = 0; // Label makes code reachable again
            }
            fprintf(ctx.output, "%s:\n", node->data.label_stmt.label);
            generate_statement(node->data.label_stmt.statement);
            break;
        }
        
        case AST_RETURN_STMT: {
            if (node->data.return_stmt.value) {
                int value = generate_expression(node->data.return_stmt.value);
                
                char *return_type = get_llvm_type_string(&ctx.current_function_return_type);
                
                if (node->data.return_stmt.value->type == AST_NUMBER) {
                    fprintf(ctx.output, "  ret %s %d\n", return_type, value);
                } else {
                    fprintf(ctx.output, "  ret %s %%t%d\n", return_type, value);
                }
                
                free(return_type);
            } else {
                fprintf(ctx.output, "  ret void\n");
            }
            ctx.in_return_block = 1;
            break;
        }
        
        case AST_EXPR_STMT: {
            if (node->data.expr_stmt.expr) {
                generate_expression(node->data.expr_stmt.expr);
            }
            break;
        }
        
        case AST_EMPTY_STMT: {
            // Empty statement - do nothing
            break;
        }
        
        case AST_STRUCT_DECL: {
            // Add struct to symbol table
            symbol_t *struct_sym = add_symbol(ctx.symbol_table, node->data.struct_decl.name, 
                                            SYM_STRUCT, create_type_info(string_duplicate(node->data.struct_decl.name), 0, 0, NULL));
            if (struct_sym && node->data.struct_decl.is_definition) {
                struct_sym->total_size = node->data.struct_decl.size;
                struct_sym->max_alignment = node->data.struct_decl.alignment;
                
                // Generate LLVM struct type definition
                generate_struct_type(struct_sym);
            }
            break;
        }
        
        case AST_UNION_DECL: {
            // Add union to symbol table
            symbol_t *union_sym = add_symbol(ctx.symbol_table, node->data.union_decl.name, 
                                           SYM_UNION, create_type_info(string_duplicate(node->data.union_decl.name), 0, 0, NULL));
            if (union_sym && node->data.union_decl.is_definition) {
                union_sym->total_size = node->data.union_decl.size;
                union_sym->max_alignment = node->data.union_decl.alignment;
                
                // Generate LLVM union type definition
                generate_union_type(union_sym);
            }
            break;
        }
        
        case AST_ENUM_DECL: {
            if (node->data.enum_decl.is_definition) {
                // Add enum constants to symbol table
                enum_value_t *value = node->data.enum_decl.values;
                int current_value = 0;
                
                while (value) {
                    int enum_val = value->value_expr ? 
                                  generate_expression(value->value_expr) : current_value;
                    add_enum_constant(ctx.symbol_table, value->name, enum_val);
                    current_value = enum_val + 1;
                    value = value->next;
                }
            }
            break;
        }
        
        default:
            fprintf(stderr, "Unknown statement type: %d\n", node->type);
            break;
    }
}

// Generate compound statement with proper scoping
static void generate_compound_statement(ast_node_t *node) {
    enter_scope(ctx.symbol_table);
    for (int i = 0; i < node->data.compound.stmt_count && !ctx.in_return_block; i++) {
        generate_statement(node->data.compound.statements[i]);
    }
    exit_scope(ctx.symbol_table);
}

// Generate function
static void generate_function(ast_node_t *node) {
    // Make deep copies for context to avoid double-free
    free(ctx.current_function_name); // Free previous if any
    ctx.current_function_name = string_duplicate(node->data.function.name);
    
    free_type_info(&ctx.current_function_return_type); // Free previous if any
    ctx.current_function_return_type = deep_copy_type_info_codegen(&node->data.function.return_type);
    
    ctx.in_return_block = 0;
    
    // Add function to symbol table
    symbol_t *func_sym = add_symbol(ctx.symbol_table, node->data.function.name, 
                                   SYM_FUNCTION, node->data.function.return_type);
    if (func_sym) {
        func_sym->is_function_defined = node->data.function.is_defined;
        func_sym->param_count = node->data.function.param_count;
        func_sym->is_variadic = node->data.function.is_variadic;
    }
    
    set_current_function(ctx.symbol_table, node->data.function.name);
    
    // Function declaration
    char *return_type_str = get_llvm_type_string(&node->data.function.return_type);
    fprintf(ctx.output, "define %s @%s(", return_type_str, node->data.function.name);
    
    // Parameters
    for (int i = 0; i < node->data.function.param_count; i++) {
        if (i > 0) fprintf(ctx.output, ", ");
        ast_node_t *param = node->data.function.params[i];
        char *param_type_str = get_llvm_type_string(&param->data.parameter.type_info);
        fprintf(ctx.output, "%s %%%s", param_type_str, param->data.parameter.name);
        free(param_type_str);
    }
    
    if (node->data.function.is_variadic && node->data.function.param_count > 0) {
        fprintf(ctx.output, ", ...");
    }
    
    fprintf(ctx.output, ") {\n");
    
    // Enter function scope
    enter_scope(ctx.symbol_table);
    
    // Add parameters to symbol table and allocate space
    for (int i = 0; i < node->data.function.param_count; i++) {
        ast_node_t *param = node->data.function.params[i];
        
        symbol_t *param_sym = add_symbol(ctx.symbol_table, param->data.parameter.name, 
                                        SYM_VARIABLE, param->data.parameter.type_info);
        if (param_sym) {
            param_sym->is_parameter = 1;
            
            char *param_type_str = get_llvm_type_string(&param->data.parameter.type_info);
            fprintf(ctx.output, "  %%%s.addr = alloca %s\n", param_sym->llvm_name, param_type_str);
            fprintf(ctx.output, "  store %s %%%s, %s* %%%s.addr\n", 
                    param_type_str, param->data.parameter.name, param_type_str, param_sym->llvm_name);
            free(param_type_str);
        }
    }
    
    // Function body
    generate_compound_statement(node->data.function.body);
    
    // Add default return if needed
    if (!ctx.in_return_block) {
        if (strcmp(node->data.function.return_type.base_type, "void") == 0) {
            fprintf(ctx.output, "  ret void\n");
        } else {
            fprintf(ctx.output, "  ret %s 0\n", return_type_str);
        }
    }
    
    fprintf(ctx.output, "}\n\n");
    
    // Exit function scope
    exit_scope(ctx.symbol_table);
    
    free(return_type_str);
}

// Main code generation function
void generate_llvm_ir(ast_node_t *ast, FILE *output) {
    // Initialize context
    ctx.output = output;
    ctx.symbol_table = create_symbol_table();
    ctx.label_counter = 0;
    ctx.temp_counter = 0;
    ctx.string_counter = 0;
    ctx.in_return_block = 0;
    ctx.current_break_label = NULL;
    ctx.current_continue_label = NULL;
    ctx.current_switch_end_label = NULL;
    ctx.current_function_name = NULL;
    ctx.current_function_return_type = create_type_info(string_duplicate("void"), 0, 0, NULL);
    ctx.string_literals = NULL;
    ctx.string_literal_count = 0;
    
    // Generate LLVM IR header
    fprintf(output, "; Complete Mini C Compiler - Generated LLVM IR\n");
    fprintf(output, "; Target: Full C subset with structs, unions, enums, and advanced features\n\n");
    
    // First pass: collect all type definitions and global declarations
    for (int i = 0; i < ast->data.program.decl_count; i++) {
        ast_node_t *decl = ast->data.program.declarations[i];
        
        switch (decl->type) {
            case AST_STRUCT_DECL:
            case AST_UNION_DECL:
            case AST_ENUM_DECL:
                generate_statement(decl);
                break;
            default:
                break;
        }
    }
    
    // Second pass: generate functions and other declarations
    for (int i = 0; i < ast->data.program.decl_count; i++) {
        ast_node_t *decl = ast->data.program.declarations[i];
        
        if (decl->type == AST_FUNCTION) {
            generate_function(decl);
        } else if (decl->type != AST_STRUCT_DECL && 
                   decl->type != AST_UNION_DECL && 
                   decl->type != AST_ENUM_DECL) {
            // Handle other global declarations
            generate_statement(decl);
        }
    }
    
    // Generate string constants at the end
    generate_string_constants();
    
    // Cleanup
    for (int i = 0; i < ctx.string_literal_count; i++) {
        free(ctx.string_literals[i].content);
    }
    free(ctx.string_literals);
    
    free(ctx.current_break_label);
    free(ctx.current_continue_label);
    free(ctx.current_switch_end_label);
    free(ctx.current_function_name);
    free_type_info(&ctx.current_function_return_type);
    
    destroy_symbol_table(ctx.symbol_table);
}
