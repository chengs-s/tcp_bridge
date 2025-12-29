#pragma once

#include "net_io.h"
#include <mutex>
#include <string>
#include <vector>

// Upper-layer TCP client with NetTcpIO-compatible API naming.
class BridgeClient
{
public:
    BridgeClient(std::string host, int port);

    // Set new remote endpoint (will Close and reopen on next Open).
    void SetRemote(std::string host, int port);

    // Open underlying TCP connection (blocking connect).
    bool Open();

    // Close connection.
    bool Close();

    // Check link status.
    bool CheckLinkOk() const;

    // Write data; returns success, optional pWriteSize.
    bool Write(const uint8_t* data, int size, int* pWriteSize = nullptr);

    // Read data; returns success, sets pReadSize (0 on timeout).
    bool Read(uint8_t* data, int size, int* pReadSize);

private:
    std::string remoteHost;
    int remotePort = 0;
    NetTcpIO conn;
    mutable std::mutex mu;
};
