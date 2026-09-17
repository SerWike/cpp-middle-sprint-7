#include "headers.h"

#include <algorithm>
#include <format>
#include <ranges>
#include <string_view>
#include <utility>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback &&callback) {
    auto is_space = [](char c) { return std::isspace(static_cast<unsigned char>(c)); };
    auto trim = [&is_space](std::string_view s) {
        if (s.empty())
            return s;
        auto first = std::ranges::find_if_not(s, is_space);
        if (first == s.end())
            first = s.begin();

        auto last = std::ranges::find_if(first + 1, s.end(), is_space);

        return std::string_view(first, last);
    };
    auto to_lower = [](std::string_view str) {
        std::string result;
        result.resize(str.size());
        std::transform(str.begin(), str.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    };

    auto headers_start = req.find("\r\n");
    if (headers_start == std::string_view::npos)
        return;

    auto headers_end = req.find("\r\n\r\n", headers_start);
    if (headers_end == std::string_view::npos || headers_start == headers_end)
        return;

    for (const auto &line_subrange :
         req.substr(headers_start + 2, headers_end - headers_start - 2) | std::views::split(std::string_view("\r\n"))) {
        if (line_subrange.empty())
            continue;
        std::string_view line(line_subrange.begin(), line_subrange.end());

        auto delim_pos = line.find(':');
        if (delim_pos == std::string_view::npos)
            continue;

        std::string_view key = trim(line.substr(0, delim_pos));
        std::string_view val = trim(line.substr(delim_pos + 1));

        callback(to_lower(key), to_lower(val));
    }
}

bool checkPort(std::string_view port) {
    if (port.empty())
        return false;

    std::uint32_t value = 0;
    auto [ptr, ec] = std::from_chars(port.begin(), port.end(), value);

    if (ec != std::errc() || ptr != port.end())
        return false;

    if (value < 1 || value > 65535)
        return false;

    return true;
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::string host = "";
    iterHeaders(req, [&host](std::string_view key, std::string_view val) {
        if (key == "host")
            host = val;
    });

    if (host.empty())
        throw std::runtime_error("Request without 'Host'");

    auto delim = host.find(':');
    if (delim == std::string_view::npos)
        return std::make_pair(std::string(host), "80");

    auto port_str = host.substr(delim + 1);
    if (!checkPort(port_str))
        throw std::runtime_error(std::format("Invalid port: {}", port_str));

    return std::make_pair(std::string(host.substr(0, delim)), port_str);
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    auto convert_to_number = [](std::string_view num) -> std::optional<size_t> {
        if (num.empty())
            return std::nullopt;

        size_t value = 0;

        auto [ptr, err] = std::from_chars(num.begin(), num.end(), value);
        if (err == std::errc())
            return value;
        return std::nullopt;
    };

    std::string length = "";
    iterHeaders(rsp, [&length](std::string_view key, std::string_view val) {
        if (key == "content-length")
            length = val;
    });

    return convert_to_number(length);
}
