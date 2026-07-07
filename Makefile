CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -g

SRC_DIR = src
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.cpp) $(SRC_DIR)/checksum/ext4checksum.cc
TARGET = $(BIN_DIR)/main
LDLIBS = -lcryptopp

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean