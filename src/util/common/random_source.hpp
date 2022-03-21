// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/** \file random_source.hpp
 *  Pseudorandom number generator (PRNG) for generating random data from a
 * given entropy source.
 */

#ifndef OPENCBDC_TX_SRC_COMMON_RANDOM_SOURCE_H_
#define OPENCBDC_TX_SRC_COMMON_RANDOM_SOURCE_H_

#include "crypto/sha3.h"
#include "hash.hpp"

#include <limits>
#include <mutex>
#include <queue>

namespace cbdc {
    /// Generates pseudo-random numbers from a given entropy source.
    /// Construct with the path to an entropy source (usually /dev/urandom)
    /// to seed the pseudo-random number generator. After initial setup,
    /// random_source uses SHA256 to generate further random numbers using a
    /// counter. Compatible with std::uniform_int_distribution.
    class random_source {
      public:
        using result_type = unsigned int;

        /// Constructor. Seeds the SHA256 engine with the entropy source.
        /// \param source_file path to a file to use as a seed.
        explicit random_source(const std::string& source_file);
        ~random_source() = default;

        random_source(const random_source& other) = delete;
        auto operator=(const random_source& other) = delete;

        random_source(random_source&& other) = delete;
        auto operator=(random_source&& other) = delete;

        /// Returns a new random integer.
        /// \return random integer.
        auto operator()() -> result_type;

        /// Returns the minimum random value this source can produce.
        /// \return the minimum random value.
        static constexpr auto min() -> result_type {
            return std::numeric_limits<result_type>::min();
        }

        /// Returns the maximum random value this source can produce.
        /// \return the maximum random value.
        static constexpr auto max() -> result_type {
            return std::numeric_limits<result_type>::max();
        }

        /// \brief Returns a random 32-byte hash value.
        ///
        /// Can generate secure-random unique identifiers (UIDs) or private
        /// keys.
        /// \return a random 32-byte value.
        auto random_hash() -> hash_t;

      private:
        auto hash_at_index(uint64_t idx) -> hash_t;

        std::mutex m_mut;

        std::queue<unsigned char> m_buf;

        SHA3_256 m_sha;
        uint64_t m_counter{};
    };
}

#endif // OPENCBDC_TX_SRC_COMMON_RANDOM_SOURCE_H_
