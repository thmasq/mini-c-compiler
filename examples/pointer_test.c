// Comprehensive pointer test for Mini C Compiler

int test_basic_pointers() {
    int x = 42;
    int *p = &x;        // Address-of operation
    int y = *p;         // Dereference operation
    
    if (y == 42) {
        return 1;       // Success
    }
    return 0;           // Failure
}

int test_pointer_arithmetic() {
    int arr[5];
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;
    
    int *p = arr;       // Array to pointer decay
    int first = *p;     // Should be 10
    
    int *q = &arr[2];   // Address of arr[2]
    int third = *q;     // Should be 30
    
    if (first == 10 && third == 30) {
        return 1;       // Success
    }
    return 0;           // Failure
}

int test_array_access() {
    int numbers[4];
    numbers[0] = 100;
    numbers[1] = 200;
    numbers[2] = 300;
    numbers[3] = 400;
    
    int sum = 0;
    int i = 0;
    while (i < 4) {
        sum = sum + numbers[i];
        i = i + 1;
    }
    
    // Sum should be 1000
    return sum;
}

int test_vla(int size) {
    int vla[size];      // Variable Length Array
    
    int i = 0;
    while (i < size) {
        vla[i] = i * 10;
        i = i + 1;
    }
    
    return vla[size - 1];  // Return last element
}

int test_multi_level_pointers() {
    int value = 123;
    int *ptr1 = &value;
    int **ptr2 = &ptr1;
    
    int result = **ptr2;   // Double dereference
    
    if (result == 123) {
        return 1;
    }
    return 0;
}

int modify_through_pointer(int *p) {
    *p = 999;
    return *p;
}

int test_function_params() {
    int x = 100;
    modify_through_pointer(&x);
    
    if (x == 999) {
        return 1;
    }
    return 0;
}

int main() {
    // Test basic pointer operations
    int basic_result = test_basic_pointers();
    
    // Test pointer arithmetic and array operations  
    int arith_result = test_pointer_arithmetic();
    
    // Test array access
    int array_sum = test_array_access();
    
    // Test VLA with size 5
    int vla_result = test_vla(5);
    
    // Test multi-level pointers
    int multi_result = test_multi_level_pointers();
    
    // Test function parameters with pointers
    int param_result = test_function_params();
    
    // Simple verification
    if (basic_result == 1 && 
        arith_result == 1 && 
        array_sum == 1000 &&
        vla_result == 40 &&
        multi_result == 1 &&
        param_result == 1) {
        return 0;  // All tests passed
    }
    
    return 1;  // Some tests failed
}
