%{
#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yylex();
extern int yyparse();
extern FILE *yyin;
extern int line_number;
extern int column;

void yyerror(const char *s);

ast_node_t *ast_root = NULL;
symbol_table_t *global_symbol_table = NULL;
// Error recovery globals
int error_count = 0;
int max_errors = 20;

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

// Helper to merge declaration specifiers and declarator with proper cleanup
static type_info_t make_complete_type(type_info_t base_type, declarator_t decl) {
    type_info_t result = deep_copy_type_info(&base_type);
    result.pointer_level = decl.pointer_level;
    result.is_array = decl.is_array;
    result.is_function = decl.is_function;
    result.array_size = decl.array_size;
    result.is_variadic = decl.is_variadic;
    if (decl.array_size && decl.array_size->type != AST_NUMBER) {
        result.is_vla = 1;
    } else {
        result.is_vla = 0;
    }
    if (decl.is_function) {
        result.param_count = decl.param_count;
        result.is_variadic = decl.is_variadic;
    }
    return result;
}

// Error recovery helpers
static ast_node_t *create_error_statement() {
    return create_expr_stmt(create_number(0));
}

static ast_node_t *create_error_expression() {
    return create_number(0);
}

// Helper to calculate sizes during parsing
static void calculate_and_store_sizes(ast_node_t *node) {
    if (!node || !global_symbol_table) return;
    switch (node->type) {
        case AST_SIZEOF:
            if (node->data.sizeof_op.is_type) {
                // Calculate size from type info stored in operand
                node->data.sizeof_op.size_value = 4;
                // Placeholder - will be calculated in semantic analysis
            } else if (node->data.sizeof_op.operand) {
                // Calculate size from expression type
                type_info_t expr_type = get_expression_type(node->data.sizeof_op.operand, global_symbol_table);
                node->data.sizeof_op.size_value = calculate_type_size(&expr_type, global_symbol_table);
                free_type_info(&expr_type);
            }
            break;
        default:
            break;
    }
}

// Safe cleanup for type_info_t
static void cleanup_type_info(type_info_t *type_info) {
    if (type_info && type_info->base_type) {
        free(type_info->base_type);
        type_info->base_type = NULL;
    }
}
%}

%union {
    int number;
    char character;
    char *string;
    ast_node_t *node;
    type_info_t type_info;
    declarator_t declarator;
    storage_class_t storage_class;
    type_qualifier_t qualifier;
    binary_op_t binary_op;
    unary_op_t unary_op;
    member_info_t *member_info;
    enum_value_t *enum_value;
    struct {
        ast_node_t **nodes;
        int count;
    } node_array;
    struct {
        member_info_t *members;
        int count;
    } member_array;
    struct {
        enum_value_t **values;
        int count;
    } enum_array;
    struct {
        declarator_t declarator;
        ast_node_t *initializer;
    } declarator_info;
    struct {
        declarator_t *declarators;
        ast_node_t **initializers;
        int count;
    } declarator_list;
}

/* Tokens */
%token <string> IDENTIFIER TYPE_NAME STRING_LITERAL
%token <number> CONSTANT
%token <character> CHARACTER

/* Keywords */
%token TYPEDEF EXTERN STATIC AUTO REGISTER INLINE RESTRICT
%token CHAR SHORT INT LONG SIGNED UNSIGNED FLOAT DOUBLE CONST VOLATILE VOID
%token BOOL COMPLEX IMAGINARY
%token STRUCT UNION ENUM ELLIPSIS
%token CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN
%token SIZEOF

/* Operators */
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN XOR_ASSIGN OR_ASSIGN

/* Punctuation */
%token SEMICOLON LBRACE RBRACE COMMA COLON ASSIGN LPAREN RPAREN
%token LBRACKET RBRACKET DOT AMPERSAND EXCLAMATION TILDE MINUS PLUS
%token ASTERISK SLASH PERCENT LESS_THAN GREATER_THAN CARET PIPE QUESTION

/* Non-terminals */
%type <node> translation_unit external_declaration function_definition
%type <node> declaration declaration_list initializer
%type <node> statement labeled_statement compound_statement expression_statement
%type <node> selection_statement iteration_statement jump_statement
%type <node> primary_expression postfix_expression unary_expression cast_expression
%type <node> multiplicative_expression additive_expression shift_expression
%type <node> relational_expression equality_expression and_expression
%type <node> exclusive_or_expression inclusive_or_expression logical_and_expression
%type <node> logical_or_expression conditional_expression assignment_expression
%type <node> expression constant_expression
%type <node> parameter_declaration abstract_declarator direct_abstract_declarator
%type <node> initializer_list designation designator

%type <type_info> declaration_specifiers type_specifier specifier_qualifier_list
%type <type_info> struct_or_union_specifier enum_specifier type_name

%type <declarator> declarator direct_declarator pointer
%type <declarator_info> init_declarator
%type <declarator_list> init_declarator_list
%type <storage_class> storage_class_specifier function_specifier
%type <qualifier> type_qualifier type_qualifier_list

%type <node_array> block_item_list parameter_type_list parameter_list argument_expression_list
%type <node_array> struct_declaration_list struct_declaration designator_list
%type <enum_array> enumerator_list enumerator

%type <member_array> struct_declarator_list
%type <member_info> struct_declarator
%type <enum_value> enumerator_item

%type <binary_op> assignment_operator
%type <string> struct_or_union

/* Operator precedence and associativity */
%right ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN XOR_ASSIGN OR_ASSIGN
%right QUESTION COLON
%left OR_OP
%left AND_OP
%left PIPE
%left CARET
%left AMPERSAND
%left EQ_OP NE_OP
%left LESS_THAN GREATER_THAN LE_OP GE_OP
%left LEFT_OP RIGHT_OP
%left PLUS MINUS
%left ASTERISK SLASH PERCENT
%right EXCLAMATION TILDE INC_OP DEC_OP SIZEOF UNARY_MINUS UNARY_PLUS ADDRESS_OF DEREFERENCE
%left DOT PTR_OP LBRACKET RBRACKET LPAREN RPAREN

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%start translation_unit

%%

/* Top level */
translation_unit
    : external_declaration {
        ast_node_t **decls;
        int count = 0;
        
        if ($1) {
            // Check if this is a compound statement (multiple declarations)
            if ($1->type == AST_COMPOUND_STMT) {
                count = $1->data.compound.stmt_count;
                decls = malloc(count * sizeof(ast_node_t*));
                for (int i = 0; i < count; i++) {
                    decls[i] = $1->data.compound.statements[i];
                }
                // Free the wrapper but keep the statements
                $1->data.compound.statements = NULL;
                $1->data.compound.stmt_count = 0;
                free_ast($1);
            } else {
                count = 1;
                decls = malloc(sizeof(ast_node_t*));
                decls[0] = $1;
            }
        } else {
            decls = NULL;
        }
        
        $$ = create_program(decls, count);
        ast_root = $$;
    }
    | translation_unit external_declaration {
        if ($2) {
            // Check if this is a compound statement (multiple declarations)
            if ($2->type == AST_COMPOUND_STMT) {
                int new_count = $2->data.compound.stmt_count;
                $$ = $1;
                
                int old_count = $$->data.program.decl_count;
                $$->data.program.decl_count += new_count;
                $$->data.program.declarations = realloc($$->data.program.declarations,
                                                       $$->data.program.decl_count * sizeof(ast_node_t*));
                
                for (int i = 0; i < new_count; i++) {
                    $$->data.program.declarations[old_count + i] = 
                        $2->data.compound.statements[i];
                }
                
                // Free the wrapper but keep the statements
                $2->data.compound.statements = NULL;
                $2->data.compound.stmt_count = 0;
                free_ast($2);
            } else {
                $$ = $1;
                $$->data.program.decl_count++;
                $$->data.program.declarations = realloc($$->data.program.declarations,
                                                       $$->data.program.decl_count * sizeof(ast_node_t*));
                $$->data.program.declarations[$$->data.program.decl_count - 1] = $2;
            }
        } else {
            $$ = $1;
        }
        ast_root = $$;
    }
    ;

external_declaration
    : function_definition { $$ = $1; }
    | declaration { $$ = $1; }
    | error SEMICOLON { $$ = NULL; yyerrok; }
    ;

/* Function definitions */
function_definition
    : declaration_specifiers declarator declaration_list compound_statement {
        type_info_t func_type = make_complete_type($1, $2);
        $$ = create_function(string_duplicate($2.name), func_type, NULL, 0, $4);
        cleanup_type_info(&$1);
        // func_type is now owned by the function node
    }
    | declaration_specifiers declarator compound_statement {
        type_info_t func_type = make_complete_type($1, $2);
        ast_node_t **params = NULL;
        int param_count = 0;

        if ($2.is_function && $2.params) {
            params = $2.params;
            param_count = $2.param_count;
        }

        $$ = create_function(string_duplicate($2.name), func_type, params, param_count, $3);
        cleanup_type_info(&$1);
        // func_type is now owned by the function node
    }
    ;

declaration_list
    : declaration { $$ = $1; }
    | declaration_list declaration { $$ = $2; }
    ;

/* Declarations */
declaration_specifiers
    : storage_class_specifier {
        $$ = create_type_info(string_duplicate("int"), 0, 0, NULL);
        $$.storage_class = $1;
    }
    | storage_class_specifier declaration_specifiers {
        $$ = $2;
        $$.storage_class = $1;
    }
    | type_specifier {
        $$ = $1;
    }
    | type_specifier declaration_specifiers {
        $$ = $1;
        if ($2.storage_class != STORAGE_NONE) $$.storage_class = $2.storage_class;
        $$.qualifiers |= $2.qualifiers;
        cleanup_type_info(&$2);
    }
    | type_qualifier {
        $$ = create_type_info(string_duplicate("int"), 0, 0, NULL);
        $$.qualifiers = $1;
    }
    | type_qualifier declaration_specifiers {
        $$ = $2;
        $$.qualifiers |= $1;
    }
    | function_specifier {
        $$ = create_type_info(string_duplicate("int"), 0, 0, NULL);
    }
    | function_specifier declaration_specifiers {
        $$ = $2;
    }
    ;

init_declarator_list
    : init_declarator {
        $$.declarators = malloc(sizeof(declarator_t));
        $$.initializers = malloc(sizeof(ast_node_t*));
        $$.declarators[0] = $1.declarator;
        $$.initializers[0] = $1.initializer;
        $$.count = 1;
    }
    | init_declarator_list COMMA init_declarator {
        $$.count = $1.count + 1;
        $$.declarators = realloc($1.declarators, $$.count * sizeof(declarator_t));
        $$.initializers = realloc($1.initializers, $$.count * sizeof(ast_node_t*));
        $$.declarators[$$.count - 1] = $3.declarator;
        $$.initializers[$$.count - 1] = $3.initializer;
    }
    ;

init_declarator
    : declarator {
        $$.declarator = $1;
        $$.initializer = NULL;
    }
    | declarator ASSIGN initializer {
        $$.declarator = $1;
        $$.initializer = $3;
    }
    ;

storage_class_specifier
    : TYPEDEF { $$ = STORAGE_TYPEDEF; }
    | EXTERN { $$ = STORAGE_EXTERN; }
    | STATIC { $$ = STORAGE_STATIC; }
    | AUTO { $$ = STORAGE_AUTO; }
    | REGISTER { $$ = STORAGE_REGISTER; }
    ;

type_specifier
    : VOID { $$ = create_type_info(string_duplicate("void"), 0, 0, NULL); }
    | CHAR { $$ = create_type_info(string_duplicate("char"), 0, 0, NULL); }
    | SHORT { $$ = create_type_info(string_duplicate("short"), 0, 0, NULL); }
    | INT { $$ = create_type_info(string_duplicate("int"), 0, 0, NULL); }
    | LONG { $$ = create_type_info(string_duplicate("long"), 0, 0, NULL); }
    | FLOAT { $$ = create_type_info(string_duplicate("float"), 0, 0, NULL); }
    | DOUBLE { $$ = create_type_info(string_duplicate("double"), 0, 0, NULL); }
    | SIGNED { $$ = create_type_info(string_duplicate("signed int"), 0, 0, NULL); }
    | UNSIGNED { $$ = create_type_info(string_duplicate("unsigned int"), 0, 0, NULL); }
    | BOOL { $$ = create_type_info(string_duplicate("_Bool"), 0, 0, NULL); }
    | COMPLEX { $$ = create_type_info(string_duplicate("_Complex"), 0, 0, NULL); }
    | IMAGINARY { $$ = create_type_info(string_duplicate("_Imaginary"), 0, 0, NULL); }
    | struct_or_union_specifier { $$ = $1; }
    | enum_specifier { $$ = $1; }
    | TYPE_NAME { 
        $$ = create_type_info(string_duplicate($1), 0, 0, NULL); 
        free($1);
    }
    ;

struct_or_union_specifier
    : struct_or_union IDENTIFIER LBRACE struct_declaration_list RBRACE {
        $$ = create_type_info(string_duplicate($2), 0, 0, NULL);
        $$.is_struct = (strcmp($1, "struct") == 0);
        $$.is_union = (strcmp($1, "union") == 0);
        free($1);
        free($2);
    }
    | struct_or_union LBRACE struct_declaration_list RBRACE {
        $$ = create_type_info(string_duplicate("anonymous"), 0, 0, NULL);
        $$.is_struct = (strcmp($1, "struct") == 0);
        $$.is_union = (strcmp($1, "union") == 0);
        free($1);
    }
    | struct_or_union IDENTIFIER {
        $$ = create_type_info(string_duplicate($2), 0, 0, NULL);
        $$.is_struct = (strcmp($1, "struct") == 0);
        $$.is_union = (strcmp($1, "union") == 0);
        $$.is_incomplete = 1;
        free($1);
        free($2);
    }
    ;

struct_or_union
    : STRUCT { $$ = string_duplicate("struct"); }
    | UNION { $$ = string_duplicate("union"); }
    ;

struct_declaration_list
    : struct_declaration { $$ = $1; }
    | struct_declaration_list struct_declaration {
        $$.count = $1.count + $2.count;
        $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
        for (int i = 0; i < $2.count; i++) {
            $$.nodes[$1.count + i] = $2.nodes[i];
        }
        free($2.nodes);
    }
    ;

struct_declaration
    : specifier_qualifier_list struct_declarator_list SEMICOLON {
        $$.count = $2.count;
        $$.nodes = malloc($$.count * sizeof(ast_node_t*));
        for (int i = 0; i < $$.count; i++) {
            member_info_t *member = &$2.members[i];
            type_info_t member_type = make_complete_type($1, make_declarator(member->name, 0, 0, NULL));
            $$.nodes[i] = create_declaration(member_type, string_duplicate(member->name), NULL);
        }
        free($2.members);
        cleanup_type_info(&$1);
    }
    ;

specifier_qualifier_list
    : type_specifier specifier_qualifier_list {
        $$ = $1;
        $$.qualifiers |= $2.qualifiers;
        cleanup_type_info(&$2);
    }
    | type_specifier { $$ = $1; }
    | type_qualifier specifier_qualifier_list {
        $$ = $2;
        $$.qualifiers |= $1;
    }
    | type_qualifier {
        $$ = create_type_info(string_duplicate("int"), 0, 0, NULL);
        $$.qualifiers = $1;
    }
    ;

struct_declarator_list
    : struct_declarator {
        $$.count = 1;
        $$.members = malloc(sizeof(member_info_t));
        $$.members[0] = *$1;
        free($1);
    }
    | struct_declarator_list COMMA struct_declarator {
        $$.count = $1.count + 1;
        $$.members = realloc($1.members, $$.count * sizeof(member_info_t));
        $$.members[$$.count - 1] = *$3;
        free($3);
    }
    ;

struct_declarator
    : declarator {
        $$ = create_member_info(string_duplicate($1.name),
                               create_type_info(string_duplicate("int"), $1.pointer_level, $1.is_array, $1.array_size), 0);
    }
    | COLON constant_expression {
        $$ = create_member_info(string_duplicate(""),
                               create_type_info(string_duplicate("int"), 0, 0, NULL),
                               $2->data.number.value);
        $$->bit_field_expr = $2;
    }
    | declarator COLON constant_expression {
        $$ = create_member_info(string_duplicate($1.name),
                               create_type_info(string_duplicate("int"), $1.pointer_level, $1.is_array, $1.array_size),
                               $3->data.number.value);
        $$->bit_field_expr = $3;
    }
    ;

enum_specifier
    : ENUM LBRACE enumerator_list RBRACE {
        $$ = create_type_info(string_duplicate("enum"), 0, 0, NULL);
        $$.is_enum = 1;
    }
    | ENUM IDENTIFIER LBRACE enumerator_list RBRACE {
        $$ = create_type_info(string_duplicate($2), 0, 0, NULL);
        $$.is_enum = 1;
        free($2);
    }
    | ENUM LBRACE enumerator_list COMMA RBRACE {
        $$ = create_type_info(string_duplicate("enum"), 0, 0, NULL);
        $$.is_enum = 1;
    }
    | ENUM IDENTIFIER LBRACE enumerator_list COMMA RBRACE {
        $$ = create_type_info(string_duplicate($2), 0, 0, NULL);
        $$.is_enum = 1;
        free($2);
    }
    | ENUM IDENTIFIER {
        $$ = create_type_info(string_duplicate($2), 0, 0, NULL);
        $$.is_enum = 1;
        $$.is_incomplete = 1;
        free($2);
    }
    ;

enumerator_list
    : enumerator { $$ = $1; }
    | enumerator_list COMMA enumerator {
        $$.count = $1.count + $3.count;
        $$.values = realloc($1.values, $$.count * sizeof(enum_value_t*));
        for (int i = 0; i < $3.count; i++) {
            $$.values[$1.count + i] = $3.values[i];
        }
        free($3.values);
    }
    ;

enumerator
    : enumerator_item {
        $$.count = 1;
        $$.values = malloc(sizeof(enum_value_t*));
        $$.values[0] = $1;
    }
    ;

enumerator_item
    : IDENTIFIER {
        $$ = create_enum_value(string_duplicate($1), 0);
        free($1);
    }
    | IDENTIFIER ASSIGN constant_expression {
        int value = ($3->type == AST_NUMBER) ?
        $3->data.number.value : 0;
        $$ = create_enum_value(string_duplicate($1), value);
        $$->value_expr = $3;
        free($1);
    }
    ;

type_qualifier
    : CONST { $$ = QUAL_CONST; }
    | RESTRICT { $$ = QUAL_RESTRICT; }
    | VOLATILE { $$ = QUAL_VOLATILE; }
    ;

function_specifier
    : INLINE { $$ = STORAGE_NONE; }
    ;

/* Declarators */
declarator
    : pointer direct_declarator {
        $$ = $2;
        $$.pointer_level = $1.pointer_level;
    }
    | direct_declarator { $$ = $1; }
    ;

direct_declarator
    : IDENTIFIER {
        $$ = make_declarator(string_duplicate($1), 0, 0, NULL);
        free($1);
    }
    | LPAREN declarator RPAREN {
        $$ = $2;
    }
    | direct_declarator LBRACKET type_qualifier_list assignment_expression RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = $4;
    }
    | direct_declarator LBRACKET type_qualifier_list RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = NULL;
    }
    | direct_declarator LBRACKET assignment_expression RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = $3;
    }
    | direct_declarator LBRACKET STATIC type_qualifier_list assignment_expression RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = $5;
    }
    | direct_declarator LBRACKET type_qualifier_list STATIC assignment_expression RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = $5;
    }
    | direct_declarator LBRACKET type_qualifier_list ASTERISK RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = NULL;
    }
    | direct_declarator LBRACKET ASTERISK RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = NULL;
    }
    | direct_declarator LBRACKET RBRACKET {
        $$ = $1;
        $$.is_array = 1;
        $$.array_size = NULL;
    }
    | direct_declarator LPAREN parameter_type_list RPAREN {
        $$ = $1;
        $$.is_function = 1;
        $$.params = $3.nodes;
        $$.param_count = $3.count;
    }
    | direct_declarator LPAREN RPAREN {
        $$ = $1;
        $$.is_function = 1;
        $$.params = NULL;
        $$.param_count = 0;
    }
    ;

pointer
    : ASTERISK {
        $$ = make_declarator(NULL, 1, 0, NULL);
    }
    | ASTERISK type_qualifier_list {
        $$ = make_declarator(NULL, 1, 0, NULL);
    }
    | ASTERISK pointer {
        $$ = $2;
        $$.pointer_level += 1;
    }
    | ASTERISK type_qualifier_list pointer {
        $$ = $3;
        $$.pointer_level += 1;
    }
    ;

type_qualifier_list
    : type_qualifier { $$ = $1; }
    | type_qualifier_list type_qualifier { $$ = $1 | $2; }
    ;

parameter_type_list
    : parameter_list { $$ = $1; }
    | parameter_list COMMA ELLIPSIS {
        $$ = $1;
        // Mark as variadic - would need to track this in declarator
    }
    ;

parameter_list
    : parameter_declaration {
        $$.nodes = malloc(sizeof(ast_node_t*));
        $$.nodes[0] = $1;
        $$.count = 1;
    }
    | parameter_list COMMA parameter_declaration {
        $$.count = $1.count + 1;
        $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
        $$.nodes[$$.count - 1] = $3;
    }
    ;

parameter_declaration
    : declaration_specifiers declarator {
        type_info_t param_type = make_complete_type($1, $2);
        $$ = create_parameter(param_type, string_duplicate($2.name));
        cleanup_type_info(&$1);
    }
    | declaration_specifiers abstract_declarator {
        type_info_t param_type = deep_copy_type_info(&$1);
        $$ = create_parameter(param_type, string_duplicate(""));
        cleanup_type_info(&$1);
    }
    | declaration_specifiers {
        type_info_t param_type = deep_copy_type_info(&$1);
        $$ = create_parameter(param_type, string_duplicate(""));
        cleanup_type_info(&$1);
    }
    ;

type_name
    : specifier_qualifier_list { $$ = $1; }
    | specifier_qualifier_list abstract_declarator { 
        $$ = $1; 
        // Apply abstract declarator modifications here if needed
    }
    ;

abstract_declarator
    : pointer { $$ = NULL; }
    | direct_abstract_declarator { $$ = $1; }
    | pointer direct_abstract_declarator { $$ = $2; }
    ;

direct_abstract_declarator
    : LPAREN abstract_declarator RPAREN { $$ = $2; }
    | LBRACKET RBRACKET { $$ = NULL; }
    | LBRACKET assignment_expression RBRACKET { $$ = NULL; }
    | direct_abstract_declarator LBRACKET RBRACKET { $$ = $1; }
    | direct_abstract_declarator LBRACKET assignment_expression RBRACKET { $$ = $1; }
    | LBRACKET ASTERISK RBRACKET { $$ = NULL; }
    | direct_abstract_declarator LBRACKET ASTERISK RBRACKET { $$ = $1; }
    | LPAREN RPAREN { $$ = NULL; }
    | LPAREN parameter_type_list RPAREN { $$ = NULL; }
    | direct_abstract_declarator LPAREN RPAREN { $$ = $1; }
    | direct_abstract_declarator LPAREN parameter_type_list RPAREN { $$ = $1; }
    ;

/* Statements */
statement
    : labeled_statement { $$ = $1; }
    | compound_statement { $$ = $1; }
    | expression_statement { $$ = $1; }
    | selection_statement { $$ = $1; }
    | iteration_statement { $$ = $1; }
    | jump_statement { $$ = $1; }
    | declaration { $$ = $1; }
    | error SEMICOLON { $$ = NULL; yyerrok; }
    ;

labeled_statement
    : IDENTIFIER COLON statement {
        $$ = create_label_stmt(string_duplicate($1), $3);
        free($1);
    }
    | CASE constant_expression COLON statement {
        $$ = create_case_stmt($2, $4);
    }
    | DEFAULT COLON statement {
        $$ = create_default_stmt($3);
    }
    ;

compound_statement
    : LBRACE RBRACE {
        $$ = create_compound_stmt(NULL, 0);
    }
    | LBRACE block_item_list RBRACE {
        $$ = create_compound_stmt($2.nodes, $2.count);
    }
    ;

block_item_list
    : statement {
        if ($1) {
            // Check if statement is a compound (multiple declarations)
            if ($1->type == AST_COMPOUND_STMT) {
                // Flatten it
                $$.nodes = $1->data.compound.statements;
                $$.count = $1->data.compound.stmt_count;
                // Don't free the statements, just the wrapper
                $1->data.compound.statements = NULL;
                $1->data.compound.stmt_count = 0;
                free_ast($1);
            } else {
                $$.nodes = malloc(sizeof(ast_node_t*));
                $$.nodes[0] = $1;
                $$.count = 1;
            }
        } else {
            $$.nodes = NULL;
            $$.count = 0;
        }
    }
    | block_item_list statement {
        if ($2) {
            // Check if statement is a compound (multiple declarations)
            if ($2->type == AST_COMPOUND_STMT) {
                // Flatten it into the list
                int new_items = $2->data.compound.stmt_count;
                $$.count = $1.count + new_items;
                $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
                for (int i = 0; i < new_items; i++) {
                    $$.nodes[$1.count + i] = $2->data.compound.statements[i];
                }
                // Don't free the statements, just the wrapper
                $2->data.compound.statements = NULL;
                $2->data.compound.stmt_count = 0;
                free_ast($2);
            } else {
                $$.count = $1.count + 1;
                $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
                $$.nodes[$$.count - 1] = $2;
            }
        } else {
            $$ = $1;
        }
    }
    ;

expression_statement
    : SEMICOLON { $$ = create_expr_stmt(NULL); }
    | expression SEMICOLON { $$ = create_expr_stmt($1); }
    ;

selection_statement
    : IF LPAREN expression RPAREN statement %prec LOWER_THAN_ELSE {
        $$ = create_if_stmt($3, $5, NULL);
    }
    | IF LPAREN expression RPAREN statement ELSE statement {
        $$ = create_if_stmt($3, $5, $7);
    }
    | SWITCH LPAREN expression RPAREN statement {
        $$ = create_switch_stmt($3, $5);
    }
    ;

iteration_statement
    : WHILE LPAREN expression RPAREN statement {
        $$ = create_while_stmt($3, $5);
    }
    | DO statement WHILE LPAREN expression RPAREN SEMICOLON {
        $$ = create_do_while_stmt($2, $5);
    }
    | FOR LPAREN expression_statement expression_statement RPAREN statement {
        ast_node_t *init = ($3->data.expr_stmt.expr) ?
        $3->data.expr_stmt.expr : NULL;
        ast_node_t *cond = ($4->data.expr_stmt.expr) ? $4->data.expr_stmt.expr : NULL;
        $$ = create_for_stmt(init, cond, NULL, $6);
    }
    | FOR LPAREN expression_statement expression_statement expression RPAREN statement {
        ast_node_t *init = ($3->data.expr_stmt.expr) ?
        $3->data.expr_stmt.expr : NULL;
        ast_node_t *cond = ($4->data.expr_stmt.expr) ? $4->data.expr_stmt.expr : NULL;
        $$ = create_for_stmt(init, cond, $5, $7);
    }
    | FOR LPAREN declaration expression_statement RPAREN statement {
        ast_node_t *cond = ($4->data.expr_stmt.expr) ?
        $4->data.expr_stmt.expr : NULL;
        $$ = create_for_stmt($3, cond, NULL, $6);
    }
    | FOR LPAREN declaration expression_statement expression RPAREN statement {
        ast_node_t *cond = ($4->data.expr_stmt.expr) ?
        $4->data.expr_stmt.expr : NULL;
        $$ = create_for_stmt($3, cond, $5, $7);
    }
    ;

jump_statement
    : GOTO IDENTIFIER SEMICOLON {
        $$ = create_goto_stmt(string_duplicate($2));
        free($2);
    }
    | CONTINUE SEMICOLON {
        $$ = create_continue_stmt();
    }
    | BREAK SEMICOLON {
        $$ = create_break_stmt();
    }
    | RETURN SEMICOLON {
        $$ = create_return_stmt(NULL);
    }
    | RETURN expression SEMICOLON {
        $$ = create_return_stmt($2);
    }
    ;

/* Expressions */
primary_expression
    : IDENTIFIER {
        $$ = create_identifier(string_duplicate($1));
        free($1);
    }
    | CONSTANT {
        $$ = create_number($1);
    }
    | CHARACTER {
        $$ = create_character($1);
    }
    | STRING_LITERAL {
        $$ = create_string_literal(string_duplicate($1));
        free($1);
    }
    | LPAREN expression RPAREN {
        $$ = $2;
    }
    ;

postfix_expression
    : primary_expression { $$ = $1; }
    | postfix_expression LBRACKET expression RBRACKET {
        $$ = create_array_access($1, $3);
    }
    | postfix_expression LPAREN RPAREN {
        if ($1->type == AST_IDENTIFIER) {
            $$ = create_call($1->data.identifier.name, NULL, 0);
        } else {
            $$ = create_error_expression();
        }
    }
    | postfix_expression LPAREN argument_expression_list RPAREN {
        if ($1->type == AST_IDENTIFIER) {
            $$ = create_call($1->data.identifier.name, $3.nodes, $3.count);
        } else {
            $$ = create_error_expression();
        }
    }
    | postfix_expression DOT IDENTIFIER {
        $$ = create_member_access($1, string_duplicate($3));
        free($3);
    }
    | postfix_expression PTR_OP IDENTIFIER {
        $$ = create_ptr_member_access($1, string_duplicate($3));
        free($3);
    }
    | postfix_expression INC_OP {
        $$ = create_unary_op(OP_POSTINC, $1);
    }
    | postfix_expression DEC_OP {
        $$ = create_unary_op(OP_POSTDEC, $1);
    }
    | LPAREN type_name RPAREN LBRACE initializer_list RBRACE {
        $$ = create_cast($2, $5);
        cleanup_type_info(&$2);
    }
    | LPAREN type_name RPAREN LBRACE initializer_list COMMA RBRACE {
        $$ = create_cast($2, $5);
        cleanup_type_info(&$2);
    }
    ;

argument_expression_list
    : assignment_expression {
        $$.nodes = malloc(sizeof(ast_node_t*));
        $$.nodes[0] = $1;
        $$.count = 1;
    }
    | argument_expression_list COMMA assignment_expression {
        $$.count = $1.count + 1;
        $$.nodes = realloc($1.nodes, $$.count * sizeof(ast_node_t*));
        $$.nodes[$$.count - 1] = $3;
    }
    ;

unary_expression
    : postfix_expression { $$ = $1; }
    | INC_OP unary_expression {
        $$ = create_unary_op(OP_PREINC, $2);
    }
    | DEC_OP unary_expression {
        $$ = create_unary_op(OP_PREDEC, $2);
    }
    | AMPERSAND cast_expression %prec ADDRESS_OF {
        $$ = create_address_of($2);
    }
    | ASTERISK cast_expression %prec DEREFERENCE {
        $$ = create_dereference($2);
    }
    | PLUS cast_expression %prec UNARY_PLUS {
        $$ = $2;
        // Unary plus is effectively a no-op
    }
    | MINUS cast_expression %prec UNARY_MINUS {
        $$ = create_unary_op(OP_NEG, $2);
    }
    | TILDE cast_expression {
        $$ = create_unary_op(OP_BNOT, $2);
    }
    | EXCLAMATION cast_expression {
        $$ = create_unary_op(OP_NOT, $2);
    }
    | SIZEOF unary_expression {
        $$ = create_sizeof_expr($2);
        calculate_and_store_sizes($$);
    }
    | SIZEOF LPAREN type_name RPAREN {
        $$ = create_sizeof_type($3);
        calculate_and_store_sizes($$);
        cleanup_type_info(&$3);
    }
    ;

cast_expression
    : unary_expression { $$ = $1; }
    | LPAREN type_name RPAREN cast_expression {
        $$ = create_cast($2, $4);
        cleanup_type_info(&$2);
    }
    ;

multiplicative_expression
    : cast_expression { $$ = $1; }
    | multiplicative_expression ASTERISK cast_expression {
        $$ = create_binary_op(OP_MUL, $1, $3);
    }
    | multiplicative_expression SLASH cast_expression {
        $$ = create_binary_op(OP_DIV, $1, $3);
    }
    | multiplicative_expression PERCENT cast_expression {
        $$ = create_binary_op(OP_MOD, $1, $3);
    }
    ;

additive_expression
    : multiplicative_expression { $$ = $1; }
    | additive_expression PLUS multiplicative_expression {
        $$ = create_binary_op(OP_ADD, $1, $3);
    }
    | additive_expression MINUS multiplicative_expression {
        $$ = create_binary_op(OP_SUB, $1, $3);
    }
    ;

shift_expression
    : additive_expression { $$ = $1; }
    | shift_expression LEFT_OP additive_expression {
        $$ = create_binary_op(OP_LSHIFT, $1, $3);
    }
    | shift_expression RIGHT_OP additive_expression {
        $$ = create_binary_op(OP_RSHIFT, $1, $3);
    }
    ;

relational_expression
    : shift_expression { $$ = $1; }
    | relational_expression LESS_THAN shift_expression {
        $$ = create_binary_op(OP_LT, $1, $3);
    }
    | relational_expression GREATER_THAN shift_expression {
        $$ = create_binary_op(OP_GT, $1, $3);
    }
    | relational_expression LE_OP shift_expression {
        $$ = create_binary_op(OP_LE, $1, $3);
    }
    | relational_expression GE_OP shift_expression {
        $$ = create_binary_op(OP_GE, $1, $3);
    }
    ;

equality_expression
    : relational_expression { $$ = $1; }
    | equality_expression EQ_OP relational_expression {
        $$ = create_binary_op(OP_EQ, $1, $3);
    }
    | equality_expression NE_OP relational_expression {
        $$ = create_binary_op(OP_NE, $1, $3);
    }
    ;

and_expression
    : equality_expression { $$ = $1; }
    | and_expression AMPERSAND equality_expression {
        $$ = create_binary_op(OP_BAND, $1, $3);
    }
    ;

exclusive_or_expression
    : and_expression { $$ = $1; }
    | exclusive_or_expression CARET and_expression {
        $$ = create_binary_op(OP_BXOR, $1, $3);
    }
    ;

inclusive_or_expression
    : exclusive_or_expression { $$ = $1; }
    | inclusive_or_expression PIPE exclusive_or_expression {
        $$ = create_binary_op(OP_BOR, $1, $3);
    }
    ;

logical_and_expression
    : inclusive_or_expression { $$ = $1; }
    | logical_and_expression AND_OP inclusive_or_expression {
        $$ = create_binary_op(OP_LAND, $1, $3);
    }
    ;

logical_or_expression
    : logical_and_expression { $$ = $1; }
    | logical_or_expression OR_OP logical_and_expression {
        $$ = create_binary_op(OP_LOR, $1, $3);
    }
    ;

conditional_expression
    : logical_or_expression { $$ = $1; }
    | logical_or_expression QUESTION expression COLON conditional_expression {
        $$ = create_conditional($1, $3, $5);
    }
    ;

assignment_expression
    : conditional_expression { $$ = $1; }
    | unary_expression assignment_operator assignment_expression {
        if ($2 == OP_ASSIGN) {
            if ($1->type == AST_IDENTIFIER) {
                $$ = create_assignment($1->data.identifier.name, $3);
            } else {
                $$ = create_assignment_to_lvalue($1, $3);
            }
        } else {
            $$ = create_compound_assignment($1, $2, $3);
        }
    }
    ;

assignment_operator
    : ASSIGN { $$ = OP_ASSIGN; }
    | MUL_ASSIGN { $$ = OP_MUL_ASSIGN; }
    | DIV_ASSIGN { $$ = OP_DIV_ASSIGN; }
    | MOD_ASSIGN { $$ = OP_MOD_ASSIGN; }
    | ADD_ASSIGN { $$ = OP_ADD_ASSIGN; }
    | SUB_ASSIGN { $$ = OP_SUB_ASSIGN; }
    | LEFT_ASSIGN { $$ = OP_LSHIFT_ASSIGN; }
    | RIGHT_ASSIGN { $$ = OP_RSHIFT_ASSIGN; }
    | AND_ASSIGN { $$ = OP_BAND_ASSIGN; }
    | XOR_ASSIGN { $$ = OP_BXOR_ASSIGN; }
    | OR_ASSIGN { $$ = OP_BOR_ASSIGN; }
    ;

expression
    : assignment_expression { $$ = $1; }
    | expression COMMA assignment_expression {
        // For now, just return the right side of comma
        $$ = $3;
    }
    ;

constant_expression
    : conditional_expression { $$ = $1; }
    ;

/* Initializers */
initializer
    : assignment_expression { $$ = $1; }
    | LBRACE initializer_list RBRACE { $$ = $2; }
    | LBRACE initializer_list COMMA RBRACE { $$ = $2; }
    ;

initializer_list
    : initializer {
        ast_node_t **values = malloc(sizeof(ast_node_t*));
        values[0] = $1;
        $$ = create_initializer_list(values, 1);
    }
    | designation initializer {
        ast_node_t **values = malloc(sizeof(ast_node_t*));
        values[0] = $2;
        $$ = create_initializer_list(values, 1);
    }
    | initializer_list COMMA initializer {
        $$ = $1;
        $$->data.initializer_list.count++;
        $$->data.initializer_list.values = realloc($$->data.initializer_list.values,
                                                   $$->data.initializer_list.count * sizeof(ast_node_t*));
        $$->data.initializer_list.values[$$->data.initializer_list.count - 1] = $3;
    }
    | initializer_list COMMA designation initializer {
        $$ = $1;
        $$->data.initializer_list.count++;
        $$->data.initializer_list.values = realloc($$->data.initializer_list.values,
                                                   $$->data.initializer_list.count * sizeof(ast_node_t*));
        $$->data.initializer_list.values[$$->data.initializer_list.count - 1] = $4;
    }
    ;

designation
    : designator_list ASSIGN { $$ = NULL; }
    ;

designator_list
    : designator { $$.nodes = NULL; $$.count = 0; }
    | designator_list designator { $$ = $1; }
    ;

designator
    : LBRACKET constant_expression RBRACKET { $$ = NULL; }
    | DOT IDENTIFIER { 
        $$ = NULL; 
        free($2);
    }
    ;

/* Top-level declaration handling */
declaration
    : declaration_specifiers init_declarator_list SEMICOLON {
        // Create a declaration for each declarator with proper type
        if ($2.count == 1) {
            // Single declarator - return it directly
            type_info_t complete_type = make_complete_type($1, $2.declarators[0]);
            $$ = create_declaration(complete_type, 
                                   string_duplicate($2.declarators[0].name), 
                                   $2.initializers[0]);
            
            // Clean up
            free($2.declarators);
            free($2.initializers);
            cleanup_type_info(&$1);
        } else {
            // Multiple declarators - create a compound statement containing all declarations
            ast_node_t **decls = malloc($2.count * sizeof(ast_node_t*));
            
            for (int i = 0; i < $2.count; i++) {
                type_info_t complete_type = make_complete_type($1, $2.declarators[i]);
                decls[i] = create_declaration(complete_type,
                                             string_duplicate($2.declarators[i].name),
                                             $2.initializers[i]);
            }
            
            // Create a compound statement to hold all declarations
            // This allows the rest of the parser to treat it as a single statement
            $$ = create_compound_stmt(decls, $2.count);
            
            // Clean up
            free($2.declarators);
            free($2.initializers);
            cleanup_type_info(&$1);
        }
    }
    | declaration_specifiers SEMICOLON {
        $$ = NULL;
        cleanup_type_info(&$1);
    }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error at line %d, column %d: %s\n", line_number, column, s);
    error_count++;

    if (error_count >= max_errors) {
        fprintf(stderr, "Too many errors (%d), stopping compilation.\n", max_errors);
        exit(1);
    }
}
