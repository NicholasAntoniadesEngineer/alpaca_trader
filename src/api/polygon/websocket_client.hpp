#ifndef WEBSOCKET_CLIENT_HPP
#define WEBSOCKET_CLIENT_HPP

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace AlpacaTrader {
namespace Logging {
struct LoggingContext;
} // namespace Logging
namespace API {
namespace Polygon {

class WebSocketClient {
public:
    using MessageCallback = std::function<bool(const std::string& message)>;
    
    WebSocketClient();
    ~WebSocketClient();
    
    bool connect(const std::string& websocketUrlString);
    void disconnect();
    bool isConnected() const;
    
    bool authenticate(const std::string& apiKeyString);
    bool subscribe(const std::string& subscriptionParamsString);
    bool unsubscribe(const std::string& subscriptionParamsString);
    
    void setMessageCallback(MessageCallback callbackFunction);
    
    bool sendMessage(const std::string& messageContent);
    
    void startReceiveLoop();
    void stopReceiveLoop();
    
    std::string getLastError() const;

private:
    std::string websocketUrlStringValue;
    std::string apiKeyStringValue;
    std::string subscriptionParamsStringValue;
    MessageCallback messageCallbackFunction;
    
    Logging::LoggingContext* parentLoggingContextPointer;
    
    std::atomic<bool> connectedFlag;
    std::atomic<bool> shouldReceiveLoopContinue;
    
    mutable std::mutex clientStateMutex;
    std::thread receiveLoopThread;
    
    std::string lastErrorStringValue;
    
    int socketFileDescriptor;
    void* sslContextPointer;
    void* sslConnectionPointer;
    
    bool establishTcpConnection();
    bool performSslHandshake();
    bool performWebSocketHandshake();
    void receiveLoopWorker();
    bool receiveAndProcessMessage();
    std::string base64Encode(const std::string& inputDataString);
    std::string sha1Hash(const std::string& inputDataString);
    std::vector<unsigned char> generateRandomBytes(size_t byteCount);
    void cleanupConnection();
    
    bool validateUrl(const std::string& urlStringToValidate) const;
    std::string extractHostname(const std::string& urlString) const;
    std::string extractPort(const std::string& urlString) const;
    std::string extractPath(const std::string& urlString) const;
    
    bool sendMessageInternal(const std::string& messageContent);
};

} // namespace Polygon
} // namespace API
} // namespace AlpacaTrader

#endif // WEBSOCKET_CLIENT_HPP

