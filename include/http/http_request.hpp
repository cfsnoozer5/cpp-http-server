#pragma once
#include <string>
#include <sstream>

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

HttpRequest parse_request(const std::string &raw)
{
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    // Should be space seperated so should work
    if (std::getline(stream, line))
    {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path >> req.version;
    }

    while (std::getline(stream, line) && line != "\r") {
        auto colon = line.find(':');
        // Header location
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim all whitespace aroung value
            value.erase(0, value.find_first_not_of(" \t"));
            if (!value.empty() && value.back() == '\n')
                value.pop_back();

            req.headers[key] = value;
        }
    }

    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end()) {
        size_t length = std::stoul(it->second);
        req.body.resize(length);
        stream.read(&req.body[0], length);
    }

    return req;
}