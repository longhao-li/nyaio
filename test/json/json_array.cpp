#include "nyaio/json.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("JSON array element access") {
    json_document doc;
    json_element root = doc.root();

    constexpr std::string_view element_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    root       = element_list;
    auto array = *root.array();

    CHECK(array.size() == std::size(element_list));

    CHECK(array.front().is_string());
    CHECK(*(array.front().string()) == "Alice");
    CHECK(static_cast<const json_array &>(array).front().is_string());
    CHECK(*(static_cast<const json_array &>(array).front().string()) == "Alice");

    CHECK(array.back().is_string());
    CHECK(*(array.back().string()) == "Walter");
    CHECK(static_cast<const json_array &>(array).back().is_string());
    CHECK(*(static_cast<const json_array &>(array).back().string()) == "Walter");

    for (size_t i = 0; i < std::size(element_list); ++i) {
        CHECK(array[i].is_string());
        CHECK(*array[i].string() == element_list[i]);
        CHECK(static_cast<const json_array &>(array)[i].is_string());
        CHECK(*static_cast<const json_array &>(array)[i].string() == element_list[i]);
    }
}

TEST_CASE("JSON array append") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    auto array = *root.array();

    CHECK(array.begin() == array.end());
    CHECK(array.rbegin() == array.rend());
    CHECK(array.empty());
    CHECK(array.size() == 0);

    constexpr size_t count = 1000;

    SUBCASE("push back") {
        for (size_t i = 0; i < count; ++i) {
            array.push_back(i);
            CHECK(array.size() == i + 1);
            CHECK(array.back().is_integer());
            CHECK(*array.back().integer() == i);
        }
    }

    CHECK(array.front().is_integer());
    CHECK(*array.front().integer() == 0);

    CHECK(array.back().is_integer());
    CHECK(*array.back().integer() == count - 1);

    for (size_t i = 0; i < count; ++i) {
        CHECK(array[i].is_integer());
        CHECK(*array[i].integer() == i);
    }

    CHECK(array.size() == count);
    CHECK(array.capacity() >= count);
}

TEST_CASE("JSON array insert") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    auto array = *root.array();

    constexpr size_t count = 1000;
    for (size_t i = 0; i < count; ++i)
        array.push_back(i);

    constexpr size_t insert_count = 100;

    SUBCASE("by iterator") {
        for (size_t i = 0; i < insert_count; ++i) {
            array.insert(array.begin(), i);
            CHECK(array.size() == count + i + 1);
            CHECK(array.front().is_integer());
            CHECK(*array.front().integer() == i);
        }
    }

    SUBCASE("by index") {
        for (size_t i = 0; i < insert_count; ++i) {
            array.insert(0, i);
            CHECK(array.size() == count + i + 1);
            CHECK(array.front().is_integer());
            CHECK(*array.front().integer() == i);
        }
    }

    SUBCASE("by emplace with iterator") {
        for (size_t i = 0; i < insert_count; ++i) {
            array.emplace(array.begin(), i);
            CHECK(array.size() == count + i + 1);
            CHECK(array.front().is_integer());
            CHECK(*array.front().integer() == i);
        }
    }

    SUBCASE("by emplace with index") {
        for (size_t i = 0; i < insert_count; ++i) {
            array.emplace(0, i);
            CHECK(array.size() == count + i + 1);
            CHECK(array.front().is_integer());
            CHECK(*array.front().integer() == i);
        }
    }

    CHECK(array.size() == count + insert_count);
}

TEST_CASE("JSON array erase") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    auto array = *root.array();

    constexpr size_t count = 1000;
    for (size_t i = 0; i < count; ++i)
        array.push_back(i);

    constexpr size_t erase_count = 100;

    SUBCASE("by iterator") {
        for (size_t i = 0; i < erase_count; ++i) {
            array.erase(array.begin());
            CHECK(array.size() == count - i - 1);
            CHECK(array.front().is_integer());
            CHECK(*array.front().integer() == i + 1);
        }
    }

    SUBCASE("range") {
        array.erase(array.begin(), array.begin() + erase_count);
    }

    CHECK(array.size() == count - erase_count);
    for (size_t i = 0; i < count - erase_count; ++i) {
        CHECK(array[i].is_integer());
        CHECK(*array[i].integer() == i + erase_count);
    }
}

TEST_CASE("JSON array clear") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    auto array = *root.array();

    constexpr size_t count = 1000;
    for (size_t i = 0; i < count; ++i)
        array.push_back(i);

    array.clear();

    CHECK(array.empty());
    CHECK(array.size() == 0);
}

TEST_CASE("JSON array pop back") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    auto array = *root.array();

    constexpr size_t count = 1000;
    for (size_t i = 0; i < count; ++i)
        array.push_back(i);

    constexpr size_t pop_count = 100;

    for (size_t i = 0; i < pop_count; ++i) {
        array.pop_back();
        CHECK(array.size() == count - i - 1);
        CHECK(array.back().is_integer());
        CHECK(*array.back().integer() == count - i - 2);
    }

    CHECK(array.size() == count - pop_count);
    for (size_t i = 0; i < count - pop_count; ++i) {
        CHECK(array[i].is_integer());
        CHECK(*array[i].integer() == i);
    }
}

TEST_CASE("JSON array reserve") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    auto array = *root.array();

    constexpr size_t count = 1000;
    for (size_t i = 0; i < count; ++i)
        array.push_back(i);

    size_t capacity = array.capacity();

    SUBCASE("smaller") {
        array.reserve(0);
        CHECK(array.capacity() == capacity);
    }

    SUBCASE("larger") {
        array.reserve(capacity * 2);
        CHECK(array.capacity() == capacity * 2);
    }
}

TEST_CASE("JSON array resize") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    auto array = *root.array();

    constexpr size_t count = 1000;
    for (size_t i = 0; i < count; ++i)
        array.push_back(i);

    SUBCASE("smaller") {
        constexpr size_t new_size = 500;

        array.resize(new_size);
        CHECK(array.size() == new_size);

        for (size_t i = 0; i < new_size; ++i) {
            CHECK(array[i].is_integer());
            CHECK(*array[i].integer() == i);
        }
    }

    SUBCASE("larger") {
        constexpr size_t new_size = 1500;

        array.resize(new_size);
        CHECK(array.size() == new_size);

        for (size_t i = 0; i < count; ++i) {
            CHECK(array[i].is_integer());
            CHECK(*array[i].integer() == i);
        }

        for (size_t i = count; i < new_size; ++i)
            CHECK(array[i].is_null());
    }
}

TEST_CASE("JSON array iterator") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    constexpr std::string_view element_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    root       = element_list;
    auto array = *root.array();

    CHECK(array.size() == std::size(element_list));

    SUBCASE("iterator element access") {
        auto iter = array.begin();
        CHECK(iter->is_string());
        CHECK(*(*iter).string() == "Alice");

        // random access element
        for (size_t i = 0; i < std::size(element_list); ++i) {
            CHECK(*iter[i].string() == element_list[i]);
            CHECK(*(iter + i)->string() == element_list[i]);
        }
    }

    SUBCASE("operator++()") {
        size_t index = 0;
        for (auto iter = array.begin(); iter != array.end(); ++iter, ++index) {
            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator++(int)") {
        size_t index = 0;
        for (auto iter = array.begin(); iter != array.end(); iter++, index++) {
            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator--()") {
        size_t index = std::size(element_list);
        for (auto iter = array.end(); iter != array.begin();) {
            --iter;
            --index;

            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator--(int)") {
        size_t index = std::size(element_list);
        for (auto iter = array.end(); iter != array.begin();) {
            iter--;
            index--;

            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator+=()") {
        size_t index = 0;
        for (auto iter = array.begin(); iter != array.end(); iter += 1, index += 1) {
            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator-=()") {
        size_t index = std::size(element_list);
        for (auto iter = array.end(); iter != array.begin();) {
            iter  -= 1;
            index -= 1;

            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator+()") {
        size_t index = 0;
        for (auto iter = array.begin(); iter + index != array.end(); index += 1) {
            CHECK((iter + index)->is_string());
            CHECK(*(iter + index)->string() == element_list[index]);
        }
    }

    SUBCASE("operator-()") {
        size_t index = 0;
        for (auto iter = array.end(); iter - index != array.begin(); ++index) {
            CHECK((iter - index - 1)->is_string());
            CHECK(*(iter - index - 1)->string() ==
                  element_list[std::size(element_list) - index - 1]);
        }
    }

    SUBCASE("arithmetic") {
        CHECK(array.end() - array.begin() == std::size(element_list));
    }
}

TEST_CASE("JSON array const iterator") {
    json_document doc;
    json_element root = doc.root();
    root.set_array();

    constexpr std::string_view element_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    root       = element_list;
    auto array = *root.array();

    CHECK(array.size() == std::size(element_list));

    SUBCASE("iterator element access") {
        auto iter = array.cbegin();
        CHECK(iter->is_string());
        CHECK(*(*iter).string() == "Alice");

        // random access element
        for (size_t i = 0; i < std::size(element_list); ++i) {
            CHECK(*iter[i].string() == element_list[i]);
            CHECK(*(iter + i)->string() == element_list[i]);
        }
    }

    SUBCASE("operator++()") {
        size_t index = 0;
        for (auto iter = array.cbegin(); iter != array.cend(); ++iter, ++index) {
            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator++(int)") {
        size_t index = 0;
        for (auto iter = array.cbegin(); iter != array.cend(); iter++, index++) {
            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator--()") {
        size_t index = std::size(element_list);
        for (auto iter = array.cend(); iter != array.cbegin();) {
            --iter;
            --index;

            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator--(int)") {
        size_t index = std::size(element_list);
        for (auto iter = array.cend(); iter != array.cbegin();) {
            iter--;
            index--;

            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator+=()") {
        size_t index = 0;
        for (auto iter = array.cbegin(); iter != array.cend(); iter += 1, index += 1) {
            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator-=()") {
        size_t index = std::size(element_list);
        for (auto iter = array.cend(); iter != array.cbegin();) {
            iter  -= 1;
            index -= 1;

            CHECK(iter->is_string());
            CHECK(*iter->string() == element_list[index]);
        }
    }

    SUBCASE("operator+()") {
        size_t index = 0;
        for (auto iter = array.cbegin(); iter + index != array.cend(); index += 1) {
            CHECK((iter + index)->is_string());
            CHECK(*(iter + index)->string() == element_list[index]);
        }
    }

    SUBCASE("operator-()") {
        size_t index = 0;
        for (auto iter = array.cend(); iter - index != array.cbegin(); ++index) {
            CHECK((iter - index - 1)->is_string());
            CHECK(*(iter - index - 1)->string() ==
                  element_list[std::size(element_list) - index - 1]);
        }
    }

    SUBCASE("arithmetic") {
        CHECK(array.cend() - array.cbegin() == std::size(element_list));
    }
}
