#include "nyaio/json.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("JSON null element") {
    json_document doc;
    json_element root = doc.root();

    root.set_null();

    CHECK(root.is_null());
    CHECK(!root.is_boolean());
    CHECK(!root.is_object());
    CHECK(!root.is_array());
    CHECK(!root.is_string());
    CHECK(!root.is_integer());
    CHECK(!root.is_floating_point());
    CHECK(!root.is_number());

    CHECK(!root.boolean().has_value());
    CHECK(!root.object().has_value());
    CHECK(!root.array().has_value());
    CHECK(!root.string().has_value());
    CHECK(!root.integer().has_value());
    CHECK(!root.floating_point().has_value());

    std::nullptr_t value;
    CHECK(root.to(value) == json_errc::ok);
}

TEST_CASE("JSON boolean element") {
    json_document doc;
    json_element root = doc.root();

    root.set_boolean(true);

    CHECK(!root.is_null());
    CHECK(root.is_boolean());
    CHECK(!root.is_object());
    CHECK(!root.is_array());
    CHECK(!root.is_string());
    CHECK(!root.is_integer());
    CHECK(!root.is_floating_point());
    CHECK(!root.is_number());

    CHECK(root.boolean().has_value());
    CHECK(!root.object().has_value());
    CHECK(!root.array().has_value());
    CHECK(!root.string().has_value());
    CHECK(!root.integer().has_value());
    CHECK(!root.floating_point().has_value());

    bool value = false;
    CHECK(root.to(value) == json_errc::ok);
    CHECK(value == true);
}

TEST_CASE("JSON object element") {
    json_document doc;
    json_element root = doc.root();

    root.set_object();

    CHECK(!root.is_null());
    CHECK(!root.is_boolean());
    CHECK(root.is_object());
    CHECK(!root.is_array());
    CHECK(!root.is_string());
    CHECK(!root.is_integer());
    CHECK(!root.is_floating_point());
    CHECK(!root.is_number());

    CHECK(!root.boolean().has_value());
    CHECK(root.object().has_value());
    CHECK(!root.array().has_value());
    CHECK(!root.string().has_value());
    CHECK(!root.integer().has_value());
    CHECK(!root.floating_point().has_value());
}

TEST_CASE("JSON array element") {
    json_document doc;
    json_element root = doc.root();

    root.set_array();

    CHECK(!root.is_null());
    CHECK(!root.is_boolean());
    CHECK(!root.is_object());
    CHECK(root.is_array());
    CHECK(!root.is_string());
    CHECK(!root.is_integer());
    CHECK(!root.is_floating_point());
    CHECK(!root.is_number());

    CHECK(!root.boolean().has_value());
    CHECK(!root.object().has_value());
    CHECK(root.array().has_value());
    CHECK(!root.string().has_value());
    CHECK(!root.integer().has_value());
    CHECK(!root.floating_point().has_value());
}

TEST_CASE("JSON string element") {
    json_document doc;
    json_element root = doc.root();

    root.set_string("hello");

    CHECK(!root.is_null());
    CHECK(!root.is_boolean());
    CHECK(!root.is_object());
    CHECK(!root.is_array());
    CHECK(root.is_string());
    CHECK(!root.is_integer());
    CHECK(!root.is_floating_point());
    CHECK(!root.is_number());

    CHECK(!root.boolean().has_value());
    CHECK(!root.object().has_value());
    CHECK(!root.array().has_value());
    CHECK(root.string().has_value());
    CHECK(!root.integer().has_value());
    CHECK(!root.floating_point().has_value());

    std::string value;
    CHECK(root.to(value) == json_errc::ok);
    CHECK(value == "hello");

    root.set_string("a");
    CHECK(root.is_string());
    CHECK(*root.string() == "a");
}

TEST_CASE("JSON integer element") {
    json_document doc;
    json_element root = doc.root();

    root.set_integer(42);

    CHECK(!root.is_null());
    CHECK(!root.is_boolean());
    CHECK(!root.is_object());
    CHECK(!root.is_array());
    CHECK(!root.is_string());
    CHECK(root.is_integer());
    CHECK(!root.is_floating_point());
    CHECK(root.is_number());

    CHECK(!root.boolean().has_value());
    CHECK(!root.object().has_value());
    CHECK(!root.array().has_value());
    CHECK(!root.string().has_value());
    CHECK(root.integer().has_value());
    CHECK(!root.floating_point().has_value());

    int value = 0;
    CHECK(root.to(value) == json_errc::ok);
    CHECK(value == 42);

    root.set_integer(0);
    CHECK(root.is_integer());
    CHECK(*root.integer() == 0);
}

TEST_CASE("JSON floating point element") {
    json_document doc;
    json_element root = doc.root();

    root.set_floating_point(3.14);

    CHECK(!root.is_null());
    CHECK(!root.is_boolean());
    CHECK(!root.is_object());
    CHECK(!root.is_array());
    CHECK(!root.is_string());
    CHECK(!root.is_integer());
    CHECK(root.is_floating_point());
    CHECK(root.is_number());

    CHECK(!root.boolean().has_value());
    CHECK(!root.object().has_value());
    CHECK(!root.array().has_value());
    CHECK(!root.string().has_value());
    CHECK(!root.integer().has_value());
    CHECK(root.floating_point().has_value());

    double value = 0.0;
    CHECK(root.to(value) == json_errc::ok);
    CHECK(value == 3.14);

    root.set_floating_point(0.0);
    CHECK(root.is_floating_point());
    CHECK(*root.floating_point() == 0.0);
}

TEST_CASE("JSON element type conversion") {
    json_document doc;
    json_element root = doc.root();

    root.convert_to(json_type::null);
    CHECK(root.type() == json_type::null);

    root.convert_to(json_type::boolean);
    CHECK(root.type() == json_type::boolean);

    root.convert_to(json_type::object);
    CHECK(root.type() == json_type::object);

    root.convert_to(json_type::array);
    CHECK(root.type() == json_type::array);

    root.convert_to(json_type::string);
    CHECK(root.type() == json_type::string);

    root.convert_to(json_type::integer);
    CHECK(root.type() == json_type::integer);

    root.convert_to(json_type::floating_point);
    CHECK(root.type() == json_type::floating_point);
}
