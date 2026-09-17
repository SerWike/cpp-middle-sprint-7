#include "headers.h"
#include <gtest/gtest.h>

TEST(iterHeaders, Empty) {
    int count = 0;
    ASSERT_NO_THROW(iterHeaders("", [&count](std::string_view key, std::string_view val) { ++count; }));

    ASSERT_EQ(0, count);
}

TEST(iterHeaders, SkipRequestLine) {
    std::string_view req = "GET /index.html HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "\r\n";

    int count = 0;
    std::vector<std::pair<std::string_view, std::string_view>> headers;

    iterHeaders(req, [&](std::string_view key, std::string_view val) {
        ++count;
        headers.emplace_back(key, val);
    });

    ASSERT_EQ(1, count);

    ASSERT_EQ(1, headers.size());
    EXPECT_EQ("host", headers[0].first);
    EXPECT_EQ("example.com", headers[0].second);
}

TEST(iterHeaders, SingleHeader) {
    std::string_view req = "GET /index.html HTTP/1.1\r\n"
                           "    Host    :   example.com   \r\n"
                           "\r\n";

    std::vector<std::pair<std::string_view, std::string_view>> headers;

    iterHeaders(req, [&](std::string_view key, std::string_view val) { headers.emplace_back(key, val); });

    ASSERT_EQ(1, headers.size());
    EXPECT_EQ("host", headers[0].first);
    EXPECT_EQ("example.com", headers[0].second);
}

TEST(iterHeaders, MultipleHeaders) {
    std::string_view req = "GET /index.html HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: curl/8.0\r\n"
                           "Accept: */*\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: 0\r\n"
                           "\r\n";

    std::vector<std::pair<std::string, std::string>> headers;

    iterHeaders(req, [&](std::string_view key, std::string_view val) { headers.emplace_back(key, val); });

    ASSERT_EQ(5, headers.size());

    EXPECT_EQ("host", headers[0].first);
    EXPECT_EQ("example.com", headers[0].second);

    EXPECT_EQ("user-agent", headers[1].first);
    EXPECT_EQ("curl/8.0", headers[1].second);

    EXPECT_EQ("accept", headers[2].first);
    EXPECT_EQ("*/*", headers[2].second);

    EXPECT_EQ("content-type", headers[3].first);
    EXPECT_EQ("application/json", headers[3].second);

    EXPECT_EQ("content-length", headers[4].first);
    EXPECT_EQ("0", headers[4].second);
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::string_view req = "GET /index.html HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: curl/8.0\r\n"
                           "Accept: */*\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: 0\r\n"
                           "Accept: *.zip\r\n"
                           "\r\n";

    std::map<std::string, std::string> headers;

    iterHeaders(req, [&](std::string_view key, std::string_view val) {
        std::string str_key = std::string(key);
        if (!headers.contains(str_key))
            headers[str_key] = "";
        headers[str_key] = headers[str_key] + val;
    });

    ASSERT_EQ(5, headers.size());

    ASSERT_TRUE(headers.contains("host"));
    EXPECT_EQ("example.com", headers["host"]);

    ASSERT_TRUE(headers.contains("user-agent"));
    EXPECT_EQ("curl/8.0", headers["user-agent"]);

    ASSERT_TRUE(headers.contains("accept"));
    EXPECT_EQ("*/**.zip", headers["accept"]);

    ASSERT_TRUE(headers.contains("content-type"));
    EXPECT_EQ("application/json", headers["content-type"]);

    ASSERT_TRUE(headers.contains("content-length"));
    EXPECT_EQ("0", headers["content-length"]);
}

TEST(findHostPort, Simple) {
    std::string_view req = "GET /index.html HTTP/1.1\r\n"
                           "Host: example.com:85\r\n"
                           "User-Agent: curl/8.0\r\n"
                           "Accept: */*\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: 0\r\n"
                           "\r\n";
    std::pair<std::string, std::string> host_and_port;
    ASSERT_NO_THROW(host_and_port = findHostPort(req));

    ASSERT_EQ("example.com", host_and_port.first);
    ASSERT_EQ("85", host_and_port.second);
}

TEST(findHostPort, NoHost) {
    std::string_view req = "GET /index.html HTTP/1.1\r\n"
                           "User-Agent: curl/8.0\r\n"
                           "Accept: */*\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: 0\r\n"
                           "\r\n";
    std::pair<std::string, std::string> host_and_port;
    ASSERT_THROW(host_and_port = findHostPort(req), std::runtime_error);
}

TEST(findContentLength, Simple) {
    std::string_view req = "GET /index.html HTTP/1.1\r\n"
                           "Host: example.com:85\r\n"
                           "User-Agent: curl/8.0\r\n"
                           "Accept: */*\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: 123\r\n"
                           "\r\n";

    auto content_length = findContentLength(req);

    ASSERT_TRUE(content_length.has_value());
    ASSERT_EQ(123, content_length.value());
}

TEST(findContentLength, NoContentLength) {
    std::string_view req = "GET /index.html HTTP/1.1\r\n"
                           "Host: example.com:85\r\n"
                           "User-Agent: curl/8.0\r\n"
                           "Accept: */*\r\n"
                           "Content-Type: application/json\r\n"
                           "\r\n";

    auto content_length = findContentLength(req);

    ASSERT_FALSE(content_length.has_value());
}
