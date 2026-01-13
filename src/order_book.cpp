#include "order_book.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace UltraFastAnalysis {

OrderBook::OrderBook(const std::string& symbol)
    : symbol_(symbol), total_orders_(0), total_trades_(0), total_volume_(0.0) {
    // Pre-reserve vectors for performance
    recent_trades_.reserve(MAX_TRADES_HISTORY);
}

bool OrderBook::add_order(std::shared_ptr<Order> order) {
    if (!order || order->symbol != symbol_) {
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    
    // Check if order already exists
    if (orders_by_id_.find(order->order_id) != orders_by_id_.end()) {
        return false;
    }
    
    // Store order by ID for fast lookup
    orders_by_id_[order->order_id] = order;
    
    // Add to appropriate price level
    if (order->side == OrderSide::BUY) {
        add_to_bid_level(order->price, order);
    } else {
        add_to_ask_level(order->price, order);
    }
    
    total_orders_++;
    
    // Process order based on type
    if (order->type == OrderType::MARKET) {
        process_market_order(order);
    } else {
        process_limit_order(order);
    }
    
    // Try to match orders
    match_orders();
    
    return true;
}

bool OrderBook::cancel_order(uint64_t order_id) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
        return false;
    }
    
    std::shared_ptr<Order> order = it->second;
    
    // Remove from price level
    if (order->side == OrderSide::BUY) {
        remove_from_bid_level(order->price, order_id);
    } else {
        remove_from_ask_level(order->price, order_id);
    }
    
    // Remove from ID lookup
    orders_by_id_.erase(it);
    
    // Update order status
    order->status = OrderStatus::CANCELLED;
    
    total_orders_--;
    
    cleanup_empty_levels();
    return true;
}

bool OrderBook::modify_order(uint64_t order_id, uint64_t new_quantity, double new_price) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
        return false;
    }
    
    std::shared_ptr<Order> order = it->second;
    
    // Remove from current price level
    if (order->side == OrderSide::BUY) {
        remove_from_bid_level(order->price, order_id);
    } else {
        remove_from_ask_level(order->price, order_id);
    }
    
    // Update order
    order->quantity = new_quantity;
    order->price = new_price;
    order->timestamp = std::chrono::high_resolution_clock::now();
    
    // Add to new price level
    if (order->side == OrderSide::BUY) {
        add_to_bid_level(order->price, order);
    } else {
        add_to_ask_level(order->price, order);
    }
    
    // Try to match orders
    match_orders();
    
    cleanup_empty_levels();
    return true;
}

double OrderBook::get_best_bid() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}

double OrderBook::get_best_ask() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

uint64_t OrderBook::get_best_bid_quantity() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    if (bids_.empty()) return 0;
    
    uint64_t total_quantity = 0;
    for (const auto& order : bids_.begin()->second) {
        total_quantity += order->remaining_quantity();
    }
    return total_quantity;
}

uint64_t OrderBook::get_best_ask_quantity() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    if (asks_.empty()) return 0;
    
    uint64_t total_quantity = 0;
    for (const auto& order : asks_.begin()->second) {
        total_quantity += order->remaining_quantity();
    }
    return total_quantity;
}

std::vector<std::pair<double, uint64_t>> OrderBook::get_bids(size_t levels) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    
    std::vector<std::pair<double, uint64_t>> result;
    result.reserve(std::min(levels, bids_.size()));
    
    size_t count = 0;
    for (const auto& [price, orders] : bids_) {
        if (count >= levels) break;
        
        uint64_t total_quantity = 0;
        for (const auto& order : orders) {
            total_quantity += order->remaining_quantity();
        }
        
        result.emplace_back(price, total_quantity);
        count++;
    }
    
    return result;
}

std::vector<std::pair<double, uint64_t>> OrderBook::get_asks(size_t levels) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    
    std::vector<std::pair<double, uint64_t>> result;
    result.reserve(std::min(levels, asks_.size()));
    
    size_t count = 0;
    for (const auto& [price, orders] : asks_) {
        if (count >= levels) break;
        
        uint64_t total_quantity = 0;
        for (const auto& order : orders) {
            total_quantity += order->remaining_quantity();
        }
        
        result.emplace_back(price, total_quantity);
        count++;
    }
    
    return result;
}

OrderBookSnapshot OrderBook::get_snapshot() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    
    OrderBookSnapshot snapshot;
    snapshot.symbol = symbol_;
    snapshot.timestamp = std::chrono::high_resolution_clock::now();
    
    // Get top 10 levels for each side
    snapshot.bids = get_bids(10);
    snapshot.asks = get_asks(10);
    
    return snapshot;
}

std::vector<MarketData> OrderBook::get_recent_trades(size_t count) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    
    size_t num_trades = std::min(count, recent_trades_.size());
    std::vector<MarketData> result;
    result.reserve(num_trades);
    
    // Return most recent trades first
    for (auto it = recent_trades_.rbegin(); 
         it != recent_trades_.rend() && result.size() < num_trades; ++it) {
        result.push_back(*it);
    }
    
    return result;
}

size_t OrderBook::get_order_count() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return total_orders_;
}

size_t OrderBook::get_trade_count() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return total_trades_;
}

double OrderBook::get_total_volume() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return total_volume_;
}

void OrderBook::lock_for_reading() const {
    rw_mutex_.lock_shared();
}

void OrderBook::unlock_for_reading() const {
    rw_mutex_.unlock_shared();
}

void OrderBook::lock_for_writing() {
    rw_mutex_.lock();
}

void OrderBook::unlock_for_writing() {
    rw_mutex_.unlock();
}

void OrderBook::process_market_order(std::shared_ptr<Order> order) {
    // Market orders are immediately matched against the opposite side
    // No need to store them in the order book
    match_orders();
}

void OrderBook::process_limit_order(std::shared_ptr<Order> order) {
    // Limit orders are already added to the price levels
    // Just try to match them
    match_orders();
}

void OrderBook::match_orders() {
    while (!bids_.empty() && !asks_.empty()) {
        double best_bid = bids_.begin()->first;
        double best_ask = asks_.begin()->first;
        
        // Check if orders can match
        if (best_bid < best_ask) {
            break; // No more matches possible
        }
        
        // Get the orders at best prices
        auto& best_bids = bids_.begin()->second;
        auto& best_asks = asks_.begin()->second;
        
        if (best_bids.empty() || best_asks.empty()) {
            break;
        }
        
        // Match orders (price-time priority)
        std::shared_ptr<Order> buy_order = best_bids.front();
        std::shared_ptr<Order> sell_order = best_asks.front();
        
        // Determine match price and quantity
        double match_price = (best_bid + best_ask) / 2.0; // Mid-price matching
        uint64_t match_quantity = std::min(buy_order->remaining_quantity(), 
                                         sell_order->remaining_quantity());
        
        // Execute the trade
        record_trade(buy_order.get(), sell_order.get(), match_price, match_quantity);
        
        // Update order quantities
        buy_order->filled_quantity += match_quantity;
        sell_order->filled_quantity += match_quantity;
        
        // Remove filled orders
        if (buy_order->is_filled()) {
            best_bids.erase(best_bids.begin());
            orders_by_id_.erase(buy_order->order_id);
            total_orders_--;
        }
        
        if (sell_order->is_filled()) {
            best_asks.erase(best_asks.begin());
            orders_by_id_.erase(sell_order->order_id);
            total_orders_--;
        }
        
        // Clean up empty price levels
        if (best_bids.empty()) {
            bids_.erase(bids_.begin());
        }
        if (best_asks.empty()) {
            asks_.erase(asks_.begin());
        }
    }
}

void OrderBook::record_trade(const Order* buy_order, const Order* sell_order, 
                            double price, uint64_t quantity) {
    MarketData trade;
    trade.type = MarketDataType::TRADE;
    trade.symbol = symbol_;
    trade.timestamp = std::chrono::high_resolution_clock::now();
    trade.trade_price = price;
    trade.trade_quantity = quantity;
    trade.trade_id = total_trades_ + 1;
    
    recent_trades_.push_back(trade);
    
    // Keep only recent trades
    if (recent_trades_.size() > MAX_TRADES_HISTORY) {
        recent_trades_.erase(recent_trades_.begin());
    }
    
    total_trades_++;
    total_volume_ += price * quantity;
}

void OrderBook::add_to_bid_level(double price, std::shared_ptr<Order> order) {
    auto& price_level = bids_[price];
    price_level.push_back(order);
    
    // Sort by timestamp for time priority
    std::sort(price_level.begin(), price_level.end(),
              [](const std::shared_ptr<Order>& a, const std::shared_ptr<Order>& b) {
                  return a->timestamp < b->timestamp;
              });
}

void OrderBook::remove_from_bid_level(double price, uint64_t order_id) {
    auto it = bids_.find(price);
    if (it == bids_.end()) return;
    
    auto& price_level = it->second;
    price_level.erase(
        std::remove_if(price_level.begin(), price_level.end(),
                      [order_id](const std::shared_ptr<Order>& order) {
                          return order->order_id == order_id;
                      }),
        price_level.end()
    );
}

void OrderBook::add_to_ask_level(double price, std::shared_ptr<Order> order) {
    auto& price_level = asks_[price];
    price_level.push_back(order);
    
    // Sort by timestamp for time priority
    std::sort(price_level.begin(), price_level.end(),
              [](const std::shared_ptr<Order>& a, const std::shared_ptr<Order>& b) {
                  return a->timestamp < b->timestamp;
              });
}

void OrderBook::remove_from_ask_level(double price, uint64_t order_id) {
    auto it = asks_.find(price);
    if (it == asks_.end()) return;
    
    auto& price_level = it->second;
    price_level.erase(
        std::remove_if(price_level.begin(), price_level.end(),
                      [order_id](const std::shared_ptr<Order>& order) {
                          return order->order_id == order_id;
                      }),
        price_level.end()
    );
}

void OrderBook::cleanup_empty_levels() {
    // Remove empty bid levels
    for (auto it = bids_.begin(); it != bids_.end();) {
        if (it->second.empty()) {
            it = bids_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Remove empty ask levels
    for (auto it = asks_.begin(); it != asks_.end();) {
        if (it->second.empty()) {
            it = asks_.erase(it);
        } else {
            ++it;
        }
    }
}

// OrderBookManager implementation
std::shared_ptr<OrderBook> OrderBookManager::get_or_create_order_book(const std::string& symbol) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second;
    }
    
    auto order_book = std::make_shared<OrderBook>(symbol);
    order_books_[symbol] = order_book;
    return order_book;
}

std::shared_ptr<OrderBook> OrderBookManager::get_order_book(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    
    auto it = order_books_.find(symbol);
    return (it != order_books_.end()) ? it->second : nullptr;
}

std::vector<std::string> OrderBookManager::get_symbols() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    
    std::vector<std::string> symbols;
    symbols.reserve(order_books_.size());
    
    for (const auto& [symbol, _] : order_books_) {
        symbols.push_back(symbol);
    }
    
    return symbols;
}

size_t OrderBookManager::get_order_book_count() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return order_books_.size();
}

void OrderBookManager::remove_order_book(const std::string& symbol) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    order_books_.erase(symbol);
}

} // namespace UltraFastAnalysis
