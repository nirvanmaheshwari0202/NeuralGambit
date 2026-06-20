# Chess Mate-in-N Puzzle Solver (C++)

An object-oriented chess bot that **solves forced-mate puzzles** ("mate in N").
Built for NeuralGambit Week 3, using
[Disservin's chess-library](https://github.com/Disservin/chess-library/) for the
board representation and legal move generation, and the puzzle datasets from
[NeuralGambit/Week3](https://github.com/nirvanmaheshwari0202/NeuralGambit/tree/main/Week3).

## What it does

Given a position where one side has a forced mate, the engine finds it and prints
the full forcing line, e.g.

```
$ ./mate_solver --fen "4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 0" --mate 2
Forced mate in 2!
Line: 1. Qb8+ Nxb8 2. Rd8#
```

It solves **100%** of the bundled datasets:

| dataset            | puzzles | solved   | first move == reference |
|--------------------|--------:|---------:|------------------------:|
| `mate_in_2.json`   |     351 | 351/351  | 334/351                 |
| `mate_in_3.json`   |     489 | 489/489  | 487/489                 |
| `mate_in_4.json`   |     462 | 462/462  | 461/462                 |

("first move == reference" is below 100% only because some puzzles admit more
than one valid forced mate; every line the engine outputs is a *proven* forced
mate, because a move is accepted only when **every** defence is refuted.)

## How it works — the algorithm

Mate solving is an **AND/OR search**:

* **Attacker's turn (OR node):** find *at least one* move that forces mate.
* **Defender's turn (AND node):** *every* reply must still lose.

A "mate in N" is a forcing line of `2N − 1` half-moves (the attacker moves `N`
times, the defender `N − 1` times). The solver uses **iterative deepening**
(try mate-in-1, then 2, …) so it always reports the *shortest* mate.

The search is **complete** — it considers every legal move, so it also finds
quiet *zugzwang* mates like
`5K1k/6pp/7R/8/8/8/8/6R1 w - - 0 1 → Rgg6 gxh6 Rg8#`, where the key move is a
non-checking waiting move. Checks/captures are merely *ordered first* as a speed
heuristic; this never changes the result.

## OOP design

| Class / type            | Responsibility                                                        |
|-------------------------|-----------------------------------------------------------------------|
| `mate::EngineSolver`    | The engine. Holds search config + stats; runs the AND/OR mate search. |
| `mate::SolveResult`     | Value object: solved?, mate distance, the line, node count, time.     |
| `mate::Puzzle`          | One puzzle: FEN, reference solution, source, expected mate-in-N.       |
| `mate::PuzzleLoader`    | Loads the JSON and text datasets into `Puzzle` objects.               |

```cpp
mate::EngineSolver solver;
mate::SolveResult  r = solver.solve("4r1rk/5K1b/7R/R7/8/8/8/8 w - - 0 1", 2);
if (r.solved) /* r.mateInMoves, r.line, r.nodes ... */;
```

## Build

Requires only a C++17 compiler and the bundled `include/chess.hpp` — no other
dependencies.

```bash
make                      # builds ./mate_solver
# or directly:
g++ -std=c++17 -O2 -DNDEBUG src/main.cpp -o mate_solver
```

On **Windows / MinGW** the same command works and produces `mate_solver.exe`:

```bat
g++ -std=c++17 -O2 -DNDEBUG src\main.cpp -o mate_solver.exe
```

## Usage

```bash
# A whole dataset (knowing the depth makes it faster):
./mate_solver puzzles/mate_in_2.json --mate 2

# Just the summary:
./mate_solver puzzles/mate_in_3.json --mate 3 --quiet

# The Bill-Harvey text format:
./mate_solver --text puzzles/m8n2.txt --mate 2

# A single position:
./mate_solver --fen "<FEN>" --mate 2
```

Flags: `--mate N` (target/limit depth), `--limit K` (first K puzzles only),
`--no-order` (disable move ordering, for benchmarking), `--quiet` (summary only).

## Layout

```
chess-mate-solver/
├── include/chess.hpp        # Disservin chess-library (single header)
├── src/
│   ├── mate_solver.hpp      # EngineSolver + SolveResult  (the engine)
│   ├── puzzle.hpp           # Puzzle + PuzzleLoader        (datasets)
│   └── main.cpp             # CLI driver, SAN rendering, reporting
├── puzzles/                 # datasets from NeuralGambit/Week3
├── Makefile
└── README.md
```
