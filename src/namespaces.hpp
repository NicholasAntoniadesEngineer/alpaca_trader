#ifndef NAMESPACES_HPP
#define NAMESPACES_HPP

// Comprehensive namespace using declarations for cleaner code
// This file provides convenient aliases for all major types across the codebase

// API namespace aliases
namespace API = AlpacaTrader::API;
namespace Clock = AlpacaTrader::API::Clock;
namespace Market = AlpacaTrader::API::Market;
namespace Orders = AlpacaTrader::API::Orders;

// Core namespace aliases
namespace Core = AlpacaTrader::Core;

// Logging namespace aliases
namespace Logging = AlpacaTrader::Logging;

// Common type aliases for convenience
using AlpacaTrader::Core::Bar;
using AlpacaTrader::Core::BarRequest;
using AlpacaTrader::Core::OrderRequest;
using AlpacaTrader::Core::ClosePositionRequest;
using AlpacaTrader::Core::ProcessedData;
using AlpacaTrader::Core::MarketSnapshot;
using AlpacaTrader::Core::AccountSnapshot;
using AlpacaTrader::Core::AccountManager;
using AlpacaTrader::Core::Trader;
using AlpacaTrader::API::AlpacaClient;
using AlpacaTrader::Logging::AccountLogger;
using AlpacaTrader::Logging::TradingLogger;
// Note: AsyncLogger has forward declaration conflicts, so we'll use the full namespace

#endif // NAMESPACES_HPP
