#include "nyaio/json.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

namespace {

struct foo {
    bool boolean;
    std::string string;
    int integer;
    double floating;
};

constexpr auto operator==(const foo &lhs, const foo &rhs) noexcept -> bool {
    return (lhs.boolean == rhs.boolean) && (lhs.string == rhs.string) &&
           (lhs.integer == rhs.integer) && (lhs.floating == rhs.floating);
}

struct bar {
    std::string name;
    std::vector<foo> foos;
};

} // namespace

template <>
struct nyaio::json_serializer<foo> {
    static auto to_json(const foo &value, json_element j) noexcept -> json_errc {
        j.set_object();
        auto object = *j.object();

        object.insert("boolean", value.boolean);
        object.insert("string", value.string);
        object.insert("integer", value.integer);
        object.insert("floating", value.floating);

        return json_errc::ok;
    }

    static auto from_json(foo &value, json_element j) noexcept -> json_errc {
        if (!j.is_object())
            return json_errc::invalid_type;

        auto object = *j.object();
        if (auto iter = object.find("boolean"); iter != object.end()) {
            if (auto ret = iter->second.to(value.boolean); ret != json_errc::ok)
                return ret;
        } else {
            return json_errc::invalid_type;
        }

        if (auto iter = object.find("string"); iter != object.end()) {
            if (auto ret = iter->second.to(value.string); ret != json_errc::ok)
                return ret;
        } else {
            return json_errc::invalid_type;
        }

        if (auto iter = object.find("integer"); iter != object.end()) {
            if (auto ret = iter->second.to(value.integer); ret != json_errc::ok)
                return ret;
        } else {
            return json_errc::invalid_type;
        }

        if (auto iter = object.find("floating"); iter != object.end()) {
            if (auto ret = iter->second.to(value.floating); ret != json_errc::ok)
                return ret;
        } else {
            return json_errc::invalid_type;
        }

        return json_errc::ok;
    }
};

template <>
struct nyaio::json_serializer<bar> {
    static auto to_json(const bar &value, json_element j) noexcept -> json_errc {
        j.set_object();
        auto object = *j.object();

        object.insert("name", value.name);
        object.insert("foos", value.foos);

        return json_errc::ok;
    }

    static auto from_json(bar &value, json_element j) noexcept -> json_errc {
        if (!j.is_object())
            return json_errc::invalid_type;

        auto object = *j.object();
        if (auto iter = object.find("name"); iter != object.end()) {
            if (auto ret = iter->second.to(value.name); ret != json_errc::ok)
                return ret;
        } else {
            return json_errc::invalid_type;
        }

        if (auto iter = object.find("foos"); iter != object.end()) {
            if (auto ret = iter->second.to(value.foos); ret != json_errc::ok)
                return ret;
        } else {
            return json_errc::invalid_type;
        }

        return json_errc::ok;
    }
};

TEST_CASE("JSON ADL serializer") {
    json_document doc;
    auto root = doc.root();

    constexpr auto count = 10;

    bar value;
    value.name = "bar";
    for (size_t i = 0; i < count; ++i)
        value.foos.push_back({i % 2 == 0, "foo", static_cast<int>(i), static_cast<double>(i)});

    root = value;
    CHECK(root.is_object());

    SUBCASE("to json") {
        auto object = *root.object();
        CHECK(object.contains("name"));
        CHECK(*object.find("name")->second.string() == value.name);

        CHECK(object.find("foos")->second.is_array());
        auto foos = *object.find("foos")->second.array();

        CHECK(foos.size() == count);
        for (size_t i = 0; i < count; ++i) {
            auto element = foos[i];
            CHECK(element.is_object());

            auto foo_obj = *element.object();
            CHECK(foo_obj.contains("boolean"));
            CHECK(*foo_obj.find("boolean")->second.boolean() == value.foos[i].boolean);

            CHECK(foo_obj.contains("string"));
            CHECK(*foo_obj.find("string")->second.string() == value.foos[i].string);

            CHECK(foo_obj.contains("integer"));
            CHECK(*foo_obj.find("integer")->second.integer() == value.foos[i].integer);

            CHECK(foo_obj.contains("floating"));
            CHECK(*foo_obj.find("floating")->second.floating_point() == value.foos[i].floating);
        }
    }

    SUBCASE("from json") {
        bar new_bar;
        CHECK(root.to(new_bar) == json_errc::ok);

        CHECK(new_bar.name == value.name);
        CHECK(new_bar.foos == value.foos);
    }

    SUBCASE("deserialization failed") {
        foo local_foo;
        CHECK(root.to(local_foo) != json_errc::ok);
        CHECK_THROWS_AS(local_foo = root, json_error &);
        CHECK_THROWS_AS(static_cast<bool>(root), json_error &);
        CHECK_THROWS_AS(static_cast<std::nullptr_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<int8_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<int16_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<int32_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<int64_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<uint8_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<uint16_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<uint32_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<uint64_t>(root), json_error &);
        CHECK_THROWS_AS(static_cast<float>(root), json_error &);
        CHECK_THROWS_AS(static_cast<double>(root), json_error &);
        CHECK_THROWS_AS(static_cast<std::string>(root), json_error &);
        CHECK_THROWS_AS(static_cast<std::string_view>(root), json_error &);
        CHECK_THROWS_AS(static_cast<std::vector<std::string>>(root), json_error &);
    }
}
