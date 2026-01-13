#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <memory>

namespace UltraFastAnalysis {

enum class OrderSide : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    MARKET = 0,
    LIMIT = 1,
    STOP = 2,
    STOP_LIMIT = 3
};

enum class OrderStatus : uint8_t {
    PENDING = 0,
    PARTIALLY_FILLED = 1,
    FILLED = 2,
    CANCELLED = 3,
    REJECTED = 4
};

struct Order {
    uint64_t order_id;
    uint64_t client_id;
    std::string symbol;
    OrderSide side;
    OrderType type;
    uint64_t quantity;
    uint64_t filled_quantity;
    double price;
    double stop_price;
    std::chrono::high_resolution_clock::time_point timestamp;
    OrderStatus status;
    
    // Performance optimization: pre-allocated memory
    static constexpr size_t MAX_SYMBOL_LENGTH = 16;
    
    Order() : order_id(0), client_id(0), side(OrderSide::BUY), 
               type(OrderType::LIMIT), quantity(0), filled_quantity(0), 
               price(0.0), stop_price(0.0), status(OrderStatus::PENDING) {
        symbol.reserve(MAX_SYMBOL_LENGTH);
    }
    
    Order(uint64_t id, uint64_t client, const std::string& sym, 
          OrderSide s, OrderType t, uint64_t qty, double prc)
        : order_id(id), client_id(client), symbol(sym), side(s), type(t),
          quantity(qty), filled_quantity(0), price(prc), stop_price(0.0),
          timestamp(std::chrono::high_resolution_clock::now()),
          status(OrderStatus::PENDING) {}
    
    bool is_filled() const { return filled_quantity >= quantity; }
    bool is_partially_filled() const { return filled_quantity > 0 && filled_quantity < quantity; }
    uint64_t remaining_quantity() const { return quantity - filled_quantity; }
    
    // Memory pool optimization
    void reset() {
        order_id = 0;
        client_id = 0;
        symbol.clear();
        side = OrderSide::BUY;
        type = OrderType::LIMIT;
        quantity = 0;
        filled_quantity = 0;
        price = 0.0;
        stop_price = 0.0;
        status = OrderStatus::PENDING;
    }
};

// Order comparison for priority queue (price-time priority)
struct OrderCompare {
    bool operator()(const Order* lhs, const Order* rhs) const {
        if (lhs->side == OrderSide::BUY) {
            // For buy orders: higher price first, then earlier timestamp
            if (lhs->price != rhs->price) {
                return lhs->price < rhs->price;
            }
            return lhs->timestamp > rhs->timestamp;
        } else {
            // For sell orders: lower price first, then earlier timestamp
            if (lhs->price != rhs->price) {
                return lhs->price > rhs->price;
            }
            return lhs->timestamp > rhs->timestamp;
        }
    }
};

} // namespace UltraFastAnalysis
