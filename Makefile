# Modern C++ Trading System Build Configuration
# Consolidated project structure - no more include/src duplication!

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I. -Isrc
LIBS = -lcurl -pthread

# Directories
OBJ_DIR = obj
BIN_DIR = bin
GIT_HASH = $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
TARGET = $(BIN_DIR)/alpaca_trader_$(GIT_HASH)

# Source files (now in consolidated structure)
SOURCES = src/main.cpp \
  src/api/clock/market_clock.cpp \
  src/api/market/market_data_client.cpp \
  src/api/alpaca_client.cpp \
  src/api/orders/order_client.cpp \
  src/core/trader.cpp \
  src/core/strategy_logic.cpp \
  src/core/risk_logic.cpp \
  src/core/market_processing.cpp \
  src/core/system_manager.cpp \
  src/configs/component_configs.cpp \
  src/configs/config_loader.cpp \
  src/core/account_manager.cpp \
  src/utils/time_utils.cpp \
  src/logging/account_logger.cpp \
  src/utils/http_utils.cpp \
  src/core/indicators.cpp \
  src/logging/async_logger.cpp \
  src/logging/trading_logger.cpp \
  src/logging/thread_logger.cpp \
  src/logging/startup_logger.cpp \
  src/configs/thread_config.cpp \
  src/threads/platform/thread_control.cpp \
  src/threads/platform/linux/linux_thread_control.cpp \
  src/threads/platform/macos/macos_thread_control.cpp \
  src/threads/platform/windows/windows_thread_control.cpp \
  src/threads/thread_manager.cpp \
  src/threads/account_data_thread.cpp \
  src/threads/market_data_thread.cpp \
  src/threads/market_gate_thread.cpp \
  src/threads/logging_thread.cpp \
  src/threads/trader_thread.cpp

# Object files
OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)

# Default target
all: build-and-clean

# Create directories if they don't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/src
	mkdir -p $(OBJ_DIR)/src/api
	mkdir -p $(OBJ_DIR)/src/api/base
	mkdir -p $(OBJ_DIR)/src/api/clock
	mkdir -p $(OBJ_DIR)/src/api/market
	mkdir -p $(OBJ_DIR)/src/api/orders
	mkdir -p $(OBJ_DIR)/src/core
	mkdir -p $(OBJ_DIR)/src/configs
	mkdir -p $(OBJ_DIR)/src/json
	mkdir -p $(OBJ_DIR)/src/utils
	mkdir -p $(OBJ_DIR)/src/logging
	mkdir -p $(OBJ_DIR)/src/threads
	mkdir -p $(OBJ_DIR)/src/threads/platform
	mkdir -p $(OBJ_DIR)/src/threads/platform/linux
	mkdir -p $(OBJ_DIR)/src/threads/platform/macos
	mkdir -p $(OBJ_DIR)/src/threads/platform/windows

# Link the target
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LIBS)
	@ln -sf $(notdir $(TARGET)) $(BIN_DIR)/alpaca_trader_latest
	@echo "Build successful: $(TARGET)"
	@echo "Latest symlink created: $(BIN_DIR)/alpaca_trader_latest"

# Compile source files to object files
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)/alpaca_trader_* $(BIN_DIR)/alpaca_trader_latest
	@echo "Clean complete"

# Build and then clean object files
build-and-clean: $(TARGET) clean-obj

# Clean only object files (keep binary)
clean-obj:
	rm -rf $(OBJ_DIR)
	@echo "Object files cleaned"

# Clean everything including binary
clean-all:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "All build artifacts cleaned"

# Force rebuild
rebuild: clean all

# Help target
help:
	@echo "Available targets:"
	@echo "  all (default)  - Build the project and clean object files"
	@echo "  clean          - Remove binary and object files"
	@echo "  clean-obj      - Remove only object files"
	@echo "  clean-all      - Remove all build artifacts"
	@echo "  rebuild        - Clean and build from scratch"
	@echo "  help           - Show this help message"
	@echo ""
	@echo "Versioning:"
	@echo "  Executable will be named: alpaca_trader_$(GIT_HASH)"
	@echo "  Symlink 'alpaca_trader_latest' points to current build"

# Prevent make from treating file names as targets
.PHONY: all clean clean-obj clean-all rebuild help build-and-clean