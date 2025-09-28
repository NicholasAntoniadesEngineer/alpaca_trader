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
  src/core/trader/trader.cpp \
  src/core/trader/strategy_logic.cpp \
  src/core/trader/risk_logic.cpp \
  src/core/trader/market_processing.cpp \
  src/core/system/system_manager.cpp \
  src/core/threads/thread_register.cpp \
 \
  src/configs/component_configs.cpp \
  src/configs/config_loader.cpp \
  src/core/trader/account_manager.cpp \
  src/core/utils/time_utils.cpp \
  src/core/logging/account_logs.cpp \
  src/core/utils/http_utils.cpp \
  src/core/trader/indicators.cpp \
  src/core/logging/async_logger.cpp \
  src/core/logging/trading_logs.cpp \
  src/core/logging/thread_logs.cpp \
  src/core/logging/startup_logs.cpp \
  src/configs/thread_config.cpp \
  src/core/threads/thread_logic/platform/thread_control.cpp \
  src/core/threads/thread_logic/platform/linux/linux_thread_control.cpp \
  src/core/threads/thread_logic/platform/macos/macos_thread_control.cpp \
  src/core/threads/thread_logic/platform/windows/windows_thread_control.cpp \
  src/core/threads/thread_logic/thread_manager.cpp \
  src/core/threads/thread_logic/thread_registry.cpp \
  src/core/threads/system_threads/account_data_thread.cpp \
  src/core/threads/system_threads/market_data_thread.cpp \
  src/core/threads/system_threads/market_gate_thread.cpp \
  src/core/threads/system_threads/logging_thread.cpp \
  src/core/threads/system_threads/trader_thread.cpp

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
	mkdir -p $(OBJ_DIR)/src/core/system
	mkdir -p $(OBJ_DIR)/src/core/trader
	mkdir -p $(OBJ_DIR)/src/core/logging
	mkdir -p $(OBJ_DIR)/src/core/utils
	mkdir -p $(OBJ_DIR)/src/core/threads
	mkdir -p $(OBJ_DIR)/src/core/threads/thread_logic
	mkdir -p $(OBJ_DIR)/src/core/threads/thread_logic/platform
	mkdir -p $(OBJ_DIR)/src/core/threads/thread_logic/platform/linux
	mkdir -p $(OBJ_DIR)/src/core/threads/thread_logic/platform/macos
	mkdir -p $(OBJ_DIR)/src/core/threads/thread_logic/platform/windows
	mkdir -p $(OBJ_DIR)/src/core/threads/system_threads
	mkdir -p $(OBJ_DIR)/src/configs
	mkdir -p $(OBJ_DIR)/src/json

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