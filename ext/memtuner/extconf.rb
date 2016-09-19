require "mkmf"

if have_header('malloc.h')
  have_func('malloc_info')
end

create_makefile("memtuner/memtuner")
