# ---------------------------------------------------------------------------
#  Build for the chess mate-in-N puzzle solver.
#  Works with g++/MinGW (Windows) and g++/clang++ (Linux/macOS).
#
#    make            # build ./mate_solver
#    make run        # solve puzzles/mate_in_2.json
#    make clean
# ---------------------------------------------------------------------------

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -DNDEBUG -Wall -Wextra
TARGET    = mate_solver
SRC       = src/main.cpp
HEADERS   = src/mate_solver.hpp src/puzzle.hpp include/chess.hpp

# On Windows/MinGW the produced binary is mate_solver.exe automatically.
$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) puzzles/mate_in_2.json --mate 2

bench: $(TARGET)
	./$(TARGET) puzzles/mate_in_3.json --mate 3 --quiet

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: run bench clean
