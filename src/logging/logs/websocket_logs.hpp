#ifndef WEBSOCKET_LOGS_HPP
#define WEBSOCKET_LOGS_HPP

#include <string>

namespace AlpacaTrader {
namespace Logging {

class WebSocketLogs {
public:
    static void log_websocket_connection_attempt(const std::string& websocketUrlString, const std::string& logFileString);
    static void log_websocket_connection_success(const std::string& websocketUrlString, const std::string& logFileString);
    static void log_websocket_connection_failure(const std::string& websocketUrlString, const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_disconnection(const std::string& logFileString);
    static void log_websocket_authentication_attempt(const std::string& logFileString);
    static void log_websocket_authentication_success(const std::string& logFileString);
    static void log_websocket_authentication_failure(const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_subscription_attempt(const std::string& subscriptionParamsString, const std::string& logFileString);
    static void log_websocket_subscription_success(const std::string& subscriptionParamsString, const std::string& logFileString);
    static void log_websocket_subscription_failure(const std::string& subscriptionParamsString, const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_message_received(const std::string& logFileString);
    static void log_websocket_message_send_failure(const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_receive_error(const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_reconnection_attempt(const std::string& logFileString);
    static void log_websocket_reconnection_success(const std::string& logFileString);
    static void log_websocket_reconnection_failure(const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_ssl_error(const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_handshake_error(const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_handshake_request(const std::string& requestString, const std::string& logFileString);
    static void log_websocket_handshake_response(const std::string& responseString, const std::string& logFileString);
    static void log_websocket_handshake_bytes_sent(int bytesSentValue, int totalBytesValue, const std::string& logFileString);
    static void log_websocket_handshake_bytes_received(int bytesReceivedValue, const std::string& logFileString);
    static void log_websocket_ssl_read_error(int sslErrorCodeValue, const std::string& logFileString);
    static void log_websocket_frame_parse_error(const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_bar_data_received(const std::string& symbolString, double openPriceValue, double highPriceValue, double lowPriceValue, double closePriceValue, double volumeValue, const std::string& timestampString, const std::string& logFileString);
    static void log_websocket_bar_data_table(const std::string& symbolString, double openPriceValue, double highPriceValue, double lowPriceValue, double closePriceValue, double volumeValue, const std::string& timestampString, const std::string& logFileString);
    static void log_websocket_accumulator_status(const std::string& symbolString, size_t barsBeforeValue, size_t barsAfterValue, size_t firstLevelCountValue, size_t secondLevelCountValue, const std::string& logFileString);
    static void log_websocket_status_message(const std::string& statusValue, const std::string& messageValue, const std::string& logFileString);
    static void log_websocket_message_details(const std::string& messageTypeString, const std::string& messageContentString, const std::string& logFileString);
    static void log_websocket_bars_request_table(const std::string& symbolString, int requestedBarsCountValue, size_t availableBarsCountValue, size_t firstLevelBarsCountValue, size_t secondLevelBarsCountValue, const std::string& logFileString);
    static void log_websocket_stale_data_table(const std::string& timestampString, long long ageSecondsValue, int maxAgeSecondsValue, const std::string& logFileString);
    static void log_websocket_connection_table(const std::string& websocketUrlString, bool successFlag, const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_handshake_table(const std::string& requestString, const std::string& responseString, int bytesSentValue, int bytesReceivedValue, const std::string& logFileString);
    static void log_websocket_authentication_table(bool successFlag, const std::string& authMessageString, const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_subscription_table(const std::string& subscriptionParamsString, bool successFlag, const std::string& errorMessageString, const std::string& logFileString);
    static void log_websocket_receive_loop_table(const std::string& messageTypeString, const std::string& messageContentString, const std::string& logFileString);
    static void log_websocket_frame_decoded_table(int opcodeValue, size_t payloadLengthValue, size_t messageLengthValue, const std::string& logFileString);
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // WEBSOCKET_LOGS_HPP

