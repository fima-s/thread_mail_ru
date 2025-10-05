#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <stdexcept>
#include <map>
#include <sstream>

int set_nonblock(int fd) {
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, & flags);
#endif
}




class ChatServer
{
public:
    ChatServer(int port)
    : m_port(port)
    {}

    void initServerListen() {
        m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in serverAddr;
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(m_port);

        int rc = bind(m_serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

        if (rc < 0) {
            std::string msg = "unable bind socket " + std::to_string(m_serverSocket).append(" with port").append(std::to_string(ntohs(serverAddr.sin_port)));
            throw std::runtime_error{msg};
        }

        set_nonblock(m_serverSocket);

        rc = listen(m_serverSocket, SOMAXCONN);

        if (rc < 0)
            throw std::runtime_error{"listen server returned error"};
    }

    void initServerEpoll() {
        m_epoll_fd = epoll_create1(0);

        if (m_epoll_fd < 0)
            throw std::runtime_error{"epoll fd for master socker < 0"};

        struct epoll_event master_ev;
        master_ev.events = EPOLLIN;
        master_ev.data.fd = m_serverSocket;

        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_serverSocket, &master_ev) < 0)
            throw std::runtime_error{"epoll_ctl finished with error"};
    }

    void runServer() {
        std::cerr << "Run server on 0.0.0.0 address and port " << m_port << std::endl;
        for (;;) {
            int nfds = epoll_wait(m_epoll_fd, m_connectionEvents, MAX_EPOLL_EVENTS, -1);

            for (int i = 0; i < nfds; ++i) {
                if (m_connectionEvents[i].data.fd == m_serverSocket) {
                    struct sockaddr_in connectionAddr;
                    socklen_t len = sizeof(connectionAddr);
                    int accept_fd = accept(m_serverSocket, (struct sockaddr *)&connectionAddr, &len); // + log

                    if (accept_fd > 0) {
                        struct epoll_event socket_event;
                        socket_event.events = EPOLLIN;
                        socket_event.data.fd = accept_fd;

                        epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, accept_fd, &socket_event); // + log
                    }

                    set_nonblock(accept_fd);

                    auto temp_addr = ntohl(connectionAddr.sin_addr.s_addr);
                    auto temp_port = ntohs(connectionAddr.sin_port);

                    char ipStr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &connectionAddr.sin_addr, ipStr, sizeof(ipStr));

                    std::string addr_inet(ipStr, sizeof(ipStr));

                    char buffer[1024];
                    int bufferLen = snprintf(buffer, sizeof(buffer), "New connection from addr: %d and port: %d \n", temp_addr, temp_port);

                    for (const auto& [socket, pair_addr_port] : m_connectedSockets) {
                        int rc = send(socket, buffer, bufferLen, MSG_NOSIGNAL);

                        if (rc < 0)
                            std::cerr << "Failed to send messege to socket: " << socket << std::endl;
                    }

                    m_connectedSockets.emplace(accept_fd, std::pair{temp_addr, temp_port});
                }
                else
                {
                    char buffer[1024];
                    int recvLen = recv(m_connectionEvents[i].data.fd, buffer, sizeof(buffer), MSG_NOSIGNAL);

                    if (recvLen == 0 && errno != EAGAIN) {
                        const std::pair<in_addr_t, in_port_t>& socket_ipaddr_and_port = m_connectedSockets[m_connectionEvents[i].data.fd];

                        char buffer[1024];
                        int len = snprintf(buffer, sizeof(buffer), "Connection from addr: %d and port: %d has been closed\n",
                                           socket_ipaddr_and_port.first, socket_ipaddr_and_port.second);

                        m_connectedSockets.erase(m_connectionEvents[i].data.fd);

                        for (const auto& [socket, pair_addr_port] : m_connectedSockets) {
                            int rc = send(socket, buffer, len, MSG_NOSIGNAL);
                            if (rc < 0)
                                std::cerr << "Failed to send messege to socket: " << socket << std::endl;
                        }

                        int close_fd = m_connectionEvents[i].data.fd;

                        shutdown(close_fd, SHUT_RDWR);
                        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, close_fd, NULL);
                        close(close_fd);
                    }
                    else if (recvLen > 0) {
                        for (const auto& [socket, pair_addr_port] : m_connectionEvents) {
                            //if (socket != m_connectionEvents[i].data.fd)
                                if (send(socket, buffer, recvLen, MSG_NOSIGNAL) < 0)
                                    std::cerr << "Failed to send messege to socket: " << socket << std::endl;
                        }
                    }
                    else {
                        std::cerr << "Failed to read data from socket: " << m_connectionEvents[i].data.fd << std::endl;
                    }
                }
            }
        }
    }

private:
    int m_serverSocket;
    int m_port;
    static const int MAX_EPOLL_EVENTS = 32;
    struct epoll_event m_connectionEvents[MAX_EPOLL_EVENTS];
    int m_epoll_fd;
    std::map<int, std::pair<in_addr_t, in_port_t>> m_connectedSockets; // sockets and its ipaddrs
};




int main()
{
    ChatServer server(8080);
    server.initServerListen();
    server.initServerEpoll();
    server.runServer();


    return 0;
}