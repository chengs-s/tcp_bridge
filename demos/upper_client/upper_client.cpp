#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
#endif

namespace {

bool initSockets()
{
#ifdef _WIN32
    static bool inited = false;
    if (!inited)
    {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            std::cerr << "WSAStartup failed\n";
            return false;
        }
        inited = true;
    }
#endif
    return true;
}

void cleanupSockets()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

void closeSocket(SOCKET s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

} // namespace

int main(int argc, char** argv)
{
    std::string bridgeHost = "127.0.0.1"; // Bridge listen address
    int bridgePort = 15000;               // Match listenPort in the bridge config

    if (argc >= 2)
    {
        bridgeHost = argv[1];
    }
    if (argc >= 3)
    {
        bridgePort = std::atoi(argv[2]);
    }
    if (!initSockets())
    {
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "socket creation failed\n";
        cleanupSockets();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bridgePort);
    inet_pton(AF_INET, bridgeHost.c_str(), &addr.sin_addr);
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "connect to bridge failed\n";
        closeSocket(sock);
        cleanupSockets();
        return 1;
    }

    std::cout << "Connected to bridge at " << bridgeHost << ":" << bridgePort << "\n";
    std::cout << "Type messages to send, or 'exit' to quit.\n";

    std::vector<char> buf(4096);
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line))
    {
        if (line == "exit" || line == "quit")
        {
            break;
        }
        if (line.empty())
        {
            continue;
        }

        // Append CRLF to each line before sending
        std::string payload = line + "\r\n";
        if (send(sock, payload.data(), static_cast<int>(payload.size()), 0) <= 0)
        {
            std::cerr << "send failed or connection closed\n";
            break;
        }

        int n = recv(sock, buf.data(), static_cast<int>(buf.size()), 0);
        if (n > 0)
        {
            std::cout << "response: " << std::string(buf.data(), buf.data() + n) << "\n";
        }
        else
        {
            std::cerr << "no response or connection closed\n";
            break;
        }
    }
    closeSocket(sock);
    cleanupSockets();
    return 0;
}
