#include "order_matching_engine.h"
#include "performance_monitor.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <chrono>
#include <thread>

namespace UltraFastAnalysis {

// Forward declarations
void print_engine_stats();
void cleanup();

// Global variables for signal handling
std::atomic<bool> shutdown_requested{false};
std::unique_ptr<OrderMatchingEngine> engine;
std::unique_ptr<PerformanceMonitor> performance_monitor;

// Signal handler function
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", initiating graceful shutdown..." << std::endl;
    shutdown_requested.store(true);
}

// Setup signal handlers
void setup_signal_handlers() {
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // Termination request
    
#ifdef _WIN32
    signal(SIGBREAK, signal_handler); // Windows Ctrl+Break
#else
    signal(SIGHUP, signal_handler);   // Hangup
#endif
}

// Print usage information
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -h, --help              Show this help message\n"
              << "  -p, --port <port>       TCP server port (default: 8080)\n"
              << "  -t, --threads <num>     Number of matching threads (default: 4)\n"
              << "  -m, --market-threads <num> Number of market data threads (default: 2)\n"
              << "  -b, --buffer-size <size> Ring buffer size (default: 65536)\n"
              << "  -v, --verbose           Enable verbose logging\n"
              << "  --no-performance        Disable performance monitoring\n"
              << "  --simulate-only         Run in simulation mode only\n"
              << std::endl;
}

// Parse command line arguments
EngineConfig parse_arguments(int argc, char* argv[]) {
    EngineConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "-p" || arg == "--port") {
            if (++i < argc) {
                config.tcp_port = std::stoi(argv[i]);
            }
        } else if (arg == "-t" || arg == "--threads") {
            if (++i < argc) {
                config.num_matching_threads = std::stoul(argv[i]);
            }
        } else if (arg == "-m" || arg == "--market-threads") {
            if (++i < argc) {
                config.num_market_data_threads = std::stoul(argv[i]);
            }
        } else if (arg == "-b" || arg == "--buffer-size") {
            if (++i < argc) {
                size_t buffer_size = std::stoul(argv[i]);
                // Ensure buffer size is a power of 2
                if ((buffer_size & (buffer_size - 1)) == 0) {
                    config.ring_buffer_size = buffer_size;
                } else {
                    std::cerr << "Warning: Buffer size must be a power of 2, using default" << std::endl;
                }
            }
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose_logging = true;
        } else if (arg == "--no-performance") {
            config.enable_performance_monitoring = false;
        } else if (arg == "--simulate-only") {
            config.simulation_mode = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            exit(1);
        }
    }
    
    return config;
}

// Print engine configuration
void print_config(const EngineConfig& config) {
    std::cout << "\n=== Engine Configuration ===" << std::endl;
    std::cout << "TCP Port: " << config.tcp_port << std::endl;
    std::cout << "Matching Threads: " << config.num_matching_threads << std::endl;
    std::cout << "Market Data Threads: " << config.num_market_data_threads << std::endl;
    std::cout << "Ring Buffer Size: " << config.ring_buffer_size << std::endl;
    std::cout << "Performance Monitoring: " << (config.enable_performance_monitoring ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Simulation Mode: " << (config.simulation_mode ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Verbose Logging: " << (config.verbose_logging ? "Enabled" : "Disabled") << std::endl;
    std::cout << "=============================" << std::endl;
}

// Main application loop
void run_application(const EngineConfig& config) {
    std::cout << "Starting Ultra-Fast Order Matching Engine..." << std::endl;
    
    try {
        // Initialize performance monitor
        if (config.enable_performance_monitoring) {
            performance_monitor = std::make_unique<PerformanceMonitor>(true);
            if (!performance_monitor->start()) {
                std::cerr << "Failed to start performance monitor" << std::endl;
                return;
            }
            std::cout << "Performance monitor started" << std::endl;
        }
        
        // Initialize order matching engine
        engine = std::make_unique<OrderMatchingEngine>(config);
        
        // Start the engine
        if (!engine->start()) {
            std::cerr << "Failed to start order matching engine" << std::endl;
            return;
        }
        
        std::cout << "Order matching engine started successfully" << std::endl;
        std::cout << "Press Ctrl+C to stop the engine" << std::endl;
        
        // Main application loop
        auto last_stats_time = std::chrono::high_resolution_clock::now();
        const auto stats_interval = std::chrono::seconds(10);
        
        while (!shutdown_requested.load()) {
            auto now = std::chrono::high_resolution_clock::now();
            
            // Print periodic statistics
            if (now - last_stats_time >= stats_interval) {
                print_engine_stats();
                last_stats_time = now;
            }
            
            // Small sleep to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
    }
}

// Print engine statistics
void print_engine_stats() {
    if (!engine) return;
    
    const auto& metrics = engine->get_performance_metrics();
    const auto& symbols = engine->get_active_symbols();
    
    std::cout << "\n=== Engine Statistics ===" << std::endl;
    std::cout << "Orders Processed: " << metrics.orders_processed.load() << std::endl;
    std::cout << "Trades Executed: " << metrics.trades_executed.load() << std::endl;
    std::cout << "Market Data Updates: " << metrics.market_data_updates.load() << std::endl;
    std::cout << "Active Symbols: " << symbols.size() << std::endl;
    
    if (metrics.orders_processed.load() > 0) {
        std::cout << "Average Latency: " << std::fixed << std::setprecision(2)
                  << metrics.get_average_latency_microseconds() << " μs" << std::endl;
        std::cout << "Min Latency: " << metrics.min_latency_ns.load() / 1000.0 << " μs" << std::endl;
        std::cout << "Max Latency: " << metrics.max_latency_ns.load() / 1000.0 << " μs" << std::endl;
    }
    
    std::cout << "Orders/sec: " << metrics.orders_per_second.load() << std::endl;
    std::cout << "Trades/sec: " << metrics.trades_per_second.load() << std::endl;
    std::cout << "Market Data/sec: " << metrics.market_data_per_second.load() << std::endl;
    std::cout << "=========================" << std::endl;
}

// Cleanup and shutdown
void cleanup() {
    std::cout << "\nCleaning up..." << std::endl;
    
    // Stop the engine
    if (engine) {
        engine->stop();
        engine.reset();
    }
    
    // Stop performance monitor
    if (performance_monitor) {
        performance_monitor->stop();
        
        // Generate final performance report
        performance_monitor->generate_report("final_performance_report");
        performance_monitor->print_summary();
        
        performance_monitor.reset();
    }
    
    std::cout << "Cleanup completed" << std::endl;
}

} // namespace UltraFastAnalysis

// Main function
int main(int argc, char* argv[]) {
    using namespace UltraFastAnalysis;
    
    try {
        // Setup signal handlers
        setup_signal_handlers();
        
        // Parse command line arguments
        EngineConfig config = parse_arguments(argc, argv);
        
        // Print configuration
        print_config(config);
        
        // Run the application
        run_application(config);
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    // Cleanup
    cleanup();
    
    std::cout << "Ultra-Fast Order Matching Engine stopped" << std::endl;
    return 0;
}
