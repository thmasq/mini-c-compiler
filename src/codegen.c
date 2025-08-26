#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Symbol table entry
typedef struct symbol {
    char *name;
    char *llvm_name;
    type_info_t type_info;
    int is_global;
    int is_parameter;
    int scope_level;
    struct symbol *next;
} symbol_t;

// Code generation context
typedef struct {
    FILE *output;
    int label_counter;
    int temp_counter;
    int scope_counter;
    int var_counter;
    symbol_t *symbols;
    char *current_function;
    int in_return_block;
} codegen_context_t;

static codegen_context_t ctx;

// Helper function to check if an AST node is a comparison or logical operation
static int is_comparison_or_logical_op(ast_node_t *node) {
    if (node && node->type == AST_BINARY_OP) {
        binary_op_t op = node->data.binary_op.op;
        return (op >= OP_EQ && op <= OP_GE) || (op == OP_LAND) || (op == OP_LOR);
    }
    return 0;
}

// Generate a unique LLVM variable name
static char *generate_unique_llvm_name(const char *base_name, int is_parameter) {
    char *llvm_name = malloc(256);
    if (!llvm_name) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    const char *func_name = ctx.current_function ? ctx.current_function : "global";
    
    if (is_parameter) {
        // Parameters include function name for clarity
        snprintf(llvm_name, 256, "%s.%s", func_name, base_name);
    } else {
        // Local variables get function.name.scope.counter format
        snprintf(llvm_name, 256, "%s.%s.%d.%d", func_name, base_name, ctx.scope_counter, ++ctx.var_counter);
    }
    
    return llvm_name;
}

// Scope management functions
static void enter_scope() {
    ctx.scope_counter++;
}

static void exit_scope() {
    ctx.scope_counter--;
    
    // Remove symbols from this scope level
    symbol_t **current = &ctx.symbols;
    while (*current) {
        if ((*current)->scope_level > ctx.scope_counter) {
            symbol_t *to_remove = *current;
            *current = (*current)->next;
            free(to_remove->name);
            free(to_remove->llvm_name);
            free_type_info(&to_remove->type_info);
            free(to_remove);
        } else {
            current = &(*current)->next;
        }
    }
}

// Symbol table functions
static void add_symbol(const char *name, type_info_t type_info, int is_global, int is_parameter) {
    symbol_t *sym = malloc(sizeof(symbol_t));
    sym->name = string_duplicate(name);
    sym->llvm_name = generate_unique_llvm_name(name, is_parameter);
    
    // Deep copy type_info
    sym->type_info.base_type = string_duplicate(type_info.base_type);
    sym->type_info.pointer_level = type_info.pointer_level;
    sym->type_info.is_array = type_info.is_array;
    sym->type_info.is_vla = type_info.is_vla;
    sym->type_info.array_size = NULL; // Don't copy the AST node for now
    
    sym->is_global = is_global;
    sym->is_parameter = is_parameter;
    sym->scope_level = ctx.scope_counter;
    sym->next = ctx.symbols;
    ctx.symbols = sym;
}

static symbol_t *find_symbol(const char *name) {
    for (symbol_t *sym = ctx.symbols; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

static int get_next_temp() {
    return ++ctx.temp_counter;
}

static int get_next_label() {
    return ++ctx.label_counter;
}

// Convert type_info to LLVM type
static char *get_llvm_type_string(type_info_t *type_info) {
    char *result = malloc(256);
    if (!result) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    // Base type
    const char *base_llvm_type;
    if (strcmp(type_info->base_type, "int") == 0) {
        base_llvm_type = "i32";
    } else if (strcmp(type_info->base_type, "char") == 0) {
        base_llvm_type = "i8";
    } else if (strcmp(type_info->base_type, "void") == 0) {
        base_llvm_type = "void";
    } else {
        base_llvm_type = "i32"; // default
    }
    
    // Build type string with pointer levels
    strcpy(result, base_llvm_type);
    for (int i = 0; i < type_info->pointer_level; i++) {
        strcat(result, "*");
    }
    
    return result;
}

// Get the basic LLVM type (for compatibility with old code)
static const char *get_llvm_type(const char *c_type) {
    if (strcmp(c_type, "int") == 0) return "i32";
    if (strcmp(c_type, "char") == 0) return "i8";
    if (strcmp(c_type, "void") == 0) return "void";
    return "i32"; // default
}

// Forward declarations
static int generate_expression(ast_node_t *node);
static void generate_statement(ast_node_t *node);
static void generate_compound_statement(ast_node_t *node);

static int generate_expression(ast_node_t *node) {
    if (!node) return -1;
    
    switch (node->type) {
        case AST_NUMBER: {
            // Constants don't need temp registers, just return the value
            return node->data.number.value;
        }
        
        case AST_IDENTIFIER: {
            symbol_t *sym = find_symbol(node->data.identifier.name);
            if (!sym) {
                fprintf(stderr, "Undefined variable: %s\n", node->data.identifier.name);
                return -1;
            }
            
            int temp = get_next_temp();
            char *type_str = get_llvm_type_string(&sym->type_info);
            
            // Handle arrays - they decay to pointers
            if (sym->type_info.is_array) {
                // For arrays, return pointer to first element
                if (sym->is_parameter) {
                    fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s.addr\n", 
                            temp, type_str, type_str, sym->llvm_name);
                } else {
                    fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%%s, i32 0, i32 0\n", 
                            temp, get_llvm_type(sym->type_info.base_type), 
                            get_llvm_type(sym->type_info.base_type), sym->llvm_name);
                }
            } else {
                // Regular variables and pointers
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
        
        case AST_ADDRESS_OF: {
            ast_node_t *operand = node->data.address_of.operand;
            
            if (operand->type == AST_IDENTIFIER) {
                symbol_t *sym = find_symbol(operand->data.identifier.name);
                if (!sym) {
                    fprintf(stderr, "Undefined variable in address-of: %s\n", operand->data.identifier.name);
                    return -1;
                }
                
                int temp = get_next_temp();
                
                // For arrays, return the array pointer directly
                if (sym->type_info.is_array) {
                    if (sym->is_parameter) {
                        fprintf(ctx.output, "  %%t%d = load %s*, %s** %%%s.addr\n", 
                                temp, get_llvm_type(sym->type_info.base_type),
                                get_llvm_type(sym->type_info.base_type), sym->llvm_name);
                    } else {
                        fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%%s, i32 0, i32 0\n", 
                                temp, get_llvm_type(sym->type_info.base_type), 
                                get_llvm_type(sym->type_info.base_type), sym->llvm_name);
                    }
                } else {
                    // For regular variables, return their address
                    if (sym->is_parameter) {
                        fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%%s.addr, i32 0\n", 
                                temp, get_llvm_type(sym->type_info.base_type),
                                get_llvm_type(sym->type_info.base_type), sym->llvm_name);
                    } else {
                        fprintf(ctx.output, "  %%t%d = getelementptr %s, %s* %%%s, i32 0\n", 
                                temp, get_llvm_type(sym->type_info.base_type),
                                get_llvm_type(sym->type_info.base_type), sym->llvm_name);
                    }
                }
                
                return temp;
            } else if (operand->type == AST_ARRAY_ACCESS) {
                // &array[index] - generate pointer arithmetic
                int array_ptr = generate_expression(operand->data.array_access.array);
                int index = generate_expression(operand->data.array_access.index);
                int temp = get_next_temp();
                
                char index_str[32];
                if (operand->data.array_access.index->type == AST_NUMBER) {
                    snprintf(index_str, sizeof(index_str), "%d", index);
                } else {
                    snprintf(index_str, sizeof(index_str), "%%t%d", index);
                }
                
                fprintf(ctx.output, "  %%t%d = getelementptr i32, i32* %%t%d, i32 %s\n", 
                        temp, array_ptr, index_str);
                return temp;
            }
            
            fprintf(stderr, "Invalid address-of operation\n");
            return -1;
        }
        
        case AST_DEREFERENCE: {
            int ptr = generate_expression(node->data.dereference.operand);
            int temp = get_next_temp();
            
            if (node->data.dereference.operand->type == AST_NUMBER) {
                // This shouldn't happen in well-formed C, but handle it
                fprintf(stderr, "Warning: Dereferencing constant\n");
                fprintf(ctx.output, "  %%t%d = inttoptr i32 %d to i32*\n", temp, ptr);
                int temp2 = get_next_temp();
                fprintf(ctx.output, "  %%t%d = load i32, i32* %%t%d\n", temp2, temp);
                return temp2;
            } else {
                fprintf(ctx.output, "  %%t%d = load i32, i32* %%t%d\n", temp, ptr);
                return temp;
            }
        }
        
        case AST_ARRAY_ACCESS: {
            // array[index] is equivalent to *(array + index)
            int array_ptr = generate_expression(node->data.array_access.array);
            int index = generate_expression(node->data.array_access.index);
            int temp = get_next_temp();
            
            char index_str[32];
            if (node->data.array_access.index->type == AST_NUMBER) {
                snprintf(index_str, sizeof(index_str), "%d", index);
            } else {
                snprintf(index_str, sizeof(index_str), "%%t%d", index);
            }
            
            // Generate pointer arithmetic
            fprintf(ctx.output, "  %%t%d = getelementptr i32, i32* %%t%d, i32 %s\n", 
                    temp, array_ptr, index_str);
            
            // Load the value
            int result = get_next_temp();
            fprintf(ctx.output, "  %%t%d = load i32, i32* %%t%d\n", result, temp);
            return result;
        }
        
        case AST_BINARY_OP: {
            // Handle logical operators with short-circuit evaluation
            if (node->data.binary_op.op == OP_LAND) {
                // Logical AND: left && right
                int left_label = get_next_label();
                int right_label = get_next_label();
                int end_label = get_next_label();
                int result_temp = get_next_temp();
                
                // Allocate result variable
                fprintf(ctx.output, "  %%t%d.addr = alloca i1\n", result_temp);
                
                // Evaluate left operand
                int left = generate_expression(node->data.binary_op.left);
                
                // Convert to boolean if needed
                int left_bool;
                if (is_comparison_or_logical_op(node->data.binary_op.left)) {
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
                
                // Short circuit: if left is false, result is false
                fprintf(ctx.output, "  br i1 %%t%d, label %%L%d, label %%L%d\n", 
                        left_bool, right_label, left_label);
                
                // Left is false - store false and jump to end
                fprintf(ctx.output, "L%d:\n", left_label);
                fprintf(ctx.output, "  store i1 false, i1* %%t%d.addr\n", result_temp);
                fprintf(ctx.output, "  br label %%L%d\n", end_label);
                
                // Left is true - evaluate right
                fprintf(ctx.output, "L%d:\n", right_label);
                int right = generate_expression(node->data.binary_op.right);
                
                // Convert to boolean if needed
                int right_bool;
                if (is_comparison_or_logical_op(node->data.binary_op.right)) {
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
                fprintf(ctx.output, "  br label %%L%d\n", end_label);
                
                // End - load result and convert to i32
                fprintf(ctx.output, "L%d:\n", end_label);
                int final_temp = get_next_temp();
                fprintf(ctx.output, "  %%t%d = load i1, i1* %%t%d.addr\n", final_temp, result_temp);
                int result = get_next_temp();
                fprintf(ctx.output, "  %%t%d = zext i1 %%t%d to i32\n", result, final_temp);
                
                return result;
                
            } else if (node->data.binary_op.op == OP_LOR) {
                // Logical OR: left || right
                int left_label = get_next_label();
                int right_label = get_next_label();
                int end_label = get_next_label();
                int result_temp = get_next_temp();
                
                // Allocate result variable
                fprintf(ctx.output, "  %%t%d.addr = alloca i1\n", result_temp);
                
                // Evaluate left operand
                int left = generate_expression(node->data.binary_op.left);
                
                // Convert to boolean if needed
                int left_bool;
                if (is_comparison_or_logical_op(node->data.binary_op.left)) {
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
                
                // Short circuit: if left is true, result is true
                fprintf(ctx.output, "  br i1 %%t%d, label %%L%d, label %%L%d\n", 
                        left_bool, left_label, right_label);
                
                // Left is true - store true and jump to end
                fprintf(ctx.output, "L%d:\n", left_label);
                fprintf(ctx.output, "  store i1 true, i1* %%t%d.addr\n", result_temp);
                fprintf(ctx.output, "  br label %%L%d\n", end_label);
                
                // Left is false - evaluate right
                fprintf(ctx.output, "L%d:\n", right_label);
                int right = generate_expression(node->data.binary_op.right);
                
                // Convert to boolean if needed
                int right_bool;
                if (is_comparison_or_logical_op(node->data.binary_op.right)) {
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
                fprintf(ctx.output, "  br label %%L%d\n", end_label);
                
                // End - load result and convert to i32
                fprintf(ctx.output, "L%d:\n", end_label);
                int final_temp = get_next_temp();
                fprintf(ctx.output, "  %%t%d = load i1, i1* %%t%d.addr\n", final_temp, result_temp);
                int result = get_next_temp();
                fprintf(ctx.output, "  %%t%d = zext i1 %%t%d to i32\n", result, final_temp);
                
                return result;
            } else {
                // Regular binary operations (non-short-circuit)
                int left = generate_expression(node->data.binary_op.left);
                int right = generate_expression(node->data.binary_op.right);
                int temp = get_next_temp();
                
                const char *op_str = "";
                switch (node->data.binary_op.op) {
                    case OP_ADD: op_str = "add"; break;
                    case OP_SUB: op_str = "sub"; break;
                    case OP_MUL: op_str = "mul"; break;
                    case OP_DIV: op_str = "sdiv"; break;
                    case OP_MOD: op_str = "srem"; break;
                    case OP_EQ: op_str = "icmp eq"; break;
                    case OP_NE: op_str = "icmp ne"; break;
                    case OP_LT: op_str = "icmp slt"; break;
                    case OP_LE: op_str = "icmp sle"; break;
                    case OP_GT: op_str = "icmp sgt"; break;
                    case OP_GE: op_str = "icmp sge"; break;
                    case OP_BAND: op_str = "and"; break;
                    case OP_BOR: op_str = "or"; break;
                    case OP_BXOR: op_str = "xor"; break;
                    case OP_LSHIFT: op_str = "shl"; break;
                    case OP_RSHIFT: op_str = "ashr"; break;
                    default:
                        fprintf(stderr, "Unknown binary operator\n");
                        return -1;
                }
                
                // Handle constants vs temporaries
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
                
                fprintf(ctx.output, "  %%t%d = %s i32 %s, %s\n", 
                        temp, op_str, left_operand, right_operand);
                
                return temp;
            }
        }
        
        case AST_UNARY_OP: {
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
            }
            
            return temp;
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
            fprintf(ctx.output, "  %%t%d = call i32 @%s(", temp, node->data.call.name);
            
            for (int i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0) fprintf(ctx.output, ", ");
                
                if (node->data.call.args[i]->type == AST_NUMBER) {
                    fprintf(ctx.output, "i32 %d", arg_temps[i]);
                } else {
                    fprintf(ctx.output, "i32 %%t%d", arg_temps[i]);
                }
            }
            
            fprintf(ctx.output, ")\n");
            
            free(arg_temps);
            return temp;
        }
        
        default:
            fprintf(stderr, "Unknown expression type in generate_expression\n");
            return -1;
    }
}

static void generate_statement(ast_node_t *node) {
    if (!node || ctx.in_return_block) return;
    
    switch (node->type) {
        case AST_DECLARATION: {
            add_symbol(node->data.declaration.name, node->data.declaration.type_info, 0, 0);
            
            // Find the symbol we just added to get its unique LLVM name
            symbol_t *sym = find_symbol(node->data.declaration.name);
            if (!sym) {
                fprintf(stderr, "Internal error: symbol not found after adding\n");
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
            add_symbol(node->data.array_decl.name, node->data.array_decl.type_info, 0, 0);
            
            symbol_t *sym = find_symbol(node->data.array_decl.name);
            if (!sym) {
                fprintf(stderr, "Internal error: symbol not found after adding\n");
                return;
            }
            
            if (node->data.array_decl.type_info.is_vla) {
                // Variable Length Array
                int size = generate_expression(node->data.array_decl.size);
                int temp = get_next_temp();
                
                char size_str[32];
                if (node->data.array_decl.size->type == AST_NUMBER) {
                    snprintf(size_str, sizeof(size_str), "%d", size);
                } else {
                    snprintf(size_str, sizeof(size_str), "%%t%d", size);
                }
                
                fprintf(ctx.output, "  %%t%d = alloca %s, i32 %s\n", 
                        temp, get_llvm_type(sym->type_info.base_type), size_str);
                fprintf(ctx.output, "  %%%s = alloca %s*\n", 
                        sym->llvm_name, get_llvm_type(sym->type_info.base_type));
                fprintf(ctx.output, "  store %s* %%t%d, %s** %%%s\n", 
                        get_llvm_type(sym->type_info.base_type), temp,
                        get_llvm_type(sym->type_info.base_type), sym->llvm_name);
            } else {
                // Fixed size array
                int size = 1;
                if (node->data.array_decl.size && node->data.array_decl.size->type == AST_NUMBER) {
                    size = node->data.array_decl.size->data.number.value;
                }
                
                fprintf(ctx.output, "  %%%s = alloca [%d x %s]\n", 
                        sym->llvm_name, size, get_llvm_type(sym->type_info.base_type));
            }
            break;
        }
        
        case AST_ASSIGNMENT: {
            if (node->data.assignment.name) {
                // Simple identifier assignment
                int value = generate_expression(node->data.assignment.value);
                
                symbol_t *sym = find_symbol(node->data.assignment.name);
                if (!sym) {
                    fprintf(stderr, "Undefined variable in assignment: %s\n", node->data.assignment.name);
                    return;
                }
                
                char *type_str = get_llvm_type_string(&sym->type_info);
                
                if (sym->is_parameter) {
                    // For parameters, store to .addr
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store %s %d, %s* %%%s.addr\n", 
                                type_str, value, type_str, sym->llvm_name);
                    } else {
                        fprintf(ctx.output, "  store %s %%t%d, %s* %%%s.addr\n", 
                                type_str, value, type_str, sym->llvm_name);
                    }
                } else {
                    // For local variables, store to unique name
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
                    
                    int array_ptr = generate_expression(array);
                    int index_val = generate_expression(index);
                    int addr_temp = get_next_temp();
                    
                    char index_str[32];
                    if (index->type == AST_NUMBER) {
                        snprintf(index_str, sizeof(index_str), "%d", index_val);
                    } else {
                        snprintf(index_str, sizeof(index_str), "%%t%d", index_val);
                    }
                    
                    // Generate address of array element
                    fprintf(ctx.output, "  %%t%d = getelementptr i32, i32* %%t%d, i32 %s\n", 
                            addr_temp, array_ptr, index_str);
                    
                    // Store value to that address
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store i32 %d, i32* %%t%d\n", value, addr_temp);
                    } else {
                        fprintf(ctx.output, "  store i32 %%t%d, i32* %%t%d\n", value, addr_temp);
                    }
                    
                } else if (node->data.assignment.lvalue->type == AST_DEREFERENCE) {
                    // Handle *pointer = value
                    int ptr = generate_expression(node->data.assignment.lvalue->data.dereference.operand);
                    
                    if (node->data.assignment.value->type == AST_NUMBER) {
                        fprintf(ctx.output, "  store i32 %d, i32* %%t%d\n", value, ptr);
                    } else {
                        fprintf(ctx.output, "  store i32 %%t%d, i32* %%t%d\n", value, ptr);
                    }
                }
            }
            break;
        }
        
        case AST_IF_STMT: {
            int cond = generate_expression(node->data.if_stmt.condition);
            int then_label = get_next_label();
            int else_label = get_next_label();
            int end_label = get_next_label();
            
            // Smart boolean conversion - avoid double conversion for comparison ops
            int bool_temp;
            if (is_comparison_or_logical_op(node->data.if_stmt.condition)) {
                // Condition already returns i1, use directly
                bool_temp = cond;
            } else {
                // Need to convert to boolean
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
                fprintf(ctx.output, "  br i1 %%t%d, label %%L%d, label %%L%d\n", 
                        bool_temp, then_label, else_label);
            } else {
                fprintf(ctx.output, "  br i1 %%t%d, label %%L%d, label %%L%d\n", 
                        bool_temp, then_label, end_label);
            }
            
            fprintf(ctx.output, "L%d:\n", then_label);
            enter_scope();
            int prev_return_state = ctx.in_return_block;
            ctx.in_return_block = 0;
            generate_statement(node->data.if_stmt.then_stmt);
            // Only generate branch if we haven't returned
            if (!ctx.in_return_block) {
                fprintf(ctx.output, "  br label %%L%d\n", end_label);
            }
            int then_returned = ctx.in_return_block;
            ctx.in_return_block = prev_return_state;
            exit_scope();
            
            if (node->data.if_stmt.else_stmt) {
                fprintf(ctx.output, "L%d:\n", else_label);
                enter_scope();
                prev_return_state = ctx.in_return_block;
                ctx.in_return_block = 0;
                generate_statement(node->data.if_stmt.else_stmt);
                if (!ctx.in_return_block) {
                    fprintf(ctx.output, "  br label %%L%d\n", end_label);
                }
                int else_returned = ctx.in_return_block;
                ctx.in_return_block = prev_return_state || (then_returned && else_returned);
                exit_scope();
            }
            
            fprintf(ctx.output, "L%d:\n", end_label);
            break;
        }
        
        case AST_WHILE_STMT: {
            int cond_label = get_next_label();
            int body_label = get_next_label();
            int end_label = get_next_label();
            
            fprintf(ctx.output, "  br label %%L%d\n", cond_label);
            fprintf(ctx.output, "L%d:\n", cond_label);
            
            int cond = generate_expression(node->data.while_stmt.condition);
            
            // Smart boolean conversion
            int bool_temp;
            if (is_comparison_or_logical_op(node->data.while_stmt.condition)) {
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
            
            fprintf(ctx.output, "  br i1 %%t%d, label %%L%d, label %%L%d\n", 
                    bool_temp, body_label, end_label);
            
            fprintf(ctx.output, "L%d:\n", body_label);
            enter_scope();
            int prev_return_state = ctx.in_return_block;
            ctx.in_return_block = 0;
            generate_statement(node->data.while_stmt.body);
            if (!ctx.in_return_block) {
                fprintf(ctx.output, "  br label %%L%d\n", cond_label);
            }
            ctx.in_return_block = prev_return_state;
            exit_scope();
            
            fprintf(ctx.output, "L%d:\n", end_label);
            break;
        }
        
        case AST_RETURN_STMT: {
            if (node->data.return_stmt.value) {
                int value = generate_expression(node->data.return_stmt.value);
                
                if (node->data.return_stmt.value->type == AST_NUMBER) {
                    fprintf(ctx.output, "  ret i32 %d\n", value);
                } else {
                    fprintf(ctx.output, "  ret i32 %%t%d\n", value);
                }
            } else {
                fprintf(ctx.output, "  ret void\n");
            }
            ctx.in_return_block = 1;
            break;
        }
        
        case AST_EXPR_STMT: {
            generate_expression(node->data.expr_stmt.expr);
            break;
        }
        
        case AST_COMPOUND_STMT: {
            generate_compound_statement(node);
            break;
        }
        
        default:
            fprintf(stderr, "Unknown statement type\n");
            break;
    }
}

static void generate_compound_statement(ast_node_t *node) {
    enter_scope();
    for (int i = 0; i < node->data.compound.stmt_count && !ctx.in_return_block; i++) {
        generate_statement(node->data.compound.statements[i]);
    }
    exit_scope();
}

static void generate_function(ast_node_t *node) {
    ctx.current_function = node->data.function.name;
    ctx.in_return_block = 0;
    
    // Function declaration
    char *return_type_str = get_llvm_type_string(&node->data.function.return_type);
    fprintf(ctx.output, "define %s @%s(", return_type_str, node->data.function.name);
    
    // Parameters
    for (int i = 0; i < node->data.function.param_count; i++) {
        if (i > 0) fprintf(ctx.output, ", ");
        ast_node_t *param = node->data.function.params[i];
        char *param_type_str = get_llvm_type_string(&param->data.parameter.type_info);
        fprintf(ctx.output, "%s %%%s", param_type_str, param->data.parameter.name);
        
        // Add parameter to symbol table - mark as parameter!
        add_symbol(param->data.parameter.name, param->data.parameter.type_info, 0, 1);
        
        free(param_type_str);
    }
    
    fprintf(ctx.output, ") {\n");
    
    // Allocate space for parameters
    for (int i = 0; i < node->data.function.param_count; i++) {
        ast_node_t *param = node->data.function.params[i];
        symbol_t *sym = find_symbol(param->data.parameter.name);
        char *param_type_str = get_llvm_type_string(&param->data.parameter.type_info);
        fprintf(ctx.output, "  %%%s.addr = alloca %s\n", sym->llvm_name, param_type_str);
        fprintf(ctx.output, "  store %s %%%s, %s* %%%s.addr\n", 
                param_type_str, param->data.parameter.name, param_type_str, sym->llvm_name);
        free(param_type_str);
    }
    
    // Function body
    generate_compound_statement(node->data.function.body);
    
    // Add default return if needed and we haven't already returned
    if (!ctx.in_return_block) {
        if (strcmp(node->data.function.return_type.base_type, "void") == 0) {
            fprintf(ctx.output, "  ret void\n");
        } else {
            // For non-void functions, we should add a default return 0
            fprintf(ctx.output, "  ret i32 0\n");
        }
    }
    
    fprintf(ctx.output, "}\n\n");
    
    free(return_type_str);
}

void generate_llvm_ir(ast_node_t *ast, FILE *output) {
    // Initialize context
    ctx.output = output;
    ctx.label_counter = 0;
    ctx.temp_counter = 0;
    ctx.scope_counter = 0;
    ctx.var_counter = 0;
    ctx.symbols = NULL;
    ctx.current_function = NULL;
    ctx.in_return_block = 0;
    
    // Generate LLVM IR header
    fprintf(output, "; Mini C Compiler - Generated LLVM IR with Pointer Support\n\n");
    
    // Generate functions
    for (int i = 0; i < ast->data.program.func_count; i++) {
        generate_function(ast->data.program.functions[i]);

        // Clean up symbols after each function
        while (ctx.symbols) {
            symbol_t *temp = ctx.symbols;
            ctx.symbols = ctx.symbols->next;
            free(temp->name);
            free(temp->llvm_name);
            free_type_info(&temp->type_info);
            free(temp);
        }
    }
}
