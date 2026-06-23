# Liszt — a from-scratch UCI chess engine

A compact but complete chess engine written in C++17. It uses
[Disservin/chess-library](https://github.com/Disservin/chess-library) (v0.9.4)
purely for board representation and **legal** move generation; the search,
evaluation, time management, transposition table, and UCI layer are all
implemented in `engine.cpp`.

Because it speaks **UCI**, you don't read moves off the terminal — you load it
into a GUI like **CuteChess**, **Arena**, or **Banksia** and watch it play on a
real board, or pit it against other engines.

---

## What's implemented

| Component | Where | Notes |
|---|---|---|
| UCI protocol | `main()` / `setPosition` / `parseGo` | `uci`, `isready`, `ucinewgame`, `setoption`, `position`, `go`, `stop`, `quit` |
| Negamax + alpha-beta | `Engine::negamax` | the core search |
| **Timed iterative deepening** | `Engine::search` | searches depth 1,2,3… and returns the best move from the last *completed* depth the instant the clock runs out |
| **Zobrist hashing** | `board.hash()` | the library maintains the Zobrist key **incrementally** on every move — we use it directly as the TT key |
| **Transposition table** | `TranspositionTable` | depth-preferred replacement, exact/lower/upper bounds, mate-distance correct |
| **Quiescence search** | `Engine::quiescence` | resolves captures at the leaves to avoid the horizon effect |
| Move ordering | `Engine::scoreMoves` | TT move → MVV/LVA captures → killer moves → history heuristic |
| Null-move pruning | inside `negamax` | guarded against zugzwang (skipped in check / pawn-only endings) |
| Late move reductions | inside `negamax` | quiet late moves searched shallower first |
| Check extension | inside `negamax` | search one ply deeper when in check |
| Tapered evaluation | `evaluate` | PeSTO material + piece-square tables, blended middlegame↔endgame by remaining material |

A note on Zobrist hashing: the request mentioned implementing it to speed up
lookups. The chess-library already keeps a correct, incrementally-updated
Zobrist key for the position (`board.hash()`), so re-implementing it by hand
would only add a slower, bug-prone copy. The engine instead consumes that key
directly — which is exactly what it's for — and spends the effort on the
transposition table that the hash enables.

---

## Files

```
engine.cpp        the entire engine
chess.hpp         the chess-library single header (v0.9.4)
Makefile          build with `make`
CMakeLists.txt    build with CMake
README.md         this file
```

---

## Building

You need a C++17 compiler. `chess.hpp` must sit next to `engine.cpp`.

### Linux / macOS

```bash
make                       # produces ./liszt
# or directly:
g++ -std=c++17 -O2 -march=native -DNDEBUG -o liszt engine.cpp
```

### Windows (MinGW / MSYS2 — matches a typical VS Code + g++ setup)

```bat
g++ -std=c++17 -O2 -march=native -DNDEBUG -o liszt.exe engine.cpp
```

or, if you have `mingw32-make`:

```bat
mingw32-make
```

> **If compilation silently produces no `.exe` (exit code 1, no errors):**
> Windows Defender real-time protection sometimes quarantines a freshly built
> executable before the linker finishes writing it. Add your build folder to
> Defender's exclusions (Windows Security → Virus & threat protection →
> Manage settings → Exclusions), or build into a folder that's already
> excluded, then recompile.

### CMake (any platform)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

> `-march=native` tunes for your CPU. If you'll move the binary to a different
> machine, drop it (or use `-mtune=generic`).

---

## Using it in CuteChess

1. Build the engine (above) so you have `liszt` / `liszt.exe`.
2. Open CuteChess → **Tools → Settings → Engines → `+` (Add)**.
3. Set:
   - **Command:** browse to your `liszt` / `liszt.exe`.
   - **Working Directory:** the engine's folder (fine to leave default).
   - **Protocol:** **UCI**.
4. Click **OK**. CuteChess will ping it with `uci`/`isready` to confirm.
5. **Game → New** (`Ctrl+N`), pick Liszt as one or both players, choose a time
   control (e.g. 1 minute + 1 second increment), and start. You'll see it move
   on the board.

To make it play itself or face another engine, just select engines for both
White and Black.

---

## Quick manual test (without a GUI)

You can drive it by hand to confirm it works. Type these after launching it:

```
uci
position startpos
go movetime 2000
```

Expected: a stream of `info depth … score cp … pv …` lines, then a single
`bestmove …`. Some things to try:

```
position fen 6k1/5ppp/8/8/8/8/8/R6K w - - 0 1
go depth 5
```
→ finds `score mate 1` / `bestmove a1a8` (Ra8#).

```
position startpos moves e2e4 e7e5 g1f3 b8c6
go wtime 60000 btime 60000 winc 1000 binc 1000
```
→ thinks within its share of the 60s clock and replies with a developing move.

---

## Time management, in one paragraph

For `go movetime X` it thinks `X` ms. For a real clock
(`go wtime … btime … winc … binc … [movestogo …]`) it allocates roughly
`remaining / movestogo` (or `remaining / 30` if `movestogo` isn't given) plus
most of the increment, never spending more than what's on the clock. It keeps a
**hard limit** (abort mid-search the moment it's exceeded) and a **soft limit**
(don't *start* a new, more expensive depth past ~60% of the budget). When the
hard limit trips during depth *N+1*, the engine discards that unfinished depth
and plays the best move from the fully completed depth *N* — so it always plays
the deepest result it could finish in time.

---

## Options

| UCI option | Default | Meaning |
|---|---|---|
| `Hash` | 64 | transposition table size in MB (`setoption name Hash value 128`) |

---

## Tuning / extending

Good next steps if you want to push strength:
- A proper aspiration-window loop around the iterative deepening.
- Static exchange evaluation (SEE) to prune losing captures in quiescence.
- Better evaluation terms: passed pawns, king safety, mobility, rook on open file.
- Pondering and a separate search thread (so `stop` can interrupt `go infinite`).
