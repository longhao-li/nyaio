#include "nyaio/json.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("JSON error message") {
    auto &category = json_error_category();

    CHECK(category.name() == std::string_view("json"));
    CHECK(json_error(json_errc::ok).what() != std::string_view("Unknown JSON error"));
    CHECK(json_error(json_errc::invalid_type).what() != std::string_view("Unknown JSON error"));

    // bad error code should return the unknown error message
    CHECK(category.message(-1) == std::string_view("Unknown JSON error"));
}
