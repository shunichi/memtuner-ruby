module Memtuner
  class ApplicationController < ActionController::Base
    protect_from_forgery with: :exception

    if ENV["MEMTUNER_PASSWORD"]
      http_basic_authenticate_with name: ENV["MEMTUNER_USERNAME"], password: ENV["MEMTUNER_PASSWORD"]
    end
  end
end
