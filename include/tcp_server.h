#pragma once

#include "order.h"
#include "market_data.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>

namespace UltraFastAnalysis {

// Message types for TCP communication
enum class MessageType : uint32_t {
    ORDER_SUBMIT = 1,
    ORDER_CANCEL = 2,
    ORDER_MODIFY = 3,
    MARKET_DATA = 4,
    ORDER_BOOK_REQUEST = 5,
    ORDER_STATUS_REQUEST = 6,
    HEARTBEAT = 7,
    LOGIN = 8,
    LOGOUT = 9
};

// Message header for all TCP messages
struct MessageHeader {
    uint32_t message_type;
    uint32_t message_length;
    uint64_t sequence_number;
    uint64_t timestamp;
    
    MessageHeader() : message_type(0), message_length(0), sequence_number(0), timestamp(0) {}
};

// Client connection class
class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    explicit ClientConnection(boost::asio::ip::tcp::socket socket);
    ~ClientConnection();
    
    void start();
    void stop();
    bool is_connected() const;
    
    // Message handling
    void send_order_confirmation(const Order& order);
    void send_trade_confirmation(const Order& order, uint64_t fill_quantity, double fill_price);
    void send_order_book_snapshot(const OrderBookSnapshot& snapshot);
    void send_market_data(const MarketData& data);
    
    // Getters
    uint64_t get_client_id() const { return client_id_; }
    const std::string& get_client_name() const { return client_name_; }
    
private:
    boost::asio::ip::tcp::socket socket_;
    std::atomic<bool> connected_{false};
    uint64_t client_id_;
    std::string client_name_;
    
    // Message buffers
    static constexpr size_t MAX_MESSAGE_SIZE = 8192;
    std::array<uint8_t, MAX_MESSAGE_SIZE> read_buffer_;
    std::array<uint8_t, MAX_MESSAGE_SIZE> write_buffer_;
    
    // Message parsing
    void handle_message(const MessageHeader& header, const uint8_t* data, size_t length);
    void handle_order_submit(const uint8_t* data, size_t length);
    void handle_order_cancel(const uint8_t* data, size_t length);
    void handle_order_modify(const uint8_t* data, size_t length);
    void handle_market_data_request(const uint8_t* data, size_t length);
    void handle_login(const uint8_t* data, size_t length);
    
    // Network I/O
    void start_read();
    void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
    void start_write();
    void handle_write(const boost::system::error_code& error, size_t bytes_transferred);
    
    // Message serialization
    template<typename T>
    void serialize_message(MessageType type, const T& data);
    
    // Callbacks
    std::function<void(std::shared_ptr<Order>)> order_submit_callback_;
    std::function<void(uint64_t, const std::string&)> order_cancel_callback_;
    std::function<void(uint64_t, const std::string&, uint64_t, double)> order_modify_callback_;
    
public:
    void set_order_submit_callback(std::function<void(std::shared_ptr<Order>)> callback) {
        order_submit_callback_ = callback;
    }
    
    void set_order_cancel_callback(std::function<void(uint64_t, const std::string&)> callback) {
        order_cancel_callback_ = callback;
    }
    
    void set_order_modify_callback(std::function<void(uint64_t, const std::string&, uint64_t, double)> callback) {
        order_modify_callback_ = callback;
    }
};

// Main TCP server class
class TCPServer {
public:
    explicit TCPServer(uint16_t port, size_t num_threads = 4);
    ~TCPServer();
    
    // Non-copyable, non-movable
    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;
    
    // Server lifecycle
    bool start();
    void stop();
    bool is_running() const;
    
    // Client management
    size_t get_client_count() const;
    std::vector<uint64_t> get_client_ids() const;
    
    // Broadcasting
    void broadcast_market_data(const MarketData& data);
    void broadcast_order_book_update(const OrderBookSnapshot& snapshot);
    
    // Callback setters
    void set_order_submit_callback(std::function<void(std::shared_ptr<Order>)> callback);
    void set_order_cancel_callback(std::function<void(uint64_t, const std::string&)> callback);
    void set_order_modify_callback(std::function<void(uint64_t, const std::string&, uint64_t, double)> callback);
    
private:
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};
    size_t num_threads_;
    
    // Client management
    std::unordered_map<uint64_t, std::shared_ptr<ClientConnection>> clients_;
    mutable std::shared_mutex clients_mutex_;
    std::atomic<uint64_t> next_client_id_{1};
    
    // Callbacks
    std::function<void(std::shared_ptr<Order>)> order_submit_callback_;
    std::function<void(uint64_t, const std::string&)> order_cancel_callback_;
    std::function<void(uint64_t, const std::string&, uint64_t, double)> order_modify_callback_;
    
    // Internal methods
    void start_accept();
    void handle_accept(const boost::system::error_code& error, boost::asio::ip::tcp::socket socket);
    void remove_client(uint64_t client_id);
    
    // Worker thread function
    void worker_thread_function();
};

} // namespace UltraFastAnalysis
