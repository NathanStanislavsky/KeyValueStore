# High-Throughput Key-Value Storage Engine (C++)

![C++](https://img.shields.io/badge/C++-17-blue.svg)

A persistent, log-structured database engine written from scratch in C++. It implements a **Log-Structured Merge (LSM) Tree** architecture to achieve high write throughput while maintaining data durability and read efficiency.

This project was engineered to explore database internals, specifically handling the trade-offs between write latency, memory usage, and disk I/O patterns.

## Key Features

* **LSM-Tree Architecture:** Optimizes for high-throughput write workloads by treating disk writes as append-only operations.
* **Persistence & Durability:** Implements a **Write-Ahead Log (WAL)** to ensure zero data loss in the event of a crash.
* **Crash Recovery:** Automated startup sequence rebuilds the in-memory state from the WAL and reconstructs Sparse Indices from disk.
* **Concurrency:** Thread-safe `MemTable` operations using `std::shared_mutex` (Read-Write Lock) to allow non-blocking reads during high write volume.
* **Sparse Indexing:** Maintains an in-memory sparse index to minimize disk seeks, reducing read complexity from $O(N)$ scan to $O(1)$ seek + small block scan.
* **SSTable Storage:** Automating flushing of in-memory data to immutable **Sorted String Tables (SSTables)** on disk.

## Architecture

The engine follows a standard layered storage hierarchy:

1.  **Write Path:**
    * Data is first appended to the **WAL** (disk) for durability.
    * Data is then inserted into the **MemTable** (Red-Black Tree in RAM) for sorting.
    * When the MemTable reaches a threshold (e.g., 4MB), it is flushed to an **SSTable** file.

2.  **Read Path:**
    * **Level 1:** Check MemTable (Fastest).
    * **Level 2:** Check SSTables in reverse chronological order.
    * **Optimization:** Uses a **Sparse Index** (Binary Search in RAM) to locate the specific disk block containing the key, avoiding full file scans.