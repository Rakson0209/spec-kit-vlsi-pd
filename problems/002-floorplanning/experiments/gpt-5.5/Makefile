CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -fopenmp -pthread

hw3: main.cpp
	$(CXX) $(CXXFLAGS) -o hw3 main.cpp

.PHONY: clean
clean:
	rm -f hw3 hw3.exe
