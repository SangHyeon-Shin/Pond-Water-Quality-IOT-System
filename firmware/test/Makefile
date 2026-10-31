# Host unit tests for firmware/libraries/PondCore (no Arduino needed).
#   make            build and run with sanitizers
#   make SANITIZE=  build without sanitizers
CXX ?= g++
SANITIZE ?= -fsanitize=address,undefined -fno-omit-frame-pointer
CXXFLAGS ?= -std=gnu++11 -Wall -Wextra -Werror -O1 -g
SRC_DIR := ../libraries/PondCore/src
SOURCES := $(wildcard $(SRC_DIR)/*.cpp) test_main.cpp
HEADERS := $(wildcard $(SRC_DIR)/*.h)
BUILD := build

all: run

$(BUILD)/pondcore_tests: $(SOURCES) $(HEADERS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(SANITIZE) -I$(SRC_DIR) $(SOURCES) -o $@ -lm

run: $(BUILD)/pondcore_tests
	./$(BUILD)/pondcore_tests

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
