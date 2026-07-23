#include "cpu_scheduler/formatters.hpp"
#include <format>
#include <iomanip>
#include <sstream>

// ── Table formatters ──

static std::string separator_line() {
  return std::string(72, '-') + '\n';
}

std::string fmt_cpu_info_table(const std::vector<CpuInfo>& info) {
  if (info.empty()) return "  No CPU info available\n";

  std::string out;
  out += std::format("  {:>4}  {:>4}  {:>4}  {:>4}  {:>4}  {}\n",
                     "CPU", "Core", "Pkg", "Sibl", "Cores", "Model");
  out += separator_line();

  for (auto& c : info) {
    out += std::format("  {:>4}  {:>4}  {:>4}  {:>4}  {:>4}  {}\n",
                       c.processor, c.core_id, c.physical_id,
                       c.siblings, c.cpu_cores, c.model_name);
  }
  return out;
}

std::string fmt_cpu_freq_table(const std::vector<CpuFreq>& freq) {
  if (freq.empty()) return "  No CPU frequency info available (cpufreq may not be enabled)\n";

  std::string out;
  out += std::format("  {:>4}  {:>14}  {:>10}  {:>10}  {:>10}  {}\n",
                     "CPU", "Governor", "Cur (MHz)", "Min (MHz)", "Max (MHz)", "");
  out += separator_line();

  for (auto& f : freq) {
    out += std::format("  {:>4}  {:>14}  {:>10.1f}  {:>10.1f}  {:>10.1f}\n",
                       f.cpu, f.governor,
                       f.cur_freq_khz / 1000.0,
                       f.min_freq_khz / 1000.0,
                       f.max_freq_khz / 1000.0);
  }
  return out;
}

std::string fmt_cpu_usage_table(const std::vector<CpuUsage>& usage) {
  if (usage.empty()) return "  No CPU usage data available\n";

  std::string out;
  out += std::format("  {:>6}  {:>8}  {:>8}  {:>8}  {:>8}  {:>8}  {:>8}  {:>6}\n",
                     "CPU", "user", "nice", "sys", "idle", "iowait", "irq", "steal");
  out += separator_line();

  for (auto& u : usage) {
    std::string cpu_label = (u.cpu == -1) ? "total" : std::to_string(u.cpu);
    out += std::format("  {:>6}  {:>8}  {:>8}  {:>8}  {:>8}  {:>8}  {:>8}  {:>6}\n",
                       cpu_label, u.user, u.nice, u.system, u.idle,
                       u.iowait, u.irq, u.softirq, u.steal);
  }
  return out;
}

std::string fmt_process_sched_table(const std::vector<ProcSched>& procs) {
  if (procs.empty()) return "  No process info available\n";

  std::string out;
  out += std::format("  {:>6}  {:>20}  {:>4}  {:>3}  {:>4}  {:>16}  {:>10}\n",
                     "PID", "NAME", "STA", "NICE", "PRI", "POLICY", "CPU(ticks)");
  out += separator_line();

  for (auto& p : procs) {
    out += std::format("  {:>6}  {:>20}  {:>4}  {:>3}  {:>4}  {:>16}  {:>10}\n",
                       p.pid, p.name, p.state, p.nice, p.priority,
                       p.policy_name(), p.cputime_ticks);
  }
  return out;
}

std::string fmt_numa_table(const std::vector<NumaNode>& nodes) {
  if (nodes.empty()) return "  No NUMA info available\n";

  std::string out;
  out += std::format("  {:>6}  {:>30}  {:>12}  {:>12}\n",
                     "Node", "CPUs", "MemTotal(MB)", "MemFree(MB)");
  out += separator_line();

  for (auto& n : nodes) {
    std::string cpu_str;
    for (size_t i = 0; i < n.cpus.size(); ++i) {
      if (i > 0) cpu_str += ",";
      cpu_str += std::to_string(n.cpus[i]);
    }
    out += std::format("  {:>6}  {:>30}  {:>12.1f}  {:>12.1f}\n",
                       n.node_id, cpu_str,
                       n.mem_total_kb / 1024.0,
                       n.mem_free_kb / 1024.0);
  }
  return out;
}

std::string fmt_policy_legend() {
  return
    "  Scheduling Policy Legend:\n"
    "    SCHED_OTHER   (0)  - Default time-sharing policy\n"
    "    SCHED_FIFO    (1)  - First In, First Out (real-time)\n"
    "    SCHED_RR      (2)  - Round Robin (real-time)\n"
    "    SCHED_BATCH   (3)  - Batch scheduling\n"
    "    SCHED_IDLE    (5)  - Idle scheduling\n"
    "    SCHED_DEADLINE(6)  - Deadline scheduling\n"
    "  State: R=running, S=sleeping, D=disk sleep, Z=zombie, T=stopped\n";
}

// ── JSON formatters ──

json to_json(const std::vector<CpuInfo>& info) {
  json arr = json::array();
  for (auto& c : info) {
    arr.push_back({
      {"processor", c.processor},
      {"core_id", c.core_id},
      {"physical_id", c.physical_id},
      {"model_name", c.model_name},
      {"siblings", c.siblings},
      {"cpu_cores", c.cpu_cores}
    });
  }
  return arr;
}

json to_json(const std::vector<CpuFreq>& freq) {
  json arr = json::array();
  for (auto& f : freq) {
    arr.push_back({
      {"cpu", f.cpu},
      {"governor", f.governor},
      {"cur_freq_mhz", f.cur_freq_khz / 1000.0},
      {"min_freq_mhz", f.min_freq_khz / 1000.0},
      {"max_freq_mhz", f.max_freq_khz / 1000.0}
    });
  }
  return arr;
}

json to_json(const std::vector<CpuUsage>& usage) {
  json arr = json::array();
  for (auto& u : usage) {
    arr.push_back({
      {"cpu", u.cpu == -1 ? "total" : std::to_string(u.cpu)},
      {"user", u.user},
      {"nice", u.nice},
      {"system", u.system},
      {"idle", u.idle},
      {"iowait", u.iowait},
      {"irq", u.irq},
      {"softirq", u.softirq},
      {"steal", u.steal}
    });
  }
  return arr;
}

json to_json(const std::vector<ProcSched>& procs) {
  json arr = json::array();
  for (auto& p : procs) {
    arr.push_back({
      {"pid", p.pid},
      {"name", p.name},
      {"state", std::string(1, p.state)},
      {"nice", p.nice},
      {"priority", p.priority},
      {"policy", p.policy_name()},
      {"cputime_ticks", p.cputime_ticks},
      {"delayacct_blkio_ticks", p.delayacct_blkio_ticks}
    });
  }
  return arr;
}

json to_json(const std::vector<NumaNode>& nodes) {
  json arr = json::array();
  for (auto& n : nodes) {
    arr.push_back({
      {"node_id", n.node_id},
      {"cpus", n.cpus},
      {"mem_total_mb", n.mem_total_kb / 1024.0},
      {"mem_free_mb", n.mem_free_kb / 1024.0}
    });
  }
  return arr;
}
