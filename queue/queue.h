/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdbool.h>
#include <stdint.h>
/******************************************************************************/

/*******************************************************************************
 * Public Typedefs
 ******************************************************************************/
typedef struct q_node_t {
    struct q_node_t * next_ptr;
    uint32_t data;
} q_node_t;

typedef struct {
    q_node_t* front;
    q_node_t* back;
    uint32_t size;
} queue_t;
/******************************************************************************/

/*******************************************************************************
 * Public Function Prototypes
 ******************************************************************************/
bool queue_initialize(queue_t** p_queue_ptr);
bool enqueue(queue_t* queue_ptr, uint32_t data);
bool dequeue(queue_t* queue_ptr, uint32_t* data_ptr);
uint32_t queue_get_size(queue_t* queue_ptr);
void queue_destroy(queue_t** p_queue_ptr);
/******************************************************************************/
