# Mini C Compiler Makefile

# Directories
SRCDIR = src
BUILDDIR = build
TESTDIR = tests
EXAMPLEDIR = examples

# Compiler settings
CC = gcc
CFLAGS = -Wall -g -std=c99 -D_POSIX_C_SOURCE=200809L -I$(SRCDIR)
FLEX = flex
BISON = bison

# Target and source files
TARGET = minicc
SOURCES = main.c ast.c codegen.c lexer.c parser.c
OBJECTS = $(SOURCES:%.c=$(BUILDDIR)/%.o)

# Generated files (in src directory)
LEXER_C = $(SRCDIR)/lexer.c
PARSER_C = $(SRCDIR)/parser.c
PARSER_H = $(SRCDIR)/parser.h

.PHONY: all clean test test-advanced dirs help install-tests test-exec test-pointers

all: dirs $(TARGET)

# Create necessary directories
dirs:
	@mkdir -p $(BUILDDIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

# Generate lexer from flex file
$(LEXER_C): $(SRCDIR)/lexer.l $(PARSER_H)
	$(FLEX) -o $@ $<

# Generate parser from bison file
$(PARSER_C) $(PARSER_H): $(SRCDIR)/parser.y
	$(BISON) -d -o $(PARSER_C) $<

# Object file compilation rules
$(BUILDDIR)/main.o: $(SRCDIR)/main.c $(SRCDIR)/ast.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/ast.o: $(SRCDIR)/ast.c $(SRCDIR)/ast.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/codegen.o: $(SRCDIR)/codegen.c $(SRCDIR)/ast.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Special compilation for generated files (suppress common flex/bison warnings)
$(BUILDDIR)/lexer.o: $(LEXER_C) $(SRCDIR)/ast.h $(PARSER_H)
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-sign-compare -c -o $@ $<

$(BUILDDIR)/parser.o: $(PARSER_C) $(SRCDIR)/ast.h
	$(CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

# Install basic test files (run once to set up)
install-tests:
	@echo "Setting up basic test files..."
	@mkdir -p $(TESTDIR)/basic $(TESTDIR)/advanced
	@echo "int main() {" > $(TESTDIR)/basic/arithmetic.c
	@echo "    int x = 10;" >> $(TESTDIR)/basic/arithmetic.c
	@echo "    int y = 20;" >> $(TESTDIR)/basic/arithmetic.c
	@echo "    int result = x + y * 2;" >> $(TESTDIR)/basic/arithmetic.c
	@echo "    return result;" >> $(TESTDIR)/basic/arithmetic.c
	@echo "}" >> $(TESTDIR)/basic/arithmetic.c
	@echo "Created $(TESTDIR)/basic/arithmetic.c"
	@echo ""
	@echo "int fibonacci(int n) {" > $(TESTDIR)/advanced/fibonacci.c
	@echo "    if (n <= 1) {" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "        return n;" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "    }" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "    return fibonacci(n - 1) + fibonacci(n - 2);" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "}" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "int main() {" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "    int i = 0;" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "    while (i < 10) {" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "        int fib = fibonacci(i);" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "        i = i + 1;" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "    }" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "    return 0;" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "}" >> $(TESTDIR)/advanced/fibonacci.c
	@echo "Created $(TESTDIR)/advanced/fibonacci.c"

# Simple test using basic arithmetic (LLVM IR only)
test: $(TARGET)
	@echo "=== Running Basic Test (LLVM IR) ==="
	@if [ ! -f $(TESTDIR)/basic/arithmetic.c ]; then \
		echo "Test files not found. Run 'make install-tests' first."; \
		exit 1; \
	fi
	@echo "Compiling $(TESTDIR)/basic/arithmetic.c to LLVM IR..."
	./$(TARGET) -S $(TESTDIR)/basic/arithmetic.c -o $(BUILDDIR)/test_arithmetic.ll
	@echo ""
	@echo "Generated LLVM IR:"
	@cat $(BUILDDIR)/test_arithmetic.ll

# Test with executable generation
test-exec: $(TARGET)
	@echo "=== Running Basic Test (Executable) ==="
	@if [ ! -f $(TESTDIR)/basic/arithmetic.c ]; then \
		echo "Test files not found. Run 'make install-tests' first."; \
		exit 1; \
	fi
	@echo "Compiling $(TESTDIR)/basic/arithmetic.c to executable..."
	./$(TARGET) -c $(TESTDIR)/basic/arithmetic.c -o $(BUILDDIR)/test_arithmetic
	@echo "Running executable..."
	$(BUILDDIR)/test_arithmetic
	@echo "Exit code: $$?"

# Advanced test with functions and control flow
test-advanced: $(TARGET)
	@echo "=== Running Advanced Test (LLVM IR) ==="
	@if [ ! -f $(TESTDIR)/advanced/fibonacci.c ]; then \
		echo "Test files not found. Run 'make install-tests' first."; \
		exit 1; \
	fi
	@echo "Compiling $(TESTDIR)/advanced/fibonacci.c to LLVM IR..."
	./$(TARGET) -S $(TESTDIR)/advanced/fibonacci.c -o $(BUILDDIR)/test_fibonacci.ll
	@echo ""
	@echo "Generated LLVM IR (first 50 lines):"
	@head -50 $(BUILDDIR)/test_fibonacci.ll

# Advanced test with executable
test-advanced-exec: $(TARGET)
	@echo "=== Running Advanced Test (Executable) ==="
	@if [ ! -f $(TESTDIR)/advanced/fibonacci.c ]; then \
		echo "Test files not found. Run 'make install-tests' first."; \
		exit 1; \
	fi
	@echo "Compiling $(TESTDIR)/advanced/fibonacci.c to executable..."
	./$(TARGET) -c $(TESTDIR)/advanced/fibonacci.c -o $(BUILDDIR)/test_fibonacci
	@echo "Running executable..."
	$(BUILDDIR)/test_fibonacci
	@echo "Exit code: $$?"

# Test with example file
test-example: $(TARGET)
	@echo "=== Running Example Test (LLVM IR) ==="
	@if [ ! -f $(EXAMPLEDIR)/sample.c ]; then \
		echo "Example file not found at $(EXAMPLEDIR)/sample.c"; \
		exit 1; \
	fi
	@echo "Compiling $(EXAMPLEDIR)/sample.c to LLVM IR..."
	./$(TARGET) -S $(EXAMPLEDIR)/sample.c -o $(BUILDDIR)/test_example.ll
	@echo ""
	@echo "Generated LLVM IR (first 50 lines):"
	@head -50 $(BUILDDIR)/test_example.ll

# Test example with executable
test-example-exec: $(TARGET)
	@echo "=== Running Example Test (Executable) ==="
	@if [ ! -f $(EXAMPLEDIR)/sample.c ]; then \
		echo "Example file not found at $(EXAMPLEDIR)/sample.c"; \
		exit 1; \
	fi
	@echo "Compiling $(EXAMPLEDIR)/sample.c to executable..."
	./$(TARGET) -c $(EXAMPLEDIR)/sample.c -o $(BUILDDIR)/test_example
	@echo "Running executable..."
	$(BUILDDIR)/test_example
	@echo "Exit code: $$?"

# Test pointer support (LLVM IR only)
test-pointers: $(TARGET)
	@echo "=== Running Pointer Test (LLVM IR) ==="
	@if [ ! -f $(EXAMPLEDIR)/pointer_test.c ]; then \
		echo "Pointer test file not found at $(EXAMPLEDIR)/pointer_test.c"; \
		exit 1; \
	fi
	@echo "Compiling $(EXAMPLEDIR)/pointer_test.c to LLVM IR..."
	./$(TARGET) -S $(EXAMPLEDIR)/pointer_test.c -o $(BUILDDIR)/test_pointers.ll
	@echo ""
	@echo "Generated LLVM IR (first 100 lines):"
	@head -100 $(BUILDDIR)/test_pointers.ll

# Test pointer support with executable
test-pointers-exec: $(TARGET)
	@echo "=== Running Pointer Test (Executable) ==="
	@if [ ! -f $(EXAMPLEDIR)/pointer_test.c ]; then \
		echo "Pointer test file not found at $(EXAMPLEDIR)/pointer_test.c"; \
		exit 1; \
	fi
	@echo "Compiling $(EXAMPLEDIR)/pointer_test.c to executable..."
	./$(TARGET) -c $(EXAMPLEDIR)/pointer_test.c -o $(BUILDDIR)/test_pointers
	@echo "Running executable..."
	$(BUILDDIR)/test_pointers
	@echo "Exit code: $$?"
	@if [ $$? -eq 0 ]; then \
		echo "SUCCESS: All pointer tests passed!"; \
	else \
		echo "FAILURE: Some pointer tests failed."; \
	fi

# Run all tests
test-all: test test-advanced test-example test-pointers
	@echo ""
	@echo "=== All LLVM IR Tests Complete ==="

# Run all executable tests
test-all-exec: test-exec test-advanced-exec test-example-exec test-pointers-exec
	@echo ""
	@echo "=== All Executable Tests Complete ==="

# Clean up generated files
clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -f $(LEXER_C) $(PARSER_C) $(PARSER_H)
	rm -rf $(BUILDDIR)
	rm -f $(TESTDIR)/**/*.ll
	@echo "Cleaned build artifacts and generated files"

# Clean only build directory (keep generated parser/lexer)
clean-build:
	rm -rf $(BUILDDIR)
	@echo "Cleaned build directory"

# Development helpers
rebuild: clean all

debug: CFLAGS += -DDEBUG -O0
debug: all

release: CFLAGS += -O2 -DNDEBUG
release: clean all

# Show help
help:
	@echo "Mini C Compiler Build System - With Pointer Support"
	@echo ""
	@echo "Setup:"
	@echo "  install-tests     - Set up basic test files (run once)"
	@echo ""
	@echo "Build targets:"
	@echo "  all               - Build the compiler (default)"
	@echo "  debug             - Build with debug flags"
	@echo "  release           - Build optimized version"
	@echo "  rebuild           - Clean and build"
	@echo ""
	@echo "Test targets (LLVM IR generation):"
	@echo "  test              - Run basic arithmetic test (IR only)"
	@echo "  test-advanced     - Run advanced fibonacci test (IR only)"
	@echo "  test-example      - Run example program test (IR only)"
	@echo "  test-pointers     - Run comprehensive pointer test (IR only)"
	@echo "  test-all          - Run all IR generation tests"
	@echo ""
	@echo "Test targets (Executable generation):"
	@echo "  test-exec         - Run basic test with executable"
	@echo "  test-advanced-exec- Run advanced test with executable"
	@echo "  test-example-exec - Run example test with executable"
	@echo "  test-pointers-exec- Run pointer test with executable"
	@echo "  test-all-exec     - Run all executable tests"
	@echo ""
	@echo "Cleanup:"
	@echo "  clean             - Remove all generated files"
	@echo "  clean-build       - Remove only build artifacts"
	@echo ""
	@echo "Usage:"
	@echo "  ./$(TARGET) -S input.c -o output.ll    # Generate LLVM IR"
	@echo "  ./$(TARGET) -c input.c -o executable   # Compile to executable"
	@echo ""
	@echo "Directory structure:"
	@echo "  $(SRCDIR)/           - Source code"
	@echo "  $(BUILDDIR)/         - Build artifacts"
	@echo "  $(TESTDIR)/          - Test programs"
	@echo "  $(EXAMPLEDIR)/       - Example programs (includes pointer_test.c)"

.DEFAULT_GOAL := all
