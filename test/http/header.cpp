#include "nyaio/http.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("http header insert element") {
    http_header header;

    CHECK(header.begin() == header.end());
    CHECK(header.cbegin() == header.cend());
    CHECK(header.rbegin() == header.rend());
    CHECK(header.crbegin() == header.crend());
    CHECK(header.empty());
    CHECK(header.size() == 0);

    SUBCASE("insert") {
        auto item = std::make_pair<std::string, std::string>("Connection", "keep-alive");

        header.insert(item);
        header.insert({"Accept", "text/html"});
        header.insert("cOntEnT-leNgTh", "1024");
    }

    SUBCASE("emplace") {
        auto item = std::make_pair<std::string, std::string>("Connection", "keep-alive");

        header.emplace(item);
        header.emplace({"Accept", "text/html"});
        header.emplace("cOntEnT-leNgTh", "1024");
    }

    CHECK(header.size() == 3);
    CHECK(header.insert("connection", "close").second == false);

    CHECK(header.contains("CoNnEcTiOn"));
    CHECK(header.count("CoNnEcTiOn") == 1);
    CHECK(header.find("connection") != header.end());
    CHECK(header.find("connection")->second == "keep-alive");
    CHECK(header["connection"] == "keep-alive");

    CHECK(header.contains("accept"));
    CHECK(header.count("accept") == 1);
    CHECK(header.find("accept") != header.end());
    CHECK(header.find("accept")->second == "text/html");
    CHECK(header["accept"] == "text/html");

    CHECK(header.contains("content-length"));
    CHECK(header.count("content-length") == 1);
    CHECK(header.find("content-length") != header.end());
    CHECK(header.find("content-length")->second == "1024");
    CHECK(header["content-length"] == "1024");

    CHECK(header.contains("Content-Type") == false);
    CHECK(header.count("Content-Type") == 0);
    CHECK(header.find("Content-Type") == header.end());
}

TEST_CASE("http header erase element") {
    http_header header;

    header.insert("Connection", "keep-alive");
    header.insert("Accept", "text/html");
    header.insert("cOntEnT-leNgTh", "1024");

    SUBCASE("by iterator") {
        auto it = header.find("connection");
        header.erase(it);
    }

    SUBCASE("by key") {
        CHECK(header.erase("connection") == 1);
        CHECK(header.erase("content-type") == 0);
    }

    CHECK(header.size() == 2);
    CHECK(!header.contains("connection"));
    CHECK(header.count("connection") == 0);
    CHECK(header.find("connection") == header.end());
}

TEST_CASE("http header clear") {
    http_header header;

    header.insert("Connection", "keep-alive");
    header.insert("Accept", "text/html");
    header.insert("cOntEnT-leNgTh", "1024");

    CHECK(header.size() == 3);
    CHECK(!header.empty());

    SUBCASE("by clear") {
        header.clear();
    }

    SUBCASE("by erase") {
        while (!header.empty())
            header.erase(header.begin());
    }

    CHECK(header.size() == 0);
    CHECK(header.empty());
}
