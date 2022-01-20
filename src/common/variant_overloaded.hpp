// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COMMON_VARIANT_OVERLOADED_H_
#define OPENCBDC_TX_SRC_COMMON_VARIANT_OVERLOADED_H_

#include <array>
#include <cassert>
#include <cstddef>
#include <variant>

namespace cbdc {
    /// \brief Variant handler template
    ///
    /// Provides template structure for defining handlers for std::variant
    /// types in an std::visit function.
    ///
    /// Example:
    /// \code{.cpp}
    ///      std::variant<A, B> somevar = ...;
    ///      std::visit(overloaded{
    ///                     [&](const A&) {...},
    ///                     [&](const B&) {...}
    ///                 },
    ///                 somevar);
    /// \endcode
    ///
    /// \tparam Ts lambda overloads
    template<class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };

    template<class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    /// \brief Default-constructs a std::variant from a template parameter pack
    ///
    /// Particularly helpful as a helper for deserialization of variants
    ///
    /// \tparam Ts the template parameter pack containing the variant's
    ///         alternative types
    /// \param i the index of the alternative type for the variant to hold
    /// \return the default-constructed variant
    template<typename... Ts>
    [[nodiscard]] auto expand_type(size_t i) -> std::variant<Ts...> {
        static_assert((std::is_default_constructible_v<Ts> && ...));
        using T = typename std::variant<Ts...>;
        assert(i < std::variant_size_v<T>);
        static constexpr auto t = std::array{+[]() {
            return T{Ts{}};
        }...};
        // TODO: deserialization error handling for variant indexes.
        return t.at(i)();
    }
}

#endif // OPENCBDC_TX_SRC_COMMON_VARIANT_OVERLOADED_H_
