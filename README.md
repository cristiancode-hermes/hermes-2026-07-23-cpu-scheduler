# CPU Scheduler Analyzer

Analizador de CPU y planificador de procesos para Linux en C++23. Inspecciona la topología de CPU, frecuencias y gobernadores de scaling, uso de CPU por núcleo, políticas de planificación de procesos y topología NUMA.

## 🚀 Características

- **Topología CPU**: modelo, cores, siblings, paquete físico
- **Frecuencia CPU**: governor actual, frecuencias min/max/actual de cada núcleo
- **Uso CPU**: lecturas de `/proc/stat` por núcleo y total
- **Planificación de procesos**: política `SCHED_OTHER/FIFO/RR/BATCH/IDLE/DEADLINE`, prioridad, nice, estado
- **Topología NUMA**: nodos, CPUs asociadas, memoria total/libre
- **Salida JSON**: flag `--json` para integración con scripts
- **CLI completa**: flags de selección por categoría

## 🛠️ Requisitos

- Linux (lectura de `/proc` y `/sys`)
- GCC 14+ o Clang 18+ (C++23)
- CMake 3.28+
- Ninja (opcional, recomendado)

## 📦 Compilación

```bash
# Con Ninja (recomendado)
cmake --preset debug
cmake --build --preset debug

# Con Make
cmake -G "Unix Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 🎯 Uso

```bash
# Mostrar toda la información
./build/debug/cpu-scheduler

# Solo topología CPU
./build/debug/cpu-scheduler --cpu

# Solo frecuencia y gobernadores
./build/debug/cpu-scheduler --freq

# Solo planificación de procesos (top 20)
./build/debug/cpu-scheduler --sched

# Planificación con más procesos
./build/debug/cpu-scheduler --sched --max-pids 50

# Solo uso de CPU
./build/debug/cpu-scheduler --usage

# Solo NUMA
./build/debug/cpu-scheduler --numa

# Leyenda de políticas
./build/debug/cpu-scheduler --legend

# Salida JSON
./build/debug/cpu-scheduler --json
```

## 🧪 Tests

```bash
cmake --build --preset debug
cd build/debug && ctest --output-on-failure -V
```

## 📁 Estructura

```
├── CMakeLists.txt
├── CMakePresets.json
├── include/cpu_scheduler/
│   ├── cpu_scheduler.hpp    # Estructuras de datos y funciones de lectura
│   └── formatters.hpp       # Formateadores de salida (tabla/JSON)
├── src/
│   ├── collector.cpp        # Lectura de /proc y /sys
│   ├── formatters.cpp       # Formateo de resultados
│   └── main.cpp             # Entry point con CLI11
├── tests/
│   └── test_main.cpp        # Tests con Catch2
└── README.md
```

## ⚙️ APIs de sistema utilizadas

| Fuente | Información |
|--------|-------------|
| `/proc/cpuinfo` | Topología, modelo, cores |
| `/sys/devices/system/cpu/cpu*/cpufreq/` | Frecuencias, gobernadores |
| `/proc/stat` | Uso de CPU por núcleo |
| `/proc/[pid]/stat` | Planificación, estado, prioridad |
| `/sys/devices/system/node/node*/` | Topología NUMA |

## 📋 Políticas de planificación

| Política | ID | Descripción |
|----------|----|-------------|
| SCHED_OTHER | 0 | Time-sharing por defecto |
| SCHED_FIFO | 1 | Tiempo real FIFO |
| SCHED_RR | 2 | Tiempo real Round Robin |
| SCHED_BATCH | 3 | Lote (batch) |
| SCHED_IDLE | 5 | Baja prioridad (idle) |
| SCHED_DEADLINE | 6 | Deadline scheduling |
