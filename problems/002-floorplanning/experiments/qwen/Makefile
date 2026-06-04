CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra
SRCDIR = src
BINDIR = ../bin
TARGET = $(BINDIR)/hw3

SRCS = $(SRCDIR)/main.cpp \
       $(SRCDIR)/parser.cpp \
       $(SRCDIR)/placement.cpp \
       $(SRCDIR)/optimizer.cpp \
       $(SRCDIR)/hpwl.cpp \
       $(SRCDIR)/output.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS) $(wildcard $(SRCDIR)/*.h)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
