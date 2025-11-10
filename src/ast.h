#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration to avoid circular dependency
struct symbol_table;

// AST Node Types - Complete implementation
typedef enum {
	AST_PROGRAM,
	AST_FUNCTION,
	AST_COMPOUND_STMT,
	AST_DECLARATION,
	AST_ASSIGNMENT,
	AST_IF_STMT,
	AST_WHILE_STMT,
	AST_FOR_STMT,
	AST_DO_WHILE_STMT,
	AST_SWITCH_STMT,
	AST_CASE_STMT,
	AST_DEFAULT_STMT,
	AST_BREAK_STMT,
	AST_CONTINUE_STMT,
	AST_GOTO_STMT,
	AST_LABEL_STMT,
	AST_RETURN_STMT,
	AST_CALL,
	AST_BINARY_OP,
	AST_UNARY_OP,
	AST_IDENTIFIER,
	AST_NUMBER,
	AST_STRING_LITERAL,
	AST_CHARACTER,
	AST_PARAMETER,
	AST_EXPR_STMT,
	AST_ADDRESS_OF,
	AST_DEREFERENCE,
	AST_ARRAY_ACCESS,
	AST_ARRAY_DECL,
	AST_STRUCT_DECL,
	AST_UNION_DECL,
	AST_ENUM_DECL,
	AST_MEMBER_ACCESS,
	AST_PTR_MEMBER_ACCESS,
	AST_CAST,
	AST_SIZEOF,
	AST_INCREMENT,
	AST_DECREMENT,
	AST_CONDITIONAL,
	AST_INITIALIZER_LIST,
	AST_TYPEDEF,
	AST_EMPTY_STMT
} ast_node_type_t;

// Binary operators - Complete set
typedef enum {
	// Arithmetic operators
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	// Comparison operators
	OP_EQ,
	OP_NE,
	OP_LT,
	OP_LE,
	OP_GT,
	OP_GE,
	// Logical operators
	OP_LAND,
	OP_LOR,
	// Bitwise operators
	OP_BAND,
	OP_BOR,
	OP_BXOR,
	OP_LSHIFT,
	OP_RSHIFT,
	// Assignment operators
	OP_ASSIGN,
	OP_ADD_ASSIGN,
	OP_SUB_ASSIGN,
	OP_MUL_ASSIGN,
	OP_DIV_ASSIGN,
	OP_MOD_ASSIGN,
	OP_LSHIFT_ASSIGN,
	OP_RSHIFT_ASSIGN,
	OP_BAND_ASSIGN,
	OP_BOR_ASSIGN,
	OP_BXOR_ASSIGN
} binary_op_t;

// Unary operators - Complete set
typedef enum {
	OP_NEG,     // -x (unary minus)
	OP_NOT,     // !x (logical not)
	OP_BNOT,    // ~x (bitwise not)
	OP_PREINC,  // ++x
	OP_POSTINC, // x++
	OP_PREDEC,  // --x
	OP_POSTDEC  // x--
} unary_op_t;

// Storage class specifiers
typedef enum {
	STORAGE_NONE,
	STORAGE_AUTO,
	STORAGE_REGISTER,
	STORAGE_STATIC,
	STORAGE_EXTERN,
	STORAGE_TYPEDEF
} storage_class_t;

// Type qualifiers
typedef enum { QUAL_NONE = 0, QUAL_CONST = 1, QUAL_VOLATILE = 2, QUAL_RESTRICT = 4 } type_qualifier_t;

// Forward declaration
struct ast_node;

// Enhanced type information structure
typedef struct {
	char *base_type;   // int, char, void, struct_name, etc.
	int pointer_level; // number of * in declaration
	int is_array;
	int is_vla;      // variable length array
	int is_function; // function type
	int is_struct;
	int is_union;
	int is_enum;
	int is_incomplete; // for forward declarations
	storage_class_t storage_class;
	type_qualifier_t qualifiers;
	struct ast_node *array_size;
	struct ast_node **param_types; // for function types
	int param_count;
	int is_variadic; // for variadic functions
} type_info_t;

// Declarator information (used in parser)
typedef struct {
	char *name;
	int pointer_level;
	int is_array;
	int is_function;
	struct ast_node *array_size;
	struct ast_node **params;
	int param_count;
	int is_variadic;
} declarator_t;

// Struct/Union member information
typedef struct member_info {
	char *name;
	type_info_t type;
	int bit_field_size; // 0 if not a bit field
	struct ast_node *bit_field_expr;
	struct member_info *next;
} member_info_t;

// Enum value information
typedef struct enum_value {
	char *name;
	int value;
	struct ast_node *value_expr; // for computed enum values
	struct enum_value *next;
} enum_value_t;

// Case label information for switch statements
typedef struct case_label {
	struct ast_node *value;
	char *label_name;
	struct case_label *next;
} case_label_t;

// AST Node structure - Complete implementation
typedef struct ast_node {
	ast_node_type_t type;
	int line_number;
	int column;

	union {
		struct {
			struct ast_node **declarations; // global declarations and functions
			int decl_count;
		} program;

		struct {
			char *name;
			type_info_t return_type;
			struct ast_node **params;
			int param_count;
			struct ast_node *body;
			storage_class_t storage_class;
			int is_variadic;
			int is_defined; // vs just declared
		} function;

		struct {
			struct ast_node **statements;
			int stmt_count;
		} compound;

		struct {
			type_info_t type_info;
			char *name;
			struct ast_node *init;
			int is_parameter;
		} declaration;

		struct {
			char *name;
			struct ast_node *lvalue;
			struct ast_node *value;
			binary_op_t op; // for compound assignments like +=
		} assignment;

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
			struct ast_node *init;
			struct ast_node *condition;
			struct ast_node *update;
			struct ast_node *body;
		} for_stmt;

		struct {
			struct ast_node *body;
			struct ast_node *condition;
		} do_while_stmt;

		struct {
			struct ast_node *expression;
			struct ast_node *body;
			case_label_t *cases;
			char *default_label;
			char *break_label;
		} switch_stmt;

		struct {
			struct ast_node *value;
			struct ast_node *statement;
			char *label_name;
		} case_stmt;

		struct {
			struct ast_node *statement;
			char *label_name;
		} default_stmt;

		struct {
			char *label;
		} goto_stmt;

		struct {
			char *label;
			struct ast_node *statement;
		} label_stmt;

		struct {
			struct ast_node *value;
		} return_stmt;

		struct {
			char *name;
			struct ast_node **args;
			int arg_count;
			type_info_t return_type; // for type checking
		} call;

		struct {
			binary_op_t op;
			struct ast_node *left;
			struct ast_node *right;
			type_info_t result_type; // for type checking
		} binary_op;

		struct {
			unary_op_t op;
			struct ast_node *operand;
			type_info_t result_type; // for type checking
		} unary_op;

		struct {
			char *name;
			type_info_t type; // resolved type information
		} identifier;

		struct {
			int value;
		} number;

		struct {
			char *value;
			int length;
		} string_literal;

		struct {
			char value;
		} character;

		struct {
			type_info_t type_info;
			char *name;
		} parameter;

		struct {
			struct ast_node *expr;
		} expr_stmt;

		struct {
			struct ast_node *operand;
			type_info_t result_type;
		} address_of;

		struct {
			struct ast_node *operand;
			type_info_t result_type;
		} dereference;

		struct {
			struct ast_node *array;
			struct ast_node *index;
			type_info_t element_type;
		} array_access;

		struct {
			type_info_t type_info;
			char *name;
			struct ast_node *size;
			int is_vla;
		} array_decl;

		struct {
			char *name;
			member_info_t *members;
			int member_count;
			int is_definition;
			size_t size;
			size_t alignment;
		} struct_decl;

		struct {
			char *name;
			member_info_t *members;
			int member_count;
			int is_definition;
			size_t size;
			size_t alignment;
		} union_decl;

		struct {
			char *name;
			enum_value_t *values;
			int value_count;
			int is_definition;
			int next_value; // for auto-incrementing enum values
		} enum_decl;

		struct {
			struct ast_node *object;
			char *member;
			type_info_t member_type;
			size_t member_offset;
		} member_access;

		struct {
			struct ast_node *object;
			char *member;
			type_info_t member_type;
			size_t member_offset;
		} ptr_member_access;

		struct {
			type_info_t target_type;
			struct ast_node *expression;
		} cast;

		struct {
			struct ast_node *operand; // can be type or expression
			int is_type;
			size_t size_value; // computed size
		} sizeof_op;

		struct {
			struct ast_node *condition;
			struct ast_node *true_expr;
			struct ast_node *false_expr;
			type_info_t result_type;
		} conditional;

		struct {
			struct ast_node **values;
			int count;
			type_info_t element_type;
		} initializer_list;

		struct {
			type_info_t type;
			char *name;
		} typedef_decl;
	} data;
} ast_node_t;

// Function prototypes for AST creation
type_info_t create_type_info(char *base_type, int pointer_level, int is_array, ast_node_t *array_size);
declarator_t make_declarator(char *name, int pointer_level, int is_array, ast_node_t *array_size);

void print_ast(struct ast_node *node, int indent);

// Program and function creation
ast_node_t *create_program(ast_node_t **declarations, int decl_count);
ast_node_t *create_function(char *name, type_info_t return_type, ast_node_t **params, int param_count,
			    ast_node_t *body);

// Statement creation
ast_node_t *create_compound_stmt(ast_node_t **statements, int stmt_count);
ast_node_t *create_if_stmt(ast_node_t *condition, ast_node_t *then_stmt, ast_node_t *else_stmt);
ast_node_t *create_while_stmt(ast_node_t *condition, ast_node_t *body);
ast_node_t *create_for_stmt(ast_node_t *init, ast_node_t *condition, ast_node_t *update, ast_node_t *body);
ast_node_t *create_do_while_stmt(ast_node_t *body, ast_node_t *condition);
ast_node_t *create_switch_stmt(ast_node_t *expression, ast_node_t *body);
ast_node_t *create_case_stmt(ast_node_t *value, ast_node_t *statement);
ast_node_t *create_default_stmt(ast_node_t *statement);
ast_node_t *create_break_stmt(void);
ast_node_t *create_continue_stmt(void);
ast_node_t *create_goto_stmt(char *label);
ast_node_t *create_label_stmt(char *label, ast_node_t *statement);
ast_node_t *create_return_stmt(ast_node_t *value);
ast_node_t *create_expr_stmt(ast_node_t *expr);
ast_node_t *create_empty_stmt(void);

// Declaration creation
ast_node_t *create_declaration(type_info_t type_info, char *name, ast_node_t *init);
ast_node_t *create_array_declaration(type_info_t type_info, char *name, ast_node_t *size);
ast_node_t *create_struct_declaration(char *name, member_info_t *members, int is_definition);
ast_node_t *create_union_declaration(char *name, member_info_t *members, int is_definition);
ast_node_t *create_enum_declaration(char *name, enum_value_t *values, int is_definition);
ast_node_t *create_typedef(type_info_t type, char *name);

// Expression creation
ast_node_t *create_assignment(char *name, ast_node_t *value);
ast_node_t *create_assignment_to_lvalue(ast_node_t *lvalue, ast_node_t *value);
ast_node_t *create_compound_assignment(ast_node_t *lvalue, binary_op_t op, ast_node_t *value);
ast_node_t *create_call(char *name, ast_node_t **args, int arg_count);
ast_node_t *create_binary_op(binary_op_t op, ast_node_t *left, ast_node_t *right);
ast_node_t *create_unary_op(unary_op_t op, ast_node_t *operand);
ast_node_t *create_conditional(ast_node_t *condition, ast_node_t *true_expr, ast_node_t *false_expr);
ast_node_t *create_cast(type_info_t target_type, ast_node_t *expression);
ast_node_t *create_sizeof_expr(ast_node_t *operand);
ast_node_t *create_sizeof_type(type_info_t type);

// Primary expressions
ast_node_t *create_identifier(char *name);
ast_node_t *create_number(int value);
ast_node_t *create_string_literal(char *value);
ast_node_t *create_character(char value);
ast_node_t *create_parameter(type_info_t type_info, char *name);

// Pointer and array operations
ast_node_t *create_address_of(ast_node_t *operand);
ast_node_t *create_dereference(ast_node_t *operand);
ast_node_t *create_array_access(ast_node_t *array, ast_node_t *index);
ast_node_t *create_member_access(ast_node_t *object, char *member);
ast_node_t *create_ptr_member_access(ast_node_t *object, char *member);

// Initializers
ast_node_t *create_initializer_list(ast_node_t **values, int count);

// Helper functions
member_info_t *create_member_info(char *name, type_info_t type, int bit_field_size);
enum_value_t *create_enum_value(char *name, int value);
case_label_t *create_case_label(ast_node_t *value, char *label_name);

// Type system functions
type_info_t merge_declaration_specifiers(type_info_t base, declarator_t declarator);
int is_integer_type(type_info_t *type);
int is_floating_type(type_info_t *type);
int is_arithmetic_type(type_info_t *type);
int is_pointer_type(type_info_t *type);
int is_array_type(type_info_t *type);
int is_function_type(type_info_t *type);
int is_struct_type(type_info_t *type);
int is_union_type(type_info_t *type);
int is_enum_type(type_info_t *type);

// Type conversion and promotion
type_info_t perform_usual_arithmetic_conversions(type_info_t *type1, type_info_t *type2);
type_info_t perform_integer_promotions(type_info_t *type);
int can_convert_to(type_info_t *from, type_info_t *to);

// Memory management
void free_ast(ast_node_t *node);
void free_type_info(type_info_t *type_info);
void free_member_info(member_info_t *member);
void free_enum_value(enum_value_t *value);
void free_case_label(case_label_t *label);

#define CLEANUP_TYPE_INFO(var) \
    do { free_type_info(&(var)); } while(0)

#define AUTO_CLEANUP_TYPE __attribute__((cleanup(cleanup_type_wrapper)))

static inline void cleanup_type_wrapper(type_info_t *t)
{
	free_type_info(t);
}

// Code generation
void generate_llvm_ir(ast_node_t *ast, FILE *output);

// Type checking and semantic analysis
int check_types(ast_node_t *ast, struct symbol_table *table);
int check_expression_types(ast_node_t *expr, struct symbol_table *table);
int check_statement_types(ast_node_t *stmt, struct symbol_table *table);

// External references
extern FILE *yyin;
extern int yyparse();
extern ast_node_t *ast_root;
extern int error_count;
extern int line_number;
extern int column;

#endif
