#include "memtuner.h"

VALUE rb_mMemtuner;

void
Init_memtuner(void)
{
  rb_mMemtuner = rb_define_module("Memtuner");
}
