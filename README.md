# High-Throughput Key-Value Storage Engine (C++)

![C++](https://img.shields.io/badge/C++-17-blue.svg)

A persistent key-value storage engine written from scratch in C++. It implements a **Log-Structured Merge (LSM) Tree** architecture with level compaction, thread safety, and crash recovery to achieve high write throughput while maintaining data durability and read efficiency.

This project demonstrates database internals, specifically handling the trade-offs between write latency, memory usage, disk I/O patterns, and concurrency.

## Key Features

* **LSM-Tree Architecture:** Optimizes for high-throughput write workloads by treating disk writes as append-only operations.
* **Level Compaction:** Implements tiered compaction with automatic merging of SSTables across levels to maintain sorted, non-overlapping files.
* **Persistence & Durability:** Implements a **Write-Ahead Log (WAL)** with rotation to ensure zero data loss in the event of a crash.
* **Crash Recovery:** Automated startup sequence rebuilds the in-memory state from the WAL and reconstructs level metadata from disk.
* **Thread Safety:** Full thread-safe operations using `std::shared_mutex` for concurrent reads and exclusive writes, with compaction state tracking to prevent race conditions.
* **Sparse Indexing:** Maintains an in-memory sparse index to minimize disk seeks, reducing read complexity from $O(N)$ scan to $O(1)$ seek + small block scan.
* **Bloom Filters:** Uses probabilistic data structures to quickly skip files that don't contain a key, reducing unnecessary disk I/O.
* **Streaming Merge:** K-way merge algorithm that processes data in streams, avoiding memory exhaustion for large datasets.
* **Tombstone Handling:** Proper deletion marker management with safe removal only at the bottom level.

## Architecture

The engine follows a standard LSM-tree storage hierarchy with multiple levels:

### Write Path

1. **WAL Write:** Data is first appended to the **WAL** (disk) for durability.
2. **MemTable Insert:** Data is then inserted into the **MemTable** (sorted map in RAM) for fast access.
3. **Flush to Level 0:** When the MemTable reaches a threshold (1000 entries), it is flushed to a **Level 0 SSTable** file.
4. **WAL Rotation:** The WAL is rotated (moved to `.tmp`) before flush, and the temp file is deleted only after the SSTable is safely written.
5. **Compaction Trigger:** If Level 0 has more than 4 files, compaction is automatically triggered.

### Read Path

1. **Level 1:** Check MemTable (fastest, O(log N)).
2. **Level 2:** Check Level 0 files in reverse chronological order (linear scan, files can overlap).
3. **Level 3+:** Check Level 1+ files using binary search (O(log N) per level, files are non-overlapping and sorted).
4. **Optimization:** Uses **Sparse Index** and **Bloom Filters** to minimize disk seeks and avoid unnecessary file reads.

### Compaction Strategy

The engine uses **level compaction** to merge and organize data:

- **Level 0:** Can have overlapping key ranges (from MemTable flushes). Threshold: 4 files.
- **Level 1+:** Non-overlapping, sorted files. Threshold: 10 files per level.
- **Compaction Process:**
  1. Identifies files to compact from current level
  2. Finds overlapping files in next level
  3. Performs K-way merge using a priority queue (min-heap)
  4. Streams merged data to new SSTable files (2MB max per file)
  5. Atomically updates metadata
  6. Deletes old files

### Thread Safety

- **Lock-Free Reads via Snapshotting:** Reads (`get()`) capture an in-memory snapshot of candidate SSTables using `std::shared_ptr` reference counting. This allows the slow disk I/O (`SSTable::search`) to occur completely lock-free, preventing reads from blocking background compactions or memtable flushes.
- **Double-Checked Locking for Flushes:** Concurrent `put()` operations use a dedicated `flush_mutex` and a double-checked locking pattern to ensure that only a single thread can trigger a WAL rotation and MemTable flush when the size threshold is reached, completely eliminating race conditions under heavy write loads.
- **Shared Mutex:** Protects the `levels` array during ultra-fast, in-memory structural modifications.
- **Active Compactions Tracking:** Prevents multiple threads from compacting the same level simultaneously.
- **Deferred File Deletion:** Files are only unlinked from the disk when their reference count drops to 0, ensuring that concurrent readers never encounter a missing file.

## Building and Running

### Prerequisites

- C++17 compatible compiler (GCC, Clang, or MSVC)
- CMake 3.10 or higher

### Build Instructions

```bash
# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# Run the test suite
./kv-server
```

### Project Structure

```
KeyValueStore/
├── include/
│   ├── kvstore.h          # Main KVStore class
│   ├── memtable.h          # In-memory table
│   ├── wal.h              # Write-ahead log
│   ├── sstable.h          # SSTable operations
│   ├── SSTableIterator.h  # Iterator for merging
│   └── bloomfilter.h      # Bloom filter implementation
├── src/
│   ├── kvstore.cpp        # Main implementation with compaction
│   ├── memtable.cpp       # MemTable implementation
│   ├── wal.cpp            # WAL with rotation
│   ├── sstable.cpp        # SSTable read/write
│   ├── SSTableIterator.cpp # Iterator implementation
│   ├── bloomfilter.cpp    # Bloom filter
│   └── main.cpp           # Test suite
├── CMakeLists.txt
└── README.md
```

## Implementation Highlights

### Streaming Compaction

The compaction algorithm uses a streaming approach to avoid memory exhaustion:

- **Priority Queue (Min-Heap):** Merges multiple sorted files efficiently
- **Batch Writing:** Writes data in 2MB batches to disk
- **No Full Buffering:** Never loads entire compaction result into memory
- **Duplicate Resolution:** Automatically keeps newest version when keys conflict

### Crash Recovery & Manifest Logging

To ensure absolutely zero data loss in the event of a crash, the engine uses a combination of WAL rotation and a Manifest file:

1. **Manifest State Tracking:** Tracks the exact state of the flush process (`ROTATED`, `SST_WRITTEN`, `COMMITTED`) using atomic file renames (`manifest.txt`).
2. **WAL Rotation:** Before a MemTable flush, the WAL is rotated (renamed to `.tmp`) and a fresh WAL is opened to accept incoming traffic without blocking.
3. **Bulletproof Startup Recovery:** On restart, the engine parses the Manifest and the file system. If an incomplete flush is detected (e.g., a `.tmp` WAL file exists but the state is not committed), it safely replays the data into the MemTable. This guarantees that data acknowledged to the user is never lost, even if a crash occurs milliseconds before a flush completes to an SSTable.

### Level Metadata Management

Each level maintains metadata for all SSTable files:
- Filename and path
- Sparse index (for fast lookups)
- Bloom filter (for quick negative checks)
- Key range (minKey, maxKey)
- File size

This metadata enables efficient binary search in Level 1+.

## Performance Characteristics

### Benchmarks (Tested on macOS)

**Sequential Write Performance:**
- Throughput: **~89,125 Ops/sec** 
- Median Latency (P50): **~6.4 μs**
- Tail Latency (P99): **~14.5 μs**

**Read Performance (Random/Uniform):**
- Throughput: **~12,273 Ops/sec**
- Median Latency (P50): **~79.6 μs**
- Tail Latency (P99): **~213.6 μs**

**Concurrent Contention (Mixed Read/Write, 8 Threads):**
- Throughput: **~20,691 Ops/sec**
- Median Latency (P50): **~46.8 μs**
- **Note:** Concurrent writes are currently bottlenecked by the WAL `std::fstream::flush()` holding a global lock.

**Complexity:**
- Write: O(log N) for MemTable insertion, O(1) amortized for disk writes
- Read Latency: 
  - MemTable: O(log N)
  - Level 0: O(N) linear scan (but with bloom filter optimization)
  - Level 1+: O(log N) binary search per level
- **Space Efficiency:** Automatic compaction reduces storage overhead
- **Concurrency:** Multiple readers, single writer per level

## Testing

### Functional Tests

The test suite (`main.cpp`) includes:

1. Basic Put/Get operations
2. Persistence (data survives restarts)
3. Compaction triggering (large writes)
4. Delete operations (tombstones)
5. Update operations (overwrites)
6. Non-existent key handling

Run functional tests:
```bash
cd build
./kv-server
```

### Performance Tests

The performance test suite (`performanceTest.cpp`) measures:
1. Write throughput for different key/value sizes
2. Read latency (average time per read)
3. Read throughput (reads per second)
4. Concurrent read performance (multi-threaded)
5. Compaction impact on write performance
6. Mixed workload performance (writes + reads)

Run performance tests:
```bash
cd build
./performance-test
```

## Design Decisions

### Why Level Compaction?

- Maintains sorted, non-overlapping files in higher levels
- Enables efficient binary search for reads
- Reduces read amplification compared to size-tiered compaction

### Why Streaming Merge?

- Handles datasets larger than available RAM
- Prevents out-of-memory errors during compaction
- Maintains constant memory usage regardless of data size

### Why WAL Rotation?

- Prevents data loss when new writes occur during MemTable flush
- Ensures durability without blocking concurrent operations
- Follows production database patterns (LevelDB, RocksDB)

## License

This project is for educational purposes.
