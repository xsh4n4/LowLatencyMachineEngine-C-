#pragma once

#include "order_book.h"
#include "ring_buffer.h"
#include "market_data.h"
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace UltraFastAnalysis {

// Forward declarations
class TCPServer;
class MarketDataProcessor;

// Configuration for the matching engine
struct EngineConfig {
    size_t num_matching_threads = 4;
    size_t num_market_data_threads = 2;
    size_t ring_buffer_size = 65536;  // Must be power of 2
    size_t max_orders_per_symbol = 100000;
    size_t max_market_data_queue_size = 1000000;
    bool enable_performance_monitoring = true;
    std::chrono::microseconds max_latency_threshold{100}; // 100 microseconds
    uint16_t tcp_port = 8080;
    bool verbose_logging = false;
    bool simulation_mode = false;
};

// Performance metrics
struct PerformanceMetrics {
    std::atomic<uint64_t> orders_processed{0};
    std::atomic<uint64_t> trades_executed{0};
    std::atomic<uint64_t> market_data_updates{0};
    std::atomic<uint64_t> total_latency_ns{0};
    std::atomic<uint64_t> max_latency_ns{0};
    std::atomic<uint64_t> min_latency_ns{UINT64_MAX};
    
    // Throughput metrics
    std::atomic<uint64_t> orders_per_second{0};
    std::atomic<uint64_t> trades_per_second{0};
    std::atomic<uint64_t> market_data_per_second{0};
    
    void reset() {
        orders_processed = 0;
        trades_executed = 0;
        market_data_updates = 0;
        total_latency_ns = 0;
        max_latency_ns = 0;
        min_latency_ns = UINT64_MAX;
        orders_per_second = 0;
        trades_per_second = 0;
        market_data_per_second = 0;
    }
    
    double get_average_latency_ns() const {
        uint64_t processed = orders_processed.load();
        return processed > 0 ? static_cast<double>(total_latency_ns.load()) / processed : 0.0;
    }
    
    double get_average_latency_microseconds() const {
        return get_average_latency_ns() / 1000.0;
    }
};

class OrderMatchingEngine {
public:
    explicit OrderMatchingEngine(const EngineConfig& config = EngineConfig{});
    ~OrderMatchingEngine();
    
    // Non-copyable, non-movable
    OrderMatchingEngine(const OrderMatchingEngine&) = delete;
    OrderMatchingEngine& operator=(const OrderMatchingEngine&) = delete;
    
    // Engine lifecycle
    bool start();
    void stop();
    bool is_running() const;
    
    // Order management
    bool submit_order(std::shared_ptr<Order> order);
    bool cancel_order(uint64_t order_id, const std::string& symbol);
    bool modify_order(uint64_t order_id, const std::string& symbol, 
                     uint64_t new_quantity, double new_price);
    
    // Market data
    bool submit_market_data(const MarketData& data);
    void set_market_data_callback(std::function<void(const MarketData&)> callback);
    
    // Order book access
    std::shared_ptr<OrderBook> get_order_book(const std::string& symbol) const;
    OrderBookSnapshot get_order_book_snapshot(const std::string& symbol) const;
    
    // Performance monitoring
    const PerformanceMetrics& get_performance_metrics() const;
    void reset_performance_metrics();
    
    // Configuration
    EngineConfig get_config() const;
    void update_config(const EngineConfig& config);
    
    // Statistics
    size_t get_total_order_count() const;
    size_t get_total_trade_count() const;
    std::vector<std::string> get_active_symbols() const;
    
private:
    EngineConfig config_;
    std::atomic<bool> running_{false};
    
    // Core components
    std::unique_ptr<OrderBookManager> order_book_manager_;
    std::unique_ptr<TCPServer> tcp_server_;
    std::unique_ptr<MarketDataProcessor> market_data_processor_;
    
    // Ring buffers for ultra-low-latency communication
    std::unique_ptr<OrderRingBuffer<65536>> order_buffer_;
    std::unique_ptr<MarketDataRingBuffer<65536>> market_data_buffer_;
    
    // Threads
    std::vector<std::thread> matching_threads_;
    std::vector<std::thread> market_data_threads_;
    
    // Performance monitoring
    PerformanceMetrics metrics_;
    std::thread metrics_thread_;
    std::chrono::high_resolution_clock::time_point start_time_;
    
    // Callbacks
    std::function<void(const MarketData&)> market_data_callback_;
    
    // Internal methods
    void matching_thread_worker();
    void market_data_thread_worker();
    void metrics_thread_worker();
    
    void process_order_batch();
    void process_market_data_batch();
    
    void update_performance_metrics(uint64_t latency_ns);
    void calculate_throughput_metrics();
    
    // Thread synchronization
    std::atomic<bool> shutdown_requested_{false};
    std::condition_variable shutdown_cv_;
    std::mutex shutdown_mutex_;
};

// Strategy interface for backtesting
class TradingStrategy {
public:
    virtual ~TradingStrategy() = default;
    
    virtual void on_order_book_update(const OrderBookSnapshot& snapshot) = 0;
    virtual void on_trade(const MarketData& trade) = 0;
    virtual void on_order_fill(const Order& order, uint64_t fill_quantity, double fill_price) = 0;
    virtual void on_order_cancelled(const Order& order) = 0;
    
    virtual void initialize() = 0;
    virtual void shutdown() = 0;
    
    // Performance tracking
    virtual double get_pnl() const = 0;
    virtual double get_sharpe_ratio() const = 0;
    virtual size_t get_total_trades() const = 0;
};

} // namespace UltraFastAnalysis
