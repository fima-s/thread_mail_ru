#include <iostream>
#include <netinet/in.h>
#include <linux/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>

int main() {

    int socket_fd_server = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd_server == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return -1;
    }

    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(8080);
    // htons тут нужен, что преобразовать номер порта в сетевой порядок байт.
    // big endian || little endian

    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    // INADDR_ANY - это специальынй адрес, который позволяет принимать сообщения на сетевом интерфейсе (0.0.0.0)
    // INADDR_LOOPBACK - это специальный адрес, который позволяет принимать сообщения только на локальном интерфейсе
    // inet_pton(AF_INET, "10.0.0.1", &sa);
    // inet_pton - это функция, которая преобразует строку в адрес в сетевом порядке байт

    int rc = bind(socket_fd_server, (struct sockaddr *)&sa, sizeof(sa));

    if (rc == -1) {
        std::cerr << "Error binding socket" << std::endl;
        return -1;
    }

    rc = listen(socket_fd_server, SOMAXCONN);
    // SOMAXCONN - это максимальное количество соединений, которые могут быть в очереди на соединение

    if (rc == -1) {
        std::cerr << "Error listening on socket" << std::endl;
        return -1;
    }

    int socket_connect_fd = -1; //  Идея тут слудующая, accept ждет пока появится соединение, как только оно повится возвращается дескриптор сокета,
    // который будет использоваться для общения с клиентом

    while ((socket_connect_fd = accept(socket_fd_server, nullptr, nullptr)) > 0) {
        char buffer[1024];
        int len = recv(socket_connect_fd, buffer, sizeof(buffer) - 1, MSG_NOSIGNAL);
        // MSG_NOSIGNAL это флаг, который говорит, что не надо отправлять сигнал SIGPIPE, если сокет закрыт
        if (len == -1) {
            std::cerr << "Error receiving message" << std::endl;
            close(socket_connect_fd);
            continue;
        }
        buffer[len] = '\0'; // добавляем нулевой символ в конец строки

        std::this_thread::sleep_for(std::chrono::seconds(3));

        std::cout << "Recieved message: " << buffer << std::endl;
        std::string response = std::string{buffer, len};
        len = send(socket_connect_fd, response.c_str(), len, MSG_NOSIGNAL);
        close(socket_connect_fd);
    }

    return 0;
}