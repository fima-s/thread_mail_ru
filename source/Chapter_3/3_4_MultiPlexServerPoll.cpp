#include <netinet/in.h>
#include <linux/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <algorithm>
#include <iostream>
#include <set>
#include <vector>
#include <string>
#include <cstring>

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

constexpr int POLL_SIZE = 4000;
int main()
{
    int masterSocketFd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(7777);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    int rc = bind(masterSocketFd, (struct sockaddr*)&sa, sizeof(sa));

    if (rc < 0)
        throw std::runtime_error{"error binding socket"};

    rc = listen(masterSocketFd, SOMAXCONN);

    struct pollfd setFd[POLL_SIZE];
    setFd[0].fd = masterSocketFd;
    setFd[0].events = POLLIN;
    //set_nonblock(setFd[0].fd);

    std::set<int> slaveSockets;

    while (true) {
        unsigned int index = 1;
        for (auto it = slaveSockets.begin(); it != slaveSockets.end(); ++it) {
            setFd[index].fd = *it;
            setFd[index].events = POLLIN;
            ++index;
        }

        rc = poll(setFd, POLL_SIZE, -1);

        if (rc > 0) {
            for (int i = 0; i < POLL_SIZE; ++i) {
                if (setFd[i].revents & POLLIN) {
                    if (i == 0) {
                        int inputSocketFd = accept(masterSocketFd, nullptr, nullptr);
                        slaveSockets.insert(inputSocketFd);
                    }
                    else {
                        char buffer[1024];
                        std::memset(buffer, '\0', sizeof(buffer));
                        int len = recv(setFd[i].fd, buffer, sizeof(buffer), MSG_NOSIGNAL);

                        if (len > 0) {
                            rc = send(setFd[i].fd, buffer, sizeof(buffer), MSG_NOSIGNAL);
                        }
                        else if (len == 0 && errno != EAGAIN) {
                            shutdown(setFd[i].fd, SHUT_RDWR);
                            close(setFd[i].fd);
                            slaveSockets.erase(setFd[i].fd);
                        }
                    }
                }
            }
        }

    }

    return 0;
}