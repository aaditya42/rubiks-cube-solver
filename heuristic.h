/*
 * ============================================================================
 *  heuristic.h — Admissible Heuristics for Rubik's Cube IDA*
 * ============================================================================
 *
 *  Provides two heuristics:
 *    1. Corner Orientation Database (3^7 = 2187 entries, BFS-precomputed)
 *    2. Edge Orientation Database (2^11 = 2048 entries, BFS-precomputed)
 *    3. Combined heuristic: max of multiple pattern databases
 *
 *  All heuristics are admissible (never overestimate) and consistent,
 *  guaranteeing IDA* finds optimal solutions.
 *
 *  Additionally includes a corner permutation database (8! = 40320 entries)
 *  for a stronger lower bound.
 * ============================================================================
 */

#ifndef HEURISTIC_H
#define HEURISTIC_H

#include "cube_state.h"
#include <queue>
#include <cstring>
#include <algorithm>

/* --------------------------------------------------------------------------
 *  Corner & Edge extraction from facelet representation
 * --------------------------------------------------------------------------
 *
 *  We define 8 corners and 12 edges by their constituent facelets.
 *  For each corner/edge we identify orientation by which facelet
 *  carries the U or D color (for corners) or the U/D/F/B color (for edges).
 *
 *  Corner positions (and their 3 facelets in order [U/D, CW1, CW2]):
 *    0: URF = (U8, R0, F2)  = facelets (8, 9, 20)
 *    1: UFL = (U6, F0, L2)  = facelets (6, 18, 38)
 *    2: ULB = (U0, L0, B2)  = facelets (0, 36, 47)
 *    3: UBR = (U2, B0, R2)  = facelets (2, 45, 11)
 *    4: DFR = (D2, F8, R6)  = facelets (29, 26, 15)
 *    5: DLF = (D0, L8, F6)  = facelets (27, 44, 24)
 *    6: DBL = (D6, B8, L6)  = facelets (33, 53, 42)
 *    7: DRB = (D8, R8, B6)  = facelets (35, 17, 51)
 *
 *  Edge positions (and their 2 facelets [primary, secondary]):
 *    Primary = the facelet on U/D face, or on F/B face if no U/D facelet.
 *    0: UR = (U5, R1)  = (5, 10)
 *    1: UF = (U7, F1)  = (7, 19)
 *    2: UL = (U3, L1)  = (3, 37)
 *    3: UB = (U1, B1)  = (1, 46)
 *    4: DR = (D5, R7)  = (32, 16)
 *    5: DF = (D1, F7)  = (28, 25)
 *    6: DL = (D3, L7)  = (30, 43)
 *    7: DB = (D7, B7)  = (34, 52)
 *    8: FR = (F5, R3)  = (23, 12)
 *    9: FL = (F3, L5)  = (21, 41)
 *   10: BL = (B5, L3)  = (50, 39)
 *   11: BR = (B3, R5)  = (48, 14)
 * -------------------------------------------------------------------------- */

/* Corner facelet triplets: [corner_id][0]=U/D facelet, [1]=CW1, [2]=CW2 */
static const int CORNER_FACELETS[8][3] = {
    { 8,  9, 20}, /* URF */
    { 6, 18, 38}, /* UFL */
    { 0, 36, 47}, /* ULB */
    { 2, 45, 11}, /* UBR */
    {29, 26, 15}, /* DFR */
    {27, 44, 24}, /* DLF */
    {33, 53, 42}, /* DBL */
    {35, 17, 51}, /* DRB */
};

/* The "home" color of each corner's U/D facelet (color that should be at [0]) */
static const uint8_t CORNER_UD_COLOR[8] = { 0,0,0,0, 3,3,3,3 }; /* U=0, D=3 */

/* Edge facelet pairs: [edge_id][0]=primary, [1]=secondary */
static const int EDGE_FACELETS[12][2] = {
    { 5, 10}, /* UR */
    { 7, 19}, /* UF */
    { 3, 37}, /* UL */
    { 1, 46}, /* UB */
    {32, 16}, /* DR */
    {28, 25}, /* DF */
    {30, 43}, /* DL */
    {34, 52}, /* DB */
    {23, 12}, /* FR */
    {21, 41}, /* FL */
    {50, 39}, /* BL */
    {48, 14}, /* BR */
};

/* --------------------------------------------------------------------------
 *  Corner orientation index: encodes the orientation of all 8 corners
 *  as a number in [0, 2187). Only 7 corners are independent (the 8th
 *  is determined by the constraint that total orientation ≡ 0 mod 3).
 *
 *  For each corner, orientation is 0 if the U/D colored facelet is on
 *  the U or D face, 1 if it's rotated CW, 2 if CCW.
 * -------------------------------------------------------------------------- */

inline int get_corner_orientation_index(const CubeState& cube) {
    int idx = 0;
    for (int c = 0; c < 7; ++c) {
        /* Find which of the 3 facelets has the U/D color */
        uint8_t f0 = cube.f[CORNER_FACELETS[c][0]];
        uint8_t f1 = cube.f[CORNER_FACELETS[c][1]];
        uint8_t f2 = cube.f[CORNER_FACELETS[c][2]];

        int ori;
        if (f0 == 0 || f0 == 3) ori = 0;      /* U/D color is at home position */
        else if (f1 == 0 || f1 == 3) ori = 1;  /* rotated CW */
        else ori = 2;                           /* rotated CCW */

        idx = idx * 3 + ori;
    }
    return idx;
}

/* --------------------------------------------------------------------------
 *  Edge orientation index: encodes the orientation of all 12 edges
 *  as a number in [0, 2048). Only 11 edges are independent.
 *
 *  Edge orientation: 0 if the primary-colored facelet is in its primary
 *  position, 1 if flipped. An edge is "good" (ori=0) if it can be solved
 *  without F or B moves.
 * -------------------------------------------------------------------------- */

inline int get_edge_orientation_index(const CubeState& cube) {
    int idx = 0;
    for (int e = 0; e < 11; ++e) {
        uint8_t f0 = cube.f[EDGE_FACELETS[e][0]];
        /* Edge is correctly oriented if primary facelet has U/D color
           (for UD edges) or F/B color (for equatorial edges) */
        int ori;
        if (e < 8) {
            /* UD-layer edge: primary facelet should have U or D color */
            ori = (f0 == 0 || f0 == 3) ? 0 : 1;
        } else {
            /* Equatorial edge: primary facelet should have F or B color */
            ori = (f0 == 2 || f0 == 5) ? 0 : 1;
        }
        idx = idx * 2 + ori;
    }
    return idx;
}

/* --------------------------------------------------------------------------
 *  UD-slice coordinate: tracks which 4 of 12 edge positions hold
 *  equatorial-slice edges (FR, FL, BL, BR = edges 8-11).
 *  This is a combination C(12,4) = 495 possible values.
 *  Provides information about edge permutation without full encoding.
 * -------------------------------------------------------------------------- */

inline int identify_edge(const CubeState& cube, int pos) {
    /* Identify which edge cubie is at position `pos` */
    uint8_t f0 = cube.f[EDGE_FACELETS[pos][0]];
    uint8_t f1 = cube.f[EDGE_FACELETS[pos][1]];

    /* Match against known edge color pairs */
    static const uint8_t EDGE_COLORS[12][2] = {
        {0,1},{0,2},{0,4},{0,5},  /* UR,UF,UL,UB */
        {3,1},{3,2},{3,4},{3,5},  /* DR,DF,DL,DB */
        {2,1},{2,4},{5,4},{5,1},  /* FR,FL,BL,BR */
    };

    for (int e = 0; e < 12; ++e) {
        if ((f0 == EDGE_COLORS[e][0] && f1 == EDGE_COLORS[e][1]) ||
            (f1 == EDGE_COLORS[e][0] && f0 == EDGE_COLORS[e][1]))
            return e;
    }
    return -1;
}

inline int get_ud_slice_index(const CubeState& cube) {
    /*
     * Compute the combinatorial index for which 4 positions
     * (out of 12) contain equatorial edges (id >= 8).
     * Uses the combinatorial number system.
     */
    int occupied[4];
    int count = 0;

    for (int pos = 0; pos < 12 && count < 4; ++pos) {
        int edge_id = identify_edge(cube, pos);
        if (edge_id >= 8) {
            occupied[count++] = pos;
        }
    }

    /* Encode using combinatorial number system: C(occupied[0],1) + C(occupied[1],2) + ... */
    /* Precomputed C(n,k) values */
    static const int C[13][5] = {
        {1,0,0,0,0},{1,1,0,0,0},{1,2,1,0,0},{1,3,3,1,0},{1,4,6,4,1},
        {1,5,10,10,5},{1,6,15,20,15},{1,7,21,35,35},{1,8,28,56,70},
        {1,9,36,84,126},{1,10,45,120,210},{1,11,55,165,330},{1,12,66,220,495}
    };

    int idx = 0;
    for (int i = 0; i < 4; ++i)
        idx += C[occupied[i]][i + 1];
    return idx;
}

/* --------------------------------------------------------------------------
 *  Corner permutation index: Lehmer code encoding of the permutation
 *  of all 8 corners. Range: [0, 40320).
 *
 *  We identify each corner by its "home" U/D-color + CW1-color pair.
 * -------------------------------------------------------------------------- */

/* Colors that identify each corner (U/D color, CW1 color) */
static const uint8_t CORNER_ID_COLORS[8][2] = {
    {0, 1}, /* URF: U,R */
    {0, 2}, /* UFL: U,F */
    {0, 4}, /* ULB: U,L */
    {0, 5}, /* UBR: U,B */
    {3, 2}, /* DFR: D,F */
    {3, 4}, /* DLF: D,L */
    {3, 5}, /* DBL: D,B */
    {3, 1}, /* DRB: D,R */
};

inline int identify_corner(const CubeState& cube, int pos) {
    /*
     * Determine which corner cubie is at position `pos`.
     * We look at the 3 facelets and match them to known corner identities.
     */
    uint8_t colors[3];
    for (int i = 0; i < 3; ++i)
        colors[i] = cube.f[CORNER_FACELETS[pos][i]];

    /* Find the U/D colored facelet */
    int ud_idx = -1;
    for (int i = 0; i < 3; ++i)
        if (colors[i] == 0 || colors[i] == 3) { ud_idx = i; break; }

    uint8_t ud_color = colors[ud_idx];
    uint8_t cw1_color = colors[(ud_idx + 1) % 3];

    for (int c = 0; c < 8; ++c)
        if (CORNER_ID_COLORS[c][0] == ud_color && CORNER_ID_COLORS[c][1] == cw1_color)
            return c;

    return -1; /* should never happen on valid cube */
}

inline int get_corner_perm_index(const CubeState& cube) {
    int perm[8];
    for (int i = 0; i < 8; ++i)
        perm[i] = identify_corner(cube, i);

    /* Lehmer code */
    int idx = 0;
    for (int i = 0; i < 8; ++i) {
        int count = 0;
        for (int j = i + 1; j < 8; ++j)
            if (perm[j] < perm[i]) ++count;
        idx = idx * (8 - i) + count;
    }
    return idx;
}

/* --------------------------------------------------------------------------
 *  Pattern Database — BFS from solved state
 * --------------------------------------------------------------------------
 *  We precompute the minimum number of moves to solve each sub-problem
 *  (corner orientation, edge orientation, corner permutation) by running
 *  BFS backward from the solved state.
 * -------------------------------------------------------------------------- */

static const int CORNER_ORI_SIZE  = 2187;      /* 3^7 */
static const int EDGE_ORI_SIZE    = 2048;      /* 2^11 */
static const int CORNER_PERM_SIZE = 40320;     /* 8! */
static const int UD_SLICE_SIZE    = 495;       /* C(12,4) */
static const int COMBINED_SIZE    = 2187 * 495; /* corner_ori × ud_slice = 1,082,565 */

/* Global pattern databases (initialized once) */
static uint8_t corner_ori_db[CORNER_ORI_SIZE];
static uint8_t edge_ori_db[EDGE_ORI_SIZE];
static uint8_t corner_perm_db[CORNER_PERM_SIZE];
static uint8_t* combined_db = nullptr;  /* dynamically allocated: ~1MB */
static bool heuristic_initialized = false;

/* Combined coordinate: corner_orientation * 495 + ud_slice */
inline int get_combined_index(const CubeState& cube) {
    return get_corner_orientation_index(cube) * UD_SLICE_SIZE + get_ud_slice_index(cube);
}

/*
 * build_pattern_db: generic BFS pattern database builder.
 * Takes a function that computes an index from a CubeState,
 * and fills the database with minimum move counts.
 */
template<typename IndexFunc>
void build_pattern_db(uint8_t* db, int db_size, IndexFunc get_index) {
    std::memset(db, 0xFF, db_size); /* 0xFF = unvisited */

    CubeState solved;
    int start_idx = get_index(solved);
    db[start_idx] = 0;

    /* BFS using (state, depth) pairs */
    std::queue<CubeState> q;
    q.push(solved);

    while (!q.empty()) {
        CubeState cur = q.front();
        q.pop();
        int cur_idx = get_index(cur);
        uint8_t cur_depth = db[cur_idx];

        /* Limit depth to keep BFS tractable */
        if (cur_depth >= 12) continue;

        for (int m = 0; m < NUM_MOVES; ++m) {
            CubeState next = cur.after_move(m);
            int next_idx = get_index(next);
            if (db[next_idx] == 0xFF) {
                db[next_idx] = cur_depth + 1;
                q.push(next);
            }
        }
    }

    /* Any unreached states get a conservative estimate */
    for (int i = 0; i < db_size; ++i)
        if (db[i] == 0xFF) db[i] = 11;
}

/*
 * init_heuristic: Build all pattern databases.
 * Called once at program startup.
 */
inline void init_heuristic() {
    if (heuristic_initialized) return;

    std::cout << "Building pattern databases..." << std::flush;

    build_pattern_db(corner_ori_db, CORNER_ORI_SIZE, get_corner_orientation_index);
    std::cout << " [corner_ori]" << std::flush;

    build_pattern_db(edge_ori_db, EDGE_ORI_SIZE, get_edge_orientation_index);
    std::cout << " [edge_ori]" << std::flush;

    build_pattern_db(corner_perm_db, CORNER_PERM_SIZE, get_corner_perm_index);
    std::cout << " [corner_perm]" << std::flush;

    /* Build combined corner_ori × ud_slice database (~1MB) */
    combined_db = new uint8_t[COMBINED_SIZE];
    build_pattern_db(combined_db, COMBINED_SIZE, get_combined_index);
    std::cout << " [combined]" << std::flush;

    std::cout << " done.\n";
    heuristic_initialized = true;
}

/* --------------------------------------------------------------------------
 *  Combined heuristic: max of all pattern database lookups
 * -------------------------------------------------------------------------- */

inline int heuristic(const CubeState& cube) {
    int h1 = corner_ori_db[get_corner_orientation_index(cube)];
    int h2 = edge_ori_db[get_edge_orientation_index(cube)];
    int h3 = corner_perm_db[get_corner_perm_index(cube)];
    int h4 = combined_db[get_combined_index(cube)];
    return std::max({h1, h2, h3, h4});
}

#endif /* HEURISTIC_H */
