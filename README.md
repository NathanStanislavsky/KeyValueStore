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

- **Shared Mutex:** Protects the `levels` data structure
  - Shared lock: Multiple concurrent readers
  - Exclusive lock: Single writer (compaction or flush)
- **Active Compactions Tracking:** Prevents multiple threads from compacting the same level simultaneously
- **Lock Minimization:** Heavy I/O operations (file reading/writing) happen without locks held

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

### WAL Rotation

To prevent data loss during concurrent writes:

1. Before MemTable flush: Rotate WAL (rename to `.tmp`, open new WAL)
2. Flush MemTable: Write SSTable to disk
3. After SSTable written: Delete `.tmp` file

This ensures new writes during flush go to a new WAL file, preventing data loss.

### Level Metadata Management

Each level maintains metadata for all SSTable files:
- Filename and path
- Sparse index (for fast lookups)
- Bloom filter (for quick negative checks)
- Key range (minKey, maxKey)
- File size

This metadata enables efficient binary search in Level 1+.

## Testing

The test suite (`main.cpp`) includes:

1. Basic Put/Get operations
2. Persistence (data survives restarts)
3. Compaction triggering (large writes)
4. Delete operations (tombstones)
5. Update operations (overwrites)
6. Non-existent key handling

Run tests:
```bash
./kv-server
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
