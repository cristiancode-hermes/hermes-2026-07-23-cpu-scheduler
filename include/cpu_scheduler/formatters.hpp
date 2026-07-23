#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "cpu_scheduler.hpp"

using json = nlohmann::json;

// ── Formatters ──
std::string fmt_cpu_info_table(const std::vector<CpuInfo>& info);
std::string fmt_cpu_freq_table(const std::vector<CpuFreq>& freq);
std::string fmt_cpu_usage_table(const std::vector<CpuUsage>& usage);
std::string fmt_process_sched_table(const std::vector<ProcSched>& procs);
std::string fmt_numa_table(const std::vector<NumaNode>& nodes);
std::string fmt_policy_legend();

// ── JSON formatters ──
json to_json(const std::vector<CpuInfo>& info);
json to_json(const std::vector<CpuFreq>& freq);
json to_json(const std::vector<CpuUsage>& usage);
json to_json(const std::vector<ProcSched>& procs);
json to_json(const std::vector<NumaNode>& nodes);
