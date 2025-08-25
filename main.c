#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern FILE *yyin;
extern int yyparse();
extern ast_node_t *ast_root;

void print_usage(const char *program_name) {
    printf("Usage: %s [options] <input_file>\n", program_name);
    printf("Options:\n");
    printf("  -o <output_file>  Specify output file (default: stdout)\n");
    printf("  -h, --help        Show this help message\n");
}

int main(int argc, char *argv[]) {
    char *input_file = NULL;
    char *output_file = NULL;
    FILE *output = stdout;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "Error: -o option requires an argument\n");
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
    
    // Open input file
    yyin = fopen(input_file, "r");
    if (!yyin) {
        perror("Error opening input file");
        return 1;
    }
    
    // Open output file if specified
    if (output_file) {
        output = fopen(output_file, "w");
        if (!output) {
            perror("Error opening output file");
            fclose(yyin);
            return 1;
        }
    }
    
    // Parse the input
    printf("Compiling %s...\n", input_file);
    
    if (yyparse() != 0) {
        fprintf(stderr, "Parse failed\n");
        fclose(yyin);
        if (output != stdout) fclose(output);
        return 1;
    }
    
    if (!ast_root) {
        fprintf(stderr, "No AST generated\n");
        fclose(yyin);
        if (output != stdout) fclose(output);
        return 1;
    }
    
    printf("Generating LLVM IR...\n");
    
    // Generate LLVM IR
    generate_llvm_ir(ast_root, output);
    
    printf("Compilation complete.\n");
    if (output_file) {
        printf("Output written to %s\n", output_file);
    }
    
    // Cleanup
    free_ast(ast_root);
    fclose(yyin);
    if (output != stdout) fclose(output);
    
    return 0;
}
