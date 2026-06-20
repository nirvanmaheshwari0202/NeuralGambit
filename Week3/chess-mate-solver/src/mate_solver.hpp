#ifndef MATE_SOLVER_HPP
#define MATE_SOLVER_HPP

// ===========================================================================
//  EngineSolver: a forced-mate ("mate in N") puzzle solver.
//
//  The engine performs an exhaustive AND/OR search:
//    * On the *attacker's* turn (the side to move at the root) we need to find
//      AT LEAST ONE move that forces mate  -> an OR node.
//    * On the *defender's* turn we need EVERY reply to still lose            -> an AND node.
//
//  A "mate in N" corresponds to a forcing line of (2*N - 1) half-moves
//  (plies): the attacker moves N times and the defender moves N-1 times.
//
//  The search is *complete*: it considers every legal move, not just checks,
//  so it also finds quiet "waiting move" (zugzwang) mates such as
//      5K1k/6pp/7R/8/8/8/8/6R1 w - - 0 1  ->  Rgg6 gxh6 Rg8#
//  Checking/forcing moves are merely ordered first to reach solutions faster.
// ===========================================================================

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../include/chess.hpp"

namespace mate {

using namespace chess;

// Outcome of a solve attempt.
struct SolveResult {
    bool                 solved       = false;  // a forced mate was found
    int                  mateInMoves  = 0;       // N (full moves) of the shortest mate found
    std::vector<Move>    line;                   // the principal forcing line (attacker + defender moves)
    std::uint64_t        nodes        = 0;       // search nodes visited
    double               seconds      = 0.0;     // wall-clock time spent

    explicit operator bool() const { return solved; }
};

// ---------------------------------------------------------------------------
//  The Engine solver class.
// ---------------------------------------------------------------------------
class EngineSolver {
   public:
    EngineSolver() = default;

    // Highest mate distance (in full moves) the solver will look for when the
    // caller does not specify one. mate-in-5 already implies a 9-ply tree.
    void setMaxMate(int n) { max_mate_ = n; }
    int  maxMate() const { return max_mate_; }

    // Toggle ordering checks/captures first. On by default (pure speed; the
    // result is identical either way because the search is exhaustive).
    void setMoveOrdering(bool on) { order_moves_ = on; }

    // -----------------------------------------------------------------------
    //  Solve from a FEN string. Uses iterative deepening so it always returns
    //  the *shortest* forced mate, trying mate-in-1, mate-in-2, ... up to
    //  maxMate() (or `knownMateIn` when the puzzle's depth is known).
    // -----------------------------------------------------------------------
    SolveResult solve(const std::string& fen, int knownMateIn = -1) {
        Board board(fen);
        return solve(board, knownMateIn);
    }

    SolveResult solve(Board& board, int knownMateIn = -1) {
        SolveResult result;
        nodes_ = 0;
        const auto start = std::chrono::steady_clock::now();

        const int limit = (knownMateIn > 0) ? knownMateIn : max_mate_;

        for (int n = 1; n <= limit; ++n) {
            std::vector<Move> line;
            if (searchMate(board, 2 * n - 1, line)) {
                result.solved      = true;
                result.mateInMoves = n;
                result.line        = std::move(line);
                break;
            }
        }

        const auto end = std::chrono::steady_clock::now();
        result.nodes   = nodes_;
        result.seconds = std::chrono::duration<double>(end - start).count();
        return result;
    }

    std::uint64_t lastNodeCount() const { return nodes_; }

   private:
    int           max_mate_    = 5;
    bool          order_moves_ = true;
    std::uint64_t nodes_       = 0;

    // -----------------------------------------------------------------------
    //  Core recursive AND/OR mate search.
    //
    //  Precondition: it is the *attacker's* turn (the side that is trying to
    //  deliver mate). `plies` is the half-move budget remaining and is always
    //  odd here, since the attacker delivers the final move.
    //
    //  Returns true iff the attacker can force checkmate within `plies`
    //  half-moves; on success `pv` holds the forcing line found.
    // -----------------------------------------------------------------------
    bool searchMate(Board& board, int plies, std::vector<Move>& pv) {
        if (plies <= 0) return false;
        ++nodes_;

        Movelist moves;
        movegen::legalmoves(moves, board);
        if (moves.empty()) return false;  // attacker is mated/stalemated: cannot win

        if (order_moves_) orderMoves(board, moves);

        for (const Move m : moves) {
            board.makeMove(m);

            // Generate the defender's replies once and reuse them both for the
            // terminal test (no replies) and the AND-node recursion.
            Movelist replies;
            movegen::legalmoves(replies, board);

            if (replies.empty()) {
                // Defender has no move: checkmate (good) or stalemate (useless).
                if (board.inCheck()) {
                    pv.assign(1, m);
                    board.unmakeMove(m);
                    return true;
                }
                board.unmakeMove(m);  // stalemate: this attacker move does not mate
                continue;
            }

            // The defender survives this move, so we need at least one more
            // attacker move after it. If the budget is exhausted, m fails.
            if (plies == 1) {
                board.unmakeMove(m);
                continue;
            }

            // AND node: every defender reply must still lead to a forced mate.
            bool              allRefuted = true;
            std::vector<Move> toughestDefence;  // kept only for a readable PV

            for (const Move d : replies) {
                board.makeMove(d);
                std::vector<Move> sub;
                const bool mated = searchMate(board, plies - 2, sub);
                board.unmakeMove(d);

                if (!mated) {  // defender escapes -> attacker move m refuted
                    allRefuted = false;
                    break;
                }
                // Remember the longest sub-line so the displayed PV shows the
                // defender putting up the stiffest (longest) resistance.
                if (static_cast<int>(sub.size()) + 1 > static_cast<int>(toughestDefence.size())) {
                    toughestDefence.clear();
                    toughestDefence.push_back(d);
                    toughestDefence.insert(toughestDefence.end(), sub.begin(), sub.end());
                }
            }

            if (allRefuted) {
                pv.clear();
                pv.push_back(m);
                pv.insert(pv.end(), toughestDefence.begin(), toughestDefence.end());
                board.unmakeMove(m);
                return true;
            }

            board.unmakeMove(m);
        }

        return false;
    }

    // -----------------------------------------------------------------------
    //  Move ordering: try forcing moves first (checks, then captures, then
    //  promotions). Purely a speed heuristic -- completeness is unaffected.
    // -----------------------------------------------------------------------
    void orderMoves(const Board& board, Movelist& moves) const {
        for (Move& m : moves) {
            int16_t score = 0;
            if (board.givesCheck(m) != CheckType::NO_CHECK) score += 10000;
            if (board.isCapture(m)) score += 1000;
            if (m.typeOf() == Move::PROMOTION) score += 500;
            m.setScore(score);
        }
        std::sort(moves.begin(), moves.end(),
                  [](const Move& a, const Move& b) { return a.score() > b.score(); });
    }
};

}  // namespace mate

#endif  // MATE_SOLVER_HPP
