CC = gcc
CFLAGS = -Wall -g -std=c99 -D_POSIX_C_SOURCE=200809L
FLEX = flex
BISON = bison

TARGET = minicc
SOURCES = main.c ast.c codegen.c lexer.c parser.c
OBJECTS = $(SOURCES:.c=.o)

# Generated files
LEXER_C = lexer.c
PARSER_C = parser.c
PARSER_H = parser.h

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

# Generate lexer from flex file
$(LEXER_C): lexer.l $(PARSER_H)
	$(FLEX) -o $@ $<

# Generate parser from bison file
$(PARSER_C) $(PARSER_H): parser.y
	$(BISON) -d -o $(PARSER_C) $<

# Dependencies
main.o: main.c ast.h
ast.o: ast.c ast.h
codegen.o: codegen.c ast.h

# Special compilation for generated files (suppress common flex/bison warnings)
lexer.o: $(LEXER_C) ast.h $(PARSER_H)
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-sign-compare -c -o $@ $<

parser.o: $(PARSER_C) ast.h
	$(CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

# Example test
test: $(TARGET)
	@echo "Creating test program..."
	@echo "int main() {" > test.c
	@echo "    int x = 10;" >> test.c
	@echo "    int y = 20;" >> test.c
	@echo "    int result = x + y * 2;" >> test.c
	@echo "    return result;" >> test.c
	@echo "}" >> test.c
	@echo ""
	@echo "Compiling test program..."
	./$(TARGET) test.c -o test.ll
	@echo ""
	@echo "Generated LLVM IR:"
	@cat test.ll

clean:
	rm -f $(OBJECTS) $(TARGET) $(LEXER_C) $(PARSER_C) $(PARSER_H)
	rm -f test.c test.ll

# Advanced test with more features
test-advanced: $(TARGET)
	@echo "Creating advanced test program..."
	@echo "int fibonacci(int n) {" > advanced_test.c
	@echo "    if (n <= 1) {" >> advanced_test.c
	@echo "        return n;" >> advanced_test.c
	@echo "    }" >> advanced_test.c
	@echo "    return fibonacci(n - 1) + fibonacci(n - 2);" >> advanced_test.c
	@echo "}" >> advanced_test.c
	@echo "" >> advanced_test.c
	@echo "int main() {" >> advanced_test.c
	@echo "    int i = 0;" >> advanced_test.c
	@echo "    while (i < 10) {" >> advanced_test.c
	@echo "        int fib = fibonacci(i);" >> advanced_test.c
	@echo "        i = i + 1;" >> advanced_test.c
	@echo "    }" >> advanced_test.c
	@echo "    return 0;" >> advanced_test.c
	@echo "}" >> advanced_test.c
	@echo ""
	@echo "Compiling advanced test program..."
	./$(TARGET) advanced_test.c -o advanced_test.ll
	@echo ""
	@echo "Generated LLVM IR:"
	@head -50 advanced_test.ll

help:
	@echo "Mini C Compiler Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build the compiler"
	@echo "  test          - Build and run a simple test"
	@echo "  test-advanced - Build and run an advanced test"
	@echo "  clean         - Remove generated files"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  ./$(TARGET) input.c -o output.ll"
