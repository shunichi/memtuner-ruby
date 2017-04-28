require "mkmf"

if have_header('malloc.h')
  have_func('malloc_info')
  have_func('memalign')
  have_func('posix_memalign')
end
have_library("stdc++")
puts "$CXXFLAGS: #{$CXXFLAGS}"
$CXXFLAGS += ' -std=c++14'

create_makefile("memtuner/memtuner")
