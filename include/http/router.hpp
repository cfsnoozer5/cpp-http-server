#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <queue>

#include "http_request.hpp"

using Params = std::unordered_map<std::string, std::string>;
using HttpHandler = std::function<std::string(HttpRequest &, Params &)>;

struct Route
{
    std::string method;
    std::string pattern;
    HttpHandler handler;
};

struct RouteNode
{
    std::unordered_map<std::string, std::unique_ptr<RouteNode>> children;
    std::string param_name;
    std::unordered_map<std::string, HttpHandler> handlers;
};

class Router
{
private:
    RouteNode trie = {};

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

    Params parse_params(const std::string &path, const std::string &requestPath)
    {
        Params params;

        auto request_split = split_path(requestPath);
        auto path_split = split_path(path);
        
        for (auto i = 0; i < request_split.size(); i++)
        {
            if (path_split[i][0] == ':') {
                params[path_split[i].substr(1)] = request_split[i];
            }
        }

        return params;
    }

    struct QueueItem {
        RouteNode* node;
        size_t segment_index;
        std::string route;
    };
public:
    void add_route(
        const std::string &method,
        const std::string &pattern,
        HttpHandler handler)
    {
        auto pattern_segments = split_path(pattern);

        if (pattern_segments.size() == 0)
        {
            trie.handlers[method] = handler;
            return;
        }

        RouteNode *node = &trie;
        size_t segment_i = 0;
        std::string current_segment;
        while (segment_i < pattern_segments.size())
        {
            current_segment = pattern_segments[segment_i];
            auto it = node->children.find(current_segment);
            if (it == node->children.end())
            {
                node->children[current_segment] = std::make_unique<RouteNode>();
                node = node->children[current_segment].get();
                node->param_name = current_segment;
            }
            else
            {
                node = it->second.get();
            }

            if (++segment_i == pattern_segments.size())
            {
                node->handlers[method] = handler;
            }
        }
    }

    std::optional<std::pair<HttpHandler, Params>> match_route(const std::string &method, const std::string &path)
    {
        auto request_segments = split_path(path);

        std::queue<QueueItem> potentialRoutes;
        potentialRoutes.push({&trie, 0, "/"});

        while (!potentialRoutes.empty()) {
            auto item = potentialRoutes.front();
            
            RouteNode* node = item.node;
            size_t seg_i = item.segment_index;

            if (seg_i == request_segments.size()) {
                auto it = node->handlers.find(method);
                if (it != node->handlers.end()) {
                    Params params = parse_params(item.route, path);
                    return std::make_optional(std::make_pair(it->second, params));
                }
                continue;
            }

            const std::string& seg = request_segments[seg_i];

            auto child_it = node->children.find(seg);
            if (child_it != node->children.end()) {
                potentialRoutes.push({child_it->second.get(), seg_i + 1, item.route + "/" + seg});
            }

            for (auto& [key, child] : node -> children) {
                if (!child->param_name.empty() && child->param_name[0] == ':') {
                    potentialRoutes.push({child.get(), seg_i + 1, item.route + "/" + child->param_name});
                }
            }

            potentialRoutes.pop();
        }

        return std::nullopt;
    }

    void print()
    {
        std::queue<RouteNode *> q;
        q.push(&trie);

        while (!q.empty())
        {
            RouteNode *n = q.front();
            std::cout << "/" << n->param_name << std::endl;
            std::cout << "children: ";
            for (const auto &[segment, n] : n->children)
            {
                std::cout << segment << ", ";
                q.push(n.get());
            }

            q.pop();

            std::cout << std::endl;
        }
    }
};
