CXX      := g++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra -I../include
EXT_OBJS := $(wildcard ../obj/*.o)
EXEC     := ../bin/hw4
SRCS     := ExampleFunction.cpp GlobalPlacer.cpp main.cpp
OBJS     := $(SRCS:.cpp=.o)
LDFLAGS  := -no-pie

all: $(EXEC)

$(EXEC): $(OBJS) $(EXT_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LDFLAGS)

ifeq (test, $(firstword $(MAKECMDGOALS)))
  TESTCASE := $(word 2, $(MAKECMDGOALS))
  $(eval $(TESTCASE):;@:)
endif

test: $(EXEC)
	@echo test on ../testcase/$(TESTCASE)/$(TESTCASE).aux
	./$(EXEC) ../testcase/$(TESTCASE)/$(TESTCASE).aux ../output/$(TESTCASE).gp.pl
	./../verifier/verify ../testcase/$(TESTCASE)/$(TESTCASE).aux ../output/$(TESTCASE).gp.pl

EXAMPLE := ../bin/example

example: $(EXAMPLE)

$(EXAMPLE): Example.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(EXT_OBJS) $(LDFLAGS)

clean:
	rm -f $(EXEC) $(OBJS) $(EXAMPLE)

.PHONY: all test example clean
