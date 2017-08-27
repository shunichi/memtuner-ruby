#include "function_hook.h"
#include "debug.h"
#include <pthread.h>

typedef int (*pthread_create_t)(pthread_t* thread, pthread_attr_t* attr, void* (*start_routine)(void*), void* arg);
typedef void (*pthread_exit_t)(void* retval);

static pthread_create_t original_pthread_create;
static pthread_exit_t original_pthread_exit;

static void* dummy_thread_func(void* arg){
    pthread_exit(NULL);
}

static void resolve_function_pointers() {
    pthread_t thread;
    pthread_create(&thread, NULL, dummy_thread_func, NULL);
    pthread_join(thread, NULL);
}

static int pthread_create_hook(pthread_t* thread, pthread_attr_t* attr, void* (*start_routine)(void*), void* arg) {
    memtuner_debug_print("pthread_create: ");
    memtuner_debug_print_hex("start_routine=", (uintptr_t)start_routine);
    memtuner_debug_println_hex(", arg=", (uintptr_t)arg);
    return original_pthread_create(thread, attr, start_routine, arg);
}

static void pthread_exit_hook(void* retval) {
    memtuner_debug_print("pthread_exit: ");
    memtuner_debug_println_hex("retval=", (uintptr_t)retval);
    original_pthread_exit(retval);
}

#define DUMP_HOOK_RESULT(name) if (original_ ## name) { memtuner_debug_print("memtuner: " #name ": hook succeeded.\n"); } else { memtuner_debug_print("memtuner: "#name ": hook faild.\n"); }
//#define DUMP_HOOK_RESULT(name)

void init_thread_tracer(void) {
    resolve_function_pointers();
    original_pthread_create = hook_function(pthread_create, pthread_create_hook);
    DUMP_HOOK_RESULT(pthread_create);
    original_pthread_exit = hook_function(pthread_exit, pthread_exit_hook);
    DUMP_HOOK_RESULT(pthread_exit);
}
