# Architecture: CPU Scheduler Analyzer

## Data Flow

```
/proc/cpuinfo ──────────┐
/sys/devices/system/cpu/ ├─→ read_cpu_info() ──→ CpuInfo[]
/sys/class/cpufreq/ ────┤
                         ├─→ read_cpu_freq() ──→ CpuFreq[]
/proc/stat ─────────────┤
                         ├─→ read_cpu_usage() ─→ CpuUsage[]
/proc/[pid]/stat ───────┤
                         ├─→ read_process_sched_info() ─→ ProcSched[]
/sys/devices/system/node/┤
                         ├─→ read_numa_topology() ─→ NumaNode[]
                         │
                         ▼
                  formatters.cpp
                     │    │
                     ▼    ▼
                  Table  JSON
```

## Module Dependency

```
main.cpp
  ├── CLI11 (argument parsing)
  ├── collector.cpp → cpu_scheduler.hpp
  │     ├── /proc/*
  │     └── /sys/*
  └── formatters.cpp → formatters.hpp
        ├── nlohmann/json
        └── <format>
```

## Error Handling

All system readers return `Result<T>` = `std::expected<T, Error>`.

- `Error{1, "Cannot open: path"}` — file not found or permission denied
- `Error{2, "Parse error: path"}` — file opened but content unparseable

Tests treat `/proc`/`/sys` access as conditional (`result.has_value()` with INFO), not mandatory.

## Design Decisions

1. **Library + Executable pattern**: avoids `main()` conflict in tests
2. **FetchContent** for dependencies: no vcpkg/Conan required
3. **C++23**: uses `std::expected`, `std::println`, `std::format`
4. **CLI11** for argument parsing: modern, header-only, composable flags
5. **Catch2 v3** for testing: BDD-style, benchmark support
