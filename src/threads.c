#include "threads.h"

static const thread_attr_t _default_attrs = {
	.name = NULL, 
	.stack_size = 300, // TODO: adjust
	.priority = tskIDLE_PRIORITY + 1
}; 

void thread_attr_init(struct thread_attrs *attr){
	memcpy(attr, &_default_attrs, sizeof(thread_attr_t)); 
}

int thread_create(thread_t *thread, uint32_t stack_size, uint8_t priority, const char *name, void (*start_routine)(void*), void *arg){
    xTaskCreate(     
		/* The function that implements the task. */
		start_routine,
		/* Text name for the task, just to help debugging. */
		name,
		/* The size (in words) of the stack that should be created
		for the task. */
		stack_size,
		/* A parameter that can be passed into the task.  Not used
		in this simple demo. */
		arg,
		/* The priority to assign to the task.  tskIDLE_PRIORITY
		(which is 0) is the lowest priority.  configMAX_PRIORITIES - 1
		is the highest priority. */
		priority,
		/* Used to obtain a handle to the created task.  Not used in
		this simple demo, so set to NULL. */
		thread);
	return 0; 
}

