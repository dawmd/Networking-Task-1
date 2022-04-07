#include <iostream>
#include <optional>
#include <variant>

constexpr int DEFAULT_PORT = 2022;
constexpr int DEFAULT_TIMEOUT = 5;

struct ServerParams {
    std::string filename;
    int port;
    int timeout;

    ServerParams(std::string &&filename_, int port_ = DEFAULT_PORT, int timeout_ = DEFAULT_TIMEOUT)
    : filename(std::move(filename_))
    , port(port_)
    , timeout(timeout_) {}

    ~ServerParams() = default;
};

std::optional<ServerParams> get_params(int arg_count, char *args[]) {
    if (arg_count & 1 || arg_count > 6) {
        return {}; // empty
    }

    for (int i = 0; i < arg_count / 2; ++i) {
        if (args[2 * i] == )
    }
}

int main(int argc, char *argv[]) {
    auto params = get_params(argc - 1, &argv[1]);
    if (!params.has_value()) {
        exit(1);
    }
    return 0;
}