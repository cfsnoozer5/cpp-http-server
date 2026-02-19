#include <sys/socket.h>
#include <csignal>
#include <atomic>
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h>  // htons, htonl
#include <unistd.h>     // close
#include <sstream>
#include <string>
#include <iostream>
#include <set>

#include "threadpool.hpp"
#include "http/router.hpp"
#include "http/http_request.hpp"

std::atomic<bool> server_running(true);
int server_socket;

std::mutex clients_mutex;
std::set<int> active_clients;

Router router;

std::string read_http_request(int client_socket)
{
    char buffer[4096];
    std::string request;
    ssize_t bytes;

    while ((bytes = read(client_socket, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes] = '\0';
        request += buffer;
        if (request.find("\r\n\r\n") != std::string::npos)
        {
            break;
        }
    }

    return request;
}

std::string make_response(const std::string &body, int status_code = 200)
{
    std::ostringstream response;
    response << "HTTP/1.1 ";
    switch (status_code)
    {
    case 200:
        response << status_code << " OK\r\n";
        break;
    case 404:
        response << status_code << " NOT FOUND\r\n";
        break;
    default:
        response << 500 << " INTERNAL SERVER ERROR\r\n";
    }

    response << "Content-Length: " << body.size() << "\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    return response.str();
}

void handle_client(int client_socket)
{
    std::string request_text = read_http_request(client_socket);
    HttpRequest req = parse_request(request_text);

    std::cout << "Request: " << req.method << " " << req.path << std::endl;
    for (auto header: req.headers) {
        auto& [key, value] = header;
        std::cout << key << ": " << value << std::endl;
    }
    std::cout << "Body: " << req.body << std::endl;

    std::string body = "<h1>404 Not Found</h1>";
    int status = 404;

    auto match = router.match_route(req.method, req.path);
    if (match)
    {
        auto& [handler, params] = *match;
        body = handler(req, params);
        status = 200;
    }

    std::string response = make_response(body, status);
    send(client_socket, response.c_str(), response.size(), 0);

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        active_clients.extract(client_socket);
    }

    close(client_socket);
}

void signal_handler(int signum)
{
    std::cout << "\nSignal (" << signum << ") recieved, shutting down..." << std::endl;
    server_running = false;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (int sock : active_clients)
        {
            shutdown(sock, SHUT_RDWR);
            close(sock);
        }

        active_clients.clear();
    }
    shutdown(server_socket, SHUT_RDWR); // unblocks accept()
}

int main()
{
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        std::cerr << "Error creating socket" << std::endl;
        ;
        return 1;
    }

    // Allow reuse should be fine, not always ideal
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "setsockopt failed\n";
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // ALL INTERFACES
    server_addr.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Error binding socket" << std::endl;
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) < 0)
    {
        std::cerr << "Error listening on socket" << std::endl;
        close(server_socket);
        return 1;
    }

    std::cout << "Server listening on port 8080..." << std::endl;

    ThreadPool pool(4);

    router.add_route(
        "GET",
        "/",
        [](HttpRequest&, Params&)
        {
            return "<h1>Welcome Home!</h1>";
        });

    router.add_route(
        "GET",
        "/hello",
        [](HttpRequest&, Params&)
        {
            return "<h1>Hello World!</h1>";
        });

    router.add_route(
        "GET",
        "/:id",
        [](HttpRequest&, Params& params)
        {
            return "<h1>" + params.at("id") + "</h1>";
        });

    std::signal(SIGINT, signal_handler);
    while (server_running)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0)
        {
            if (!server_running)
                break;
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        std::cout << "New connection accepted" << std::endl;
        pool.enqueue([client_socket]()
                     {
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                active_clients.insert(client_socket);
            }
            std::cout << "Handling client" << std::endl;
            handle_client(client_socket); });
    }

    pool.shutdown();
    std::cout << "Thread pool closed." << std::endl;

    close(server_socket);
    std::cout << "Server socket closed. Exiting." << std::endl;

    return 0;
}