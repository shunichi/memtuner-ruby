module Memtuner
  class MemoryStatistics
    attr_reader :rss_usage, :glibc_mallinfo, :glibc_malloc_info, :gc_stat
  
    def initialize
      @rss_usage = Memtuner.rss_usage
      @glibc_mallinfo = Memtuner.glibc_mallinfo
      @glibc_malloc_info = Memtuner.glibc_malloc_info
      @gc_stat = GC.stat
    end
  
    def self.ruby_rvalue_size
      GC::INTERNAL_CONSTANTS[:RVALUE_SIZE]
    end
  
    %i(available_slots live_slots free_slots final_slots).each do |name|
      sym = "heap_#{name}".to_sym
      define_method "ruby_#{name}" do
        @gc_stat[sym]
      end
      define_method "ruby_#{name}_bytes" do
        @gc_stat[sym] * self.class.ruby_rvalue_size
      end
    end
  end
end
