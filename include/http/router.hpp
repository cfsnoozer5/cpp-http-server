#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>

#include "http_request.hpp"

using Params = std::unordered_map<std::string, std::string>;
using HttpHandler = std::function<std::string(HttpRequest&, Params&)>;

struct Route
{
    std::string method;
    std::string pattern;
    HttpHandler handler;
};

class Router
{
private:
    std::vector<Route> routes;

public:
    void add_route(
        const std::string &method,
        const std::string &pattern,
        HttpHandler handler)
    {
        routes.push_back({method, pattern, handler});
    }

    std::vector<std::string> split_path(const std::string &path)
    {
        std::vector<std::string> segments;
        std::stringstream ss(path);
        std::string segment;

        while (std::getline(ss, segment, '/'))
        {
            if (!segment.empty())
                segments.push_back(segment);
        }

        return segments;
    }


    // Note: Needs to be ordered properly, so if /hello and /:id. /hello must be added before /:id or /:id will always be called
    std::optional<std::pair<HttpHandler, Params>> match_route(const std::string &method, const std::string &path)
    {
        auto request_segments = split_path(path);

        for (const auto &route : routes)
        {
            if (route.method != method)
                continue;

            auto pattern_segments = split_path(route.pattern);

            if (pattern_segments.size() != request_segments.size())
                continue;

            Params params;
            bool matched = true;

            for (size_t i = 0; i < pattern_segments.size(); i++)
            {
                const auto &pattern = pattern_segments[i];
                const auto &value = request_segments[i];

                if (!pattern.empty() && pattern[0] == ':')
                {
                    params[pattern.substr(1)] = value;
                }
                else if (pattern != value)
                {
                    matched = false;
                    break;
                }
            }

            if (matched)
            {
                return std::make_optional(std::make_pair(route.handler, params));
            }
        }

        return std::nullopt;
    }
};
