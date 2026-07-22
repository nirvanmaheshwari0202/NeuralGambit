import sys, time, argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

from nnue_common import NUM_FEATURES, EVAL_SCALE, fen_to_features, export_nnue

LAMBDA = 0.7

class PosDataset(Dataset):
    def __init__(self, path):
        self.own, self.opp, self.tgt = [], [], []
        with open(path) as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                a, b, c = line.split("|")
                fw, fb, stm = fen_to_features(a.strip())
                wscore, wresult = float(b), float(c)
                own, opp = (fw, fb) if stm == 0 else (fb, fw)
                stm_score  = wscore  if stm == 0 else -wscore
                stm_result = wresult if stm == 0 else 1.0 - wresult
                eval_wp = 1.0 / (1.0 + np.exp(-stm_score / EVAL_SCALE))
                self.own.append(torch.tensor(own, dtype=torch.long))
                self.opp.append(torch.tensor(opp, dtype=torch.long))
                self.tgt.append(LAMBDA * eval_wp + (1.0 - LAMBDA) * stm_result)
        print(f"loaded {len(self.tgt)} positions from {path}")

    def __len__(self):
        return len(self.tgt)

    def __getitem__(self, i):
        return self.own[i], self.opp[i], self.tgt[i]

def collate(batch):
    own, opp, tgt = zip(*batch)
    def pack(lists):
        offsets, flat, running = [], [], 0
        for t in lists:
            offsets.append(running)
            flat.append(t)
            running += t.numel()
        return torch.cat(flat), torch.tensor(offsets, dtype=torch.long)
    own_flat, own_off = pack(own)
    opp_flat, opp_off = pack(opp)
    return own_flat, own_off, opp_flat, opp_off, torch.tensor(tgt, dtype=torch.float32)

class NNUE(nn.Module):
    def __init__(self, HL):
        super().__init__()
        self.HL = HL
        self.ft = nn.EmbeddingBag(NUM_FEATURES, HL, mode="sum")
        self.ft_bias = nn.Parameter(torch.zeros(HL))
        self.out = nn.Linear(2 * HL, 1)
        nn.init.normal_(self.ft.weight, std=0.01)
        nn.init.normal_(self.out.weight, std=0.01)
        nn.init.zeros_(self.out.bias)

    def forward(self, own_flat, own_off, opp_flat, opp_off):
        acc_own = self.ft(own_flat, own_off) + self.ft_bias
        acc_opp = self.ft(opp_flat, opp_off) + self.ft_bias
        h = torch.clamp(torch.cat([acc_own, acc_opp], dim=1), 0.0, 1.0)
        return self.out(h).squeeze(1)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data")
    ap.add_argument("out")
    ap.add_argument("--hl", type=int, default=256)
    ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--batch", type=int, default=16384)
    ap.add_argument("--lr", type=float, default=1e-3)
    args = ap.parse_args()

    dev = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"device: {dev}")

    ds = PosDataset(args.data)
    dl = DataLoader(ds, batch_size=args.batch, shuffle=True,
                    collate_fn=collate, num_workers=2, drop_last=False)

    net = NNUE(args.hl).to(dev)
    opt = torch.optim.Adam(net.parameters(), lr=args.lr)
    lossfn = nn.MSELoss()

    for epoch in range(args.epochs):
        net.train()
        tot, nb, t0 = 0.0, 0, time.time()
        for own_flat, own_off, opp_flat, opp_off, tgt in dl:
            own_flat, own_off = own_flat.to(dev), own_off.to(dev)
            opp_flat, opp_off = opp_flat.to(dev), opp_off.to(dev)
            tgt = tgt.to(dev)
            pred = torch.sigmoid(net(own_flat, own_off, opp_flat, opp_off))
            loss = lossfn(pred, tgt)
            opt.zero_grad(); loss.backward(); opt.step()

            with torch.no_grad():
                net.ft.weight.clamp_(-1.98, 1.98)
            tot += loss.item(); nb += 1
        print(f"  epoch {epoch+1:2d}/{args.epochs}  loss {tot/nb:.5f}  ({time.time()-t0:.1f}s)")

    W1 = net.ft.weight.detach().cpu().numpy()
    b1 = net.ft_bias.detach().cpu().numpy()
    W2 = net.out.weight.detach().cpu().numpy()[0]
    b2 = float(net.out.bias.detach().cpu().numpy()[0])
    export_nnue(args.out, W1, b1, W2, b2)

if __name__ == "__main__":
    main()
