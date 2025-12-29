#include "net_io.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

// Simple toggleable debug logger controlled via env BRIDGE_DEBUG=1 or --debug flag
std::atomic<bool> gDebug{false};

void debugLog(const std::string& msg)
{
    if (gDebug)
    {
        std::cout << "[debug] " << msg << std::endl;
    }
}

} // namespace

// Per-bridge configuration: target external endpoint and the local listening port paired to it
struct BridgeConfig
{
    std::string remoteIp;
    int remotePort = 0;
    int listenPort = 0;
};

// Represents one bidirectional bridge: maintains a long-lived client link to the remote
// device and accepts upstream connections from the local host for forwarding.
class TcpBridgeInstance
{
public:
    explicit TcpBridgeInstance(BridgeConfig cfg) : config(std::move(cfg)) {}

    void start()
    {
        setupRemote();
        setupServer();
        debugLog("bridge started: remote " + config.remoteIp + ":" + std::to_string(config.remotePort) +
                 " <-> listen " + std::to_string(config.listenPort));
    }

    bool isRemoteConnected() const
    {
        return remote.CheckLinkOk();
    }

    const BridgeConfig& getConfig() const
    {
        return config;
    }

private:
    BridgeConfig config;
    NetTcpIO remote;
    NetTcpIO server;
    std::atomic<bool> running{true};
    std::thread maintainThread;
    std::mutex remoteReadMutex;
    std::mutex remoteWriteMutex;

    void setupRemote()
    {
        // Configure the always-on client socket to the external device
        NetTcpPARAM param{};
        param.bServer = 0;
        param.RemoteIp = config.remoteIp;
        param.RemotePort = config.remotePort;
        param.bRefRecvTimeout = 1;
        param.RecvTimeout = 200;
        param.bRefConnectTimeout = 0; // use blocking connect for local demo stability
        param.bNoDelay = 1;
        remote.SetParam(param);
        if (!remote.Open())
        {
            debugLog("initial remote connect failed: " + config.remoteIp + ":" + std::to_string(config.remotePort));
        }
        else
        {
            debugLog("connected to remote " + config.remoteIp + ":" + std::to_string(config.remotePort));
        }

        // Separate maintenance thread keeps the remote link alive
        maintainThread = std::thread([this]() { maintainRemoteConnection(); });
    }

    void setupServer()
    {
        // Start a local TCP server so the upstream host can connect
        NetTcpPARAM param{};
        param.bServer = 1;
        param.bRefLocalPort = 1;
        param.LocalPort = config.listenPort;
        param.ServerFunc = [this](SOCKET_T clientSock) {
            std::thread(&TcpBridgeInstance::handleConnection, this, clientSock).detach();
        };

        server.SetParam(param);
        server.Open();
    }

    void maintainRemoteConnection()
    {
        // Periodically check and reconnect the remote side if necessary
        while (running)
        {
            if (!remote.CheckLinkOk())
            {
                remote.Close();
                remote.Open();
                if (remote.CheckLinkOk())
                {
                    debugLog("reconnected to remote " + config.remoteIp + ":" + std::to_string(config.remotePort));
                }
                else
                {
                    debugLog("reconnect failed " + config.remoteIp + ":" + std::to_string(config.remotePort));
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    bool ensureRemoteConnected()
    {
        if (!remote.CheckLinkOk())
        {
            remote.Close();
            remote.Open();
        }
        return remote.CheckLinkOk();
    }

    void handleConnection(SOCKET_T clientSock)
    {
        // Bridge one upstream client with the persistent remote connection
        std::atomic<bool> active{true};
        debugLog("client connected on port " + std::to_string(config.listenPort));

        auto closeClient = [clientSock]() {
            if (clientSock != INVALID_SOCKET_T)
            {
#ifdef _WIN32
                closesocket(clientSock);
#else
                close(clientSock);
#endif
            }
        };

        std::thread upstream([&]() {
            std::vector<uint8_t> buffer(4096);
            while (active)
            {
                // Read from upstream host and push to remote device
                int received = ::recv(clientSock, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
                if (received <= 0)
                {
                    active = false;
                    break;
                }

                if (!ensureRemoteConnected())
                {
                    continue;
                }

                std::lock_guard<std::mutex> lock(remoteWriteMutex);
                if (!remote.sendData(buffer.data(), received))
                {
                    debugLog("send to remote failed, closing remote");
                    remote.Close();
                }
            }
        });

        std::thread downstream([&]() {
            std::vector<uint8_t> buffer(4096);
            while (active)
            {
                // Pull data from remote device and forward to upstream host
                if (!ensureRemoteConnected())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }

                int readSize = 0;
                {
                    std::lock_guard<std::mutex> lock(remoteReadMutex);
                    if (!remote.Read(buffer.data(), static_cast<int>(buffer.size()), &readSize))
                    {
                        continue;
                    }
                }

                if (readSize <= 0)
                {
                    continue;
                }

                int written = ::send(clientSock, reinterpret_cast<const char*>(buffer.data()), readSize, 0);
                if (written <= 0)
                {
                    debugLog("send to client failed, closing client");
                    active = false;
                    break;
                }
            }
        });

        while (active)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        closeClient();
        if (upstream.joinable())
        {
            upstream.join();
        }
        if (downstream.joinable())
        {
            downstream.join();
        }
        debugLog("client disconnected on port " + std::to_string(config.listenPort));
    }
};

class TcpBridgeManager
{
public:
    TcpBridgeManager(std::vector<BridgeConfig> cfgs, int statusPort)
        : configs(std::move(cfgs)), statusListenPort(statusPort) {}

    void start()
    {
        // Instantiate all bridges and launch their loops
        for (const auto& cfg : configs)
        {
            bridges.emplace_back(std::make_unique<TcpBridgeInstance>(cfg));
            bridges.back()->start();
        }
        startStatusServer();
    }

private:
    std::vector<BridgeConfig> configs;
    std::vector<std::unique_ptr<TcpBridgeInstance>> bridges;
    int statusListenPort = 0;
    NetTcpIO statusServer;

    void startStatusServer()
    {
        // Lightweight status server for the upper host to query bridge health
        NetTcpPARAM statusParam{};
        statusParam.bServer = 1;
        statusParam.bRefLocalPort = 1;
        statusParam.LocalPort = statusListenPort;
        statusParam.ServerFunc = [this](SOCKET_T clientSock) {
            const std::string report = buildStatusReport();
            ::send(clientSock, report.c_str(), static_cast<int>(report.size()), 0);
#ifdef _WIN32
            closesocket(clientSock);
#else
            close(clientSock);
#endif
        };

        statusServer.SetParam(statusParam);
        statusServer.Open();
        debugLog("status server listening on port " + std::to_string(statusListenPort));
    }

    std::string buildStatusReport() const
    {
        // Build plain-text status lines for each bridge
        std::string report;
        for (const auto& bridge : bridges)
        {
            const auto& cfg = bridge->getConfig();
            report += "remote " + cfg.remoteIp + ":" + std::to_string(cfg.remotePort);
            report += " -> listen " + std::to_string(cfg.listenPort);
            report += " connected=" + std::string(bridge->isRemoteConnected() ? "1" : "0") + "\n";
        }
        return report;
    }
};

int main(int argc, char** argv)
{
    // Enable debug when BRIDGE_DEBUG=1 or --debug flag is provided
    if (const char* env = std::getenv("BRIDGE_DEBUG"))
    {
        if (std::string(env) == "1")
        {
            gDebug = true;
        }
    }
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--debug")
        {
            gDebug = true;
        }
    }

    // Example configuration: two remote endpoints and one status port
    std::vector<BridgeConfig> configs = {
        {"192.168.200.112", 9100, 15000},
        {"192.168.200.113", 9100, 15001},
        {"192.168.200.114", 9100, 15002},
        {"192.168.200.115", 9100, 15003}
    };

    TcpBridgeManager manager(std::move(configs), 16000);
    manager.start();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
