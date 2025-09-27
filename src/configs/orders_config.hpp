// OrdersConfig.hpp
#ifndef ORDERS_CONFIG_HPP
#define ORDERS_CONFIG_HPP

#include <string>

struct OrdersConfig {
    // Order validation
    int min_quantity;                    // Minimum order quantity
    int price_precision;                 // Decimal places for price formatting
    
    // Order types and time in force
    std::string default_time_in_force;   // Default time in force (gtc, ioc, etc.)
    std::string default_order_type;      // Default order type (market, limit, etc.)
    
    // Position closure settings
    std::string position_closure_side_buy;   // Side for closing long positions
    std::string position_closure_side_sell;  // Side for closing short positions
    
    // API endpoints
    std::string orders_endpoint;         // Orders API endpoint
    std::string positions_endpoint;      // Positions API endpoint
    std::string orders_status_filter;    // Status filter for orders query
    
    // Error handling
    bool zero_quantity_check;            // Enable zero quantity validation
    bool position_verification_enabled;  // Enable position verification after closure
    
    // Order cancellation strategy
    std::string cancellation_mode;       // "all", "directional", "opposite_only"
    bool cancel_opposite_side;           // Cancel orders opposite to new signal
    bool cancel_same_side;               // Cancel orders same side as new signal
    int max_orders_to_cancel;            // Maximum orders to cancel per operation
};

#endif // ORDERS_CONFIG_HPP
