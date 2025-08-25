int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b; // Note: modulo not implemented yet
        a = temp;
    }
    return a;
}

int power(int base, int exp) {
    int result = 1;
    int i = 0;
    
    while (i < exp) {
        result = result * base;
        i = i + 1;
    }
    
    return result;
}

int main() {
    int x = 12;
    int y = 18;
    
    // Test arithmetic and assignments
    int sum = x + y;
    int diff = x - y;
    int product = x * y;
    int quotient = x / 3;
    
    // Test conditionals
    if (x > y) {
        int max = x;
    } else {
        int max = y;
    }
    
    // Test function calls
    int result = power(2, 4);
    
    // Test comparisons
    if (sum == 30) {
        return 1;
    }
    
    if (product != 216) {
        return 0;
    }
    
    // Test while loop with function call
    int counter = 5;
    while (counter > 0) {
        int val = power(counter, 2);
        counter = counter - 1;
    }
    
    return result;
}
