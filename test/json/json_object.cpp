#include "nyaio/json.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("JSON object insert") {
    json_document doc;
    json_element root = doc.root();
    root.set_object();

    auto object = static_cast<json_object>(root);

    CHECK(object.begin() == object.end());
    CHECK(object.rbegin() == object.rend());
    CHECK(object.empty());
    CHECK(object.size() == 0);

    constexpr size_t count = 1000;

    SUBCASE("by insert") {
        for (size_t i = 0; i < count; ++i) {
            std::string key = std::to_string(i);

            auto [iter, succeeded] = object.insert(key, i);
            CHECK(iter->first == key);
            CHECK(iter->second.is_integer());
            CHECK(*iter->second.integer() == i);

            CHECK(succeeded);
            CHECK(object.contains(key));
            CHECK(object.find(key) == iter);
        }
    }

    SUBCASE("by emplace") {
        for (size_t i = 0; i < count; ++i) {
            std::string key = std::to_string(i);

            auto [iter, succeeded] = object.emplace(key, i);
            CHECK(iter->first == key);
            CHECK(iter->second.is_integer());
            CHECK(*iter->second.integer() == i);

            CHECK(succeeded);
            CHECK(object.contains(key));
            CHECK(object.find(key) == iter);
        }
    }

    for (size_t i = 0; i < count; ++i) {
        std::string key = std::to_string(i);

        auto [iter, succeeded] = object.insert(key, i);
        CHECK(!succeeded);
        CHECK(iter->first == key);
        CHECK(iter->second.is_integer());
        CHECK(*iter->second.integer() == i);
    }

    std::string count_key = std::to_string(count);

    CHECK(object.size() == count);
    CHECK(!object.contains(count_key));
}

TEST_CASE("JSON object erase") {
    json_document doc;
    json_element root = doc.root();
    root.set_object();

    auto object = static_cast<json_object>(root);

    constexpr size_t count = 1000;

    for (size_t i = 0; i < count; ++i) {
        std::string key = std::to_string(i);

        auto [iter, succeeded] = object.insert(key, i);
        CHECK(succeeded);
    }

    SUBCASE("by key") {
        for (size_t i = 0; i < count; ++i) {
            std::string key = std::to_string(i);

            CHECK(object.contains(key));
            CHECK(object.erase(key) == 1);
            CHECK(!object.contains(key));
        }
    }

    SUBCASE("by iterator") {
        for (size_t i = 0; i < count; ++i) {
            std::string key = std::to_string(i);

            auto iter = object.find(key);
            CHECK(iter != object.end());

            object.erase(iter);
            CHECK(!object.contains(key));
        }
    }

    SUBCASE("range") {
        object.erase(object.begin(), object.end());
    }

    SUBCASE("clear") {
        object.clear();
    }

    CHECK(object.empty());
    CHECK(object.erase("0") == 0);
}

TEST_CASE("JSON object find") {
    json_document doc;
    json_element root = doc.root();
    root.set_object();

    constexpr std::string_view key_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    auto object = *root.object();
    for (size_t i = 0; i < std::size(key_list); ++i)
        object.insert(key_list[i], i);

    SUBCASE("find exists") {
        for (size_t i = 0; i < std::size(key_list); ++i) {
            auto iter = object.find(key_list[i]);
            CHECK(iter != object.end());
            CHECK(iter->first == key_list[i]);
            CHECK(static_cast<size_t>(iter->second) == i);
        }
    }

    SUBCASE("const find exists") {
        for (size_t i = 0; i < std::size(key_list); ++i) {
            auto iter = static_cast<const json_object &>(object).find(key_list[i]);
            CHECK(iter != object.end());
            CHECK(iter->first == key_list[i]);
            CHECK(static_cast<size_t>(iter->second) == i);
        }
    }

    SUBCASE("find non-exists") {
        auto iter = object.find("Something Else");
        CHECK(iter == object.end());

        iter = object.find("zzz");
        CHECK(iter == object.cend());
    }

    SUBCASE("const find non-exists") {
        auto iter = static_cast<const json_object &>(object).find("Something Else");
        CHECK(iter == object.cend());

        iter = static_cast<const json_object &>(object).find("zzz");
        CHECK(iter == object.cend());
    }
}

TEST_CASE("JSON object access by key") {
    json_document doc;
    json_element root = doc.root();
    root.set_object();

    constexpr std::string_view key_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    auto object = *root.object();
    for (size_t i = 0; i < std::size(key_list); ++i)
        object.insert(key_list[i], i);

    CHECK(object.contains("Niaj"));
    CHECK(object["Niaj"].is_integer());
    CHECK(*object["Niaj"].integer() == 11);

    CHECK(!object.contains("zzz"));
    CHECK(object["zzz"].is_null());
    CHECK(object.contains("zzz"));
}

TEST_CASE("JSON object iterator") {
    json_document doc;
    json_element root = doc.root();
    root.set_object();

    constexpr std::string_view key_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    auto object = *root.object();
    for (size_t i = 0; i < std::size(key_list); ++i)
        object.insert(key_list[i], i);

    SUBCASE("iterator element access") {
        auto iter = object.begin();
        CHECK(iter->first == key_list[0]);
        CHECK((*iter).first == key_list[0]);
        CHECK(static_cast<size_t>(iter->second) == 0);
        CHECK(static_cast<size_t>((*iter).second) == 0);

        // random access element
        for (size_t i = 0; i < std::size(key_list); ++i) {
            CHECK(iter[i].first == key_list[i]);
            CHECK(static_cast<size_t>(iter[i].second) == i);

            CHECK((iter + i)->first == key_list[i]);
            CHECK(static_cast<size_t>((iter + i)->second) == i);
        }
    }

    SUBCASE("operator++()") {
        size_t index = 0;
        for (auto iter = object.begin(); iter != object.end(); ++iter, ++index) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator++(int)") {
        size_t index = 0;
        for (auto iter = object.begin(); iter != object.end(); iter++, index++) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator--()") {
        size_t index = std::size(key_list);
        for (auto iter = object.end(); iter != object.begin();) {
            --iter;
            --index;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator--(int)") {
        size_t index = std::size(key_list);
        for (auto iter = object.end(); iter != object.begin();) {
            iter--;
            index--;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator+=()") {
        size_t index = 0;
        for (auto iter = object.begin(); iter != object.end(); iter += 1, index += 1) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator-=()") {
        size_t index = std::size(key_list);
        for (auto iter = object.end(); iter != object.begin();) {
            iter  -= 1;
            index -= 1;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator+()") {
        size_t index = 0;
        for (auto iter = object.begin(); iter + index != object.end(); index += 1) {
            CHECK((iter + index)->first == key_list[index]);
            CHECK(static_cast<size_t>((iter + index)->second) == index);
        }
    }

    SUBCASE("operator-()") {
        size_t index = 0;
        for (auto iter = object.end(); iter - index != object.begin(); ++index) {
            CHECK((iter - index - 1)->first == key_list[std::size(key_list) - index - 1]);
            CHECK(static_cast<size_t>((iter - index - 1)->second) ==
                  std::size(key_list) - index - 1);
        }
    }

    SUBCASE("arithmetic") {
        CHECK(object.end() - object.begin() == std::size(key_list));
    }
}

TEST_CASE("JSON object const iterator") {
    json_document doc;
    json_element root = doc.root();
    root.set_object();

    constexpr std::string_view key_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    auto object = *root.object();
    for (size_t i = 0; i < std::size(key_list); ++i)
        object.insert(key_list[i], i);

    SUBCASE("iterator element access") {
        auto iter = object.cbegin();
        CHECK(iter->first == key_list[0]);
        CHECK((*iter).first == key_list[0]);
        CHECK(static_cast<size_t>(iter->second) == 0);
        CHECK(static_cast<size_t>((*iter).second) == 0);

        // random access element
        for (size_t i = 0; i < std::size(key_list); ++i) {
            CHECK(iter[i].first == key_list[i]);
            CHECK(static_cast<size_t>(iter[i].second) == i);

            CHECK((iter + i)->first == key_list[i]);
            CHECK(static_cast<size_t>((iter + i)->second) == i);
        }
    }

    SUBCASE("operator++()") {
        size_t index = 0;
        for (auto iter = object.cbegin(); iter != object.cend(); ++iter, ++index) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator++(int)") {
        size_t index = 0;
        for (auto iter = object.cbegin(); iter != object.cend(); iter++, index++) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator--()") {
        size_t index = std::size(key_list);
        for (auto iter = object.cend(); iter != object.cbegin();) {
            --iter;
            --index;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator--(int)") {
        size_t index = std::size(key_list);
        for (auto iter = object.cend(); iter != object.cbegin();) {
            iter--;
            index--;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator+=()") {
        size_t index = 0;
        for (auto iter = object.cbegin(); iter != object.cend(); iter += 1, index += 1) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator-=()") {
        size_t index = std::size(key_list);
        for (auto iter = object.cend(); iter != object.cbegin();) {
            iter  -= 1;
            index -= 1;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator+()") {
        size_t index = 0;
        for (auto iter = object.cbegin(); iter + index != object.cend(); index += 1) {
            CHECK((iter + index)->first == key_list[index]);
            CHECK(static_cast<size_t>((iter + index)->second) == index);
        }
    }

    SUBCASE("operator-()") {
        size_t index = 0;
        for (auto iter = object.cend(); iter - index != object.cbegin(); ++index) {
            CHECK((iter - index - 1)->first == key_list[std::size(key_list) - index - 1]);
            CHECK(static_cast<size_t>((iter - index - 1)->second) ==
                  std::size(key_list) - index - 1);
        }
    }

    SUBCASE("arithmetic") {
        CHECK(object.cend() - object.cbegin() == std::size(key_list));
    }
}

TEST_CASE("JSON object reverse iterator") {
    json_document doc;
    json_element root = doc.root();
    root.set_object();

    constexpr std::string_view key_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    auto object = *root.object();
    for (size_t i = 0; i < std::size(key_list); ++i)
        object.insert(key_list[i], i);

    SUBCASE("iterator element access") {
        auto iter = object.rbegin();
        CHECK(iter->first == key_list[std::size(key_list) - 1]);
        CHECK((*iter).first == key_list[std::size(key_list) - 1]);
        CHECK(static_cast<size_t>(iter->second) == std::size(key_list) - 1);
        CHECK(static_cast<size_t>((*iter).second) == std::size(key_list) - 1);

        // random access element
        for (size_t i = 0; i < std::size(key_list); ++i) {
            CHECK(iter[i].first == key_list[std::size(key_list) - i - 1]);
            CHECK(static_cast<size_t>(iter[i].second) == std::size(key_list) - i - 1);

            CHECK((iter + i)->first == key_list[std::size(key_list) - i - 1]);
            CHECK(static_cast<size_t>((iter + i)->second) == std::size(key_list) - i - 1);
        }
    }

    SUBCASE("operator++()") {
        size_t index = std::size(key_list) - 1;
        for (auto iter = object.rbegin(); iter != object.rend(); ++iter, --index) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator++(int)") {
        size_t index = std::size(key_list) - 1;
        for (auto iter = object.rbegin(); iter != object.rend(); iter++, --index) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator--()") {
        size_t index = 0;
        for (auto iter = object.rend(); iter != object.rbegin(); ++index) {
            --iter;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator--(int)") {
        size_t index = 0;
        for (auto iter = object.rend(); iter != object.rbegin(); ++index) {
            iter--;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator+=()") {
        size_t index = std::size(key_list) - 1;
        for (auto iter = object.rbegin(); iter != object.rend(); iter += 1, --index) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator-=()") {
        size_t index = 0;
        for (auto iter = object.rend(); iter != object.rbegin(); ++index) {
            iter -= 1;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator+()") {
        size_t index = 0;
        for (auto iter = object.rbegin(); iter + index != object.rend(); ++index) {
            CHECK((iter + index)->first == key_list[std::size(key_list) - index - 1]);
            CHECK(static_cast<size_t>((iter + index)->second) == std::size(key_list) - index - 1);
        }
    }

    SUBCASE("operator-()") {
        size_t index = 0;
        for (auto iter = object.rend(); iter - index != object.rbegin(); ++index) {
            CHECK((iter - index - 1)->first == key_list[index]);
            CHECK(static_cast<size_t>((iter - index - 1)->second) == index);
        }
    }

    SUBCASE("arithmetic") {
        CHECK(object.rend() - object.rbegin() == std::size(key_list));
    }
}

TEST_CASE("JSON object const reverse iterator") {
    json_document doc;
    json_element root = doc.root();
    root.set_object();

    constexpr std::string_view key_list[] = {
        "Alice",  "Bob",   "Carol", "David",   "Erin",   "Frank",  "Grace",
        "Heidi",  "Ivan",  "Judy",  "Michael", "Niaj",   "Olivia", "Peggy",
        "Robert", "Sybil", "Trudy", "Victor",  "Walter",
    };

    auto object = *root.object();
    for (size_t i = 0; i < std::size(key_list); ++i)
        object.insert(key_list[i], i);

    SUBCASE("iterator element access") {
        auto iter = object.crbegin();
        CHECK(iter->first == key_list[std::size(key_list) - 1]);
        CHECK((*iter).first == key_list[std::size(key_list) - 1]);
        CHECK(static_cast<size_t>(iter->second) == std::size(key_list) - 1);
        CHECK(static_cast<size_t>((*iter).second) == std::size(key_list) - 1);

        // random access element
        for (size_t i = 0; i < std::size(key_list); ++i) {
            CHECK(iter[i].first == key_list[std::size(key_list) - i - 1]);
            CHECK(static_cast<size_t>(iter[i].second) == std::size(key_list) - i - 1);

            CHECK((iter + i)->first == key_list[std::size(key_list) - i - 1]);
            CHECK(static_cast<size_t>((iter + i)->second) == std::size(key_list) - i - 1);
        }
    }

    SUBCASE("operator++()") {
        size_t index = std::size(key_list) - 1;
        for (auto iter = object.crbegin(); iter != object.crend(); ++iter, --index) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator++(int)") {
        size_t index = std::size(key_list) - 1;
        for (auto iter = object.crbegin(); iter != object.crend(); iter++, --index) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator--()") {
        size_t index = 0;
        for (auto iter = object.crend(); iter != object.crbegin(); ++index) {
            --iter;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator--(int)") {
        size_t index = 0;
        for (auto iter = object.crend(); iter != object.crbegin(); ++index) {
            iter--;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator+=()") {
        size_t index = std::size(key_list) - 1;
        for (auto iter = object.crbegin(); iter != object.crend(); iter += 1, --index) {
            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator-=()") {
        size_t index = 0;
        for (auto iter = object.crend(); iter != object.crbegin(); ++index) {
            iter -= 1;

            CHECK(iter->first == key_list[index]);
            CHECK(static_cast<size_t>(iter->second) == index);
        }
    }

    SUBCASE("operator+()") {
        size_t index = 0;
        for (auto iter = object.crbegin(); iter + index != object.crend(); ++index) {
            CHECK((iter + index)->first == key_list[std::size(key_list) - index - 1]);
            CHECK(static_cast<size_t>((iter + index)->second) == std::size(key_list) - index - 1);
        }
    }

    SUBCASE("operator-()") {
        size_t index = 0;
        for (auto iter = object.crend(); iter - index != object.crbegin(); ++index) {
            CHECK((iter - index - 1)->first == key_list[index]);
            CHECK(static_cast<size_t>((iter - index - 1)->second) == index);
        }
    }

    SUBCASE("arithmetic") {
        CHECK(object.crend() - object.crbegin() == std::size(key_list));
    }
}
