#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"

#define SCOPE_BUCKETS 256

// Symbol types
typedef enum symbol_type {
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
	size_t size;      // Size in bytes
	size_t alignment; // Alignment requirement
	size_t offset;    // Offset in struct/union or stack frame

	// Array information
	int is_array;
	int array_len;
	int array_dimensions;
	size_t *array_sizes; // Size of each dimension
	int is_vla;
	ast_node_t *vla_size_expr; // For VLAs

	// Function information
	ast_node_t **param_symbols;
	int param_count;
	int is_function_defined;
	int is_variadic;

	// Struct/Union/Enum information
	struct symbol **members; // For struct/union members
	int member_count;
	size_t total_size;    // Total size of struct/union
	size_t max_alignment; // Maximum alignment of members

	// Enum information
	int enum_value; // For enum constants

	// Bit field information
	int bit_position; // Position within the storage unit

	// Label information
	char *label_name; // For goto labels
	int label_defined;

	// Forward declaration flag
	// int is_forward_decl;
	
	// Bit field information
	int bit_field_size; // número de bits (0 se não é bit field)

	// Hash table bucket chaining
	struct symbol *next;
} symbol_t;

// Scope management
typedef struct scope {
	symbol_t **buckets;
	size_t bucket_count; // normally SCOPE_BUCKETS
	size_t symbol_count;
	int level;
	struct scope *parent;
} scope_t;

// Pending labels (for goto validation)
typedef struct pending_label {
    char *name;
    int line_number;       // where it was referenced
    int referenced_count;  // how many times it was used
    int resolved;          // 1 if the label was defined
    struct pending_label *next;
} pending_label_t;

// Symbol table context
typedef struct symbol_table {
	scope_t *current_scope;
	scope_t *global_scope;
	int scope_counter;
	int temp_counter;
	char *current_function;
	pending_label_t *pending_labels;
} symbol_table_t;

// Main symbol table functions
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
type_info_t deep_copy_type_info(const type_info_t *src);
size_t symbol_table_hash(const char *src);
// size_t symbol_table_nhash(const char *src, size_t size);

// Memory management helpers
void free_symbol(symbol_t *sym);
void cleanup_symbol_type_info(symbol_t *sym);

// Debug functions
void print_symbol_table(symbol_table_t *table);
void print_symbol(symbol_t *sym, int indent);

// Resolução de typedefs
int resolve_typedef(type_info_t *t, symbol_table_t *table);

// Checagem de tipos runtime/incompletos (arrays/VLA)
int is_runtime_sized(const type_info_t *t);

// Validação de storage class e qualifiers
int validate_storage_combo(symbol_t *sym);
int validate_qualifiers(type_info_t *t1, type_info_t *t2);

// Validação de redeclaração de função
int validate_function_redeclaration(symbol_t *old, symbol_t *new);

// Checagem de modificabilidade (const correctness)
int can_modify_lvalue(ast_node_t *lv, symbol_table_t *table);

// (Opcional) Para bit fields, se for acessar fora de symbol_table.c:
// size_t calculate_struct_size_with_bitfields(symbol_t *struct_sym);

// Pending label management
void register_goto(symbol_table_t *table, const char *label_name, int line_number);
void register_label_definition(symbol_table_t *table, const char *label_name);
void check_pending_labels(symbol_table_t *table);
void clear_pending_labels(symbol_table_t *table);

#endif // SYMBOL_TABLE_H
