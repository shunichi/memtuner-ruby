#include "memtuner.h"
#include "getrss.h"
#include "malloc_tracer.h"
#include <stdlib.h>
#include <stdio.h>
#if HAVE_MALLOC_INFO
#include <malloc.h> /* malloc_info */
#endif

VALUE rb_mMemtuner;

static VALUE sym_current;
static VALUE sym_peak;

/* struct mallinfo members */
static VALUE sym_arena;     /* Non-mmapped space allocated (bytes) */
static VALUE sym_ordblks;   /* Number of free chunks */
static VALUE sym_smblks;    /* Number of free fastbin blocks */
static VALUE sym_hblks;     /* Number of mmapped regions */
static VALUE sym_hblkhd;    /* Space allocated in mmapped regions (bytes) */
static VALUE sym_usmblks;   /* Maximum total allocated space (bytes) */
static VALUE sym_fsmblks;   /* Space in freed fastbin blocks (bytes) */
static VALUE sym_uordblks;  /* Total allocated space (bytes) */
static VALUE sym_fordblks;  /* Total free space (bytes) */
static VALUE sym_keepcost;  /* Top-most, releasable space (bytes) */

VALUE rb_memtuner_mallinfo(VALUE self)
{
#if HAVE_MALLOC_INFO
    struct mallinfo mi = mallinfo();
    VALUE hash = rb_hash_new();
    rb_hash_aset(hash, sym_arena, INT2NUM(mi.arena));
    rb_hash_aset(hash, sym_ordblks, INT2NUM(mi.ordblks));
    rb_hash_aset(hash, sym_smblks, INT2NUM(mi.smblks));
    rb_hash_aset(hash, sym_hblks, INT2NUM(mi.hblks));
    rb_hash_aset(hash, sym_hblkhd, INT2NUM(mi.hblkhd));
    rb_hash_aset(hash, sym_usmblks, INT2NUM(mi.usmblks));
    rb_hash_aset(hash, sym_fsmblks, INT2NUM(mi.fsmblks));
    rb_hash_aset(hash, sym_uordblks, INT2NUM(mi.uordblks));
    rb_hash_aset(hash, sym_fordblks, INT2NUM(mi.fordblks));
    rb_hash_aset(hash, sym_keepcost, INT2NUM(mi.keepcost));
    return hash;
#else
    return Qnil;
#endif
}

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
    init_malloc_tracer();

#define DEF_SYM(name) sym_ ## name = ID2SYM(rb_intern(#name))
    DEF_SYM(current);
    DEF_SYM(peak);
    DEF_SYM(arena);
    DEF_SYM(ordblks);
    DEF_SYM(smblks);
    DEF_SYM(hblks);
    DEF_SYM(hblkhd);
    DEF_SYM(usmblks);
    DEF_SYM(fsmblks);
    DEF_SYM(uordblks);
    DEF_SYM(fordblks);
    DEF_SYM(keepcost);
#undef DEF_SYM

    rb_mMemtuner = rb_define_module("Memtuner");
    rb_define_module_function(rb_mMemtuner, "glibc_mallinfo", rb_memtuner_mallinfo, 0);
    rb_define_module_function(rb_mMemtuner, "glibc_malloc_info", rb_memtuner_malloc_info, 0);
    rb_define_module_function(rb_mMemtuner, "rss_usage", rb_memtuner_rss_usage, 0);
}



