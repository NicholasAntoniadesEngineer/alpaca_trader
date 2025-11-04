#include "websocket_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "logging/logger/logging_macros.hpp"
#include <iomanip>
#include <sstream>
#include <ctime>

using AlpacaTrader::Logging::log_message;

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_connection_attempt(const std::string& websocketUrlString, const std::string& logFileString) {
    log_message(std::string("Attempting connection to ") + websocketUrlString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_connection_success(const std::string& websocketUrlString, const std::string& logFileString) {
    log_message(std::string("Successfully connected to ") + websocketUrlString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_connection_failure(const std::string& websocketUrlString, const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("Connection failure to ") + websocketUrlString + " - " + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_disconnection(const std::string& logFileString) {
    log_message("Connection closed", logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_authentication_attempt(const std::string& logFileString) {
    log_message("Attempting authentication", logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_authentication_success(const std::string& logFileString) {
    log_message("Authentication successful", logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_authentication_failure(const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("Authentication failure - ") + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_subscription_attempt(const std::string& subscriptionParamsString, const std::string& logFileString) {
    log_message(std::string("Attempting subscription to ") + subscriptionParamsString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_subscription_success(const std::string& subscriptionParamsString, const std::string& logFileString) {
    log_message(std::string("Successfully subscribed to ") + subscriptionParamsString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_subscription_failure(const std::string& subscriptionParamsString, const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("Subscription failure for ") + subscriptionParamsString + " - " + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_received(const std::string& logFileString) {
    log_message("Message received", logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_send_failure(const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("Failed to send message - ") + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_receive_error(const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("Receive error - ") + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_reconnection_attempt(const std::string& logFileString) {
    log_message("Attempting reconnection", logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_reconnection_success(const std::string& logFileString) {
    log_message("Reconnection successful", logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_reconnection_failure(const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("Reconnection failure - ") + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_ssl_error(const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("SSL error - ") + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_handshake_error(const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("Handshake error - ") + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_handshake_request(const std::string& requestString, const std::string& logFileString) {
    log_message(std::string("Sending handshake request (") + std::to_string(requestString.length()) + " bytes):", logFileString);
    log_message(requestString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_handshake_response(const std::string& responseString, const std::string& logFileString) {
    log_message(std::string("Received handshake response (") + std::to_string(responseString.length()) + " bytes):", logFileString);
    size_t maxLengthToLog = responseString.length() > 1024 ? 1024 : responseString.length();
    log_message(responseString.substr(0, maxLengthToLog) + (responseString.length() > 1024 ? "..." : ""), logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_handshake_bytes_sent(int bytesSentValue, int totalBytesValue, const std::string& logFileString) {
    log_message(std::string("Handshake bytes sent: ") + std::to_string(bytesSentValue) + " / " + std::to_string(totalBytesValue), logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_handshake_bytes_received(int bytesReceivedValue, const std::string& logFileString) {
    log_message(std::string("Handshake bytes received: ") + std::to_string(bytesReceivedValue), logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_ssl_read_error(int sslErrorCodeValue, const std::string& logFileString) {
    log_message(std::string("SSL read error code: ") + std::to_string(sslErrorCodeValue), logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_frame_parse_error(const std::string& errorMessageString, const std::string& logFileString) {
    log_message(std::string("Frame parse error - ") + errorMessageString, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_bar_data_received(const std::string& symbolString, double openPriceValue, double highPriceValue, double lowPriceValue, double closePriceValue, double volumeValue, const std::string& timestampString, const std::string& logFileString) {
    std::string logMessageString = "Bar data received - Symbol: " + symbolString +
                                    ", Open: " + std::to_string(openPriceValue) +
                                    ", High: " + std::to_string(highPriceValue) +
                                    ", Low: " + std::to_string(lowPriceValue) +
                                    ", Close: " + std::to_string(closePriceValue) +
                                    ", Volume: " + std::to_string(volumeValue) +
                                    ", Timestamp: " + timestampString;
    log_message(logMessageString, logFileString);
}



void AlpacaTrader::Logging::WebSocketLogs::log_websocket_status_message(const std::string& statusValue, const std::string& messageValue, const std::string& logFileString) {
    std::ostringstream statusStream;
    statusStream << "  Status │ " << std::setw(15) << statusValue << " │ " << messageValue;
    log_message(statusStream.str(), logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(const std::string& messageTypeString, const std::string& messageContentString, const std::string& logFileString) {
    size_t maxContentLength = 500;
    std::string truncatedContent = messageContentString.length() > maxContentLength ? 
                                   messageContentString.substr(0, maxContentLength) + "..." : 
                                   messageContentString;
    std::string logMessageString = "Message received - Type: " + messageTypeString +
                                   ", Content: " + truncatedContent;
    log_message(logMessageString, logFileString);
}


void AlpacaTrader::Logging::WebSocketLogs::log_websocket_stale_data_table(const std::string& timestampString, long long ageSecondsValue, int maxAgeSecondsValue, const std::string& logFileString) {
    std::string headerLineOneValue = "┌───────────────────┬──────────────────────────────────────────────────┐";
    log_message(headerLineOneValue, logFileString);
    std::string headerLineTwoValue = "│ STALE DATA        │ WebSocket Bar Staleness Detection                  │";
    log_message(headerLineTwoValue, logFileString);
    std::string headerLineThreeValue = "├───────────────────┼──────────────────────────────────────────────────┤";
    log_message(headerLineThreeValue, logFileString);
    
    std::string formattedTimestampString = timestampString;
    try {
        long long timestampMillisValue = std::stoll(timestampString);
        time_t timestampSecondsValue = static_cast<time_t>(timestampMillisValue / 1000);
        struct tm* timeInfoPointer = gmtime(&timestampSecondsValue);
        if (timeInfoPointer) {
            std::ostringstream timestampStream;
            timestampStream << std::put_time(timeInfoPointer, "%Y-%m-%d %H:%M:%S UTC");
            formattedTimestampString = timestampStream.str() + " (" + std::to_string(timestampMillisValue) + " ms)";
        }
    } catch (...) {
        formattedTimestampString = timestampString + " (invalid)";
    }
    
    std::string timestampRowValue = "Bar timestamp: " + formattedTimestampString;
    std::string timestampRowLineValue = "│ Timestamp         │ " + timestampRowValue.substr(0, 48);
    if (timestampRowValue.length() < 48) {
        timestampRowLineValue += std::string(48 - timestampRowValue.length(), ' ');
    }
    timestampRowLineValue += " │";
    log_message(timestampRowLineValue, logFileString);
    
    std::string ageRowValue = "Age: " + std::to_string(ageSecondsValue) + " seconds old";
    std::string ageRowLineValue = "│ Age               │ " + ageRowValue.substr(0, 48);
    if (ageRowValue.length() < 48) {
        ageRowLineValue += std::string(48 - ageRowValue.length(), ' ');
    }
    ageRowLineValue += " │";
    log_message(ageRowLineValue, logFileString);
    
    std::string maxAgeRowValue = "Maximum allowed: " + std::to_string(maxAgeSecondsValue) + " seconds";
    std::string maxAgeRowLineValue = "│ Max Age           │ " + maxAgeRowValue.substr(0, 48);
    if (maxAgeRowValue.length() < 48) {
        maxAgeRowLineValue += std::string(48 - maxAgeRowValue.length(), ' ');
    }
    maxAgeRowLineValue += " │";
    log_message(maxAgeRowLineValue, logFileString);
    
    std::string reasonRowValue = "WebSocket must provide real-time data";
    std::string reasonRowLineValue = "│ Reason            │ " + reasonRowValue.substr(0, 48);
    if (reasonRowValue.length() < 48) {
        reasonRowLineValue += std::string(48 - reasonRowValue.length(), ' ');
    }
    reasonRowLineValue += " │";
    log_message(reasonRowLineValue, logFileString);
    
    std::string footerLineValue = "└───────────────────┴──────────────────────────────────────────────────┘";
    log_message(footerLineValue, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_connection_table(const std::string& websocketUrlString, bool successFlag, const std::string& errorMessageString, const std::string& logFileString) {
    std::string headerLineOneValue = "┌───────────────────┬──────────────────────────────────────────────────┐";
    log_message(headerLineOneValue, logFileString);
    std::string headerLineTwoValue = "│ WebSocket Connect│ Connection Status Details                         │";
    log_message(headerLineTwoValue, logFileString);
    std::string headerLineThreeValue = "├───────────────────┼──────────────────────────────────────────────────┤";
    log_message(headerLineThreeValue, logFileString);
    
    std::string urlRowValue = websocketUrlString;
    std::string urlRowLineValue = "│ URL               │ " + urlRowValue.substr(0, 48);
    if (urlRowValue.length() < 48) {
        urlRowLineValue += std::string(48 - urlRowValue.length(), ' ');
    }
    urlRowLineValue += " │";
    log_message(urlRowLineValue, logFileString);
    
    std::string statusRowValue = successFlag ? "Connected successfully" : "Connection failed";
    std::string statusRowLineValue = "│ Status            │ " + statusRowValue.substr(0, 48);
    if (statusRowValue.length() < 48) {
        statusRowLineValue += std::string(48 - statusRowValue.length(), ' ');
    }
    statusRowLineValue += " │";
    log_message(statusRowLineValue, logFileString);
    
    if (!errorMessageString.empty()) {
        std::string errorRowValue = errorMessageString;
        std::string errorRowLineValue = "│ Error             │ " + errorRowValue.substr(0, 48);
        if (errorRowValue.length() < 48) {
            errorRowLineValue += std::string(48 - errorRowValue.length(), ' ');
        }
        errorRowLineValue += " │";
        log_message(errorRowLineValue, logFileString);
    }
    
    std::string footerLineValue = "└───────────────────┴──────────────────────────────────────────────────┘";
    log_message(footerLineValue, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_handshake_table(const std::string& requestString, const std::string& responseString, int bytesSentValue, int bytesReceivedValue, const std::string& logFileString) {
    std::string headerLineOneValue = "┌───────────────────┬──────────────────────────────────────────────────┐";
    log_message(headerLineOneValue, logFileString);
    std::string headerLineTwoValue = "│ WebSocket Handshak│ Handshake Request & Response                     │";
    log_message(headerLineTwoValue, logFileString);
    std::string headerLineThreeValue = "├───────────────────┼──────────────────────────────────────────────────┤";
    log_message(headerLineThreeValue, logFileString);
    
    std::string bytesSentRowValue = "Bytes sent: " + std::to_string(bytesSentValue);
    std::string bytesSentRowLineValue = "│ Request Size      │ " + bytesSentRowValue.substr(0, 48);
    if (bytesSentRowValue.length() < 48) {
        bytesSentRowLineValue += std::string(48 - bytesSentRowValue.length(), ' ');
    }
    bytesSentRowLineValue += " │";
    log_message(bytesSentRowLineValue, logFileString);
    
    std::string requestSingleLineValue = requestString;
    size_t newlinePos = requestSingleLineValue.find('\n');
    while (newlinePos != std::string::npos) {
        requestSingleLineValue.replace(newlinePos, 1, " ");
        newlinePos = requestSingleLineValue.find('\n', newlinePos);
        if (newlinePos != std::string::npos && newlinePos < requestSingleLineValue.length() - 1 && requestSingleLineValue[newlinePos + 1] == '\r') {
            requestSingleLineValue.erase(newlinePos + 1, 1);
        }
    }
    newlinePos = requestSingleLineValue.find('\r');
    while (newlinePos != std::string::npos) {
        requestSingleLineValue.replace(newlinePos, 1, " ");
        newlinePos = requestSingleLineValue.find('\r', newlinePos);
    }
    std::string requestPreviewValue = requestSingleLineValue.length() > 45 ? requestSingleLineValue.substr(0, 42) + "..." : requestSingleLineValue;
    std::string requestRowLineValue = "│ Request           │ " + requestPreviewValue.substr(0, 48);
    if (requestPreviewValue.length() < 48) {
        requestRowLineValue += std::string(48 - requestPreviewValue.length(), ' ');
    }
    requestRowLineValue += " │";
    log_message(requestRowLineValue, logFileString);
    
    std::string bytesReceivedRowValue = "Bytes received: " + std::to_string(bytesReceivedValue);
    std::string bytesReceivedRowLineValue = "│ Response Size     │ " + bytesReceivedRowValue.substr(0, 48);
    if (bytesReceivedRowValue.length() < 48) {
        bytesReceivedRowLineValue += std::string(48 - bytesReceivedRowValue.length(), ' ');
    }
    bytesReceivedRowLineValue += " │";
    log_message(bytesReceivedRowLineValue, logFileString);
    
    std::string responseSingleLineValue = responseString;
    newlinePos = responseSingleLineValue.find('\n');
    while (newlinePos != std::string::npos) {
        responseSingleLineValue.replace(newlinePos, 1, " ");
        newlinePos = responseSingleLineValue.find('\n', newlinePos);
        if (newlinePos != std::string::npos && newlinePos < responseSingleLineValue.length() - 1 && responseSingleLineValue[newlinePos + 1] == '\r') {
            responseSingleLineValue.erase(newlinePos + 1, 1);
        }
    }
    newlinePos = responseSingleLineValue.find('\r');
    while (newlinePos != std::string::npos) {
        responseSingleLineValue.replace(newlinePos, 1, " ");
        newlinePos = responseSingleLineValue.find('\r', newlinePos);
    }
    std::string responsePreviewValue = responseSingleLineValue.length() > 45 ? responseSingleLineValue.substr(0, 42) + "..." : responseSingleLineValue;
    std::string responseRowLineValue = "│ Response          │ " + responsePreviewValue.substr(0, 48);
    if (responsePreviewValue.length() < 48) {
        responseRowLineValue += std::string(48 - responsePreviewValue.length(), ' ');
    }
    responseRowLineValue += " │";
    log_message(responseRowLineValue, logFileString);
    
    std::string footerLineValue = "└───────────────────┴──────────────────────────────────────────────────┘";
    log_message(footerLineValue, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_authentication_table(bool successFlag, const std::string& authMessageString, const std::string& errorMessageString, const std::string& logFileString) {
    std::string headerLineOneValue = "┌───────────────────┬──────────────────────────────────────────────────┐";
    log_message(headerLineOneValue, logFileString);
    std::string headerLineTwoValue = "│ Authentication    │ WebSocket Authentication Status                  │";
    log_message(headerLineTwoValue, logFileString);
    std::string headerLineThreeValue = "├───────────────────┼──────────────────────────────────────────────────┤";
    log_message(headerLineThreeValue, logFileString);
    
    std::string statusRowValue = successFlag ? "Authentication successful" : "Authentication failed";
    std::string statusRowLineValue = "│ Status            │ " + statusRowValue.substr(0, 48);
    if (statusRowValue.length() < 48) {
        statusRowLineValue += std::string(48 - statusRowValue.length(), ' ');
    }
    statusRowLineValue += " │";
    log_message(statusRowLineValue, logFileString);
    
    if (!authMessageString.empty()) {
        std::string authMessagePreviewValue = authMessageString.length() > 45 ? authMessageString.substr(0, 42) + "..." : authMessageString;
        std::string authMessageRowLineValue = "│ Message           │ " + authMessagePreviewValue.substr(0, 48);
        if (authMessagePreviewValue.length() < 48) {
            authMessageRowLineValue += std::string(48 - authMessagePreviewValue.length(), ' ');
        }
        authMessageRowLineValue += " │";
        log_message(authMessageRowLineValue, logFileString);
    }
    
    if (!errorMessageString.empty()) {
        std::string errorRowValue = errorMessageString;
        std::string errorRowLineValue = "│ Error             │ " + errorRowValue.substr(0, 48);
        if (errorRowValue.length() < 48) {
            errorRowLineValue += std::string(48 - errorRowValue.length(), ' ');
        }
        errorRowLineValue += " │";
        log_message(errorRowLineValue, logFileString);
    }
    
    std::string footerLineValue = "└───────────────────┴──────────────────────────────────────────────────┘";
    log_message(footerLineValue, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_subscription_table(const std::string& subscriptionParamsString, bool successFlag, const std::string& errorMessageString, const std::string& logFileString) {
    std::string headerLineOneValue = "┌───────────────────┬──────────────────────────────────────────────────┐";
    log_message(headerLineOneValue, logFileString);
    std::string headerLineTwoValue = "│ Subscription      │ WebSocket Subscription Status                    │";
    log_message(headerLineTwoValue, logFileString);
    std::string headerLineThreeValue = "├───────────────────┼──────────────────────────────────────────────────┤";
    log_message(headerLineThreeValue, logFileString);
    
    std::string paramsRowValue = subscriptionParamsString;
    std::string paramsRowLineValue = "│ Parameters        │ " + paramsRowValue.substr(0, 48);
    if (paramsRowValue.length() < 48) {
        paramsRowLineValue += std::string(48 - paramsRowValue.length(), ' ');
    }
    paramsRowLineValue += " │";
    log_message(paramsRowLineValue, logFileString);
    
    std::string statusRowValue = successFlag ? "Subscribed successfully" : "Subscription failed";
    std::string statusRowLineValue = "│ Status            │ " + statusRowValue.substr(0, 48);
    if (statusRowValue.length() < 48) {
        statusRowLineValue += std::string(48 - statusRowValue.length(), ' ');
    }
    statusRowLineValue += " │";
    log_message(statusRowLineValue, logFileString);
    
    if (!errorMessageString.empty()) {
        std::string errorRowValue = errorMessageString;
        std::string errorRowLineValue = "│ Error             │ " + errorRowValue.substr(0, 48);
        if (errorRowValue.length() < 48) {
            errorRowLineValue += std::string(48 - errorRowValue.length(), ' ');
        }
        errorRowLineValue += " │";
        log_message(errorRowLineValue, logFileString);
    }
    
    std::string footerLineValue = "└───────────────────┴──────────────────────────────────────────────────┘";
    log_message(footerLineValue, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_receive_loop_table(const std::string& messageTypeString, const std::string& messageContentString, const std::string& logFileString) {
    std::string headerLineOneValue = "┌───────────────────┬──────────────────────────────────────────────────┐";
    log_message(headerLineOneValue, logFileString);
    std::string headerLineTwoValue = "│ Receive Loop       │ WebSocket Receive Loop Status                   │";
    log_message(headerLineTwoValue, logFileString);
    std::string headerLineThreeValue = "├───────────────────┼──────────────────────────────────────────────────┤";
    log_message(headerLineThreeValue, logFileString);
    
    std::string typeRowValue = messageTypeString;
    std::string typeRowLineValue = "│ Message Type       │ " + typeRowValue.substr(0, 48);
    if (typeRowValue.length() < 48) {
        typeRowLineValue += std::string(48 - typeRowValue.length(), ' ');
    }
    typeRowLineValue += " │";
    log_message(typeRowLineValue, logFileString);
    
    std::string contentPreviewValue = messageContentString.length() > 48 ? messageContentString.substr(0, 45) + "..." : messageContentString;
    std::string contentRowLineValue = "│ Content            │ " + contentPreviewValue.substr(0, 48);
    if (contentPreviewValue.length() < 48) {
        contentRowLineValue += std::string(48 - contentPreviewValue.length(), ' ');
    }
    contentRowLineValue += " │";
    log_message(contentRowLineValue, logFileString);
    
    std::string footerLineValue = "└───────────────────┴──────────────────────────────────────────────────┘";
    log_message(footerLineValue, logFileString);
}

void AlpacaTrader::Logging::WebSocketLogs::log_websocket_frame_decoded_table(int opcodeValue, size_t payloadLengthValue, size_t messageLengthValue, const std::string& logFileString) {
    std::string headerLineOneValue = "┌───────────────────┬──────────────────────────────────────────────────┐";
    log_message(headerLineOneValue, logFileString);
    std::string headerLineTwoValue = "│ Frame Decoded      │ WebSocket Frame Decoding Information            │";
    log_message(headerLineTwoValue, logFileString);
    std::string headerLineThreeValue = "├───────────────────┼──────────────────────────────────────────────────┤";
    log_message(headerLineThreeValue, logFileString);
    
    std::string opcodeRowValue = "Opcode: " + std::to_string(opcodeValue);
    std::string opcodeRowLineValue = "│ Opcode            │ " + opcodeRowValue.substr(0, 48);
    if (opcodeRowValue.length() < 48) {
        opcodeRowLineValue += std::string(48 - opcodeRowValue.length(), ' ');
    }
    opcodeRowLineValue += " │";
    log_message(opcodeRowLineValue, logFileString);
    
    std::string payloadRowValue = "Payload length: " + std::to_string(payloadLengthValue) + " bytes";
    std::string payloadRowLineValue = "│ Payload Length    │ " + payloadRowValue.substr(0, 48);
    if (payloadRowValue.length() < 48) {
        payloadRowLineValue += std::string(48 - payloadRowValue.length(), ' ');
    }
    payloadRowLineValue += " │";
    log_message(payloadRowLineValue, logFileString);
    
    std::string messageRowValue = "Message length: " + std::to_string(messageLengthValue) + " bytes";
    std::string messageRowLineValue = "│ Message Length    │ " + messageRowValue.substr(0, 48);
    if (messageRowValue.length() < 48) {
        messageRowLineValue += std::string(48 - messageRowValue.length(), ' ');
    }
    messageRowLineValue += " │";
    log_message(messageRowLineValue, logFileString);
    
    std::string footerLineValue = "└───────────────────┴──────────────────────────────────────────────────┘";
    log_message(footerLineValue, logFileString);
}
