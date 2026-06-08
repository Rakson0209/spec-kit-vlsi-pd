CXX = g++
CXXFLAGS = -std=c++20 -O3 -fopenmp -pthread
OUT = hw3

all: $(OUT)

$(OUT): main.cpp
	$(CXX) $(CXXFLAGS) -o $(OUT) main.cpp

clean:
	rm -f $(OUT) $(OUT).exe
