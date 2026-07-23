#include "cpu_scheduler/cpu_scheduler.hpp"
#include "cpu_scheduler/formatters.hpp"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <print>
#include <thread>

int main(int argc, char** argv) {
  CLI::App app{"CPU Scheduler Analyzer — Linux CPU topology, frequency scaling, process scheduling, and NUMA info"};

  bool show_all = true;
  bool show_cpu = false;
  bool show_freq = false;
  bool show_usage = false;
  bool show_sched = false;
  bool show_numa = false;
  bool show_json = false;
  bool show_legend = false;
  int max_pids = 20;
  std::string log_level = "warn";

  app.add_flag("-a,--all", show_all, "Show all info (default)");
  app.add_flag("--cpu", show_cpu, "Show CPU topology only");
  app.add_flag("--freq", show_freq, "Show CPU frequency scaling only");
  app.add_flag("--usage", show_usage, "Show CPU usage from /proc/stat only");
  app.add_flag("--sched", show_sched, "Show process scheduling info only");
  app.add_flag("--numa", show_numa, "Show NUMA topology only");
  app.add_flag("--json,-j", show_json, "Output in JSON format");
  app.add_flag("--legend,-l", show_legend, "Show scheduling policy legend");
  app.add_option("--max-pids", max_pids, "Max PIDs to show in sched view (default: 20)");
  app.add_option("--log-level", log_level, "Log level: trace, debug, info, warn, error");

  CLI11_PARSE(app, argc, argv);

  // Set log level
  if (log_level == "trace") spdlog::set_level(spdlog::level::trace);
  else if (log_level == "debug") spdlog::set_level(spdlog::level::debug);
  else if (log_level == "info") spdlog::set_level(spdlog::level::info);
  else if (log_level == "warn") spdlog::set_level(spdlog::level::warn);
  else if (log_level == "error") spdlog::set_level(spdlog::level::err);

  // When specific flags are given, disable --all
  if (show_cpu || show_freq || show_usage || show_sched || show_numa || show_legend) {
    show_all = false;
  }

  if (show_legend) {
    std::cout << fmt_policy_legend();
    return 0;
  }

  json output_json;
  bool any_data = false;

  // ── CPU Topology ──
  if (show_all || show_cpu) {
    spdlog::debug("Reading CPU topology...");
    auto result = read_cpu_info();
    if (result) {
      if (show_json) {
        output_json["cpu_topology"] = to_json(*result);
      } else {
        std::cout << "\n  ═══ CPU Topology ═══\n\n";
        std::cout << fmt_cpu_info_table(*result);
      }
      any_data = true;
    } else {
      spdlog::warn("CPU topology: {}", result.error().message);
    }
  }

  // ── CPU Frequency ──
  if (show_all || show_freq) {
    spdlog::debug("Reading CPU frequency scaling...");
    auto result = read_cpu_freq();
    if (result) {
      if (show_json) {
        output_json["cpu_frequency"] = to_json(*result);
      } else {
        std::cout << "\n  ═══ CPU Frequency Scaling ═══\n\n";
        std::cout << fmt_cpu_freq_table(*result);
      }
      any_data = true;
    } else {
      spdlog::warn("CPU frequency: {}", result.error().message);
    }
  }

  // ── CPU Usage ──
  if (show_all || show_usage) {
    spdlog::debug("Reading CPU usage...");
    auto result = read_cpu_usage();
    if (result) {
      if (show_json) {
        output_json["cpu_usage"] = to_json(*result);
      } else {
        std::cout << "\n  ═══ CPU Usage (/proc/stat) ═══\n\n";
        std::cout << fmt_cpu_usage_table(*result);
      }
      any_data = true;
    } else {
      spdlog::warn("CPU usage: {}", result.error().message);
    }
  }

  // ── Process Scheduling ──
  if (show_all || show_sched) {
    spdlog::debug("Reading process scheduling info...");
    auto result = read_process_sched_info(max_pids);
    if (result) {
      if (show_json) {
        output_json["process_scheduling"] = to_json(*result);
      } else {
        std::cout << "\n  ═══ Process Scheduling (top " << max_pids << " PIDs) ═══\n\n";
        std::cout << fmt_process_sched_table(*result);
      }
      any_data = true;
    } else {
      spdlog::warn("Process scheduling: {}", result.error().message);
    }
  }

  // ── NUMA Topology ──
  if (show_all || show_numa) {
    spdlog::debug("Reading NUMA topology...");
    auto result = read_numa_topology();
    if (result) {
      if (show_json) {
        output_json["numa_topology"] = to_json(*result);
      } else {
        std::cout << "\n  ═══ NUMA Topology ═══\n\n";
        std::cout << fmt_numa_table(*result);
      }
      any_data = true;
    } else {
      spdlog::warn("NUMA topology: {}", result.error().message);
    }
  }

  // Output JSON
  if (show_json) {
    if (any_data) {
      std::cout << output_json.dump(2) << '\n';
    } else {
      std::cout << "{}\n";
    }
  }

  if (show_all && !show_json) {
    std::cout << "\n  ── Legend ──\n";
    std::cout << fmt_policy_legend();
  }

  return 0;
}
