require "spec_helper"

describe Memtuner do
  it "has a version number" do
    expect(Memtuner::VERSION).not_to be nil
  end

  it "does something useful" do
    expect(Memtuner.malloc_info).not_to be_nil
  end
end
