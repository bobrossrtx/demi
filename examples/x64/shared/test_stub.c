#include <stdio.h>
extern int greet(void);
int main() {
    int r = greet();
    printf("greet() returned %d\n", r);
    return 0;
}
