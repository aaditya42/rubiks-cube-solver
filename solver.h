/*
 * ============================================================================
 *  solver.h — Search Algorithms for Rubik's Cube
 * ============================================================================
 *
 *  Implements:
 *    1. BFS  — Breadth-first search (optimal, memory-intensive)
 *    2. DFS  — Depth-limited depth-first search
 *    3. IDDFS — Iterative deepening DFS
 *    4. IDA* — Iterative deepening A* (Korf's algorithm, core solver)
 *
 *  All solvers return a vector of move indices and track stats
 *  (states explored, time taken).
 * ============================================================================
 */

#ifndef SOLVER_H
#define SOLVER_H

#include "cube_state.h"
#include "heuristic.h"
#include <vector>
#include <queue>
#include <unordered_set>
#include <chrono>
#include <climits>
#include <iostream>

/* --------------------------------------------------------------------------
 *  Solver result structure
 * -------------------------------------------------------------------------- */

struct SolveResult {
    std::vector<int> moves;     /* sequence of move indices */
    long long states_explored;  /* total states visited */
    double time_seconds;        /* wall-clock time */
    bool solved;                /* whether a solution was found */
    std::string algorithm;      /* name of algorithm used */
};

/* --------------------------------------------------------------------------
 *  1. BFS — Breadth-First Search
 * --------------------------------------------------------------------------
 *  Guarantees shortest solution but uses exponential memory.
 *  Practical only for shallow scrambles (depth ≤ ~7).
 * -------------------------------------------------------------------------- */

inline SolveResult solve_bfs(const CubeState& start, int max_depth = 7) {
    auto t0 = std::chrono::high_resolution_clock::now();
    SolveResult result;
    result.algorithm = "BFS";
    result.states_explored = 0;
    result.solved = false;

    if (start.is_solved()) {
        result.solved = true;
        auto t1 = std::chrono::high_resolution_clock::now();
        result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
        return result;
    }

    /* BFS queue: each entry is (state, move_sequence) */
    struct BFSNode {
        CubeState state;
        std::vector<int> path;
    };

    std::queue<BFSNode> q;
    std::unordered_set<CubeState, CubeHash> visited;

    q.push({start, {}});
    visited.insert(start);
    result.states_explored = 1;

    while (!q.empty()) {
        BFSNode cur = std::move(q.front());
        q.pop();

        if ((int)cur.path.size() >= max_depth) continue;

        int last_move = cur.path.empty() ? -1 : cur.path.back();

        for (int m = 0; m < NUM_MOVES; ++m) {
            /* Pruning: skip redundant moves */
            if (is_move_redundant(m, last_move)) continue;

            CubeState next = cur.state.after_move(m);
            ++result.states_explored;

            if (next.is_solved()) {
                result.moves = cur.path;
                result.moves.push_back(m);
                result.solved = true;
                auto t1 = std::chrono::high_resolution_clock::now();
                result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
                return result;
            }

            if (visited.find(next) == visited.end()) {
                visited.insert(next);
                std::vector<int> new_path = cur.path;
                new_path.push_back(m);
                q.push({next, std::move(new_path)});
            }
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
    return result;
}

/* --------------------------------------------------------------------------
 *  2. DFS — Depth-Limited Depth-First Search
 * --------------------------------------------------------------------------
 *  Uses minimal memory but does not guarantee shortest path.
 * -------------------------------------------------------------------------- */

namespace detail {

inline bool dfs_recursive(const CubeState& state, int depth, int last_move,
                           std::vector<int>& path, long long& explored) {
    if (state.is_solved()) return true;
    if (depth <= 0) return false;

    for (int m = 0; m < NUM_MOVES; ++m) {
        if (is_move_redundant(m, last_move)) continue;

        CubeState next = state.after_move(m);
        ++explored;

        path.push_back(m);
        if (dfs_recursive(next, depth - 1, m, path, explored))
            return true;
        path.pop_back();
    }
    return false;
}

} /* namespace detail */

inline SolveResult solve_dfs(const CubeState& start, int max_depth = 12) {
    auto t0 = std::chrono::high_resolution_clock::now();
    SolveResult result;
    result.algorithm = "DFS";
    result.states_explored = 1;
    result.solved = false;

    if (start.is_solved()) {
        result.solved = true;
        auto t1 = std::chrono::high_resolution_clock::now();
        result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
        return result;
    }

    result.solved = detail::dfs_recursive(start, max_depth, -1,
                                           result.moves, result.states_explored);

    auto t1 = std::chrono::high_resolution_clock::now();
    result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
    return result;
}

/* --------------------------------------------------------------------------
 *  3. IDDFS — Iterative Deepening Depth-First Search
 * --------------------------------------------------------------------------
 *  Combines BFS optimality with DFS memory efficiency.
 *  Guarantees shortest path found.
 * -------------------------------------------------------------------------- */

inline SolveResult solve_iddfs(const CubeState& start, int max_depth = 14) {
    auto t0 = std::chrono::high_resolution_clock::now();
    SolveResult result;
    result.algorithm = "IDDFS";
    result.states_explored = 0;
    result.solved = false;

    if (start.is_solved()) {
        result.solved = true;
        auto t1 = std::chrono::high_resolution_clock::now();
        result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
        return result;
    }

    for (int depth = 0; depth <= max_depth; ++depth) {
        result.moves.clear();
        long long explored = 0;

        if (detail::dfs_recursive(start, depth, -1, result.moves, explored)) {
            result.states_explored += explored;
            result.solved = true;
            auto t1 = std::chrono::high_resolution_clock::now();
            result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
            return result;
        }
        result.states_explored += explored;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
    return result;
}

/* --------------------------------------------------------------------------
 *  4. IDA* — Iterative Deepening A* (Korf's Algorithm)
 * --------------------------------------------------------------------------
 *  The core solver. Combines depth-first search with heuristic pruning.
 *
 *  At each node:
 *    f = g + h  (g = current depth, h = heuristic estimate)
 *    If f > threshold, prune and return f as the next threshold candidate.
 *
 *  Key optimizations:
 *    - Move pruning (no same-face or bad opposite-face sequences)
 *    - Admissible heuristic from pattern databases
 *    - Minimal memory: only stores current path on stack
 *    - No visited-state set needed (heuristic handles pruning)
 * -------------------------------------------------------------------------- */

namespace detail {

/*
 * ida_search: recursive DFS with f-cost cutoff.
 * Returns INT_MAX if solved (solution in `path`),
 * otherwise returns the minimum f-cost that exceeded the threshold.
 */
inline int ida_search(const CubeState& state, int g, int threshold,
                       int last_move, std::vector<int>& path,
                       long long& explored) {
    int h = heuristic(state);
    int f = g + h;

    /* Prune: f exceeds current threshold */
    if (f > threshold) return f;

    /* Goal check */
    if (state.is_solved()) return INT_MAX; /* sentinel: solved */

    int min_next = INT_MAX - 1; /* track minimum exceeded threshold */

    for (int m = 0; m < NUM_MOVES; ++m) {
        if (is_move_redundant(m, last_move)) continue;

        CubeState next = state.after_move(m);
        ++explored;

        path.push_back(m);
        int t = ida_search(next, g + 1, threshold, m, path, explored);

        if (t == INT_MAX) return INT_MAX; /* solved */
        if (t < min_next) min_next = t;

        path.pop_back();
    }

    return min_next;
}

} /* namespace detail */

inline SolveResult solve_ida_star(const CubeState& start, int max_depth = 25) {
    auto t0 = std::chrono::high_resolution_clock::now();
    SolveResult result;
    result.algorithm = "IDA*";
    result.states_explored = 0;
    result.solved = false;

    /* Ensure heuristic databases are built */
    init_heuristic();

    if (start.is_solved()) {
        result.solved = true;
        auto t1 = std::chrono::high_resolution_clock::now();
        result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
        return result;
    }

    /* Initial threshold = heuristic of start state */
    int threshold = heuristic(start);

    while (threshold <= max_depth) {
        result.moves.clear();
        long long explored = 0;

        int t = detail::ida_search(start, 0, threshold, -1,
                                    result.moves, explored);
        result.states_explored += explored;

        if (t == INT_MAX) {
            result.solved = true;
            auto t1 = std::chrono::high_resolution_clock::now();
            result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
            return result;
        }

        /* Increase threshold to the minimum f that exceeded it */
        threshold = t;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.time_seconds = std::chrono::duration<double>(t1 - t0).count();
    return result;
}

/* --------------------------------------------------------------------------
 *  Convenience: print solve result
 * -------------------------------------------------------------------------- */

inline void print_result(const SolveResult& res) {
    std::cout << "Algorithm: " << res.algorithm << "\n";
    if (res.solved) {
        std::cout << "Solution:  " << solution_string(res.moves) << "\n";
        std::cout << "Moves:     " << res.moves.size() << "\n";
    } else {
        std::cout << "Status:    NO SOLUTION FOUND within depth limit\n";
    }
    std::cout << "States:    " << res.states_explored << "\n";
    std::cout << "Time:      " << res.time_seconds << " sec\n";
}

#endif /* SOLVER_H */
