# Strategy Profiles

This directory contains individual strategy configuration files that can be used with the Alpaca Trader system.

## Available Strategies

### 01_conservative.csv
- **Perfect for**: Trending markets, risk-averse traders
- **Characteristics**: Few trades, high win rate, strong confirmations
- **Risk Level**: LOW | Signal Frequency: LOW | Hold Time: MEDIUM-LONG

### 02_ultra_aggressive.csv
- **Perfect for**: Scalping, high-volume trading, catching every move
- **Characteristics**: Many trades, mixed win rate, minimal confirmations
- **Risk Level**: HIGH | Signal Frequency: VERY HIGH | Hold Time: VERY SHORT

### 03_breakout.csv
- **Perfect for**: Range-bound markets, resistance/support breaks
- **Characteristics**: Medium trades, good win rate, level-based entries
- **Risk Level**: MEDIUM | Signal Frequency: MEDIUM | Hold Time: SHORT-MEDIUM

### 04_trend_following.csv
- **Perfect for**: Trending markets, following momentum, swing trades
- **Characteristics**: Fewer trades, high win rate, trend confirmations
- **Risk Level**: LOW-MEDIUM | Signal Frequency: LOW | Hold Time: LONG

### 05_moderate.csv
- **Perfect for**: Mixed market conditions, moderate risk tolerance
- **Characteristics**: Balanced trades, decent win rate, flexible confirmations
- **Risk Level**: MEDIUM | Signal Frequency: MEDIUM | Hold Time: MEDIUM

### 06_doji_specialist.csv
- **Perfect for**: Support/resistance levels, reversal patterns
- **Characteristics**: Selective trades, pattern-based, reversal focus
- **Risk Level**: MEDIUM | Signal Frequency: LOW | Hold Time: MEDIUM

### 07_scalping.csv
- **Perfect for**: Day trading, quick profits, high-frequency
- **Characteristics**: Very many trades, quick exits, minimal holds
- **Risk Level**: HIGH | Signal Frequency: EXTREME | Hold Time: SECONDS-MINUTES

### 08_swing_trading.csv
- **Perfect for**: Multi-day holds, strong moves, patient trading
- **Characteristics**: Few trades, high conviction, longer holds
- **Risk Level**: LOW | Signal Frequency: VERY LOW | Hold Time: DAYS-WEEKS

### 00_template.csv
- **Template for creating custom strategies**
- Contains all available parameters with value ranges
- Copy and modify to create your own strategy

## How to Use

1. **Select a strategy**: Choose one of the numbered strategy files based on your trading style and market conditions
2. **Copy to main config**: Copy the contents of your chosen strategy file into `runtime_config.csv` in the main config directory
3. **Customize if needed**: Modify any parameters to better suit your specific needs
4. **Test thoroughly**: Always test new strategies with paper trading first

## Strategy Parameters

Each strategy file contains the following parameter categories:

- **Signal Detection**: Controls when buy/sell signals are generated
- **Filter Thresholds**: Sets minimum standards for trade quality
- **Risk Management**: Defines position sizing and risk controls
- **Risk/Reward & Timing**: Sets profit targets and execution frequency

## Creating Custom Strategies

1. Copy `00_template.csv` to a new file (e.g., `09_my_strategy.csv`)
2. Update the header comments with your strategy description
3. Modify the parameter values according to your strategy logic
4. Test thoroughly before using in live trading
