#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    uint16_t *var_ptr = (uint16_t *)malloc(sizeof(uint16_t) * 4);
    printf("%x\n", &var_ptr);
    uint16_t *var_ptr2 = realloc(var_ptr, sizeof(uint16_t));
    printf("%x\n", &var_ptr2);
    return 0;
}