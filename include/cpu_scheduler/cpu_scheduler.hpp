#pragma once

#include <expected>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Error {
  int code;
  std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

// ── CPU Topology ──
struct CpuInfo {
  int processor;
  int core_id;
  int physical_id;
  std::string model_name;
  int siblings;
  int cpu_cores;
};

Result<std::vector<CpuInfo>> read_cpu_info();

// ── CPU Frequency Scaling ──
struct CpuFreq {
  int cpu;
  std::string governor;
  unsigned long cur_freq_khz;
  unsigned long min_freq_khz;
  unsigned long max_freq_khz;
};

Result<std::vector<CpuFreq>> read_cpu_freq();

// ── CPU Usage from /proc/stat ──
struct CpuUsage {
  int cpu;  // -1 for total
  unsigned long user;
  unsigned long nice;
  unsigned long system;
  unsigned long idle;
  unsigned long iowait;
  unsigned long irq;
  unsigned long softirq;
  unsigned long steal;

  [[nodiscard]] unsigned long total() const;
  [[nodiscard]] double usage_pct(unsigned long prev_total, unsigned long prev_idle) const;
};

Result<std::vector<CpuUsage>> read_cpu_usage();

// ── Process Scheduler Info ──
struct ProcSched {
  int pid;
  std::string name;
  char state;
  int priority;
  int nice;
  unsigned long policy;  // SCHED_OTHER=0, SCHED_FIFO=1, SCHED_RR=2, SCHED_BATCH=3, SCHED_IDLE=5
  unsigned long long cputime_ticks;
  unsigned long long delayacct_blkio_ticks;

  [[nodiscard]] std::string policy_name() const;
};

Result<std::vector<ProcSched>> read_process_sched_info(int max_pids = 20);

// ── NUMA Topology ──
struct NumaNode {
  int node_id;
  std::vector<int> cpus;
  unsigned long mem_total_kb;
  unsigned long mem_free_kb;
};

Result<std::vector<NumaNode>> read_numa_topology();

// ── Helpers ──
Result<unsigned long> read_ulong_from_file(const std::string& path);
Result<std::string> read_string_from_file(const std::string& path);
