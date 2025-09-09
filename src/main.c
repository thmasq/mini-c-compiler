#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

extern FILE *yyin;
extern int yyparse();
extern ast_node_t *ast_root;
extern int error_count;
extern symbol_table_t *global_symbol_table;

// Program version and info
#define VERSION "2.0.0"
#define PROGRAM_NAME "Enhanced Mini C Compiler"

void print_usage(const char *program_name) {
    printf("%s v%s\n", PROGRAM_NAME, VERSION);
    printf("A complete C subset compiler supporting structs, unions, enums, and advanced features\n\n");
    printf("Usage: %s [options] <input_file>\n", program_name);
    printf("\nOptions:\n");
    printf("  -o <output_file>  Specify output file (default: stdout for IR, a.out for executable)\n");
    printf("  -S                Generate LLVM IR only (default)\n");
    printf("  -c                Compile to executable\n");
    printf("  -O <level>        Optimization level (0-3, default: 0)\n");
    printf("  -f                Force compilation despite errors (for testing)\n");
    printf("  -v, --verbose     Verbose output with symbol table information\n");
    printf("  -t, --type-check  Enable enhanced type checking\n");
    printf("  -d, --debug       Enable debug output\n");
    printf("  -h, --help        Show this help message\n");
    printf("  --version         Show version information\n");
    printf("\nSupported Language Features:\n");
    printf("  • Complete C type system (int, char, float, double, void, etc.)\n");
    printf("  • Pointers and arrays (including VLAs)\n");
    printf("  • Structs and unions with proper alignment\n");
    printf("  • Enums with automatic and explicit values\n");
    printf("  • All control flow statements (if, while, for, do-while, switch)\n");
    printf("  • Function definitions and calls (including variadic functions)\n");
    printf("  • Full operator set (arithmetic, logical, bitwise, assignment)\n");
    printf("  • String literals and character constants\n");
    printf("  • Typedefs and storage classes\n");
    printf("  • Compound assignment operators (+=, -=, etc.)\n");
    printf("  • Increment/decrement operators (++, --)\n");
    printf("  • Ternary conditional operator (?:)\n");
    printf("  • Sizeof operator\n");
    printf("  • Type casting\n");
    printf("  • Labels and goto statements\n");
    printf("  • Break and continue statements\n");
    printf("\nExamples:\n");
    printf("  %s -S program.c -o program.ll      # Generate LLVM IR\n", program_name);
    printf("  %s -c program.c -o program          # Compile to executable\n", program_name);
    printf("  %s -v -t program.c                  # Verbose compilation with type checking\n", program_name);
}

void print_version() {
    printf("%s v%s\n", PROGRAM_NAME, VERSION);
    printf("Built with enhanced C language support\n");
    printf("Features: structs, unions, enums, VLAs, complete type system\n");
}

int run_command(const char *cmd) {
    printf("Running: %s\n", cmd);
    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "Command failed with exit code %d\n", WEXITSTATUS(result));
        return -1;
    }
    return 0;
}

// Enhanced semantic analysis with symbol table
int perform_semantic_analysis(ast_node_t *ast, symbol_table_t *symbol_table, int verbose) {
    if (!ast || !symbol_table) {
        fprintf(stderr, "Invalid AST or symbol table for semantic analysis\n");
        return 0;
    }
    
    if (verbose) {
        printf("Performing semantic analysis...\n");
    }
    
    // Perform type checking and symbol resolution
    int success = check_types(ast, symbol_table);
    
    if (verbose) {
        printf("Symbol table after semantic analysis:\n");
        print_symbol_table(symbol_table);
    }
    
    if (!success) {
        fprintf(stderr, "Semantic analysis failed\n");
        return 0;
    }
    
    if (verbose) {
        printf("Semantic analysis completed successfully\n");
    }
    
    return 1;
}

// Collect compilation statistics
typedef struct {
    int functions;
    int variables;
    int structs;
    int unions;
    int enums;
    int lines_of_ir;
} compilation_stats_t;

void collect_stats(ast_node_t *ast, compilation_stats_t *stats) {
    if (!ast || !stats) return;
    
    switch (ast->type) {
        case AST_PROGRAM:
            for (int i = 0; i < ast->data.program.decl_count; i++) {
                collect_stats(ast->data.program.declarations[i], stats);
            }
            break;
        case AST_FUNCTION:
            stats->functions++;
            collect_stats(ast->data.function.body, stats);
            break;
        case AST_DECLARATION:
        case AST_ARRAY_DECL:
            stats->variables++;
            break;
        case AST_STRUCT_DECL:
            if (ast->data.struct_decl.is_definition) stats->structs++;
            break;
        case AST_UNION_DECL:
            if (ast->data.union_decl.is_definition) stats->unions++;
            break;
        case AST_ENUM_DECL:
            if (ast->data.enum_decl.is_definition) stats->enums++;
            break;
        case AST_COMPOUND_STMT:
            for (int i = 0; i < ast->data.compound.stmt_count; i++) {
                collect_stats(ast->data.compound.statements[i], stats);
            }
            break;
        default:
            // For other node types, recursively check children if needed
            break;
    }
}

void print_stats(compilation_stats_t *stats, int verbose) {
    if (!verbose) return;
    
    printf("\nCompilation Statistics:\n");
    printf("  Functions:     %d\n", stats->functions);
    printf("  Variables:     %d\n", stats->variables);
    printf("  Structures:    %d\n", stats->structs);
    printf("  Unions:        %d\n", stats->unions);
    printf("  Enumerations:  %d\n", stats->enums);
    if (stats->lines_of_ir > 0) {
        printf("  LLVM IR lines: %d\n", stats->lines_of_ir);
    }
}

int count_ir_lines(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;
    
    int lines = 0;
    char ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') lines++;
    }
    
    fclose(file);
    return lines;
}

int main(int argc, char *argv[]) {
    char *input_file = NULL;
    char *output_file = NULL;
    FILE *output = stdout;
    int compile_to_executable = 0;
    int optimization_level = 0;
    int force_compilation = 0;
    int verbose = 0;
    int enable_type_checking = 1; // Default enabled
    int debug_mode = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "Error: -o option requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            compile_to_executable = 1;
        } else if (strcmp(argv[i], "-S") == 0) {
            compile_to_executable = 0;
        } else if (strcmp(argv[i], "-f") == 0) {
            force_compilation = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--type-check") == 0) {
            enable_type_checking = 1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
            verbose = 1; // Debug implies verbose
        } else if (strcmp(argv[i], "-O") == 0) {
            if (i + 1 < argc) {
                optimization_level = atoi(argv[++i]);
                if (optimization_level < 0 || optimization_level > 3) {
                    fprintf(stderr, "Error: Invalid optimization level. Use 0-3.\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: -O option requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (input_file == NULL) {
                input_file = argv[i];
            } else {
                fprintf(stderr, "Error: Multiple input files specified\n");
                return 1;
            }
        }
    }
    
    if (input_file == NULL) {
        fprintf(stderr, "Error: No input file specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (verbose) {
        printf("%s v%s\n", PROGRAM_NAME, VERSION);
        printf("Compiling: %s\n", input_file);
        if (debug_mode) {
            printf("Debug mode enabled\n");
        }
    }
    
    // Generate temporary IR file name if compiling to executable
    char *ir_file = NULL;
    if (compile_to_executable) {
        ir_file = malloc(strlen(input_file) + 20);
        sprintf(ir_file, "%s.ll", input_file);
        
        output = fopen(ir_file, "w");
        if (!output) {
            perror("Error creating temporary IR file");
            return 1;
        }
    } else if (output_file) {
        output = fopen(output_file, "w");
        if (!output) {
            perror("Error opening output file");
            return 1;
        }
    }
    
    // Open input file
    yyin = fopen(input_file, "r");
    if (!yyin) {
        perror("Error opening input file");
        if (output != stdout) fclose(output);
        return 1;
    }
    
    // Initialize global symbol table
    global_symbol_table = create_symbol_table();
    
    // Reset error count for this compilation
    error_count = 0;
    
    if (verbose) {
        printf("Phase 1: Lexical and syntactic analysis...\n");
    }
    
    yyparse();
    
    // Report parsing results
    if (error_count > 0) {
        printf("Parsing completed with %d error(s)\n", error_count);
        
        if (!force_compilation) {
            printf("Compilation stopped due to errors. Use -f to force compilation.\n");
            fclose(yyin);
            if (output != stdout) fclose(output);
            if (ir_file) {
                unlink(ir_file);
                free(ir_file);
            }
            destroy_symbol_table(global_symbol_table);
            return 1;
        } else {
            printf("Forcing compilation despite errors (-f flag used).\n");
        }
    } else if (verbose) {
        printf("Parsing completed successfully.\n");
    }
    
    // Check if we have a valid AST
    if (!ast_root) {
        fprintf(stderr, "No AST generated - cannot continue\n");
        fclose(yyin);
        if (output != stdout) fclose(output);
        if (ir_file) {
            unlink(ir_file);
            free(ir_file);
        }
        destroy_symbol_table(global_symbol_table);
        return 1;
    }
    
    // Collect compilation statistics
    compilation_stats_t stats = {0};
    collect_stats(ast_root, &stats);
    
    // Perform semantic analysis if enabled
    int semantic_success = 1;
    if (enable_type_checking && (error_count == 0 || force_compilation)) {
        if (verbose) {
            printf("Phase 2: Semantic analysis and type checking...\n");
        }
        
        semantic_success = perform_semantic_analysis(ast_root, global_symbol_table, debug_mode);
        
        if (!semantic_success && !force_compilation) {
            printf("Compilation stopped due to semantic errors. Use -f to force compilation.\n");
            fclose(yyin);
            if (output != stdout) fclose(output);
            if (ir_file) {
                unlink(ir_file);
                free(ir_file);
            }
            free_ast(ast_root);
            destroy_symbol_table(global_symbol_table);
            return 1;
        }
    }
    
    // Generate code if parsing was successful or forced
    if (error_count == 0 || force_compilation) {
        if (verbose) {
            printf("Phase 3: Code generation...\n");
        }
        
        // Generate LLVM IR
        generate_llvm_ir(ast_root, output);
        
        if (error_count > 0) {
            printf("Warning: IR generated with parse errors - may not be valid\n");
        } else if (verbose) {
            printf("LLVM IR generation complete.\n");
        }
    }
    
    // Count IR lines for statistics
    if (output != stdout) {
        fclose(output);
        if (ir_file || output_file) {
            stats.lines_of_ir = count_ir_lines(ir_file ? ir_file : output_file);
        }
    }
    
    // Print compilation statistics
    print_stats(&stats, verbose);
    
    // Cleanup parsing resources
    free_ast(ast_root);
    fclose(yyin);
    destroy_symbol_table(global_symbol_table);
    
    // If compiling to executable and no critical errors, use clang
    if (compile_to_executable && (error_count == 0 || force_compilation)) {
        char command[1024];
        const char *final_output = output_file ? output_file : "a.out";
        
        if (verbose) {
            printf("Phase 4: Linking with LLVM/Clang...\n");
        }
        
        // Build clang command with optimization
        snprintf(command, sizeof(command), 
                "clang -O%d -o %s %s", 
                optimization_level, final_output, ir_file);
        
        if (run_command(command) != 0) {
            fprintf(stderr, "Failed to compile IR to executable\n");
            if (ir_file) {
                unlink(ir_file);
                free(ir_file);
            }
            return 1;
        }
        
        if (error_count > 0) {
            printf("Compilation completed with warnings! Executable: %s\n", final_output);
        } else {
            if (verbose) {
                printf("Compilation successful! Executable: %s\n", final_output);
            } else {
                printf("Compilation successful: %s\n", final_output);
            }
        }
        
        // Clean up temporary IR file
        if (ir_file) {
            if (!debug_mode) {
                unlink(ir_file);
            } else {
                printf("Debug: IR file preserved at %s\n", ir_file);
            }
            free(ir_file);
        }
    } else if (compile_to_executable) {
        printf("Executable generation skipped due to errors.\n");
        if (ir_file) {
            unlink(ir_file);
            free(ir_file);
        }
    } else {
        if (output_file) {
            if (verbose) {
                printf("LLVM IR written to %s (%d lines)\n", output_file, stats.lines_of_ir);
            } else {
                printf("Output written to %s\n", output_file);
            }
        }
    }
    
    // Return appropriate exit code
    int exit_code = 0;
    if (error_count > 0 && !force_compilation) {
        exit_code = 1;
    } else if (error_count > 0) {
        exit_code = 2; // Warnings but compilation forced
    }
    
    if (verbose && exit_code == 0) {
        printf("Compilation completed successfully.\n");
    }
    
    return exit_code;
}
