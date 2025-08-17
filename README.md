# Alpaca Trader

An algorithmic trading system built in C++ for the Alpaca Markets API. This system implements a multi-threaded, real-time trading bot with risk management, market data processing, and position management capabilities.

## Trading Logic Flow

```mermaid
graph TD
    A[Application Start] --> B[Load Configuration]
    B --> C[Initialize Components]
    C --> D[Start Worker Threads]
    D --> E{Market Open?}
    E -->|No| F[Wait/Sleep]
    E -->|Yes| G[Fetch Market Data]
    G --> H[Calculate Indicators]
    H --> I[Check Risk Conditions]
    I -->|Fail| F
    I -->|Pass| J[Generate Trading Signal]
    J --> K{Signal Valid?}
    K -->|No| F
    K -->|Yes| L[Execute Trade]
    L --> M[Update Positions]
    M --> F
    F --> E
```

## System Architecture

The system uses a multi-threaded architecture with the following key components:

- **Main Application** (`main.cpp`) - Orchestrates components and manages worker threads
- **Trading Engine** (`core/trader.cpp`) - Core trading logic, risk management, and order execution
- **API Client** (`api/alpaca_client.cpp`) - Handles Alpaca API communication and market data
- **Account Manager** (`data/account_manager.cpp`) - Manages account data and position tracking
- **Market Data Worker** (`workers/market_data_worker.cpp`) - Background market data processing
- **Account Data Worker** (`workers/account_data_worker.cpp`) - Background account monitoring
- **Logging System** (`utils/async_logger.cpp`) - Thread-safe asynchronous logging
- **Display Layer** (`ui/account_display.cpp`) - Account status presentation

##  Configuration System

The system uses a modular configuration approach with specialized config structures:

- **`ApiConfig`**: Alpaca API credentials and endpoints
- **`StrategyConfig`**: Trading algorithm parameters (ATR periods, multipliers)
- **`RiskConfig`**: Risk management limits (daily P/L, exposure, position sizing)
- **`TimingConfig`**: Polling intervals and timing parameters
- **`SessionConfig`**: Market hours and timezone settings
- **`TargetConfig`**: Target symbol and trading pair configuration


## Risk Management

### Position Sizing
- **Risk per Trade**: Configurable percentage of account equity
- **ATR-based Sizing**: Position size based on market volatility
- **Maximum Exposure**: Portfolio-wide exposure limits

### Daily Limits
- **Profit Target**: Daily profit taking threshold
- **Maximum Loss**: Daily drawdown protection
- **Circuit Breakers**: Automatic trading halt on limit breach

### Market Conditions
- **Trading Hours**: Automated market session validation
- **Volatility Filters**: ATR-based market condition assessment
- **Volume Validation**: Minimum volume requirements for trades

##  Building and Running

### Prerequisites
- **C++17 compatible compiler** (GCC 7+ or Clang 10+)
- **libcurl** for HTTP requests
- **pthread** for threading support

### Build Commands
```bash
# Clean build
make clean

# Compile
make

# Build and run
make run

# Show help
make help
```

### Running the Application
```bash
# Direct execution
./bin/trader

# Background execution
./bin/trader &

# View logs in real-time
tail -f trade_log.txt
```


## Project Structure

```
Alpaca Trader/
├── README.md                 # This documentation
├── Makefile                  # Build system configuration
├── trade_log.txt            # Runtime logging output
├── bin/                     # Compiled binaries
│   └── trader              # Main executable
├── src/                     # Source code implementation
│   ├── main.cpp            # Application entry point and orchestration
│   ├── api/
│   │   └── alpaca_client.cpp    # Alpaca API client implementation
│   ├── core/
│   │   └── trader.cpp           # Core trading logic and decision engine
│   ├── data/
│   │   └── account_manager.cpp  # Account data management and operations
│   ├── ui/
│   │   └── account_display.cpp  # Account status presentation layer
│   ├── utils/
│   │   ├── async_logger.cpp     # Thread-safe logging system
│   │   ├── http_utils.cpp       # HTTP utilities for API communication
│   │   └── indicators.cpp       # Technical analysis indicators
│   └── workers/
│       ├── account_data_worker.cpp  # Background account data fetching
│       └── market_data_worker.cpp   # Background market data processing
└── include/                 # Header files and interfaces
    ├── Config.hpp           # Master configuration aggregator
    ├── configs/             # Configuration structures
    ├── api/
    │   └── alpaca_client.hpp    # Alpaca API client interface
    ├── core/
    │   └── trader.hpp           # Core trading engine interface
    ├── data/
    │   ├── account_manager.hpp  # Account management interface
    │   └── data_structures.hpp  # Common data structures and types
    ├── external/
    │   └── json.hpp            # JSON parsing library (nlohmann/json)
    ├── ui/
    │   └── account_display.hpp  # Account display interface
    ├── utils/
    │   ├── async_logger.hpp     # Async logging interface
    │   ├── http_utils.hpp       # HTTP utilities interface
    │   └── indicators.hpp       # Technical indicators interface
    └── workers/
        ├── account_data_worker.hpp  # Account data worker interface
        └── market_data_worker.hpp   # Market data worker interface
```
