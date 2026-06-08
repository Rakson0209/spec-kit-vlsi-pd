CXX = g++
CXXFLAGS = -std=c++20 -O3 -fopenmp -pthread
TARGET = hw3

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp

clean:
	rm -f $(TARGET)

.PHONY: all clean
