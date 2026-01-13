#pragma once

#include "order.h"
#include "market_data.h"
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace UltraFastAnalysis {

class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);
    ~OrderBook() = default;
    
    // Non-copyable, non-movable
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    
    // Order management
    bool add_order(std::shared_ptr<Order> order);
    bool cancel_order(uint64_t order_id);
    bool modify_order(uint64_t order_id, uint64_t new_quantity, double new_price);
    
    // Order book queries
    double get_best_bid() const;
    double get_best_ask() const;
    uint64_t get_best_bid_quantity() const;
    uint64_t get_best_ask_quantity() const;
    
    // Level 2 data
    std::vector<std::pair<double, uint64_t>> get_bids(size_t levels = 10) const;
    std::vector<std::pair<double, uint64_t>> get_asks(size_t levels = 10) const;
    
    // Market data generation
    OrderBookSnapshot get_snapshot() const;
    std::vector<MarketData> get_recent_trades(size_t count = 100) const;
    
    // Performance metrics
    size_t get_order_count() const;
    size_t get_trade_count() const;
    double get_total_volume() const;
    
    // Thread safety
    void lock_for_reading() const;
    void unlock_for_reading() const;
    void lock_for_writing();
    void unlock_for_writing();
    
private:
    std::string symbol_;
    
    // Order storage - using maps for price-time priority
    std::map<double, std::vector<std::shared_ptr<Order>>, std::greater<double>> bids_;
    std::map<double, std::vector<std::shared_ptr<Order>>, std::less<double>> asks_;
    
    // Fast order lookup by ID
    std::unordered_map<uint64_t, std::shared_ptr<Order>> orders_by_id_;
    
    // Trade history
    std::vector<MarketData> recent_trades_;
    
    // Statistics
    size_t total_orders_;
    size_t total_trades_;
    double total_volume_;
    
    // Thread safety
    mutable std::shared_mutex rw_mutex_;
    
    // Internal methods
    void process_market_order(std::shared_ptr<Order> order);
    void process_limit_order(std::shared_ptr<Order> order);
    void match_orders();
    void record_trade(const Order* buy_order, const Order* sell_order, 
                     double price, uint64_t quantity);
    
    // Price level management
    void add_to_bid_level(double price, std::shared_ptr<Order> order);
    void remove_from_bid_level(double price, uint64_t order_id);
    void add_to_ask_level(double price, std::shared_ptr<Order> order);
    void remove_from_ask_level(double price, uint64_t order_id);
    
    // Cleanup empty price levels
    void cleanup_empty_levels();
    
    // Constants
    static constexpr size_t MAX_TRADES_HISTORY = 1000;
    static constexpr size_t MAX_PRICE_LEVELS = 100;
};

// Order book manager for multiple symbols
class OrderBookManager {
public:
    OrderBookManager() = default;
    ~OrderBookManager() = default;
    
    // Non-copyable, non-movable
    OrderBookManager(const OrderBookManager&) = delete;
    OrderBookManager& operator=(const OrderBookManager&) = delete;
    
    std::shared_ptr<OrderBook> get_or_create_order_book(const std::string& symbol);
    std::shared_ptr<OrderBook> get_order_book(const std::string& symbol) const;
    
    std::vector<std::string> get_symbols() const;
    size_t get_order_book_count() const;
    
    void remove_order_book(const std::string& symbol);
    
private:
    mutable std::shared_mutex rw_mutex_;
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books_;
};

} // namespace UltraFastAnalysis
