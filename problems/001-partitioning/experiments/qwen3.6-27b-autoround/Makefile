CXX = g++
CXXFLAGS = -std=c++20 -O3 -fopenmp -pthread
TARGET = hw2.exe

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp

clean:
	del /f $(TARGET) 2>nul || rm -f $(TARGET)

.PHONY: all clean
