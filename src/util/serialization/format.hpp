// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SERIALIZATION_FORMAT_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_FORMAT_H_

#include "serializer.hpp"
#include "util/common/buffer.hpp"
#include "util/common/config.hpp"
#include "util/common/variant_overloaded.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace cbdc {

    /// Serializes the std::byte as a std::uint8_t.
    auto operator<<(serializer& packet, std::byte b) -> serializer&;

    /// \brief Deserializes a single std::byte.
    ///
    /// Copies a single byte (`CHAR_BIT` bits) of data into `b`.
    ///
    /// \see \ref cbdc::operator<<(serializer&, std::byte)
    auto operator>>(serializer& packet, std::byte& b) -> serializer&;

    /// \brief Serializes a raw byte buffer.
    ///
    /// Writes the size of the buffer as a 64-bit uint, followed by the actual
    /// buffer data.
    ///
    /// \see \ref cbdc::operator>>(serializer&, buffer&)
    auto operator<<(serializer& ser, const buffer& b) -> serializer&;

    /// \brief Deserializes a raw byte buffer.
    auto operator>>(serializer& deser, buffer& b) -> serializer&;

    /// Serializes nothing if `T` is an empty type.
    /// \tparam T an empty type
    /// \param s the serializer (to which nothing will be written)
    template<typename T>
    auto operator<<(serializer& s, T /* t */) ->
        typename std::enable_if_t<std::is_empty_v<T>, serializer&> {
        return s;
    }

    /// Deserializes nothing if `T` is an empty type.
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename T>
    auto operator>>(serializer& s, T& /* t */) ->
        typename std::enable_if_t<std::is_empty_v<T>, serializer&> {
        return s;
    }

    /// \brief Serializes the integral argument.
    ///
    /// Copies `sizeof(T)` bytes from `t` following machine endianness.
    ///
    /// \tparam T the integral type of the value to serialize
    /// \param t the value to serialize
    template<typename T>
    auto operator<<(serializer& s, T t) ->
        typename std::enable_if_t<std::is_integral_v<T> && !std::is_enum_v<T>,
                                  serializer&> {
        s.write(&t, sizeof(t));
        return s;
    }

    /// \brief Deserializes the integral argument.
    ///
    /// Writes `sizeof(T)` bytes into `t` following machine endianness.
    ///
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename T>
    auto operator>>(serializer& s, T& t) ->
        typename std::enable_if_t<std::is_integral_v<T> && !std::is_enum_v<T>,
                                  serializer&> {
        s.read(&t, sizeof(t));
        return s;
    }

    /// Serializes the array of integral values in-order.
    ///
    /// \see \ref cbdc::operator<<(serializer&, T)
    ///
    /// \tparam T the underlying integral type
    /// \tparam len the length of the array to be serialized
    /// \param packet the serializer to receive the data
    /// \param arr the array of data to be serialized
    template<typename T, size_t len>
    auto operator<<(serializer& packet, const std::array<T, len>& arr) ->
        typename std::enable_if_t<std::is_integral_v<T>, serializer&> {
        packet.write(arr.data(), sizeof(T) * len);
        return packet;
    }

    /// Deserializes the array of integral values in-order.
    /// \see \ref cbdc::operator<<(serializer&, const std::array<T, len>&)
    template<typename T, size_t len>
    auto operator>>(serializer& packet, std::array<T, len>& arr) ->
        typename std::enable_if_t<std::is_integral_v<T>, serializer&> {
        packet.read(arr.data(), sizeof(T) * len);
        return packet;
    }

    /// Deserializes an optional value.
    /// \see \ref cbdc::operator<<(serializer&, const std::optional<T>&)
    template<typename T>
    auto operator>>(serializer& deser, std::optional<T>& val) -> serializer& {
        bool has_value{};
        if(!(deser >> has_value)) {
            return deser;
        }

        if(has_value) {
            auto opt_val = T();
            if(!(deser >> opt_val)) {
                return deser;
            }
            val = std::move(opt_val);
        } else {
            val = std::nullopt;
        }

        return deser;
    }

    /// Serializes `val.has_value()`, and if `val.has_value() == true`,
    /// serializes the value itself.
    ///
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename T>
    auto operator<<(serializer& ser, const std::optional<T>& val)
        -> serializer& {
        auto has_value = val.has_value();
        ser << has_value;
        if(has_value) {
            ser << *val;
        }
        return ser;
    }

    /// Serializes a pair of values: first, then second.
    ///
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename A, typename B>
    auto operator<<(serializer& ser, const std::pair<A, B>& p) -> serializer& {
        ser << p.first << p.second;
        return ser;
    }

    /// Deserializes a pair of values.
    /// \see \ref cbdc::operator<<(serializer&, const std::pair<A,B>&)
    template<typename A, typename B>
    auto operator>>(serializer& deser, std::pair<A, B>& p) -> serializer& {
        auto a = A();
        if(!(deser >> a)) {
            return deser;
        }

        auto b = B();
        if(!(deser >> b)) {
            return deser;
        }

        p = {std::move(a), std::move(b)};
        return deser;
    }

    /// Serializes the count of elements in the vector, and then each element
    /// in-order.
    ///
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename T>
    auto operator<<(serializer& packet, const std::vector<T>& vec)
        -> serializer& {
        const auto len = static_cast<uint64_t>(vec.size());
        packet << len;
        for(uint64_t i = 0; i < len; i++) {
            packet << static_cast<T>(vec[i]);
        }
        return packet;
    }

    /// Deserializes a vector of elements.
    /// \see \ref cbdc::operator<<(serializer&, const std::vector<T>&)
    template<typename T>
    auto operator>>(serializer& packet, std::vector<T>& vec) -> serializer& {
        static_assert(sizeof(T) <= config::maximum_reservation,
                      "Vector element size too large");

        uint64_t len{};
        if(!(packet >> len)) {
            return packet;
        }

        uint64_t allocated = 0;
        while(allocated < len) {
            allocated = std::min(
                len,
                allocated + config::maximum_reservation / sizeof(T));
            vec.reserve(allocated);
            while(vec.size() < allocated) {
                if constexpr(std::is_default_constructible_v<T>) {
                    T val{};
                    if(!(packet >> val)) {
                        return packet;
                    }
                    vec.push_back(std::move(val));
                } else {
                    auto val = T(packet);
                    if(!packet) {
                        return packet;
                    }
                    vec.push_back(std::move(val));
                }
            }
        }

        vec.shrink_to_fit();
        return packet;
    }

    /// Serializes the count of key-value pairs, and then each key and value,
    /// statically-casted.
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename K, typename V, typename... Ts>
    auto operator<<(serializer& ser,
                    const std::unordered_map<K, V, Ts...>& map)
        -> serializer& {
        auto len = static_cast<uint64_t>(map.size());
        ser << len;
        for(const auto& it : map) {
            ser << static_cast<K>(it.first);
            ser << static_cast<V>(it.second);
        }
        return ser;
    }

    /// Deserializes an unordered map of key-value pairs.
    /// \see \ref cbdc::operator<<(serializer&, const std::unordered_map<K, V, Ts...>&)
    template<typename K, typename V, typename... Ts>
    auto operator>>(serializer& deser, std::unordered_map<K, V, Ts...>& map)
        -> serializer& {
        static_assert(sizeof(K) + sizeof(V) <= config::maximum_reservation,
                      "Unordered Map element size too large");
        auto len = uint64_t();
        if(!(deser >> len)) {
            return deser;
        }

        uint64_t allocated = 0;
        while(allocated < len) {
            allocated = std::min(len,
                                 allocated
                                     + config::maximum_reservation
                                           / (sizeof(K) + sizeof(V)));
            map.reserve(allocated);
            while(map.size() < allocated) {
                auto key = K();
                if(!(deser >> key)) {
                    return deser;
                }

                auto val = V();
                if(!(deser >> val)) {
                    return deser;
                }

                map.emplace(std::move(key), std::move(val));
            }
        }
        return deser;
    }

    /// Deserializes a map of key-value pairs.
    template<typename K, typename V, typename... Ts>
    auto operator>>(serializer& deser, std::map<K, V, Ts...>& map)
        -> serializer& {
        auto len = uint64_t();
        if(!(deser >> len)) {
            return deser;
        }

        for(uint64_t i = 0; i < len; i++) {
            auto key = K();
            if(!(deser >> key)) {
                return deser;
            }

            auto val = V();
            if(!(deser >> val)) {
                return deser;
            }

            map.emplace(std::move(key), std::move(val));
        }

        return deser;
    }

    /// Serializes the count of items, and then each item statically-casted.
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename K, typename... Ts>
    auto operator<<(serializer& ser, const std::set<K, Ts...>& set)
        -> serializer& {
        auto len = static_cast<uint64_t>(set.size());
        ser << len;
        for(const auto& key : set) {
            ser << static_cast<K>(key);
        }
        return ser;
    }

    /// Deserializes a set of items.
    /// \see \ref cbdc::operator<<(serializer&, const std::set<K, Ts...>&)
    template<typename K, typename... Ts>
    auto operator>>(serializer& deser, std::set<K, Ts...>& set)
        -> serializer& {
        auto len = uint64_t();
        if(!(deser >> len)) {
            return deser;
        }

        for(uint64_t i = 0; i < len; i++) {
            auto key = K();
            if(!(deser >> key)) {
                return deser;
            }
            set.emplace(std::move(key));
        }
        return deser;
    }

    /// Serializes the count of items, and then each item statically-casted.
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename K, typename... Ts>
    auto operator<<(serializer& ser, const std::unordered_set<K, Ts...>& set)
        -> serializer& {
        auto len = static_cast<uint64_t>(set.size());
        ser << len;
        for(const auto& key : set) {
            ser << static_cast<K>(key);
        }
        return ser;
    }

    /// Deserializes an unordered set of items.
    /// \see \ref cbdc::operator<<(serializer&, const std::unordered_set<K, Ts...>&)
    template<typename K, typename... Ts>
    auto operator>>(serializer& deser, std::unordered_set<K, Ts...>& set)
        -> serializer& {
        static_assert(sizeof(K) <= config::maximum_reservation,
                      "Unordered Set element size too large");
        auto len = uint64_t();
        if(!(deser >> len)) {
            return deser;
        }

        uint64_t allocated = 0;
        while(allocated < len) {
            allocated = std::min(
                len,
                allocated + config::maximum_reservation / sizeof(K));
            while(set.size() < allocated) {
                auto key = K();
                if(!(deser >> key)) {
                    return deser;
                }
                set.emplace(std::move(key));
            }
        }
        return deser;
    }

    /// Serializes the variant index of the value, and then the value itself.
    /// \see \ref cbdc::operator<<(serializer&, T)
    template<typename... Ts>
    auto operator<<(serializer& ser, const std::variant<Ts...>& var)
        -> serializer& {
        using S = uint8_t;
        static_assert(
            std::variant_size_v<std::remove_reference_t<decltype(var)>> < std::
                numeric_limits<S>::max());
        auto idx = static_cast<S>(var.index());
        ser << idx;
        std::visit(
            [&](auto&& arg) {
                ser << arg;
            },
            var);
        return ser;
    }

    /// Deserializes a variant whose alternatives are default-constructible.
    /// \see \ref cbdc::operator<<(serializer&, const std::variant<Ts...>&)
    template<typename... Ts>
    auto operator>>(serializer& deser, std::variant<Ts...>& var)
        -> std::enable_if_t<(std::is_default_constructible_v<Ts> && ...),
                            serializer&> {
        using S = uint8_t;
        static_assert(
            std::variant_size_v<std::remove_reference_t<decltype(var)>> < std::
                numeric_limits<S>::max());
        S idx{};
        deser >> idx;
        auto var_idx = static_cast<size_t>(idx);
        var = expand_type<Ts...>(var_idx);
        std::visit(
            [&](auto&& arg) {
                deser >> arg;
            },
            var);
        return deser;
    }

    /// \brief Deserializes a variant where the alternatives are
    /// all default constructible or all are not default constructible
    ///  If all alternatives are default constructible ,
    /// each type must provide a constructor of the form
    /// T(serializer&) which deserializes the type from its argument.
    /// \see \ref cbdc::operator<<(serializer&, const std::variant<Ts...>&)
    template<typename... Ts>
    [[nodiscard]] auto get_variant(serializer& deser) -> std::variant<Ts...> {
        using T = typename std::variant<Ts...>;
        using S = uint8_t;
        static_assert(std::variant_size_v<T> < std::numeric_limits<S>::max());
        // Since if the variant holds only default constructiable types
        // we will be only able to unpack it using >>
        // thus we can't extract the index value from the deser
        if constexpr((std::is_default_constructible_v<Ts> && ...)) {
            T variants;
            deser >> variants;
            return variants;
        } else {
            S idx{};
            deser >> idx;
            auto i = static_cast<size_t>(idx);
            assert(i < std::variant_size_v<T>);
            static constexpr auto t = std::array{+[](serializer& d) {
                return T{std::in_place_type<Ts>, d};
            }...};
            // TODO: deserialization error handling for variant indexes.
            return t.at(i)(deser);
        }
    }

    // TODO: use std::is_scoped_enum_v and std::to_underlying once C++23 is
    // available.
    /// Serializes an enum via its underlying type.
    template<typename T>
    auto operator<<(serializer& ser, T e) ->
        typename std::enable_if_t<std::is_enum_v<T>, serializer&> {
        return ser << static_cast<std::underlying_type_t<T>>(e);
    }

    /// Deserializes an enum.
    template<typename T>
    auto operator>>(serializer& deser, T& e) ->
        typename std::enable_if_t<std::is_enum_v<T>, serializer&> {
        std::underlying_type_t<T> val{};
        if(deser >> val) {
            e = static_cast<T>(val);
        }
        return deser;
    }
}

#endif // OPENCBDC_TX_SRC_SERIALIZATION_FORMAT_H_
