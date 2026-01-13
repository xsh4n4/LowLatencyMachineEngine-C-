#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cassert>

namespace UltraFastAnalysis {

// Forward declarations
class PerformanceCounter;
class LatencyHistogram;
class MemoryTracker;
class CPUTracker;
class CacheMonitor;

// Counter types
enum class CounterType {
    COUNTER,
    GAUGE,
    HISTOGRAM,
    LATENCY,    // Add missing types
    THROUGHPUT  // Add missing types
};

// Performance counter class
class PerformanceCounter {
public:
    explicit PerformanceCounter(CounterType type = CounterType::COUNTER);
    explicit PerformanceCounter(const std::string& name, CounterType type = CounterType::COUNTER);
    
    void increment(uint64_t value = 1);
    void set(uint64_t value);
    void record_latency(uint64_t latency_ns);
    void update(uint64_t value); // Alias for increment
    
    uint64_t get_count() const;
    uint64_t get_total() const;
    double get_average() const;
    uint64_t get_min() const;
    uint64_t get_max() const;
    
    // Histogram-specific methods
    std::vector<uint64_t> get_histogram() const;
    double get_percentile(double percentile) const;
    
    void reset();
    
    // Public members for direct access
    std::atomic<uint64_t> value{0};
    std::atomic<uint64_t> count{0};
    std::atomic<uint64_t> min_value{UINT64_MAX};
    std::atomic<uint64_t> max_value{0};
    CounterType type;
    
private:
    CounterType type_;
    std::atomic<uint64_t> count_{0};
    std::atomic<uint64_t> total_{0};
    std::atomic<uint64_t> min_{UINT64_MAX};
    std::atomic<uint64_t> max_{0};
    
    // Histogram data
    mutable std::mutex histogram_mutex_;
    std::vector<uint64_t> histogram_;
    static constexpr size_t HISTOGRAM_BINS = 100;
    static constexpr uint64_t MAX_LATENCY_NS = 1000000000; // 1 second
};

// Latency histogram class
class LatencyHistogram {
public:
    LatencyHistogram();
    explicit LatencyHistogram(size_t num_buckets);
    
    void record_latency(uint64_t latency_ns);
    void add_latency(uint64_t latency_ns); // Alias for record_latency
    std::vector<std::pair<uint64_t, uint64_t>> get_histogram() const; // Fix return type
    uint64_t get_percentile(double percentile) const; // Fix return type
    void reset();
    
private:
    mutable std::mutex mutex_;
    std::vector<uint64_t> histogram_;
    std::vector<uint64_t> sorted_latencies_;
    
    // Implementation-specific members
    std::vector<uint64_t> buckets_;
    std::atomic<uint64_t> total_count_{0};
    size_t num_buckets_;
    uint64_t bucket_size_ns_;
    
    size_t get_bucket_index(uint64_t latency_ns) const;
    
    static constexpr size_t HISTOGRAM_BINS = 100;
    static constexpr uint64_t MAX_LATENCY_NS = 1000000000; // 1 second
};

// Memory tracker class
class MemoryTracker {
public:
    MemoryTracker();
    ~MemoryTracker();
    
    void start_monitoring();
    void stop_monitoring();
    void update_memory_usage();
    
    size_t get_current_memory_usage() const;
    size_t get_peak_memory_usage() const;
    double get_memory_growth_rate() const;
    
    // Additional methods from implementation
    double get_memory_usage_mb() const;
    double get_peak_memory_usage_mb() const;
    void reset();
    
private:
    std::atomic<bool> monitoring_{false};
    std::thread monitoring_thread_;
    std::atomic<bool> shutdown_requested_{false};
    
    std::atomic<size_t> current_memory_{0};
    std::atomic<size_t> peak_memory_{0};
    std::atomic<size_t> initial_memory_{0};
    std::atomic<double> growth_rate_{0.0};
    
    std::chrono::steady_clock::time_point last_update_;
    mutable std::mutex mutex_;
    
    void monitoring_thread_worker();
    size_t get_process_memory_usage() const;
};

// CPU tracker class
class CPUTracker {
public:
    CPUTracker();
    ~CPUTracker();
    
    void start_monitoring();
    void stop_monitoring();
    void update_cpu_usage();
    
    double get_current_cpu_usage() const;
    double get_average_cpu_usage() const;
    double get_cpu_utilization() const;
    
    // Additional methods from implementation
    void reset();
    
private:
    std::atomic<bool> monitoring_{false};
    std::thread monitoring_thread_;
    std::atomic<bool> shutdown_requested_{false};
    
    std::atomic<double> current_cpu_{0.0};
    std::atomic<double> average_cpu_{0.0};
    std::atomic<double> cpu_utilization_{0.0};
    
    // Implementation-specific members
    std::atomic<double> total_cpu_{0.0};
    std::atomic<uint64_t> cpu_readings_{0};
    double last_cpu_time_{0.0};
    
    std::chrono::steady_clock::time_point last_update_;
    mutable std::mutex mutex_;
    
    void monitoring_thread_worker();
    double get_system_cpu_time() const;
    double get_process_cpu_time() const;
};

// Cache monitor class
class CacheMonitor {
public:
    CacheMonitor();
    ~CacheMonitor();
    
    void start_monitoring();
    void stop_monitoring();
    void update_cache_metrics();
    
    uint64_t get_cache_misses() const;
    uint64_t get_branch_misses() const;
    uint64_t get_context_switches() const;
    
    // Additional methods from implementation
    bool is_monitoring() const;
    void reset();
    
private:
    std::atomic<bool> monitoring_{false};
    std::thread monitoring_thread_;
    std::atomic<bool> shutdown_requested_{false};
    
    std::atomic<uint64_t> cache_misses_{0};
    std::atomic<uint64_t> branch_misses_{0};
    std::atomic<uint64_t> context_switches_{0};
    
    void monitoring_thread_worker();
    
    // Platform-specific cache monitoring
    bool initialize_perf_counters();
    void cleanup_perf_counters();
    
    int perf_fd_cache_misses_{-1};
    int perf_fd_branch_misses_{-1};
    int perf_fd_context_switches_{-1};
};

// Main performance monitor class
class PerformanceMonitor {
public:
    explicit PerformanceMonitor(bool enable_detailed_monitoring = true);
    ~PerformanceMonitor();
    
    // Non-copyable, non-movable
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
    
    // Monitor lifecycle
    bool start();
    void stop();
    bool is_running() const;
    
    // Counter management
    PerformanceCounter* get_counter(const std::string& name);
    const PerformanceCounter* get_counter(const std::string& name) const; // Add const version
    void create_counter(const std::string& name, CounterType type);
    void remove_counter(const std::string& name);
    
    // Latency tracking
    void record_latency(const std::string& operation, uint64_t latency_ns);
    double get_average_latency(const std::string& operation) const;
    double get_percentile_latency(const std::string& operation, double percentile) const;
    
    // Throughput tracking
    void record_throughput(const std::string& operation, uint64_t count);
    uint64_t get_throughput(const std::string& operation) const;
    
    // Memory and CPU tracking
    size_t get_current_memory_usage() const;
    double get_current_cpu_usage() const;
    
    // Cache performance
    uint64_t get_cache_misses() const;
    uint64_t get_branch_misses() const;
    
    // Reporting
    void generate_report(const std::string& filename = "");
    void print_summary() const;
    
    // Configuration
    void set_monitoring_interval(std::chrono::milliseconds interval);
    void enable_detailed_monitoring(bool enable);
    
    // Reset all counters
    void reset_all_counters();
    
private:
    std::atomic<bool> running_{false};
    std::thread monitoring_thread_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Monitoring components
    std::unique_ptr<MemoryTracker> memory_tracker_;
    std::unique_ptr<CPUTracker> cpu_tracker_;
    std::unique_ptr<CacheMonitor> cache_monitor_;
    
    // Counters
    std::unordered_map<std::string, std::unique_ptr<PerformanceCounter>> counters_;
    mutable std::mutex counters_mutex_;
    
    // Latency histograms
    std::unordered_map<std::string, std::unique_ptr<LatencyHistogram>> latency_histograms_;
    mutable std::mutex histograms_mutex_;
    
    // Configuration
    std::chrono::milliseconds monitoring_interval_{1000};
    bool detailed_monitoring_enabled_;
    
    // Internal methods
    void monitoring_thread_worker();
    void update_system_metrics();
    void cleanup_old_data();
    
    // Report generation
    void write_csv_report(const std::string& filename);
    void write_json_report(const std::string& filename);
    
    // Thread synchronization
    std::condition_variable shutdown_cv_;
    std::mutex shutdown_mutex_;
};

} // namespace UltraFastAnalysis