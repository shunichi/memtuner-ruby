require "memtuner/version"
require "memtuner/memtuner"
require "memtuner/memory_statistics"

module Memtuner
  @@rss_usage_on_load = Memtuner.rss_usage
  @@glibc_mallinfo_on_load = Memtuner.glibc_mallinfo
  @@glibc_malloc_info_on_load = Memtuner.glibc_malloc_info
  @@gc_stat_on_load = GC.stat

  def self.rss_usage_on_load
    @@rss_usage_on_load
  end

  def self.glibc_mallinfo_on_load
    @@glibc_mallinfo_on_load
  end

  def self.glibc_malloc_info_on_load
    @@glibc_malloc_info_on_load
  end

  def self.gc_stat_on_load
    @@gc_stat_on_load
  end
end

if defined?(Rails)
  require 'memtuner/engine'
end

# # rss usage
# plain ruby                                  :  8,568,832
# require after bundle/setup (config/boot.rb) : 19,202,048
# require before rails (config/application.rb): 28,327,936
# first in Gemfile                            : 39,596,032
# later in Gemfile                            : 69,185,536
#
# plain ruby
#  RUBYLIB=$PWD/lib:$RUBYLIB ruby -rmemtuner -e 'puts Memtuner.rss_usage_on_load[:current]'
