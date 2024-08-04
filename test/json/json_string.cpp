#include "nyaio/json.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("JSON string huge string") {
    constexpr std::string_view str_unit = "Hello, world!";

    std::string str;
    str.reserve(65536 * str_unit.size());
    for (size_t i = 0; i < 65536; ++i)
        str.append(str_unit);

    json_document doc;
    json_element root = doc.root();
    root.set_string(str);

    CHECK(root.is_string());
    CHECK(*root.string() == str);

    root.set_string(str_unit);
    CHECK(root.is_string());
    CHECK(*root.string() != str);
    CHECK(*root.string() == str_unit);
}
