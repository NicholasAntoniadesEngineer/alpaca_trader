# Modern C++ Trading System Build Configuration

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
  src/api/market_clock.cpp \
  src/api/market_data_client.cpp \
  src/api/alpaca_client.cpp \
  src/api/order_client.cpp \
  src/core/trader/trader.cpp \
  src/core/trader/trading_engine/trading_engine.cpp \
  src/core/trader/analysis/risk_manager.cpp \
  src/core/trader/trading_engine/order_execution_engine.cpp \
  src/core/trader/trading_engine/position_manager.cpp \
  src/core/trader/trading_engine/trade_validator.cpp \
  src/core/trader/analysis/strategy_logic.cpp \
  src/core/trader/analysis/risk_logic.cpp \
  src/core/trader/analysis/indicators.cpp \
  src/core/trader/analysis/price_manager.cpp \
  src/core/trader/data/market_processing.cpp \
  src/core/trader/data/market_data_fetcher.cpp \
  src/core/utils/connectivity_manager.cpp \
  src/core/system/system_manager.cpp \
  src/core/threads/thread_register.cpp \
  src/core/system/system_monitor.cpp \
 \
  src/core/trader/config_loader/config_loader.cpp \
  src/core/trader/data/account_manager.cpp \
  src/core/utils/time_utils.cpp \
  src/core/logging/account_logs.cpp \
  src/core/logging/market_data_logs.cpp \
  src/core/logging/risk_logs.cpp \
  src/core/utils/http_utils.cpp \
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

# Production source files
PRODUCTION_SOURCES = src/production_main.cpp \
  src/api/clock/market_clock.cpp \
  src/api/market/market_data_client.cpp \
  src/api/alpaca_client.cpp \
  src/api/orders/order_client.cpp \
  src/core/trader/trader.cpp \
  src/core/trader/trading_engine/trading_engine.cpp \
  src/core/trader/analysis/risk_manager.cpp \
  src/core/trader/trading_engine/order_execution_engine.cpp \
  src/core/trader/trading_engine/position_manager.cpp \
  src/core/trader/trading_engine/trade_validator.cpp \
  src/core/trader/analysis/strategy_logic.cpp \
  src/core/trader/analysis/risk_logic.cpp \
  src/core/trader/analysis/indicators.cpp \
  src/core/trader/analysis/price_manager.cpp \
  src/core/trader/data/market_processing.cpp \
  src/core/trader/data/market_data_fetcher.cpp \
  src/core/utils/connectivity_manager.cpp \
  src/core/system/system_manager.cpp \
  src/core/threads/thread_register.cpp \
  src/core/system/system_monitor.cpp \
  src/core/trader/config_loader/config_loader.cpp \
  src/core/trader/data/account_manager.cpp \
  src/core/utils/time_utils.cpp \
  src/core/logging/account_logs.cpp \
  src/core/logging/market_data_logs.cpp \
  src/core/logging/risk_logs.cpp \
  src/core/utils/http_utils.cpp \
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
PRODUCTION_OBJECTS = $(PRODUCTION_SOURCES:%.cpp=$(OBJ_DIR)/%.o)

# Production target
PRODUCTION_TARGET = $(BIN_DIR)/alpaca_trader_production_$(GIT_HASH)

# Default target
all: build-and-clean

# Production target
production: $(PRODUCTION_TARGET) clean-obj

# Create directories if they don't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/src
	mkdir -p $(OBJ_DIR)/src/api
	mkdir -p $(OBJ_DIR)/src/core
	mkdir -p $(OBJ_DIR)/src/core/system
	mkdir -p $(OBJ_DIR)/src/core/trader/core
	mkdir -p $(OBJ_DIR)/src/core/trader/analysis
	mkdir -p $(OBJ_DIR)/src/core/trader/config_loader
	mkdir -p $(OBJ_DIR)/src/core/trader/data
	mkdir -p $(OBJ_DIR)/src/core/trader/trading_engine
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

# Link the production target
$(PRODUCTION_TARGET): $(PRODUCTION_OBJECTS) | $(BIN_DIR)
	$(CXX) $(PRODUCTION_OBJECTS) -o $@ $(LIBS)
	@ln -sf $(notdir $(PRODUCTION_TARGET)) $(BIN_DIR)/alpaca_trader_production_latest
	@echo "Production build successful: $(PRODUCTION_TARGET)"
	@echo "Production symlink created: $(BIN_DIR)/alpaca_trader_production_latest"

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
	@echo "Alpaca Trader Build System"
	@echo "========================="
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build main executable"
	@echo "  production   - Build production executable with monitoring"
	@echo "  clean        - Remove build artifacts"
	@echo "  clean-all    - Remove all artifacts and logs"
	@echo "  install      - Install dependencies (macOS)"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Versioning:"
	@echo "  Standard:    alpaca_trader_$(GIT_HASH)"
	@echo "  Production:  alpaca_trader_production_$(GIT_HASH)"
	@echo "  Symlinks:    alpaca_trader_latest, alpaca_trader_production_latest"

# Prevent make from treating file names as targets
.PHONY: all clean clean-obj clean-all rebuild help build-and-clean