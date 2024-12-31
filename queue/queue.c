
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "queue.h"

#include <stdlib.h>
/******************************************************************************/

/*******************************************************************************
 * 
 ******************************************************************************/
/******************************************************************************/

/*******************************************************************************
 * Public Function Definitions
 ******************************************************************************/
// Allocate memory for a queue and initialize it.
// Returns true if memory allocation was successful, false otherwise.
bool queue_initialize(queue_t** p_queue_ptr) {
    // Allocate memory for the queue
    *p_queue_ptr = malloc(sizeof(queue_t));
    if (!(*p_queue_ptr)) {
        return false;
    }

    // Initialize the queue
    (*p_queue_ptr)->front = NULL;
    (*p_queue_ptr)->back = NULL;
    (*p_queue_ptr)->size = 0;
    return true;
}
//------------------------------------------------------------------------------

// Allocate memory for a node to hold the data and add it to the queue.
// Returns true if memory allocation was successful, false otherwise.
bool enqueue(queue_t* queue_ptr, uint32_t data) {
    // Allocate memory for the node
    q_node_t* node_ptr = malloc(sizeof(q_node_t));
    if (!node_ptr) {
        return false;
    }

    // Add data to the node
    node_ptr->data = data;
    node_ptr->next_ptr = NULL;

    // Add this node to the queue
    if (0 == queue_ptr->size) {
        queue_ptr->front = node_ptr;
        queue_ptr->back = node_ptr;
    }
    else {
        queue_ptr->back->next_ptr = node_ptr;
        queue_ptr->back = node_ptr;
    }

    queue_ptr->size++;

    return true;
}
//------------------------------------------------------------------------------

// Remove the front node and populate the data pointer with the data it
// contained
// Returns true if there was a front node, false otherwise
bool dequeue(queue_t* queue_ptr, uint32_t* data_ptr) {
    // Verify there is a front node
    if ((0 == queue_ptr->size) || !(queue_ptr->front)) {
        return false;
    }

    // Extract data from front node
    if (data_ptr) {
        *data_ptr = queue_ptr->front->data;
    }

    // Remove front node
    q_node_t* node_to_delete = queue_ptr->front;
    queue_ptr->front = node_to_delete->next_ptr;
    free(node_to_delete);
    queue_ptr->size--;

    // Return the data
    return true;
}
//------------------------------------------------------------------------------

// Get the current length of the queue
// Returns the number of nodes in the queue
uint32_t queue_get_size(queue_t* queue_ptr) {
    return queue_ptr->size;
}
//------------------------------------------------------------------------------

// Free memory for every node in the queue, then free the queue memory
void queue_destroy(queue_t** p_queue_ptr) {
    // Destroy every node in the queue
    while((*p_queue_ptr)->front) {
        dequeue(*p_queue_ptr, NULL);
    }

    // Destroy the queue
    free(*p_queue_ptr);
    *p_queue_ptr = NULL;
}
//------------------------------------------------------------------------------

/******************************************************************************/