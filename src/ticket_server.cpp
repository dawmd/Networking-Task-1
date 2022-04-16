#include "common.h"
#include "database.h"
#include "networking.h"

#include <iostream>
#include <fstream>
#include <filesystem>

#include <cstdint>
#include <cstdlib> // std::size_t

#include <string>

constexpr std::size_t MAX_REQUEST_SIZE = 53;

struct ServerParameters {
    std::string filepath;
    int port;
    uint64_t timeout;
};

ServerParameters parse_parameters(int argc, char *argv[]) {
    return ServerParameters(); // TODO
}

void run(const ServerParameters &parameters) {
    char buffer[MAX_REQUEST_SIZE];
    memset(buffer, 0, sizeof(buffer));

    int socket_fd = bind_socket(parameters.port);
    struct sockaddr_in client_address;
    std::size_t read_length;

    Database db = load_database(parameters);

    while (true) {
        read_length = read_message(socket_fd, client_address, buffer, sizeof(buffer));
        if (!read_length)
            std::cerr << "The server has received an empty message. Ignoring.\n";
        else
            handle_request(db, buffer, read_length, socket_fd, client_address);
    }

    close(socket_fd);
}

int main(int argc, char *argv[]) {
    auto parameters = parse_parameters(argc - 1, &argv[1]);
    run(parameters);

    return 0;
}

