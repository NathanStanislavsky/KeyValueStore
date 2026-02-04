#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <thread>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <atomic>
#include <mutex>
#include <filesystem>
#include "kvstore.h"

using namespace std;
using namespace std::chrono;

namespace fs = std::filesystem;
struct BenchmarkResult {
    double throughputOps;
    double p50_latency_us;
    double p99_latency_us;
    double p999_latency_us;
    double max_latency_us;
};

class Stats {
public:
    static BenchmarkResult calculate(const vector<double>& latencies_us, double duration_sec) {
        if (latencies_us.empty()) return {0, 0, 0, 0, 0};

        vector<double> sorted = latencies_us;
        std::sort(sorted.begin(), sorted.end());

        size_t n = sorted.size();
        return {
            n / duration_sec,                    // Throughput
            sorted[n * 0.50],                    // P50 (Median)
            sorted[n * 0.99],                    // P99
            sorted[n * 0.999],                   // P99.9
            sorted.back()                        // Max
        };
    }

    static void print(const string& name, const BenchmarkResult& res) {
        cout << left << setw(25) << name 
             << " | Ops/sec: " << right << setw(10) << fixed << setprecision(1) << res.throughputOps 
             << " | P50: " << setw(6) << res.p50_latency_us << "us"
             << " | P99: " << setw(6) << res.p99_latency_us << "us"
             << " | Max: " << setw(8) << res.max_latency_us << "us" << endl;
    }
};

class KeyGenerator {
public:
    static string getSequential(int i) {
        stringstream ss;
        ss << "key_" << setfill('0') << setw(8) << i;
        return ss.str();
    }

    static string getZipfian(int maxKeys, mt19937& gen) {
        uniform_int_distribution<> dist(0, 100);
        if (dist(gen) < 80) {
            uniform_int_distribution<> hotDist(0, maxKeys * 0.2);
            return getSequential(hotDist(gen));
        } else {
            uniform_int_distribution<> coldDist(maxKeys * 0.2 + 1, maxKeys - 1);
            return getSequential(coldDist(gen));
        }
    }
};

class BenchmarkSuite {
private:
    string dataDir;
    const int NUM_KEYS = 100000;
    const int VALUE_SIZE = 128;

    void resetEnvironment() {
        system(("rm -f " + dataDir + "/wal.log " + dataDir + "/*.sst " + dataDir + "/level_*_*.sst 2>/dev/null").c_str());
    }

    string randomString(size_t length) {
        string s(length, 'x');
        return s;
    }

public:
    BenchmarkSuite(string dir) : dataDir(dir) {}

    void runSequentialWrite(KVStore& store) {
        vector<double> latencies;
        latencies.reserve(NUM_KEYS);
        
        string val = randomString(VALUE_SIZE);

        auto start = high_resolution_clock::now();
        for (int i = 0; i < NUM_KEYS; i++) {
            auto t1 = high_resolution_clock::now();
            store.put(KeyGenerator::getSequential(i), val);
            auto t2 = high_resolution_clock::now();
            latencies.push_back(duration<double, micro>(t2 - t1).count());
        }
        auto end = high_resolution_clock::now();

        double duration_seconds = duration<double>(end - start).count();
        Stats::print("Sequential Write", Stats::calculate(latencies, duration_seconds));
    }

    void runRandomRead(KVStore& store) {
        vector<double> latencies;
        latencies.reserve(NUM_KEYS);

        mt19937 gen(42);
        uniform_int_distribution<> dis(0, NUM_KEYS - 1);

        auto start = high_resolution_clock::now();
        for (int i = 0; i < NUM_KEYS; i++) {
            string key = KeyGenerator::getSequential(dis(gen));
            
            auto t1 = high_resolution_clock::now();
            store.get(key);
            auto t2 = high_resolution_clock::now();
            
            latencies.push_back(duration<double, micro>(t2 - t1).count());
        }
        auto end = high_resolution_clock::now();

        double duration_seconds = duration<double>(end - start).count();
        Stats::print("Random Read (Uniform)", Stats::calculate(latencies, duration_seconds));
    }

    void runHotRead(KVStore& store) {
        vector<double> latencies;
        latencies.reserve(NUM_KEYS);

        mt19937 gen(42);

        auto start = high_resolution_clock::now();
        for (int i = 0; i < NUM_KEYS; i++) {
            string key = KeyGenerator::getZipfian(NUM_KEYS, gen);
            
            auto t1 = high_resolution_clock::now();
            store.get(key);
            auto t2 = high_resolution_clock::now();
            
            latencies.push_back(duration<double, micro>(t2 - t1).count());
        }
        auto end = high_resolution_clock::now();

        double duration_seconds = duration<double>(end - start).count();
        Stats::print("Hot/Cold Read (80/20)", Stats::calculate(latencies, duration_seconds));
    }

    void runConcurrentMixed(KVStore& store, int numThreads) {
        atomic<bool> startFlag{false};
        vector<thread> threads;
        vector<vector<double>> threadLatencies(numThreads);
        
        int opsPerThread = NUM_KEYS / numThreads;
        string val = randomString(VALUE_SIZE);

        for (int t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t]() {
                mt19937 gen(t + 100);
                uniform_int_distribution<> dis(0, NUM_KEYS - 1);
                uniform_int_distribution<> typeDis(0, 100);

                while (!startFlag);

                for (int i = 0; i < opsPerThread; i++) {
                    string key = KeyGenerator::getSequential(dis(gen));
                    
                    auto t1 = high_resolution_clock::now();
                    
                    if (typeDis(gen) < 90) {
                        store.get(key);
                    } else {
                        store.put(key, val);
                    }
                    
                    auto t2 = high_resolution_clock::now();
                    threadLatencies[t].push_back(duration<double, micro>(t2 - t1).count());
                }
            });
        }

        auto start = high_resolution_clock::now();
        startFlag = true;
        
        for (auto& t : threads) t.join();
        
        auto end = high_resolution_clock::now();

        vector<double> allLatencies;
        for (const auto& v : threadLatencies) {
            allLatencies.insert(allLatencies.end(), v.begin(), v.end());
        }

        double duration_seconds = duration<double>(end - start).count();
        string title = "Concurrent (" + to_string(numThreads) + " threads)";
        Stats::print(title, Stats::calculate(allLatencies, duration_seconds));
    }

    void runAll() {
        cout << "================================================================================" << endl;
        cout << "  ADVANCED KEY-VALUE STORE BENCHMARK" << endl;
        cout << "  (Keys: " << NUM_KEYS << ", ValSize: " << VALUE_SIZE << "B)" << endl;
        cout << "================================================================================" << endl;

        resetEnvironment();
        
        {
            KVStore store(dataDir + "/wal.log", dataDir);
            
            cout << "\n--- Phase 1: Population ---" << endl;
            runSequentialWrite(store);
            
            cout << "\n--- Phase 2: Read Patterns ---" << endl;
            runRandomRead(store);
            runHotRead(store);

            cout << "\n--- Phase 3: Concurrency & Contention ---" << endl;
            runConcurrentMixed(store, 4);
            runConcurrentMixed(store, 8);
        }

        cout << "================================================================================" << endl;
    }
};

int main() {
    BenchmarkSuite suite(".");
    suite.runAll();
    return 0;
}