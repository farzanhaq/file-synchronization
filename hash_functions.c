#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

char *hash(FILE *f) {
    
    char *terminator_array = malloc(sizeof(char) * 8);
    
    // Check if file is NULL
    if (f == NULL) {
        fprintf(stderr, "ERROR: could not open file\n");
    }
    
    // Initialize terminator array with null terminators
    for (int i = 0; i < 8; i++) {
        terminator_array[i] = '\0';
    }
    
    char hash_array[sizeof(f)];
    
    // Read contents of file into the array
    fread(hash_array, sizeof(char), sizeof(f), f);
    
    int i = 0;
    
    // Iterate over f and assign result from bitwise operation to terminator array
    for (int j = 0; j < sizeof(f); j++) {
        terminator_array[j] = hash_array[i] ^ terminator_array[j];
        if (j == 8 - 1) {
            j = 0;
        } else {
            j++;
            i++;
        }
    }
    
    return terminator_array;
}
