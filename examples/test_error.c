// Test file with multiple errors for error recovery testing
int good_var = 42;

// Error on line 5: missing semicolon  
int bad_var = 10

// This should still parse after the error
int another_good_var = 20;

// Error in function parameter list
int bad_function(int a, , int c) {
    // Error: incomplete expression
    int result = a + ;
    
    // This should still parse
    int valid_var = 5;
    
    // Error: invalid if syntax
    if a > 0 {
        valid_var = valid_var + 1;
    }
    
    // Error: invalid assignment target
    42 = valid_var;
    
    // This should still work
    return valid_var;
}

// This function should parse correctly despite previous errors
int good_function(int x) {
    if (x > 0) {
        return x * 2;
    }
    return 0;
}

// Error: malformed function
int incomplete_func(
