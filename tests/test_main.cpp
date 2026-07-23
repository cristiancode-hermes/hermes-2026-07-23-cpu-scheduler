#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "cpu_scheduler/cpu_scheduler.hpp"
#include "cpu_scheduler/formatters.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ── CPU Topology ──

TEST_CASE("Read CPU topology", "[cpu][integration]") {
  auto result = read_cpu_info();
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->empty());

  // Should have at least 1 CPU
  REQUIRE(result->size() >= 1);

  // Check first CPU has reasonable values
  auto& cpu = result->at(0);
  REQUIRE(cpu.processor >= 0);
  REQUIRE(cpu.cpu_cores >= 1);
  REQUIRE_FALSE(cpu.model_name.empty());
}

TEST_CASE("CPU topology JSON roundtrip", "[cpu][json]") {
  auto result = read_cpu_info();
  REQUIRE(result.has_value());

  json j = to_json(*result);
  REQUIRE(j.is_array());
  REQUIRE(j.size() == result->size());

  if (!result->empty()) {
    REQUIRE(j[0].contains("processor"));
    REQUIRE(j[0]["model_name"].is_string());
  }
}

// ── CPU Frequency ──

TEST_CASE("Read CPU frequency scaling", "[freq][integration]") {
  // This test may fail if cpufreq is not enabled (container/VM)
  auto result = read_cpu_freq();
  if (result.has_value()) {
    if (!result->empty()) {
      auto& f = result->at(0);
      REQUIRE(f.cpu >= 0);
      // Governor should be a known value
      REQUIRE_FALSE(f.governor.empty());
    }
  }
  // If no cpufreq, it's OK — skip without failure
  // Note: REQUIRE would be too strict; just report
  INFO("CPU freq data available: " << result.has_value());
}

TEST_CASE("CPU frequency JSON", "[freq][json]") {
  auto result = read_cpu_freq();
  if (result.has_value() && !result->empty()) {
    json j = to_json(*result);
    REQUIRE(j.is_array());
    REQUIRE(j[0].contains("governor"));
    REQUIRE(j[0].contains("cur_freq_mhz"));
  }
}

// ── CPU Usage ──

TEST_CASE("Read CPU usage", "[usage][integration]") {
  auto result = read_cpu_usage();
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->empty());

  // Should have at least 2 entries (total + 1 CPU)
  REQUIRE(result->size() >= 2);

  // First entry should be total (cpu == -1)
  REQUIRE(result->at(0).cpu == -1);

  // Values should be non-zero
  REQUIRE(result->at(0).total() > 0);
}

TEST_CASE("CPU usage percentages", "[usage][math]") {
  auto result = read_cpu_usage();
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->empty());

  auto& total = result->at(0);
  // Simulate a "previous" reading
  unsigned long prev_total = total.total();
  unsigned long prev_idle = total.idle;

  // If no change, usage is 0
  double pct = total.usage_pct(prev_total, prev_idle);
  REQUIRE(pct == Catch::Approx(0.0).margin(0.001));
}

// ── Process Scheduling ──

TEST_CASE("Read process scheduling info", "[sched][integration]") {
  auto result = read_process_sched_info(10);
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->empty());

  // PID 1 (init/systemd) should exist
  bool found_pid1 = false;
  for (auto& p : *result) {
    if (p.pid == 1) {
      found_pid1 = true;
      REQUIRE_FALSE(p.name.empty());
      // PID 1 should have a valid state
      REQUIRE((p.state == 'S' || p.state == 'R' || p.state == 'D'));
      break;
    }
  }
  REQUIRE(found_pid1);
}

TEST_CASE("Process scheduling JSON", "[sched][json]") {
  auto result = read_process_sched_info(5);
  REQUIRE(result.has_value());

  json j = to_json(*result);
  REQUIRE(j.is_array());
  if (!result->empty()) {
    REQUIRE(j[0].contains("pid"));
    REQUIRE(j[0].contains("policy"));
    REQUIRE(j[0].contains("name"));
  }
}

// ── NUMA Topology ──

TEST_CASE("Read NUMA topology", "[numa][integration]") {
  // NUMA may not be available on all systems
  auto result = read_numa_topology();
  if (result.has_value()) {
    if (!result->empty()) {
      REQUIRE(result->at(0).node_id >= 0);
      REQUIRE_FALSE(result->at(0).cpus.empty());
    }
  }
  INFO("NUMA data available: " << result.has_value());
}

// ── Formatters ──

TEST_CASE("Policy legend is non-empty", "[fmt]") {
  auto legend = fmt_policy_legend();
  REQUIRE_FALSE(legend.empty());
  REQUIRE(legend.find("SCHED_OTHER") != std::string::npos);
  REQUIRE(legend.find("SCHED_FIFO") != std::string::npos);
}

TEST_CASE("Format CPU info table", "[fmt]") {
  auto info = read_cpu_info();
  REQUIRE(info.has_value());
  auto table = fmt_cpu_info_table(*info);
  REQUIRE_FALSE(table.empty());
  REQUIRE(table.find("Model") != std::string::npos);
}

TEST_CASE("Format CPU usage table", "[fmt]") {
  auto usage = read_cpu_usage();
  REQUIRE(usage.has_value());
  auto table = fmt_cpu_usage_table(*usage);
  REQUIRE_FALSE(table.empty());
  REQUIRE(table.find("total") != std::string::npos);
}

TEST_CASE("Format process sched table", "[fmt]") {
  auto procs = read_process_sched_info(5);
  REQUIRE(procs.has_value());
  auto table = fmt_process_sched_table(*procs);
  REQUIRE_FALSE(table.empty());
  REQUIRE(table.find("PID") != std::string::npos);
}

// ── Error handling ──

TEST_CASE("Read nonexistent file returns error", "[error]") {
  auto result = read_ulong_from_file("/proc/nonexistent_file_xyz");
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().code > 0);
}

// ── Policy name ──

TEST_CASE("Policy name mapping", "[sched]") {
  ProcSched p{};
  p.policy = 0;
  REQUIRE(p.policy_name() == "SCHED_OTHER");
  p.policy = 1;
  REQUIRE(p.policy_name() == "SCHED_FIFO");
  p.policy = 2;
  REQUIRE(p.policy_name() == "SCHED_RR");
  p.policy = 3;
  REQUIRE(p.policy_name() == "SCHED_BATCH");
  p.policy = 5;
  REQUIRE(p.policy_name() == "SCHED_IDLE");
  p.policy = 6;
  REQUIRE(p.policy_name() == "SCHED_DEADLINE");
  p.policy = 99;
  REQUIRE(p.policy_name().find("SCHED_UNKNOWN") != std::string::npos);
}
