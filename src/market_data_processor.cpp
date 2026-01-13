#include "market_data_processor.h"
#include <iostream>
#include <random>
#include <chrono>
#include <algorithm>

namespace UltraFastAnalysis {

// SimulatedMarketDataSource implementation
SimulatedMarketDataSource::SimulatedMarketDataSource(const MarketDataConfig& config)
    : config_(config), tick_rate_(1000), volatility_(0.01) {
    
    // Initialize default symbols
    symbols_ = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN"};
    
    // Initialize price simulation
    for (const auto& symbol : symbols_) {
        current_prices_[symbol] = 100.0 + (std::rand() % 900); // Random price between 100-1000
        price_volatility_[symbol] = volatility_;
    }
}

SimulatedMarketDataSource::~SimulatedMarketDataSource() {
    stop_streaming();
    if (streaming_thread_.joinable()) {
        streaming_thread_.join();
    }
}

bool SimulatedMarketDataSource::connect() {
    connected_.store(true);
    return true;
}

void SimulatedMarketDataSource::disconnect() {
    connected_.store(false);
}

bool SimulatedMarketDataSource::is_connected() const {
    return connected_.load();
}

bool SimulatedMarketDataSource::start_streaming() {
    if (!connected_.load() || streaming_.load()) {
        return false;
    }

    streaming_.store(true);
    shutdown_requested_.store(false);
    
    streaming_thread_ = std::thread(&SimulatedMarketDataSource::streaming_thread_worker, this);
    return true;
}

void SimulatedMarketDataSource::stop_streaming() {
    if (!streaming_.load()) {
        return;
    }

    shutdown_requested_.store(true);
    streaming_.store(false);
    
    if (streaming_thread_.joinable()) {
        streaming_thread_.join();
    }
}

void SimulatedMarketDataSource::set_data_callback(std::function<void(const MarketData&)> callback) {
    data_callback_ = callback;
}

void SimulatedMarketDataSource::set_error_callback(std::function<void(const std::string&)> callback) {
    error_callback_ = callback;
}

const MarketDataStats& SimulatedMarketDataSource::get_stats() const {
    return stats_;
}

void SimulatedMarketDataSource::reset_stats() {
    stats_.reset();
}

void SimulatedMarketDataSource::set_symbols(const std::vector<std::string>& symbols) {
    symbols_ = symbols;
    
    // Initialize prices for new symbols
    for (const auto& symbol : symbols) {
        if (current_prices_.find(symbol) == current_prices_.end()) {
            current_prices_[symbol] = 100.0 + (std::rand() % 900);
            price_volatility_[symbol] = volatility_;
        }
    }
}

void SimulatedMarketDataSource::set_tick_rate(size_t ticks_per_second) {
    tick_rate_ = ticks_per_second;
}

void SimulatedMarketDataSource::set_volatility(double volatility) {
    volatility_ = volatility;
    
    // Update volatility for all symbols
    for (auto& [symbol, vol] : price_volatility_) {
        vol = volatility;
    }
}

void SimulatedMarketDataSource::streaming_thread_worker() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    auto tick_interval = std::chrono::microseconds(1000000 / tick_rate_);
    auto last_tick = std::chrono::high_resolution_clock::now();

    while (!shutdown_requested_.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = now - last_tick;

        if (elapsed >= tick_interval) {
            // Generate random market data
            for (const auto& symbol : symbols_) {
                if (dis(gen) < 0.3) { // 30% chance of trade
                    auto trade = generate_random_trade(symbol);
                    if (data_callback_) {
                        data_callback_(trade);
                        stats_.messages_received.fetch_add(1);
                    }
                }
                
                if (dis(gen) < 0.7) { // 70% chance of quote update
                    auto quote = generate_random_quote(symbol);
                    if (data_callback_) {
                        data_callback_(quote);
                        stats_.messages_received.fetch_add(1);
                    }
                }
                
                if (dis(gen) < 0.5) { // 50% chance of tick
                    auto tick = generate_random_tick(symbol);
                    if (data_callback_) {
                        data_callback_(tick);
                        stats_.messages_received.fetch_add(1);
                    }
                }
            }
            
            last_tick = now;
        }
        
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // Small sleep to prevent busy waiting
    }
}

MarketData SimulatedMarketDataSource::generate_random_tick(const std::string& symbol) {
    MarketData tick;
    tick.type = MarketDataType::TICK;
    tick.symbol = symbol;
    tick.timestamp = std::chrono::high_resolution_clock::now();
    tick.sequence_number = stats_.messages_received.load() + 1;
    
    // Generate random price movement
    double price_change = generate_price_change(symbol);
    current_prices_[symbol] += price_change;
    
    tick.trade_price = current_prices_[symbol];
    tick.trade_quantity = 100 + (std::rand() % 1000); // Random quantity
    
    return tick;
}

MarketData SimulatedMarketDataSource::generate_random_trade(const std::string& symbol) {
    MarketData trade;
    trade.type = MarketDataType::TRADE;
    trade.symbol = symbol;
    trade.timestamp = std::chrono::high_resolution_clock::now();
    trade.sequence_number = stats_.messages_received.load() + 1;
    
    // Generate random price movement
    double price_change = generate_price_change(symbol);
    current_prices_[symbol] += price_change;
    
    trade.trade_price = current_prices_[symbol];
    trade.trade_quantity = 100 + (std::rand() % 10000); // Random quantity
    trade.trade_id = stats_.messages_received.load() + 1;
    
    return trade;
}

MarketData SimulatedMarketDataSource::generate_random_quote(const std::string& symbol) {
    MarketData quote;
    quote.type = MarketDataType::QUOTE;
    quote.symbol = symbol;
    quote.timestamp = std::chrono::high_resolution_clock::now();
    quote.sequence_number = stats_.messages_received.load() + 1;
    
    // Generate bid/ask spread around current price
    double spread = current_prices_[symbol] * 0.001; // 0.1% spread
    quote.bid_price = current_prices_[symbol] - spread / 2;
    quote.ask_price = current_prices_[symbol] + spread / 2;
    
    quote.bid_quantity = 1000 + (std::rand() % 10000);
    quote.ask_quantity = 1000 + (std::rand() % 10000);
    
    return quote;
}

double SimulatedMarketDataSource::generate_price_change(const std::string& symbol) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> normal(0.0, price_volatility_[symbol]);
    
    double change = normal(gen);
    double max_change = current_prices_[symbol] * 0.05; // Max 5% change
    
    // Clamp the change
    if (change > max_change) change = max_change;
    if (change < -max_change) change = -max_change;
    
    return change;
}

// MarketDataProcessor implementation
MarketDataProcessor::MarketDataProcessor(const MarketDataConfig& config)
    : config_(config) {
    
    // Initialize ring buffer
    input_buffer_ = std::make_unique<MarketDataRingBuffer<65536>>();
    
    // Create appropriate data source based on config
    if (config.source_type == DataSourceType::SIMULATED) {
        data_source_ = std::make_unique<SimulatedMarketDataSource>(config);
    }
    // Add other data source types here as needed
}

MarketDataProcessor::~MarketDataProcessor() {
    stop();
}

bool MarketDataProcessor::start() {
    if (running_.load()) {
        return true;
    }

    try {
        // Start data source
        if (data_source_ && !data_source_->start_streaming()) {
            std::cerr << "Failed to start data source" << std::endl;
            return false;
        }

        // Start processing threads
        for (size_t i = 0; i < config_.num_processing_threads; ++i) {
            processing_threads_.emplace_back(&MarketDataProcessor::processing_thread_worker, this);
        }

        running_.store(true);
        std::cout << "Market data processor started with " << config_.num_processing_threads << " threads" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to start market data processor: " << e.what() << std::endl;
        return false;
    }
}

void MarketDataProcessor::stop() {
    if (!running_.load()) {
        return;
    }

    std::cout << "Stopping market data processor..." << std::endl;

    shutdown_requested_.store(true);
    running_.store(false);

    // Stop data source
    if (data_source_) {
        data_source_->stop_streaming();
    }

    // Wait for processing threads
    for (auto& thread : processing_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    processing_threads_.clear();

    std::cout << "Market data processor stopped" << std::endl;
}

bool MarketDataProcessor::is_running() const {
    return running_.load();
}

bool MarketDataProcessor::connect_data_source() {
    if (!data_source_) {
        return false;
    }
    return data_source_->connect();
}

void MarketDataProcessor::disconnect_data_source() {
    if (data_source_) {
        data_source_->disconnect();
    }
}

bool MarketDataProcessor::is_data_source_connected() const {
    return data_source_ && data_source_->is_connected();
}

bool MarketDataProcessor::submit_market_data(const MarketData& data) {
    if (!running_.load()) {
        return false;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Validate data
    if (config_.enable_validation && !validate_market_data(data)) {
        stats_.validation_errors.fetch_add(1);
        if (error_callback_) {
            error_callback_("Market data validation failed");
        }
        return false;
    }

    // Try to add to ring buffer
    if (!input_buffer_->try_push(data)) {
        stats_.messages_dropped.fetch_add(1);
        if (error_callback_) {
            error_callback_("Input buffer full, dropping market data");
        }
        return false;
    }

    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    update_statistics(data, latency.count());

    return true;
}

void MarketDataProcessor::set_data_callback(std::function<void(const MarketData&)> callback) {
    data_callback_ = callback;
}

void MarketDataProcessor::set_error_callback(std::function<void(const std::string&)> callback) {
    error_callback_ = callback;
}

MarketDataConfig MarketDataProcessor::get_config() const {
    return config_;
}

void MarketDataProcessor::update_config(const MarketDataConfig& config) {
    config_ = config;
}

const MarketDataStats& MarketDataProcessor::get_stats() const {
    return stats_;
}

void MarketDataProcessor::reset_stats() {
    stats_.reset();
}

size_t MarketDataProcessor::get_queue_size() const {
    return input_buffer_->size();
}

double MarketDataProcessor::get_processing_latency_microseconds() const {
    return stats_.get_average_latency_microseconds();
}

void MarketDataProcessor::processing_thread_worker() {
    std::cout << "Market data processing thread started: " << std::this_thread::get_id() << std::endl;

    while (!shutdown_requested_.load()) {
        process_market_data_batch();
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    std::cout << "Market data processing thread stopped: " << std::this_thread::get_id() << std::endl;
}

void MarketDataProcessor::process_market_data_batch() {
    std::vector<MarketData> batch;
    batch.reserve(config_.batch_size);

    // Collect market data from ring buffer
    while (batch.size() < config_.batch_size) {
        MarketData data;
        if (!input_buffer_->try_pop(data)) {
            break;
        }
        batch.push_back(data);
    }

    if (batch.empty()) {
        return;
    }

    // Process batch
    for (const auto& data : batch) {
        try {
            stats_.messages_processed.fetch_add(1);

            // Call data callback if set
            if (data_callback_) {
                data_callback_(data);
            }

        } catch (const std::exception& e) {
            stats_.processing_errors.fetch_add(1);
            if (error_callback_) {
                error_callback_(std::string("Processing error: ") + e.what());
            }
        }
    }
}

bool MarketDataProcessor::validate_market_data(const MarketData& data) {
    // Basic validation
    if (data.symbol.empty()) {
        return false;
    }

    if (data.timestamp.time_since_epoch().count() == 0) {
        return false;
    }

    // Type-specific validation
    switch (data.type) {
        case MarketDataType::TRADE:
            if (data.trade_price <= 0.0 || data.trade_quantity == 0) {
                return false;
            }
            break;
        case MarketDataType::QUOTE:
            if (data.bid_price <= 0.0 || data.ask_price <= 0.0 || 
                data.bid_price >= data.ask_price) {
                return false;
            }
            break;
        case MarketDataType::ORDER_BOOK_UPDATE:
            if (data.price <= 0.0) {
                return false;
            }
            break;
        default:
            break;
    }

    return true;
}

void MarketDataProcessor::update_statistics(const MarketData& data, uint64_t latency_ns) {
    stats_.total_latency_ns.fetch_add(latency_ns);
    
    // Update min/max latency
    uint64_t current_min = stats_.min_latency_ns.load();
    while (latency_ns < current_min &&
           !stats_.min_latency_ns.compare_exchange_weak(current_min, latency_ns)) {}

    uint64_t current_max = stats_.max_latency_ns.load();
    while (latency_ns > current_max &&
           !stats_.max_latency_ns.compare_exchange_weak(current_max, latency_ns)) {}
}

void MarketDataProcessor::handle_processing_error(const std::string& error) {
    stats_.processing_errors.fetch_add(1);
    if (error_callback_) {
        error_callback_(error);
    }
}

void MarketDataProcessor::handle_validation_error(const MarketData& data, const std::string& error) {
    stats_.validation_errors.fetch_add(1);
    if (error_callback_) {
        error_callback_(error);
    }
}

} // namespace UltraFastAnalysis
