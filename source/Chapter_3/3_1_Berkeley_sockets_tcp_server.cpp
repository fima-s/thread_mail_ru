#include <iostream>
#include <netinet/in.h>
#include <linux/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>

class Server final
{
public:
    Server(int type, int port) :
           m_type(type),
           m_port(port)
    {}

    int initServer() {
        m_socket_fd_server = socket(m_type, SOCK_STREAM, 0);

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

        if (rc == -1) {
            std::cerr << "Error binding socket" << std::endl;
            return -1;
        }

        rc = listen(m_socket_fd_server, SOMAXCONN);
        // SOMAXCONN - это максимальное количество соединений, которые могут быть в очереди на соединение

        if (rc == -1) {
            std::cerr << "Error listening on socket" << std::endl;
            return -1;
        }

        return rc;
    }

    int getServerSocket() {
        return m_socket_fd_server;
    }

    int acceptConnection() const {
        return accept(m_socket_fd_server, nullptr, nullptr);
    }

    int readMesssage(int socket_connect_fd, std::string &res) const {
        std::string message_buffer ("\0", 1024);
        // MSG_NOSIGNAL это флаг, который говорит, что не надо отправлять сигнал SIGPIPE, если сокет закрыт
        int len = recv(socket_connect_fd, message_buffer.data(), message_buffer.size() - 1, MSG_NOSIGNAL);

        if (len > 0) {
            message_buffer.resize(len);
            message_buffer[len - 1] = '\0';
            res = std::move(message_buffer);
        }

        return len;
    }

    int sendResponse(int socket_connect_fd, std::string responce) const {
        std::string answer = std::move(responce);
        return send(socket_connect_fd, answer.c_str(), answer.size(), MSG_NOSIGNAL);
    }

    ~Server() { close(m_socket_fd_server); }
private:
    int m_socket_fd_server;
    int m_type; // AF_INET
    int m_port;

};


int main() {

    Server server(AF_INET, 8080);
    server.initServer();

    int socket_connect_fd = -1; //  Идея тут слудующая, accept ждет пока появится соединение, как только оно повится возвращается дескриптор сокета,
    // который будет использоваться для общения с клиентом

    while ((socket_connect_fd = server.acceptConnection()) > 0) {
        std::string buffer;

        int len = server.readMesssage(socket_connect_fd, buffer);
        // MSG_NOSIGNAL это флаг, который говорит, что не надо отправлять сигнал SIGPIPE, если сокет закрыт
        if (len == -1) {
            std::cerr << "Error receiving message" << std::endl;
            close(socket_connect_fd);
            continue;
        }
        buffer[len] = '\0'; // добавляем нулевой символ в конец строки

        std::this_thread::sleep_for(std::chrono::seconds(3));

        std::cout << "Recieved message: " << buffer << std::endl;
        std::string response = std::move(buffer);
        server.sendResponse(socket_connect_fd, response);
        close(socket_connect_fd);
    }

    return 0;
}