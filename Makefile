# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -O2
LDFLAGS := 

# Directories
SRC_DIR := .
OBJ_DIR := obj
BIN_DIR := bin

# Find all source files
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)

# Generate object file names
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# Generate binary names (one per source file)
BINARIES := $(patsubst $(SRC_DIR)/%.cpp,$(BIN_DIR)/%,$(SOURCES))

# Default target
.PHONY: all
all: $(BINARIES)
	@echo "✓ Build complete! Binaries in $(BIN_DIR)/"

# Create directories if they don't exist
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@
	@echo "Created directory: $@"

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "Compiling: $< -> $@"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Link object files to binaries
$(BIN_DIR)/%: $(OBJ_DIR)/%.o | $(BIN_DIR)
	@echo "Linking: $< -> $@"
	@$(CXX) $(LDFLAGS) $< -o $@

# Run a specific binary
.PHONY: run
run:
	@if [ -z "$(NAME)" ]; then \
		echo "Usage: make run NAME=<binary_name>"; \
		echo "Available binaries:"; \
		for bin in $(BIN_DIR)/*; do \
			[ -f "$$bin" ] && echo "  - $$(basename $$bin)"; \
		done; \
		exit 1; \
	fi
	@if [ ! -f "$(BIN_DIR)/$(NAME)" ]; then \
		echo "Error: Binary '$(NAME)' not found in $(BIN_DIR)/"; \
		echo "Run 'make' first to build binaries."; \
		exit 1; \
	fi
	@echo "Running: $(BIN_DIR)/$(NAME) $(ARGS)"
	@$(BIN_DIR)/$(NAME) $(ARGS)

# Run server
.PHONY: run-server
run-server: $(BIN_DIR)/server
	@echo "Starting server on port 2203..."
	@$(BIN_DIR)/server

# Run client with arguments
.PHONY: run-client
run-client: $(BIN_DIR)/client
	@if [ -z "$(ARGS)" ]; then \
		echo "Usage: make run-client ARGS='<command>'"; \
		echo "Examples:"; \
		echo "  make run-client ARGS='set key value'"; \
		echo "  make run-client ARGS='get key'"; \
		echo "  make run-client ARGS='del key'"; \
		exit 1; \
	fi
	@echo "Running: $(BIN_DIR)/client $(ARGS)"
	@$(BIN_DIR)/client $(ARGS)


# Clean generated files
.PHONY: clean
clean:
	@echo "Cleaning generated files..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "✓ Clean complete"

# Clean and rebuild
.PHONY: rebuild
rebuild: clean all



# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo ""
	@echo "  make                  - Build all binaries (default)"
	@echo "  make all              - Build all binaries"
	@echo "  make clean            - Remove all generated files"
	@echo "  make rebuild          - Clean and rebuild everything"
	@echo ""
	@echo "Running programs:"
	@echo "  make run NAME=<name> [ARGS='...']  - Run specific binary"
	@echo "  make run-server                     - Run the server"
	@echo "  make run-client ARGS='<cmd>'        - Run client with command"
	@echo ""
	@echo ""
	@echo "Information:"
	@echo "  make help             - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make run-server"
	@echo "  make run-client ARGS='set mykey myvalue'"
	@echo "  make run-client ARGS='get mykey'"
	@echo "  make run NAME=server"
	@echo "  make run NAME=client ARGS='del mykey'"

# Debug target - show makefile variables
.PHONY: debug
debug:
	@echo "Makefile Debug Information:"
	@echo "CXX        = $(CXX)"
	@echo "CXXFLAGS   = $(CXXFLAGS)"
	@echo "LDFLAGS    = $(LDFLAGS)"
	@echo "SRC_DIR    = $(SRC_DIR)"
	@echo "OBJ_DIR    = $(OBJ_DIR)"
	@echo "BIN_DIR    = $(BIN_DIR)"
	@echo "SOURCES    = $(SOURCES)"
	@echo "OBJECTS    = $(OBJECTS)"
	@echo "BINARIES   = $(BINARIES)"

.PHONY: all clean rebuild run run-server run-client test list tree help debug