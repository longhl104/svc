#include <stdio.h>
#include <stdlib.h>

void f(int* a) { *a += 5; }
void f(int a) { a += 5; }  //! WRONG!

int main() {
    int a = 5;
    f(&a);
    printf("%d\n", a);
}