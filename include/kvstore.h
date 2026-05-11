#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>

#include "bloomfilter.h"
#include "memtable.h"
#include "sstable.h"
#include "wal.h"

struct SSTableMetadata {
  std::string filename;
  std::vector<IndexEntry> index;
  BloomFilter bloomFilter;

  int fileId;
  std::string minKey;
  std::string maxKey;
  long fileSize;

  bool markedForDeletion = false;

  ~SSTableMetadata() {
    if (markedForDeletion && std::filesystem::exists(filename)) {
      std::filesystem::remove(filename);
    }
  }

  SSTableMetadata() = default;
  SSTableMetadata(const SSTableMetadata &) = delete;
  SSTableMetadata &operator=(const SSTableMetadata &) = delete;

  bool operator<(const SSTableMetadata &other) const {
    return filename < other.filename;
  }
};

enum class FlushState { NONE, ROTATED, SST_WRITTEN, COMMITTED };

struct FlushManifest {
  uint64_t flushId = 0;
  FlushState state = FlushState::NONE;
  std::string tmpWal;
  std::string sstFile;
  bool valid = true;
};

class KVStore {
public:
  KVStore(const std::string &filename, const std::string &directory);

  void put(const std::string &key, const std::string &value);

  std::optional<std::string> get(const std::string &key) const;

  void remove(const std::string &key);

private:
  std::unique_ptr<MemTable> memtable;
  std::unique_ptr<WAL> wal;
  std::vector<std::vector<std::shared_ptr<SSTableMetadata>>> levels;
  std::string data_directory;
  mutable std::shared_mutex levels_mutex;
  std::set<int> active_compactions;
  std::string wal_filename;
  std::string manifest_path;
  uint64_t current_flush_id = 0;
  std::mutex flush_mutex;

  void checkCompactionStatus();
  void compact(int level);
  void loadSSTables();
  std::string generateSSTableFilename(int level, int file_id);

  static FlushManifest readManifest(const std::string &manifestPath);
  static FlushState parseFlushState(const std::string &value, bool &ok);
  static std::string flushStateToString(FlushState s);
  static void writeManifestAtomically(const std::string &manifestPath,
                                      const FlushManifest &m);
};