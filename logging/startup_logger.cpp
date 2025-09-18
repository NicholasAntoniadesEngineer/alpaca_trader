#include "startup_logger.hpp"
#include "async_logger.hpp"
#include "logging_macros.hpp"
#include "../configs/timing_config.hpp"
#include <iomanip>
#include <sstream>

std::string StartupLogger::format_currency(double amount) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

void StartupLogger::log_application_header() {
    log_message("", "");
    log_message("================================================================================", "");
    log_message("                                   ALPACA TRADER", "");
    log_message("                            Advanced Momentum Trading Bot", "");
    log_message("================================================================================", "");
    log_message("", "");
}

void StartupLogger::log_account_status_header() {
    log_message("", "");
    log_message("================================================================================", "");
    log_message("                              ACCOUNT STATUS SUMMARY", "");
    log_message("================================================================================", "");
}

void StartupLogger::log_account_overview(const AccountManager& account_manager) {
    auto account_info = account_manager.get_account_info();
    
    LOG_STARTUP_SECTION_HEADER("ACCOUNT OVERVIEW");
    if (!account_info.account_number.empty()) {
        LOG_STARTUP_CONTENT("Account Number: " + account_info.account_number);
    }
    if (!account_info.status.empty()) {
        LOG_STARTUP_CONTENT("Status: " + account_info.status);
    }
    if (!account_info.currency.empty()) {
        LOG_STARTUP_CONTENT("Currency: " + account_info.currency);
    }
    LOG_STARTUP_CONTENT("Pattern Day Trader: " + std::string(account_info.pattern_day_trader ? "YES" : "NO"));
    
    if (!account_info.trading_blocked_reason.empty()) {
        LOG_STARTUP_CONTENT("Trading Blocked: " + account_info.trading_blocked_reason);
    }
    if (!account_info.transfers_blocked_reason.empty()) {
        LOG_STARTUP_CONTENT("Transfers Blocked: " + account_info.transfers_blocked_reason);
    }
    if (!account_info.account_blocked_reason.empty()) {
        LOG_STARTUP_CONTENT("Account Blocked: " + account_info.account_blocked_reason);
    }
    if (!account_info.created_at.empty()) {
        LOG_STARTUP_CONTENT("Created: " + account_info.created_at);
    }
    LOG_STARTUP_SEPARATOR();
}

void StartupLogger::log_financial_summary(const AccountManager& account_manager) {
    auto account_info = account_manager.get_account_info();
    
    LOG_STARTUP_SECTION_HEADER("FINANCIAL SUMMARY");
    LOG_STARTUP_CONTENT("Equity: " + format_currency(account_info.equity));
    LOG_STARTUP_CONTENT("Last Equity: " + format_currency(account_info.last_equity));
    LOG_STARTUP_CONTENT("Cash: " + format_currency(account_info.cash));
    LOG_STARTUP_CONTENT("Buying Power: " + format_currency(account_info.buying_power));
    LOG_STARTUP_CONTENT("Long Market Value: " + format_currency(account_info.long_market_value));
    LOG_STARTUP_CONTENT("Short Market Value: " + format_currency(account_info.short_market_value));
    LOG_STARTUP_CONTENT("Initial Margin: " + format_currency(account_info.initial_margin));
    LOG_STARTUP_CONTENT("Maintenance Margin: " + format_currency(account_info.maintenance_margin));
    LOG_STARTUP_CONTENT("SMA: " + format_currency(account_info.sma));
    LOG_STARTUP_CONTENT("Day Trade Count: " + std::to_string(static_cast<int>(account_info.day_trade_count)));
    LOG_STARTUP_CONTENT("RegT Buying Power: " + format_currency(account_info.regt_buying_power));
    LOG_STARTUP_CONTENT("Day Trading Buying Power: " + format_currency(account_info.daytrading_buying_power));
    LOG_STARTUP_SEPARATOR();
}

void StartupLogger::log_current_positions(const AccountManager& account_manager) {
    auto snapshot = account_manager.get_account_snapshot();
    
    log_message("+-- CURRENT POSITIONS", "");
    
    if (snapshot.pos_details.qty == 0) {
        LOG_STARTUP_CONTENT("No positions held");
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        std::string side = (snapshot.pos_details.qty > 0) ? "LONG" : "SHORT";
        oss << "Position: " << side << " " << std::abs(snapshot.pos_details.qty) << " shares";
        LOG_STARTUP_CONTENT(oss.str());
        
        oss.str("");
        oss << "Current Value: $" << snapshot.pos_details.current_value;
        LOG_STARTUP_CONTENT(oss.str());
        
        oss.str("");
        oss << "Unrealized P/L: $" << snapshot.pos_details.unrealized_pl;
        LOG_STARTUP_CONTENT(oss.str());
        
        oss.str("");
        oss << "Exposure: " << std::setprecision(1) << snapshot.exposure_pct << "%";
        LOG_STARTUP_CONTENT(oss.str());
    }
    
    if (snapshot.open_orders > 0) {
        LOG_STARTUP_CONTENT("Open Orders: " + std::to_string(snapshot.open_orders));
    }
    
    LOG_STARTUP_SEPARATOR();
}

void StartupLogger::log_account_status_footer() {
    log_message("-------------------------------------------------------------------------------", "");
    log_message("", "");
}

void StartupLogger::log_data_source_configuration(const SystemConfig& config) {
    log_message("", "");
    log_message("================================================================================", "");
    log_message("                            DATA SOURCE CONFIGURATION", "");
    log_message("================================================================================", "");
    log_message("HISTORICAL BARS:", "");
    log_message("   - IEX FEED (FREE): 15-minute delayed, limited symbol coverage", "");
    log_message("   - SIP FEED (PAID): Real-time, full market coverage ($100+/month)", "");
    log_message("", "");
    log_message("REAL-TIME QUOTES:", "");
    log_message("   - IEX FREE QUOTES: Limited symbols, may not be available", "");
    log_message("   - FALLBACK: Delayed bar close prices with conservative buffers", "");
    log_message("", "");
    log_message("TRADING SYMBOL: " + config.target.symbol, "");
    std::string account_type = (config.api.base_url.find("paper") != std::string::npos) ? "PAPER TRADING" : "LIVE TRADING";
    log_message("ACCOUNT TYPE: " + account_type, "");
    log_message("================================================================================", "");
    log_message("", "");
}

void StartupLogger::log_thread_system_startup(const TimingConfig& timing_config) {
    LOG_STARTUP_SECTION_HEADER("THREAD SYSTEM INITIALIZATION");
    LOG_STARTUP_CONTENT("System Status:");
    
    if (timing_config.thread_priorities.enable_thread_priorities) {
        LOG_STARTUP_CONTENT("  - Thread priorities: ENABLED");
    } else {
        LOG_STARTUP_CONTENT("  - Thread priorities: DISABLED");
    }
    
    if (timing_config.thread_priorities.enable_cpu_affinity) {
        LOG_STARTUP_CONTENT("  - CPU affinity: ENABLED");
    } else {
        LOG_STARTUP_CONTENT("  - CPU affinity: DISABLED");
    }
    LOG_STARTUP_SEPARATOR();
    LOG_STARTUP_CONTENT("Thread Startup:");
}

void StartupLogger::log_thread_started(const std::string& thread_name, const std::string& thread_info) {
    LOG_STARTUP_CONTENT(thread_name + " thread started: " + thread_info);
}

void StartupLogger::log_thread_priority_status(const std::string& thread_name, const std::string& priority, bool success) {
    std::ostringstream oss;
    oss << thread_name << ": " << priority << " priority [" << (success ? "OK" : "FAIL") << "]";
    LOG_STARTUP_CONTENT(oss.str());
}

void StartupLogger::log_thread_system_complete() {
    LOG_STARTUP_SECTION_HEADER("");
}

void StartupLogger::log_trading_configuration(const TraderConfig& config) {
    log_message("", "");
    LOG_STARTUP_SECTION_HEADER("CONFIGURATION:");
    LOG_STARTUP_CONTENT("Symbol: " + config.target.symbol);
    
    std::ostringstream risk_oss;
    risk_oss << "Risk per Trade: " << std::fixed << std::setprecision(3) << (config.risk.risk_per_trade * 100) << "%";
    LOG_STARTUP_CONTENT(risk_oss.str());
    
    LOG_STARTUP_CONTENT("Risk/Reward Ratio: 1:" + std::to_string(config.strategy.rr_ratio));
    LOG_STARTUP_CONTENT("Loop Interval: " + std::to_string(config.timing.sleep_interval_sec) + " seconds");
    LOG_STARTUP_SECTION_HEADER("");
    log_message("", "");
}
