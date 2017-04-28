#include "call_info.h"
#include "debug.h"
#include <pthread.h>
#include <unistd.h> /* getpagesize */
#include <sys/mman.h> /* mmap */

#define ROUND_UP(n, align) (((n) + (align) - 1) / (align) * (align))
#define MALLOC_TRACER_THREAD_MAX 256

static call_info_buffer_t s_thread_call_info_buffers[MALLOC_TRACER_THREAD_MAX];
static size_t s_used_thread_count = 0;
static pthread_mutex_t s_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

static call_info_buffer_t* new_call_info_buffer(pthread_t thread_id)
{
	if (s_used_thread_count < MALLOC_TRACER_THREAD_MAX) {
		call_info_buffer_t* const buffer = &s_thread_call_info_buffers[s_used_thread_count++];
	    size_t const page_size = getpagesize();
	    size_t const len = ROUND_UP(sizeof(call_info_t) * CALL_INFO_MAX, page_size);
		call_info_t* infos = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (infos == MAP_FAILED) {
			debug_print("new_call_info_buffer failed\n");
			return NULL;
		}
	    buffer->thread_id = thread_id;
    	buffer->call_infos = infos;
    	buffer->size = 0;
    	return buffer;
    } else {
    	return NULL;
    }
}

call_info_buffer_t* find_call_info_buffer() {
	call_info_buffer_t* ret = NULL;
	size_t i = 0;
	pthread_t current_thread_id = pthread_self();

	pthread_mutex_lock(&s_buffer_mutex);
	for(i = 0; i < s_used_thread_count; ++i) {
		call_info_buffer_t* buffer = &s_thread_call_info_buffers[i];
		if (buffer->call_infos != NULL && pthread_equal(buffer->thread_id, current_thread_id)) {
			ret = buffer;
			break;
		}
	}
	if (ret == NULL)
		ret = new_call_info_buffer(current_thread_id);
	pthread_mutex_unlock(&s_buffer_mutex);

	return ret;
}

void clear_call_info_buffer(void) {
	call_info_buffer_t* buffer = find_call_info_buffer();
	if (buffer) 
		buffer->size = 0;
}

void add_call_info(call_info_t const* info) {
	call_info_buffer_t* buffer = find_call_info_buffer();
    if (buffer != NULL && buffer->size < CALL_INFO_MAX) {
        buffer->call_infos[buffer->size++] = *info;
    }
}

