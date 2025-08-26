// Test program for pointers and arrays in Mini C Compiler

// Function to test basic array operations
int test_arrays() {
    // Fixed size array
    int numbers[5];
    
    // Initialize array elements
    numbers[0] = 10;
    numbers[1] = 20;
    numbers[2] = 30;
    numbers[3] = 40;
    numbers[4] = 50;
    
    // Access and use array elements
    int sum = numbers[0] + numbers[1];
    int product = numbers[2] * numbers[3];
    
    // Test array indexing with variables
    int i = 1;
    int value = numbers[i];
    
    return sum + product + value;
}

// Function to test basic pointer operations
int test_pointers() {
    int x = 42;
    int* ptr;
    
    // Get address of x
    ptr = &x;
    
    // Dereference pointer
    int value = *ptr;
    
    // Modify value through pointer
    *ptr = 100;
    
    return x + value; // Should be 100 + 42 = 142
}

// Function to test pointer and array interaction
int test_pointer_array() {
    int arr[3];
    int* p;
    
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    
    // Point to first element of array
    p = &arr[0];
    
    // Access through pointer
    int first = *p;
    
    return first + arr[1] + arr[2]; // Should be 1 + 2 + 3 = 6
}

// Function with array parameter
int sum_array(int* arr, int size) {
    int total = 0;
    int i = 0;
    
    while (i < size) {
        total = total + arr[i];
        i = i + 1;
    }
    
    return total;
}

// Main function to test all features
int main() {
    int result1 = test_arrays();
    int result2 = test_pointers();
    int result3 = test_pointer_array();
    
    // Test array parameter
    int test_arr[4];
    test_arr[0] = 5;
    test_arr[1] = 10;
    test_arr[2] = 15;
    test_arr[3] = 20;
    int result4 = sum_array(test_arr, 4);
    
    // Test pointer arithmetic (basic)
    int data[3];
    data[0] = 100;
    data[1] = 200;
    data[2] = 300;
    
    int* ptr = &data[0];
    int val1 = *ptr;        // 100
    int val2 = data[1];     // 200 (direct access)
    
    return result1 + result2 + result3 + result4 + val1 + val2;
}
