// ============================================================================
//  Liszt - a small but complete UCI chess engine
//  ----------------------------------------------------------------------------
//  Built on top of Disservin/chess-library (v0.9.4) for board representation
//  and legal move generation.  Everything else (search, evaluation, time
//  management, transposition table, UCI loop) is implemented here from scratch.
//
//  Features
//    * Full UCI protocol  -> plays in CuteChess, Arena, Banksia, etc.
//    * Negamax + alpha-beta pruning
//    * Iterative deepening with a hard time budget: it searches as deep as it
//      can and, the instant the clock runs out, returns the best move found so
//      far from the last fully searched line.
//    * Transposition table keyed by the library's incremental Zobrist hash
//      (board.hash()) -> cheaper re-search and far better pruning.
//    * Quiescence search -> resolves captures at the leaves so the engine
//      doesn't blunder into the "horizon effect".
//    * Move ordering: TT move -> MVV/LVA captures -> killers -> history.
//    * Null-move pruning + a one-ply check extension.
//    * Tapered PeSTO evaluation (material + piece-square tables blended between
//      middlegame and endgame by remaining material).
//
//  Build:  see README.md / Makefile / CMakeLists.txt
// ============================================================================

#include "chess.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace chess;
using Clock = std::chrono::steady_clock;

// ----------------------------------------------------------------------------
//  Engine-wide constants
// ----------------------------------------------------------------------------
static constexpr int INF          = 32000;   // "infinity" for alpha/beta bounds
static constexpr int MATE         = 30000;   // score for a forced mate at ply 0
static constexpr int MATE_IN_MAX  = MATE - 256;  // |score| above this => a mate
static constexpr int MAX_PLY      = 64;      // hard ceiling on search depth

// ============================================================================
//  1. EVALUATION  (tapered PeSTO: material + piece-square tables)
// ============================================================================
//
//  The tables below are written in the conventional "human readable" layout:
//  the first row is rank 8 (a8..h8) and the last row is rank 1 (a1..h1).
//
//  The chess-library numbers squares with index = rank*8 + file, where rank 0
//  is the FIRST rank.  So a white piece on library square `sq` reads the table
//  at index `sq ^ 56` (vertical flip), and a black piece reads it at `sq`
//  directly (which mirrors it to white's point of view).  See evaluate().
// ----------------------------------------------------------------------------

// Material values, separate for middlegame (mg) and endgame (eg). Order:
// PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING  (matches PieceType's int cast)
static const int MG_VALUE[6] = { 82, 337, 365, 477, 1025, 0 };
static const int EG_VALUE[6] = { 94, 281, 297, 512,  936, 0 };

// How much each piece contributes to the "game phase" (used to blend mg/eg).
static const int PHASE_INC[6] = { 0, 1, 1, 2, 4, 0 };
static constexpr int PHASE_MAX = 24;  // 4 minors + 4 rooks*2 + 2 queens*4

static const int MG_PAWN[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0,
};
static const int EG_PAWN[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};
static const int MG_KNIGHT[64] = {
   -167, -89, -34, -49,  61, -97, -15,-107,
    -73, -41,  72,  36,  23,  62,   7, -17,
    -47,  60,  37,  65,  84, 129,  73,  44,
     -9,  17,  19,  53,  37,  69,  18,  22,
    -13,   4,  16,  13,  28,  19,  21,  -8,
    -23,  -9,  12,  10,  19,  17,  25, -16,
    -29, -53, -12,  -3,  -1,  18, -14, -19,
   -105, -21, -58, -33, -17, -28, -19, -23,
};
static const int EG_KNIGHT[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};
static const int MG_BISHOP[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};
static const int EG_BISHOP[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17,
};
static const int MG_ROOK[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26,
};
static const int EG_ROOK[64] = {
     13,  10,  18,  15,  12,  12,   8,   5,
     11,  13,  13,  11,  -3,   3,   8,   3,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
      4,   3,  13,   1,   2,   1,  -1,   2,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -9,   2,   3,  -1,  -5, -13,   4, -20,
};
static const int MG_QUEEN[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};
static const int EG_QUEEN[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};
static const int MG_KING[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};
static const int EG_KING[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43,
};

// Convenient indexing: MG_PST[pieceType][square], EG_PST[pieceType][square]
static const int* MG_PST[6] = { MG_PAWN, MG_KNIGHT, MG_BISHOP, MG_ROOK, MG_QUEEN, MG_KING };
static const int* EG_PST[6] = { EG_PAWN, EG_KNIGHT, EG_BISHOP, EG_ROOK, EG_QUEEN, EG_KING };

static const PieceType PT_ORDER[6] = {
    PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
    PieceType::ROOK, PieceType::QUEEN,  PieceType::KING
};

// Returns the static evaluation in centipawns, from the side-to-move's POV
// (positive = good for the player on move). This is what negamax expects.
static int evaluate(const Board& board) {
    int mg = 0, eg = 0, phase = 0;

    const Color colors[2] = { Color::WHITE, Color::BLACK };
    for (int c = 0; c < 2; ++c) {
        const int sign = (c == 0) ? 1 : -1;  // white adds, black subtracts
        for (int pt = 0; pt < 6; ++pt) {
            Bitboard bb = board.pieces(PT_ORDER[pt], colors[c]);
            while (bb) {
                const int sq  = bb.pop();                 // 0..63 (library index)
                const int idx = (c == 0) ? (sq ^ 56) : sq; // flip for white
                mg += sign * (MG_VALUE[pt] + MG_PST[pt][idx]);
                eg += sign * (EG_VALUE[pt] + EG_PST[pt][idx]);
                phase += PHASE_INC[pt];
            }
        }
    }

    if (phase > PHASE_MAX) phase = PHASE_MAX;            // clamp (early promotions)
    // Blend middlegame and endgame scores by the current game phase.
    int score = (mg * phase + eg * (PHASE_MAX - phase)) / PHASE_MAX;

    return (board.sideToMove() == Color::WHITE) ? score : -score;
}

// ============================================================================
//  2. TRANSPOSITION TABLE  (keyed by the library's Zobrist hash)
// ============================================================================
//
//  Each searched position is stored so that if we reach it again (via a
//  different move order, or in the next iterative-deepening pass) we can reuse
//  the result instead of re-searching it. The "bound" flag records whether the
//  stored score is exact, or merely a lower/upper bound from a beta cutoff /
//  alpha failure.
// ----------------------------------------------------------------------------
enum Bound : uint8_t { BOUND_NONE = 0, BOUND_EXACT, BOUND_LOWER, BOUND_UPPER };

struct TTEntry {
    uint64_t key   = 0;     // full Zobrist key (collision check)
    uint16_t move  = 0;     // best move's raw bits (for ordering / PV)
    int16_t  score = 0;     // score, with mate distance stored relative to node
    uint8_t  depth = 0;     // search depth this score was obtained at
    uint8_t  flag  = BOUND_NONE;
};

class TranspositionTable {
public:
    explicit TranspositionTable(size_t mb = 64) { resize(mb); }

    void resize(size_t mb) {
        size_t bytes   = mb * 1024 * 1024;
        size_t entries = bytes / sizeof(TTEntry);
        count_ = 1;
        while (count_ * 2 <= entries) count_ *= 2;   // round down to power of two
        table_.assign(count_, TTEntry{});
        mask_ = count_ - 1;
    }

    void clear() { std::fill(table_.begin(), table_.end(), TTEntry{}); }

    // Probe: returns true and fills `out` if a matching entry exists.
    bool probe(uint64_t key, TTEntry& out) const {
        const TTEntry& e = table_[key & mask_];
        if (e.flag != BOUND_NONE && e.key == key) { out = e; return true; }
        return false;
    }

    // Store, preferring deeper / exact results (depth-preferred replacement).
    void store(uint64_t key, uint16_t move, int score, int depth, Bound flag) {
        TTEntry& e = table_[key & mask_];
        if (e.key == key && e.depth > depth && flag != BOUND_EXACT) return;
        e.key = key; e.move = move; e.score = (int16_t)score;
        e.depth = (uint8_t)depth; e.flag = flag;
    }

private:
    std::vector<TTEntry> table_;
    size_t count_ = 0, mask_ = 0;
};

// ============================================================================
//  3. SEARCH STATE & TIME MANAGEMENT
// ============================================================================
struct SearchLimits {
    long long wtime = 0, btime = 0, winc = 0, binc = 0;
    int  movestogo  = 0;
    long long movetime = 0;     // fixed ms per move (overrides clock logic)
    int  depth      = 0;        // fixed depth (0 = use time)
    bool infinite   = false;
};

class Engine {
public:
    Engine() : tt_(64) {}

    void setHash(size_t mb) { tt_.resize(mb); }
    void newGame()          { tt_.clear(); }

    // The public entry point used by the UCI loop. Runs a timed iterative
    // deepening search and prints "bestmove ...".
    void search(Board& board, const SearchLimits& limits);

private:
    int  negamax(Board& board, int depth, int alpha, int beta, int ply);
    int  quiescence(Board& board, int alpha, int beta, int ply);
    void scoreMoves(const Board& board, Movelist& moves, uint16_t ttMove, int ply);
    bool outOfTime();
    std::string buildPV(Board board, int maxLen);

    TranspositionTable tt_;

    // --- per-search bookkeeping ---
    std::atomic<bool> stop_{false};
    Clock::time_point start_;
    long long hardLimit_ = 0;    // ms; abort search once we pass this
    long long softLimit_ = 0;    // ms; don't *start* a new depth past this
    bool      useTime_   = true;
    uint64_t  nodes_     = 0;

    // Move ordering heuristics.
    uint16_t killers_[MAX_PLY][2] = {};         // two killer moves per ply
    int      history_[64][64]     = {};         // [from][to] success counts
};

// Returns true once the allotted time has elapsed (checked sparsely so the
// chrono call doesn't dominate). Sets the stop flag so the whole search unwinds.
bool Engine::outOfTime() {
    if (stop_.load(std::memory_order_relaxed)) return true;
    if (!useTime_) return false;
    if ((nodes_ & 2047) != 0) return false;     // only poll every 2048 nodes
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  Clock::now() - start_).count();
    if (ms >= hardLimit_) { stop_.store(true, std::memory_order_relaxed); return true; }
    return false;
}

// MVV/LVA table indexed [victim][attacker]: capture a valuable piece with a
// cheap one => high score. Used inside scoreMoves.
static inline int mvvLva(int victim, int attacker) {
    return victim * 16 - attacker;
}

// Assign an ordering score to every move so the most promising ones are tried
// first (which makes alpha-beta prune far more). Stored on the Move itself.
void Engine::scoreMoves(const Board& board, Movelist& moves, uint16_t ttMove, int ply) {
    for (auto& m : moves) {
        int s = 0;
        if (m.move() == ttMove) {
            s = 1 << 20;                                    // hash move: try first
        } else if (board.isCapture(m)) {
            int victim   = (m.typeOf() == Move::ENPASSANT)
                               ? (int)PieceType(PieceType::PAWN)
                               : (int)board.at<PieceType>(m.to());
            int attacker = (int)board.at<PieceType>(m.from());
            s = (1 << 16) + mvvLva(victim, attacker);       // winning captures high
        } else if (killers_[ply][0] == m.move()) {
            s = (1 << 15);                                  // first killer
        } else if (killers_[ply][1] == m.move()) {
            s = (1 << 15) - 1;                              // second killer
        } else {
            s = history_[m.from().index()][m.to().index()]; // quiet history
        }
        m.setScore((int16_t)std::min(s, 32767));            // clamp into int16
    }
    std::sort(moves.begin(), moves.end(),
              [](const Move& a, const Move& b) { return a.score() > b.score(); });
}

// ----------------------------------------------------------------------------
//  Quiescence search: at a leaf we keep resolving *captures* (and capture-
//  promotions) until the position is "quiet". This avoids evaluating a
//  position in the middle of a trade and grossly misjudging it.
// ----------------------------------------------------------------------------
int Engine::quiescence(Board& board, int alpha, int beta, int ply) {
    if (outOfTime()) return 0;
    ++nodes_;

    int standPat = evaluate(board);
    if (standPat >= beta) return standPat;          // already too good: cutoff
    if (standPat > alpha) alpha = standPat;         // can't do worse than this
    if (ply >= MAX_PLY) return standPat;

    Movelist captures;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(captures, board);
    scoreMoves(board, captures, 0, ply);

    int best = standPat;
    for (const auto& m : captures) {
        board.makeMove(m);
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmakeMove(m);

        if (stop_.load(std::memory_order_relaxed)) return 0;
        if (score > best) {
            best = score;
            if (score > alpha) alpha = score;
            if (alpha >= beta) break;               // fail-high
        }
    }
    return best;
}

// ----------------------------------------------------------------------------
//  Negamax with alpha-beta pruning, a transposition table, null-move pruning
//  and a one-ply check extension.
// ----------------------------------------------------------------------------
int Engine::negamax(Board& board, int depth, int alpha, int beta, int ply) {
    if (outOfTime()) return 0;

    const bool isRoot = (ply == 0);
    const bool pvNode = (beta - alpha) > 1;

    // Draw detection (repetition / 50-move / insufficient material). Treating a
    // single repetition as a draw inside the search is the standard trick.
    if (!isRoot && (board.isRepetition(1) || board.isHalfMoveDraw() ||
                    board.isInsufficientMaterial())) {
        return 0;
    }

    // Mate-distance pruning: never look for a mate longer than one we already
    // have, and keep mate scores meaningful relative to the root.
    if (!isRoot) {
        alpha = std::max(alpha, -MATE + ply);
        beta  = std::min(beta,   MATE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    const bool inCheck = board.inCheck();
    if (inCheck) depth += 1;                         // check extension

    if (depth <= 0) return quiescence(board, alpha, beta, ply);

    ++nodes_;
    const uint64_t key = board.hash();               // library's Zobrist hash

    // --- Transposition table probe ---
    uint16_t ttMove = 0;
    TTEntry tte;
    if (tt_.probe(key, tte)) {
        ttMove = tte.move;
        if (!pvNode && !isRoot && tte.depth >= depth) {
            int s = tte.score;
            if (s >  MATE_IN_MAX) s -= ply;          // un-adjust mate distance
            if (s < -MATE_IN_MAX) s += ply;
            if (tte.flag == BOUND_EXACT) return s;
            if (tte.flag == BOUND_LOWER && s >= beta) return s;
            if (tte.flag == BOUND_UPPER && s <= alpha) return s;
        }
    }

    // --- Null-move pruning ---
    // If we can "pass" and still be good enough to cause a beta cutoff, the
    // position is so strong we can prune. Skipped in check and in pawn-only
    // endgames (zugzwang danger).
    if (!pvNode && !inCheck && depth >= 3 &&
        board.hasNonPawnMaterial(board.sideToMove())) {
        int R = 2 + depth / 6;                        // reduction
        board.makeNullMove();
        int score = -negamax(board, depth - 1 - R, -beta, -beta + 1, ply + 1);
        board.unmakeNullMove();
        if (stop_.load(std::memory_order_relaxed)) return 0;
        if (score >= beta) return beta;               // fail-high: prune
    }

    // --- Generate & order moves ---
    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty()) {                              // no legal moves:
        return inCheck ? (-MATE + ply)                //   checkmate
                       : 0;                           //   stalemate
    }
    scoreMoves(board, moves, ttMove, ply);

    int  bestScore = -INF;
    uint16_t bestMove = 0;
    int  origAlpha = alpha;
    int  moveCount = 0;

    for (const auto& m : moves) {
        ++moveCount;
        board.makeMove(m);

        int score;
        if (moveCount == 1) {
            // Search the first (best-ordered) move with the full window.
            score = -negamax(board, depth - 1, -beta, -alpha, ply + 1);
        } else {
            // Late Move Reductions: quiet, late moves are searched shallower
            // first; only re-searched at full depth if they surprise us.
            int reduction = 0;
            if (depth >= 3 && moveCount > 3 && !inCheck && !board.isCapture(m) &&
                m.typeOf() != Move::PROMOTION) {
                reduction = 1 + (moveCount > 6 ? 1 : 0);
            }
            score = -negamax(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && reduction > 0)         // reduced search beat alpha
                score = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && score < beta)          // PV move: full window
                score = -negamax(board, depth - 1, -beta, -alpha, ply + 1);
        }

        board.unmakeMove(m);
        if (stop_.load(std::memory_order_relaxed)) return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMove  = m.move();
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    // Beta cutoff. Remember quiet moves that cause cutoffs so
                    // we try them earlier next time (killers + history).
                    if (!board.isCapture(m)) {
                        if (killers_[ply][0] != m.move()) {
                            killers_[ply][1] = killers_[ply][0];
                            killers_[ply][0] = m.move();
                        }
                        history_[m.from().index()][m.to().index()] += depth * depth;
                    }
                    break;
                }
            }
        }
    }

    // --- Store result in the transposition table ---
    Bound flag = (bestScore <= origAlpha) ? BOUND_UPPER
               : (bestScore >= beta)      ? BOUND_LOWER
                                          : BOUND_EXACT;
    int stored = bestScore;
    if (stored >  MATE_IN_MAX) stored += ply;         // adjust mate distance
    if (stored < -MATE_IN_MAX) stored -= ply;
    tt_.store(key, bestMove, stored, depth, flag);

    return bestScore;
}

// Walk the transposition table from the root, following best moves, to build a
// principal-variation string for the "info ... pv ..." UCI output.
std::string Engine::buildPV(Board board, int maxLen) {
    std::string pv;
    for (int i = 0; i < maxLen; ++i) {
        TTEntry e;
        if (!tt_.probe(board.hash(), e) || e.move == 0) break;
        Move m(e.move);
        // Make sure the stored move is actually legal in this position.
        Movelist legal;
        movegen::legalmoves(legal, board);
        bool ok = false;
        for (const auto& lm : legal) if (lm.move() == m.move()) { ok = true; break; }
        if (!ok) break;
        if (!pv.empty()) pv += ' ';
        pv += uci::moveToUci(m);
        board.makeMove(m);
        if (board.isRepetition(1)) break;             // avoid endless PV loops
    }
    return pv;
}

// ----------------------------------------------------------------------------
//  Iterative deepening driver + time allocation. Searches depth 1, 2, 3, ...
//  Each completed iteration improves move ordering for the next. The moment the
//  hard time limit trips, we stop and play the best move from the last fully
//  completed depth.
// ----------------------------------------------------------------------------
void Engine::search(Board& board, const SearchLimits& limits) {
    stop_.store(false);
    nodes_ = 0;
    start_ = Clock::now();
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));

    // ---- Decide how long to think ----
    int maxDepth = (limits.depth > 0) ? std::min(limits.depth, MAX_PLY) : MAX_PLY;
    useTime_ = !(limits.infinite || (limits.depth > 0 &&
                 limits.movetime == 0 && limits.wtime == 0 && limits.btime == 0));

    if (useTime_) {
        const long long overhead = 30;                // ms safety margin
        if (limits.movetime > 0) {
            hardLimit_ = std::max(1LL, limits.movetime - overhead);
            softLimit_ = hardLimit_;
        } else {
            long long t   = (board.sideToMove() == Color::WHITE) ? limits.wtime : limits.btime;
            long long inc = (board.sideToMove() == Color::WHITE) ? limits.winc  : limits.binc;
            long long mtg = (limits.movestogo > 0) ? limits.movestogo : 30;
            // Use a slice of the remaining time plus most of the increment.
            long long budget = t / mtg + (inc * 3) / 4;
            budget = std::min(budget, std::max(1LL, t - overhead));  // never overspend
            hardLimit_ = std::max(1LL, budget);
            softLimit_ = std::max(1LL, (budget * 60) / 100);         // ~60% to start a depth
        }
    }

    Move bestMove(0);
    int  bestScore = 0;

    for (int depth = 1; depth <= maxDepth; ++depth) {
        int score = negamax(board, depth, -INF, INF, 0);

        const bool aborted = stop_.load(std::memory_order_relaxed);

        // Pull the best move for this depth straight from the TT root entry.
        TTEntry rootEntry;
        Move depthBest(0);
        if (tt_.probe(board.hash(), rootEntry) && rootEntry.move != 0)
            depthBest = Move(rootEntry.move);

        // Accept the result if the depth finished, or if this is depth 1 (so we
        // always have *some* legal move to return even on a very short clock).
        if (!aborted || depth == 1) {
            if (depthBest != Move(0)) { bestMove = depthBest; bestScore = score; }

            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          Clock::now() - start_).count();
            long long nps = ms > 0 ? (long long)(nodes_ * 1000 / ms) : 0;

            // Report mate scores as "mate N", everything else as centipawns.
            std::ostringstream info;
            info << "info depth " << depth;
            if (std::abs(bestScore) >= MATE_IN_MAX) {
                int mateIn = (MATE - std::abs(bestScore) + 1) / 2;
                info << " score mate " << (bestScore > 0 ? mateIn : -mateIn);
            } else {
                info << " score cp " << bestScore;
            }
            info << " nodes " << nodes_ << " nps " << nps
                 << " time " << ms;
            std::string pv = buildPV(board, depth);
            if (!pv.empty()) info << " pv " << pv;
            std::cout << info.str() << std::endl;
        }

        if (aborted) break;

        // Stop early on a found mate, or if the soft time budget says the next
        // (much more expensive) depth almost certainly won't finish.
        if (std::abs(bestScore) >= MATE_IN_MAX) break;
        if (useTime_) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          Clock::now() - start_).count();
            if (ms >= softLimit_) break;
        }
    }

    if (bestMove == Move(0)) {            // safety net: pick any legal move
        Movelist legal;
        movegen::legalmoves(legal, board);
        if (!legal.empty()) bestMove = legal[0];
    }

    std::cout << "bestmove " << (bestMove == Move(0) ? "0000"
                                                     : uci::moveToUci(bestMove))
              << std::endl;
}

// ============================================================================
//  4. UCI PROTOCOL LOOP
// ============================================================================
//
//  Implements the subset of UCI that GUIs (CuteChess, Arena, ...) actually use:
//    uci / isready / ucinewgame / setoption / position / go / stop / quit
// ----------------------------------------------------------------------------
static void setPosition(Board& board, std::istringstream& is) {
    std::string token;
    is >> token;

    if (token == "startpos") {
        board.setFen(constants::STARTPOS);
        is >> token;                       // consume optional "moves"
    } else if (token == "fen") {
        std::string fen;
        while (is >> token && token != "moves") fen += token + " ";
        board.setFen(fen);
    }

    if (token == "moves") {                // replay the move list onto the board
        std::string mv;
        while (is >> mv) board.makeMove(uci::uciToMove(board, mv));
    }
}

static void parseGo(std::istringstream& is, SearchLimits& limits) {
    std::string token;
    while (is >> token) {
        if      (token == "wtime")     is >> limits.wtime;
        else if (token == "btime")     is >> limits.btime;
        else if (token == "winc")      is >> limits.winc;
        else if (token == "binc")      is >> limits.binc;
        else if (token == "movestogo") is >> limits.movestogo;
        else if (token == "movetime")  is >> limits.movetime;
        else if (token == "depth")     is >> limits.depth;
        else if (token == "infinite")  limits.infinite = true;
        // "nodes", "mate", "ponder", "searchmoves" are accepted but ignored.
    }
}

int main() {
    std::ios::sync_with_stdio(false);

    Engine engine;
    Board  board(constants::STARTPOS);

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream is(line);
        std::string cmd;
        is >> cmd;

        if (cmd == "uci") {
            std::cout << "id name Liszt 1.0\n";
            std::cout << "id author from-scratch (chess-library backend)\n";
            std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
            std::cout << "uciok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            engine.newGame();
            board.setFen(constants::STARTPOS);
        } else if (cmd == "setoption") {
            // format: setoption name <Name> value <Value>
            std::string token, name, value;
            is >> token;                   // "name"
            is >> name;
            is >> token;                   // "value"
            is >> value;
            if (name == "Hash") {
                try { engine.setHash(std::max(1, std::stoi(value))); } catch (...) {}
            }
        } else if (cmd == "position") {
            setPosition(board, is);
        } else if (cmd == "go") {
            SearchLimits limits;
            parseGo(is, limits);
            engine.search(board, limits);
        } else if (cmd == "stop") {
            // Search is synchronous and self-terminating via the time manager,
            // so there is nothing to interrupt here.
        } else if (cmd == "quit") {
            break;
        } else if (cmd == "d") {           // handy non-standard debug command
            std::cout << board << std::endl;
        }
    }
    return 0;
}
