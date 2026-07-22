#pragma once

#include "chess.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace nnue {

static constexpr int   NUM_FEATURES = 768;
static constexpr int   QA           = 255;
static constexpr int   QB           = 64;
static constexpr int   EVAL_SCALE    = 400;
static constexpr uint32_t MAGIC      = 0x4C4E4E31;

inline int featureIndex(int persp, int pieceColor, int pt, int sq) {
    if (persp == 0) {
        int half = (pieceColor == 0) ? 0 : 1;
        return half * 384 + pt * 64 + sq;
    } else {
        int half = (pieceColor == 1) ? 0 : 1;
        return half * 384 + pt * 64 + (sq ^ 56);
    }
}

class Network {
public:
    bool loaded = false;
    int  HL     = 0;

    std::vector<int16_t> ftWeight;
    std::vector<int16_t> ftBias;
    std::vector<int16_t> outWeight;
    int32_t              outBias = 0;

    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;

        uint32_t magic = 0; int32_t hl = 0;
        f.read(reinterpret_cast<char*>(&magic), 4);
        f.read(reinterpret_cast<char*>(&hl),    4);
        if (!f || magic != MAGIC || hl <= 0 || hl > 4096) return false;

        HL = hl;
        ftWeight.resize((size_t)NUM_FEATURES * HL);
        ftBias.resize(HL);
        outWeight.resize((size_t)2 * HL);

        f.read(reinterpret_cast<char*>(ftWeight.data()), ftWeight.size() * sizeof(int16_t));
        f.read(reinterpret_cast<char*>(ftBias.data()),   ftBias.size()   * sizeof(int16_t));
        f.read(reinterpret_cast<char*>(outWeight.data()),outWeight.size()* sizeof(int16_t));
        f.read(reinterpret_cast<char*>(&outBias), sizeof(int32_t));
        if (!f) { loaded = false; return false; }

        loaded = true;
        return true;
    }

    int evaluate(const chess::Board& board) const {
        using namespace chess;
        const int stm = (board.sideToMove() == Color::WHITE) ? 0 : 1;

        std::vector<int32_t> accOwn(HL), accOpp(HL);
        for (int h = 0; h < HL; ++h) { accOwn[h] = ftBias[h]; accOpp[h] = ftBias[h]; }

        static const PieceType PTS[6] = {
            PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
            PieceType::ROOK, PieceType::QUEEN,  PieceType::KING
        };

        for (int c = 0; c < 2; ++c) {
            Color col = (c == 0) ? Color::WHITE : Color::BLACK;
            for (int pt = 0; pt < 6; ++pt) {
                Bitboard bb = board.pieces(PTS[pt], col);
                while (bb) {
                    int sq   = bb.pop();
                    int fOwn = featureIndex(stm,     c, pt, sq);
                    int fOpp = featureIndex(stm ^ 1, c, pt, sq);
                    const int16_t* wOwn = &ftWeight[(size_t)fOwn * HL];
                    const int16_t* wOpp = &ftWeight[(size_t)fOpp * HL];
                    for (int h = 0; h < HL; ++h) { accOwn[h] += wOwn[h]; accOpp[h] += wOpp[h]; }
                }
            }
        }

        int64_t out = outBias;
        for (int h = 0; h < HL; ++h) out += (int64_t)crelu(accOwn[h]) * outWeight[h];
        for (int h = 0; h < HL; ++h) out += (int64_t)crelu(accOpp[h]) * outWeight[HL + h];

        return (int)(out * EVAL_SCALE / ((int64_t)QA * QB));
    }

private:
    static inline int32_t crelu(int32_t x) { return x < 0 ? 0 : (x > QA ? QA : x); }
};

inline Network  g_net;
inline bool     g_use = false;

}
