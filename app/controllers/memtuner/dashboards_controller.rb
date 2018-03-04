module Memtuner
  class DashboardsController < ApplicationController
    def show
      @memory_statistics = MemoryStatistics.new
    end
  end
end
