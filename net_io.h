#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <memory>
#include <thread>

typedef  int SOCKET_T;
const int INVALID_SOCKET_T = -1;


struct NetUdpPARAM
{
    int    bRefLocalIp = 0;
    int    bRefLocalPort = 0;
    int    bRefRecvTimeout = 0;
    int    bBroadcast = 0;

    std::string   LocalIp = "192.168.1.101";
    int    LocalPort = 28020;

    int    RecvTimeout = 100; // ms

    std::string   RemoteIp = "192.168.1.222";
    int    RemotePort = 28020;
public:
    NetUdpPARAM()
    {
        LocalIp = "192.168.1.101";
        RemoteIp = "192.168.1.222";
        bRefRecvTimeout = 1;
    }
    NetUdpPARAM(const NetUdpPARAM& r)
    {
        bRefLocalIp = r.bRefLocalIp;
        bRefLocalPort = r.bRefLocalPort;
        bBroadcast = r.bBroadcast;
        bRefRecvTimeout = r.bRefRecvTimeout;
        RecvTimeout = r.RecvTimeout;
        LocalIp = r.LocalIp;
        LocalPort = r.LocalPort;
        RemoteIp = r.RemoteIp;
        RemotePort = r.RemotePort;
    }
    NetUdpPARAM operator= (const NetUdpPARAM& r)
    {
        bRefLocalIp = r.bRefLocalIp;
        bRefLocalPort = r.bRefLocalPort;
        bBroadcast = r.bBroadcast;
        bRefRecvTimeout = r.bRefRecvTimeout;
        RecvTimeout = r.RecvTimeout;
        LocalIp = r.LocalIp;
        LocalPort = r.LocalPort;
        RemoteIp = r.RemoteIp;
        RemotePort = r.RemotePort;
        return *this;
    }
};


class NetUdpIO
{
    NetUdpPARAM    Param;
    SOCKET_T     sock = INVALID_SOCKET_T;

    bool           bOpen = false;
    std::mutex     m_OpenAct;
protected:
    //bool IsErrorTimeout();
    bool SetUdpRecvTimeout();
    bool SetUdpBoardCast();
    int DoClose();

public:
    NetUdpIO() {}
    NetUdpIO(NetUdpPARAM param) { Param = param; }
    NetUdpIO(const NetUdpIO& io) 
    {
        Param = io.Param; 
    }

    void  SetParam(NetUdpPARAM param) { Param = param; }

    SOCKET_T GetSocket(){return sock;}

    int CheckLinkOk() { return bOpen; }
    int Open();
    int Close();
    int Read(uint8_t *pData, int DataSize, int *pReadSize);
    int Write(const uint8_t *pData, int DataSize, int *pWriteSize);
    bool ReadClear();
public:

    static int TransIp(std::string ipstr);

    // recvfrom
    static int recvfrom(SOCKET_T s, void *buf, size_t size);

    // sendto
    static int sendto(SOCKET_T s, const void *buf, size_t size, std::string remote_ip, int remote_port);
};

// void(readfunc, writefunc);
typedef std::function<void(std::function<int(char*, int)>, std::function<int(const char*, int)>)> TcpSerFunc;

struct NetTcpPARAM
{
    int    bServer = 0;
    int    bRefLocalIp = 0;
    int    bRefLocalPort = 0;
    int    bRefRecvTimeout = 0;
    int    bRefConnectTimeout = 0;
    int    bNoDelay = 0;

    std::string   LocalIp = "192.168.183.2";
    int    LocalPort = 0;

    std::string   RemoteIp = "192.168.183.1";
    int    RemotePort = 1947;

    int    ConnectTimeout = 0;
    int    RecvTimeout = 100; // ms

    TcpSerFunc ServerFunc;

    // 重载 == 操作符
    bool operator==(const NetTcpPARAM& other) const
    {
        return (bServer == other.bServer &&
            bRefLocalIp == other.bRefLocalIp &&
            bRefLocalPort == other.bRefLocalPort &&
            bRefRecvTimeout == other.bRefRecvTimeout &&
            bRefConnectTimeout == other.bRefConnectTimeout &&
            bNoDelay == other.bNoDelay &&
            LocalIp == other.LocalIp &&
            LocalPort == other.LocalPort &&
            RemoteIp == other.RemoteIp &&
            RemotePort == other.RemotePort &&
            ConnectTimeout == other.ConnectTimeout &&
            RecvTimeout == other.RecvTimeout );
    }
    // 重载 != 操作符
    bool operator!=(const NetTcpPARAM& other) const
    {
        // 基于 == 实现 !=
        return !(*this == other);
    }
};

class NetTcpIO
{
    NetTcpPARAM    Param;
    SOCKET_T     sock = INVALID_SOCKET_T;
    bool           bOpen = false;
    int            Sock_Error = 0;
    std::mutex     m_OpenAct;
    std::unique_ptr<std::thread> listening;
protected:
    bool RunServer();
    bool ConnectServer();
    //bool IsErrorTimeout();
    bool SetTcpRecvTimeout();
    bool DoClose();
public:
    NetTcpIO() {}
    NetTcpIO(NetTcpPARAM param) { Param = param; }

    void  SetParam(NetTcpPARAM param) 
    { 
        if (Param != param)
        {
            Close();
            Param = param;
        }
    }
    NetTcpPARAM GetParam() { return Param; }

    //int  GetSockError();
    bool CheckLinkOk() { return bOpen; }
    bool Open();
    bool Close();
    bool isSocketReadable(int timeout_sec);
    bool Read(uint8_t* pData, int DataSize, int* pReadSize);
    bool Write(const uint8_t* pData, int DataSize, int* pWriteSize);
    bool isSocketWritable(int sockfd, int timeout_sec = 1);
    bool sendData(const uint8_t* pData, int DataSize);
    bool ReadClear();

    void WaitServerFinish();

    //static bool SetRecvTimeout(SOCKET_T s, int ms);
};
