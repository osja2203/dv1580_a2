#include "memory_manager.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
 
typedef struct MemBlock {
     size_t offset;          // Where this block starts in the memory pool
     size_t size;            // How big this block is
     int    is_free;         // 1 = block is free, 0 = it's in use
     struct MemBlock *next;  // Points to the next block in the list
} MemBlock;

//Global variables
static char      *memory_pool  = NULL;   // The entire memory pool
static size_t     pool_size    = 0;      // How big the memory pool is
static MemBlock  *block_list   = NULL;   // The list of memory blocks (free/used)
 
static pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER; // The global lock
 
 // Shortcuts for locking/unlocking the mutex
#define LOCK()   pthread_mutex_lock(&mem_mutex)
#define UNLOCK() pthread_mutex_unlock(&mem_mutex)
 
// Helper function to create the first block
static void create_initial_block(size_t size) {
     // Step 1: Allocate memory for a new block
     block_list = malloc(sizeof(MemBlock));
 
     // Step 2: If something went wrong, clean up and exit
     if (!block_list) {
         fprintf(stderr, "Error: Could not allocate block list (%s)\n", strerror(errno));
         free(memory_pool);
         exit(EXIT_FAILURE);
     }
 
     // Step 3: Fill in the details for the very first block
     block_list->offset  = 0;
     block_list->size    = size;
     block_list->is_free = 1;       // 1 = free
     block_list->next    = NULL;    // No blocks after
}
 

// Set up the memory pool and first block
void mem_init(size_t size) {
     // Step 1: Allocate the full memory pool
     memory_pool = malloc(size);
     if (!memory_pool) {
         fprintf(stderr, "Error: Could not allocate memory pool (%s)\n", strerror(errno));
         exit(EXIT_FAILURE);
     }
 
     // Step 2: Lock, initialize metadata, then unlock
     LOCK();
     pool_size = size;
     create_initial_block(size);
     UNLOCK();
}
 
// Try to find space and give the user a pointer to it
void *mem_alloc(size_t size) {
     LOCK();
     // Step 1: Special case; if size is 0, return first free block if any
     if (size == 0) {
         for (MemBlock *curr = block_list; curr; curr = curr->next) {
             if (curr->is_free) {
                 void *ptr = memory_pool + curr->offset;
                 UNLOCK();
                 return ptr;
             }
         }
         UNLOCK();
         return NULL;
     }
     // Step 2: Look for a big enough "free" block
     for (MemBlock *curr = block_list; curr; curr = curr->next) {
         if (!curr->is_free || curr->size < size) continue;
 
         // Step 3: If the block is bigger than needed, split it
         if (curr->size > size) {
             MemBlock *new_block = malloc(sizeof(MemBlock));
             if (!new_block) {
                 UNLOCK();
                 return NULL;
             }
 
             // Step 4: Create new block for leftover space
             new_block->offset  = curr->offset + size;
             new_block->size    = curr->size - size;
             new_block->is_free = 1;
             new_block->next    = curr->next;
 
             // Step 5: Update current block
             curr->size    = size;
             curr->is_free = 0;
             curr->next    = new_block;
         } else {
             // Step 6: Exact fit, mark as used
             curr->is_free = 0;
         }
 
         // Step 7: Return pointer to start of the block
         void *ptr = memory_pool + curr->offset;
         UNLOCK();
         return ptr;
     }
 
     // Step 8: If we got here, no memory available
     UNLOCK();
     return NULL;
}

//Free a block of memory and merge with neighbors if possible
void mem_free(void *ptr) {
     if (!ptr) return;
 
     LOCK();
     // Step 1: Find the block that matches the pointer
     size_t offset = (char *)ptr - memory_pool;
     MemBlock *prev = NULL;
 
     for (MemBlock *curr = block_list; curr; curr = curr->next) {
         if (curr->offset != offset) {
             prev = curr;
             continue;
         }
 
         // Step 2: Already free? Do nothing
         if (curr->is_free) { UNLOCK(); return; }
 
         // Step 3: Mark block as free
         curr->is_free = 1;
 
         // Step 4: Merge with next block if next block is free
         if (curr->next && curr->next->is_free) {
             MemBlock *next = curr->next;
             curr->size += next->size;
             curr->next = next->next;
             free(next);
         }
 
         // Step 5: Merge with previous block if it’s free
         if (prev && prev->is_free) {
             prev->size += curr->size;
             prev->next  = curr->next;
             free(curr);
         }
        
        
         UNLOCK();
         return;
     }
     UNLOCK();
}
 
// Change the size of a block
void *mem_resize(void *ptr, size_t size) {
    // Step 1: If the pointer is NULL, allocate new memory
    if (!ptr) return mem_alloc(size);

    // Step 2: If new size is 0, free the block and return NULL
    if (size == 0) {
        mem_free(ptr);
        return NULL;
    }

    LOCK();
    // Step 3: Calculate the offset for the block and find it in the block list
    size_t offset = (char *)ptr - memory_pool;
    for (MemBlock *curr = block_list; curr; curr = curr->next) {
        if (curr->offset != offset) continue;

        // Step 4: If the block is already big enough, shrink it if needed
        if (curr->size >= size) {
            if (curr->size > size) {
                // Create a new free block with leftover space
                MemBlock *new_block = malloc(sizeof(MemBlock));
                if (!new_block) {
                    UNLOCK();
                    return NULL;
                }

                new_block->offset  = curr->offset + size;
                new_block->size    = curr->size - size;
                new_block->is_free = 1;
                new_block->next    = curr->next;

                curr->size = size;
                curr->next = new_block;
            }

            UNLOCK();
            return ptr; // Done shrinking, return pointer
        }

        // Step 5: Try to expand into the next block if it’s free and big enough
        if (curr->next && curr->next->is_free &&
            curr->size + curr->next->size >= size) {

            // Merge current and next block
            MemBlock *next = curr->next;
            curr->size += next->size;
            curr->next  = next->next;
            free(next);

            // If there's still extra space, split again
            if (curr->size > size) {
                MemBlock *new_block = malloc(sizeof(MemBlock));
                if (!new_block) {
                    UNLOCK();
                    return NULL;
                }

                new_block->offset  = curr->offset + size;
                new_block->size    = curr->size - size;
                new_block->is_free = 1;
                new_block->next    = curr->next;

                curr->size = size;
                curr->next = new_block;
            }
           
            UNLOCK();
            return ptr; // Return pointer
        }

        // Step 6: Can't resize in place, so allocate new memory
        size_t old_size = curr->size;
        UNLOCK(); // Unlock before doing a new allocation

        // Allocate a new block
        void *new_ptr = mem_alloc(size);
        if (!new_ptr) return NULL;

        // Copy old data to the new block
        memcpy(new_ptr, ptr, old_size < size ? old_size : size);

        // Free the old block
        mem_free(ptr);

        return new_ptr; // Return new pointer
    }

    // Step 7: Block was not found in the list
    UNLOCK();
    return NULL;
}

// Free everything and reset the memory manager
void mem_deinit() {
     LOCK();
 
     // Step 1: Free the memory pool
     free(memory_pool);
     memory_pool = NULL;
     pool_size   = 0;
 
     // Step 2: Free all the metadata blocks
     MemBlock *curr = block_list;
     while (curr) {
         MemBlock *next = curr->next;
         free(curr);
         curr = next;
     }
     block_list = NULL;
 
     UNLOCK();
}
