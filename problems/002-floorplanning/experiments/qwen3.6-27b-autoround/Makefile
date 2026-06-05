# Makefile for Fixed-outline Floorplanning

CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall
SRCDIR = src
BINDIR = bin
TARGET = $(BINDIR)/hw3

SRCS = $(SRCDIR)/main.cpp \
       $(SRCDIR)/parser.cpp \
       $(SRCDIR)/module.cpp \
       $(SRCDIR)/evaluator.cpp \
       $(SRCDIR)/placer.cpp \
       $(SRCDIR)/sa.cpp

OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
