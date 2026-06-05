CXX      = g++
CXXFLAGS = -std=c++11 -O3
TARGET   = hw3

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all clean
