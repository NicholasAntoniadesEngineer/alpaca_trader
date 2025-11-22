#include "websocket_client.hpp"
#include "logging/logs/websocket_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "json/json.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <cstring>

using json = nlohmann::json;
using AlpacaTrader::Logging::WebSocketLogs;

namespace AlpacaTrader {
namespace API {
namespace Polygon {

WebSocketClient::WebSocketClient()
    : parentLoggingContextPointer(nullptr)
    , connectedFlag(false)
    , shouldReceiveLoopContinue(false)
    , socketFileDescriptor(-1)
    , sslContextPointer(nullptr)
    , sslConnectionPointer(nullptr)
{
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    lastErrorStringValue.clear();
}

WebSocketClient::~WebSocketClient() {
    try {
        disconnect();
    } catch (const std::exception& destructorExceptionError) {
        lastErrorStringValue = std::string("Destructor error: ") + destructorExceptionError.what();
    } catch (...) {
        lastErrorStringValue = "Unknown destructor error";
    }
}

bool WebSocketClient::connect(const std::string& websocketUrlString) {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    
    try {
        // Always cleanup any existing connection before creating new one
        // This prevents connection limit issues from accumulating connections
        if (connectedFlag.load() || socketFileDescriptor >= 0 || sslConnectionPointer || sslContextPointer) {
            try {
                cleanupConnection();
            } catch (...) {
                // Ignore cleanup errors, continue with new connection
            }
            connectedFlag.store(false);
        }
        
        if (!validateUrl(websocketUrlString)) {
            lastErrorStringValue = "Invalid WebSocket URL format";
            return false;
        }
        
        websocketUrlStringValue = websocketUrlString;
        
        if (!establishTcpConnection()) {
            WebSocketLogs::log_websocket_connection_table(websocketUrlString, false, lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        if (!performSslHandshake()) {
            cleanupConnection();
            WebSocketLogs::log_websocket_connection_table(websocketUrlString, false, lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        if (!performWebSocketHandshake()) {
            cleanupConnection();
            WebSocketLogs::log_websocket_connection_table(websocketUrlString, false, lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        connectedFlag.store(true);
        return true;
        
    } catch (const std::exception& connectionExceptionError) {
        lastErrorStringValue = std::string("Connection exception: ") + connectionExceptionError.what();
        WebSocketLogs::log_websocket_connection_table(websocketUrlString, false, lastErrorStringValue, "trading_system.log");
        cleanupConnection();
        return false;
    } catch (...) {
        lastErrorStringValue = "Unknown connection error";
        WebSocketLogs::log_websocket_connection_table(websocketUrlString, false, lastErrorStringValue, "trading_system.log");
        cleanupConnection();
        return false;
    }
}

void WebSocketClient::disconnect() {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    
    try {
        // Only cleanup if not already disconnected to prevent double-free
        if (connectedFlag.load() || socketFileDescriptor >= 0 || sslConnectionPointer || sslContextPointer) {
            stopReceiveLoop();
            cleanupConnection();
            connectedFlag.store(false);
            try {
                WebSocketLogs::log_websocket_disconnection("trading_system.log");
            } catch (...) {
                // Logging failed, continue
            }
        }
    } catch (const std::exception& disconnectExceptionError) {
        lastErrorStringValue = std::string("Disconnect error: ") + disconnectExceptionError.what();
    } catch (...) {
        lastErrorStringValue = "Unknown disconnect error";
    }
}

bool WebSocketClient::isConnected() const {
    return connectedFlag.load();
}

bool WebSocketClient::authenticate(const std::string& apiKeyString) {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    
    try {
        if (!connectedFlag.load()) {
            lastErrorStringValue = "Not connected to WebSocket";
            return false;
        }
        
        apiKeyStringValue = apiKeyString;
        
        json authMessageJson;
        authMessageJson["action"] = "auth";
        authMessageJson["params"] = apiKeyString;
        
        std::string authMessageString = authMessageJson.dump();
        
        bool sendResultValue = sendMessageInternal(authMessageString);
        
        if (!sendResultValue) {
            WebSocketLogs::log_websocket_authentication_table(false, authMessageString, lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& authExceptionError) {
        lastErrorStringValue = std::string("Authentication exception: ") + authExceptionError.what();
        json authMessageJson;
        authMessageJson["action"] = "auth";
        authMessageJson["params"] = apiKeyString;
        std::string authMessageString = authMessageJson.dump();
        WebSocketLogs::log_websocket_authentication_table(false, authMessageString, lastErrorStringValue, "trading_system.log");
        return false;
    } catch (...) {
        lastErrorStringValue = "Unknown authentication error";
        json authMessageJson;
        authMessageJson["action"] = "auth";
        authMessageJson["params"] = apiKeyString;
        std::string authMessageString = authMessageJson.dump();
        WebSocketLogs::log_websocket_authentication_table(false, authMessageString, lastErrorStringValue, "trading_system.log");
        return false;
    }
}

bool WebSocketClient::subscribe(const std::string& subscriptionParamsString) {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    
    try {
        if (!connectedFlag.load()) {
            lastErrorStringValue = "Not connected to WebSocket";
            return false;
        }
        
        subscriptionParamsStringValue = subscriptionParamsString;
        
        json subscribeMessageJson;
        subscribeMessageJson["action"] = "subscribe";
        subscribeMessageJson["params"] = subscriptionParamsString;
        
        std::string subscribeMessageString = subscribeMessageJson.dump();
        
        if (!sendMessageInternal(subscribeMessageString)) {
            WebSocketLogs::log_websocket_subscription_table(subscriptionParamsString, false, lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        WebSocketLogs::log_websocket_subscription_table(subscriptionParamsString, true, "", "trading_system.log");
        return true;
        
    } catch (const std::exception& subscribeExceptionError) {
        lastErrorStringValue = std::string("Subscription exception: ") + subscribeExceptionError.what();
        WebSocketLogs::log_websocket_subscription_table(subscriptionParamsString, false, lastErrorStringValue, "trading_system.log");
        return false;
    } catch (...) {
        lastErrorStringValue = "Unknown subscription error";
        WebSocketLogs::log_websocket_subscription_table(subscriptionParamsString, false, lastErrorStringValue, "trading_system.log");
        return false;
    }
}

bool WebSocketClient::unsubscribe(const std::string& subscriptionParamsString) {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    
    try {
        if (!connectedFlag.load()) {
            lastErrorStringValue = "Not connected to WebSocket";
            return false;
        }
        
        json unsubscribeMessageJson;
        unsubscribeMessageJson["action"] = "unsubscribe";
        unsubscribeMessageJson["params"] = subscriptionParamsString;
        
        std::string unsubscribeMessageString = unsubscribeMessageJson.dump();
        return sendMessageInternal(unsubscribeMessageString);
        
    } catch (const std::exception& unsubscribeExceptionError) {
        lastErrorStringValue = std::string("Unsubscribe exception: ") + unsubscribeExceptionError.what();
        return false;
    } catch (...) {
        lastErrorStringValue = "Unknown unsubscribe error";
        return false;
    }
}

void WebSocketClient::setMessageCallback(MessageCallback callbackFunction) {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    messageCallbackFunction = callbackFunction;
}

bool WebSocketClient::sendMessage(const std::string& messageContent) {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    return sendMessageInternal(messageContent);
}

bool WebSocketClient::sendMessageInternal(const std::string& messageContent) {
    try {
        bool connectionStateValue = connectedFlag.load();
        
        if (!connectionStateValue) {
            lastErrorStringValue = "Not connected to WebSocket";
            WebSocketLogs::log_websocket_message_send_failure(lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        std::vector<unsigned char> frameBuffer;
        size_t messageLength = messageContent.length();
        
        frameBuffer.push_back(0x81);
        
        std::vector<unsigned char> maskingKeyBytes = generateRandomBytes(4);
        
        if (messageLength < 126) {
            frameBuffer.push_back(static_cast<unsigned char>(messageLength | 0x80));
        } else if (messageLength < 65536) {
            frameBuffer.push_back(126 | 0x80);
            frameBuffer.push_back(static_cast<unsigned char>((messageLength >> 8) & 0xFF));
            frameBuffer.push_back(static_cast<unsigned char>(messageLength & 0xFF));
        } else {
            frameBuffer.push_back(127 | 0x80);
            for (int i = 7; i >= 0; --i) {
                frameBuffer.push_back(static_cast<unsigned char>((messageLength >> (i * 8)) & 0xFF));
            }
        }
        
        frameBuffer.insert(frameBuffer.end(), maskingKeyBytes.begin(), maskingKeyBytes.end());
        
        std::vector<unsigned char> maskedPayload(messageContent.begin(), messageContent.end());
        for (size_t i = 0; i < maskedPayload.size(); ++i) {
            maskedPayload[i] ^= maskingKeyBytes[i % 4];
        }
        
        frameBuffer.insert(frameBuffer.end(), maskedPayload.begin(), maskedPayload.end());
        
        int totalBytesSent = 0;
        size_t totalBytesToSend = frameBuffer.size();
        int sendAttemptsCount = 0;
        const int maxSendAttemptsValue = 10;
        
        while (totalBytesSent < static_cast<int>(totalBytesToSend) && sendAttemptsCount < maxSendAttemptsValue) {
            sendAttemptsCount++;
            
            int bytesSent = 0;
            if (sslConnectionPointer) {
                SSL* sslConnectionPointerTyped = static_cast<SSL*>(sslConnectionPointer);
                bytesSent = SSL_write(sslConnectionPointerTyped, 
                                     frameBuffer.data() + totalBytesSent, 
                                     static_cast<int>(totalBytesToSend - totalBytesSent));
                
                if (bytesSent <= 0) {
                    int sslErrorCode = SSL_get_error(sslConnectionPointerTyped, bytesSent);
                    
                    if (sslErrorCode == SSL_ERROR_WANT_READ || sslErrorCode == SSL_ERROR_WANT_WRITE) {
                        fd_set readFileDescriptorSet;
                        fd_set writeFileDescriptorSet;
                        struct timeval timeoutStruct;
                        
                        FD_ZERO(&readFileDescriptorSet);
                        FD_ZERO(&writeFileDescriptorSet);
                        
                        if (sslErrorCode == SSL_ERROR_WANT_READ) {
                            FD_SET(socketFileDescriptor, &readFileDescriptorSet);
                        } else {
                            FD_SET(socketFileDescriptor, &writeFileDescriptorSet);
                        }
                        
                        timeoutStruct.tv_sec = 5;
                        timeoutStruct.tv_usec = 0;
                        
                        int selectResult = select(socketFileDescriptor + 1, &readFileDescriptorSet, &writeFileDescriptorSet, nullptr, &timeoutStruct);
                        if (selectResult <= 0) {
                            unsigned long opensslErrorCode = ERR_get_error();
                            char errorBuffer[512];
                            if (opensslErrorCode != 0) {
                                ERR_error_string_n(opensslErrorCode, errorBuffer, sizeof(errorBuffer));
                                lastErrorStringValue = std::string("SSL write timeout/error: ") + errorBuffer + " (SSL_get_error: " + std::to_string(sslErrorCode) + ")";
                            } else {
                                lastErrorStringValue = "SSL write timeout: select() returned " + std::to_string(selectResult) + ", SSL_get_error=" + std::to_string(sslErrorCode);
                            }
                            WebSocketLogs::log_websocket_message_send_failure(lastErrorStringValue, "trading_system.log");
                            return false;
                        }
                        continue;
                    } else {
                        unsigned long opensslErrorCode = ERR_get_error();
                        char errorBuffer[512];
                        if (opensslErrorCode != 0) {
                            ERR_error_string_n(opensslErrorCode, errorBuffer, sizeof(errorBuffer));
                            lastErrorStringValue = std::string("SSL write failed: ") + errorBuffer + " (SSL_get_error: " + std::to_string(sslErrorCode) + ")";
                        } else {
                            lastErrorStringValue = std::string("SSL write failed: SSL_get_error=") + std::to_string(sslErrorCode) + ", bytesSent=" + std::to_string(bytesSent);
                        }
                        WebSocketLogs::log_websocket_message_send_failure(lastErrorStringValue, "trading_system.log");
                        return false;
                    }
                }
            } else {
                bytesSent = ::write(socketFileDescriptor, frameBuffer.data() + totalBytesSent, totalBytesToSend - totalBytesSent);
                
                if (bytesSent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    lastErrorStringValue = std::string("write() failed: errno=") + std::to_string(errno) + " (" + strerror(errno) + ")";
                    WebSocketLogs::log_websocket_message_send_failure(lastErrorStringValue, "trading_system.log");
                    return false;
                } else if (bytesSent == 0) {
                    lastErrorStringValue = "write() returned 0 (connection closed?)";
                    WebSocketLogs::log_websocket_message_send_failure(lastErrorStringValue, "trading_system.log");
                    return false;
                }
            }
            
            totalBytesSent += bytesSent;
        }
        
        if (totalBytesSent < static_cast<int>(totalBytesToSend)) {
            lastErrorStringValue = "Failed to send complete WebSocket frame: sent " + std::to_string(totalBytesSent) + " / " + std::to_string(totalBytesToSend);
            WebSocketLogs::log_websocket_message_send_failure(lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        // SEND_MESSAGE_SUCCESS log removed to reduce log noise
        
        return true;
        
    } catch (const std::exception& sendExceptionError) {
        lastErrorStringValue = std::string("Send message exception: ") + sendExceptionError.what();
        WebSocketLogs::log_websocket_message_send_failure(lastErrorStringValue, "trading_system.log");
        return false;
    } catch (...) {
        lastErrorStringValue = "Unknown send message error";
        WebSocketLogs::log_websocket_message_send_failure(lastErrorStringValue, "trading_system.log");
        return false;
    }
}

void WebSocketClient::startReceiveLoop() {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    
    if (shouldReceiveLoopContinue.load()) {
        try {
            WebSocketLogs::log_websocket_receive_loop_table(
                "RECEIVE_LOOP",
                "Receive loop already running, skipping start",
                "trading_system.log"
            );
        } catch (...) {
            // Logging failed, continue
        }
        return;
    }
    
    try {
        AlpacaTrader::Logging::LoggingContext* parentLoggingContextForThread = nullptr;
        try {
            parentLoggingContextForThread = AlpacaTrader::Logging::get_logging_context();
        } catch (...) {
            parentLoggingContextForThread = nullptr;
        }
        
        parentLoggingContextPointer = parentLoggingContextForThread;
    } catch (...) {
        parentLoggingContextPointer = nullptr;
    }
    
    // Receive loop startup logs removed to reduce log noise
    shouldReceiveLoopContinue.store(true);
    receiveLoopThread = std::thread(&WebSocketClient::receiveLoopWorker, this);
}

void WebSocketClient::stopReceiveLoop() {
    shouldReceiveLoopContinue.store(false);
    
    if (receiveLoopThread.joinable()) {
        receiveLoopThread.join();
    }
    
    // Don't call cleanupConnection here - let disconnect() handle it
    // This prevents double-free when stopReceiveLoop() is called before disconnect()
    // The connection will be cleaned up when disconnect() is called explicitly
    // or when the destructor calls disconnect()
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    connectedFlag.store(false);
}

std::string WebSocketClient::getLastError() const {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    return lastErrorStringValue;
}

bool WebSocketClient::establishTcpConnection() {
    try {
        std::string hostnameString = extractHostname(websocketUrlStringValue);
        std::string portString = extractPort(websocketUrlStringValue);
        
        if (hostnameString.empty() || portString.empty()) {
            lastErrorStringValue = "Failed to extract hostname or port from URL";
            return false;
        }
        
        struct addrinfo hintsStruct;
        struct addrinfo* resultsPointer = nullptr;
        
        std::memset(&hintsStruct, 0, sizeof(hintsStruct));
        hintsStruct.ai_family = AF_UNSPEC;
        hintsStruct.ai_socktype = SOCK_STREAM;
        
        int getAddrInfoResult = getaddrinfo(hostnameString.c_str(), portString.c_str(), &hintsStruct, &resultsPointer);
        if (getAddrInfoResult != 0) {
            lastErrorStringValue = std::string("getaddrinfo failed: ") + gai_strerror(getAddrInfoResult);
            return false;
        }
        
        socketFileDescriptor = -1;
        for (struct addrinfo* currentAddressPointer = resultsPointer; currentAddressPointer != nullptr; currentAddressPointer = currentAddressPointer->ai_next) {
            socketFileDescriptor = socket(currentAddressPointer->ai_family, currentAddressPointer->ai_socktype, currentAddressPointer->ai_protocol);
            if (socketFileDescriptor < 0) {
                continue;
            }
            
            if (::connect(socketFileDescriptor, currentAddressPointer->ai_addr, static_cast<socklen_t>(currentAddressPointer->ai_addrlen)) == 0) {
                break;
            }
            
            ::close(socketFileDescriptor);
            socketFileDescriptor = -1;
        }
        
        freeaddrinfo(resultsPointer);
        
        if (socketFileDescriptor < 0) {
            lastErrorStringValue = "Failed to establish TCP connection";
            return false;
        }
        
        int flagsValue = fcntl(socketFileDescriptor, F_GETFL, 0);
        fcntl(socketFileDescriptor, F_SETFL, flagsValue | O_NONBLOCK);
        
        return true;
        
    } catch (const std::exception& tcpExceptionError) {
        lastErrorStringValue = std::string("TCP connection exception: ") + tcpExceptionError.what();
        return false;
    } catch (...) {
        lastErrorStringValue = "Unknown TCP connection error";
        return false;
    }
}

bool WebSocketClient::performSslHandshake() {
    try {
        const SSL_METHOD* methodPointer = TLS_client_method();
        sslContextPointer = SSL_CTX_new(methodPointer);
        
        if (!sslContextPointer) {
            unsigned long sslErrorCode = ERR_get_error();
            char errorBuffer[512];
            ERR_error_string_n(sslErrorCode, errorBuffer, sizeof(errorBuffer));
            lastErrorStringValue = std::string("Failed to create SSL context: ") + errorBuffer;
            WebSocketLogs::log_websocket_ssl_error(lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        SSL_CTX_set_options(static_cast<SSL_CTX*>(sslContextPointer), SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
        SSL_CTX_set_min_proto_version(static_cast<SSL_CTX*>(sslContextPointer), TLS1_2_VERSION);
        
        sslConnectionPointer = SSL_new(static_cast<SSL_CTX*>(sslContextPointer));
        if (!sslConnectionPointer) {
            unsigned long sslErrorCode = ERR_get_error();
            char errorBuffer[512];
            ERR_error_string_n(sslErrorCode, errorBuffer, sizeof(errorBuffer));
            lastErrorStringValue = std::string("Failed to create SSL connection: ") + errorBuffer;
            SSL_CTX_free(static_cast<SSL_CTX*>(sslContextPointer));
            sslContextPointer = nullptr;
            WebSocketLogs::log_websocket_ssl_error(lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        SSL_set_fd(static_cast<SSL*>(sslConnectionPointer), socketFileDescriptor);
        
        std::string hostnameString = extractHostname(websocketUrlStringValue);
        if (!hostnameString.empty()) {
            SSL_set_tlsext_host_name(static_cast<SSL*>(sslConnectionPointer), hostnameString.c_str());
        }
        
        SSL* sslConnectionPointerTyped = static_cast<SSL*>(sslConnectionPointer);
        
        int sslConnectResult = SSL_connect(sslConnectionPointerTyped);
        
        while (sslConnectResult <= 0) {
            int sslErrorCode = SSL_get_error(sslConnectionPointerTyped, sslConnectResult);
            
            if (sslErrorCode == SSL_ERROR_WANT_READ || sslErrorCode == SSL_ERROR_WANT_WRITE) {
                fd_set readFileDescriptorSet;
                fd_set writeFileDescriptorSet;
                struct timeval timeoutStruct;
                
                FD_ZERO(&readFileDescriptorSet);
                FD_ZERO(&writeFileDescriptorSet);
                
                if (sslErrorCode == SSL_ERROR_WANT_READ) {
                    FD_SET(socketFileDescriptor, &readFileDescriptorSet);
                } else {
                    FD_SET(socketFileDescriptor, &writeFileDescriptorSet);
                }
                
                timeoutStruct.tv_sec = 10;
                timeoutStruct.tv_usec = 0;
                
                int selectResult = select(socketFileDescriptor + 1, &readFileDescriptorSet, &writeFileDescriptorSet, nullptr, &timeoutStruct);
                if (selectResult <= 0) {
                    unsigned long opensslErrorCode = ERR_get_error();
                    char errorBuffer[512];
                    if (opensslErrorCode != 0) {
                        ERR_error_string_n(opensslErrorCode, errorBuffer, sizeof(errorBuffer));
                        lastErrorStringValue = std::string("SSL handshake timeout/error: ") + errorBuffer;
                    } else {
                        lastErrorStringValue = "SSL handshake timeout: select() returned " + std::to_string(selectResult);
                    }
                    SSL_free(sslConnectionPointerTyped);
                    SSL_CTX_free(static_cast<SSL_CTX*>(sslContextPointer));
                    sslConnectionPointer = nullptr;
                    sslContextPointer = nullptr;
                    WebSocketLogs::log_websocket_ssl_error(lastErrorStringValue, "trading_system.log");
                    return false;
                }
                
                sslConnectResult = SSL_connect(sslConnectionPointerTyped);
            } else {
                unsigned long opensslErrorCode = ERR_get_error();
                char errorBuffer[512];
                if (opensslErrorCode != 0) {
                    ERR_error_string_n(opensslErrorCode, errorBuffer, sizeof(errorBuffer));
                    lastErrorStringValue = std::string("SSL handshake failed: ") + errorBuffer + " (SSL_get_error: " + std::to_string(sslErrorCode) + ")";
                } else {
                    lastErrorStringValue = std::string("SSL handshake failed: SSL_get_error=") + std::to_string(sslErrorCode);
                }
                SSL_free(sslConnectionPointerTyped);
                SSL_CTX_free(static_cast<SSL_CTX*>(sslContextPointer));
                sslConnectionPointer = nullptr;
                sslContextPointer = nullptr;
                WebSocketLogs::log_websocket_ssl_error(lastErrorStringValue, "trading_system.log");
                return false;
            }
        }
        
        return true;
        
    } catch (const std::exception& sslExceptionError) {
        lastErrorStringValue = std::string("SSL handshake exception: ") + sslExceptionError.what();
        if (sslConnectionPointer) {
            SSL_free(static_cast<SSL*>(sslConnectionPointer));
            sslConnectionPointer = nullptr;
        }
        if (sslContextPointer) {
            SSL_CTX_free(static_cast<SSL_CTX*>(sslContextPointer));
            sslContextPointer = nullptr;
        }
        WebSocketLogs::log_websocket_ssl_error(lastErrorStringValue, "trading_system.log");
        return false;
    } catch (...) {
        lastErrorStringValue = "Unknown SSL handshake error";
        if (sslConnectionPointer) {
            SSL_free(static_cast<SSL*>(sslConnectionPointer));
            sslConnectionPointer = nullptr;
        }
        if (sslContextPointer) {
            SSL_CTX_free(static_cast<SSL_CTX*>(sslContextPointer));
            sslContextPointer = nullptr;
        }
        WebSocketLogs::log_websocket_ssl_error(lastErrorStringValue, "trading_system.log");
        return false;
    }
}

bool WebSocketClient::performWebSocketHandshake() {
    try {
        std::vector<unsigned char> webSocketKeyBytes = generateRandomBytes(16);
        std::string webSocketKeyString = base64Encode(std::string(webSocketKeyBytes.begin(), webSocketKeyBytes.end()));
        
        std::string hostnameString = extractHostname(websocketUrlStringValue);
        std::string pathString = extractPath(websocketUrlStringValue);
        if (pathString.empty()) {
            pathString = "/";
        }
        
        std::stringstream handshakeStream;
        handshakeStream << "GET " << pathString << " HTTP/1.1\r\n";
        handshakeStream << "Host: " << hostnameString << "\r\n";
        handshakeStream << "Upgrade: websocket\r\n";
        handshakeStream << "Connection: Upgrade\r\n";
        handshakeStream << "Sec-WebSocket-Key: " << webSocketKeyString << "\r\n";
        handshakeStream << "Sec-WebSocket-Version: 13\r\n";
        handshakeStream << "\r\n";
        
        std::string handshakeRequestString = handshakeStream.str();
        
        int bytesSent = 0;
        if (sslConnectionPointer) {
            SSL* sslConnectionPointerTyped = static_cast<SSL*>(sslConnectionPointer);
            bytesSent = SSL_write(sslConnectionPointerTyped, handshakeRequestString.c_str(), static_cast<int>(handshakeRequestString.length()));
            
            if (bytesSent <= 0) {
                int sslErrorCode = SSL_get_error(sslConnectionPointerTyped, bytesSent);
                unsigned long opensslErrorCode = ERR_get_error();
                char errorBuffer[512];
                if (opensslErrorCode != 0) {
                    ERR_error_string_n(opensslErrorCode, errorBuffer, sizeof(errorBuffer));
                    lastErrorStringValue = std::string("SSL write failed: ") + errorBuffer + " (SSL_get_error: " + std::to_string(sslErrorCode) + ")";
                } else {
                    lastErrorStringValue = std::string("SSL write failed: SSL_get_error=") + std::to_string(sslErrorCode) + ", bytesSent=" + std::to_string(bytesSent);
                }
                WebSocketLogs::log_websocket_handshake_table(handshakeRequestString, "", bytesSent, 0, "trading_system.log");
                return false;
            }
        } else {
            bytesSent = ::write(socketFileDescriptor, handshakeRequestString.c_str(), handshakeRequestString.length());
            if (bytesSent < 0) {
                lastErrorStringValue = std::string("write() failed: errno=") + std::to_string(errno) + " (" + strerror(errno) + ")";
                WebSocketLogs::log_websocket_handshake_table(handshakeRequestString, "", bytesSent, 0, "trading_system.log");
                return false;
            }
        }
        
        if (static_cast<size_t>(bytesSent) != handshakeRequestString.length()) {
            lastErrorStringValue = "Failed to send complete WebSocket handshake request";
            WebSocketLogs::log_websocket_handshake_table(handshakeRequestString, "", bytesSent, 0, "trading_system.log");
            return false;
        }
        
        std::string handshakeResponseString;
        char responseBuffer[4096];
        
        bool headersComplete = false;
        int totalBytesReceivedValue = 0;
        int readAttemptsCount = 0;
        const int maxReadAttemptsValue = 10;
        
        while (!headersComplete && handshakeResponseString.length() < 4096 && readAttemptsCount < maxReadAttemptsValue) {
            readAttemptsCount++;
            
            int bytesReceived = 0;
            if (sslConnectionPointer) {
                SSL* sslConnectionPointerTyped = static_cast<SSL*>(sslConnectionPointer);
                bytesReceived = SSL_read(sslConnectionPointerTyped, responseBuffer, sizeof(responseBuffer) - 1);
                
                if (bytesReceived <= 0) {
                    int sslErrorCode = SSL_get_error(sslConnectionPointerTyped, bytesReceived);
                    
                    if (sslErrorCode == SSL_ERROR_WANT_READ || sslErrorCode == SSL_ERROR_WANT_WRITE) {
                        fd_set readFileDescriptorSet;
                        fd_set writeFileDescriptorSet;
                        struct timeval timeoutStruct;
                        
                        FD_ZERO(&readFileDescriptorSet);
                        FD_ZERO(&writeFileDescriptorSet);
                        
                        if (sslErrorCode == SSL_ERROR_WANT_READ) {
                            FD_SET(socketFileDescriptor, &readFileDescriptorSet);
                        } else {
                            FD_SET(socketFileDescriptor, &writeFileDescriptorSet);
                        }
                        
                        timeoutStruct.tv_sec = 5;
                        timeoutStruct.tv_usec = 0;
                        
                        int selectResult = select(socketFileDescriptor + 1, &readFileDescriptorSet, &writeFileDescriptorSet, nullptr, &timeoutStruct);
                        if (selectResult <= 0) {
                            lastErrorStringValue = std::string("SSL read timeout/error: select() returned ") + std::to_string(selectResult) + ", SSL_get_error=" + std::to_string(sslErrorCode);
                            WebSocketLogs::log_websocket_ssl_read_error(sslErrorCode, "trading_system.log");
                            WebSocketLogs::log_websocket_handshake_error(lastErrorStringValue, "trading_system.log");
                            if (totalBytesReceivedValue > 0) {
                                WebSocketLogs::log_websocket_handshake_table(handshakeRequestString, handshakeResponseString, bytesSent, totalBytesReceivedValue, "trading_system.log");
                            }
                            return false;
                        }
                        continue;
                    } else {
                        unsigned long opensslErrorCode = ERR_get_error();
                        char errorBuffer[512];
                        if (opensslErrorCode != 0) {
                            ERR_error_string_n(opensslErrorCode, errorBuffer, sizeof(errorBuffer));
                            lastErrorStringValue = std::string("SSL read failed: ") + errorBuffer + " (SSL_get_error: " + std::to_string(sslErrorCode) + ")";
                        } else {
                            lastErrorStringValue = std::string("SSL read failed: SSL_get_error=") + std::to_string(sslErrorCode) + ", bytesReceived=" + std::to_string(bytesReceived);
                        }
                        WebSocketLogs::log_websocket_ssl_read_error(sslErrorCode, "trading_system.log");
                        WebSocketLogs::log_websocket_handshake_error(lastErrorStringValue, "trading_system.log");
                        if (totalBytesReceivedValue > 0) {
                            WebSocketLogs::log_websocket_handshake_table(handshakeRequestString, handshakeResponseString, bytesSent, totalBytesReceivedValue, "trading_system.log");
                        }
                        return false;
                    }
                }
            } else {
                bytesReceived = ::read(socketFileDescriptor, responseBuffer, sizeof(responseBuffer) - 1);
                
                if (bytesReceived < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    lastErrorStringValue = std::string("read() failed: errno=") + std::to_string(errno) + " (" + strerror(errno) + ")";
                    WebSocketLogs::log_websocket_handshake_error(lastErrorStringValue, "trading_system.log");
                    if (totalBytesReceivedValue > 0) {
                        WebSocketLogs::log_websocket_handshake_response(handshakeResponseString, "trading_system.log");
                    }
                    return false;
                } else if (bytesReceived == 0) {
                    lastErrorStringValue = "Connection closed by server during handshake";
                    WebSocketLogs::log_websocket_handshake_error(lastErrorStringValue, "trading_system.log");
                    if (totalBytesReceivedValue > 0) {
                        WebSocketLogs::log_websocket_handshake_response(handshakeResponseString, "trading_system.log");
                    }
                    return false;
                }
            }
            
            responseBuffer[bytesReceived] = '\0';
            handshakeResponseString.append(responseBuffer, bytesReceived);
            totalBytesReceivedValue += bytesReceived;
            
            WebSocketLogs::log_websocket_handshake_bytes_received(bytesReceived, "trading_system.log");
            
            if (handshakeResponseString.find("\r\n\r\n") != std::string::npos) {
                headersComplete = true;
            }
        }
        
        if (!headersComplete) {
            lastErrorStringValue = std::string("WebSocket handshake response incomplete after ") + std::to_string(readAttemptsCount) + " read attempts, " + std::to_string(totalBytesReceivedValue) + " bytes received";
            WebSocketLogs::log_websocket_handshake_table(handshakeRequestString, handshakeResponseString, bytesSent, totalBytesReceivedValue, "trading_system.log");
            return false;
        }
        
        WebSocketLogs::log_websocket_handshake_table(handshakeRequestString, handshakeResponseString, bytesSent, totalBytesReceivedValue, "trading_system.log");
        
        if (handshakeResponseString.find("HTTP/1.1 101") == std::string::npos &&
            handshakeResponseString.find("HTTP/1.0 101") == std::string::npos) {
            size_t firstLineEndPos = handshakeResponseString.find("\r\n");
            std::string firstLine = firstLineEndPos != std::string::npos ? handshakeResponseString.substr(0, firstLineEndPos) : handshakeResponseString.substr(0, 100);
            lastErrorStringValue = std::string("WebSocket handshake failed - invalid response code. First line: ") + firstLine;
            return false;
        }
        
        std::string expectedAcceptString = webSocketKeyString + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string expectedAcceptHashString = sha1Hash(expectedAcceptString);
        std::string expectedAcceptBase64String = base64Encode(expectedAcceptHashString);
        
        if (handshakeResponseString.find(expectedAcceptBase64String) == std::string::npos) {
            size_t acceptKeyStartPos = handshakeResponseString.find("Sec-WebSocket-Accept:");
            std::string acceptKeyLine = "not found";
            if (acceptKeyStartPos != std::string::npos) {
                size_t acceptKeyEndPos = handshakeResponseString.find("\r\n", acceptKeyStartPos);
                acceptKeyLine = handshakeResponseString.substr(acceptKeyStartPos, acceptKeyEndPos != std::string::npos ? acceptKeyEndPos - acceptKeyStartPos : 200);
            }
            lastErrorStringValue = std::string("WebSocket handshake failed - invalid accept key. Expected: ") + expectedAcceptBase64String + ", Found in response: " + acceptKeyLine;
            WebSocketLogs::log_websocket_handshake_error(lastErrorStringValue, "trading_system.log");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& handshakeExceptionError) {
        lastErrorStringValue = std::string("WebSocket handshake exception: ") + handshakeExceptionError.what();
        WebSocketLogs::log_websocket_handshake_error(lastErrorStringValue, "trading_system.log");
        return false;
    } catch (...) {
        lastErrorStringValue = "Unknown WebSocket handshake error";
        WebSocketLogs::log_websocket_handshake_error(lastErrorStringValue, "trading_system.log");
        return false;
    }
}

void WebSocketClient::receiveLoopWorker() {
    int loopIterationCount = 0;
    
    try {
        AlpacaTrader::Logging::LoggingContext* parentLoggingContextValue = parentLoggingContextPointer;
        if (parentLoggingContextValue) {
            try {
                AlpacaTrader::Logging::set_logging_context(*parentLoggingContextValue);
                AlpacaTrader::Logging::set_log_thread_tag("WS    ");
                // Receive loop started log removed to reduce log noise
            } catch (const std::exception& setContextExceptionError) {
                std::cerr << "WARNING: WebSocket receive loop could not set logging context: " << setContextExceptionError.what() << " - continuing without logging context" << std::endl;
            } catch (...) {
                std::cerr << "WARNING: WebSocket receive loop could not set logging context - continuing without logging context" << std::endl;
            }
        } else {
            std::cerr << "WebSocket receive loop started (no logging context available from parent thread)" << std::endl;
        }
    } catch (...) {
        std::cerr << "WebSocket receive loop started (logging initialization failed)" << std::endl;
    }
    
    while (shouldReceiveLoopContinue.load()) {
        try {
            // CRITICAL: Ensure logging context is maintained throughout receive loop
            // Re-initialize if lost (can happen during reconnections or thread state changes)
            try {
                AlpacaTrader::Logging::LoggingContext* parentLoggingContextValue = parentLoggingContextPointer;
                if (parentLoggingContextValue) {
                    // Verify context is still set, re-set if needed
                    try {
                        AlpacaTrader::Logging::get_logging_context();
                    } catch (...) {
                        // Context lost, re-initialize
                        AlpacaTrader::Logging::set_logging_context(*parentLoggingContextValue);
                        AlpacaTrader::Logging::set_log_thread_tag("WS    ");
                    }
                }
            } catch (...) {
                // If logging context cannot be maintained, continue without it
            }
            
            if (!connectedFlag.load()) {
                if (shouldReceiveLoopContinue.load()) {
                    // Cleanup old connection before reconnecting to prevent connection limit issues
                    {
                        std::lock_guard<std::mutex> cleanupGuard(clientStateMutex);
                        try {
                            cleanupConnection();
                        } catch (...) {
                            // Ignore cleanup errors, continue with reconnection
                        }
                        connectedFlag.store(false);
                    }
                    
                    // Wait longer before reconnection to allow server-side cleanup
                    // This prevents hitting max_connections limit
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                    try {
                        WebSocketLogs::log_websocket_reconnection_attempt("trading_system.log");
                    } catch (...) {
                        std::cerr << "WebSocket attempting reconnection..." << std::endl;
                    }
                    
                    if (connect(websocketUrlStringValue)) {
                        if (!apiKeyStringValue.empty()) {
                            authenticate(apiKeyStringValue);
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            if (!subscriptionParamsStringValue.empty()) {
                                subscribe(subscriptionParamsStringValue);
                            }
                        }
                        try {
                            WebSocketLogs::log_websocket_reconnection_success("trading_system.log");
                        } catch (...) {
                            std::cerr << "WebSocket reconnection successful" << std::endl;
                        }
                    } else {
                        try {
                            WebSocketLogs::log_websocket_reconnection_failure(lastErrorStringValue, "trading_system.log");
                        } catch (...) {
                            std::cerr << "WebSocket reconnection failed: " << lastErrorStringValue << std::endl;
                        }
                        // Longer delay on failure to prevent rapid retry loops
                        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                        continue;
                    }
                } else {
                    break;
                }
            }
            
            bool messageProcessed = receiveAndProcessMessage();
            
            if (!messageProcessed && connectedFlag.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            loopIterationCount++;
            
            // Compliance: Removed unused variables per "No unused parameters" rule
            // Receive loop status logs removed to reduce log noise
            
        } catch (const std::exception& receiveLoopExceptionError) {
            try {
                WebSocketLogs::log_websocket_receive_error(receiveLoopExceptionError.what(), "trading_system.log");
            } catch (...) {
                std::cerr << "WebSocket receive loop error: " << receiveLoopExceptionError.what() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            if (shouldReceiveLoopContinue.load()) {
                // Cleanup old connection before reconnecting to prevent connection limit issues
                {
                    std::lock_guard<std::mutex> cleanupGuard(clientStateMutex);
                    try {
                        cleanupConnection();
                    } catch (...) {
                        // Ignore cleanup errors, continue with reconnection
                    }
                    connectedFlag.store(false);
                }
                
                // Wait longer before reconnection to allow server-side cleanup
                // This prevents hitting max_connections limit
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                
                try {
                    WebSocketLogs::log_websocket_reconnection_attempt("trading_system.log");
                } catch (...) {
                    std::cerr << "WebSocket attempting reconnection..." << std::endl;
                }
                
                if (connect(websocketUrlStringValue)) {
                    if (!apiKeyStringValue.empty()) {
                        authenticate(apiKeyStringValue);
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        if (!subscriptionParamsStringValue.empty()) {
                            subscribe(subscriptionParamsStringValue);
                        }
                    }
                    try {
                        WebSocketLogs::log_websocket_reconnection_success("trading_system.log");
                    } catch (...) {
                        std::cerr << "WebSocket reconnection successful" << std::endl;
                    }
                } else {
                    try {
                        WebSocketLogs::log_websocket_reconnection_failure(lastErrorStringValue, "trading_system.log");
                    } catch (...) {
                        std::cerr << "WebSocket reconnection failed: " << lastErrorStringValue << std::endl;
                    }
                    // Longer delay on failure to prevent rapid retry loops
                    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                }
            }
        } catch (...) {
            try {
                WebSocketLogs::log_websocket_receive_error("Unknown receive loop error", "trading_system.log");
            } catch (...) {
                std::cerr << "WebSocket receive loop unknown error" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    try {
        WebSocketLogs::log_websocket_receive_loop_table("RECEIVE_LOOP", "WebSocket receive loop stopped - total iterations: " + std::to_string(loopIterationCount), "trading_system.log");
    } catch (...) {
        std::cerr << "WebSocket receive loop stopped - total iterations: " << loopIterationCount << std::endl;
    }
}

bool WebSocketClient::receiveAndProcessMessage() {
    try {
        unsigned char frameHeaderBytes[14];
        int headerBytesRead = 0;
        
        if (sslConnectionPointer) {
            SSL* sslConnectionPointerTyped = static_cast<SSL*>(sslConnectionPointer);
            headerBytesRead = SSL_read(sslConnectionPointerTyped, frameHeaderBytes, 2);
            
            if (headerBytesRead <= 0) {
                int sslErrorCode = SSL_get_error(sslConnectionPointerTyped, headerBytesRead);
                if (sslErrorCode == SSL_ERROR_WANT_READ || sslErrorCode == SSL_ERROR_WANT_WRITE) {
                    return false;
                } else if (sslErrorCode == SSL_ERROR_ZERO_RETURN) {
                    try {
                        WebSocketLogs::log_websocket_message_details("SSL_CLOSED", 
                            "SSL connection closed by server",
                            "trading_system.log");
                    } catch (...) {
                        // Logging failed, continue
                    }
                    connectedFlag.store(false);
                    return false;
                } else {
                    unsigned long opensslErrorCode = ERR_get_error();
                    char errorBuffer[512];
                    try {
                        if (opensslErrorCode != 0) {
                            ERR_error_string_n(opensslErrorCode, errorBuffer, sizeof(errorBuffer));
                            WebSocketLogs::log_websocket_message_details("SSL_READ_ERROR", 
                                std::string("SSL read error: ") + errorBuffer + " (SSL_get_error: " + std::to_string(sslErrorCode) + ")",
                                "trading_system.log");
                        } else {
                            WebSocketLogs::log_websocket_message_details("SSL_READ_ERROR", 
                                "SSL read error: SSL_get_error=" + std::to_string(sslErrorCode) + ", bytesRead=" + std::to_string(headerBytesRead),
                                "trading_system.log");
                        }
                    } catch (...) {
                        // Logging failed, continue
                    }
                    return false;
                }
            }
        } else {
            headerBytesRead = ::read(socketFileDescriptor, frameHeaderBytes, 2);
            
            if (headerBytesRead <= 0) {
                if (headerBytesRead == 0) {
                    try {
                        WebSocketLogs::log_websocket_message_details("SOCKET_CLOSED", 
                            "Socket connection closed by server",
                            "trading_system.log");
                    } catch (...) {
                        // Logging failed, continue
                    }
                    connectedFlag.store(false);
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return false;
                } else {
                    try {
                        WebSocketLogs::log_websocket_message_details("READ_ERROR", 
                            "read() error: errno=" + std::to_string(errno) + " (" + strerror(errno) + ")",
                            "trading_system.log");
                    } catch (...) {
                        // Logging failed, continue
                    }
                }
                return false;
            }
        }
        
        if (headerBytesRead < 2) {
            try {
                WebSocketLogs::log_websocket_frame_parse_error("Incomplete frame header", "trading_system.log");
            } catch (...) {
                // Logging failed, continue
            }
            return false;
        }
        
        unsigned char opcodeValue = frameHeaderBytes[0] & 0x0F;
        bool maskedFlag = (frameHeaderBytes[1] & 0x80) != 0;
        unsigned long long payloadLengthValue = frameHeaderBytes[1] & 0x7F;
        
        if (opcodeValue == 0x8) {
            std::string closeReasonString = "Unknown reason";
            try {
                if (payloadLengthValue >= 2) {
                    std::vector<unsigned char> closePayloadBytes(payloadLengthValue);
                    int closeBytesRead = 0;
                    if (sslConnectionPointer) {
                        closeBytesRead = SSL_read(static_cast<SSL*>(sslConnectionPointer), closePayloadBytes.data(), static_cast<int>(payloadLengthValue));
                    } else {
                        closeBytesRead = ::read(socketFileDescriptor, closePayloadBytes.data(), static_cast<int>(payloadLengthValue));
                    }
                    
                    if (closeBytesRead >= 2) {
                        unsigned short closeCodeValue = (static_cast<unsigned short>(closePayloadBytes[0]) << 8) | closePayloadBytes[1];
                        closeReasonString = "Close code: " + std::to_string(closeCodeValue);
                        
                        if (closeBytesRead > 2) {
                            std::string closeMessageString(reinterpret_cast<const char*>(closePayloadBytes.data() + 2), closeBytesRead - 2);
                            if (!closeMessageString.empty()) {
                                closeReasonString += ", Message: " + closeMessageString;
                            }
                        }
                    }
                }
                
                WebSocketLogs::log_websocket_message_details("CLOSE_FRAME", 
                    "Received WebSocket close frame - " + closeReasonString,
                    "trading_system.log");
            } catch (...) {
                try {
                    WebSocketLogs::log_websocket_message_details("CLOSE_FRAME", 
                        "Received WebSocket close frame (error reading close reason)",
                        "trading_system.log");
                } catch (...) {
                    // Logging failed, continue
                }
            }
            // Cleanup connection resources before marking as disconnected
            // This prevents connection accumulation when server closes connection
            {
                std::lock_guard<std::mutex> cleanupGuard(clientStateMutex);
                try {
                    cleanupConnection();
                } catch (...) {
                    // Ignore cleanup errors, continue
                }
            }
            connectedFlag.store(false);
            return false;
        }
        
        if (payloadLengthValue == 126) {
            unsigned char lengthBytes[2];
            int lengthBytesRead = 0;
            if (sslConnectionPointer) {
                lengthBytesRead = SSL_read(static_cast<SSL*>(sslConnectionPointer), lengthBytes, 2);
            } else {
                lengthBytesRead = ::read(socketFileDescriptor, lengthBytes, 2);
            }
            
            if (lengthBytesRead != 2) {
                try {
                    WebSocketLogs::log_websocket_frame_parse_error("Failed to read extended length", "trading_system.log");
                } catch (...) {
                    // Logging failed, continue
                }
                return false;
            }
            
            payloadLengthValue = (static_cast<unsigned long long>(lengthBytes[0]) << 8) | lengthBytes[1];
        } else if (payloadLengthValue == 127) {
            unsigned char lengthBytes[8];
            int lengthBytesRead = 0;
            if (sslConnectionPointer) {
                lengthBytesRead = SSL_read(static_cast<SSL*>(sslConnectionPointer), lengthBytes, 8);
            } else {
                lengthBytesRead = ::read(socketFileDescriptor, lengthBytes, 8);
            }
            
            if (lengthBytesRead != 8) {
                try {
                    WebSocketLogs::log_websocket_frame_parse_error("Failed to read extended length (64-bit)", "trading_system.log");
                } catch (...) {
                    // Logging failed, continue
                }
                return false;
            }
            
            payloadLengthValue = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLengthValue = (payloadLengthValue << 8) | lengthBytes[i];
            }
        }
        
        unsigned char maskingKeyBytes[4];
        if (maskedFlag) {
            int maskingKeyBytesRead = 0;
            if (sslConnectionPointer) {
                maskingKeyBytesRead = SSL_read(static_cast<SSL*>(sslConnectionPointer), maskingKeyBytes, 4);
            } else {
                maskingKeyBytesRead = ::read(socketFileDescriptor, maskingKeyBytes, 4);
            }
            
            if (maskingKeyBytesRead != 4) {
                try {
                    WebSocketLogs::log_websocket_frame_parse_error("Failed to read masking key", "trading_system.log");
                } catch (...) {
                    // Logging failed, continue
                }
                return false;
            }
        }
        
        std::vector<unsigned char> payloadBuffer(payloadLengthValue);
        size_t totalBytesRead = 0;
        
        while (totalBytesRead < payloadLengthValue) {
            int bytesRead = 0;
            if (sslConnectionPointer) {
                bytesRead = SSL_read(static_cast<SSL*>(sslConnectionPointer), 
                                   payloadBuffer.data() + totalBytesRead, 
                                   static_cast<int>(payloadLengthValue - totalBytesRead));
            } else {
                bytesRead = ::read(socketFileDescriptor, 
                                 payloadBuffer.data() + totalBytesRead, 
                                 static_cast<int>(payloadLengthValue - totalBytesRead));
            }
            
            if (bytesRead <= 0) {
                try {
                    WebSocketLogs::log_websocket_frame_parse_error("Failed to read payload", "trading_system.log");
                } catch (...) {
                    // Logging failed, continue
                }
                return false;
            }
            
            totalBytesRead += bytesRead;
        }
        
        if (maskedFlag) {
            for (size_t i = 0; i < payloadLengthValue; ++i) {
                payloadBuffer[i] ^= maskingKeyBytes[i % 4];
            }
        }
        
        std::string messageString(payloadBuffer.begin(), payloadBuffer.end());
        
        if (!messageString.empty()) {
            if (messageCallbackFunction) {
                bool callbackResult = false;
                try {
                    callbackResult = messageCallbackFunction(messageString);
                } catch (const std::exception& callbackExceptionError) {
                    try {
                        WebSocketLogs::log_websocket_message_details("CALLBACK_EXCEPTION", 
                            "Exception in message callback: " + std::string(callbackExceptionError.what()),
                            "trading_system.log");
                    } catch (...) {
                        // If logging context not available, fall back to stderr
                        std::cerr << "WS CALLBACK EXCEPTION: " << callbackExceptionError.what() << std::endl;
                    }
                    callbackResult = false;
                } catch (...) {
                    try {
                        WebSocketLogs::log_websocket_message_details("CALLBACK_UNKNOWN_EXCEPTION", 
                            "Unknown exception in message callback",
                            "trading_system.log");
                    } catch (...) {
                        // If logging context not available, fall back to stderr
                        std::cerr << "WS CALLBACK UNKNOWN EXCEPTION" << std::endl;
                    }
                    callbackResult = false;
                }
                
                if (!callbackResult) {
                    try {
                        WebSocketLogs::log_websocket_message_details("CALLBACK_FAILED", 
                            "Message callback returned false for message: " + messageString.substr(0, 100),
                            "trading_system.log");
                    } catch (...) {
                        // If logging context not available, fall back to stderr
                        std::cerr << "WS CALLBACK FAILED for message: " << messageString.substr(0, 100) << std::endl;
                    }
                }
            } else {
                try {
                    WebSocketLogs::log_websocket_message_details("NO_CALLBACK", 
                        "Message received but no callback function set. Message: " + messageString.substr(0, 100),
                        "trading_system.log");
                } catch (...) {
                    // If logging context not available, fall back to stderr
                    std::cerr << "WS NO CALLBACK set, message received: " << messageString.substr(0, 100) << std::endl;
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& receiveExceptionError) {
        try {
            WebSocketLogs::log_websocket_receive_error(receiveExceptionError.what(), "trading_system.log");
        } catch (...) {
            // Logging failed, continue
        }
        return false;
    } catch (...) {
        try {
            WebSocketLogs::log_websocket_receive_error("Unknown receive error", "trading_system.log");
        } catch (...) {
            // Logging failed, continue
        }
        return false;
    }
}

std::string WebSocketClient::base64Encode(const std::string& inputDataString) {
    static const std::string base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encodedString;
    int i = 0;
    int j = 0;
    unsigned char charArray3[3];
    unsigned char charArray4[4];
    
    size_t inputLength = inputDataString.length();
    const unsigned char* inputBytes = reinterpret_cast<const unsigned char*>(inputDataString.c_str());
    
    while (inputLength--) {
        charArray3[i++] = *(inputBytes++);
        if (i == 3) {
            charArray4[0] = (charArray3[0] & 0xfc) >> 2;
            charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
            charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
            charArray4[3] = charArray3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) {
                encodedString += base64Chars[charArray4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++) {
            charArray3[j] = '\0';
        }
        
        charArray4[0] = (charArray3[0] & 0xfc) >> 2;
        charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
        charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
        charArray4[3] = charArray3[2] & 0x3f;
        
        for (j = 0; j < i + 1; j++) {
            encodedString += base64Chars[charArray4[j]];
        }
        
        while (i++ < 3) {
            encodedString += '=';
        }
    }
    
    return encodedString;
}

std::string WebSocketClient::sha1Hash(const std::string& inputDataString) {
    unsigned char hashBuffer[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(inputDataString.c_str()), inputDataString.length(), hashBuffer);
    
    return std::string(reinterpret_cast<char*>(hashBuffer), SHA_DIGEST_LENGTH);
}

std::vector<unsigned char> WebSocketClient::generateRandomBytes(size_t byteCount) {
    std::vector<unsigned char> randomBytes(byteCount);
    
    if (RAND_bytes(randomBytes.data(), static_cast<int>(byteCount)) != 1) {
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::uniform_int_distribution<unsigned char> distribution(0, 255);
        
        for (size_t i = 0; i < byteCount; ++i) {
            randomBytes[i] = distribution(generator);
        }
    }
    
    return randomBytes;
}

void WebSocketClient::cleanupConnection() {
    std::lock_guard<std::mutex> stateGuard(clientStateMutex);
    try {
        // Cleanup SSL connection - check if pointer is valid before freeing
        void* sslConnectionToFree = sslConnectionPointer;
        if (sslConnectionToFree) {
            sslConnectionPointer = nullptr; // Clear pointer first to prevent double-free
            try {
                SSL_shutdown(static_cast<SSL*>(sslConnectionToFree));
            } catch (...) {
                // Ignore shutdown errors, continue with free
            }
            try {
                SSL_free(static_cast<SSL*>(sslConnectionToFree));
            } catch (...) {
                // Ignore free errors
            }
        }

        // Cleanup SSL context - check if pointer is valid before freeing
        void* sslContextToFree = sslContextPointer;
        if (sslContextToFree) {
            sslContextPointer = nullptr; // Clear pointer first to prevent double-free
            try {
                SSL_CTX_free(static_cast<SSL_CTX*>(sslContextToFree));
            } catch (...) {
                // Ignore free errors
            }
        }

        // Cleanup socket - check if file descriptor is valid before closing
        int socketFileDescriptorToClose = socketFileDescriptor;
        if (socketFileDescriptorToClose >= 0) {
            socketFileDescriptor = -1; // Clear descriptor first to prevent double-close
            try {
                ::close(socketFileDescriptorToClose);
            } catch (...) {
                // Ignore close errors
            }
        }
    } catch (const std::exception& cleanupExceptionError) {
        lastErrorStringValue = std::string("Cleanup error: ") + cleanupExceptionError.what();
    } catch (...) {
        lastErrorStringValue = "Unknown cleanup error";
    }
}

bool WebSocketClient::validateUrl(const std::string& urlStringToValidate) const {
    return urlStringToValidate.find("wss://") == 0 || urlStringToValidate.find("ws://") == 0;
}

std::string WebSocketClient::extractHostname(const std::string& urlString) const {
    size_t protocolStart = urlString.find("://");
    if (protocolStart == std::string::npos) {
        return "";
    }
    
    size_t hostnameStart = protocolStart + 3;
    size_t hostnameEnd = urlString.find('/', hostnameStart);
    if (hostnameEnd == std::string::npos) {
        hostnameEnd = urlString.find(':', hostnameStart);
        if (hostnameEnd == std::string::npos) {
            hostnameEnd = urlString.length();
        }
    }
    
    size_t portStart = urlString.find(':', hostnameStart);
    if (portStart != std::string::npos && portStart < hostnameEnd) {
        return urlString.substr(hostnameStart, portStart - hostnameStart);
    }
    
    return urlString.substr(hostnameStart, hostnameEnd - hostnameStart);
}

std::string WebSocketClient::extractPort(const std::string& urlString) const {
    size_t protocolStart = urlString.find("://");
    if (protocolStart == std::string::npos) {
        return "";
    }
    
    size_t hostnameStart = protocolStart + 3;
    size_t portStart = urlString.find(':', hostnameStart);
    
    if (portStart != std::string::npos) {
        size_t portEnd = urlString.find('/', portStart);
        if (portEnd == std::string::npos) {
            portEnd = urlString.length();
        }
        return urlString.substr(portStart + 1, portEnd - portStart - 1);
    }
    
    if (urlString.find("wss://") == 0) {
        return "443";
    }
    
    return "80";
}

std::string WebSocketClient::extractPath(const std::string& urlString) const {
    size_t protocolStart = urlString.find("://");
    if (protocolStart == std::string::npos) {
        return "/";
    }
    
    size_t hostnameStart = protocolStart + 3;
    size_t pathStart = urlString.find('/', hostnameStart);
    
    if (pathStart == std::string::npos) {
        pathStart = urlString.find('?', hostnameStart);
        if (pathStart == std::string::npos) {
            return "/";
        }
    }
    
    size_t pathEnd = urlString.find('?', pathStart);
    if (pathEnd == std::string::npos) {
        pathEnd = urlString.length();
    }
    
    return urlString.substr(pathStart, pathEnd - pathStart);
}

} // namespace Polygon
} // namespace API
} // namespace AlpacaTrader

