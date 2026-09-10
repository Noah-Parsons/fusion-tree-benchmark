# Makefile — everything the project builds, in one place.
#
#   make test     build and run every correctness test
#   make bench    build and run the timing campaign (takes a few minutes)
#   make spread   build and run the sketch-window experiment
#   make figures  run the R analysis
#   make all      test, then bench, then spread, then figures
#   make clean    remove build products
#
# If your CPU is older than about 2013 it will not have BMI2. Drop -mbmi2 and
# -DFT_USE_PEXT from CXXFLAGS; everything still builds and runs, using the
# portable fallback.

CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++20 -march=native -mbmi2 -DFT_USE_PEXT -Iinclude -Wall -Wextra -static
BUILD    := build

.PHONY: all test bench spread figures clean dirs

all: test bench spread figures

dirs:
	@mkdir -p $(BUILD) results figures

$(BUILD)/test_node: tests/test_node.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) tests/test_node.cpp -o $@

$(BUILD)/test_structures: tests/test_structures.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) tests/test_structures.cpp -o $@

$(BUILD)/bench_pred: bench/bench_pred.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) bench/bench_pred.cpp -o $@

$(BUILD)/spread: bench/spread.cpp include/*.hpp | dirs
	$(CXX) $(CXXFLAGS) bench/spread.cpp -o $@

test: dirs $(BUILD)/test_node $(BUILD)/test_structures
	./$(BUILD)/test_node
	./$(BUILD)/test_structures

bench: dirs $(BUILD)/bench_pred
	./$(BUILD)/bench_pred 15 22 > results/timing_raw.csv
	@echo "wrote results/timing_raw.csv"

spread: dirs $(BUILD)/spread
	./$(BUILD)/spread 2000 > results/spread.csv
	@echo "wrote results/spread.csv"

figures:
	Rscript analysis/analyse.R

clean:
	rm -rf $(BUILD)
