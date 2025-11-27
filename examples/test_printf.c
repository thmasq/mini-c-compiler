// External function declaration (prototype)
extern int printf(const char*, ...);

int main() {
    printf("Hello, World!\n");
    printf("The answer is %d\n", 42);
    
    int x = 10;
    int y = 20;
    printf("Sum of %d and %d is %d\n", x, y, x + y);
    
    return 0;
}
