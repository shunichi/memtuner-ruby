#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if HAVE_MALLOC_INFO
#include <malloc.h>
#endif
#include <ruby/ruby.h>
#include "function_hook.h"
#include "debug.h"
#include "call_info.h"

typedef void *(*malloc_func_t)(size_t);
typedef void (*free_func_t)(void *);
typedef void *(*realloc_func_t)(void *, size_t);
typedef void *(*calloc_func_t)(size_t, size_t);
typedef void *(*memalign_func_t)(size_t, size_t);
typedef int (*posix_memalign_t)(void **memptr, size_t alignment, size_t size);

static malloc_func_t original_malloc;
static free_func_t original_free;
static realloc_func_t original_realloc;
static calloc_func_t original_calloc;
#if HAVE_MEMALIGN
static memalign_func_t original_memalign;
#endif
#if HAVE_POSIX_MEMALIGN
static posix_memalign_t original_posix_memalign;
#endif
static void *malloc_hook(size_t size) {
    void* p = original_malloc(size);
    call_info_t info;
    info.type = CALL_FUNC_MALLOC;
    info.malloc.size = size;
    info.malloc.allocated = p;
    add_call_info(&info);

    debug_print("call malloc\n");
    // rb_postponed_job_register_one();
    return p;
}

static void free_hook(void *p) {
    call_info_t info;
    info.type = CALL_FUNC_FREE;
    info.free.ptr = p;
    add_call_info(&info);

    debug_print("call free\n");
    original_free(p);
}

static void *realloc_hook(void *p, size_t size) {
    void* new_p = original_realloc(p, size);
    call_info_t info;
    info.type = CALL_FUNC_REALLOC;
    info.realloc.size = size;
    info.realloc.ptr = p;
    info.realloc.allocated = new_p;
    add_call_info(&info);

    debug_print("call realloc\n");
    return new_p;
}
static void *calloc_hook(size_t n, size_t size) {
    void* p = original_calloc(n, size);
    call_info_t info;
    info.type = CALL_FUNC_CALLOC;
    info.calloc.size = size;
    info.calloc.count = n;
    info.calloc.allocated = p;
    add_call_info(&info);

    debug_print("call calloc\n");
    return p;
}
#if HAVE_MEMALIGN
static void *memalign_hook(size_t align, size_t size) {
    void* p = original_memalign(align, size);
    call_info_t info;
    info.type = CALL_FUNC_MEMALIGN;
    info.memalign.size = size;
    info.memalign.align = align;
    info.memalign.allocated = p;
    add_call_info(&info);

    debug_print("call memalign\n");
    return p;
}
#endif
#if HAVE_POSIX_MEMALIGN
static int posix_memalign_hook(void **pp, size_t align, size_t size)
{
    int ret = original_posix_memalign(pp, align, size);
    call_info_t info;
    info.type = CALL_FUNC_POSIX_MEMALIGN;
    info.posix_memalign.size = size;
    info.posix_memalign.align = align;
    info.posix_memalign.allocated = *pp;
    info.posix_memalign.return_value = ret;
    add_call_info(&info);

    debug_print("call posix_memalign\n");
    return ret;
}
#endif

static void resolve_function_pointers(void) {
    void *p = malloc(1);
    p = realloc(p, 2);
    free(p);
    p = calloc(1, 2);
    free(p);
#if HAVE_MEMALIGN
    p = memalign(4, 1);
    free(p);
#endif
#if HAVE_POSIX_MEMALIGN
    if(posix_memalign(&p, 4, 1) == 0)
      free(p);
#endif
}

static void hook_functions(void) {
    resolve_function_pointers();

    debug_print("----- realloc\n");
    original_realloc = hook_function(realloc, realloc_hook);
    debug_print("----- malloc\n");
    original_malloc = hook_function(malloc, malloc_hook);
    debug_print("----- free\n");
    original_free = hook_function(free, free_hook);
    debug_print("----- calloc\n");
    original_calloc = hook_function(calloc, calloc_hook);
#if HAVE_MEMALIGN
   debug_print("----- memalign\n");
   original_memalign = hook_function(memalign, memalign_hook);
#endif
#if HAVE_POSIX_MEMALIGN
    debug_print("----- posix_memalign\n");
    original_posix_memalign = hook_function(posix_memalign, posix_memalign_hook);
#endif
}

void init_malloc_tracer(void){
    debug_print("init_malloc_tracer\n");

    hook_functions();
}
