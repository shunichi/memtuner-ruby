#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h> /* write, getpagesize */
#if HAVE_MALLOC_INFO
#include <malloc.h>
#endif
#include "function_hook.h"
#include "debug.h"

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

static void *malloc_hook(size_t size);
static void free_hook(void *p);
static void *realloc_hook(void *p, size_t size);
static void *calloc_hook(size_t n, size_t size);
#if HAVE_MEMALIGN
static void *memalign_hook(size_t align, size_t size);
#endif
#if HAVE_POSIX_MEMALIGN
static int posix_memalign_hook(void **memptr, size_t alignment, size_t size);
#endif

enum {
    FUNC_INDEX_MALLOC,
    FUNC_INDEX_FREE,
    FUNC_INDEX_REALLOC,
    FUNC_INDEX_CALLOC,
#if HAVE_MEMALIGN
    FUNC_INDEX_MEMALIGN,
#endif
};

#if 0
static struct {
    char const* name;
    void const* fp;
    void const *hook_fp;
    void const *original_fp;
} func_tables[] = {
    { "malloc", malloc, malloc_hook, },
    { "free", free, free_hook, },
    { "realloc", realloc, realloc_hook, },
    { "calloc", calloc, calloc_hook, },
#if HAVE_MEMALIGN
    { "memalign", memalign, memalign_hook, },
#endif
};
static size_t read_word(unsigned char const *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
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

static void *malloc_hook(size_t size) {
    void* p = original_malloc(size);
    write_stdout("call malloc\n");
    return p;
}

static void free_hook(void *p) {
    write_stdout("call free\n");
    original_free(p);
}

static void *realloc_hook(void *p, size_t size) {
    void* new_p = original_realloc(p, size);
    write_stdout("call realloc\n");
    return new_p;
}
static void *calloc_hook(size_t n, size_t size) {
    void* p = original_calloc(n, size);
    write_stdout("call calloc\n");
    return p;
}
#if HAVE_MEMALIGN
static void *memalign_hook(size_t align, size_t size) {
    void* p = original_memalign(align, size);
    write_stdout("call memalign\n");
    return p;
}
#endif
#if HAVE_POSIX_MEMALIGN
static int posix_memalign_hook(void **memptr, size_t alignment, size_t size)
{
    int ret = original_posix_memalign(memptr, alignment, size);
    write_stdout("call posix_memalign\n");
    return ret;
}
#endif

static void hook_functions(void) {
    original_malloc = hook_function(malloc, malloc_hook);
    original_free = hook_function(free, free_hook);
    original_calloc = hook_function(calloc, calloc_hook);
    original_realloc = hook_function(realloc, realloc_hook);
#if HAVE_MEMALIGN
    original_memalign = hook_function(memalign, memalign_hook);
#endif
#if HAVE_POSIX_MEMALIGN
    original_posix_memalign = hook_function(posix_memalign, posix_memalign_hook);
#endif
}

void init_malloc_tracer(void){
    write_stdout("init_malloc_tracer\n");

    resolve_function_pointers();
    hook_functions();

#if 0
    size_t i = 0;
    for(i = 0; i < sizeof(func_tables)/sizeof(func_tables[0]); ++i){
        /*
         この方法はダメだった。
         fp が plt であることを期待していたが、 shared library としてコンパイルすると
         関数ポインタが直接 __libc_malloc を指していた。
        */
        unsigned char const *fp = (unsigned char const *)func_tables[i].fp;
        /* jmpq *0xXXXXXXXX(%rip) ff 25 XX XX XX XX */
        if (fp[0] == 0xff && fp[1] == 0x25) {
            void const **ppfunc = (void const **)(fp + 6 + read_word(fp + 2));
            func_tables[i].original_fp = *ppfunc;
            *ppfunc = func_tables[i].hook_fp;
            write_stdout(func_tables[i].name);
            write_stdout(" hooked\n");
        }
    }
#endif
}
