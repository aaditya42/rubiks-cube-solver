<p align="center">
  <img src="https://img.shields.io/badge/C++-17-blue?logo=cplusplus&logoColor=white" alt="C++17"/>
  <img src="https://img.shields.io/badge/Algorithm-IDA*-orange" alt="IDA*"/>
  <img src="https://img.shields.io/badge/Optimal-Solutions-green" alt="Optimal"/>
  <img src="https://img.shields.io/badge/License-MIT-yellow" alt="MIT License"/>
</p>

# 🧊 Rubik's Cube Solver

A high-performance, optimal 3×3 Rubik's Cube solver built in C++ using **Korf's IDA\*** algorithm with **pattern database heuristics**. Finds the shortest possible solution for any scrambled state.

---

## ✨ Features

- **Optimal Solutions** — Guarantees the minimum number of moves to solve any scramble
- **Multiple Search Algorithms** — BFS, DFS, IDDFS, and IDA\* (Iterative Deepening A\*)
- **Pattern Database Heuristics** — 4 BFS-precomputed lookup tables for aggressive pruning
- **Move Pruning** — Eliminates redundant move sequences to reduce branching factor
- **Fast State Hashing** — FNV-1a 64-bit hash for efficient visited-state tracking
- **Solution Verification** — Every solution is replayed and verified for correctness
- **Compact Representation** — 54-byte facelet array with precomputed 4-cycle move permutations

---

## 🚀 Quick Start

### Prerequisites

- **g++** with C++17 support (GCC 7+ / Clang 5+ / MSVC 2017+)

### Build

```bash
# Optimized build
g++ -O2 -std=c++17 -o rubik_solver main.cpp

# Or use the Makefile
make
```

### Run

```bash
# Run full benchmark suite
./rubik_solver

# Solve a specific scramble
./rubik_solver "R U F' D2 L B"

# More examples
./rubik_solver "R U R' U'"
./rubik_solver "R U2 R' U' R U' R' L' U L"
```

### Move Notation

| Symbol | Meaning |
|:------:|---------|
| `R` | Right face 90° clockwise |
| `R'` | Right face 90° counter-clockwise |
| `R2` | Right face 180° |
| `U D L R F B` | Up, Down, Left, Right, Front, Back |

---

## 📊 Benchmark Results

All tests find **optimal solutions** verified by replaying the moves.

| Scramble Depth | Solution Length | States Explored | Time |
|:-:|:-:|--:|--:|
| 1 | 1 | 5 | < 1 ms |
| 3 | 3 | 12 | < 1 ms |
| 5 | 5 | 37 | < 1 ms |
| 7 | 7 | 64 | < 1 ms |
| 9 | 9 | 80,340 | 0.05 s |
| 10 | 10 | 1,085,122 | 0.70 s |
| 11 | 11 | 4,238,104 | 2.60 s |
| 12 | 10 *(optimal)* | 919,506 | 0.58 s |

### Algorithm Comparison

Solving the same 5-move scramble (`R U F D L`):

| Algorithm | States Explored | Time | Speedup |
|-----------|----------------:|-----:|--------:|
| BFS | 470,724 | 0.40 s | 1× |
| IDDFS | 508,897 | 0.01 s | 40× |
| **IDA\*** | **37** | **< 1 ms** | **12,700×** |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────┐
│                    main.cpp                         │
│         Entry point · Benchmarks · Verification     │
├─────────────────────────────────────────────────────┤
│                    solver.h                         │
│           BFS · DFS · IDDFS · IDA*                  │
├──────────────────────┬──────────────────────────────┤
│    heuristic.h       │       cube_state.h           │
│  Pattern Databases   │  State · Moves · Hashing     │
│  (4 BFS tables)      │  Pruning · I/O · Parsing     │
└──────────────────────┴──────────────────────────────┘
```

### File Overview

| File | Description |
|------|-------------|
| `cube_state.h` | 54-facelet cube representation, 18 move operations (6 faces × CW/CCW/180°), FNV-1a hashing, move pruning, scramble parsing, visual output |
| `heuristic.h` | Admissible heuristic using 4 pattern databases precomputed via BFS from solved state |
| `solver.h` | Search algorithms — BFS (optimal, memory-heavy), DFS (depth-limited), IDDFS (iterative deepening), IDA\* (heuristic-guided, core solver) |
| `main.cpp` | CLI interface, 12-case benchmark suite, algorithm comparison, solution verification |

---

## 🧠 How It Works

### Cube Representation

The cube is stored as a **54-element array** where each entry holds a color value (0–5). Moves are implemented as **precomputed 4-cycle permutations** — each face rotation applies exactly 5 cycles of 4 facelets, making moves extremely fast.

```
            U0  U1  U2
            U3  U4  U5
            U6  U7  U8

 L0 L1 L2  F0  F1  F2  R0 R1 R2  B0 B1 B2
 L3 L4 L5  F3  F4  F5  R3 R4 R5  B3 B4 B5
 L6 L7 L8  F6  F7  F8  R6 R7 R8  B6 B7 B8

            D0  D1  D2
            D3  D4  D5
            D6  D7  D8
```

Move cycles were **derived from 3D coordinate geometry** and verified with algebraic identities:
- `X⁴ = Identity` (all quarter turns)
- `X · X' = Identity` (move + inverse)
- `(R U R' U')⁶ = Identity` (sexy move)

### Pattern Database Heuristics

Four BFS-precomputed lookup tables provide admissible lower bounds on the solution length:

| Database | Coordinate | Size | What It Captures |
|----------|-----------|-----:|------------------|
| Corner Orientation | 3⁷ orientations | 2,187 | How corners are twisted |
| Edge Orientation | 2¹¹ orientations | 2,048 | How edges are flipped |
| Corner Permutation | 8! arrangements | 40,320 | Where corners sit |
| **Combined** | **corner_ori × UD-slice** | **1,082,565** | Corner twist + equatorial edge positions |

The combined heuristic takes the **maximum** of all four lookups, which remains admissible while providing much tighter bounds. This is the key optimization that makes solving 10+ move scrambles practical.

### IDA\* (Iterative Deepening A\*)

The core solver uses **Korf's IDA\* algorithm**:

1. Start with threshold = `h(start_state)`
2. Run depth-first search, pruning nodes where `g + h > threshold`
3. If no solution found, set threshold = minimum `f` that exceeded the cutoff
4. Repeat until solved

**Key optimizations:**
- **No visited-state set** — the admissible heuristic handles cycle avoidance
- **Move pruning** — eliminates same-face and opposite-face redundancies (reduces branching from 18 to ~15 effective moves)
- **Stack-only memory** — only the current path is stored, giving O(depth) space complexity
- **Consistent heuristic** — guarantees no re-expansion needed

---

## 📁 Project Structure

```
rubiks-cube-solver/
├── cube_state.h       # Cube state representation & move engine
├── heuristic.h        # Pattern database heuristic functions
├── solver.h           # Search algorithms (BFS, DFS, IDDFS, IDA*)
├── main.cpp           # Entry point, benchmarks, and testing
├── Makefile           # Build targets (all, debug, test, clean)
├── .gitignore         # Git ignore rules
└── README.md          # This file
```

---

## 🔧 Build Targets

```bash
make          # Build optimized binary
make debug    # Build with debug symbols (-g -O0)
make test     # Build and run benchmarks
make clean    # Remove build artifacts
```

---

## 📖 Sample Output

```
================================================================
  RUBIK'S CUBE SOLVER — BENCHMARK SUITE
================================================================

--- Test: 9-move ---
Scramble: F R U R' U' F' L D B
Algorithm: IDA*
Solution:  B' D' L' F U R U' R' F'
Moves:     9
States:    80340
Time:      0.0536 sec
Verify:    PASS ✓

--- Test: 11-move ---
Scramble: R U R' U R U2 R' F R' F' R
Algorithm: IDA*
Solution:  R' F R F' R U2 R' U' R U' R'
Moves:     11
States:    4238104
Time:      2.6045 sec
Verify:    PASS ✓
```

---

## ⚡ Performance Notes

- **Pattern database initialization** takes ~3–5 seconds on first run (one-time cost)
- Scrambles up to **~10 moves** solve in under **1 second**
- Scrambles of **11 moves** solve in **~3 seconds**
- Deeper scrambles (12+) may take longer as search space grows exponentially
- All solutions are **provably optimal** (minimum move count)

---

## 🤝 Contributing

Contributions are welcome! Some ideas for improvement:

- [ ] Add full edge permutation pattern database for deeper pruning
- [ ] Implement Kociemba's two-phase algorithm for sub-second solves at any depth
- [ ] Add pattern database file caching (save/load to disk)
- [ ] Support for cube state input via facelet color string
- [ ] Web interface / visualization

---


## 📚 References

- Korf, R. E. (1997). *Finding Optimal Solutions to Rubik's Cube Using Pattern Databases*. AAAI-97.
- Kociemba, H. *The Two-Phase Algorithm*. [kociemba.org](http://kociemba.org/cube.htm)
- Singmaster, D. (1981). *Notes on Rubik's Magic Cube*. Penguin Books.

---

<p align="center">
  Built with ❤️ and C++17
</p>
