// Standard library functions (external declarations)
extern int printf(const char*, ...);
extern int puts(const char*);
extern void *malloc(unsigned long);
extern void free(void*);

// Helper function to demonstrate mixing internal and external
int calculate(int a, int b) {
    return (a * b) + a + b;
}

int main() {
    // Use external printf
    printf("Starting program...\n");
    
    // Use external puts
    puts("Testing external functions");
    
    // Calculate something with internal function
    int result = calculate(5, 7);
    printf("Calculation result: %d\n", result);
    
    // Use external malloc/free (just demonstration, not dereferencing)
    void *ptr = malloc(100);
    if (ptr) {
        puts("Memory allocated successfully");
        free(ptr);
        puts("Memory freed");
    }
    
    return 0;
}
