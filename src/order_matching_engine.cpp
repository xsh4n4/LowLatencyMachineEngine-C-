#include "order_matching_engine.h"
#include "tcp_server.h"
#include "market_data_processor.h"
#include <iostream>
#include <chrono>
#include <algorithm>

namespace UltraFastAnalysis {

OrderMatchingEngine::OrderMatchingEngine(const EngineConfig& config)
    : config_(config), start_time_(std::chrono::high_resolution_clock::now()) {
    
    // Initialize ring buffers
    order_buffer_ = std::make_unique<OrderRingBuffer<65536>>();
    market_data_buffer_ = std::make_unique<MarketDataRingBuffer<65536>>();
    
    // Initialize core components
    order_book_manager_ = std::make_unique<OrderBookManager>();
    tcp_server_ = std::make_unique<TCPServer>(8080, config.num_matching_threads);
    market_data_processor_ = std::make_unique<MarketDataProcessor>();
    
    // Set up TCP server callbacks
    tcp_server_->set_order_submit_callback([this](std::shared_ptr<Order> order) {
        submit_order(order);
    });
    
    tcp_server_->set_order_cancel_callback([this](uint64_t order_id, const std::string& symbol) {
        cancel_order(order_id, symbol);
    });
    
    tcp_server_->set_order_modify_callback([this](uint64_t order_id, const std::string& symbol, 
                                                 uint64_t new_quantity, double new_price) {
        modify_order(order_id, symbol, new_quantity, new_price);
    });
    
    // Set up market data processor callback
    market_data_processor_->set_data_callback([this](const MarketData& data) {
        submit_market_data(data);
    });
}

OrderMatchingEngine::~OrderMatchingEngine() {
    stop();
}

bool OrderMatchingEngine::start() {
    if (running_.load()) {
        return true;
    }
    
    try {
        // Start TCP server
        if (!tcp_server_->start()) {
            std::cerr << "Failed to start TCP server" << std::endl;
            return false;
        }
        
        // Start market data processor
        if (!market_data_processor_->start()) {
            std::cerr << "Failed to start market data processor" << std::endl;
            tcp_server_->stop();
            return false;
        }
        
        // Start matching threads
        for (size_t i = 0; i < config_.num_matching_threads; ++i) {
            matching_threads_.emplace_back(&OrderMatchingEngine::matching_thread_worker, this);
        }
        
        // Start market data threads
        for (size_t i = 0; i < config_.num_market_data_threads; ++i) {
            market_data_threads_.emplace_back(&OrderMatchingEngine::market_data_thread_worker, this);
        }
        
        // Start metrics thread if enabled
        if (config_.enable_performance_monitoring) {
            metrics_thread_ = std::thread(&OrderMatchingEngine::metrics_thread_worker, this);
        }
        
        running_.store(true);
        start_time_ = std::chrono::high_resolution_clock::now();
        
        std::cout << "Order matching engine started successfully" << std::endl;
        std::cout << "Matching threads: " << config_.num_matching_threads << std::endl;
        std::cout << "Market data threads: " << config_.num_market_data_threads << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start order matching engine: " << e.what() << std::endl;
        return false;
    }
}

void OrderMatchingEngine::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "Stopping order matching engine..." << std::endl;
    
    // Signal shutdown
    shutdown_requested_.store(true);
    
    // Stop TCP server
    if (tcp_server_) {
        tcp_server_->stop();
    }
    
    // Stop market data processor
    if (market_data_processor_) {
        market_data_processor_->stop();
    }
    
    // Wait for threads to finish
    for (auto& thread : matching_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    matching_threads_.clear();
    
    for (auto& thread : market_data_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    market_data_threads_.clear();
    
    if (metrics_thread_.joinable()) {
        metrics_thread_.join();
    }
    
    running_.store(false);
    shutdown_requested_.store(false);
    
    std::cout << "Order matching engine stopped" << std::endl;
}

bool OrderMatchingEngine::is_running() const {
    return running_.load();
}

bool OrderMatchingEngine::submit_order(std::shared_ptr<Order> order) {
    if (!running_.load() || !order) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Try to add to ring buffer
    if (!order_buffer_->try_push(order)) {
        std::cerr << "Order buffer full, dropping order " << order->order_id << std::endl;
        return false;
    }
    
    // Calculate latency
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    update_performance_metrics(latency.count());
    
    return true;
}

bool OrderMatchingEngine::cancel_order(uint64_t order_id, const std::string& symbol) {
    if (!running_.load()) {
        return false;
    }
    
    auto order_book = order_book_manager_->get_order_book(symbol);
    if (!order_book) {
        return false;
    }
    
    return order_book->cancel_order(order_id);
}

bool OrderMatchingEngine::modify_order(uint64_t order_id, const std::string& symbol, 
                                      uint64_t new_quantity, double new_price) {
    if (!running_.load()) {
        return false;
    }
    
    auto order_book = order_book_manager_->get_order_book(symbol);
    if (!order_book) {
        return false;
    }
    
    return order_book->modify_order(order_id, new_quantity, new_price);
}

bool OrderMatchingEngine::submit_market_data(const MarketData& data) {
    if (!running_.load()) {
        return false;
    }
    
    // Try to add to ring buffer
    if (!market_data_buffer_->try_push(data)) {
        std::cerr << "Market data buffer full, dropping data" << std::endl;
        return false;
    }
    
    return true;
}

void OrderMatchingEngine::set_market_data_callback(std::function<void(const MarketData&)> callback) {
    market_data_callback_ = callback;
}

std::shared_ptr<OrderBook> OrderMatchingEngine::get_order_book(const std::string& symbol) const {
    return order_book_manager_->get_order_book(symbol);
}

OrderBookSnapshot OrderMatchingEngine::get_order_book_snapshot(const std::string& symbol) const {
    auto order_book = order_book_manager_->get_order_book(symbol);
    if (!order_book) {
        return OrderBookSnapshot{};
    }
    return order_book->get_snapshot();
}

const PerformanceMetrics& OrderMatchingEngine::get_performance_metrics() const {
    return metrics_;
}

void OrderMatchingEngine::reset_performance_metrics() {
    metrics_.reset();
}

EngineConfig OrderMatchingEngine::get_config() const {
    return config_;
}

void OrderMatchingEngine::update_config(const EngineConfig& config) {
    // Note: This is a simplified implementation
    // In a real system, you'd need to handle dynamic reconfiguration carefully
    config_ = config;
}

size_t OrderMatchingEngine::get_total_order_count() const {
    size_t total = 0;
    auto symbols = order_book_manager_->get_symbols();
    for (const auto& symbol : symbols) {
        auto order_book = order_book_manager_->get_order_book(symbol);
        if (order_book) {
            total += order_book->get_order_count();
        }
    }
    return total;
}

size_t OrderMatchingEngine::get_total_trade_count() const {
    size_t total = 0;
    auto symbols = order_book_manager_->get_symbols();
    for (const auto& symbol : symbols) {
        auto order_book = order_book_manager_->get_order_book(symbol);
        if (order_book) {
            total += order_book->get_trade_count();
        }
    }
    return total;
}

std::vector<std::string> OrderMatchingEngine::get_active_symbols() const {
    return order_book_manager_->get_symbols();
}

void OrderMatchingEngine::matching_thread_worker() {
    std::cout << "Matching thread started: " << std::this_thread::get_id() << std::endl;
    
    while (!shutdown_requested_.load()) {
        process_order_batch();
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    
    std::cout << "Matching thread stopped: " << std::this_thread::get_id() << std::endl;
}

void OrderMatchingEngine::market_data_thread_worker() {
    std::cout << "Market data thread started: " << std::this_thread::get_id() << std::endl;
    
    while (!shutdown_requested_.load()) {
        process_market_data_batch();
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    
    std::cout << "Market data thread stopped: " << std::this_thread::get_id() << std::endl;
}

void OrderMatchingEngine::metrics_thread_worker() {
    std::cout << "Metrics thread started: " << std::this_thread::get_id() << std::endl;
    
    auto last_update = std::chrono::high_resolution_clock::now();
    
    while (!shutdown_requested_.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update);
        
        if (elapsed.count() >= 1) {
            calculate_throughput_metrics();
            last_update = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Metrics thread stopped: " << std::this_thread::get_id() << std::endl;
}

void OrderMatchingEngine::process_order_batch() {
    std::vector<std::shared_ptr<Order>> orders;
    orders.reserve(100); // Process up to 100 orders at once
    
    // Collect orders from ring buffer
    while (orders.size() < 100) {
        std::shared_ptr<Order> order;
        if (!order_buffer_->try_pop(order)) {
            break;
        }
        orders.push_back(order);
    }
    
    if (orders.empty()) {
        return;
    }
    
    // Process orders
    for (auto& order : orders) {
        auto order_book = order_book_manager_->get_or_create_order_book(order->symbol);
        if (order_book->add_order(order)) {
            metrics_.orders_processed.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void OrderMatchingEngine::process_market_data_batch() {
    std::vector<MarketData> data_batch;
    data_batch.reserve(100); // Process up to 100 market data updates at once
    
    // Collect market data from ring buffer
    while (data_batch.size() < 100) {
        MarketData data;
        if (!market_data_buffer_->try_pop(data)) {
            break;
        }
        data_batch.push_back(data);
    }
    
    if (data_batch.empty()) {
        return;
    }
    
    // Process market data
    for (const auto& data : data_batch) {
        metrics_.market_data_updates.fetch_add(1, std::memory_order_relaxed);
        
        // Call market data callback if set
        if (market_data_callback_) {
            market_data_callback_(data);
        }
    }
}

void OrderMatchingEngine::update_performance_metrics(uint64_t latency_ns) {
    metrics_.total_latency_ns.fetch_add(latency_ns, std::memory_order_relaxed);
    
    // Update min/max latency
    uint64_t current_min = metrics_.min_latency_ns.load(std::memory_order_relaxed);
    while (latency_ns < current_min && 
           !metrics_.min_latency_ns.compare_exchange_weak(current_min, latency_ns, std::memory_order_relaxed)) {}
    
    uint64_t current_max = metrics_.max_latency_ns.load(std::memory_order_relaxed);
    while (latency_ns > current_max && 
           !metrics_.max_latency_ns.compare_exchange_weak(current_max, latency_ns, std::memory_order_relaxed)) {}
}

void OrderMatchingEngine::calculate_throughput_metrics() {
    // Calculate orders per second
    static uint64_t last_orders = 0;
    uint64_t current_orders = metrics_.orders_processed.load(std::memory_order_relaxed);
    metrics_.orders_per_second.store(current_orders - last_orders, std::memory_order_relaxed);
    last_orders = current_orders;
    
    // Calculate trades per second
    static uint64_t last_trades = 0;
    uint64_t current_trades = metrics_.trades_executed.load(std::memory_order_relaxed);
    metrics_.trades_per_second.store(current_trades - last_trades, std::memory_order_relaxed);
    last_trades = current_trades;
    
    // Calculate market data per second
    static uint64_t last_market_data = 0;
    uint64_t current_market_data = metrics_.market_data_updates.load(std::memory_order_relaxed);
    metrics_.market_data_per_second.store(current_market_data - last_market_data, std::memory_order_relaxed);
    last_market_data = current_market_data;
}

} // namespace UltraFastAnalysis
