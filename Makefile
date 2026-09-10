# Makefile — everything the project builds, in one place.
#
#   make test       build and run every correctness test (PEXT sketch)
#   make test-loop  the same tests with the O(r) loop sketch
#   make bench      throughput campaign -> results/timing_raw.csv
#   make bench-lat  latency campaign    -> results/timing_lat.csv
#   make bench-loop throughput campaign, loop sketch -> results/timing_loop.csv
#   make spread     Experiment 1 (K = 2..16) -> results/spread.csv
#   make node-lat   Experiment 3a: one node search, in cycles -> results/node_lat.csv
#   make traffic    Experiment 3b: cache lines and modelled misses -> results/traffic.csv
#   make figures    run the R analysis
#   make all        test, then bench, then spread, then figures
#   make clean      remove build products
#
# Any target can be built with clang instead:  make test CXX=clang++ BUILD=build_clang
#
# CPU selects the logical processor the benchmark is pinned to. On the
# Core Ultra 7 255HX, 8 is a performance core; 2 is an EFFICIENCY core. See
# results/machine.txt. CPU=-1 leaves the thread unpinned.
#
# If your CPU is older than about 2013 it will not have BMI2. Drop -mbmi2 and
# -DFT_USE_PEXT from CXXFLAGS; everything still builds and runs, using the
# portable fallback.

CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++20 -march=native -mbmi2 -DFT_USE_PEXT -Iinclude -Wall -Wextra -static
LOOPFLAGS := $(filter-out -DFT_USE_PEXT,$(CXXFLAGS))
BUILD    := build
CPU      ?= 8
REPS     ?= 15
MAXLOG   ?= 22

.PHONY: all test test-loop bench bench-lat bench-loop spread node-lat traffic figures clean dirs

all: test bench spread figures

dirs:
	@mkdir -p $(BUILD) results figures

$(BUILD)/test_node: tests/test_node.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) tests/test_node.cpp -o $@

$(BUILD)/test_structures: tests/test_structures.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) tests/test_structures.cpp -o $@

$(BUILD)/test_node_loop: tests/test_node.cpp include/*.hpp | dirs
	$(CXX) $(LOOPFLAGS) tests/test_node.cpp -o $@

$(BUILD)/test_structures_loop: tests/test_structures.cpp include/*.hpp | dirs
	$(CXX) $(LOOPFLAGS) tests/test_structures.cpp -o $@

$(BUILD)/bench_pred: bench/bench_pred.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) bench/bench_pred.cpp -o $@

$(BUILD)/bench_pred_loop: bench/bench_pred.cpp include/*.hpp | dirs
	$(CXX) $(LOOPFLAGS) bench/bench_pred.cpp -o $@

$(BUILD)/spread: bench/spread.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) bench/spread.cpp -o $@

$(BUILD)/node_lat: bench/node_lat.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) bench/node_lat.cpp -o $@

$(BUILD)/traffic: bench/traffic.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) bench/traffic.cpp -o $@

test: dirs $(BUILD)/test_node $(BUILD)/test_structures
	./$(BUILD)/test_node
	./$(BUILD)/test_structures

test-loop: dirs $(BUILD)/test_node_loop $(BUILD)/test_structures_loop
	./$(BUILD)/test_node_loop
	./$(BUILD)/test_structures_loop

bench: dirs $(BUILD)/bench_pred
	./$(BUILD)/bench_pred $(REPS) $(MAXLOG) $(CPU) tput > results/timing_raw.csv
	@echo "wrote results/timing_raw.csv"

bench-lat: dirs $(BUILD)/bench_pred
	./$(BUILD)/bench_pred $(REPS) $(MAXLOG) $(CPU) lat > results/timing_lat.csv
	@echo "wrote results/timing_lat.csv"

bench-loop: dirs $(BUILD)/bench_pred_loop
	./$(BUILD)/bench_pred_loop $(REPS) $(MAXLOG) $(CPU) tput > results/timing_loop.csv
	@echo "wrote results/timing_loop.csv"

spread: dirs $(BUILD)/spread
	./$(BUILD)/spread 2000 > results/spread.csv
	@echo "wrote results/spread.csv"

node-lat: dirs $(BUILD)/node_lat
	./$(BUILD)/node_lat $(REPS) $(CPU) > results/node_lat.csv
	@echo "wrote results/node_lat.csv"

traffic: dirs $(BUILD)/traffic
	./$(BUILD)/traffic $(MAXLOG) > results/traffic.csv
	@echo "wrote results/traffic.csv"

figures:
	Rscript analysis/analyse.R

clean:
	rm -rf $(BUILD)
