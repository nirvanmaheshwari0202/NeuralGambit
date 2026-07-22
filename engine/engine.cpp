#include "chess.hpp"
#include "nnue.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace chess;
using Clock = std::chrono::steady_clock;

static constexpr int INF          = 32000;
static constexpr int MATE         = 30000;
static constexpr int MATE_IN_MAX  = MATE - 256;
static constexpr int MAX_PLY      = 64;

static const int MG_VALUE[6] = { 82, 337, 365, 477, 1025, 0 };
static const int EG_VALUE[6] = { 94, 281, 297, 512,  936, 0 };

static const int PHASE_INC[6] = { 0, 1, 1, 2, 4, 0 };
static constexpr int PHASE_MAX = 24;

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

static const int* MG_PST[6] = { MG_PAWN, MG_KNIGHT, MG_BISHOP, MG_ROOK, MG_QUEEN, MG_KING };
static const int* EG_PST[6] = { EG_PAWN, EG_KNIGHT, EG_BISHOP, EG_ROOK, EG_QUEEN, EG_KING };

static const PieceType PT_ORDER[6] = {
    PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
    PieceType::ROOK, PieceType::QUEEN,  PieceType::KING
};

static int evaluatePeSTO(const Board& board) {
    int mg = 0, eg = 0, phase = 0;

    const Color colors[2] = { Color::WHITE, Color::BLACK };
    for (int c = 0; c < 2; ++c) {
        const int sign = (c == 0) ? 1 : -1;
        for (int pt = 0; pt < 6; ++pt) {
            Bitboard bb = board.pieces(PT_ORDER[pt], colors[c]);
            while (bb) {
                const int sq  = bb.pop();
                const int idx = (c == 0) ? (sq ^ 56) : sq;
                mg += sign * (MG_VALUE[pt] + MG_PST[pt][idx]);
                eg += sign * (EG_VALUE[pt] + EG_PST[pt][idx]);
                phase += PHASE_INC[pt];
            }
        }
    }

    if (phase > PHASE_MAX) phase = PHASE_MAX;

    int score = (mg * phase + eg * (PHASE_MAX - phase)) / PHASE_MAX;

    return (board.sideToMove() == Color::WHITE) ? score : -score;
}

static int evaluate(const Board& board) {
    if (nnue::g_use && nnue::g_net.loaded) return nnue::g_net.evaluate(board);
    return evaluatePeSTO(board);
}

enum Bound : uint8_t { BOUND_NONE = 0, BOUND_EXACT, BOUND_LOWER, BOUND_UPPER };

struct TTEntry {
    uint64_t key   = 0;
    uint16_t move  = 0;
    int16_t  score = 0;
    uint8_t  depth = 0;
    uint8_t  flag  = BOUND_NONE;
};

class TranspositionTable {
public:
    explicit TranspositionTable(size_t mb = 64) { resize(mb); }

    void resize(size_t mb) {
        size_t bytes   = mb * 1024 * 1024;
        size_t entries = bytes / sizeof(TTEntry);
        count_ = 1;
        while (count_ * 2 <= entries) count_ *= 2;
        table_.assign(count_, TTEntry{});
        mask_ = count_ - 1;
    }

    void clear() { std::fill(table_.begin(), table_.end(), TTEntry{}); }

    bool probe(uint64_t key, TTEntry& out) const {
        const TTEntry& e = table_[key & mask_];
        if (e.flag != BOUND_NONE && e.key == key) { out = e; return true; }
        return false;
    }

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

struct SearchLimits {
    long long wtime = 0, btime = 0, winc = 0, binc = 0;
    int  movestogo  = 0;
    long long movetime = 0;
    int  depth      = 0;
    bool infinite   = false;
};

class Engine {
public:
    Engine() : tt_(64) {}

    void setHash(size_t mb) { tt_.resize(mb); }
    void newGame()          { tt_.clear(); }

    void search(Board& board, const SearchLimits& limits);

    std::pair<Move, int> searchFixedDepth(Board& board, int depth) {
        stop_.store(false);
        nodes_ = 0;
        useTime_ = false;
        std::memset(killers_, 0, sizeof(killers_));
        std::memset(history_, 0, sizeof(history_));
        int score = 0;
        Move best(0);
        for (int d = 1; d <= depth; ++d) {
            score = negamax(board, d, -INF, INF, 0);
            TTEntry e;
            if (tt_.probe(board.hash(), e) && e.move != 0) best = Move(e.move);
        }
        return { best, score };
    }

private:
    int  negamax(Board& board, int depth, int alpha, int beta, int ply);
    int  quiescence(Board& board, int alpha, int beta, int ply);
    void scoreMoves(const Board& board, Movelist& moves, uint16_t ttMove, int ply);
    bool outOfTime();
    std::string buildPV(Board board, int maxLen);

    TranspositionTable tt_;

    std::atomic<bool> stop_{false};
    Clock::time_point start_;
    long long hardLimit_ = 0;
    long long softLimit_ = 0;
    bool      useTime_   = true;
    uint64_t  nodes_     = 0;

    uint16_t killers_[MAX_PLY][2] = {};
    int      history_[64][64]     = {};
};

bool Engine::outOfTime() {
    if (stop_.load(std::memory_order_relaxed)) return true;
    if (!useTime_) return false;
    if ((nodes_ & 2047) != 0) return false;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  Clock::now() - start_).count();
    if (ms >= hardLimit_) { stop_.store(true, std::memory_order_relaxed); return true; }
    return false;
}

static inline int mvvLva(int victim, int attacker) {
    return victim * 16 - attacker;
}

void Engine::scoreMoves(const Board& board, Movelist& moves, uint16_t ttMove, int ply) {
    for (auto& m : moves) {
        int s = 0;
        if (m.move() == ttMove) {
            s = 1 << 20;
        } else if (board.isCapture(m)) {
            int victim   = (m.typeOf() == Move::ENPASSANT)
                               ? (int)PieceType(PieceType::PAWN)
                               : (int)board.at<PieceType>(m.to());
            int attacker = (int)board.at<PieceType>(m.from());
            s = (1 << 16) + mvvLva(victim, attacker);
        } else if (killers_[ply][0] == m.move()) {
            s = (1 << 15);
        } else if (killers_[ply][1] == m.move()) {
            s = (1 << 15) - 1;
        } else {
            s = history_[m.from().index()][m.to().index()];
        }
        m.setScore((int16_t)std::min(s, 32767));
    }
    std::sort(moves.begin(), moves.end(),
              [](const Move& a, const Move& b) { return a.score() > b.score(); });
}

int Engine::quiescence(Board& board, int alpha, int beta, int ply) {
    if (outOfTime()) return 0;
    ++nodes_;

    int standPat = evaluate(board);
    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;
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
            if (alpha >= beta) break;
        }
    }
    return best;
}

int Engine::negamax(Board& board, int depth, int alpha, int beta, int ply) {
    if (outOfTime()) return 0;

    const bool isRoot = (ply == 0);
    const bool pvNode = (beta - alpha) > 1;

    if (!isRoot && (board.isRepetition(1) || board.isHalfMoveDraw() ||
                    board.isInsufficientMaterial())) {
        return 0;
    }

    if (!isRoot) {
        alpha = std::max(alpha, -MATE + ply);
        beta  = std::min(beta,   MATE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    const bool inCheck = board.inCheck();
    if (inCheck) depth += 1;

    if (depth <= 0) return quiescence(board, alpha, beta, ply);

    ++nodes_;
    const uint64_t key = board.hash();

    uint16_t ttMove = 0;
    TTEntry tte;
    if (tt_.probe(key, tte)) {
        ttMove = tte.move;
        if (!pvNode && !isRoot && tte.depth >= depth) {
            int s = tte.score;
            if (s >  MATE_IN_MAX) s -= ply;
            if (s < -MATE_IN_MAX) s += ply;
            if (tte.flag == BOUND_EXACT) return s;
            if (tte.flag == BOUND_LOWER && s >= beta) return s;
            if (tte.flag == BOUND_UPPER && s <= alpha) return s;
        }
    }

    if (!pvNode && !inCheck && depth >= 3 &&
        board.hasNonPawnMaterial(board.sideToMove())) {
        int R = 2 + depth / 6;
        board.makeNullMove();
        int score = -negamax(board, depth - 1 - R, -beta, -beta + 1, ply + 1);
        board.unmakeNullMove();
        if (stop_.load(std::memory_order_relaxed)) return 0;
        if (score >= beta) return beta;
    }

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty()) {
        return inCheck ? (-MATE + ply)
                       : 0;
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

            score = -negamax(board, depth - 1, -beta, -alpha, ply + 1);
        } else {

            int reduction = 0;
            if (depth >= 3 && moveCount > 3 && !inCheck && !board.isCapture(m) &&
                m.typeOf() != Move::PROMOTION) {
                reduction = 1 + (moveCount > 6 ? 1 : 0);
            }
            score = -negamax(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && reduction > 0)
                score = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && score < beta)
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

    Bound flag = (bestScore <= origAlpha) ? BOUND_UPPER
               : (bestScore >= beta)      ? BOUND_LOWER
                                          : BOUND_EXACT;
    int stored = bestScore;
    if (stored >  MATE_IN_MAX) stored += ply;
    if (stored < -MATE_IN_MAX) stored -= ply;
    tt_.store(key, bestMove, stored, depth, flag);

    return bestScore;
}

std::string Engine::buildPV(Board board, int maxLen) {
    std::string pv;
    for (int i = 0; i < maxLen; ++i) {
        TTEntry e;
        if (!tt_.probe(board.hash(), e) || e.move == 0) break;
        Move m(e.move);

        Movelist legal;
        movegen::legalmoves(legal, board);
        bool ok = false;
        for (const auto& lm : legal) if (lm.move() == m.move()) { ok = true; break; }
        if (!ok) break;
        if (!pv.empty()) pv += ' ';
        pv += uci::moveToUci(m);
        board.makeMove(m);
        if (board.isRepetition(1)) break;
    }
    return pv;
}

void Engine::search(Board& board, const SearchLimits& limits) {
    stop_.store(false);
    nodes_ = 0;
    start_ = Clock::now();
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));

    int maxDepth = (limits.depth > 0) ? std::min(limits.depth, MAX_PLY) : MAX_PLY;
    useTime_ = !(limits.infinite || (limits.depth > 0 &&
                 limits.movetime == 0 && limits.wtime == 0 && limits.btime == 0));

    if (useTime_) {
        const long long overhead = 30;
        if (limits.movetime > 0) {
            hardLimit_ = std::max(1LL, limits.movetime - overhead);
            softLimit_ = hardLimit_;
        } else {
            long long t   = (board.sideToMove() == Color::WHITE) ? limits.wtime : limits.btime;
            long long inc = (board.sideToMove() == Color::WHITE) ? limits.winc  : limits.binc;
            long long mtg = (limits.movestogo > 0) ? limits.movestogo : 30;

            long long budget = t / mtg + (inc * 3) / 4;
            budget = std::min(budget, std::max(1LL, t - overhead));
            hardLimit_ = std::max(1LL, budget);
            softLimit_ = std::max(1LL, (budget * 60) / 100);
        }
    }

    Move bestMove(0);
    int  bestScore = 0;

    for (int depth = 1; depth <= maxDepth; ++depth) {
        int score = negamax(board, depth, -INF, INF, 0);

        const bool aborted = stop_.load(std::memory_order_relaxed);

        TTEntry rootEntry;
        Move depthBest(0);
        if (tt_.probe(board.hash(), rootEntry) && rootEntry.move != 0)
            depthBest = Move(rootEntry.move);

        if (!aborted || depth == 1) {
            if (depthBest != Move(0)) { bestMove = depthBest; bestScore = score; }

            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          Clock::now() - start_).count();
            long long nps = ms > 0 ? (long long)(nodes_ * 1000 / ms) : 0;

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

        if (std::abs(bestScore) >= MATE_IN_MAX) break;
        if (useTime_) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          Clock::now() - start_).count();
            if (ms >= softLimit_) break;
        }
    }

    if (bestMove == Move(0)) {
        Movelist legal;
        movegen::legalmoves(legal, board);
        if (!legal.empty()) bestMove = legal[0];
    }

    std::cout << "bestmove " << (bestMove == Move(0) ? "0000"
                                                     : uci::moveToUci(bestMove))
              << std::endl;
}

static void setPosition(Board& board, std::istringstream& is) {
    std::string token;
    is >> token;

    if (token == "startpos") {
        board.setFen(constants::STARTPOS);
        is >> token;
    } else if (token == "fen") {
        std::string fen;
        while (is >> token && token != "moves") fen += token + " ";
        board.setFen(fen);
    }

    if (token == "moves") {
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

    }
}

static int runDatagen(int argc, char** argv) {
    int games = 100, depth = 6, randPlies = 8, maxMoves = 240;
    std::string outPath = "data.txt";
    unsigned seed = 42;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string k = argv[i], v = argv[i + 1];
        if      (k == "games")    games     = std::stoi(v);
        else if (k == "depth")    depth     = std::stoi(v);
        else if (k == "rand")     randPlies = std::stoi(v);
        else if (k == "maxmoves") maxMoves  = std::stoi(v);
        else if (k == "out")      outPath   = v;
        else if (k == "seed")     seed      = (unsigned)std::stoul(v);
    }

    std::mt19937 rng(seed);
    std::ofstream out(outPath);
    if (!out) { std::cerr << "cannot open " << outPath << "\n"; return 1; }

    Engine engine;
    long long totalPositions = 0;

    for (int g = 0; g < games; ++g) {
        engine.newGame();
        Board board(constants::STARTPOS);

        bool bad = false;
        for (int i = 0; i < randPlies; ++i) {
            Movelist ml; movegen::legalmoves(ml, board);
            if (ml.empty()) { bad = true; break; }
            board.makeMove(ml[std::uniform_int_distribution<int>(0, ml.size() - 1)(rng)]);
        }
        if (bad || board.isGameOver().second != GameResult::NONE) continue;

        std::vector<std::string> fens;
        std::vector<int>         scores;
        double whiteResult = 0.5;
        int ply = 0;
        while (true) {
            auto over = board.isGameOver();
            if (over.second != GameResult::NONE) {
                if (over.second == GameResult::LOSE)
                    whiteResult = (board.sideToMove() == Color::WHITE) ? 0.0 : 1.0;
                else
                    whiteResult = 0.5;
                break;
            }
            if (ply >= maxMoves) break;

            auto [mv, score] = engine.searchFixedDepth(board, depth);
            if (mv == Move(0)) break;

            if (!board.inCheck() && std::abs(score) < 20000) {

                int whiteScore = (board.sideToMove() == Color::WHITE) ? score : -score;
                fens.push_back(board.getFen());
                scores.push_back(whiteScore);
            }
            board.makeMove(mv);
            ++ply;
        }

        for (size_t i = 0; i < fens.size(); ++i) {
            out << fens[i] << " | " << scores[i] << " | " << whiteResult << "\n";
            ++totalPositions;
        }
        if ((g + 1) % 25 == 0)
            std::cerr << "game " << (g + 1) << "/" << games
                      << "  positions " << totalPositions << "\r" << std::flush;
    }
    std::cerr << "\nwrote " << totalPositions << " positions to " << outPath << "\n";
    return 0;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);

    if (argc >= 2 && std::string(argv[1]) == "datagen")
        return runDatagen(argc, argv);

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
            std::cout << "option name EvalFile type string default <empty>\n";
            std::cout << "option name UseNNUE type check default false\n";
            std::cout << "uciok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            engine.newGame();
            board.setFen(constants::STARTPOS);
        } else if (cmd == "setoption") {

            std::string token, name, value;
            is >> token;
            is >> name;
            is >> token;
            is >> value;
            if (name == "Hash") {
                try { engine.setHash(std::max(1, std::stoi(value))); } catch (...) {}
            } else if (name == "EvalFile") {
                if (nnue::g_net.load(value)) {
                    nnue::g_use = true;
                    std::cout << "info string loaded NNUE " << value
                              << " (HL=" << nnue::g_net.HL << ")" << std::endl;
                } else {
                    std::cout << "info string FAILED to load NNUE " << value << std::endl;
                }
            } else if (name == "UseNNUE") {
                nnue::g_use = (value == "true" || value == "1");
            }
        } else if (cmd == "position") {
            setPosition(board, is);
        } else if (cmd == "go") {
            SearchLimits limits;
            parseGo(is, limits);
            engine.search(board, limits);
        } else if (cmd == "stop") {

        } else if (cmd == "quit") {
            break;
        } else if (cmd == "d") {
            std::cout << board << std::endl;
        } else if (cmd == "eval") {
            std::cout << "nnue " << (nnue::g_use && nnue::g_net.loaded)
                      << "  eval " << evaluate(board)
                      << "  (pesto " << evaluatePeSTO(board) << ")" << std::endl;
        }
    }
    return 0;
}
