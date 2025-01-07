#include <stdio.h>

int main(void) {
    char a[10] = "abcdef";
    char* b = a;
    printf("%lu\n", sizeof(a));
    printf("%lu\n", sizeof(b));
    return 0;
}
