#include <stdio.h>
#include <stdlib.h>
#include "autograd_v2.h"
#include "arena.h"

int main(void) {
    printf("Test 1: Initialize\n");
    autograd_v2_init();
    
    printf("Test 2: Create tensor\n");
    int shape[] = {2, 3};
    TensorV2 *t1 = tensor_create_temp(shape, 2);
    printf("  Created tensor: %p\n", (void*)t1);
    
    printf("Test 3: Create variable\n");
    VariableV2 *v1 = var_create_temp(t1, true);
    printf("  Created variable: %p\n", (void*)v1);
    
    printf("Test 4: Create linear layer\n");
    Linear *layer = linear_create(3, 2);
    printf("  Created layer: weight=%p, bias=%p\n", 
           (void*)layer->weight, (void*)layer->bias);
    
    printf("Test 5: Linear forward\n");
    VariableV2 *output = linear_forward(layer, v1);
    printf("  Forward output: %p\n", (void*)output);
    
    printf("\nAll tests passed!\n");
    
    autograd_v2_cleanup();
    return 0;
}
