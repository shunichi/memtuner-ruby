#include "memtuner.h"
#include <stdlib.h>
#include <stdio.h>
#if HAVE_MALLOC_INFO
#include <malloc.h> /* malloc_info */
#endif

VALUE rb_mMemtuner;

VALUE
rb_memtuner_malloc_info(VALUE self)
{
#if HAVE_MALLOC_INFO
    VALUE str;
    char *buf;
    size_t size;
    FILE *fp = open_memstream(&buf, &size);
    malloc_info(0, fp);
    fclose(fp);
    str = rb_str_new(buf, size);
    free(buf);
    return str;
#else
    return Qnil;
#endif
}

void
Init_memtuner(void)
{
  rb_mMemtuner = rb_define_module("Memtuner");

  rb_define_module_function(rb_mMemtuner, "malloc_info", rb_memtuner_malloc_info, 0);
}



