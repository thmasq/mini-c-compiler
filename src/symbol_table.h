#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"

// Symbol types
typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_TYPEDEF,
    SYM_STRUCT,
    SYM_UNION,
    SYM_ENUM,
    SYM_ENUM_CONSTANT,
    SYM_LABEL
} symbol_type_t;

// Symbol table entry with complete information
typedef struct symbol {
    char *name;
    char *llvm_name;
    symbol_type_t sym_type;
    type_info_t type_info;
    
    // Scope and storage information
    int scope_level;
    int is_global;
    int is_parameter;
    int is_static;
    int is_extern;
    
    // Size and offset information
    size_t size;          // Size in bytes
    size_t alignment;     // Alignment requirement
    size_t offset;        // Offset in struct/union or stack frame
    
    // Array information
    int array_dimensions;
    size_t *array_sizes;  // Size of each dimension
    int is_vla;
    ast_node_t *vla_size_expr; // For VLAs
    
    // Function information
    ast_node_t **param_symbols;
    int param_count;
    int is_function_defined;
    int is_variadic;
    
    // Struct/Union/Enum information
    struct symbol **members;     // For struct/union members
    int member_count;
    size_t total_size;          // Total size of struct/union
    size_t max_alignment;       // Maximum alignment of members
    
    // Enum information
    int enum_value;             // For enum constants
    
    // Label information
    char *label_name;           // For goto labels
    int label_defined;
    
    // Linked list
    struct symbol *next;
} symbol_t;

// Scope management
typedef struct scope {
    symbol_t *symbols;
    int level;
    struct scope *parent;
} scope_t;

// Symbol table context
typedef struct symbol_table {
    scope_t *current_scope;
    scope_t *global_scope;
    int scope_counter;
    int temp_counter;
    char *current_function;
} symbol_table_t;

// Function prototypes
symbol_table_t *create_symbol_table(void);
void destroy_symbol_table(symbol_table_t *table);

// Scope management
void enter_scope(symbol_table_t *table);
void exit_scope(symbol_table_t *table);

// Symbol creation and management
symbol_t *create_symbol(const char *name, symbol_type_t sym_type, type_info_t type_info);
symbol_t *add_symbol(symbol_table_t *table, const char *name, symbol_type_t sym_type, type_info_t type_info);
symbol_t *find_symbol(symbol_table_t *table, const char *name);
symbol_t *find_symbol_in_scope(scope_t *scope, const char *name);

// Type size calculations
size_t calculate_type_size(type_info_t *type_info, symbol_table_t *table);
size_t calculate_type_alignment(type_info_t *type_info, symbol_table_t *table);
size_t calculate_struct_size(symbol_t *struct_sym);
size_t calculate_union_size(symbol_t *union_sym);

// Struct/Union member management
void add_struct_member(symbol_t *struct_sym, symbol_t *member);
symbol_t *find_struct_member(symbol_t *struct_sym, const char *member_name);
void calculate_struct_offsets(symbol_t *struct_sym);

// Enum management
symbol_t *add_enum_constant(symbol_table_t *table, const char *name, int value);

// Function management
void set_current_function(symbol_table_t *table, const char *func_name);
symbol_t *get_current_function(symbol_table_t *table);

// Label management
symbol_t *add_label(symbol_table_t *table, const char *label_name);
symbol_t *find_label(symbol_table_t *table, const char *label_name);

// Utility functions
char *generate_unique_name(symbol_table_t *table, const char *base_name);
int is_compatible_type(type_info_t *type1, type_info_t *type2);
type_info_t get_expression_type(ast_node_t *expr, symbol_table_t *table);

// Memory management helpers
void free_symbol(symbol_t *sym);
void cleanup_symbol_type_info(symbol_t *sym);

// Debug functions
void print_symbol_table(symbol_table_t *table);
void print_symbol(symbol_t *sym, int indent);

#endif // SYMBOL_TABLE_H
