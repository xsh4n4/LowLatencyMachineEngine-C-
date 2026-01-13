#include "tcp_server.h"
#include <iostream>
#include <cstring>
#include <sstream>

namespace UltraFastAnalysis {

// ClientConnection implementation
ClientConnection::ClientConnection(boost::asio::ip::tcp::socket socket)
    : socket_(std::move(socket)), client_id_(0), client_name_("Unknown") {
}

ClientConnection::~ClientConnection() {
    stop();
}

void ClientConnection::start() {
    if (connected_.load()) {
        return;
    }
    
    connected_.store(true);
    start_read();
}

void ClientConnection::stop() {
    if (!connected_.load()) {
        return;
    }
    
    connected_.store(false);
    
    boost::system::error_code ec;
    socket_.close(ec);
}

bool ClientConnection::is_connected() const {
    return connected_.load();
}

void ClientConnection::send_order_confirmation(const Order& order) {
    // Create confirmation message
    std::stringstream ss;
    ss << "ORDER_CONFIRMED:" << order.order_id << ":" << order.symbol << ":" 
       << (order.side == OrderSide::BUY ? "BUY" : "SELL") << ":" 
       << order.quantity << ":" << order.price;
    
    std::string message = ss.str();
    serialize_message(MessageType::ORDER_SUBMIT, message);
}

void ClientConnection::send_trade_confirmation(const Order& order, uint64_t fill_quantity, double fill_price) {
    // Create trade confirmation message
    std::stringstream ss;
    ss << "TRADE_EXECUTED:" << order.order_id << ":" << order.symbol << ":" 
       << (order.side == OrderSide::BUY ? "BUY" : "SELL") << ":" 
       << fill_quantity << ":" << fill_price;
    
    std::string message = ss.str();
    serialize_message(MessageType::ORDER_SUBMIT, message);
}

void ClientConnection::send_order_book_snapshot(const OrderBookSnapshot& snapshot) {
    // Create order book snapshot message
    std::stringstream ss;
    ss << "ORDER_BOOK:" << snapshot.symbol << ":";
    
    // Add bids
    ss << "BIDS:";
    for (const auto& [price, quantity] : snapshot.bids) {
        ss << price << "," << quantity << ";";
    }
    
    // Add asks
    ss << "ASKS:";
    for (const auto& [price, quantity] : snapshot.asks) {
        ss << price << "," << quantity << ";";
    }
    
    std::string message = ss.str();
    serialize_message(MessageType::ORDER_BOOK_REQUEST, message);
}

void ClientConnection::send_market_data(const MarketData& data) {
    // Create market data message
    std::stringstream ss;
    ss << "MARKET_DATA:" << data.symbol << ":" << static_cast<int>(data.type) << ":";
    
    switch (data.type) {
        case MarketDataType::TRADE:
            ss << data.trade_price << ":" << data.trade_quantity << ":" << data.trade_id;
            break;
        case MarketDataType::QUOTE:
            ss << data.bid_price << ":" << data.bid_quantity << ":" 
               << data.ask_price << ":" << data.ask_quantity;
            break;
        case MarketDataType::ORDER_BOOK_UPDATE:
            ss << data.price << ":" << data.quantity << ":" << (data.is_bid ? "BID" : "ASK");
            break;
        default:
            ss << "UNKNOWN";
            break;
    }
    
    std::string message = ss.str();
    serialize_message(MessageType::MARKET_DATA, message);
}

void ClientConnection::start_read() {
    if (!connected_.load()) {
        return;
    }
    
    // Read message header first
    boost::asio::async_read(socket_,
        boost::asio::buffer(&read_buffer_[0], sizeof(MessageHeader)),
        [this](const boost::system::error_code& error, size_t bytes_transferred) {
            if (!error) {
                handle_read(error, bytes_transferred);
            } else {
                // Handle error
                stop();
            }
        });
}

void ClientConnection::handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
    if (error || !connected_.load()) {
        stop();
        return;
    }
    
    if (bytes_transferred == sizeof(MessageHeader)) {
        // Parse header
        MessageHeader header;
        std::memcpy(&header, &read_buffer_[0], sizeof(MessageHeader));
        
        // Validate message size
        if (header.message_length > MAX_MESSAGE_SIZE - sizeof(MessageHeader)) {
            std::cerr << "Message too large: " << header.message_length << std::endl;
            stop();
            return;
        }
        
        // Read message body if present
        if (header.message_length > 0) {
            boost::asio::async_read(socket_,
                boost::asio::buffer(&read_buffer_[sizeof(MessageHeader)], header.message_length),
                [this, header](const boost::system::error_code& error, size_t bytes_transferred) {
                    if (!error) {
                        handle_message(header, &read_buffer_[sizeof(MessageHeader)], bytes_transferred);
                        start_read(); // Continue reading
                    } else {
                        stop();
                    }
                });
        } else {
            handle_message(header, nullptr, 0);
            start_read(); // Continue reading
        }
    } else {
        start_read(); // Continue reading
    }
}

void ClientConnection::handle_message(const MessageHeader& header, const uint8_t* data, size_t length) {
    switch (static_cast<MessageType>(header.message_type)) {
        case MessageType::ORDER_SUBMIT:
            handle_order_submit(data, length);
            break;
        case MessageType::ORDER_CANCEL:
            handle_order_cancel(data, length);
            break;
        case MessageType::ORDER_MODIFY:
            handle_order_modify(data, length);
            break;
        case MessageType::MARKET_DATA:
            handle_market_data_request(data, length);
            break;
        case MessageType::LOGIN:
            handle_login(data, length);
            break;
        case MessageType::HEARTBEAT:
            // Send heartbeat response
            break;
        default:
            std::cerr << "Unknown message type: " << header.message_type << std::endl;
            break;
    }
}

void ClientConnection::handle_order_submit(const uint8_t* data, size_t length) {
    if (!data || length == 0) return;
    
    std::string message(reinterpret_cast<const char*>(data), length);
    std::istringstream ss(message);
    std::string token;
    
    // Parse order message: SYMBOL:SIDE:QUANTITY:PRICE:TYPE
    std::vector<std::string> tokens;
    while (std::getline(ss, token, ':')) {
        tokens.push_back(token);
    }
    
    if (tokens.size() < 5) {
        std::cerr << "Invalid order message format" << std::endl;
        return;
    }
    
    try {
        auto order = std::make_shared<Order>();
        order->symbol = tokens[0];
        order->side = (tokens[1] == "BUY") ? OrderSide::BUY : OrderSide::SELL;
        order->quantity = std::stoull(tokens[2]);
        order->price = std::stod(tokens[3]);
        order->type = static_cast<OrderType>(std::stoi(tokens[4]));
        order->order_id = ++client_id_; // Simple ID generation
        order->client_id = client_id_;
        order->timestamp = std::chrono::high_resolution_clock::now();
        
        if (order_submit_callback_) {
            order_submit_callback_(order);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing order: " << e.what() << std::endl;
    }
}

void ClientConnection::handle_order_cancel(const uint8_t* data, size_t length) {
    if (!data || length == 0) return;
    
    std::string message(reinterpret_cast<const char*>(data), length);
    std::istringstream ss(message);
    std::string token;
    
    // Parse cancel message: ORDER_ID:SYMBOL
    std::vector<std::string> tokens;
    while (std::getline(ss, token, ':')) {
        tokens.push_back(token);
    }
    
    if (tokens.size() < 2) {
        std::cerr << "Invalid cancel message format" << std::endl;
        return;
    }
    
    try {
        uint64_t order_id = std::stoull(tokens[0]);
        std::string symbol = tokens[1];
        
        if (order_cancel_callback_) {
            order_cancel_callback_(order_id, symbol);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing cancel: " << e.what() << std::endl;
    }
}

void ClientConnection::handle_order_modify(const uint8_t* data, size_t length) {
    if (!data || length == 0) return;
    
    std::string message(reinterpret_cast<const char*>(data), length);
    std::istringstream ss(message);
    std::string token;
    
    // Parse modify message: ORDER_ID:SYMBOL:NEW_QUANTITY:NEW_PRICE
    std::vector<std::string> tokens;
    while (std::getline(ss, token, ':')) {
        tokens.push_back(token);
    }
    
    if (tokens.size() < 4) {
        std::cerr << "Invalid modify message format" << std::endl;
        return;
    }
    
    try {
        uint64_t order_id = std::stoull(tokens[0]);
        std::string symbol = tokens[1];
        uint64_t new_quantity = std::stoull(tokens[2]);
        double new_price = std::stod(tokens[3]);
        
        if (order_modify_callback_) {
            order_modify_callback_(order_id, symbol, new_quantity, new_price);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing modify: " << e.what() << std::endl;
    }
}

void ClientConnection::handle_market_data_request(const uint8_t* data, size_t length) {
    // Handle market data subscription request
    // This could be used to control which symbols the client receives
}

void ClientConnection::handle_login(const uint8_t* data, size_t length) {
    if (!data || length == 0) return;
    
    std::string message(reinterpret_cast<const char*>(data), length);
    std::istringstream ss(message);
    std::string token;
    
    // Parse login message: CLIENT_NAME
    std::vector<std::string> tokens;
    while (std::getline(ss, token, ':')) {
        tokens.push_back(token);
    }
    
    if (!tokens.empty()) {
        client_name_ = tokens[0];
        std::cout << "Client connected: " << client_name_ << " (ID: " << client_id_ << ")" << std::endl;
    }
}

void ClientConnection::start_write() {
    // This would be implemented for async writes
    // For simplicity, we're using synchronous writes in this implementation
}

void ClientConnection::handle_write(const boost::system::error_code& error, size_t bytes_transferred) {
    if (error) {
        stop();
    }
}

template<typename T>
void ClientConnection::serialize_message(MessageType type, const T& data) {
    MessageHeader header;
    header.message_type = static_cast<uint32_t>(type);
    header.message_length = sizeof(T);
    header.sequence_number = 0; // Could implement sequence numbering
    header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    // For string data, we need to handle it specially
    if constexpr (std::is_same_v<T, std::string>) {
        header.message_length = data.length();
        
        // Copy header
        std::memcpy(&write_buffer_[0], &header, sizeof(MessageHeader));
        
        // Copy data
        std::memcpy(&write_buffer_[sizeof(MessageHeader)], data.data(), data.length());
        
        // Send synchronously for simplicity
        boost::system::error_code ec;
        boost::asio::write(socket_, 
            boost::asio::buffer(&write_buffer_[0], sizeof(MessageHeader) + data.length()), ec);
        
        if (ec) {
            std::cerr << "Write error: " << ec.message() << std::endl;
            stop();
        }
    }
}

// TCPServer implementation
TCPServer::TCPServer(uint16_t port, size_t num_threads)
    : acceptor_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      num_threads_(num_threads) {
}

TCPServer::~TCPServer() {
    stop();
}

bool TCPServer::start() {
    if (running_.load()) {
        return true;
    }
    
    try {
        start_accept();
        
        // Start worker threads
        for (size_t i = 0; i < num_threads_; ++i) {
            worker_threads_.emplace_back(&TCPServer::worker_thread_function, this);
        }
        
        running_.store(true);
        std::cout << "TCP server started on port " << acceptor_.local_endpoint().port() << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start TCP server: " << e.what() << std::endl;
        return false;
    }
}

void TCPServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "Stopping TCP server..." << std::endl;
    
    running_.store(false);
    
    // Close acceptor
    boost::system::error_code ec;
    acceptor_.close(ec);
    
    // Close all client connections
    {
        std::unique_lock<std::shared_mutex> lock(clients_mutex_);
        for (auto& [id, client] : clients_) {
            client->stop();
        }
        clients_.clear();
    }
    
    // Stop io_context
    io_context_.stop();
    
    // Wait for worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    std::cout << "TCP server stopped" << std::endl;
}

bool TCPServer::is_running() const {
    return running_.load();
}

size_t TCPServer::get_client_count() const {
    std::shared_lock<std::shared_mutex> lock(clients_mutex_);
    return clients_.size();
}

std::vector<uint64_t> TCPServer::get_client_ids() const {
    std::shared_lock<std::shared_mutex> lock(clients_mutex_);
    
    std::vector<uint64_t> ids;
    ids.reserve(clients_.size());
    
    for (const auto& [id, _] : clients_) {
        ids.push_back(id);
    }
    
    return ids;
}

void TCPServer::broadcast_market_data(const MarketData& data) {
    std::shared_lock<std::shared_mutex> lock(clients_mutex_);
    
    for (auto& [id, client] : clients_) {
        if (client->is_connected()) {
            client->send_market_data(data);
        }
    }
}

void TCPServer::broadcast_order_book_update(const OrderBookSnapshot& snapshot) {
    std::shared_lock<std::shared_mutex> lock(clients_mutex_);
    
    for (auto& [id, client] : clients_) {
        if (client->is_connected()) {
            client->send_order_book_snapshot(snapshot);
        }
    }
}

void TCPServer::set_order_submit_callback(std::function<void(std::shared_ptr<Order>)> callback) {
    order_submit_callback_ = callback;
}

void TCPServer::set_order_cancel_callback(std::function<void(uint64_t, const std::string&)> callback) {
    order_cancel_callback_ = callback;
}

void TCPServer::set_order_modify_callback(std::function<void(uint64_t, const std::string&, uint64_t, double)> callback) {
    order_modify_callback_ = callback;
}

void TCPServer::start_accept() {
    acceptor_.async_accept(
        [this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
            if (!error) {
                handle_accept(error, std::move(socket));
            } else {
                std::cerr << "Accept error: " << error.message() << std::endl;
            }
            
            if (running_.load()) {
                start_accept();
            }
        });
}

void TCPServer::handle_accept(const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
    if (error) {
        return;
    }
    
    // Create new client connection
    auto client = std::make_shared<ClientConnection>(std::move(socket));
    
    // Set callbacks
    client->set_order_submit_callback(order_submit_callback_);
    client->set_order_cancel_callback(order_cancel_callback_);
    client->set_order_modify_callback(order_modify_callback_);
    
    // Assign client ID
    uint64_t client_id = next_client_id_++;
    
    // Store client
    {
        std::unique_lock<std::shared_mutex> lock(clients_mutex_);
        clients_[client_id] = client;
    }
    
    // Start client
    client->start();
    
    std::cout << "New client connected, total clients: " << get_client_count() << std::endl;
}

void TCPServer::remove_client(uint64_t client_id) {
    std::unique_lock<std::shared_mutex> lock(clients_mutex_);
    clients_.erase(client_id);
    std::cout << "Client disconnected, total clients: " << get_client_count() << std::endl;
}

void TCPServer::worker_thread_function() {
    try {
        io_context_.run();
    } catch (const std::exception& e) {
        std::cerr << "Worker thread error: " << e.what() << std::endl;
    }
}

} // namespace UltraFastAnalysis
