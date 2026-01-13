#include "performance_monitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <memoryapi.h>
#else
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

namespace UltraFastAnalysis {

// PerformanceCounter implementation
PerformanceCounter::PerformanceCounter(CounterType type)
    : type_(type) {
}

PerformanceCounter::PerformanceCounter(const std::string& name, CounterType type)
    : type_(type) {
}

void PerformanceCounter::increment(uint64_t value) {
    count_.fetch_add(1);
    total_.fetch_add(value);
    
    uint64_t current = value;
    uint64_t min_val = min_.load();
    while (current < min_val && !min_.compare_exchange_weak(min_val, current)) {}
    
    uint64_t max_val = max_.load();
    while (current > max_val && !max_.compare_exchange_weak(max_val, current)) {}
}

void PerformanceCounter::set(uint64_t value) {
    this->value.store(value);
}

void PerformanceCounter::record_latency(uint64_t latency_ns) {
    increment(latency_ns);
}

void PerformanceCounter::update(uint64_t value) {
    increment(value);
}

uint64_t PerformanceCounter::get_count() const {
    return count_.load();
}

uint64_t PerformanceCounter::get_total() const {
    return total_.load();
}

double PerformanceCounter::get_average() const {
    uint64_t count = count_.load();
    return count > 0 ? static_cast<double>(total_.load()) / count : 0.0;
}

uint64_t PerformanceCounter::get_min() const {
    return min_.load();
}

uint64_t PerformanceCounter::get_max() const {
    return max_.load();
}

std::vector<uint64_t> PerformanceCounter::get_histogram() const {
    // Simplified histogram implementation
    std::vector<uint64_t> result(100, 0);
    return result;
}

double PerformanceCounter::get_percentile(double percentile) const {
    // Simplified percentile implementation
    return get_average();
}

void PerformanceCounter::reset() {
    count_.store(0);
    total_.store(0);
    min_.store(UINT64_MAX);
    max_.store(0);
    value.store(0);
}

// LatencyHistogram implementation
LatencyHistogram::LatencyHistogram()
    : num_buckets_(100), bucket_size_ns_(1000) { // Default constructor
    
    buckets_.resize(num_buckets_, 0);
}

LatencyHistogram::LatencyHistogram(size_t num_buckets)
    : num_buckets_(num_buckets), bucket_size_ns_(1000) { // 1 microsecond buckets
    
    buckets_.resize(num_buckets, 0);
}

void LatencyHistogram::add_latency(uint64_t latency_ns) {
    size_t bucket_index = get_bucket_index(latency_ns);
    if (bucket_index < buckets_.size()) {
        std::lock_guard<std::mutex> lock(mutex_);
        buckets_[bucket_index]++;
        total_count_.fetch_add(1);
    }
}

void LatencyHistogram::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& bucket : buckets_) {
        bucket = 0;
    }
    total_count_.store(0);
}

std::vector<std::pair<uint64_t, uint64_t>> LatencyHistogram::get_histogram() const {
    std::vector<std::pair<uint64_t, uint64_t>> result;
    result.reserve(buckets_.size());
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < buckets_.size(); ++i) {
        uint64_t count = buckets_[i];
        if (count > 0) {
            uint64_t latency_ns = i * bucket_size_ns_;
            result.emplace_back(latency_ns, count);
        }
    }
    
    return result;
}

uint64_t LatencyHistogram::get_percentile(double percentile) const {
    if (percentile < 0.0 || percentile > 100.0) {
        return 0;
    }
    
    uint64_t total = total_count_.load();
    if (total == 0) {
        return 0;
    }
    
    uint64_t target_count = static_cast<uint64_t>(total * percentile / 100.0);
    uint64_t current_count = 0;
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < buckets_.size(); ++i) {
        current_count += buckets_[i];
        if (current_count >= target_count) {
            return i * bucket_size_ns_;
        }
    }
    
    return (buckets_.size() - 1) * bucket_size_ns_;
}

size_t LatencyHistogram::get_bucket_index(uint64_t latency_ns) const {
    uint64_t bucket_index = latency_ns / bucket_size_ns_;
    uint64_t max_bucket = static_cast<uint64_t>(num_buckets_ - 1);
    return static_cast<size_t>((bucket_index < max_bucket) ? bucket_index : max_bucket);
}

// MemoryTracker implementation
MemoryTracker::MemoryTracker() {
    update_memory_usage();
}

MemoryTracker::~MemoryTracker() {
    stop_monitoring();
}

void MemoryTracker::update_memory_usage() {
    size_t current = get_process_memory_usage();
    current_memory_.store(current);
    
    size_t peak = peak_memory_.load();
    while (current > peak &&
           !peak_memory_.compare_exchange_weak(peak, current)) {}
}

void MemoryTracker::start_monitoring() {
    if (monitoring_.load()) {
        return;
    }
    
    monitoring_.store(true);
    shutdown_requested_.store(false);
    
    monitoring_thread_ = std::thread(&MemoryTracker::monitoring_thread_worker, this);
}

void MemoryTracker::stop_monitoring() {
    if (!monitoring_.load()) {
        return;
    }
    
    shutdown_requested_.store(true);
    monitoring_.store(false);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void MemoryTracker::monitoring_thread_worker() {
    while (!shutdown_requested_.load()) {
        update_memory_usage();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

size_t MemoryTracker::get_current_memory_usage() const {
    return current_memory_.load();
}

size_t MemoryTracker::get_peak_memory_usage() const {
    return peak_memory_.load();
}

double MemoryTracker::get_memory_usage_mb() const {
    return static_cast<double>(current_memory_.load()) / (1024 * 1024);
}

double MemoryTracker::get_peak_memory_usage_mb() const {
    return static_cast<double>(peak_memory_.load()) / (1024 * 1024);
}

double MemoryTracker::get_memory_growth_rate() const {
    return growth_rate_.load();
}

void MemoryTracker::reset() {
    current_memory_.store(0);
    peak_memory_.store(0);
}

size_t MemoryTracker::get_process_memory_usage() const {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    FILE* file = fopen("/proc/self/status", "r");
    if (!file) return 0;
    
    size_t memory = 0;
    char line[128];
    
    while (fgets(line, 128, file) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %zu", &memory);
            break;
        }
    }
    
    fclose(file);
    return memory * 1024; // Convert KB to bytes
#endif
}

// CPUTracker implementation
CPUTracker::CPUTracker() : last_cpu_time_(0.0) {
    last_update_ = std::chrono::high_resolution_clock::now();
    update_cpu_usage();
}

CPUTracker::~CPUTracker() {
    stop_monitoring();
}

void CPUTracker::update_cpu_usage() {
    auto now = std::chrono::high_resolution_clock::now();
    double current_cpu_time = get_process_cpu_time();
    double system_cpu_time = get_system_cpu_time();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_).count();
    if (elapsed > 0) {
        double cpu_usage = ((current_cpu_time - last_cpu_time_) / elapsed) * 100.0;
        current_cpu_.store(cpu_usage);
        
        total_cpu_.fetch_add(cpu_usage);
        cpu_readings_.fetch_add(1);
    }
    
    last_cpu_time_ = current_cpu_time;
    last_update_ = now;
}

void CPUTracker::start_monitoring() {
    if (monitoring_.load()) {
        return;
    }
    
    monitoring_.store(true);
    shutdown_requested_.store(false);
    
    monitoring_thread_ = std::thread(&CPUTracker::monitoring_thread_worker, this);
}

void CPUTracker::stop_monitoring() {
    if (!monitoring_.load()) {
        return;
    }
    
    shutdown_requested_.store(true);
    monitoring_.store(false);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void CPUTracker::monitoring_thread_worker() {
    while (!shutdown_requested_.load()) {
        update_cpu_usage();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

double CPUTracker::get_current_cpu_usage() const {
    return current_cpu_.load();
}

double CPUTracker::get_average_cpu_usage() const {
    uint64_t readings = cpu_readings_.load();
    return readings > 0 ? total_cpu_.load() / readings : 0.0;
}

double CPUTracker::get_cpu_utilization() const {
    return current_cpu_.load();
}

void CPUTracker::reset() {
    current_cpu_.store(0.0);
    total_cpu_.store(0.0);
    cpu_readings_.store(0);
}

double CPUTracker::get_process_cpu_time() const {
#ifdef _WIN32
    FILETIME create_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time)) {
        uint64_t kernel = (static_cast<uint64_t>(kernel_time.dwHighDateTime) << 32) | kernel_time.dwLowDateTime;
        uint64_t user = (static_cast<uint64_t>(user_time.dwHighDateTime) << 32) | user_time.dwLowDateTime;
        return static_cast<double>(kernel + user) / 10000000.0; // Convert to seconds
    }
    return 0.0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<double>(usage.ru_utime.tv_sec) + 
               static_cast<double>(usage.ru_utime.tv_usec) / 1000000.0 +
               static_cast<double>(usage.ru_stime.tv_sec) + 
               static_cast<double>(usage.ru_stime.tv_usec) / 1000000.0;
    }
    return 0.0;
#endif
}

double CPUTracker::get_system_cpu_time() const {
#ifdef _WIN32
    // Windows doesn't have a direct equivalent, return 0
    return 0.0;
#else
    FILE* file = fopen("/proc/stat", "r");
    if (!file) return 0.0;
    
    unsigned long user, nice, system, idle;
    if (fscanf(file, "cpu %lu %lu %lu %lu", &user, &nice, &system, &idle) == 4) {
        fclose(file);
        return static_cast<double>(user + nice + system + idle) / 100.0;
    }
    
    fclose(file);
    return 0.0;
#endif
}

// CacheMonitor implementation
CacheMonitor::CacheMonitor() {
    initialize_perf_counters();
}

CacheMonitor::~CacheMonitor() {
    stop_monitoring();
    cleanup_perf_counters();
}

void CacheMonitor::start_monitoring() {
    if (monitoring_.load()) {
        return;
    }
    
    monitoring_.store(true);
    shutdown_requested_.store(false);
    
    monitoring_thread_ = std::thread(&CacheMonitor::monitoring_thread_worker, this);
}

void CacheMonitor::stop_monitoring() {
    if (!monitoring_.load()) {
        return;
    }
    
    shutdown_requested_.store(true);
    monitoring_.store(false);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

bool CacheMonitor::is_monitoring() const {
    return monitoring_.load();
}

uint64_t CacheMonitor::get_cache_misses() const {
    return cache_misses_.load();
}

uint64_t CacheMonitor::get_branch_misses() const {
    return branch_misses_.load();
}

uint64_t CacheMonitor::get_context_switches() const {
    return context_switches_.load();
}

void CacheMonitor::reset() {
    cache_misses_.store(0);
    branch_misses_.store(0);
    context_switches_.store(0);
}

void CacheMonitor::monitoring_thread_worker() {
    while (!shutdown_requested_.load()) {
        update_cache_metrics();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void CacheMonitor::update_cache_metrics() {
    // Platform-specific cache monitoring implementation
    // This is a simplified version - in production you'd use proper performance counters
#ifdef _WIN32
    // Windows implementation would use PdhCollectQueryData or similar
#else
    // Linux implementation would read from /proc/self/status or use perf_event_open
#endif
}

bool CacheMonitor::initialize_perf_counters() {
    // Platform-specific initialization
    // This is a placeholder - in production you'd set up proper performance monitoring
    return true;
}

void CacheMonitor::cleanup_perf_counters() {
    // Platform-specific cleanup
}

// PerformanceMonitor implementation
PerformanceMonitor::PerformanceMonitor(bool enable_detailed_monitoring)
    : detailed_monitoring_enabled_(enable_detailed_monitoring) {
    
    // Initialize monitoring components
    memory_tracker_ = std::make_unique<MemoryTracker>();
    cpu_tracker_ = std::make_unique<CPUTracker>();
    cache_monitor_ = std::make_unique<CacheMonitor>();
}

PerformanceMonitor::~PerformanceMonitor() {
    stop();
}

bool PerformanceMonitor::start() {
    if (running_.load()) {
        return true;
    }
    
    try {
        // Start cache monitoring if detailed monitoring is enabled
        if (detailed_monitoring_enabled_) {
            cache_monitor_->start_monitoring();
        }
        
        // Start monitoring thread
        monitoring_thread_ = std::thread(&PerformanceMonitor::monitoring_thread_worker, this);
        
        running_.store(true);
        std::cout << "Performance monitor started" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start performance monitor: " << e.what() << std::endl;
        return false;
    }
}

void PerformanceMonitor::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "Stopping performance monitor..." << std::endl;
    
    shutdown_requested_.store(true);
    running_.store(false);
    
    // Stop cache monitoring
    if (cache_monitor_) {
        cache_monitor_->stop_monitoring();
    }
    
    // Wait for monitoring thread
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    std::cout << "Performance monitor stopped" << std::endl;
}

bool PerformanceMonitor::is_running() const {
    return running_.load();
}

PerformanceCounter* PerformanceMonitor::get_counter(const std::string& name) {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    auto it = counters_.find(name);
    return (it != counters_.end()) ? it->second.get() : nullptr;
}

const PerformanceCounter* PerformanceMonitor::get_counter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    auto it = counters_.find(name);
    return (it != counters_.end()) ? it->second.get() : nullptr;
}

void PerformanceMonitor::create_counter(const std::string& name, CounterType type) {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    counters_[name] = std::make_unique<PerformanceCounter>(name, type);
}

void PerformanceMonitor::remove_counter(const std::string& name) {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    counters_.erase(name);
}

void PerformanceMonitor::record_latency(const std::string& operation, uint64_t latency_ns) {
    // Get or create counter
    PerformanceCounter* counter = get_counter(operation);
    if (!counter) {
        create_counter(operation, CounterType::LATENCY);
        counter = get_counter(operation);
    }
    
    if (counter) {
        counter->update(latency_ns);
        
        // Update latency histogram if detailed monitoring is enabled
        if (detailed_monitoring_enabled_) {
            std::lock_guard<std::mutex> lock(histograms_mutex_);
            if (latency_histograms_.find(operation) == latency_histograms_.end()) {
                latency_histograms_[operation] = std::make_unique<LatencyHistogram>();
            }
            latency_histograms_[operation]->add_latency(latency_ns);
        }
    }
}

double PerformanceMonitor::get_average_latency(const std::string& operation) const {
    auto counter = get_counter(operation);
    return counter ? counter->get_average() : 0.0;
}

double PerformanceMonitor::get_percentile_latency(const std::string& operation, double percentile) const {
    if (!detailed_monitoring_enabled_) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> lock(histograms_mutex_);
    auto it = latency_histograms_.find(operation);
    if (it != latency_histograms_.end()) {
        return it->second->get_percentile(percentile);
    }
    return 0.0;
}

void PerformanceMonitor::record_throughput(const std::string& operation, uint64_t count) {
    PerformanceCounter* counter = get_counter(operation);
    if (!counter) {
        create_counter(operation, CounterType::THROUGHPUT);
        counter = get_counter(operation);
    }
    
    if (counter) {
        counter->update(count);
    }
}

uint64_t PerformanceMonitor::get_throughput(const std::string& operation) const {
    auto counter = get_counter(operation);
    return counter ? counter->value.load() : 0;
}

size_t PerformanceMonitor::get_current_memory_usage() const {
    return memory_tracker_ ? memory_tracker_->get_current_memory_usage() : 0;
}

double PerformanceMonitor::get_current_cpu_usage() const {
    return cpu_tracker_ ? cpu_tracker_->get_current_cpu_usage() : 0.0;
}

uint64_t PerformanceMonitor::get_cache_misses() const {
    return cache_monitor_ ? cache_monitor_->get_cache_misses() : 0;
}

uint64_t PerformanceMonitor::get_branch_misses() const {
    return cache_monitor_ ? cache_monitor_->get_branch_misses() : 0;
}

void PerformanceMonitor::generate_report(const std::string& filename) {
    if (filename.empty()) {
        write_csv_report("performance_report.csv");
        write_json_report("performance_report.json");
    } else {
        std::string base_name = filename.substr(0, filename.find_last_of('.'));
        write_csv_report(base_name + ".csv");
        write_json_report(base_name + ".json");
    }
}

void PerformanceMonitor::print_summary() const {
    std::cout << "\n=== Performance Monitor Summary ===" << std::endl;
    
    // Memory usage
    if (memory_tracker_) {
        std::cout << "Memory Usage: " << std::fixed << std::setprecision(2)
                  << memory_tracker_->get_memory_usage_mb() << " MB (Peak: "
                  << memory_tracker_->get_peak_memory_usage_mb() << " MB)" << std::endl;
    }
    
    // CPU usage
    if (cpu_tracker_) {
        std::cout << "CPU Usage: " << std::fixed << std::setprecision(2)
                  << cpu_tracker_->get_current_cpu_usage() << "% (Avg: "
                  << cpu_tracker_->get_average_cpu_usage() << "%)" << std::endl;
    }
    
    // Cache performance
    if (cache_monitor_ && detailed_monitoring_enabled_) {
        std::cout << "Cache Misses: " << cache_monitor_->get_cache_misses() << std::endl;
        std::cout << "Branch Misses: " << cache_monitor_->get_branch_misses() << std::endl;
    }
    
    // Counter summary
    std::cout << "\nCounters:" << std::endl;
    std::lock_guard<std::mutex> lock(counters_mutex_);
    for (const auto& [name, counter] : counters_) {
        std::cout << "  " << name << ": " << counter->value.load()
                  << " (Avg: " << std::fixed << std::setprecision(2) << counter->get_average() << ")" << std::endl;
    }
    
    std::cout << "===================================" << std::endl;
}

void PerformanceMonitor::set_monitoring_interval(std::chrono::milliseconds interval) {
    monitoring_interval_ = interval;
}

void PerformanceMonitor::enable_detailed_monitoring(bool enable) {
    detailed_monitoring_enabled_ = enable;
}

void PerformanceMonitor::reset_all_counters() {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    for (auto& [name, counter] : counters_) {
        counter->reset();
    }
    
    std::lock_guard<std::mutex> hist_lock(histograms_mutex_);
    for (auto& [name, histogram] : latency_histograms_) {
        histogram->reset();
    }
    
    if (memory_tracker_) memory_tracker_->reset();
    if (cpu_tracker_) cpu_tracker_->reset();
    if (cache_monitor_) cache_monitor_->reset();
}

void PerformanceMonitor::monitoring_thread_worker() {
    std::cout << "Performance monitoring thread started: " << std::this_thread::get_id() << std::endl;
    
    while (!shutdown_requested_.load()) {
        update_system_metrics();
        cleanup_old_data();
        
        std::this_thread::sleep_for(monitoring_interval_);
    }
    
    std::cout << "Performance monitoring thread stopped: " << std::this_thread::get_id() << std::endl;
}

void PerformanceMonitor::update_system_metrics() {
    if (memory_tracker_) {
        memory_tracker_->update_memory_usage();
    }
    
    if (cpu_tracker_) {
        cpu_tracker_->update_cpu_usage();
    }
}

void PerformanceMonitor::cleanup_old_data() {
    // Clean up old histogram data periodically
    // This is a simplified implementation
}

void PerformanceMonitor::write_csv_report(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open CSV file: " << filename << std::endl;
        return;
    }
    
    // Write header
    file << "Counter,Type,Current,Min,Max,Average,Count\n";
    
    // Write counter data
    std::lock_guard<std::mutex> lock(counters_mutex_);
    for (const auto& [name, counter] : counters_) {
        file << name << ","
             << static_cast<int>(counter->type) << ","
             << counter->value.load() << ","
             << counter->min_value.load() << ","
             << counter->max_value.load() << ","
             << std::fixed << std::setprecision(2) << counter->get_average() << ","
             << counter->count.load() << "\n";
    }
    
    file.close();
    std::cout << "CSV report written to: " << filename << std::endl;
}

void PerformanceMonitor::write_json_report(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open JSON file: " << filename << std::endl;
        return;
    }
    
    file << "{\n";
    file << "  \"timestamp\": \"" << std::chrono::system_clock::now().time_since_epoch().count() << "\",\n";
    file << "  \"counters\": [\n";
    
    // Write counter data
    std::lock_guard<std::mutex> lock(counters_mutex_);
    bool first = true;
    for (const auto& [name, counter] : counters_) {
        if (!first) file << ",\n";
        file << "    {\n";
        file << "      \"name\": \"" << name << "\",\n";
        file << "      \"type\": " << static_cast<int>(counter->type) << ",\n";
        file << "      \"current\": " << counter->value.load() << ",\n";
        file << "      \"min\": " << counter->min_value.load() << ",\n";
        file << "      \"max\": " << counter->max_value.load() << ",\n";
        file << "      \"average\": " << std::fixed << std::setprecision(2) << counter->get_average() << ",\n";
        file << "      \"count\": " << counter->count.load() << "\n";
        file << "    }";
        first = false;
    }
    
    file << "\n  ]\n";
    file << "}\n";
    
    file.close();
    std::cout << "JSON report written to: " << filename << std::endl;
}

} // namespace UltraFastAnalysis
