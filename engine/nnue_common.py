import struct
import numpy as np

NUM_FEATURES = 768
QA          = 255
QB          = 64
EVAL_SCALE  = 400
MAGIC       = 0x4C4E4E31

_PT = {'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4, 'k': 5}

def feature_index(persp: int, color: int, pt: int, sq: int) -> int:
    if persp == 0:
        half = 0 if color == 0 else 1
        return half * 384 + pt * 64 + sq
    else:
        half = 0 if color == 1 else 1
        return half * 384 + pt * 64 + (sq ^ 56)

def fen_to_features(fen: str):
    parts = fen.split()
    placement, side = parts[0], parts[1]
    fw, fb = [], []
    r_fen = 0
    f = 0
    for ch in placement:
        if ch == '/':
            r_fen += 1
            f = 0
        elif ch.isdigit():
            f += int(ch)
        else:
            color = 0 if ch.isupper() else 1
            pt = _PT[ch.lower()]
            sq = (7 - r_fen) * 8 + f
            fw.append(feature_index(0, color, pt, sq))
            fb.append(feature_index(1, color, pt, sq))
            f += 1
    stm = 0 if side == 'w' else 1
    return fw, fb, stm

def export_nnue(path, W1, b1, W2, b2):
    HL = b1.shape[0]

    q_ftw = np.round(W1 * QA).astype(np.int32)
    q_ftb = np.round(b1 * QA).astype(np.int32)
    q_outw = np.round(W2 * QB).astype(np.int32)
    q_outb = int(round(float(b2) * QA * QB))

    for name, arr in [("ft_weight", q_ftw), ("ft_bias", q_ftb), ("out_weight", q_outw)]:
        if np.abs(arr).max() > 32767:
            print(f"  WARNING: {name} saturates int16 (max |{np.abs(arr).max()}|); clamping")
    q_ftw = np.clip(q_ftw, -32768, 32767).astype(np.int16)
    q_ftb = np.clip(q_ftb, -32768, 32767).astype(np.int16)
    q_outw = np.clip(q_outw, -32768, 32767).astype(np.int16)

    with open(path, "wb") as fh:
        fh.write(struct.pack("<I", MAGIC))
        fh.write(struct.pack("<i", HL))

        fh.write(q_ftw.tobytes(order="C"))
        fh.write(q_ftb.tobytes(order="C"))
        fh.write(q_outw.tobytes(order="C"))
        fh.write(struct.pack("<i", q_outb))
    print(f"  wrote {path}  (HL={HL}, "
          f"{q_ftw.nbytes + q_ftb.nbytes + q_outw.nbytes + 4} bytes)")
