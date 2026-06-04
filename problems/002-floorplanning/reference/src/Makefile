CC = g++
CFLAGS = -O3 -lm
TARGETS = main
OUTPUT = ../bin/hw3

.PHONY: all
# create the hw3 executable to ../bin
# remove the main in current directory
all: $(TARGETS)
	$(CC) $(CFLAGS) $(INCLUDE) -o $(OUTPUT) $(TARGETS).cpp
	rm -f $(TARGETS)

.PHONY: clean
# remove the hw3 executable in ../bin
clean:
	rm -f $(OUTPUT)