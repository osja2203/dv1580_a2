#include "linked_list.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER; // The global lock
 
// Shortcuts for locking/unlocking the mutex
#define LOCK()   pthread_mutex_lock(&list_mutex)
#define UNLOCK() pthread_mutex_unlock(&list_mutex)

// Initialize linked list and custom memory pool
void list_init(Node **head, size_t size){
    // Step 1: Lock the list to avoid race conditions
    LOCK();
    mem_init(size); // Initialize memory pool
    *head = NULL; // Set list head to NULL since list is empty at start
    UNLOCK(); // Unlock the list
}

// Helper to allocate new node
static Node *new_node(uint16_t data){
    // Step 1: Allocate memory from memory pool
    Node *n = (Node *)mem_alloc(sizeof(Node));
    if (!n)
        return NULL; // Allocation failed

    // Step 2: Initialize node data
    n->data = data; // Store the data
    n->next = NULL; // No next node

    // Step 3: Initialize node's individual mutex
    pthread_mutex_init(&n->lock, NULL);
    return n;
}

// Insert a node at the end of the list
void list_insert(Node **head, uint16_t data){
    // Step 1: Lock the entire list
    LOCK();

    // Step 2: Create a new node to insert
    Node *n = new_node(data);
    if (!n) {
        fprintf(stderr, "list_insert: allocation failed\n");
        UNLOCK();
        return;
    }

    // Step 3: If list is empty, new node becomes head
    if (*head == NULL) {
        *head = n;
    } else {
        // Step 4: Otherwise, walk to the end of the list
        Node *cur = *head;
        while (cur->next)
            cur = cur->next;

        // Step 5: Add the new node at the end
        cur->next = n;
    }

    // Step 6: Unlock the list
    UNLOCK();
}

// Insert a node after a "specific node"
void list_insert_after(Node *prev_node, uint16_t data){
    // Step 1: Validate input
    if (!prev_node) {
        fprintf(stderr, "list_insert_after: prev_node is NULL\n");
        return;
    }

    // Step 2: Lock the list
    LOCK();

    // Step 3: Allocate the new node
    Node *n = new_node(data);
    if (!n) {
        fprintf(stderr, "list_insert_after: allocation failed\n");
        UNLOCK();
        return;
    }

    // Step 4: Insert new node after "previous node" and unlock list
    n->next = prev_node->next;
    prev_node->next = n;
    UNLOCK();
}

// Insert a node before a "specific node"
void list_insert_before(Node **head, Node *next_node, uint16_t data){
    // Step 1: Validate input
    if (!next_node) {
        fprintf(stderr, "list_insert_before: next_node is NULL\n");
        return;
    }

    // Step 2: Lock the list
    LOCK();

    // Step 3: Special case; insert before head
    if (*head == next_node) {
        Node *n = new_node(data);
        if (!n) {
            fprintf(stderr, "list_insert_before: allocation failed\n");
            UNLOCK();
            return;
        }
        n->next = *head;
        *head = n;
        UNLOCK();
        return;
    }

    // Step 4: Walk through list to find node before "next_node"
    Node *cur = *head;
    while (cur && cur->next != next_node)
        cur = cur->next;

    // Step 5: If "next_node" not found, print error
    if (!cur) {
        fprintf(stderr, "list_insert_before: next_node not found\n");
        UNLOCK();
        return;
    }

    // Step 6: Create and insert new node
    Node *n = new_node(data);
    if (!n) {
        fprintf(stderr, "list_insert_before: allocation failed\n");
        UNLOCK();
        return;
    }
    n->next = cur->next;
    cur->next = n;

    // Step 7: Unlock the list
    UNLOCK();
}

// Delete first node that matches the data
void list_delete(Node **head, uint16_t data){
    // Step 1: Lock the list
    LOCK();

    // Step 2: Check if the list is empty
    if (!*head) {
        fprintf(stderr, "list_delete: empty list, nothing to delete\n");
        UNLOCK();
        return;
    }

    // Step 3: Walk through list to find the matching node
    Node *cur = *head;
    Node *prev = NULL;
    while (cur && cur->data != data) {
        prev = cur;
        cur = cur->next;
    }

    // Step 4: If not found, print and exit
    if (!cur) {
        fprintf(stderr, "list_delete: value %u not found\n", data);
        UNLOCK();
        return;
    }

    // Step 5: Remove node from list
    if (!prev) // deleting the head
        *head = cur->next;
    else
        prev->next = cur->next;

    // Step 6: Clean up node and unlock the list
    pthread_mutex_destroy(&cur->lock);
    mem_free(cur);
    UNLOCK();
}

// Search for node by value
Node *list_search(Node **head, uint16_t data){
    // Step 1: Lock the list
    LOCK();

    // Step 2: Walk through the list looking for the value
    Node *cur = *head;
    while (cur) {
        if (cur->data == data) {
            UNLOCK();
            return cur;
        }
        cur = cur->next;
    }

    // Step 3: Not found, unlock and return NULL
    UNLOCK();
    return NULL;
}

// Helper to print a list range (from start to end)
static void print_range(Node **head, Node *start, Node *end){
    // Step 1: Print opening bracket for the list output
    putchar('[');

    // Step 2: If "start" is provided begin from it, otherwise start from the head of the list
    Node *cur = start ? start : *head;
    int first = 1; // Used to decide whether to print a comma before the number

    // Step 3: Walk through the list
    while (cur) {
        if (!first)
            printf(", ");

        printf("%u", cur->data);
        if (cur == end)
            break;

        cur = cur->next; // Move to the next node
        first = 0;
    }
    // Step 4: Print closing bracket for the list output
    putchar(']');
}

// Print all elements in the list
void list_display(Node **head) {
    // Step 1: Lock the list
    LOCK();

    // Step 2: Print the whole list (start and end are NULL)
    print_range(head, NULL, NULL);

    // Step 3: Unlock the list
    UNLOCK();
}

// Print elements between two given nodes
void list_display_range(Node **head, Node *start_node, Node *end_node) {
    // Step 1: Lock the list
    LOCK();

    // Step 2: If the list is empty, just print "[]" and return
    if (!*head) {
        printf("[]");
        UNLOCK();
        return;
    }

    // Step 3: Call the helper to print from "start_node" to "end_node"
    print_range(head, start_node, end_node);

    // Step 4: Unlock the list
    UNLOCK();
}

// Count the total number of nodes in the list
int list_count_nodes(Node **head) {
    // Step 1: Lock the list
    LOCK();

    // Step 2: Walk through the list and count each node
    int count = 0;
    for (Node *cur = *head; cur; cur = cur->next)
        ++count;

    // Step 3: Unlock and return the count
    UNLOCK();
    return count;
}

// Free all nodes and clean up memory
void list_cleanup(Node **head) {
    // Step 1: Lock the list
    LOCK();

    // Step 2: Walk through the list and free each node one by one
    Node *cur = *head;
    while (cur) {
        Node *next = cur->next; // Store pointer to next node before freeing current
        pthread_mutex_destroy(&cur->lock); // Destroy the mutex
        mem_free(cur); // Free the memory used by the node
        cur = next; // Move to the next node
    }

    // Step 3: Reset the head to NULL since the list is now empty
    *head = NULL;

    // Step 4: Deinitialize the memory pool and unlock the list
    mem_deinit();
    UNLOCK();
}