#pragma once

#include <atomic>
#include <cstdint>
#include <array>
#include <memory>
#include "market_data.h"
#include "order.h"

namespace UltraFastAnalysis {

template<typename T, size_t Size>
class LockFreeRingBuffer {
    static_assert(Size > 0 && ((Size & (Size - 1)) == 0), "Size must be a power of 2");
    
private:
    static constexpr size_t MASK = Size - 1;
    std::array<T, Size> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    
public:
    LockFreeRingBuffer() = default;
    
    // Non-copyable, non-movable
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;
    
    bool try_push(const T& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) & MASK;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer is full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool try_pop(T& item) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer is empty
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) & MASK, std::memory_order_release);
        return true;
    }
    
    bool empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    bool full() const {
        size_t next_tail = (tail_.load(std::memory_order_acquire) + 1) & MASK;
        return next_tail == head_.load(std::memory_order_acquire);
    }
    
    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        
        if (tail >= head) {
            return tail - head;
        } else {
            return Size - head + tail;
        }
    }
    
    size_t capacity() const { return Size; }
    
    void clear() {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }
};

// Specialized ring buffer for market data with pre-allocated memory
template<size_t Size>
class MarketDataRingBuffer : public LockFreeRingBuffer<MarketData, Size> {
private:
    std::array<MarketData, Size> data_pool_;
    
public:
    MarketDataRingBuffer() {
        // Pre-allocate all MarketData objects
        for (auto& data : data_pool_) {
            data.symbol.reserve(16);
        }
    }
    
    MarketData* get_pool_item(size_t index) {
        return &data_pool_[index & (Size - 1)];
    }
    
    void reset_pool_item(size_t index) {
        data_pool_[index & (Size - 1)].reset();
    }
};

// Specialized ring buffer for orders
template<size_t Size>
class OrderRingBuffer : public LockFreeRingBuffer<std::shared_ptr<Order>, Size> {
public:
    OrderRingBuffer() = default;
};

} // namespace UltraFastAnalysis
