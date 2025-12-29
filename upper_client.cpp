#include "bridge_client.h"

BridgeClient::BridgeClient(std::string host, int port)
    : remoteHost(std::move(host)), remotePort(port)
{
}

void BridgeClient::SetRemote(std::string host, int port)
{
    std::lock_guard<std::mutex> lock(mu);
    remoteHost = std::move(host);
    remotePort = port;
    conn.Close();
}

bool BridgeClient::Open()
{
    std::lock_guard<std::mutex> lock(mu);
    NetTcpPARAM p{};
    p.bServer = 0;
    p.RemoteIp = remoteHost;
    p.RemotePort = remotePort;
    p.bRefRecvTimeout = 1;
    p.RecvTimeout = 500;      // ms
    p.bRefConnectTimeout = 0; // blocking connect
    p.bNoDelay = 1;
    conn.SetParam(p);
    return conn.Open();
}

bool BridgeClient::Close()
{
    std::lock_guard<std::mutex> lock(mu);
    return conn.Close();
}

bool BridgeClient::CheckLinkOk() const
{
    return conn.CheckLinkOk();
}

bool BridgeClient::Write(const uint8_t* data, int size, int* pWriteSize)
{
    std::lock_guard<std::mutex> lock(mu);
    if (!conn.CheckLinkOk())
        return false;
    return conn.Write(data, size, pWriteSize);
}

bool BridgeClient::Read(uint8_t* data, int size, int* pReadSize)
{
    std::lock_guard<std::mutex> lock(mu);
    if (!conn.CheckLinkOk())
        return false;
    return conn.Read(data, size, pReadSize);
}
