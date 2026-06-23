CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -g

SRC_DIR = src
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
TARGET = $(BIN_DIR)/main

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean