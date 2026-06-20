// ===========================================================================
//  Chess mate-in-N puzzle solver  --  command-line driver.
//
//  Usage:
//    mate_solver <puzzles.json> [--mate N] [--limit K] [--no-order] [--quiet]
//    mate_solver --text <puzzles.txt> [--mate N] [--limit K]
//    mate_solver --fen "<FEN>" [--mate N]
//
//  Examples:
//    ./mate_solver puzzles/mate_in_2.json --mate 2
//    ./mate_solver --fen "4r1rk/5K1b/7R/R7/8/8/8/8 w - - 0 1" --mate 2
// ===========================================================================

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "../include/chess.hpp"
#include "mate_solver.hpp"
#include "puzzle.hpp"

using namespace chess;
using namespace mate;

// Replay a forcing line from `fen` and render it as a SAN string.
static std::string lineToSan(const std::string& fen, const std::vector<Move>& line) {
    Board board(fen);
    std::string out;
    // Number the line from 1 (datasets do the same, and some FENs carry an
    // invalid fullmove field such as ".. 1 0").
    int  moveNo      = 1;
    bool whiteToMove = board.sideToMove() == Color::WHITE;

    for (std::size_t i = 0; i < line.size(); ++i) {
        if (whiteToMove) {
            out += std::to_string(moveNo) + ". ";
        } else if (i == 0) {
            out += std::to_string(moveNo) + "... ";
        }
        out += uci::moveToSan(board, line[i]) + " ";
        board.makeMove(line[i]);
        if (!whiteToMove) ++moveNo;
        whiteToMove = !whiteToMove;
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// Normalise a SAN token for comparison (drop check/mate/annotation glyphs and
// unify promotion notation: the dataset writes "e8/Q", SAN writes "e8=Q").
static std::string normSan(std::string s) {
    std::string out;
    for (char c : s) {
        if (c == '+' || c == '#' || c == '!' || c == '?') continue;
        out.push_back(c == '/' ? '=' : c);
    }
    return out;
}

// Pull the first actual move (not a move number) out of a reference solution.
static std::string referenceFirstMove(const std::string& solution) {
    std::istringstream iss(solution);
    std::string tok;
    while (iss >> tok) {
        // Skip pure move-number tokens like "1." or "1..." .
        bool isNumber = !tok.empty() && std::isdigit(static_cast<unsigned char>(tok[0]));
        bool onlyNumDots = isNumber;
        for (char c : tok)
            if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.') onlyNumDots = false;
        if (onlyNumDots) continue;
        return normSan(tok);
    }
    return "";
}

static void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <puzzles.json> [--mate N] [--limit K] [--no-order] [--quiet]\n"
              << "  " << prog << " --text <puzzles.txt> [--mate N] [--limit K] [--no-order] [--quiet]\n"
              << "  " << prog << " --fen \"<FEN>\" [--mate N]\n";
}

int main(int argc, char* argv[]) {
    std::string jsonPath, textPath, fen;
    int  mateIn = -1;
    int  limit  = -1;
    bool order  = true;
    bool quiet  = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--fen" && i + 1 < argc) {
            fen = argv[++i];
        } else if (a == "--text" && i + 1 < argc) {
            textPath = argv[++i];
        } else if (a == "--mate" && i + 1 < argc) {
            mateIn = std::stoi(argv[++i]);
        } else if (a == "--limit" && i + 1 < argc) {
            limit = std::stoi(argv[++i]);
        } else if (a == "--no-order") {
            order = false;
        } else if (a == "--quiet") {
            quiet = true;
        } else if (!a.empty() && a[0] != '-') {
            jsonPath = a;
        } else {
            printUsage(argv[0]);
            return 1;
        }
    }

    EngineSolver solver;
    solver.setMoveOrdering(order);
    if (mateIn > 0) solver.setMaxMate(mateIn);

    // --- Single position mode -------------------------------------------------
    if (!fen.empty()) {
        SolveResult r = solver.solve(fen, mateIn);
        std::cout << "FEN: " << fen << "\n";
        if (r.solved) {
            std::cout << "Forced mate in " << r.mateInMoves << "!\n"
                      << "Line: " << lineToSan(fen, r.line) << "\n";
        } else {
            std::cout << "No forced mate found within "
                      << (mateIn > 0 ? mateIn : solver.maxMate()) << " moves.\n";
        }
        std::cout << "Nodes: " << r.nodes << "   Time: " << r.seconds << "s\n";
        return 0;
    }

    // --- Puzzle-set mode ------------------------------------------------------
    std::vector<Puzzle> puzzles;
    if (!textPath.empty())
        puzzles = PuzzleLoader::loadText(textPath, mateIn);
    else if (!jsonPath.empty())
        puzzles = PuzzleLoader::loadJson(jsonPath, mateIn);
    else {
        printUsage(argv[0]);
        return 1;
    }

    if (puzzles.empty()) {
        std::cerr << "No puzzles loaded (check the path/format).\n";
        return 1;
    }
    if (limit > 0 && static_cast<int>(puzzles.size()) > limit)
        puzzles.resize(limit);

    int           solved = 0, firstMoveMatch = 0;
    std::uint64_t totalNodes = 0;
    double        totalTime  = 0.0;

    for (std::size_t i = 0; i < puzzles.size(); ++i) {
        const Puzzle& p = puzzles[i];
        SolveResult   r = solver.solve(p.fen, p.mateIn);
        totalNodes += r.nodes;
        totalTime  += r.seconds;

        bool matched = false;
        if (r.solved) {
            ++solved;
            std::string foundLine  = lineToSan(p.fen, r.line);
            std::string foundFirst = normSan(foundLine.substr(0, foundLine.find(' ')));
            // normSan above also keeps a possible leading move-number; strip it.
            std::string solverFirst = referenceFirstMove(foundLine);
            std::string refFirst     = referenceFirstMove(p.solution);
            matched = !refFirst.empty() && solverFirst == refFirst;
            if (matched) ++firstMoveMatch;

            if (!quiet) {
                std::cout << "[" << (i + 1) << "] M" << r.mateInMoves
                          << "  " << foundLine
                          << "   (ref: " << p.solution << ")"
                          << (matched ? "  [match]" : "  [alt]") << "\n";
            }
        } else if (!quiet) {
            std::cout << "[" << (i + 1) << "] UNSOLVED  " << p.fen
                      << "   (ref: " << p.solution << ")\n";
        }
    }

    std::cout << "\n=========== Summary ===========\n";
    std::cout << "Puzzles            : " << puzzles.size() << "\n";
    std::cout << "Solved (forced mate): " << solved << " / " << puzzles.size() << "\n";
    std::cout << "First move == ref   : " << firstMoveMatch << " / " << puzzles.size() << "\n";
    std::cout << "Total nodes        : " << totalNodes << "\n";
    std::cout << "Total time         : " << totalTime << "s\n";
    if (totalTime > 0)
        std::cout << "Speed              : "
                  << static_cast<std::uint64_t>(totalNodes / totalTime) << " nodes/s\n";
    return 0;
}
