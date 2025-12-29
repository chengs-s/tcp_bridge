#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_arg = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
using socklen_arg = socklen_t;
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

int main()
{
    const int listenPort = 5000; // Match remotePort in the bridge config
    if (!initSockets())
    {
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET)
    {
        std::cerr << "Failed to create socket\n";
        cleanupSockets();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listenPort);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "bind failed\n";
        closeSocket(server);
        cleanupSockets();
        return 1;
    }

    if (listen(server, 2) == SOCKET_ERROR)
    {
        std::cerr << "listen failed\n";
        closeSocket(server);
        cleanupSockets();
        return 1;
    }

    std::cout << "Device server listening on port " << listenPort << std::endl;
    sockaddr_in clientAddr{};
    socklen_arg len = sizeof(clientAddr);
    SOCKET client = accept(server, reinterpret_cast<sockaddr*>(&clientAddr), &len);
    if (client == INVALID_SOCKET)
    {
        std::cerr << "accept failed\n";
        closeSocket(server);
        cleanupSockets();
        return 1;
    }

    std::vector<char> buf(4096);
    for (;;)
    {
        int n = recv(client, buf.data(), static_cast<int>(buf.size()), 0);
        if (n <= 0)
        {
            break;
        }
        std::cout << "recv: " << std::string(buf.data(), buf.data() + n) << std::endl;
        send(client, buf.data(), n, 0); // Echo back
    }

    closeSocket(client);
    closeSocket(server);
    cleanupSockets();
    return 0;
}
