#include "call_info.h"
#include "debug.h"
#include <pthread.h>
#include <unistd.h> /* getpagesize */
#include <sys/mman.h> /* mmap */
#include <ruby/ruby.h>
#include <ruby/debug.h>

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
			memtuner_debug_print("new_call_info_buffer failed\n");
			return NULL;
		}
	    buffer->thread_id = thread_id;
    	buffer->call_infos = infos;
    	buffer->size = 0;
    	buffer->in_handler_calls = 0;
    	buffer->job_handler_queued = 0;
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
	if (buffer) {
    	buffer->size = 0;
    	buffer->in_handler_calls = 0;
    	buffer->job_handler_queued = 0;
	}
}

#define MEMTUNER_FRAME_BUFFER_SIZE 2048
static VALUE s_memtuner_frame_buffer[MEMTUNER_FRAME_BUFFER_SIZE];
static int s_memtuner_line_buffer[MEMTUNER_FRAME_BUFFER_SIZE];

typedef struct {
	size_t alloc_count;
	size_t realloc_count;
	size_t free_count;
	size_t alloc_size;
	size_t realloc_size;
} call_summary_t;

static call_summary_t build_call_summary(call_info_buffer_t const* buffer) {
	size_t i;
	call_summary_t summary = { 0 };
	for (i = 0; i < buffer->size; ++i) {
		call_info_t const* info = &buffer->call_infos[i];
		switch (info->type) {
	    case CALL_FUNC_MALLOC:
	    	summary.alloc_count += 1;
	    	summary.alloc_size += info->malloc.size;
		    break;
        case CALL_FUNC_FREE:		
	    	summary.free_count += 1;
		    break;
	    case CALL_FUNC_CALLOC:
	    	summary.alloc_count += 1;
	    	summary.alloc_size += info->calloc.size * info->calloc.count;
		    break;
	    case CALL_FUNC_REALLOC:
	    	summary.realloc_count += 1;
	    	summary.realloc_size += info->realloc.size;
		    break;
	    case CALL_FUNC_MEMALIGN:
	    	summary.alloc_count += 1;
	    	summary.alloc_size += info->memalign.size;
		    break;
	    case CALL_FUNC_POSIX_MEMALIGN:
	    	summary.alloc_count += 1;
	    	summary.alloc_size += info->posix_memalign.size;
		    break;
	    default:
		    break;
		}
	}
	return summary;
}

static void memtuner_dump_sample() {
	call_info_buffer_t* buffer = find_call_info_buffer();
    if (buffer != NULL && buffer->size > 0) {
	    int num = rb_profile_frames(0, MEMTUNER_FRAME_BUFFER_SIZE, s_memtuner_frame_buffer, s_memtuner_line_buffer);
	    int i;

		memtuner_debug_print("---------------------------\n");
    	for(i = 0; i < num; ++i) {
    		VALUE frame = s_memtuner_frame_buffer[i];
    		int line = s_memtuner_line_buffer[i];
			VALUE name = rb_profile_frame_full_label(frame);
			char const* name_cstr = StringValueCStr(name);
		    VALUE file = rb_profile_frame_absolute_path(frame);
		    if (NIL_P(file))
				file = rb_profile_frame_path(frame);    		
			memtuner_debug_print(StringValueCStr(file));
			if (line > 0)
				memtuner_debug_print_signed(":", line);
			memtuner_debug_print(" ");
			memtuner_debug_println(name_cstr);
    	}
    	{
			call_summary_t summary = build_call_summary(buffer);
			memtuner_debug_println_unsigned("  alloc count  : ", summary.alloc_count);
			memtuner_debug_println_unsigned("  free count   : ", summary.free_count);
			memtuner_debug_println_unsigned("  realloc count: ", summary.realloc_count);
			memtuner_debug_println_unsigned("  alloc size  : ", summary.alloc_size);
			memtuner_debug_println_unsigned("  realloc size: ", summary.realloc_size);
	    	memtuner_debug_println_unsigned("  in handler calls: ", buffer->in_handler_calls );
		}

    	clear_call_info_buffer();
	}
}

static int memtuner_in_handler = 0;
static void memtuner_job_handler(void* data) {
    if (memtuner_in_handler) return;

    memtuner_in_handler++;
    memtuner_dump_sample();
    memtuner_in_handler--;
}

void add_call_info(call_info_t const* info) {
	call_info_buffer_t* buffer = find_call_info_buffer();
    if (buffer != NULL && buffer->size < CALL_INFO_MAX) {
    	if (memtuner_in_handler)
    	{
    		++buffer->in_handler_calls;
    	}
    	else
    	{
        	buffer->call_infos[buffer->size++] = *info;
        	if (buffer->job_handler_queued == 0) {
	        	buffer->job_handler_queued = 1;
    	    	rb_postponed_job_register_one(0, memtuner_job_handler, 0);
        	}
    	}
    }
}

