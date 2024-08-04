#include "nyaio/json.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("JSON dump null") {
    json_document doc;
    json_element root = doc.root();
    root.set_null();

    CHECK(root.dump() == "null");
    CHECK(root.dump("  ") == "null");
}

TEST_CASE("JSON dump boolean") {
    json_document doc;
    json_element root = doc.root();

    root.set_boolean(true);
    CHECK(root.dump() == "true");
    CHECK(root.dump("  ") == "true");

    root.set_boolean(false);
    CHECK(root.dump() == "false");
    CHECK(root.dump("  ") == "false");
}

TEST_CASE("JSON dump object") {
    json_document doc;
    json_element root = doc.root();

    root.set_object();
    CHECK(root.dump() == "{}");
    CHECK(root.dump("  ") == "{\n}");
}

TEST_CASE("JSON dump array") {
    json_document doc;
    json_element root = doc.root();

    root.set_array();
    CHECK(root.dump() == "[]");
    CHECK(root.dump("  ") == "[\n]");
}

TEST_CASE("JSON dump string") {
    json_document doc;
    json_element root = doc.root();

    root.set_string("Hello, world!\n");
    CHECK(root.dump() == "\"Hello, world!\\n\"");
    CHECK(root.dump("  ") == "\"Hello, world!\\n\"");
}

TEST_CASE("JSON dump integer") {
    json_document doc;
    json_element root = doc.root();

    root.set_integer(1234);
    CHECK(root.dump() == "1234");
    CHECK(root.dump("  ") == "1234");

    root.set_integer(-5678);
    CHECK(root.dump() == "-5678");
    CHECK(root.dump("  ") == "-5678");
}

TEST_CASE("JSON dump floating point") {
    json_document doc;
    json_element root = doc.root();

    root.set_floating_point(1.375);
    CHECK(root.dump() == "1.375");
    CHECK(root.dump("  ") == "1.375");

    root.set_floating_point(-0.125);
    CHECK(root.dump() == "-0.125");
    CHECK(root.dump("  ") == "-0.125");
}

namespace {

struct some_complex_type {
    std::string name;
    std::map<std::string, std::vector<int>> data;
    std::vector<std::vector<uint32_t>> matrix;
};

} // namespace

template <>
struct nyaio::json_serializer<some_complex_type> {
    static auto to_json(const some_complex_type &value, json_element j) noexcept -> json_errc {
        j.set_object();
        auto object = static_cast<json_object>(j);

        object["name"]   = value.name;
        object["data"]   = value.data;
        object["matrix"] = value.matrix;

        return json_errc::ok;
    }
};

TEST_CASE("JSON dump complex type") {
    some_complex_type data = {
        .name = "some complex type",
        .data =
            {
                {"first", {1, 2, 3}},
                {"second", {4, 5, 6}},
            },
        .matrix =
            {
                {1, 2, 3, 4, 5},
                {6, 7, 8, 9, 10},
            },
    };

    static constexpr std::string_view complex_value_text =
        R"({"data":{"first":[1,2,3],"second":[4,5,6]},"matrix":[[1,2,3,4,5],[6,7,8,9,10]],"name":"some complex type"})";

    static constexpr std::string_view complex_value_indented_text =
        R"({
  "data": {
    "first": [
      1,
      2,
      3
    ],
    "second": [
      4,
      5,
      6
    ]
  },
  "matrix": [
    [
      1,
      2,
      3,
      4,
      5
    ],
    [
      6,
      7,
      8,
      9,
      10
    ]
  ],
  "name": "some complex type"
})";

    json_document doc;
    json_element root = doc.root();
    root              = data;

    CHECK(doc.dump() == complex_value_text);
    CHECK(doc.dump("  ") == complex_value_indented_text);
}
