#include <netinet/in.h>
#include <linux/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>

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

// int flags = fcntl(client_fd, F_GETFL, 0);
// fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

class Server final
{
public:
    Server(int type, int port) :
           m_type(type),
           m_port(port)
    {}

    int initServer() {
        m_socket_fd_server = socket(m_type, SOCK_STREAM | SOCK_NONBLOCK, 0);

        sockaddr_in sa;
        sa.sin_family = m_type;
        sa.sin_port = htons(m_port);
        // htons тут нужен, что преобразовать номер порта в сетевой порядок байт.
        // big endian || little endian

        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        // INADDR_ANY - это специальынй адрес, который позволяет принимать сообщения на сетевом интерфейсе (0.0.0.0)
        // INADDR_LOOPBACK - это специальный адрес, который позволяет принимать сообщения только на локальном интерфейсе
        // inet_pton(AF_INET, "10.0.0.1", &sa);
        // inet_pton - это функция, которая преобразует строку в адрес в сетевом порядке байт

        int rc = bind(m_socket_fd_server, (struct sockaddr *)&sa, sizeof(sa));

        if (rc == -1)
            throw std::runtime_error{"Error binding socket"};

        rc = listen(m_socket_fd_server, SOMAXCONN);
        // SOMAXCONN - это максимальное количество соединений, которые могут быть в очереди на соединение

        if (rc == -1)
            throw std::runtime_error{"Error listening on socket"};

        return rc;
    }

    int getServerSocket() {
        return m_socket_fd_server;
    }

    int acceptConnection() const {
        return accept(m_socket_fd_server, nullptr, nullptr);
    }

    int readMesssage(int socket_connect_fd, char *message_buffer, std::size_t buffer_size) const {
        return recv(socket_connect_fd, message_buffer, buffer_size, MSG_NOSIGNAL);
    }

    int sendResponse(int socket_connect_fd, char *message_buffer, std::size_t buffer_size) const {
        return send(socket_connect_fd, message_buffer, buffer_size, MSG_NOSIGNAL);
    }

    ~Server() { close(m_socket_fd_server); }
private:
    int m_socket_fd_server;
    int m_type; // AF_INET
    int m_port;
    std::set<int> m_desc_set;
};


int main()
{
    Server server(AF_INET, 7771);
    set_nonblock(server.getServerSocket());
    server.initServer();

    fd_set readfds;
    FD_ZERO(&readfds);

    int serverSocket = server.getServerSocket();
    FD_SET(serverSocket, &readfds);

    std::set<int> clientConnectedSockets;
    int maxFdDesc = serverSocket; // На данный момент это максимальный сокет


    // close(socket_fd) <<<<<

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        // Серверный сокет срабатывает, только если кто то хочет подключиться
        // Для взаимодействия сервера и клиента используется сокет-acceptor
        // Поэтому мы храним их отдельно
        for (const auto& fd : clientConnectedSockets)
            FD_SET(fd, &readfds);

        // Процесс уснул, пока не сработает 1 из нескольких событий указаных в аргументах (4 величины после maxFdDesc + 1)
        // Возникло сообытие на чтение из сета дискриторов на чтение (&readfds), если из дискриптора можно прочитатть, событие произошло
        // Возникло событие на запись из сета дискрипторов на запись
        // Возникло событие ошибки из сета дискриторов ошибки
        // Последнее это таймаут, если мы не хотим, чтобы select вечно держал процесс, а и мы выполняли какуюто другую работу
        int res = select(maxFdDesc + 1, &readfds, nullptr, nullptr, nullptr);

        // Серверный сокет срабатывает, только если кто то хочет подключиться
        // Для взаимодействия сервера и клиента используется сокет-acceptor
        if (res != -1) {
            if (FD_ISSET(serverSocket, &readfds)) {
                int serverSocketAcceptor = server.acceptConnection();
                if (serverSocketAcceptor < 0)
                    continue;

                clientConnectedSockets.insert(serverSocketAcceptor);

                std::string messageBuffer;
                std::cout << "new client connection from: " << serverSocketAcceptor << std::endl;
                FD_CLR(serverSocket, &readfds);
            }
            else {
                for (auto clientSocketIter = clientConnectedSockets.begin(); clientSocketIter != clientConnectedSockets.end();) {
                    if (FD_ISSET(*clientSocketIter, &readfds)) {
                        char clientMessageBuffer[1024];
                        auto lenBuffer = sizeof(clientMessageBuffer);

                        std::memset(clientMessageBuffer, '\0', sizeof(clientMessageBuffer));

                        int clientSocket = *clientSocketIter;

                        int len = server.readMesssage(clientSocket, clientMessageBuffer);
                        if (len > 0) {
                            for (const auto& fd : clientConnectedSockets) {
                                if (fd != clientSocket) {
                                    std::string responseToAll = std::string{"Cliend with id = "}
                                                       .append(std::to_string(clientSocket)).append(" says: ").append(clientMessageBuffer).append("\n");
                                    server.sendResponse(fd, responseToAll.data());
                                }
                            }
                            server.sendResponse(clientSocket, clientMessageBuffer);
                            ++clientSocketIter;
                        }
                        else if (len == 0 && errno != EAGAIN) {
                            // len = 0 означет что клиент вывал close или shutdown(SHUT_WR)
                            std::cout << "client " << clientSocket << " disconnected " << std::endl;
                            shutdown(clientSocket, SHUT_RDWR);
                            close(clientSocket);
                            clientSocketIter = clientConnectedSockets.erase(clientSocketIter);
                        }
                        else
                            continue;
                    }
                    else
                        ++clientSocketIter;
                }
            }
        }
        else
            return -1;

        if (!clientConnectedSockets.empty())
            maxFdDesc = std::min(std::max(maxFdDesc, *clientConnectedSockets.crbegin()), 1022);
    }

    return 0;
}