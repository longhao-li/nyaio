#include "nyaio/json.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

namespace {

struct some_complex_type {
    std::nullptr_t null;
    bool boolean;
    std::string name;
    std::map<std::string, std::vector<int>> data;
    std::vector<std::vector<uint32_t>> matrix;
    float floating_point;
};

} // namespace

template <>
struct nyaio::json_serializer<some_complex_type> {
    static auto to_json(const some_complex_type &value, json_element j) noexcept -> json_errc {
        j.set_object();
        auto object = static_cast<json_object>(j);

        object["null"]           = value.null;
        object["boolean"]        = value.boolean;
        object["name"]           = value.name;
        object["data"]           = value.data;
        object["matrix"]         = value.matrix;
        object["floating_point"] = value.floating_point;

        return json_errc::ok;
    }
};

TEST_CASE("JSON document clone complex type") {
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
        .floating_point = 3.1415f,
    };

    json_document doc;
    doc.root() = data;

    json_document cloned(doc.root());
    CHECK(cloned.root().is_object());
    CHECK(cloned.dump() == doc.dump());
    CHECK(cloned.dump("  ") == doc.dump("  "));

    json_document moved(std::move(cloned));
    CHECK(moved.root().is_object());
    CHECK(moved.dump() == doc.dump());
    CHECK(moved.dump("  ") == doc.dump("  "));

    json_document assigned;
    CHECK(assigned.root().is_null());

    assigned = std::move(moved);
    CHECK(assigned.root().is_object());
    CHECK(assigned.dump() == doc.dump());
    CHECK(assigned.dump("  ") == doc.dump("  "));

    // self assignment
    assigned = std::move(assigned);
    CHECK(assigned.root().is_object());
    CHECK(assigned.dump() == doc.dump());
    CHECK(assigned.dump("  ") == doc.dump("  "));
}
