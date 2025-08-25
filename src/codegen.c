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
    char *type;
    int is_global;
    struct symbol *next;
} symbol_t;

// Code generation context
typedef struct {
    FILE *output;
    int label_counter;
    int temp_counter;
    symbol_t *symbols;
    char *current_function;
} codegen_context_t;

static codegen_context_t ctx;

// Symbol table functions
static void add_symbol(const char *name, const char *type, int is_global) {
    symbol_t *sym = malloc(sizeof(symbol_t));
    sym->name = string_duplicate(name);
    sym->type = string_duplicate(type);
    sym->is_global = is_global;
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

// Convert C type to LLVM type
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
            // We'll handle this differently for different contexts
            return node->data.number.value;
        }
        
        case AST_IDENTIFIER: {
            symbol_t *sym = find_symbol(node->data.identifier.name);
            if (!sym) {
                fprintf(stderr, "Undefined variable: %s\n", node->data.identifier.name);
                return -1;
            }
            
            int temp = get_next_temp();
            fprintf(ctx.output, "  %%t%d = load %s, %s* %%%s\n", 
                    temp, get_llvm_type(sym->type), get_llvm_type(sym->type), 
                    node->data.identifier.name);
            return temp;
        }
        
        case AST_BINARY_OP: {
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
            
            if (node->data.binary_op.op >= OP_EQ) {
                fprintf(ctx.output, "  %%t%d = %s i32 %s, %s\n", 
                        temp, op_str, left_operand, right_operand);
            } else {
                fprintf(ctx.output, "  %%t%d = %s i32 %s, %s\n", 
                        temp, op_str, left_operand, right_operand);
            }
            
            return temp;
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
            fprintf(stderr, "Unknown expression type\n");
            return -1;
    }
}

static void generate_statement(ast_node_t *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_DECLARATION: {
            add_symbol(node->data.declaration.name, node->data.declaration.type, 0);
            
            fprintf(ctx.output, "  %%%s = alloca %s\n", 
                    node->data.declaration.name, 
                    get_llvm_type(node->data.declaration.type));
            
            if (node->data.declaration.init) {
                int init_value = generate_expression(node->data.declaration.init);
                
                if (node->data.declaration.init->type == AST_NUMBER) {
                    fprintf(ctx.output, "  store %s %d, %s* %%%s\n", 
                            get_llvm_type(node->data.declaration.type), init_value,
                            get_llvm_type(node->data.declaration.type), 
                            node->data.declaration.name);
                } else {
                    fprintf(ctx.output, "  store %s %%t%d, %s* %%%s\n", 
                            get_llvm_type(node->data.declaration.type), init_value,
                            get_llvm_type(node->data.declaration.type), 
                            node->data.declaration.name);
                }
            }
            break;
        }
        
        case AST_ASSIGNMENT: {
            int value = generate_expression(node->data.assignment.value);
            
            if (node->data.assignment.value->type == AST_NUMBER) {
                fprintf(ctx.output, "  store i32 %d, i32* %%%s\n", 
                        value, node->data.assignment.name);
            } else {
                fprintf(ctx.output, "  store i32 %%t%d, i32* %%%s\n", 
                        value, node->data.assignment.name);
            }
            break;
        }
        
        case AST_IF_STMT: {
            int cond = generate_expression(node->data.if_stmt.condition);
            int then_label = get_next_label();
            int else_label = get_next_label();
            int end_label = get_next_label();
            
            char cond_str[32];
            if (node->data.if_stmt.condition->type == AST_NUMBER) {
                snprintf(cond_str, sizeof(cond_str), "%d", cond);
            } else {
                snprintf(cond_str, sizeof(cond_str), "%%t%d", cond);
            }
            
            // Convert condition to i1
            int bool_temp = get_next_temp();
            fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", bool_temp, cond_str);
            
            if (node->data.if_stmt.else_stmt) {
                fprintf(ctx.output, "  br i1 %%t%d, label %%L%d, label %%L%d\n", 
                        bool_temp, then_label, else_label);
            } else {
                fprintf(ctx.output, "  br i1 %%t%d, label %%L%d, label %%L%d\n", 
                        bool_temp, then_label, end_label);
            }
            
            fprintf(ctx.output, "L%d:\n", then_label);
            generate_statement(node->data.if_stmt.then_stmt);
            fprintf(ctx.output, "  br label %%L%d\n", end_label);
            
            if (node->data.if_stmt.else_stmt) {
                fprintf(ctx.output, "L%d:\n", else_label);
                generate_statement(node->data.if_stmt.else_stmt);
                fprintf(ctx.output, "  br label %%L%d\n", end_label);
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
            
            char cond_str[32];
            if (node->data.while_stmt.condition->type == AST_NUMBER) {
                snprintf(cond_str, sizeof(cond_str), "%d", cond);
            } else {
                snprintf(cond_str, sizeof(cond_str), "%%t%d", cond);
            }
            
            int bool_temp = get_next_temp();
            fprintf(ctx.output, "  %%t%d = icmp ne i32 %s, 0\n", bool_temp, cond_str);
            fprintf(ctx.output, "  br i1 %%t%d, label %%L%d, label %%L%d\n", 
                    bool_temp, body_label, end_label);
            
            fprintf(ctx.output, "L%d:\n", body_label);
            generate_statement(node->data.while_stmt.body);
            fprintf(ctx.output, "  br label %%L%d\n", cond_label);
            
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
    for (int i = 0; i < node->data.compound.stmt_count; i++) {
        generate_statement(node->data.compound.statements[i]);
    }
}

static void generate_function(ast_node_t *node) {
    ctx.current_function = node->data.function.name;
    
    // Function declaration
    fprintf(ctx.output, "define %s @%s(", 
            get_llvm_type(node->data.function.return_type),
            node->data.function.name);
    
    // Parameters
    for (int i = 0; i < node->data.function.param_count; i++) {
        if (i > 0) fprintf(ctx.output, ", ");
        ast_node_t *param = node->data.function.params[i];
        fprintf(ctx.output, "%s %%%s", 
                get_llvm_type(param->data.parameter.type),
                param->data.parameter.name);
        
        // Add parameter to symbol table
        add_symbol(param->data.parameter.name, param->data.parameter.type, 0);
    }
    
    fprintf(ctx.output, ") {\n");
    
    // Allocate space for parameters
    for (int i = 0; i < node->data.function.param_count; i++) {
        ast_node_t *param = node->data.function.params[i];
        fprintf(ctx.output, "  %%%s.addr = alloca %s\n", 
                param->data.parameter.name, 
                get_llvm_type(param->data.parameter.type));
        fprintf(ctx.output, "  store %s %%%s, %s* %%%s.addr\n", 
                get_llvm_type(param->data.parameter.type), 
                param->data.parameter.name,
                get_llvm_type(param->data.parameter.type), 
                param->data.parameter.name);
    }
    
    // Function body
    generate_compound_statement(node->data.function.body);
    
    // Add default return if needed
    if (strcmp(node->data.function.return_type, "void") == 0) {
        fprintf(ctx.output, "  ret void\n");
    }
    
    fprintf(ctx.output, "}\n\n");
}

void generate_llvm_ir(ast_node_t *ast, FILE *output) {
    // Initialize context
    ctx.output = output;
    ctx.label_counter = 0;
    ctx.temp_counter = 0;
    ctx.symbols = NULL;
    ctx.current_function = NULL;
    
    // Generate LLVM IR header
    fprintf(output, "; Mini C Compiler - Generated LLVM IR\n\n");
    
    // Generate functions
    for (int i = 0; i < ast->data.program.func_count; i++) {
        generate_function(ast->data.program.functions[i]);
        // Clear symbols for next function (simple approach)
        while (ctx.symbols) {
            symbol_t *temp = ctx.symbols;
            ctx.symbols = ctx.symbols->next;
            free(temp->name);
            free(temp->type);
            free(temp);
        }
    }
}
