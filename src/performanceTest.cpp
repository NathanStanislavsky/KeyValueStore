#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <thread>
#include <atomic>
#include <iomanip>
#include <fstream>
#include "kvstore.h"

using namespace std;
using namespace std::chrono;

class SilentCout {
private:
    streambuf* originalCout;
    ofstream nullStream;
    
public:
    SilentCout() {
        originalCout = cout.rdbuf();
        nullStream.open("/dev/null");
        if (nullStream.is_open()) {
            cout.rdbuf(nullStream.rdbuf());
        }
    }
    
    ~SilentCout() {
        cout.rdbuf(originalCout);
        nullStream.close();
    }
};

class PerformanceTest
{
private:
    string dataDir;

    double measureWriteThroughput(int numKeys, int keySize, int valueSize)
    {
        system(("rm -f " + dataDir + "/wal.log " + dataDir + "/*.sst " + dataDir + "/level_*_*.sst 2>/dev/null").c_str());

        SilentCout silence;
        KVStore store(dataDir + "/wal.log", dataDir);

        auto start = high_resolution_clock::now();

        for (int i = 0; i < numKeys; i++)
        {
            string key = "key_" + string(keySize - 6, 'x') + to_string(i);
            string value = "value_" + string(valueSize - 7, 'x') + to_string(i);
            store.put(key, value);
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();

        return (numKeys * 1000.0) / duration;
    }

    double measureReadLatency(int numKeys, int numReads)
    {
        SilentCout silence;
        KVStore store(dataDir + "/wal.log", dataDir);

        vector<long long> latencies;
        latencies.reserve(numReads);

        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, numKeys - 1);

        for (int i = 0; i < numReads; i++)
        {
            int keyId = dis(gen);
            string key = "key_" + to_string(keyId);

            auto start = high_resolution_clock::now();
            auto result = store.get(key);
            auto end = high_resolution_clock::now();

            auto latency = duration_cast<microseconds>(end - start).count();
            latencies.push_back(latency);
        }

        long long sum = 0;
        for (auto lat : latencies)
        {
            sum += lat;
        }

        return sum / (double)latencies.size();
    }

    double measureReadThroughput(int numKeys, int numReads)
    {
        SilentCout silence;
        KVStore store(dataDir + "/wal.log", dataDir);

        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, numKeys - 1);

        auto start = high_resolution_clock::now();

        for (int i = 0; i < numReads; i++)
        {
            int keyId = dis(gen);
            string key = "key_" + to_string(keyId);
            store.get(key);
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();

        return (numReads * 1000.0) / duration;
    }

    void testConcurrentReads(int numKeys, int numThreads, int readsPerThread)
    {
        {
            SilentCout silence;
            KVStore store(dataDir + "/wal.log", dataDir);
            for (int i = 0; i < numKeys; i++)
            {
                store.put("key_" + to_string(i), "value_" + to_string(i));
            }
        }

        vector<thread> threads;
        atomic<int> totalReads(0);
        atomic<long long> totalLatency(0);

        auto start = high_resolution_clock::now();

        for (int t = 0; t < numThreads; t++)
        {
            threads.emplace_back([&, t]()
                                 {
                SilentCout silence;
                KVStore store(dataDir + "/wal.log", dataDir);
                random_device rd;
                mt19937 gen(rd() + t);
                uniform_int_distribution<> dis(0, numKeys - 1);
                
                for (int i = 0; i < readsPerThread; i++) {
                    int keyId = dis(gen);
                    string key = "key_" + to_string(keyId);
                    
                    auto readStart = high_resolution_clock::now();
                    store.get(key);
                    auto readEnd = high_resolution_clock::now();
                    
                    totalLatency += duration_cast<microseconds>(readEnd - readStart).count();
                    totalReads++;
                } });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();

        cout << "  Concurrent Reads (" << numThreads << " threads):" << endl;
        cout << "    Total reads: " << totalReads << endl;
        cout << "    Duration: " << duration << " ms" << endl;
        cout << "    Throughput: " << fixed << setprecision(2)
             << (totalReads * 1000.0 / duration) << " reads/sec" << endl;
        cout << "    Avg latency: " << (totalLatency / (double)totalReads)
             << " μs" << endl;
    }

public:
    PerformanceTest(const string &dir = ".") : dataDir(dir) {}

    void runAllTests()
    {
        cout << "========================================" << endl;
        cout << "  KEY-VALUE STORE PERFORMANCE TESTS" << endl;
        cout << "========================================" << endl
             << endl;

        cout << "1. WRITE THROUGHPUT TEST" << endl;
        cout << "   Writing 10,000 keys (small: 10 bytes key, 20 bytes value)..." << endl;
        double smallWrite = measureWriteThroughput(10000, 10, 20);
        cout << "   Result: " << fixed << setprecision(2) << smallWrite
             << " writes/sec" << endl
             << endl;

        cout << "   Writing 5,000 keys (medium: 50 bytes key, 100 bytes value)..." << endl;
        double mediumWrite = measureWriteThroughput(5000, 50, 100);
        cout << "   Result: " << fixed << setprecision(2) << mediumWrite
             << " writes/sec" << endl
             << endl;

        cout << "2. READ LATENCY TEST" << endl;
        cout << "   Measuring average read latency (10,000 random reads)..." << endl;
        double avgLatency = measureReadLatency(10000, 10000);
        cout << "   Result: " << fixed << setprecision(2) << avgLatency
             << " μs per read" << endl
             << endl;

        cout << "3. READ THROUGHPUT TEST" << endl;
        cout << "   Measuring read throughput (50,000 random reads)..." << endl;
        double readThroughput = measureReadThroughput(10000, 50000);
        cout << "   Result: " << fixed << setprecision(2) << readThroughput
             << " reads/sec" << endl
             << endl;

        cout << "4. CONCURRENT READ TEST" << endl;
        testConcurrentReads(10000, 4, 5000);
        cout << endl;

        cout << "5. COMPACTION IMPACT TEST" << endl;
        cout << "   Writing 15,000 keys to trigger multiple compactions..." << endl;
        auto start = high_resolution_clock::now();
        {
            SilentCout silence;
            KVStore store(dataDir + "/wal.log", dataDir);
            for (int i = 0; i < 15000; i++)
            {
                store.put("key_" + to_string(i), "value_" + to_string(i));
            }
        }
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();
        cout << "   Duration: " << duration << " ms" << endl;
        cout << "   Throughput: " << fixed << setprecision(2)
             << (15000 * 1000.0 / duration) << " writes/sec" << endl;
        cout << "   (Includes compaction overhead)" << endl
             << endl;

        cout << "6. MIXED WORKLOAD TEST" << endl;
        long long mixedDuration;
        {
            SilentCout silence;
            KVStore store(dataDir + "/wal.log", dataDir);

            auto start = high_resolution_clock::now();

            for (int i = 0; i < 5000; i++)
            {
                store.put("mixed_key_" + to_string(i), "mixed_value_" + to_string(i));
            }

            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<> dis(0, 4999);
            for (int i = 0; i < 1000; i++)
            {
                int keyId = dis(gen);
                store.get("mixed_key_" + to_string(keyId));
            }

            auto end = high_resolution_clock::now();
            mixedDuration = duration_cast<milliseconds>(end - start).count();
        }
        cout << "   Mixed workload (5,000 writes + 1,000 reads): " << mixedDuration
             << " ms" << endl;
        cout << endl;

        cout << "========================================" << endl;
        cout << "  PERFORMANCE TESTS COMPLETE" << endl;
        cout << "========================================" << endl;
    }
};

int main()
{
    PerformanceTest test(".");
    test.runAllTests();
    return 0;
}
