
//#include "stdc++.h"
//#include "special_math.h"
//#include "file.h"
#include "net_io.h"
#include <iostream>
#include <chrono>
//#include "spdloguse.h"

#ifdef _WIN32
#include <windows.h>

#pragma comment(lib,"ws2_32.lib") 
typedef int sockaddr_size_t;

#elif __linux__

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

typedef socklen_t sockaddr_size_t;
#endif
#include <assert.h>


static void InitSocketSystem()
{
#ifdef _WIN32
    static bool bInit = false;
    if (!bInit)
    {
        WSADATA Ws;
        if (WSAStartup(0x202, &Ws) != 0)
        {
            //printf("Init Windows Socket Failed\r\n");
        }
        bInit = true;
    }
#endif
}

static bool SetRecvTimeout(SOCKET_T s, int ms)
{
    int time_out = ms; //ms
#ifdef _WIN32
    return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out)) >= 0;
#elif __linux__
    struct timeval time{};
    time.tv_sec = time_out / 1000;         //set the rcv wait time
    time.tv_usec = time_out % 1000 * 1000; //100000us = 0.1s
    return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) >= 0;
#endif
}

static int GetSockError()
{
#ifdef _WIN32
    return GetLastError();
#else
    return errno;
#endif
}

static bool IsErrorTimeout()
{
#ifdef _WIN32
    int err = GetLastError();
    return err == 10060;
#else
    return (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN);
#endif
}


//bool NetUdpIO::IsErrorTimeout()
//{
//    return false;
//}

bool NetUdpIO::SetUdpRecvTimeout()
{
    if (Param.bRefRecvTimeout)
        return SetRecvTimeout(sock, Param.RecvTimeout);
    return true;
}

bool NetUdpIO::SetUdpBoardCast()
{
    if (Param.bBroadcast)
    {
#ifdef _WIN32
        bool bOpt = true; 
        return setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&bOpt, sizeof(bOpt)) >= 0;
#elif __linux__
        const int opt = 1;
        return setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt)) >= 0;
#endif
    }
    return true;
}

int NetUdpIO::DoClose()
{
    bOpen = false;
    if (sock == INVALID_SOCKET_T)
        return 1;
#ifdef _WIN32
    closesocket(sock);
#elif __linux__
    close(sock);
#endif
    sock = INVALID_SOCKET_T;
    return 1;
}

int NetUdpIO::Open()
{
    std::lock_guard<std::mutex> lock(m_OpenAct);
    if (!bOpen)
    {
        assert(sock == INVALID_SOCKET_T);
        InitSocketSystem();

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = Param.bRefLocalPort ? htons(Param.LocalPort) : 0;
        address.sin_addr.s_addr = Param.bRefLocalIp ? inet_addr(Param.LocalIp.c_str()) : 0; // INADDR_ANY;

        sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0)
        {
            //printf("can not bind socket");
            DoClose();
            return 0;
        }

        if (!SetUdpRecvTimeout())
        {
           // printf("SetUdpRecvTimeout failed:%d\r\n", GetSockError());
            DoClose();
            return 0;
        }

        if (!SetUdpBoardCast())
        {
            DoClose();
            return 0;
        }

//        int time_out = 500; //ms
//#ifdef _WIN32
//        if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out)) < 0)
//        {
//            printf("set udp 500ms timeout failed:%d\r\n", GetSockError());
//                DoClose();
//                return 0;
//        }
//#elif __linux__
//        struct timeval time;
//        time.tv_sec = time_out / 1000;         //set the rcv wait time
//        time.tv_usec = time_out % 1000 * 1000; //100000us = 0.1s
//        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
//#endif
        bOpen = true;
        return 1;
    }
    return 1;
}

int NetUdpIO::Close()
{
    std::lock_guard<std::mutex> lock(m_OpenAct);
    return DoClose();
}

int NetUdpIO::Read(uint8_t *pData, int DataSize, int *pReadSize)
{
    if (Open())
    {
        sockaddr_in source{};
        sockaddr_size_t source_size = sizeof(source);
        int Ret = ::recvfrom(sock, (char*)pData, DataSize, 0,
            (sockaddr*)&source, &source_size);
        if (Ret < 0)
        {
            if (pReadSize)
                *pReadSize = 0;
            if (!IsErrorTimeout())
            {
                DoClose();
            }
            return 0;
        }
        else
        {
            if (pReadSize)
                *pReadSize = Ret;
            return 1;
        }
    }
    return 0;
}

int NetUdpIO::Write(const uint8_t *pData, int DataSize, int *pWriteSize)
{
    if (Open())
    {
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(Param.RemotePort);
        //if(Param.bBroadcast)
        //    server_addr.sin_addr.s_addr = INADDR_BROADCAST;
        //else
            server_addr.sin_addr.s_addr = inet_addr(Param.RemoteIp.c_str());
        int Ret = ::sendto(sock, (const char*)pData, DataSize, 0,
            (sockaddr*)&(server_addr), sizeof(server_addr));

        if (Ret < 0)
        {
            if (pWriteSize)
                *pWriteSize = 0;
            Close();
            return 0;
        }
        else
        {
            if (pWriteSize)
                *pWriteSize = Ret;
            return 1;
        }
    }
    return 0;
}

bool NetUdpIO::ReadClear()
{
    if (Open())
    {
        SetRecvTimeout(sock, 1);
        std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(0x400);
        while (true)
        {
            int size = 0;
            int ret = Read(buf.get(), 0x400, &size);
            if (size == 0)
                break;
        }
        SetUdpRecvTimeout();
        return 1;
    }
    else
        return 0;
}

int NetUdpIO::TransIp(std::string ipstr)
{
    return inet_addr(ipstr.c_str());
}

int NetUdpIO::recvfrom(SOCKET_T sock, void *buf, size_t size)
{
    sockaddr_in source{};
    sockaddr_size_t source_size = sizeof(source);
    return ::recvfrom(sock, (char*)buf, size, 0,
                       (sockaddr *)&source, &source_size);
}

int NetUdpIO::sendto(SOCKET_T sock, const void *buf, size_t size, std::string remote_ip, int remote_port)
{
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(remote_port);
    server_addr.sin_addr.s_addr = inet_addr(remote_ip.c_str());
    return ::sendto(sock, (const char*)buf, size, 0,
                     (sockaddr *)&(server_addr), sizeof(server_addr));
}

//// tcp
//int NetTcpIO::GetSockError()
//{
//#ifdef _WIN32
//    return GetLastError();
//#else
//    return errno;
//#endif
//}

bool NetTcpIO::Open()
{
	std::lock_guard<std::mutex> guard(m_OpenAct);
	if (!bOpen)
	{
        InitSocketSystem();
        assert(sock == INVALID_SOCKET_T);
        sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET_T)
            return 0;

        //printf("bind\r\n");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = Param.bRefLocalPort ? htons(Param.LocalPort) : 0;
        address.sin_addr.s_addr = Param.bRefLocalIp ? inet_addr(Param.LocalIp.c_str()) : 0; // INADDR_ANY;
        if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            //printf("can not bind socketr\r\n");
            DoClose();
            return 0;
        }
//#ifdef _WIN32
//#else
//        // 设置接收缓冲区大小
//        int rcvbuf = 1024 * 6;  // 6K
//        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) == -1) {
//            perror("setsockopt SO_RCVBUF");
//        }
//
//        // 设置发送缓冲区大小
//        int sndbuf = 1024 * 6;  // 6K
//        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) == -1) {
//            perror("setsockopt SO_SNDBUF");
//        }
//#endif
        if(Param.bNoDelay)
        {
            //printf("set tcp_nodelay\r\n");
#ifdef _WIN32
            char opt = 1;
#else
            int opt = 1;
#endif
            if(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt)) < 0)
            {
                //printf("set tcp_nodelay failed:%d\r\n", GetSockError());
                DoClose();
                return 0;
            }
        }

        if(SetTcpRecvTimeout() == false)
        {
            DoClose();
            return 0;
        }

        bool ret = false;
        if(Param.bServer)
            ret = RunServer();
        else
            ret = ConnectServer();
        if(ret == false)
        {
            DoClose();
            return 0;
        }
        bOpen = true;
        return 1;
	}
	return 1;
}

bool NetTcpIO::Close()
{
	std::lock_guard<std::mutex> guard(m_OpenAct);
    return DoClose();
}

bool NetTcpIO::isSocketReadable(int timeout_sec)
{
    if (!bOpen || sock == INVALID_SOCKET_T)
        return false;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    struct timeval timeout{};
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    int result = select(sock + 1, &read_fds, NULL, NULL, &timeout);
    return result > 0 && FD_ISSET(sock, &read_fds);
}

bool NetTcpIO::DoClose()
{
    bOpen = false;
    if(sock == INVALID_SOCKET_T)
        return 1;
#ifdef _WIN32
    closesocket(sock);
#elif __linux__
    close(sock);
#endif
    sock = INVALID_SOCKET_T;
    WaitServerFinish();
	return 1;
}

void NetTcpIO::WaitServerFinish()
{
    if(Param.bServer)
    { 
    if (listening)
    {
        listening->join();
    }
    }
}

bool NetTcpIO::ConnectServer()
{
    if (Param.bRefConnectTimeout != 0)
    {
        //socket设置为非阻塞 
#ifdef _WIN32
         unsigned long on = 1;
         if (ioctlsocket(sock, FIONBIO, &on) < 0) {
             //printf("ioctlsocket failed\n");
             return false;
         }
#else
         int flags = fcntl(sock, F_GETFL, 0);  // 获取当前文件描述符标志
         if (flags == -1) {
             perror("fcntl(F_GETFL)");
             return false;
         }

         flags |= O_NONBLOCK;  // 设置非阻塞标志
         if (fcntl(sock, F_SETFL, flags) == -1) {  // 更新文件描述符标志
             perror("fcntl(F_SETFL)");
             return false;
         }
#endif

        //尝试连接
        sockaddr_in clientService{};
        clientService.sin_family = AF_INET;
        clientService.sin_addr.s_addr = inet_addr(Param.RemoteIp.c_str());
        clientService.sin_port = htons(Param.RemotePort);
        int ret = connect(sock, (struct sockaddr*)&clientService, sizeof(clientService));
        if (ret == 0) {
            printf("connect success1\n");
            return true;
        }

        //因为是非阻塞的，这个时候错误码应该是WSAEWOULDBLOCK，Linux下是EINPROGRESS
#ifdef _WIN32
        if (ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
            //printf("connect failed with error: %ld\n", WSAGetLastError());
            return false;
        }
#elif __linux__
        if (ret < 0 && GetSockError() != EINPROGRESS) {
            //printf("connect failed with error: %d\n", GetSockError());
            return false;
    }
#endif

        fd_set writeset;
        FD_ZERO(&writeset);
        FD_SET(sock, &writeset);
        timeval tv{};
        tv.tv_sec =  Param.ConnectTimeout;
        tv.tv_usec = 0;//10000;
        ret = select(sock + 1, nullptr, &writeset, nullptr, &tv);
        if (ret == 0) {
            printf("connect timeout\n");
            return false;
        }
        else if (ret < 0) {
            //printf("connect failed with error: %d\n", GetSockError());
            return false;
        }
        else {
            //printf("connect success2\n");
        }
        // //
        // on = 0;
        // if (ioctlsocket(sock, FIONBIO, &on) < 0) {
        //     printf("ioctlsocket failed\n");
        //     return false;
        // }

        return true;
    }
    else
    {
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(Param.RemotePort);
        server_addr.sin_addr.s_addr = inet_addr(Param.RemoteIp.c_str());
        return ::connect(sock, (sockaddr*)&(server_addr), sizeof(server_addr)) != -1;
    }
}

bool NetTcpIO::RunServer()
{
    int ret = ::listen(sock, 2);
    if(ret < 0)
        return false;
    //std::cout << "listening..." << std::endl;
    auto listenfunc = [=]() {
        while (true)
        {
            sockaddr_in clientaddr{};
            sockaddr_size_t clientaddrsize = sizeof(sockaddr_in);
            SOCKET_T newconnect = ::accept(sock, (sockaddr *)&(clientaddr), &clientaddrsize);

            if (newconnect == INVALID_SOCKET_T)
            {
                auto err = GetSockError();
                std::cout << "error:" << err << std::endl;
                break;
            }
            if(Param.ServerFunc)
                Param.ServerFunc([=, &newconnect](char* pbuf, int bufsize) { return ::recv(newconnect, pbuf, bufsize, 0); },
                    [=, &newconnect](const char* pbuf, int bufsize) {return ::send(newconnect, pbuf, bufsize, 0); });
        }
    };
    listening = std::make_unique<std::thread>(listenfunc);
    return true;
}

bool NetTcpIO::SetTcpRecvTimeout()
{
    if(Param.bRefRecvTimeout)
    {
        return SetRecvTimeout(sock, Param.RecvTimeout);
    }
    return 1;
}

//bool NetTcpIO::IsErrorTimeout()
//{
//#ifdef _WIN32
//    return GetLastError() == 10060;
//#else
//    return errno == ETIMEDOUT;
//#endif
//}

bool NetTcpIO::Read(uint8_t* pData, int DataSize, int* pReadSize)
{
    if (Open())
    {
        //if (!isSocketReadable(5)) // 等待 5 秒，确认可读
        //{
        //    if (pReadSize) *pReadSize = 0;
        //    return false;
        //}

        int Ret = recv(sock, (char*)pData, DataSize, 0);
        if (Ret < 0)
        {
            if (pReadSize)
                *pReadSize = 0;

            if (!IsErrorTimeout())
            {
                /*
#ifdef _WIN32
                int errCode = WSAGetLastError();
                char* errMsg = nullptr;
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errMsg, 0, NULL);
                SpdLogUseA("Error recv data. ip[%s] Error[%d]: %s", Param.RemoteIp.c_str(), errCode, errMsg);
                LocalFree(errMsg);
#else
                SpdLogUseA("Error recv data. ip[%s] Error[%d]: %s", Param.RemoteIp.c_str(), errno, strerror(errno));
#endif
                */
                Close();
                bOpen = false;
                return 0;
            }
            return 0;
        }
        else if(Ret == 0)
        {
            if (pReadSize)
                *pReadSize = 0;
            /*
#ifdef _WIN32
            int errCode = WSAGetLastError();
            char* errMsg = nullptr;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errMsg, 0, NULL);
            SpdLogUseA("Error recv data. ip[%s] Error[%d]: %s", Param.RemoteIp.c_str(), errCode, errMsg);
            LocalFree(errMsg);
#else
            SpdLogUseA("Error recv data. ip[%s] Error[%d]: %s", Param.RemoteIp.c_str(), errno, strerror(errno));
#endif            
            */
            Close();
            bOpen = false;
            return 0;
        }
        else
        {
            if (pReadSize)
                *pReadSize = Ret;
            return 1;
        }
    }
    else
        return 0;
}

bool NetTcpIO::ReadClear()
{
	if (Open())
	{
		SetRecvTimeout(sock, 1);
		std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(0x400);
		while (true)
		{
			int size = 0;
			int ret = Read(buf.get(), 0x400, &size);
			if (size == 0)
				break;
		}
		SetTcpRecvTimeout();
		return 1;
	}
	else
		return 0;
}

bool NetTcpIO::Write(const uint8_t* pData, int DataSize, int* pWriteSize)
{
    if (bOpen)
    {
        int Ret = ::send(sock, (const char*)pData, DataSize, 0);

        if (Ret < 0)
        {
            if (pWriteSize)
                *pWriteSize = 0;
            if(!IsErrorTimeout())
            {
                /*
#ifdef _WIN32
                int errCode = WSAGetLastError();
                char* errMsg = nullptr;
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errMsg, 0, NULL);
                SpdLogUseA("Error send data. ip[%s] Error[%d]: %s", Param.RemoteIp.c_str(), errCode, errMsg);
                LocalFree(errMsg);
#else
                SpdLogUseA("Error send data. ip[%s] Error[%d]: %s", Param.RemoteIp.c_str(), errno, strerror(errno));
#endif
*/
                bOpen = false;
                Close();
                return false;
            }
            return true;
        }
        else if(Ret == 0)
        {
            if (pWriteSize)
                *pWriteSize = 0;
            /*
#ifdef _WIN32
            int errCode = WSAGetLastError();
            char* errMsg = nullptr;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errMsg, 0, NULL);
            SpdLogUseA("Error send data. ip[%s] Error[%d]: %s", Param.RemoteIp.c_str(), errCode, errMsg);
            LocalFree(errMsg);
#else
            SpdLogUseA("Error send data. ip[%s] Error[%d]: %s", Param.RemoteIp.c_str(), errno, strerror(errno));
#endif
*/
            bOpen = false;
            Close();
            return false;
        }
        else
        {
            if (pWriteSize)
                *pWriteSize = Ret;
            return true;
        }
    }
    else
        return false;
}

bool NetTcpIO::isSocketWritable(int sockfd, int timeout_sec)
{
    fd_set write_fds;
    struct timeval timeout{};
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    FD_ZERO(&write_fds);
    FD_SET(sockfd, &write_fds);

    int result = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
    return result > 0 && FD_ISSET(sockfd, &write_fds);
}

bool NetTcpIO::sendData(const uint8_t* data, int dataSize)
{
    if (!bOpen)
        return false;
    std::chrono::high_resolution_clock::time_point startTime = std::chrono::high_resolution_clock::now();

    size_t totalSent = 0;
    int sent_size = 0;
    while (totalSent < dataSize) {
        // 等待直到套接字可写
        if (!isSocketWritable(sock)) {
            //SpdLogUseA("ip[%s] Socket not writable, waiting...", Param.RemoteIp.c_str());
            continue;
        }
        //SpdLogUseA("NetTcpIO::sendData\n");

        int ret = Write(data + totalSent, static_cast<int>(dataSize - totalSent), &sent_size);
        if (!ret)
            return false;
        if (sent_size == 0) {
            auto end = std::chrono::high_resolution_clock::now();
            // 计算时间间隔
            std::chrono::duration<double, std::milli> elapsed_milliseconds = end - startTime;
            if (elapsed_milliseconds.count() > 1000) {
                //SpdLogUseA("ip[%s] Socket not writable 1000ms, waiting...", Param.RemoteIp.c_str());
                startTime = std::chrono::high_resolution_clock::now();
            }
        }
        totalSent += sent_size;
    }
    return true;
}

//bool NetTcpIO::SetRecvTimeout(SOCKET_T s, int ms)
//{
//        int time_out = ms; //ms
//    #ifdef _WIN32
//        return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&time_out, sizeof(time_out)) >= 0;
//    #elif __linux__
//        struct timeval time;
//        time.tv_sec = time_out / 1000;         //set the rcv wait time
//        time.tv_usec = time_out % 1000 * 1000; //100000us = 0.1s
//        return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) >= 0;
//    #endif
//}

