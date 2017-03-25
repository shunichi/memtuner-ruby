require "spec_helper"

describe Memtuner do
  it "has a version number" do
    expect(Memtuner::VERSION).not_to be nil
  end

  describe '#glibc_mallinfo' do
    it 'returns non nil value' do
      expect(Memtuner.glibc_mallinfo).not_to be_nil
    end
    it 'has mallinfo member keys' do
      %i(arena ordblks smblks hblks hblkhd usmblks fsmblks uordblks fordblks keepcost).each do |sym|
        expect(Memtuner.glibc_mallinfo).to include sym
      end
    end
  end

  describe '#glibc_malloc_info' do
    it 'returns non nil value' do
      expect(Memtuner.glibc_malloc_info).not_to be_nil
    end
  end

  describe '#rss_usage' do
    it "returns hash" do
      expect(Memtuner.rss_usage).not_to be_nil
      expect(Memtuner.rss_usage).to include :current
      expect(Memtuner.rss_usage).to include :peak
    end
  end
end
