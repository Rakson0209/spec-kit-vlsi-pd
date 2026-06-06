CXX      := g++
CXXFLAGS := -std=c++20 -O3 -Wall -I../reference/include
EXT_OBJS := $(wildcard ../reference/obj/*.o)
LDFLAGS  := -no-pie

hpwl_eval: hpwl_eval.cpp $(EXT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f hpwl_eval

.PHONY: clean
