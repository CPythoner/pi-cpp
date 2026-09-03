#include <doctest/doctest.h>

#include <string>

#include "util/string.hpp"

TEST_CASE("startsWith 基本与边界") {
    // 空串与空前缀
    CHECK(pi::startsWith("", ""));
    CHECK(pi::startsWith("abc", ""));
    CHECK_FALSE(pi::startsWith("", "a"));

    // 前缀等于自身
    CHECK(pi::startsWith("abc", "abc"));
    CHECK(pi::startsWith("", ""));

    // 部分匹配
    CHECK(pi::startsWith("abc", "a"));
    CHECK(pi::startsWith("abc", "ab"));
    CHECK_FALSE(pi::startsWith("abc", "bc"));   // 中间出现不算前缀
    CHECK_FALSE(pi::startsWith("abc", "abcd")); // 前缀更长
    CHECK_FALSE(pi::startsWith("abc", "Abc"));  // 大小写敏感
}

TEST_CASE("endsWith 重叠边界") {
    CHECK(pi::endsWith("", ""));
    CHECK(pi::endsWith("abc", ""));
    CHECK(pi::endsWith("abc", "abc"));
    CHECK(pi::endsWith("abc", "c"));
    CHECK(pi::endsWith("abc", "bc"));
    CHECK_FALSE(pi::endsWith("", "c"));

    // 重叠边界："aaa" 的末尾既是 "aa" 也是 "a"
    CHECK(pi::endsWith("aaa", "aa"));
    CHECK(pi::endsWith("aaa", "a"));
    CHECK_FALSE(pi::endsWith("aaa", "aaaa"));
    CHECK(pi::endsWith("aab", "ab"));
    CHECK_FALSE(pi::endsWith("ab", "aba"));
}

TEST_CASE("contains") {
    CHECK(pi::contains("", ""));
    CHECK(pi::contains("abc", ""));
    CHECK_FALSE(pi::contains("", "a"));
    CHECK(pi::contains("hello world", "world"));
    CHECK(pi::contains("aaa", "aa"));
    CHECK(pi::contains("abc", "abc"));
    CHECK_FALSE(pi::contains("abc", "abcd"));
    CHECK_FALSE(pi::contains("abc", "cb"));
}

TEST_CASE("接受 string/string_view 混合实参") {
    const std::string s = "picpp";
    const std::string prefix = "pi";
    CHECK(pi::startsWith(s, prefix));
    CHECK(pi::endsWith(std::string_view(s), std::string_view("pp")));
    CHECK(pi::contains(s, "cp"));
}
