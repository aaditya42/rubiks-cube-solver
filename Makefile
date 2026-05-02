# ==============================================================================
#  Makefile — Rubik's Cube Solver
# ==============================================================================
#
#  Targets:
#    make          — build the solver (optimized)
#    make debug    — build with debug symbols
#    make test     — build and run benchmarks
#    make clean    — remove build artifacts
# ==============================================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
OPT      = -O2
DEBUG    = -g -O0 -DDEBUG

TARGET   = rubik_solver
SRC      = main.cpp
HEADERS  = cube_state.h heuristic.h solver.h

.PHONY: all debug test clean

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(OPT) -o $(TARGET) $(SRC)

debug: $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(DEBUG) -o $(TARGET)_debug $(SRC)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(TARGET)_debug $(TARGET)_debug.exe
	rm -f verify_moves verify_moves.exe derive_cycles derive_cycles.exe
