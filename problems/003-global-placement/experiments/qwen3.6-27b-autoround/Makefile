# Global Placement — build (constitution R2). No Boost, no reference/obj link.
# The exe must be static-linked so the scorer subprocess can launch it without
# mingw runtime DLLs on PATH, and placed under D:\FSecret to run (Defender ASR).
CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O3 -fopenmp -pthread -static
OUT      ?= D:/FSecret/hw4.exe

hw4: main.cpp
	$(CXX) $(CXXFLAGS) -o $(OUT) main.cpp

.PHONY: hw4
