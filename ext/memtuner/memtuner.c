#include "memtuner.h"
#include "getrss.h"
#include <stdlib.h>
#include <stdio.h>
#if HAVE_MALLOC_INFO
#include <malloc.h> /* malloc_info */
#endif

VALUE rb_mMemtuner;

static VALUE sym_current;
static VALUE sym_peak;

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

VALUE
rb_memtuner_rss_usage(VALUE self)
{
    size_t current = getCurrentRSS();
    size_t peak = getPeakRSS();
    VALUE hash = rb_hash_new();
    rb_hash_aset(hash, sym_current, UINT2NUM(current));
    rb_hash_aset(hash, sym_peak, UINT2NUM(peak));
    return hash;
}

void
Init_memtuner(void)
{
    sym_current = ID2SYM(rb_intern("current"));
    sym_peak = ID2SYM(rb_intern("peak"));

    rb_mMemtuner = rb_define_module("Memtuner");
    rb_define_module_function(rb_mMemtuner, "malloc_info", rb_memtuner_malloc_info, 0);
    rb_define_module_function(rb_mMemtuner, "rss_usage", rb_memtuner_rss_usage, 0);
}



