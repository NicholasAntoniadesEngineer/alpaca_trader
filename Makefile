# Modern C++ Trading System Build Configuration
# Consolidated project structure - no more include/src duplication!

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I.
LIBS = -lcurl -pthread

# Directories
OBJ_DIR = obj
BIN_DIR = bin
GIT_HASH = $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
TARGET = $(BIN_DIR)/alpaca_trader_$(GIT_HASH)

# Source files (now in consolidated structure)
SOURCES = main.cpp \
          api/clock/market_clock.cpp \
          api/market/market_data_client.cpp \
          api/orders/order_client.cpp \
          core/trader.cpp \
          core/strategy_logic.cpp \
          core/risk_logic.cpp \
          core/market_processing.cpp \
          utils/config_loader.cpp \
          data/account_manager.cpp \
          ui/account_display.cpp \
          utils/http_utils.cpp \
          core/indicators.cpp \
          logging/async_logger.cpp \
          logging/trading_logger.cpp \
          logging/thread_logger.cpp \
          logging/startup_logger.cpp \
          threads/config/thread_config.cpp \
          threads/platform/thread_control.cpp \
          threads/platform/linux/linux_thread_control.cpp \
          threads/platform/macos/macos_thread_control.cpp \
          threads/platform/windows/windows_thread_control.cpp \
          threads/thread_manager.cpp \
          threads/account_data_thread.cpp \
          threads/market_data_thread.cpp \
          threads/logging_thread.cpp \
          threads/trader_thread.cpp

# Object files
OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)

# Default target
all: build-and-clean

# Create directories if they don't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/api
	mkdir -p $(OBJ_DIR)/api/base
	mkdir -p $(OBJ_DIR)/api/clock
	mkdir -p $(OBJ_DIR)/api/market
	mkdir -p $(OBJ_DIR)/api/orders
	mkdir -p $(OBJ_DIR)/core
	mkdir -p $(OBJ_DIR)/data
	mkdir -p $(OBJ_DIR)/ui
	mkdir -p $(OBJ_DIR)/utils
	mkdir -p $(OBJ_DIR)/logging
	mkdir -p $(OBJ_DIR)/threads
	mkdir -p $(OBJ_DIR)/threads/config
	mkdir -p $(OBJ_DIR)/threads/platform
	mkdir -p $(OBJ_DIR)/threads/platform/linux
	mkdir -p $(OBJ_DIR)/threads/platform/macos
	mkdir -p $(OBJ_DIR)/threads/platform/windows

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