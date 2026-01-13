#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
#include "order_matching_engine.h"
#include "order_book.h"
#include "market_data.h"
#include "performance_monitor.h"

namespace py = pybind11;
using namespace UltraFastAnalysis;

// Python wrapper for Order
class PyOrder {
public:
    PyOrder(uint64_t order_id, uint64_t client_id, const std::string& symbol, 
            const std::string& side, const std::string& type, uint64_t quantity, double price)
        : order_(std::make_shared<Order>()) {
        order_->order_id = order_id;
        order_->client_id = client_id;
        order_->symbol = symbol;
        order_->side = (side == "BUY") ? OrderSide::BUY : OrderSide::SELL;
        order_->type = (type == "MARKET") ? OrderType::MARKET : OrderType::LIMIT;
        order_->quantity = quantity;
        order_->price = price;
        order_->timestamp = std::chrono::high_resolution_clock::now();
    }
    
    std::shared_ptr<Order> get_order() const { return order_; }
    
    // Getters
    uint64_t get_order_id() const { return order_->order_id; }
    uint64_t get_client_id() const { return order_->client_id; }
    std::string get_symbol() const { return order_->symbol; }
    std::string get_side() const { 
        return (order_->side == OrderSide::BUY) ? "BUY" : "SELL"; 
    }
    std::string get_type() const { 
        switch (order_->type) {
            case OrderType::MARKET: return "MARKET";
            case OrderType::LIMIT: return "LIMIT";
            case OrderType::STOP: return "STOP";
            case OrderType::STOP_LIMIT: return "STOP_LIMIT";
            default: return "UNKNOWN";
        }
    }
    uint64_t get_quantity() const { return order_->quantity; }
    uint64_t get_filled_quantity() const { return order_->filled_quantity; }
    double get_price() const { return order_->price; }
    std::string get_status() const {
        switch (order_->status) {
            case OrderStatus::PENDING: return "PENDING";
            case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
            case OrderStatus::FILLED: return "FILLED";
            case OrderStatus::CANCELLED: return "CANCELLED";
            case OrderStatus::REJECTED: return "REJECTED";
            default: return "UNKNOWN";
        }
    }
    
private:
    std::shared_ptr<Order> order_;
};

// Python wrapper for MarketData
class PyMarketData {
public:
    PyMarketData(const std::string& symbol, const std::string& type, double price, uint64_t quantity)
        : data_() {
        data_.symbol = symbol;
        data_.type = (type == "TRADE") ? MarketDataType::TRADE : 
                    (type == "QUOTE") ? MarketDataType::QUOTE : MarketDataType::TICK;
        data_.timestamp = std::chrono::high_resolution_clock::now();
        data_.price = price;
        data_.quantity = quantity;
    }
    
    const MarketData& get_data() const { return data_; }
    
    // Getters
    std::string get_symbol() const { return data_.symbol; }
    std::string get_type() const { 
        switch (data_.type) {
            case MarketDataType::TRADE: return "TRADE";
            case MarketDataType::QUOTE: return "QUOTE";
            case MarketDataType::ORDER_BOOK_UPDATE: return "ORDER_BOOK_UPDATE";
            case MarketDataType::TICK: return "TICK";
            default: return "UNKNOWN";
        }
    }
    double get_price() const { return data_.price; }
    uint64_t get_quantity() const { return data_.quantity; }
    
private:
    MarketData data_;
};

// Python wrapper for OrderBookSnapshot
class PyOrderBookSnapshot {
public:
    PyOrderBookSnapshot(const OrderBookSnapshot& snapshot) : snapshot_(snapshot) {}
    
    // Getters
    std::string get_symbol() const { return snapshot_.symbol; }
    std::vector<std::pair<double, uint64_t>> get_bids() const { return snapshot_.bids; }
    std::vector<std::pair<double, uint64_t>> get_asks() const { return snapshot_.asks; }
    
    // Python-friendly methods
    py::list get_bids_list() const {
        py::list result;
        for (const auto& [price, quantity] : snapshot_.bids) {
            py::dict bid;
            bid["price"] = price;
            bid["quantity"] = quantity;
            result.append(bid);
        }
        return result;
    }
    
    py::list get_asks_list() const {
        py::list result;
        for (const auto& [price, quantity] : snapshot_.asks) {
            py::dict ask;
            ask["price"] = price;
            ask["quantity"] = quantity;
            result.append(ask);
        }
        return result;
    }
    
private:
    OrderBookSnapshot snapshot_;
};

// Python wrapper for OrderMatchingEngine
class PyOrderMatchingEngine {
public:
    PyOrderMatchingEngine(size_t num_matching_threads = 4, size_t num_market_data_threads = 2)
        : engine_(std::make_unique<OrderMatchingEngine>()) {
        
        EngineConfig config;
        config.num_matching_threads = num_matching_threads;
        config.num_market_data_threads = num_market_data_threads;
        config.enable_performance_monitoring = true;
        
        engine_ = std::make_unique<OrderMatchingEngine>(config);
    }
    
    bool start() { return engine_->start(); }
    void stop() { engine_->stop(); }
    bool is_running() const { return engine_->is_running(); }
    
    bool submit_order(const PyOrder& py_order) {
        return engine_->submit_order(py_order.get_order());
    }
    
    bool cancel_order(uint64_t order_id, const std::string& symbol) {
        return engine_->cancel_order(order_id, symbol);
    }
    
    bool modify_order(uint64_t order_id, const std::string& symbol, 
                     uint64_t new_quantity, double new_price) {
        return engine_->modify_order(order_id, symbol, new_quantity, new_price);
    }
    
    bool submit_market_data(const PyMarketData& py_data) {
        return engine_->submit_market_data(py_data.get_data());
    }
    
    PyOrderBookSnapshot get_order_book_snapshot(const std::string& symbol) const {
        auto snapshot = engine_->get_order_book_snapshot(symbol);
        return PyOrderBookSnapshot(snapshot);
    }
    
    // Performance metrics
    py::dict get_performance_metrics() const {
        const auto& metrics = engine_->get_performance_metrics();
        py::dict result;
        result["orders_processed"] = metrics.orders_processed.load();
        result["trades_executed"] = metrics.trades_executed.load();
        result["market_data_updates"] = metrics.market_data_updates.load();
        result["avg_latency_microseconds"] = metrics.get_average_latency_microseconds();
        result["orders_per_second"] = metrics.orders_per_second.load();
        result["trades_per_second"] = metrics.trades_per_second.load();
        result["market_data_per_second"] = metrics.market_data_per_second.load();
        return result;
    }
    
    // Statistics
    size_t get_total_order_count() const { return engine_->get_total_order_count(); }
    size_t get_total_trade_count() const { return engine_->get_total_trade_count(); }
    py::list get_active_symbols() const { 
        auto symbols = engine_->get_active_symbols();
        py::list result;
        for (const auto& symbol : symbols) {
            result.append(symbol);
        }
        return result;
    }
    
    // Set callbacks
    void set_market_data_callback(py::function callback) {
        engine_->set_market_data_callback([callback](const MarketData& data) {
            py::gil_scoped_acquire gil;
            PyMarketData py_data(data.symbol, "TICK", data.price, data.quantity);
            callback(py_data);
        });
    }
    
private:
    std::unique_ptr<OrderMatchingEngine> engine_;
};

// Python wrapper for PerformanceMonitor
class PyPerformanceMonitor {
public:
    PyPerformanceMonitor(bool enable_detailed_monitoring = true)
        : monitor_(std::make_unique<PerformanceMonitor>(enable_detailed_monitoring)) {}
    
    bool start() { return monitor_->start(); }
    void stop() { monitor_->stop(); }
    bool is_running() const { return monitor_->is_running(); }
    
    void record_latency(const std::string& operation, uint64_t latency_ns) {
        monitor_->record_latency(operation, latency_ns);
    }
    
    void record_throughput(const std::string& operation, uint64_t count) {
        monitor_->record_throughput(operation, count);
    }
    
    double get_average_latency(const std::string& operation) const {
        return monitor_->get_average_latency(operation);
    }
    
    double get_percentile_latency(const std::string& operation, double percentile) const {
        return monitor_->get_percentile_latency(operation, percentile);
    }
    
    size_t get_current_memory_usage() const { return monitor_->get_current_memory_usage(); }
    double get_current_cpu_usage() const { return monitor_->get_current_cpu_usage(); }
    
    void generate_report(const std::string& filename = "") {
        monitor_->generate_report(filename);
    }
    
    void print_summary() const { monitor_->print_summary(); }
    
private:
    std::unique_ptr<PerformanceMonitor> monitor_;
};

PYBIND11_MODULE(order_engine_python, m) {
    m.doc() = "Ultra-Fast Analysis Order Matching Engine Python Bindings";
    
    // Order class
    py::class_<PyOrder>(m, "Order")
        .def(py::init<uint64_t, uint64_t, const std::string&, const std::string&, 
                      const std::string&, uint64_t, double>(),
             py::arg("order_id"), py::arg("client_id"), py::arg("symbol"), 
             py::arg("side"), py::arg("type"), py::arg("quantity"), py::arg("price"))
        .def_property_readonly("order_id", &PyOrder::get_order_id)
        .def_property_readonly("client_id", &PyOrder::get_client_id)
        .def_property_readonly("symbol", &PyOrder::get_symbol)
        .def_property_readonly("side", &PyOrder::get_side)
        .def_property_readonly("type", &PyOrder::get_type)
        .def_property_readonly("quantity", &PyOrder::get_quantity)
        .def_property_readonly("filled_quantity", &PyOrder::get_filled_quantity)
        .def_property_readonly("price", &PyOrder::get_price)
        .def_property_readonly("status", &PyOrder::get_status);
    
    // MarketData class
    py::class_<PyMarketData>(m, "MarketData")
        .def(py::init<const std::string&, const std::string&, double, uint64_t>(),
             py::arg("symbol"), py::arg("type"), py::arg("price"), py::arg("quantity"))
        .def_property_readonly("symbol", &PyMarketData::get_symbol)
        .def_property_readonly("type", &PyMarketData::get_type)
        .def_property_readonly("price", &PyMarketData::get_price)
        .def_property_readonly("quantity", &PyMarketData::get_quantity);
    
    // OrderBookSnapshot class
    py::class_<PyOrderBookSnapshot>(m, "OrderBookSnapshot")
        .def_property_readonly("symbol", &PyOrderBookSnapshot::get_symbol)
        .def_property_readonly("bids", &PyOrderBookSnapshot::get_bids_list)
        .def_property_readonly("asks", &PyOrderBookSnapshot::get_asks_list);
    
    // OrderMatchingEngine class
    py::class_<PyOrderMatchingEngine>(m, "OrderMatchingEngine")
        .def(py::init<size_t, size_t>(), 
             py::arg("num_matching_threads") = 4, 
             py::arg("num_market_data_threads") = 2)
        .def("start", &PyOrderMatchingEngine::start)
        .def("stop", &PyOrderMatchingEngine::stop)
        .def("is_running", &PyOrderMatchingEngine::is_running)
        .def("submit_order", &PyOrderMatchingEngine::submit_order)
        .def("cancel_order", &PyOrderMatchingEngine::cancel_order)
        .def("modify_order", &PyOrderMatchingEngine::modify_order)
        .def("submit_market_data", &PyOrderMatchingEngine::submit_market_data)
        .def("get_order_book_snapshot", &PyOrderMatchingEngine::get_order_book_snapshot)
        .def("get_performance_metrics", &PyOrderMatchingEngine::get_performance_metrics)
        .def("get_total_order_count", &PyOrderMatchingEngine::get_total_order_count)
        .def("get_total_trade_count", &PyOrderMatchingEngine::get_total_trade_count)
        .def("get_active_symbols", &PyOrderMatchingEngine::get_active_symbols)
        .def("set_market_data_callback", &PyOrderMatchingEngine::set_market_data_callback);
    
    // PerformanceMonitor class
    py::class_<PyPerformanceMonitor>(m, "PerformanceMonitor")
        .def(py::init<bool>(), py::arg("enable_detailed_monitoring") = true)
        .def("start", &PyPerformanceMonitor::start)
        .def("stop", &PyPerformanceMonitor::stop)
        .def("is_running", &PyPerformanceMonitor::is_running)
        .def("record_latency", &PyPerformanceMonitor::record_latency)
        .def("record_throughput", &PyPerformanceMonitor::record_throughput)
        .def("get_average_latency", &PyPerformanceMonitor::get_average_latency)
        .def("get_percentile_latency", &PyPerformanceMonitor::get_percentile_latency)
        .def("get_current_memory_usage", &PyPerformanceMonitor::get_current_memory_usage)
        .def("get_current_cpu_usage", &PyPerformanceMonitor::get_current_cpu_usage)
        .def("generate_report", &PyPerformanceMonitor::generate_report)
        .def("print_summary", &PyPerformanceMonitor::print_summary);
    
    // Constants
    m.attr("ORDER_SIDE_BUY") = "BUY";
    m.attr("ORDER_SIDE_SELL") = "SELL";
    m.attr("ORDER_TYPE_MARKET") = "MARKET";
    m.attr("ORDER_TYPE_LIMIT") = "LIMIT";
    m.attr("MARKET_DATA_TYPE_TRADE") = "TRADE";
    m.attr("MARKET_DATA_TYPE_QUOTE") = "QUOTE";
    m.attr("MARKET_DATA_TYPE_TICK") = "TICK";
    
    // Example usage function
    m.def("example_usage", []() {
        py::print("=== Example Usage ===");
        py::print("from order_engine_python import OrderMatchingEngine, Order, MarketData");
        py::print("");
        py::print("# Create engine");
        py::print("engine = OrderMatchingEngine(num_matching_threads=4)");
        py::print("engine.start()");
        py::print("");
        py::print("# Submit order");
        py::print("order = Order(1, 1, 'AAPL', 'BUY', 'LIMIT', 100, 150.50)");
        py::print("engine.submit_order(order)");
        py::print("");
        py::print("# Get order book");
        py::print("snapshot = engine.get_order_book_snapshot('AAPL')");
        py::print("print('Bids:', snapshot.bids)");
        py::print("print('Asks:', snapshot.asks)");
        py::print("");
        py::print("# Stop engine");
        py::print("engine.stop()");
    });
}
