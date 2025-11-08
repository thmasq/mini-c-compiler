
// examples/test_semantic_errors.c
// This file contains several simple semantic errors to test the semantic analyzer.

int add(int x, int y) {
    return x + y;
}

int main() {
    int a = 5;
    int a = 10; // Error: Redeclaration of 'a' in the same scope.

    a = b + 5;  // Error: 'b' is not declared.

    int c;
    c = "hello"; // Error: Type mismatch, assigning a string literal to an int.

    int result;
    result = add(5); // Error: Incorrect number of arguments for function 'add'.

    return "done"; // Error: Returning a string literal from a function that should return an int.
}
