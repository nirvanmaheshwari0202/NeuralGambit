#ifndef PUZZLE_HPP
#define PUZZLE_HPP

// ===========================================================================
//  Puzzle data model + loaders for the two formats shipped in NeuralGambit:
//    1. JSON:  { "<FEN>": "<solution in SAN>", ... }   (mate_in_N.json)
//    2. Text:  citation / FEN / "1. .. .. #" blocks      (m8nN.txt)
//
//  A tiny dependency-free JSON reader is used (the files are flat string->string
//  objects whose values never contain escaped quotes -- verified), so the whole
//  project builds with nothing but a C++17 compiler and chess.hpp.
// ===========================================================================

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../include/chess.hpp"

namespace mate {

// A single puzzle: the position to solve plus the reference answer.
struct Puzzle {
    std::string fen;            // position (side to move = the attacker)
    std::string solution;       // reference solution text from the dataset
    std::string source;         // citation / origin (text format only)
    int         mateIn = 0;     // expected mate distance, if known from the file
};

// ---------------------------------------------------------------------------
//  PuzzleLoader: static helpers that turn files into std::vector<Puzzle>.
// ---------------------------------------------------------------------------
class PuzzleLoader {
   public:
    // Load a NeuralGambit-style JSON file of {FEN: solution}. `mateIn` is the
    // known mate distance for the file (e.g. 2 for mate_in_2.json), or <=0 if
    // unknown.
    static std::vector<Puzzle> loadJson(const std::string& path, int mateIn = -1) {
        std::vector<Puzzle> puzzles;
        std::ifstream in(path);
        if (!in) return puzzles;

        std::stringstream ss;
        ss << in.rdbuf();
        const std::string text = ss.str();

        // Extract every JSON string literal in order; (2k, 2k+1) are key/value.
        std::vector<std::string> tokens = extractStrings(text);
        for (std::size_t i = 0; i + 1 < tokens.size(); i += 2) {
            Puzzle p;
            p.fen      = tokens[i];
            p.solution = tokens[i + 1];
            p.mateIn   = mateIn;
            puzzles.push_back(std::move(p));
        }
        return puzzles;
    }

    // Load a Bill-Harvey-style text file (citation line / FEN line / solution
    // line, separated by blanks, with a free-text preamble that is skipped).
    static std::vector<Puzzle> loadText(const std::string& path, int mateIn = -1) {
        std::vector<Puzzle> puzzles;
        std::ifstream in(path);
        if (!in) return puzzles;

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line)) {
            // Strip a trailing '\r' from CRLF files.
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }

        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (!looksLikeFen(lines[i])) continue;
            Puzzle p;
            p.fen    = lines[i];
            p.mateIn = mateIn;
            // The previous non-empty line is the citation/source.
            for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
                if (!lines[j].empty()) { p.source = lines[j]; break; }
            }
            // The next non-empty line is the solution.
            for (std::size_t j = i + 1; j < lines.size(); ++j) {
                if (!lines[j].empty()) { p.solution = trim(lines[j]); break; }
            }
            puzzles.push_back(std::move(p));
        }
        return puzzles;
    }

   private:
    // Pull out all double-quoted strings, honouring \" and \\ escapes.
    static std::vector<std::string> extractStrings(const std::string& text) {
        std::vector<std::string> out;
        bool        inStr = false;
        std::string cur;
        for (std::size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (inStr) {
                if (c == '\\' && i + 1 < text.size()) {
                    char n = text[++i];
                    cur.push_back(n);  // keep escaped char verbatim
                } else if (c == '"') {
                    out.push_back(cur);
                    cur.clear();
                    inStr = false;
                } else {
                    cur.push_back(c);
                }
            } else if (c == '"') {
                inStr = true;
            }
        }
        return out;
    }

    // Heuristic FEN detector: first field has 7 '/', second is "w" or "b".
    static bool looksLikeFen(const std::string& s) {
        std::istringstream iss(trim(s));
        std::string board, stm;
        if (!(iss >> board >> stm)) return false;
        if (stm != "w" && stm != "b") return false;
        int slashes = 0;
        for (char c : board)
            if (c == '/') ++slashes;
        return slashes == 7;
    }

    static std::string trim(const std::string& s) {
        std::size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        std::size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }
};

}  // namespace mate

#endif  // PUZZLE_HPP
