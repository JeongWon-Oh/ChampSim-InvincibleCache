#include <stdlib.h>

// Replace these with actual LLC cache parameters
#define LLC_SETS 1024      // Number of sets in LLC
#define LLC_ASSOC 4        // LLC associativity
#define LLC_LINE_SIZE 64   // Cache line size in bytes

int main() {
    // Calculate the total size needed to fill the cache
    size_t llc_size = LLC_SETS * LLC_ASSOC * LLC_LINE_SIZE;

    // Allocate a buffer of the size of the LLC
    char *buffer = malloc(llc_size);
    if (buffer == NULL) {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }
	for (size_t j = 0; j < (1<<(sizeof(size_t))); j++) {
    // Access each cache line to ensure it's loaded into the cache
    for (size_t i = 0; i < llc_size; i += LLC_LINE_SIZE) {
        buffer[i] = (char)i;  // Write to the cache line to ensure a cache fill
    }
	}
    // Optional: Use the buffer here to prevent compiler optimizations that might remove the buffer

    // Free the allocated buffer
    free(buffer);

    return EXIT_SUCCESS;
}

