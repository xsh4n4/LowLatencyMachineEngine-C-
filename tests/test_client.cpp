#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/array.hpp>

using boost::asio::ip::tcp;

class TestClient {
public:
    TestClient(const std::string& host, const std::string& port)
        : io_context_(), socket_(io_context_) {
        try {
            tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(host, port);
            
            boost::asio::connect(socket_, endpoints);
            std::cout << "Connected to " << host << ":" << port << std::endl;
            
        } catch (std::exception& e) {
            std::cerr << "Connection failed: " << e.what() << std::endl;
            throw;
        }
    }
    
    ~TestClient() {
        if (socket_.is_open()) {
            socket_.close();
        }
    }
    
    // Submit a limit order
    void submit_order(const std::string& symbol, const std::string& side, 
                     uint64_t quantity, double price) {
        std::string message = symbol + ":" + side + ":" + 
                             std::to_string(quantity) + ":" + 
                             std::to_string(price) + ":1"; // LIMIT order type
        
        send_message(1, message); // ORDER_SUBMIT message type
        std::cout << "Submitted " << side << " order: " << symbol << " " 
                  << quantity << " @ " << price << std::endl;
    }
    
    // Cancel an order
    void cancel_order(uint64_t order_id, const std::string& symbol) {
        std::string message = std::to_string(order_id) + ":" + symbol;
        send_message(2, message); // ORDER_CANCEL message type
        std::cout << "Cancelled order " << order_id << " for " << symbol << std::endl;
    }
    
    // Modify an order
    void modify_order(uint64_t order_id, const std::string& symbol, 
                     uint64_t new_quantity, double new_price) {
        std::string message = std::to_string(order_id) + ":" + symbol + ":" + 
                             std::to_string(new_quantity) + ":" + 
                             std::to_string(new_price);
        send_message(3, message); // ORDER_MODIFY message type
        std::cout << "Modified order " << order_id << " to " << new_quantity 
                  << " @ " << new_price << std::endl;
    }
    
    // Request order book snapshot
    void request_order_book(const std::string& symbol) {
        std::string message = symbol;
        send_message(5, message); // ORDER_BOOK_REQUEST message type
        std::cout << "Requested order book for " << symbol << std::endl;
    }
    
    // Login
    void login(const std::string& client_name) {
        std::string message = client_name;
        send_message(8, message); // LOGIN message type
        std::cout << "Logged in as " << client_name << std::endl;
    }
    
    // Start listening for responses
    void start_listening() {
        std::thread([this]() {
            while (socket_.is_open()) {
                try {
                    // Read message header
                    boost::array<uint8_t, 24> header_buffer;
                    size_t len = boost::asio::read(socket_, boost::asio::buffer(header_buffer));
                    
                    if (len == sizeof(header_buffer)) {
                        // Parse header (simplified)
                        uint32_t message_type = *reinterpret_cast<uint32_t*>(&header_buffer[0]);
                        uint32_t message_length = *reinterpret_cast<uint32_t*>(&header_buffer[4]);
                        
                        // Read message body if present
                        if (message_length > 0) {
                            std::vector<uint8_t> body_buffer(message_length);
                            len = boost::asio::read(socket_, boost::asio::buffer(body_buffer));
                            
                            if (len == message_length) {
                                std::string body(reinterpret_cast<char*>(body_buffer.data()), message_length);
                                handle_response(message_type, body);
                            }
                        }
                    }
                } catch (std::exception& e) {
                    if (socket_.is_open()) {
                        std::cerr << "Read error: " << e.what() << std::endl;
                    }
                    break;
                }
            }
        }).detach();
    }
    
private:
    void send_message(uint32_t message_type, const std::string& data) {
        try {
            // Create message header
            struct {
                uint32_t message_type;
                uint32_t message_length;
                uint64_t sequence_number;
                uint64_t timestamp;
            } header;
            
            header.message_type = message_type;
            header.message_length = data.length();
            header.sequence_number = ++sequence_number_;
            header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            
            // Send header
            boost::asio::write(socket_, boost::asio::buffer(&header, sizeof(header)));
            
            // Send data if present
            if (!data.empty()) {
                boost::asio::write(socket_, boost::asio::buffer(data));
            }
            
        } catch (std::exception& e) {
            std::cerr << "Send error: " << e.what() << std::endl;
        }
    }
    
    void handle_response(uint32_t message_type, const std::string& data) {
        switch (message_type) {
            case 1: // ORDER_SUBMIT response
                std::cout << "Order confirmation: " << data << std::endl;
                break;
            case 4: // MARKET_DATA
                std::cout << "Market data: " << data << std::endl;
                break;
            case 5: // ORDER_BOOK response
                std::cout << "Order book: " << data << std::endl;
                break;
            default:
                std::cout << "Response (type " << message_type << "): " << data << std::endl;
                break;
        }
    }
    
    boost::asio::io_context io_context_;
    tcp::socket socket_;
    uint64_t sequence_number_ = 0;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <host> <port>\n";
        std::cout << "Example: " << argv[0] << " localhost 8080\n";
        return 1;
    }
    
    try {
        std::string host = argv[1];
        std::string port = argv[2];
        
        TestClient client(host, port);
        
        // Login
        client.login("TestClient");
        
        // Start listening for responses
        client.start_listening();
        
        // Wait a bit for connection to stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Submit some test orders
        std::cout << "\n=== Submitting Test Orders ===" << std::endl;
        
        // Submit buy orders
        client.submit_order("AAPL", "BUY", 1000, 150.50);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        client.submit_order("AAPL", "BUY", 500, 150.45);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        client.submit_order("GOOGL", "BUY", 200, 2800.00);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Submit sell orders
        client.submit_order("AAPL", "SELL", 800, 150.55);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        client.submit_order("AAPL", "SELL", 1200, 150.60);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        client.submit_order("GOOGL", "SELL", 300, 2805.00);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Request order book
        std::cout << "\n=== Requesting Order Books ===" << std::endl;
        client.request_order_book("AAPL");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        client.request_order_book("GOOGL");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Wait for responses
        std::cout << "\nWaiting for responses... (press Enter to exit)" << std::endl;
        std::cin.get();
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
