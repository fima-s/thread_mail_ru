#include <netinet/in.h>
#include <linux/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <stdexcept>



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

constexpr int MAX_EVENTS = 1;

int main()
{
    int masterSocketFd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(8080);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    int rc = bind(masterSocketFd, (struct sockaddr*)&sa, sizeof(sa));

    if (rc < 0)
        throw std::runtime_error{"error binding socket"};

    set_nonblock(masterSocketFd);

    rc = listen(masterSocketFd, SOMAXCONN);

    std::cerr << "server run on ipaddr: 0.0.0.0 and port: " << htons(sa.sin_port) << std::endl;
    int epollFd = epoll_create1(0);

    struct epoll_event masterSocketEvent;
    masterSocketEvent.data.fd = masterSocketFd;
    masterSocketEvent.events = EPOLLIN; // Отслеживаем доступоность на чтение
    // EPOLLIN | EPOLLET;
    // EPOLLET - нотифицируем только о новых данных. То есть если мы не дочитали из сокета данные, о них не будет нотифицировано.

    epoll_ctl(epollFd, EPOLL_CTL_ADD, masterSocketFd, &masterSocketEvent);

    while (true) {
        struct epoll_event epollEvents[MAX_EVENTS];
        int N = epoll_wait(epollFd, epollEvents, MAX_EVENTS, -1);

        for (int i = 0; i < N; ++i) {
            if (epollEvents[i].data.fd == masterSocketFd) {
                int slaveSocketFd = accept(masterSocketFd, nullptr, nullptr);
                set_nonblock(slaveSocketFd);

                struct epoll_event slaveSocketEvent;
                slaveSocketEvent.data.fd = slaveSocketFd;
                slaveSocketEvent.events = EPOLLIN;

                epoll_ctl(epollFd, EPOLL_CTL_ADD, slaveSocketFd, &slaveSocketEvent);
            }
            else {
                static char buffer[1024];
                int recvRes = recv(epollEvents[i].data.fd, buffer, sizeof(buffer), MSG_NOSIGNAL);
                if (recvRes == 0 && errno != EAGAIN) {
                    shutdown(epollEvents[i].data.fd, SHUT_RDWR);
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, epollEvents[i].data.fd, &epollEvents[i]); // ?????
                    close(epollEvents[i].data.fd); // Вот это очень важно
                }
                else if (recvRes > 0) {
                    send(epollEvents[i].data.fd, buffer, recvRes, MSG_NOSIGNAL);
                }
            }
        }
    }

    return 0;
}