# Using modern C++ best practices with snake_case naming

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I./include
LIBS = -lcurl -pthread

# Directories
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

# Target binary
TARGET = $(BIN_DIR)/alpaca_trader

# Source files
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/api/alpaca_client.cpp \
          $(SRC_DIR)/core/trader.cpp \
          $(SRC_DIR)/core/strategy_logic.cpp \
          $(SRC_DIR)/core/risk_logic.cpp \
          $(SRC_DIR)/core/market_processing.cpp \
          $(SRC_DIR)/core/trader_logging.cpp \
          $(SRC_DIR)/utils/config_loader.cpp \
          $(SRC_DIR)/data/account_manager.cpp \
          $(SRC_DIR)/ui/account_display.cpp \
          $(SRC_DIR)/utils/async_logger.cpp \
          $(SRC_DIR)/utils/http_utils.cpp \
          $(SRC_DIR)/utils/indicators.cpp \
          $(SRC_DIR)/utils/thread_utils.cpp \
          $(SRC_DIR)/utils/thread_manager.cpp \
          $(SRC_DIR)/utils/thread_logging.cpp \
          $(SRC_DIR)/workers/account_data_worker.cpp \
          $(SRC_DIR)/workers/market_data_worker.cpp

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Default target
all: build-and-clean

# Create directories if they don't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/api
	mkdir -p $(OBJ_DIR)/core
	mkdir -p $(OBJ_DIR)/data
	mkdir -p $(OBJ_DIR)/ui
	mkdir -p $(OBJ_DIR)/utils
	mkdir -p $(OBJ_DIR)/workers

# Link the target
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LIBS)
	@echo "Build successful: $(TARGET)"

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)/alpaca_trader
	@echo "Clean complete"

# Clean object files only (keep binary)
clean-obj:
	rm -rf $(OBJ_DIR)
	@echo "Object files cleaned"

# Clean everything including directories
distclean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Distribution clean complete"

# Build and then clean object files (sequential to avoid race with -j)
build-and-clean: $(TARGET)
	$(MAKE) clean-obj
	@echo "Build completed and object files cleaned"

# Rebuild everything
rebuild: clean all

# Run the program
run: $(TARGET)
	./$(TARGET)

# Show help
help:
	@echo "Available targets:"
	@echo "  all            - Build the program (default)"
	@echo "  clean          - Remove object files and binary"
	@echo "  clean-obj      - Remove object files only (keep binary)"
	@echo "  build-and-clean- Build and then clean object files"
	@echo "  distclean      - Remove all build directories"
	@echo "  rebuild        - Clean and build"
	@echo "  run            - Build and run the program"
	@echo "  help           - Show this help message"

# Declare phony targets
.PHONY: all clean clean-obj build-and-clean distclean rebuild run help

# Include dependency files if they exist
-include $(OBJECTS:.o=.d)

# Generate dependency files
$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@
