#include <iostream>
#include <netinet/in.h>
#include <linux/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <poll.h>
#include <arpa/inet.h>

#include <sstream>
#include <thread>
#include <vector>


int recv_with_timeout(int socket_fd_client, char *buffer, std::size_t buffer_size, int time_out_in_sec)
{
    struct pollfd pfd[1];
    pfd[0].fd = socket_fd_client;
    pfd[0].events = POLLIN;

    //char buffer[1024];

    int timeout_ms = time_out_in_sec * 1000; // конвертируем в миллисекунды

    // Вот это тот самый поллинг. Мы ждем втечении таймаута были ли события POLLIN на сокете
    // Если события были, то poll_result > 0
    int poll_result = poll(pfd, 1, timeout_ms);

    if (poll_result == -1) {
        std::cerr << "Error in poll" << std::endl;
        close(socket_fd_client);
        return -1;
    } else if (poll_result == 0) {
        std::cerr << "Timeout occurred" << std::endl;
        close(socket_fd_client);
        return -1;
    }

    int len = recv(socket_fd_client, buffer, buffer_size, MSG_NOSIGNAL);

    if (len == -1) {
        std::cerr << "Error receiving message" << std::endl;
        close(socket_fd_client);
        return -1;
    }

    return len;
}

int send_message(int socket_fd_client, const std::string &message)
{
    int send_len = send(socket_fd_client, message.c_str(), message.size(), MSG_NOSIGNAL);

    if (send_len == -1) {
        std::cerr << "Error sending message" << std::endl;
        close(socket_fd_client);
        return -1;
    }

    return send_len;
}


struct Client
{
public:
    Client(int port, const std::string &ip_address)
        : m_port(port), m_ip_address(ip_address)
    {}

    ~Client()
    {
        close(m_socket_fd_client);
    }

    void initClient()
    {
        m_socket_fd_client = socket(AF_INET, SOCK_STREAM, 0);

        if (m_socket_fd_client == -1) {
            std::cerr << "Error creating socket" << std::endl;
            return;
        }

        sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port = htons(m_port);
        inet_pton(AF_INET, m_ip_address.c_str(), &sa.sin_addr);

        int rc = connect(m_socket_fd_client, (struct sockaddr *)&sa, sizeof(sa));

        if (rc == -1) {
            close(m_socket_fd_client);
            throw std::runtime_error("Error connecting to socket");
        }
    }

    int sendMessage(const std::string &message)
    {
        int send_len = send(m_socket_fd_client, message.c_str(), message.size(), MSG_NOSIGNAL);

        if (send_len == -1) {
            std::cerr << "Error sending message" << std::endl;
            close(m_socket_fd_client);
            return -1;
        }

        return send_len;
    }

    int recieveMessageWithTimeout(char *buffer, std::size_t buffer_size, int time_out_in_sec) {
        struct pollfd pfd[1];
        pfd[0].fd = m_socket_fd_client;
        pfd[0].events = POLLIN;

        int timeout_ms = time_out_in_sec * 1000; // конвертируем в миллисекунды

        // Вот это тот самый поллинг. Мы ждем втечении таймаута были ли события POLLIN на сокете
        // Если события были, то poll_result > 0
        int poll_result = poll(pfd, 1, timeout_ms);

        if (poll_result == -1) {
            std::cerr << "Error in poll" << std::endl;
            close(m_socket_fd_client);
            return -1;
        } else if (poll_result == 0) {
            std::cerr << "Timeout occurred" << std::endl;
            close(m_socket_fd_client);
            return -1;
        }

        int len = recv(m_socket_fd_client, buffer, buffer_size, MSG_NOSIGNAL);

        if (len == -1) {
            std::cerr << "Error receiving message" << std::endl;
            close(m_socket_fd_client);
            return -1;
        }

        buffer[len] = '\0'; // добавляем нулевой символ в конец строки

        return len;
    }

private:
    int m_port;
    std::string m_ip_address;
    int m_socket_fd_client;
};


int main()
{
    // Client client(8080, "127.0.0.1");
    // client.initClient();

    // std::string message = "Hello, server!";

    // if (client.sendMessage(message) < 0) {
    //     std::cerr << "Error sending message" << std::endl;
    //     return -1;
    // }

    // std::string buffer(1024, '\0');

    // if (client.recieveMessageWithTimeout(buffer.data(), buffer.length(), 5) < 0) {
    //     std::cerr << "Error receiving message" << std::endl;
    //     return -1;
    // }

    // std::cout << "Received message from server: " << buffer << std::endl;


    auto MultiClientRequest = [](std::size_t port_number, const std::string& current_ipaddr) {
        Client client(port_number, current_ipaddr);
        client.initClient();

        std::ostringstream osstr;
        osstr << std::this_thread::get_id();
        std::string message = "Hello, server from thread: " + osstr.str() + '\n';

        while (true) {
            if (client.sendMessage(message) < 0 )
                std::cerr << "Error sending message" << std::endl;

            std::string buffer(1024, '\0');
            if (client.recieveMessageWithTimeout(buffer.data(), buffer.length(), 1) < 0) {
                std::cerr << "Error receiving message" << std::endl;
            }
            else {
                std::cout << "Received message from server: " << buffer << std::endl;
            }
        }
    };

    std::vector<std::thread> threadPool;

    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");
    threadPool.emplace_back(MultiClientRequest, 8080, "127.0.0.1");

    for (std::thread& t : threadPool) {
        t.join();
    }

    return 0;
}