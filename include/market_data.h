#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

namespace UltraFastAnalysis {

enum class MarketDataType : uint8_t {
    TRADE = 0,
    QUOTE = 1,
    ORDER_BOOK_UPDATE = 2,
    TICK = 3
};

struct MarketData {
    uint64_t sequence_number;
    std::string symbol;
    MarketDataType type;
    std::chrono::high_resolution_clock::time_point timestamp;
    
    // Trade data
    double trade_price;
    uint64_t trade_quantity;
    uint64_t trade_id;
    
    // Quote data
    double bid_price;
    uint64_t bid_quantity;
    double ask_price;
    uint64_t ask_quantity;
    
    // Order book update
    double price;
    uint64_t quantity;
    bool is_bid;
    
    MarketData() : sequence_number(0), type(MarketDataType::TICK),
                   trade_price(0.0), trade_quantity(0), trade_id(0),
                   bid_price(0.0), bid_quantity(0), ask_price(0.0), ask_quantity(0),
                   price(0.0), quantity(0), is_bid(false) {
        symbol.reserve(16);
    }
    
    void reset() {
        sequence_number = 0;
        symbol.clear();
        type = MarketDataType::TICK;
        trade_price = 0.0;
        trade_quantity = 0;
        trade_id = 0;
        bid_price = 0.0;
        bid_quantity = 0;
        ask_price = 0.0;
        ask_quantity = 0;
        price = 0.0;
        quantity = 0;
        is_bid = false;
    }
};

// Market data message for TCP communication
struct MarketDataMessage {
    uint32_t message_type;
    uint32_t message_length;
    MarketData data;
    
    MarketDataMessage() : message_type(0), message_length(0) {}
};

// Level 2 order book snapshot
struct OrderBookSnapshot {
    std::string symbol;
    std::chrono::high_resolution_clock::time_point timestamp;
    std::vector<std::pair<double, uint64_t>> bids;  // price, quantity
    std::vector<std::pair<double, uint64_t>> asks;  // price, quantity
    
    OrderBookSnapshot() {
        symbol.reserve(16);
        bids.reserve(10);
        asks.reserve(10);
    }
    
    void clear() {
        symbol.clear();
        bids.clear();
        asks.clear();
    }
};

} // namespace UltraFastAnalysis
