/*
 * ============================================================================
 *  main.cpp — Rubik's Cube Solver Entry Point
 * ============================================================================
 *
 *  Provides:
 *    1. Interactive mode: enter scramble sequences to solve
 *    2. Benchmark mode: run predefined test cases
 *    3. Comparison mode: solve same scramble with all algorithms
 *
 *  Usage:
 *    ./rubik_solver              — run benchmarks
 *    ./rubik_solver "R U F' D2"  — solve a specific scramble
 * ============================================================================
 */

#include "solver.h"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

/* --------------------------------------------------------------------------
 *  verify_solution: apply the solution moves to the scrambled state
 *  and confirm it produces the solved state.
 * -------------------------------------------------------------------------- */

bool verify_solution(const CubeState& scrambled, const std::vector<int>& moves) {
    CubeState cube = scrambled;
    for (int m : moves)
        cube.apply_move(m);
    return cube.is_solved();
}

/* --------------------------------------------------------------------------
 *  Benchmark test cases: scrambles of increasing difficulty
 * -------------------------------------------------------------------------- */

struct TestCase {
    std::string name;
    std::string scramble;
};

static const std::vector<TestCase> TEST_CASES = {
    /* Trivial: 1-3 moves */
    {"1-move",       "R"},
    {"2-move",       "R U"},
    {"3-move",       "R U F"},

    /* Easy: 4-6 moves */
    {"4-move",       "R U R' U'"},
    {"5-move",       "R U F D L"},
    {"6-move",       "R U R' F D B"},

    /* Moderate: 7-10 moves */
    {"7-move",       "R U2 F' D L B R'"},
    {"8-move",       "R U F D' L' B R U'"},
    {"9-move",       "F R U R' U' F' L D B"},
    {"10-move",      "R U2 R' U' R U' R' L' U L"},

    /* Harder: 11-14 moves */
    {"11-move",      "R U R' U R U2 R' F R' F' R"},
    {"12-move",      "R2 D' R U2 R' D R U2 R F2 L F2"},
};

/* --------------------------------------------------------------------------
 *  run_benchmarks: test each scramble with IDA* solver
 * -------------------------------------------------------------------------- */

void run_benchmarks() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  RUBIK'S CUBE SOLVER — BENCHMARK SUITE\n";
    std::cout << "================================================================\n\n";

    int passed = 0, failed = 0;

    for (const auto& tc : TEST_CASES) {
        std::cout << "--- Test: " << tc.name << " ---\n";
        std::cout << "Scramble: " << tc.scramble << "\n";

        CubeState cube = apply_scramble(tc.scramble);

        /* Solve with IDA* */
        SolveResult res = solve_ida_star(cube);
        print_result(res);

        /* Verify */
        if (res.solved) {
            bool valid = verify_solution(cube, res.moves);
            if (valid) {
                std::cout << "Verify:    PASS ✓\n";
                ++passed;
            } else {
                std::cout << "Verify:    FAIL ✗ (solution does not solve cube!)\n";
                ++failed;
            }
        } else {
            std::cout << "Verify:    FAIL ✗ (no solution found)\n";
            ++failed;
        }
        std::cout << "\n";
    }

    std::cout << "================================================================\n";
    std::cout << "  Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "================================================================\n\n";
}

/* --------------------------------------------------------------------------
 *  compare_algorithms: solve same scramble with BFS, IDDFS, and IDA*
 * -------------------------------------------------------------------------- */

void compare_algorithms(const std::string& scramble) {
    CubeState cube = apply_scramble(scramble);

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  Algorithm Comparison for: " << scramble << "\n";
    std::cout << "================================================================\n\n";

    print_cube(cube);
    std::cout << "\n";

    /* BFS (limited depth) */
    std::cout << "--- BFS (max depth 7) ---\n";
    SolveResult bfs_res = solve_bfs(cube, 7);
    print_result(bfs_res);
    if (bfs_res.solved)
        std::cout << "Verify: " << (verify_solution(cube, bfs_res.moves) ? "PASS" : "FAIL") << "\n";
    std::cout << "\n";

    /* IDDFS */
    std::cout << "--- IDDFS (max depth 10) ---\n";
    SolveResult iddfs_res = solve_iddfs(cube, 10);
    print_result(iddfs_res);
    if (iddfs_res.solved)
        std::cout << "Verify: " << (verify_solution(cube, iddfs_res.moves) ? "PASS" : "FAIL") << "\n";
    std::cout << "\n";

    /* IDA* */
    std::cout << "--- IDA* ---\n";
    SolveResult ida_res = solve_ida_star(cube);
    print_result(ida_res);
    if (ida_res.solved)
        std::cout << "Verify: " << (verify_solution(cube, ida_res.moves) ? "PASS" : "FAIL") << "\n";
    std::cout << "\n";
}

/* --------------------------------------------------------------------------
 *  Main entry point
 * -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
    std::cout << std::fixed << std::setprecision(4);

    if (argc > 1) {
        /* Command-line argument: treat as scramble sequence */
        std::string scramble;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) scramble += ' ';
            scramble += argv[i];
        }

        std::cout << "\nSolving scramble: " << scramble << "\n\n";

        /* Initialize heuristic */
        init_heuristic();

        CubeState cube = apply_scramble(scramble);
        std::cout << "Scrambled state:\n";
        print_cube(cube);
        std::cout << "\n";

        /* Solve with IDA* */
        SolveResult res = solve_ida_star(cube);
        print_result(res);

        if (res.solved) {
            bool valid = verify_solution(cube, res.moves);
            std::cout << "Verify:    " << (valid ? "PASS ✓" : "FAIL ✗") << "\n";
        }
    } else {
        /* No arguments: run benchmarks + algorithm comparison */
        init_heuristic();
        run_benchmarks();

        /* Compare algorithms on a moderate scramble */
        compare_algorithms("R U F D L");
    }

    return 0;
}
