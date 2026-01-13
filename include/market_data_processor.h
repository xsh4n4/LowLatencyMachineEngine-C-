#pragma once

#include "market_data.h"
#include "ring_buffer.h"
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <chrono>

namespace UltraFastAnalysis {

// Market data source types
enum class DataSourceType : uint8_t {
    NASDAQ_ITCH = 0,
    CRYPTO_EXCHANGE = 1,
    SIMULATED = 2,
    CUSTOM_FEED = 3
};

// Market data processor configuration
struct MarketDataConfig {
    DataSourceType source_type = DataSourceType::SIMULATED;
    size_t num_processing_threads = 2;
    size_t batch_size = 1000;
    size_t max_queue_size = 1000000;
    bool enable_validation = true;
    bool enable_compression = false;
    std::chrono::milliseconds heartbeat_interval{1000};
    std::string data_source_url;
    uint16_t data_source_port;
    
    // Performance tuning
    size_t ring_buffer_size = 65536;
    size_t max_message_size = 8192;
    bool enable_batching = true;
    std::chrono::microseconds max_processing_latency{50}; // 50 microseconds
};

// Market data statistics
struct MarketDataStats {
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> messages_processed{0};
    std::atomic<uint64_t> messages_dropped{0};
    std::atomic<uint64_t> validation_errors{0};
    std::atomic<uint64_t> processing_errors{0};
    
    // Latency metrics
    std::atomic<uint64_t> total_latency_ns{0};
    std::atomic<uint64_t> max_latency_ns{0};
    std::atomic<uint64_t> min_latency_ns{UINT64_MAX};
    
    // Throughput metrics
    std::atomic<uint64_t> messages_per_second{0};
    std::atomic<uint64_t> bytes_per_second{0};
    
    void reset() {
        messages_received = 0;
        messages_processed = 0;
        messages_dropped = 0;
        validation_errors = 0;
        processing_errors = 0;
        total_latency_ns = 0;
        max_latency_ns = 0;
        min_latency_ns = UINT64_MAX;
        messages_per_second = 0;
        bytes_per_second = 0;
    }
    
    double get_average_latency_ns() const {
        uint64_t processed = messages_processed.load();
        return processed > 0 ? static_cast<double>(total_latency_ns.load()) / processed : 0.0;
    }
    
    double get_average_latency_microseconds() const {
        return get_average_latency_ns() / 1000.0;
    }
};

// Abstract market data source
class MarketDataSource {
public:
    virtual ~MarketDataSource() = default;
    
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    virtual bool start_streaming() = 0;
    virtual void stop_streaming() = 0;
    
    virtual void set_data_callback(std::function<void(const MarketData&)> callback) = 0;
    virtual void set_error_callback(std::function<void(const std::string&)> callback) = 0;
    
    virtual const MarketDataStats& get_stats() const = 0;
    virtual void reset_stats() = 0;
};

// Simulated market data source for testing
class SimulatedMarketDataSource : public MarketDataSource {
public:
    explicit SimulatedMarketDataSource(const MarketDataConfig& config);
    ~SimulatedMarketDataSource() override;
    
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    
    bool start_streaming() override;
    void stop_streaming() override;
    
    void set_data_callback(std::function<void(const MarketData&)> callback) override;
    void set_error_callback(std::function<void(const std::string&)> callback) override;
    
    const MarketDataStats& get_stats() const override;
    void reset_stats() override;
    
    // Configuration
    void set_symbols(const std::vector<std::string>& symbols);
    void set_tick_rate(size_t ticks_per_second);
    void set_volatility(double volatility);
    
private:
    MarketDataConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> streaming_{false};
    
    std::thread streaming_thread_;
    std::atomic<bool> shutdown_requested_{false};
    
    std::function<void(const MarketData&)> data_callback_;
    std::function<void(const std::string&)> error_callback_;
    
    std::vector<std::string> symbols_;
    size_t tick_rate_;
    double volatility_;
    
    MarketDataStats stats_;
    
    // Internal methods
    void streaming_thread_worker();
    MarketData generate_random_tick(const std::string& symbol);
    MarketData generate_random_trade(const std::string& symbol);
    MarketData generate_random_quote(const std::string& symbol);
    
    // Price simulation
    std::unordered_map<std::string, double> current_prices_;
    std::unordered_map<std::string, double> price_volatility_;
    
    double generate_price_change(const std::string& symbol);
};

// Main market data processor
class MarketDataProcessor {
public:
    explicit MarketDataProcessor(const MarketDataConfig& config = MarketDataConfig{});
    ~MarketDataProcessor();
    
    // Non-copyable, non-movable
    MarketDataProcessor(const MarketDataProcessor&) = delete;
    MarketDataProcessor& operator=(const MarketDataProcessor&) = delete;
    
    // Processor lifecycle
    bool start();
    void stop();
    bool is_running() const;
    
    // Data source management
    bool connect_data_source();
    void disconnect_data_source();
    bool is_data_source_connected() const;
    
    // Data processing
    bool submit_market_data(const MarketData& data);
    void set_data_callback(std::function<void(const MarketData&)> callback);
    void set_error_callback(std::function<void(const std::string&)> callback);
    
    // Configuration
    MarketDataConfig get_config() const;
    void update_config(const MarketDataConfig& config);
    
    // Statistics
    const MarketDataStats& get_stats() const;
    void reset_stats();
    
    // Performance monitoring
    size_t get_queue_size() const;
    double get_processing_latency_microseconds() const;
    
private:
    MarketDataConfig config_;
    std::atomic<bool> running_{false};
    
    // Data source
    std::unique_ptr<MarketDataSource> data_source_;
    
    // Ring buffer for incoming data
    std::unique_ptr<MarketDataRingBuffer<65536>> input_buffer_;
    
    // Processing threads
    std::vector<std::thread> processing_threads_;
    
    // Callbacks
    std::function<void(const MarketData&)> data_callback_;
    std::function<void(const std::string&)> error_callback_;
    
    // Statistics
    MarketDataStats stats_;
    
    // Internal methods
    void processing_thread_worker();
    void process_market_data_batch();
    
    bool validate_market_data(const MarketData& data);
    void update_statistics(const MarketData& data, uint64_t latency_ns);
    
    // Error handling
    void handle_processing_error(const std::string& error);
    void handle_validation_error(const MarketData& data, const std::string& error);
    
    // Thread synchronization
    std::atomic<bool> shutdown_requested_{false};
    std::condition_variable shutdown_cv_;
    std::mutex shutdown_mutex_;
};

} // namespace UltraFastAnalysis
