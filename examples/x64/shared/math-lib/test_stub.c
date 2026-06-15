#include <stdio.h>
extern int math_add(int a, int b);
extern int math_sub(int a, int b);
extern int math_mul(int a, int b);
int main() {
    printf("math_add(3, 4)  = %d\n", math_add(3, 4));
    printf("math_sub(10, 7) = %d\n", math_sub(10, 7));
    printf("math_mul(6, 7)  = %d\n", math_mul(6, 7));
    return 0;
}
