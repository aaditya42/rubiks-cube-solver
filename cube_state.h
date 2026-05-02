/*
 * ============================================================================
 *  cube_state.h — Rubik's Cube State Representation & Move Engine
 * ============================================================================
 *
 *  Represents a 3×3 Rubik's Cube using a 54-element facelet array.
 *  Each element stores which color (face) occupies that facelet position.
 *
 *  Face layout (index mapping):
 *
 *              U0  U1  U2          Indices: 0  1  2
 *              U3  U4  U5                   3  4  5
 *              U6  U7  U8                   6  7  8
 *
 *   L0 L1 L2  F0  F1  F2  R0 R1 R2  B0 B1 B2
 *   36 37 38   18  19  20  9  10 11  45 46 47
 *   L3 L4 L5  F3  F4  F5  R3 R4 R5  B3 B4 B5
 *   39 40 41   21  22  23  12 13 14  48 49 50
 *   L6 L7 L8  F6  F7  F8  R6 R7 R8  B6 B7 B8
 *   42 43 44   24  25  26  15 16 17  51 52 53
 *
 *              D0  D1  D2          Indices: 27 28 29
 *              D3  D4  D5                   30 31 32
 *              D6  D7  D8                   33 34 35
 *
 *  Face order: U=0, R=1, F=2, D=3, L=4, B=5
 *  Face base indices: U=0, R=9, F=18, D=27, L=36, B=45
 *
 *  Move encoding:
 *    0=U  1=U'  2=U2    3=R  4=R'  5=R2
 *    6=F  7=F'  8=F2    9=D  10=D' 11=D2
 *   12=L  13=L' 14=L2  15=B  16=B' 17=B2
 */

#ifndef CUBE_STATE_H
#define CUBE_STATE_H

#include <cstdint>
#include <cstring>
#include <string>
#include <array>
#include <vector>
#include <functional>
#include <iostream>

/* --------------------------------------------------------------------------
 *  Constants
 * -------------------------------------------------------------------------- */

static constexpr int NUM_FACELETS = 54;
static constexpr int NUM_FACES    = 6;
static constexpr int NUM_MOVES    = 18;

/* Face identifiers (also used as color values) */
enum Face : uint8_t { U = 0, R = 1, F = 2, D = 3, L = 4, B = 5 };

/* Base index of each face in the facelet array */
static constexpr int FACE_BASE[6] = { 0, 9, 18, 27, 36, 45 };

/* Human-readable move names */
static const char* MOVE_NAMES[NUM_MOVES] = {
    "U", "U'", "U2",
    "R", "R'", "R2",
    "F", "F'", "F2",
    "D", "D'", "D2",
    "L", "L'", "L2",
    "B", "B'", "B2"
};

/* --------------------------------------------------------------------------
 *  Move Permutation Tables
 * --------------------------------------------------------------------------
 *  Each base move (CW quarter turn) is a permutation of facelet indices.
 *  We store only the cycles (groups of 4 indices that rotate together).
 *  A CW move cycles a→b→c→d→a (i.e., a gets d's value, b gets a's, etc.)
 * -------------------------------------------------------------------------- */

/*
 * rotate_face_cw_indices: the 8 non-center facelets of a face,
 * listed as two 4-cycles for a 90° CW rotation.
 */
struct FaceCycles {
    int face_cycle1[4]; /* corners of the face */
    int face_cycle2[4]; /* edges of the face */
    int adj_cycle1[4];  /* adjacent band cycle 1 */
    int adj_cycle2[4];  /* adjacent band cycle 2 */
    int adj_cycle3[4];  /* adjacent band cycle 3 */
};

/*
 * Each base move (CW) has:
 *   - 2 cycles for the rotating face itself (corners + edges)
 *   - 3 cycles for the adjacent band of 12 facelets
 */
/*
 * Move permutation cycles derived from 3D coordinate geometry simulation.
 * Each cycle {a,b,c,d} means: CW rotation sends a→b→c→d→a.
 * Implementation: tmp=f[d]; f[d]=f[c]; f[c]=f[b]; f[b]=f[a]; f[a]=tmp;
 *
 * Verified against all identity tests:
 *   - X^4 = identity for all quarter turns
 *   - X * X' = identity
 *   - (R U R' U')^6 = identity
 */
static const FaceCycles BASE_MOVE_CYCLES[6] = {
    /* U move (CW looking from top) */
    {
        {0, 2, 8, 6},       /* U face corners */
        {1, 5, 7, 3},       /* U face edges */
        {9, 18, 36, 45},    /* adjacent band */
        {10, 19, 37, 46},
        {11, 20, 38, 47}
    },
    /* R move (CW looking from right) */
    {
        {9, 15, 17, 11},    /* R face corners */
        {10, 12, 16, 14},   /* R face edges */
        {2, 20, 29, 51},    /* adjacent band */
        {5, 23, 32, 48},
        {8, 26, 35, 45}
    },
    /* F move (CW looking from front) */
    {
        {18, 24, 26, 20},   /* F face corners */
        {19, 21, 25, 23},   /* F face edges */
        {6, 44, 29, 9},     /* adjacent band */
        {7, 41, 28, 12},
        {8, 38, 27, 15}
    },
    /* D move (CW looking from bottom) */
    {
        {27, 29, 35, 33},   /* D face corners */
        {28, 32, 34, 30},   /* D face edges */
        {15, 51, 42, 24},   /* adjacent band */
        {16, 52, 43, 25},
        {17, 53, 44, 26}
    },
    /* L move (CW looking from left) */
    {
        {36, 42, 44, 38},   /* L face corners */
        {37, 39, 43, 41},   /* L face edges */
        {0, 53, 27, 18},    /* adjacent band */
        {3, 50, 30, 21},
        {6, 47, 33, 24}
    },
    /* B move (CW looking from back) */
    {
        {45, 51, 53, 47},   /* B face corners */
        {46, 48, 52, 50},   /* B face edges */
        {0, 11, 35, 42},    /* adjacent band */
        {1, 14, 34, 39},
        {2, 17, 33, 36}
    }
};

/* --------------------------------------------------------------------------
 *  CubeState — Core cube data structure
 * -------------------------------------------------------------------------- */

struct CubeState {
    /*
     * Facelet array: f[i] stores the color (0-5) at position i.
     * In the solved state, f[i] = i / 9.
     */
    uint8_t f[NUM_FACELETS];

    /* Initialize to solved state */
    CubeState() {
        for (int i = 0; i < NUM_FACELETS; ++i)
            f[i] = static_cast<uint8_t>(i / 9);
    }

    /* Check if cube is in solved state */
    bool is_solved() const {
        for (int i = 0; i < NUM_FACELETS; ++i)
            if (f[i] != static_cast<uint8_t>(i / 9))
                return false;
        return true;
    }

    /* Equality comparison */
    bool operator==(const CubeState& o) const {
        return std::memcmp(f, o.f, NUM_FACELETS) == 0;
    }

    bool operator!=(const CubeState& o) const {
        return !(*this == o);
    }

    /* ----- Move application -----
     *
     * apply_cycle_cw: given 4 indices forming a cycle, perform CW rotation.
     *   a→b→c→d→a  means:  new[b]=old[a], new[c]=old[b], new[d]=old[c], new[a]=old[d]
     *   Equivalent to: tmp=f[d]; f[d]=f[c]; f[c]=f[b]; f[b]=f[a]; f[a]=tmp;
     */
    inline void apply_cycle_cw(const int c[4]) {
        uint8_t tmp = f[c[3]];
        f[c[3]] = f[c[2]];
        f[c[2]] = f[c[1]];
        f[c[1]] = f[c[0]];
        f[c[0]] = tmp;
    }

    /* CCW rotation of a 4-cycle (reverse direction) */
    inline void apply_cycle_ccw(const int c[4]) {
        uint8_t tmp = f[c[0]];
        f[c[0]] = f[c[1]];
        f[c[1]] = f[c[2]];
        f[c[2]] = f[c[3]];
        f[c[3]] = tmp;
    }

    /* 180° rotation of a 4-cycle (two swaps) */
    inline void apply_cycle_180(const int c[4]) {
        std::swap(f[c[0]], f[c[2]]);
        std::swap(f[c[1]], f[c[3]]);
    }

    /*
     * apply_base_move_cw: Apply a clockwise quarter turn for face `face_id`.
     * Rotates both the face's own facelets and the adjacent band.
     */
    void apply_base_move_cw(int face_id) {
        const FaceCycles& fc = BASE_MOVE_CYCLES[face_id];
        apply_cycle_cw(fc.face_cycle1);
        apply_cycle_cw(fc.face_cycle2);
        apply_cycle_cw(fc.adj_cycle1);
        apply_cycle_cw(fc.adj_cycle2);
        apply_cycle_cw(fc.adj_cycle3);
    }

    /* Apply a counter-clockwise quarter turn */
    void apply_base_move_ccw(int face_id) {
        const FaceCycles& fc = BASE_MOVE_CYCLES[face_id];
        apply_cycle_ccw(fc.face_cycle1);
        apply_cycle_ccw(fc.face_cycle2);
        apply_cycle_ccw(fc.adj_cycle1);
        apply_cycle_ccw(fc.adj_cycle2);
        apply_cycle_ccw(fc.adj_cycle3);
    }

    /* Apply a 180° turn */
    void apply_base_move_180(int face_id) {
        const FaceCycles& fc = BASE_MOVE_CYCLES[face_id];
        apply_cycle_180(fc.face_cycle1);
        apply_cycle_180(fc.face_cycle2);
        apply_cycle_180(fc.adj_cycle1);
        apply_cycle_180(fc.adj_cycle2);
        apply_cycle_180(fc.adj_cycle3);
    }

    /*
     * apply_move: Apply move by index (0–17).
     *   move_id = face_id * 3 + variant
     *   variant: 0=CW, 1=CCW, 2=180°
     */
    void apply_move(int move_id) {
        int face_id = move_id / 3;
        int variant = move_id % 3;
        switch (variant) {
            case 0: apply_base_move_cw(face_id);  break;
            case 1: apply_base_move_ccw(face_id); break;
            case 2: apply_base_move_180(face_id);  break;
        }
    }

    /* Return a new state with a move applied (does not modify this state) */
    CubeState after_move(int move_id) const {
        CubeState copy = *this;
        copy.apply_move(move_id);
        return copy;
    }
};

/* --------------------------------------------------------------------------
 *  Hashing — for unordered_set / unordered_map usage
 * --------------------------------------------------------------------------
 *  Uses FNV-1a hash over the 54-byte facelet array.
 *  Fast and has good distribution for our use case.
 * -------------------------------------------------------------------------- */

struct CubeHash {
    size_t operator()(const CubeState& s) const {
        /* FNV-1a 64-bit hash */
        size_t h = 14695981039346656037ULL;
        for (int i = 0; i < NUM_FACELETS; ++i) {
            h ^= static_cast<size_t>(s.f[i]);
            h *= 1099511628211ULL;
        }
        return h;
    }
};

/* --------------------------------------------------------------------------
 *  Move Pruning — avoid obviously redundant move sequences
 * --------------------------------------------------------------------------
 *  Rules:
 *    1. Don't apply the same face twice in a row (e.g., U then U' = no-op).
 *    2. For opposite faces (U-D, R-L, F-B), enforce an ordering:
 *       if we just moved one, don't move its opposite before doing
 *       something else on a different axis. This prevents X Y being
 *       duplicated as Y X when X,Y are on the same axis.
 * -------------------------------------------------------------------------- */

/*
 * is_move_redundant: returns true if `move` is redundant after `last_move`.
 * last_move = -1 means no previous move.
 */
inline bool is_move_redundant(int move, int last_move) {
    if (last_move < 0) return false;

    int face      = move / 3;
    int last_face = last_move / 3;

    /* Rule 1: same face twice in a row */
    if (face == last_face) return true;

    /*
     * Rule 2: opposite-face ordering.
     * Opposite pairs: U(0)-D(3), R(1)-L(4), F(2)-B(5)
     * For each pair, enforce that the lower-index face goes first.
     * If last_face and face are opposite and face < last_face, it's redundant.
     */
    static const int opposite[6] = {3, 4, 5, 0, 1, 2};
    if (opposite[face] == last_face && face > last_face) return true;

    return false;
}

/* --------------------------------------------------------------------------
 *  Input Parsing — read cube state from a string
 * --------------------------------------------------------------------------
 *  Input format: 54 characters representing facelets in order:
 *    U0..U8 R0..R8 F0..F8 D0..D8 L0..L8 B0..B8
 *  Characters: U/R/F/D/L/B (case-insensitive)
 *
 *  Alternatively, accept a scramble sequence (e.g., "R U F' D2 ...")
 * -------------------------------------------------------------------------- */

inline int char_to_face(char c) {
    switch (c) {
        case 'U': case 'u': return 0;
        case 'R': case 'r': return 1;
        case 'F': case 'f': return 2;
        case 'D': case 'd': return 3;
        case 'L': case 'l': return 4;
        case 'B': case 'b': return 5;
        default: return -1;
    }
}

/* Parse a single move token like "R", "U'", "F2" and return move index 0-17 */
inline int parse_move_token(const std::string& tok) {
    if (tok.empty()) return -1;
    int face = char_to_face(tok[0]);
    if (face < 0) return -1;

    int variant = 0; /* default: CW */
    if (tok.size() > 1) {
        if (tok[1] == '\'' || tok[1] == '`' || tok[1] == 'i') {
            variant = 1; /* CCW */
        } else if (tok[1] == '2') {
            variant = 2; /* 180° */
        }
    }
    return face * 3 + variant;
}

/*
 * apply_scramble: Apply a sequence of moves (space-separated string)
 * to a solved cube and return the scrambled state.
 */
inline CubeState apply_scramble(const std::string& scramble) {
    CubeState cube;
    std::string token;
    for (size_t i = 0; i <= scramble.size(); ++i) {
        if (i == scramble.size() || scramble[i] == ' ') {
            if (!token.empty()) {
                int m = parse_move_token(token);
                if (m >= 0) cube.apply_move(m);
                token.clear();
            }
        } else {
            token += scramble[i];
        }
    }
    return cube;
}

/* --------------------------------------------------------------------------
 *  Output Utilities
 * -------------------------------------------------------------------------- */

inline std::string solution_string(const std::vector<int>& moves) {
    std::string result;
    for (size_t i = 0; i < moves.size(); ++i) {
        if (i > 0) result += ' ';
        result += MOVE_NAMES[moves[i]];
    }
    return result.empty() ? "(already solved)" : result;
}

/* Print cube state in a visual layout */
inline void print_cube(const CubeState& cube) {
    static const char FACE_CHAR[] = "URFDLB";
    auto c = [&](int idx) -> char { return FACE_CHAR[cube.f[idx]]; };

    /* Print U face */
    std::cout << "         " << c(0) << " " << c(1) << " " << c(2) << "\n";
    std::cout << "         " << c(3) << " " << c(4) << " " << c(5) << "\n";
    std::cout << "         " << c(6) << " " << c(7) << " " << c(8) << "\n";

    /* Print L F R B faces side by side */
    for (int row = 0; row < 3; ++row) {
        /* L face */
        std::cout << "  " << c(36 + row*3) << " " << c(37 + row*3) << " " << c(38 + row*3);
        /* F face */
        std::cout << "  " << c(18 + row*3) << " " << c(19 + row*3) << " " << c(20 + row*3);
        /* R face */
        std::cout << "  " << c( 9 + row*3) << " " << c(10 + row*3) << " " << c(11 + row*3);
        /* B face */
        std::cout << "  " << c(45 + row*3) << " " << c(46 + row*3) << " " << c(47 + row*3);
        std::cout << "\n";
    }

    /* Print D face */
    std::cout << "         " << c(27) << " " << c(28) << " " << c(29) << "\n";
    std::cout << "         " << c(30) << " " << c(31) << " " << c(32) << "\n";
    std::cout << "         " << c(33) << " " << c(34) << " " << c(35) << "\n";
}

#endif /* CUBE_STATE_H */
