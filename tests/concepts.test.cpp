// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <forward_list>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <set>
#include <span>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <version>
#if __cpp_lib_flat_map >= 202207L
    #include <flat_map>
#endif
#if __cpp_lib_flat_set >= 202207L
    #include <flat_set>
#endif

#include <catch2/catch_test_macros.hpp>

#include <reflex/enum_indexed_fwd.hpp>

#include <reflex/config/concepts.hpp>

#include "test_types.hpp"

namespace reflex::config {

TEST_CASE("only non-cv-qualified types satisfy unqualified") {
    STATIC_REQUIRE(unqualified<std::int32_t>);
    STATIC_REQUIRE(unqualified<double>);
    STATIC_REQUIRE(unqualified<std::vector<std::string>>);

    STATIC_REQUIRE(!unqualified<const volatile std::vector<float>>);
    STATIC_REQUIRE(!unqualified<volatile bool>);
    STATIC_REQUIRE(!unqualified<const volatile std::monostate>);
}

TEST_CASE("one_of checks for exact type matches") {
    STATIC_REQUIRE(one_of<int, int, float, bool>);
    STATIC_REQUIRE(one_of<std::string, std::vector<int>, float, std::string>);

    STATIC_REQUIRE(!one_of<const int, int, float, bool>);
    STATIC_REQUIRE(!one_of<std::string, std::vector<int>, float, const std::string>);
    STATIC_REQUIRE(!one_of<bool>);
}

TEST_CASE("only possibly cv-qualified numeric types match numeric") {
    STATIC_REQUIRE(numeric<std::int8_t>);
    STATIC_REQUIRE(numeric<std::uint8_t>);
    STATIC_REQUIRE(numeric<const float>);
    STATIC_REQUIRE(numeric<volatile double>);
    STATIC_REQUIRE(numeric<custom_number<int>>);

    STATIC_REQUIRE(!numeric<bool>);
    STATIC_REQUIRE(!numeric<char>);
    STATIC_REQUIRE(!numeric<const char32_t>);
}

TEST_CASE("only numeric types of the same category satisfy same_numeric_category_as") {
    STATIC_REQUIRE(same_numeric_category_as<short, int>);
    STATIC_REQUIRE(same_numeric_category_as<signed char, long long>);
    STATIC_REQUIRE(same_numeric_category_as<unsigned short, unsigned long long>);
    STATIC_REQUIRE(same_numeric_category_as<float, long double>);
    STATIC_REQUIRE(same_numeric_category_as<float, long double>);
    STATIC_REQUIRE(same_numeric_category_as<custom_number<int>, long>);
    STATIC_REQUIRE(same_numeric_category_as<custom_number<float>, long double>);
    STATIC_REQUIRE(same_numeric_category_as<custom_number<float>, custom_number<double>>);

    STATIC_REQUIRE(!same_numeric_category_as<char, long>);
    STATIC_REQUIRE(!same_numeric_category_as<int, bool>);
    STATIC_REQUIRE(!same_numeric_category_as<int, unsigned>);
    STATIC_REQUIRE(!same_numeric_category_as<float, std::int64_t>);
    STATIC_REQUIRE(!same_numeric_category_as<std::string, double>);
    STATIC_REQUIRE(!same_numeric_category_as<std::uint32_t, char32_t>);
    STATIC_REQUIRE(!same_numeric_category_as<custom_number<int>, float>);
    STATIC_REQUIRE(!same_numeric_category_as<custom_number<float>, custom_number<long>>);
}

TEST_CASE("standard containers satisfy container while views do not") {
    STATIC_REQUIRE(container<std::vector<int>>);
    STATIC_REQUIRE(container<std::string>);
    STATIC_REQUIRE(container<std::deque<std::string>>);
    STATIC_REQUIRE(container<const std::unordered_map<std::int32_t, bool>>);
    STATIC_REQUIRE(container<std::set<std::string>>);

    STATIC_REQUIRE(!container<std::ranges::iota_view<std::size_t>>);
    STATIC_REQUIRE(!container<std::span<const std::string>>);
}

TEST_CASE("standard associative containers satisfy associative_container while other types do not") {
    STATIC_REQUIRE(associative_container<std::set<std::string>>);
    STATIC_REQUIRE(associative_container<std::multiset<std::string>>);
    STATIC_REQUIRE(associative_container<std::map<std::string, std::vector<bool>>>);
    STATIC_REQUIRE(associative_container<std::multiset<std::uint64_t>>);
    STATIC_REQUIRE(associative_container<std::multimap<bool, std::string>>);
    STATIC_REQUIRE(associative_container<std::unordered_set<std::string>>);
    STATIC_REQUIRE(associative_container<std::unordered_map<std::int16_t, std::string>>);
    STATIC_REQUIRE(associative_container<std::unordered_multiset<std::string>>);
    STATIC_REQUIRE(associative_container<std::unordered_multimap<std::string, std::vector<std::uint32_t>>>);
#if __cpp_lib_flat_map >= 202207L
    STATIC_REQUIRE(associative_container<std::flat_map<std::uint32_t, std::string>>);
    STATIC_REQUIRE(associative_container<std::flat_multimap<std::uint32_t, std::string>>);
#endif
#if __cpp_lib_flat_set >= 202207L
    STATIC_REQUIRE(associative_container<std::flat_set<std::string>>);
    STATIC_REQUIRE(associative_container<std::flat_multiset<std::string>>);
#endif

    STATIC_REQUIRE(!associative_container<bool>);
    STATIC_REQUIRE(!associative_container<std::pair<std::int32_t, float>>);

    STATIC_REQUIRE(!associative_container<std::string>);
    STATIC_REQUIRE(!associative_container<std::array<std::size_t, 7>>);
    STATIC_REQUIRE(!associative_container<std::vector<std::string>>);
    STATIC_REQUIRE(!associative_container<std::deque<std::uint8_t>>);
    STATIC_REQUIRE(!associative_container<std::forward_list<double>>);
    STATIC_REQUIRE(!associative_container<std::list<std::vector<std::string>>>);

    STATIC_REQUIRE(!associative_container<std::stack<std::string>>);
    STATIC_REQUIRE(!associative_container<std::queue<std::pair<std::int32_t, float>>>);
    STATIC_REQUIRE(!associative_container<std::priority_queue<std::vector<std::string>>>);
}

TEST_CASE("standard sequence containers satisfy sequence_container while other types do not") {
    STATIC_REQUIRE(sequence_container<std::string>);
    STATIC_REQUIRE(sequence_container<std::array<std::size_t, 7>>);
    STATIC_REQUIRE(sequence_container<std::vector<std::string>>);
    STATIC_REQUIRE(sequence_container<std::deque<std::uint8_t>>);
    STATIC_REQUIRE(sequence_container<std::forward_list<double>>);
    STATIC_REQUIRE(sequence_container<std::list<std::vector<std::string>>>);

    STATIC_REQUIRE(!sequence_container<float>);
    STATIC_REQUIRE(!sequence_container<std::uint64_t>);

    STATIC_REQUIRE(!sequence_container<std::set<std::string>>);
    STATIC_REQUIRE(!sequence_container<std::map<std::string, std::vector<bool>>>);
    STATIC_REQUIRE(!sequence_container<std::multiset<std::uint64_t>>);
    STATIC_REQUIRE(!sequence_container<std::multimap<bool, std::string>>);
    STATIC_REQUIRE(!sequence_container<std::unordered_set<std::string>>);
    STATIC_REQUIRE(!sequence_container<std::unordered_map<std::int16_t, std::string>>);
    STATIC_REQUIRE(!sequence_container<std::unordered_multiset<std::string>>);
    STATIC_REQUIRE(!sequence_container<std::unordered_multimap<std::string, std::vector<std::uint32_t>>>);

    STATIC_REQUIRE(!sequence_container<std::stack<std::string>>);
    STATIC_REQUIRE(!sequence_container<std::queue<std::pair<std::int32_t, float>>>);
    STATIC_REQUIRE(!sequence_container<std::priority_queue<std::vector<std::string>>>);

#if __cpp_lib_flat_map >= 202207L
    STATIC_REQUIRE(!sequence_container<std::flat_map<std::int32_t, std::string>>);
    STATIC_REQUIRE(!sequence_container<std::flat_multimap<std::int32_t, std::string>>);
#endif
#if __cpp_lib_flat_set >= 202207L
    STATIC_REQUIRE(!sequence_container<std::flat_set<std::string>>);
    STATIC_REQUIRE(!sequence_container<std::flat_multiset<std::int32_t>>);
#endif
}

TEST_CASE("standard sequence containers of char satisfy string_like") {
    STATIC_REQUIRE(string_like<std::string>);
    STATIC_REQUIRE(string_like<std::basic_string<char, std::char_traits<char>, custom_allocator<char>>>);
    STATIC_REQUIRE(string_like<std::vector<char>>);

    STATIC_REQUIRE(!string_like<std::string_view>);
    STATIC_REQUIRE(!string_like<std::u8string>);
    STATIC_REQUIRE(!string_like<std::wstring>);
    STATIC_REQUIRE(!string_like<std::list<char>>);
    STATIC_REQUIRE(!string_like<std::set<char>>);
}

TEST_CASE("standard maps satisfy map_like while other containers do not") {
    STATIC_REQUIRE(map_like<std::map<std::string, std::int32_t>>);
    STATIC_REQUIRE(map_like<std::unordered_map<std::uint16_t, std::vector<std::int32_t>>>);
    STATIC_REQUIRE(map_like<std::multimap<std::string, std::string>>);
    STATIC_REQUIRE(map_like<std::unordered_multimap<std::size_t, std::array<bool, 7>>>);
#if __cpp_lib_flat_map >= 202207L
    STATIC_REQUIRE(map_like<std::flat_map<std::string, int>>)
    STATIC_REQUIRE(map_like<std::flat_multimap<std::uint64_t, std::string>>)
#endif

    STATIC_REQUIRE(!map_like<std::string>);
    STATIC_REQUIRE(!map_like<std::array<float, 42>>);
    STATIC_REQUIRE(!map_like<std::vector<std::string>>);
    STATIC_REQUIRE(!map_like<std::set<std::string>>);
    STATIC_REQUIRE(!map_like<std::set<std::uint64_t>>);
    STATIC_REQUIRE(!map_like<std::unordered_multiset<std::u32string>>);
#if __cpp_lib_flat_set >= 202207L
    STATIC_REQUIRE(!map_like<std::flat_set<std::size_t>>);
    STATIC_REQUIRE(!map_like<std::flat_multiset<std::string>>);
#endif
}

TEST_CASE("Standard sets satisfy set_like while other containers do not") {
    STATIC_REQUIRE(set_like<std::set<std::string>>);
    STATIC_REQUIRE(set_like<std::set<std::uint32_t>>);
    STATIC_REQUIRE(set_like<std::set<std::vector<int>>>);
    STATIC_REQUIRE(set_like<std::unordered_set<int>>);
    STATIC_REQUIRE(set_like<std::unordered_set<std::string>>);
#if __cpp_lib_flat_set >= 202207L
    STATIC_REQUIRE(set_like<std::flat_set<std::string>>);
    STATIC_REQUIRE(set_like<std::flat_multiset<std::vector<int>>>);
#endif

    STATIC_REQUIRE(!set_like<std::vector<bool>>);
    STATIC_REQUIRE(!set_like<std::vector<std::string>>);
    STATIC_REQUIRE(!set_like<std::string>);
    STATIC_REQUIRE(!set_like<std::array<int, 10>>);
    STATIC_REQUIRE(!set_like<std::map<std::string, std::string>>);
    STATIC_REQUIRE(!set_like<std::unordered_map<std::int32_t, std::string>>);
#if __cpp_lib_flat_map >= 202207L
    STATIC_REQUIRE(!set_like<std::flat_map<std::int32_t, std::string>>);
    STATIC_REQUIRE(!set_like<std::flat_multimap<std::int32_t, std::string>>);
#endif
}

TEST_CASE("std::array<T> satisfies array_like") {
    STATIC_REQUIRE(array_like<std::array<int, 1>>);
    STATIC_REQUIRE(array_like<std::array<std::string, 42>>);
}

TEST_CASE("std::optional<T> std::unique_ptr<T> satisfy optional_like while other types do not") {
    STATIC_REQUIRE(optional_like<std::optional<std::string>>);
    STATIC_REQUIRE(optional_like<std::optional<std::uint64_t>>);

    STATIC_REQUIRE(optional_like<std::unique_ptr<bool>>);
    STATIC_REQUIRE(optional_like<std::unique_ptr<std::vector<std::array<std::uint32_t, 7>>>>);

    struct custom_deleter {
        void operator()(void *ptr) const noexcept;
    };
    STATIC_REQUIRE(!optional_like<std::unique_ptr<std::string, custom_deleter>>);

    STATIC_REQUIRE(!optional_like<std::string *>);
    STATIC_REQUIRE(!optional_like<std::vector<std::string>>);
}

TEST_CASE("is_enum_indexed_array_v is true for reflex::EnumIndexedArray<T, E>") {
    STATIC_REQUIRE(is_enum_indexed_array_v<reflex::EnumIndexedArray<float, test_enum>>);
    STATIC_REQUIRE(!is_enum_indexed_array_v<std::array<std::string, 2>>);
}

TEST_CASE("detail::enum_indexed_traits is defined for reflex::EnumIndexedArray<T, E>") {
    using traits_t = detail::enum_indexed_array_traits<reflex::EnumIndexedArray<int, test_enum>>;
    STATIC_REQUIRE(std::same_as<typename traits_t::value_type, int>);
    STATIC_REQUIRE(std::same_as<typename traits_t::key_type, test_enum>);
}

TEST_CASE("types for which custom_traits is specialized satisfy custom_type while others do not") {
    STATIC_REQUIRE(custom_type<CustomClass>);

    STATIC_REQUIRE(!custom_type<int>);
    STATIC_REQUIRE(!custom_type<std::string>);
    STATIC_REQUIRE(!custom_type<std::vector<std::unordered_map<std::string, float>>>);
    STATIC_REQUIRE(!custom_type<custom_data_type>);
}

TEST_CASE("types with valid custom_traits::to_string implementations satisfy custom_type_with_to_string") {
    STATIC_REQUIRE(custom_type_with_to_string<CustomStringTag<true>>);

    STATIC_REQUIRE(!custom_type_with_to_string<CustomStringTag<false>>);
    STATIC_REQUIRE(!custom_type_with_to_string<CustomClass>);
    STATIC_REQUIRE(!custom_type_with_to_string<int>);
    STATIC_REQUIRE(!custom_type_with_to_string<std::string>);
}

TEST_CASE("custom types with string-optimized try_deserialize satisfy custom_type_with_try_deserialize_string") {
    STATIC_REQUIRE(custom_type_with_try_deserialize_string<CustomStringTag<true>>);

    STATIC_REQUIRE(!custom_type_with_try_deserialize_string<CustomStringTag<false>>);
    STATIC_REQUIRE(!custom_type_with_try_deserialize_string<int>);
    STATIC_REQUIRE(!custom_type_with_try_deserialize_string<std::string>);
    STATIC_REQUIRE(!custom_type_with_try_deserialize_string<CustomClass>);
}

} // namespace reflex::config
