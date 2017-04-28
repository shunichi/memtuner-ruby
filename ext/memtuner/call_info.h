
#ifndef __CALL_INFO_H
#define __CALL_INFO_H
#include <pthread.h>

typedef enum {
    CALL_FUNC_MALLOC,
    CALL_FUNC_FREE,
    CALL_FUNC_CALLOC,
    CALL_FUNC_REALLOC,
    CALL_FUNC_MEMALIGN,
    CALL_FUNC_POSIX_MEMALIGN,
} call_func_type_t;

typedef struct {
    size_t size;
    void* allocated;
} malloc_call_info_t;

typedef struct {
    void* ptr;
} free_call_info_t;

typedef struct {
    size_t size;
    void* ptr;
    void* allocated;
} realloc_call_info_t;

typedef struct {
    size_t count;
    size_t size;
    void* allocated;
} calloc_call_info_t;

typedef struct {
    size_t align;
    size_t size;
    void* allocated;
} memalign_call_info_t;

typedef struct {
    size_t align;
    size_t size;
    void* allocated;
    int return_value;
} posix_memalign_call_info_t;

typedef struct {
    call_func_type_t type;
    union {
        malloc_call_info_t malloc;
        calloc_call_info_t calloc;
        memalign_call_info_t memalign;
        posix_memalign_call_info_t posix_memalign;
        realloc_call_info_t realloc;
        free_call_info_t free;
    };
} call_info_t;

static size_t const CALL_INFO_MAX = 10000;
typedef struct {
    pthread_t thread_id;
    call_info_t* call_infos;
    size_t size;
} call_info_buffer_t;

extern void clear_call_info_buffer(void);
extern void add_call_info(call_info_t const* info);

#endif