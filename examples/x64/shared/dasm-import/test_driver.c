#include <stdio.h>
extern int greet(void);
int main() { printf("greet() from DASM .so returned %d\n", greet()); return 0; }
