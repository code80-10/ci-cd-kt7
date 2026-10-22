#include <winsock2.h>
#include <ws2tcpip.h>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>

namespace {
constexpr unsigned short kPort = 8080;
constexpr std::size_t kMaxRequestSize = 1024;

struct WinsockGuard {
    bool ready = false;

    WinsockGuard() {
        WSADATA data{};
        ready = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }

    ~WinsockGuard() {
        if (ready) {
            WSACleanup();
        }
    }
};

bool sendAll(SOCKET socket, const std::string& message) {
    std::size_t sent = 0;
    while (sent < message.size()) {
        const int result = send(socket, message.data() + sent,
                                static_cast<int>(message.size() - sent), 0);
        if (result == SOCKET_ERROR || result == 0) {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

bool receiveLine(SOCKET socket, std::string& request) {
    char buffer[256];
    while (request.size() <= kMaxRequestSize) {
        const int received = recv(socket, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            return !request.empty();
        }
        request.append(buffer, static_cast<std::size_t>(received));
        const auto newline = request.find('\n');
        if (newline != std::string::npos) {
            request.resize(newline);
            if (!request.empty() && request.back() == '\r') {
                request.pop_back();
            }
            return true;
        }
    }
    return false;
}

bool parsePositiveInteger(const std::string& text, std::int64_t& value) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end && value > 0;
}

bool calculateTotal(const std::string& request, std::int64_t& total) {
    const auto first = request.find(';');
    const auto second = first == std::string::npos ? std::string::npos
                                                    : request.find(';', first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        request.find(';', second + 1) != std::string::npos || first == 0) {
        return false;
    }

    std::int64_t price = 0;
    std::int64_t quantity = 0;
    if (!parsePositiveInteger(request.substr(first + 1, second - first - 1), price) ||
        !parsePositiveInteger(request.substr(second + 1), quantity) ||
        price > std::numeric_limits<std::int64_t>::max() / quantity) {
        return false;
    }

    total = price * quantity;
    return true;
}
}  // namespace

int main() {
    WinsockGuard winsock;
    if (!winsock.ready) {
        std::cerr << "Failed to initialize WinSock\n";
        return 1;
    }

    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(kPort);

    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on port " << kPort << " (WinSock error "
                  << WSAGetLastError() << ")\n";
        closesocket(listener);
        return 1;
    }

    std::cout << "Server started on port " << kPort << std::endl;
    while (true) {
        const SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            std::cerr << "Failed to accept client\n";
            continue;
        }

        std::cout << "Client connected" << std::endl;
        std::string request;
        std::string response;
        std::int64_t total = 0;
        if (receiveLine(client, request)) {
            std::cout << "Received: " << request << std::endl;
            if (calculateTotal(request, total)) {
                std::cout << "Calculated total: " << total << std::endl;
                response = "OK;total=" + std::to_string(total);
            } else {
                response = "ERROR;invalid_request";
            }
        } else {
            std::cout << "Received: invalid or oversized request" << std::endl;
            response = "ERROR;invalid_request";
        }

        sendAll(client, response + "\n");
        std::cout << "Response sent: " << response << std::endl;
        shutdown(client, SD_BOTH);
        closesocket(client);
    }
}

