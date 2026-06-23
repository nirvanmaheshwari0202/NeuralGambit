# ---------------------------------------------------------------------------
# Makefile for the Liszt chess engine
#
#   make            -> optimized native build  (./liszt  or  liszt.exe)
#   make debug      -> build with assertions + sanitizers
#   make clean      -> remove build artifacts
#
# Works with g++ / clang++ on Linux, macOS, and MinGW (Windows).
# ---------------------------------------------------------------------------

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -march=native -DNDEBUG -Wall -Wextra
TARGET   := liszt
SRC      := engine.cpp

# On Windows (MinGW) produce liszt.exe automatically.
ifeq ($(OS),Windows_NT)
    TARGET := liszt.exe
endif

all: $(TARGET)

$(TARGET): $(SRC) chess.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# Slower build with checks turned on - use this if something looks wrong.
debug: $(SRC) chess.hpp
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined -o $(TARGET) $(SRC)

clean:
	rm -f liszt liszt.exe

.PHONY: all debug clean
