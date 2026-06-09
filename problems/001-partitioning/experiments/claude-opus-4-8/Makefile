CXX      = g++
CXXFLAGS = -std=c++20 -O3 -fopenmp -pthread -static -static-libgcc -static-libstdc++
TARGET   = hw2
SRC      = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) $(TARGET).exe
