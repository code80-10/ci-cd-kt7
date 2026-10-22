#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>

namespace {
constexpr unsigned short kPort = 8080;

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

bool receiveLine(SOCKET socket, std::string& response) {
    char buffer[256];
    while (response.size() <= 1024) {
        const int received = recv(socket, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            return !response.empty();
        }
        response.append(buffer, static_cast<std::size_t>(received));
        const auto newline = response.find('\n');
        if (newline != std::string::npos) {
            response.resize(newline);
            if (!response.empty() && response.back() == '\r') {
                response.pop_back();
            }
            return true;
        }
    }
    return false;
}
}  // namespace

int main(int argc, char* argv[]) {
    const std::string request = argc > 1 ? argv[1] : "Keyboard;5000;2";

    WinsockGuard winsock;
    if (!winsock.ready) {
        std::cerr << "Failed to initialize WinSock\n";
        return 1;
    }

    const SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketHandle == INVALID_SOCKET) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(kPort);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(socketHandle, reinterpret_cast<sockaddr*>(&server), sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to localhost:" << kPort << "\n";
        closesocket(socketHandle);
        return 1;
    }

    std::cout << "Connected to server\n";
    std::cout << "Request: " << request << '\n';
    if (!sendAll(socketHandle, request + "\n")) {
        std::cerr << "Failed to send request\n";
        closesocket(socketHandle);
        return 1;
    }

    std::string response;
    if (!receiveLine(socketHandle, response)) {
        std::cerr << "Failed to receive response\n";
        closesocket(socketHandle);
        return 1;
    }

    std::cout << "Response: " << response << '\n';
    closesocket(socketHandle);
    return 0;
}

