import sys, time, argparse
import numpy as np
from nnue_common import (NUM_FEATURES, EVAL_SCALE, fen_to_features, export_nnue)

def load_dataset(path):
    fens, wscore, wresult = [], [], []
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            a, b, c = line.split("|")
            fens.append(a.strip())
            wscore.append(float(b))
            wresult.append(float(c))
    n = len(fens)
    print(f"loaded {n} positions from {path}")

    Xown = np.zeros((n, NUM_FEATURES), dtype=np.float32)
    Xopp = np.zeros((n, NUM_FEATURES), dtype=np.float32)
    target = np.zeros(n, dtype=np.float32)

    LAMBDA = 0.7
    for i, fen in enumerate(fens):
        fw, fb, stm = fen_to_features(fen)
        own, opp = (fw, fb) if stm == 0 else (fb, fw)
        Xown[i, own] = 1.0
        Xopp[i, opp] = 1.0
        stm_score  = wscore[i]  if stm == 0 else -wscore[i]
        stm_result = wresult[i] if stm == 0 else 1.0 - wresult[i]
        eval_wp = 1.0 / (1.0 + np.exp(-stm_score / EVAL_SCALE))
        target[i] = LAMBDA * eval_wp + (1.0 - LAMBDA) * stm_result
    return Xown, Xopp, target

def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-x))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data")
    ap.add_argument("out")
    ap.add_argument("--hl", type=int, default=128)
    ap.add_argument("--epochs", type=int, default=30)
    ap.add_argument("--batch", type=int, default=512)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    Xown, Xopp, target = load_dataset(args.data)
    n, HL = Xown.shape[0], args.hl

    W1 = (rng.standard_normal((NUM_FEATURES, HL)) * 0.01).astype(np.float32)
    b1 = np.zeros(HL, dtype=np.float32)
    W2 = (rng.standard_normal(2 * HL) * 0.01).astype(np.float32)
    b2 = np.float32(0.0)

    params = {"W1": W1, "b1": b1, "W2": W2, "b2": np.array(b2)}
    m = {k: np.zeros_like(v) for k, v in params.items()}
    v = {k: np.zeros_like(v) for k, v in params.items()}
    beta1, beta2, eps = 0.9, 0.999, 1e-8
    t = 0

    print(f"training: n={n} HL={HL} epochs={args.epochs} batch={args.batch} lr={args.lr}")
    for epoch in range(args.epochs):
        perm = rng.permutation(n)
        ep_loss, nb = 0.0, 0
        t0 = time.time()
        for s in range(0, n, args.batch):
            idx = perm[s:s + args.batch]
            xo, xp, y = Xown[idx], Xopp[idx], target[idx]
            B = xo.shape[0]

            Ao = xo @ W1 + b1
            Ap = xp @ W1 + b1
            Ho = np.clip(Ao, 0.0, 1.0)
            Hp = np.clip(Ap, 0.0, 1.0)
            H = np.concatenate([Ho, Hp], axis=1)
            z = H @ W2 + b2
            p = sigmoid(z)
            loss = np.mean((p - y) ** 2)
            ep_loss += loss; nb += 1

            dp = (2.0 / B) * (p - y)
            dz = dp * p * (1.0 - p)
            gW2 = H.T @ dz
            gb2 = dz.sum()
            dH = np.outer(dz, W2)
            dHo, dHp = dH[:, :HL], dH[:, HL:]

            dAo = dHo * ((Ao > 0.0) & (Ao < 1.0))
            dAp = dHp * ((Ap > 0.0) & (Ap < 1.0))
            gW1 = xo.T @ dAo + xp.T @ dAp
            gb1 = dAo.sum(0) + dAp.sum(0)

            t += 1
            grads = {"W1": gW1, "b1": gb1, "W2": gW2, "b2": np.array(gb2)}
            for k in params:
                m[k] = beta1 * m[k] + (1 - beta1) * grads[k]
                v[k] = beta2 * v[k] + (1 - beta2) * grads[k] ** 2
                mhat = m[k] / (1 - beta1 ** t)
                vhat = v[k] / (1 - beta2 ** t)
                params[k] -= args.lr * mhat / (np.sqrt(vhat) + eps)
            W1, b1, W2 = params["W1"], params["b1"], params["W2"]
            b2 = params["b2"]

        print(f"  epoch {epoch+1:2d}/{args.epochs}  loss {ep_loss/nb:.5f}  "
              f"({time.time()-t0:.1f}s)")

    export_nnue(args.out, W1, b1, W2, float(b2))

    print("\nsanity check (float net, first 5 positions):")
    for i in range(5):
        Ho = np.clip(Xown[i] @ W1 + b1, 0, 1)
        Hp = np.clip(Xopp[i] @ W1 + b1, 0, 1)
        z = np.concatenate([Ho, Hp]) @ W2 + b2
        print(f"  pred {z*EVAL_SCALE:+7.1f} cp   target_wp {target[i]:.3f}")

if __name__ == "__main__":
    main()
