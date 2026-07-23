#include "cpu_scheduler/cpu_scheduler.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <string_view>
#include <dirent.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

// ── Helpers ──

Result<unsigned long> read_ulong_from_file(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    return std::unexpected(Error{1, "Cannot open: " + path});
  }
  unsigned long val;
  f >> val;
  if (f.fail()) {
    return std::unexpected(Error{2, "Parse error: " + path});
  }
  return val;
}

Result<std::string> read_string_from_file(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    return std::unexpected(Error{1, "Cannot open: " + path});
  }
  std::string line;
  std::getline(f, line);
  return line;
}

// Trim trailing whitespace from a string view
static void trim_trailing(std::string_view& sv) {
  while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\n' || sv.back() == '\r'))
    sv.remove_suffix(1);
}

// Split key:value line (handles tab before colon like /proc/cpuinfo)
static std::pair<std::string_view, std::string_view> split_kv(std::string_view line) {
  auto pos = line.find(':');
  if (pos == std::string_view::npos) return {};
  auto key = line.substr(0, pos);
  trim_trailing(key);
  auto val = line.substr(pos + 1);
  while (!val.empty() && val.front() == ' ') val.remove_prefix(1);
  return {key, val};
}

// ── CPU Topology ──

Result<std::vector<CpuInfo>> read_cpu_info() {
  std::ifstream f("/proc/cpuinfo");
  if (!f.is_open()) {
    return std::unexpected(Error{1, "Cannot open /proc/cpuinfo"});
  }

  std::vector<CpuInfo> cpus;
  CpuInfo current{};
  std::string line;

  while (std::getline(f, line)) {
    if (line.empty()) {
      if (current.processor > 0 || !cpus.empty()) {
        cpus.push_back(current);
      }
      current = CpuInfo{};
      continue;
    }
    auto [key, val] = split_kv(line);
    if (key == "processor") current.processor = std::stoi(std::string(val));
    else if (key == "core id") current.core_id = std::stoi(std::string(val));
    else if (key == "physical id") current.physical_id = std::stoi(std::string(val));
    else if (key == "model name") current.model_name = std::string(val);
    else if (key == "siblings") current.siblings = std::stoi(std::string(val));
    else if (key == "cpu cores") current.cpu_cores = std::stoi(std::string(val));
  }
  if (!f.bad() && (current.processor > 0 || !cpus.empty())) {
    cpus.push_back(current);
  }

  return cpus;
}

// ── CPU Frequency Scaling ──

Result<std::vector<CpuFreq>> read_cpu_freq() {
  std::vector<CpuFreq> freqs;

  for (int cpu = 0; ; ++cpu) {
    std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/";
    if (!fs::exists(base)) break;

    CpuFreq f{};
    f.cpu = cpu;

    auto gov = read_string_from_file(base + "scaling_governor");
    if (gov) f.governor = *gov;

    auto cur = read_ulong_from_file(base + "scaling_cur_freq");
    if (cur) f.cur_freq_khz = *cur;

    auto min_f = read_ulong_from_file(base + "scaling_min_freq");
    if (min_f) f.min_freq_khz = *min_f;

    auto max_f = read_ulong_from_file(base + "scaling_max_freq");
    if (max_f) f.max_freq_khz = *max_f;

    freqs.push_back(f);
  }

  return freqs;
}

// ── CPU Usage from /proc/stat ──

Result<std::vector<CpuUsage>> read_cpu_usage() {
  std::ifstream f("/proc/stat");
  if (!f.is_open()) {
    return std::unexpected(Error{1, "Cannot open /proc/stat"});
  }

  std::vector<CpuUsage> usage;
  std::string line;

  while (std::getline(f, line)) {
    if (!line.starts_with("cpu")) break;
    std::istringstream ss(line);
    std::string label;
    ss >> label;

    CpuUsage u{};
    if (label == "cpu") u.cpu = -1;
    else u.cpu = std::stoi(label.substr(3));

    ss >> u.user >> u.nice >> u.system >> u.idle >> u.iowait >> u.irq >> u.softirq >> u.steal;
    usage.push_back(u);
  }

  return usage;
}

// ── Process Scheduler Info ──

[[maybe_unused]] static std::string read_proc_status_field(int pid, const std::string& field) {
  std::string path = "/proc/" + std::to_string(pid) + "/status";
  std::ifstream f(path);
  if (!f.is_open()) return {};
  std::string line;
  while (std::getline(f, line)) {
    auto [key, val] = split_kv(line);
    if (key == field) return std::string(val);
  }
  return {};
}

[[maybe_unused]] static std::string read_proc_cmdline(int pid) {
  std::string path = "/proc/" + std::to_string(pid) + "/cmdline";
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) return {};
  std::string buf;
  char c;
  while (f.get(c)) {
    if (c == '\0') buf += ' ';
    else buf += c;
  }
  while (!buf.empty() && buf.back() == ' ') buf.pop_back();
  return buf;
}

[[maybe_unused]] static unsigned long long read_proc_stat_field(int pid, int field_index) {
  std::string path = "/proc/" + std::to_string(pid) + "/stat";
  std::ifstream f(path);
  if (!f.is_open()) return 0;
  std::string line;
  std::getline(f, line);
  // Skip PID and comm (in parentheses)
  auto pos = line.rfind(')');
  if (pos == std::string::npos) return 0;
  std::string after = line.substr(pos + 2);  // skip ") "
  std::istringstream ss(after);
  std::vector<std::string> fields;
  std::string field;
  while (ss >> field) fields.push_back(field);

  if (field_index >= static_cast<int>(fields.size())) return 0;

  try {
    return std::stoull(fields[field_index]);
  } catch (...) {
    return 0;
  }
}

Result<std::vector<ProcSched>> read_process_sched_info(int max_pids) {
  std::vector<ProcSched> procs;

  DIR* dir = opendir("/proc");
  if (!dir) {
    return std::unexpected(Error{1, "Cannot open /proc"});
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr && static_cast<int>(procs.size()) < max_pids) {
    if (entry->d_type != DT_DIR) continue;
    int pid = 0;
    bool is_num = true;
    for (const char* p = entry->d_name; *p; ++p) {
      if (!std::isdigit(*p)) { is_num = false; break; }
    }
    if (!is_num) continue;
    pid = std::stoi(entry->d_name);

    ProcSched ps{};
    ps.pid = pid;

    // comm from /proc/pid/stat
    std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream sf(stat_path);
    if (!sf.is_open()) continue;
    std::string sline;
    std::getline(sf, sline);

    // Extract comm (between parentheses)
    auto cstart = sline.find('(');
    auto cend = sline.rfind(')');
    if (cstart == std::string::npos || cend == std::string::npos) continue;
    ps.name = sline.substr(cstart + 1, cend - cstart - 1);

    // After ") " we have state, ppid, pgrp, session, tty_nr, tpgid, flags, minflt, cminflt, majflt, cmajflt, utime, stime, ...
    std::string after = sline.substr(cend + 2);
    std::istringstream af(after);
    std::vector<std::string> fields;
    std::string f;
    while (af >> f) fields.push_back(f);

    // fields[0] = state
    if (fields.size() > 0 && !fields[0].empty()) ps.state = fields[0][0];
    // fields[17] = priority, fields[18] = nice
    if (fields.size() > 18) {
      try { ps.priority = std::stoi(fields[17]); } catch (...) {}
      try { ps.nice = std::stoi(fields[18]); } catch (...) {}
    }
    // fields[39] = policy (SCHED_*)
    if (fields.size() > 39) {
      try { ps.policy = std::stoul(fields[39]); } catch (...) {}
    }
    // fields[11] = utime, fields[12] = stime
    if (fields.size() > 12) {
      try { ps.cputime_ticks = std::stoull(fields[11]) + std::stoull(fields[12]); } catch (...) {}
    }
    // fields[41] = delayacct_blkio_ticks
    if (fields.size() > 41) {
      try { ps.delayacct_blkio_ticks = std::stoull(fields[41]); } catch (...) {}
    }

    procs.push_back(ps);
  }

  closedir(dir);
  return procs;
}

std::string ProcSched::policy_name() const {
  switch (policy) {
    case 0: return "SCHED_OTHER";
    case 1: return "SCHED_FIFO";
    case 2: return "SCHED_RR";
    case 3: return "SCHED_BATCH";
    case 5: return "SCHED_IDLE";
    case 6: return "SCHED_DEADLINE";
    default: return "SCHED_UNKNOWN(" + std::to_string(policy) + ")";
  }
}

unsigned long CpuUsage::total() const {
  return user + nice + system + idle + iowait + irq + softirq + steal;
}

double CpuUsage::usage_pct(unsigned long prev_total, unsigned long prev_idle) const {
  auto tot = total();
  auto idl = idle;
  if (tot == prev_total) return 0.0;
  return 100.0 * (1.0 - static_cast<double>(idl - prev_idle) / static_cast<double>(tot - prev_total));
}

// ── NUMA Topology ──

Result<std::vector<NumaNode>> read_numa_topology() {
  std::vector<NumaNode> nodes;

  for (int node = 0; ; ++node) {
    std::string base = "/sys/devices/system/node/node" + std::to_string(node);
    if (!fs::exists(base)) break;

    NumaNode nn{};
    nn.node_id = node;

    // Read CPU list
    auto cpulist = read_string_from_file(base + "/cpulist");
    if (cpulist) {
      // Parse simple list like "0-3,8-11"
      std::string_view sv(*cpulist);
      while (!sv.empty()) {
        auto comma = sv.find(',');
        auto part = sv.substr(0, comma);
        auto dash = part.find('-');
        if (dash != std::string_view::npos) {
          int lo = std::stoi(std::string(part.substr(0, dash)));
          int hi = std::stoi(std::string(part.substr(dash + 1)));
          for (int c = lo; c <= hi; ++c) nn.cpus.push_back(c);
        } else {
          nn.cpus.push_back(std::stoi(std::string(part)));
        }
        if (comma == std::string_view::npos) break;
        sv.remove_prefix(comma + 1);
      }
    }

    // Read memory info
    auto meminfo = read_string_from_file(base + "/meminfo");
    if (meminfo) {
      std::istringstream ms(*meminfo);
      std::string line;
      while (std::getline(ms, line)) {
        auto [key, val] = split_kv(line);
        std::string k(key);
        if (k == "MemTotal") {
          // Value is like "12345678 kB"
          std::istringstream vs{std::string(val)};
          vs >> nn.mem_total_kb;
        } else if (k == "MemFree") {
          std::istringstream vs2{std::string(val)};
          vs2 >> nn.mem_free_kb;
        }
      }
    }

    nodes.push_back(nn);
  }

  return nodes;
}
