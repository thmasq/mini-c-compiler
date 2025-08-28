#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

extern FILE *yyin;
extern int yyparse();
extern ast_node_t *ast_root;
extern int error_count;  // From parser.y

void print_usage(const char *program_name) {
    printf("Usage: %s [options] <input_file>\n", program_name);
    printf("Options:\n");
    printf("  -o <output_file>  Specify output file (default: stdout for IR, a.out for executable)\n");
    printf("  -S                Generate LLVM IR only (default)\n");
    printf("  -c                Compile to executable\n");
    printf("  -O <level>        Optimization level (0-3, default: 0)\n");
    printf("  -f                Force compilation despite errors (for testing)\n");
    printf("  -h, --help        Show this help message\n");
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

int main(int argc, char *argv[]) {
    char *input_file = NULL;
    char *output_file = NULL;
    FILE *output = stdout;
    int compile_to_executable = 0;
    int optimization_level = 0;
    int force_compilation = 0;  // New flag to force compilation despite errors
    
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
    
    // Parse the input
    printf("Compiling %s...\n", input_file);
    
    // Reset error count for this compilation
    error_count = 0;
    
    // Parse - this will now continue through errors
    int parse_result = yyparse();
    
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
            return 1;
        } else {
            printf("Forcing compilation despite errors (-f flag used).\n");
        }
    } else {
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
        return 1;
    }
    
    // Only generate code if parsing was successful or forced
    if (error_count == 0 || force_compilation) {
        printf("Generating LLVM IR...\n");
        
        // Generate LLVM IR
        generate_llvm_ir(ast_root, output);
        
        if (error_count > 0) {
            printf("Warning: IR generated with parse errors - may not be valid\n");
        } else {
            printf("LLVM IR generation complete.\n");
        }
    }
    
    // Cleanup parsing resources
    free_ast(ast_root);
    fclose(yyin);
    if (output != stdout) fclose(output);
    
    // If compiling to executable and no critical errors, use clang
    if (compile_to_executable && (error_count == 0 || force_compilation)) {
        char command[1024];
        const char *final_output = output_file ? output_file : "a.out";
        
        // Build clang command
        snprintf(command, sizeof(command), 
                "clang -O%d -o %s %s", 
                optimization_level, final_output, ir_file);
        
        printf("Compiling IR to executable...\n");
        if (run_command(command) != 0) {
            fprintf(stderr, "Failed to compile IR to executable\n");
            unlink(ir_file); // Clean up temp file
            free(ir_file);
            return 1;
        }
        
        if (error_count > 0) {
            printf("Compilation completed with warnings! Executable: %s\n", final_output);
        } else {
            printf("Compilation successful! Executable: %s\n", final_output);
        }
        
        // Clean up temporary IR file
        unlink(ir_file);
        free(ir_file);
    } else if (compile_to_executable) {
        printf("Executable generation skipped due to errors.\n");
        if (ir_file) {
            unlink(ir_file);
            free(ir_file);
        }
    } else {
        if (output_file) {
            printf("Output written to %s\n", output_file);
        }
    }
    
    // Return appropriate exit code
    return (error_count > 0 && !force_compilation) ? 1 : 0;
}
