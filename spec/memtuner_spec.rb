require "spec_helper"

describe Memtuner do
  it "has a version number" do
    expect(Memtuner::VERSION).not_to be nil
  end

  describe '#mallinfo' do
    it 'returns non nil value' do
      expect(Memtuner.mallinfo).not_to be_nil
    end
    it 'has mallinfo member keys' do
      %i(arena ordblks smblks hblks hblkhd usmblks fsmblks uordblks fordblks keepcost).each do |sym|
        expect(Memtuner.mallinfo).to include sym
      end
    end
  end

  describe '#malloc_info' do
    it 'returns non nil value' do
      expect(Memtuner.malloc_info).not_to be_nil
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
