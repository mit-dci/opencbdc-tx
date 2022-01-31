// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_SERIALIZATION_STREAM_SERIALIZER_H_
#define OPENCBDC_TX_SRC_SERIALIZATION_STREAM_SERIALIZER_H_

#include "serializer.hpp"

#include <ios>

namespace cbdc {
    /// \brief Implementation of \ref serializer for std::ios.
    ///
    /// Cannot be used directly. Must be specialized for the stream type.
    /// \see istream_serializer
    class stream_serializer : public serializer {
      public:
        /// Constructor.
        /// \param s IO stream to serialize into or from.
        explicit stream_serializer(std::ios& s);

        /// Indicates whether the last serialization operation succeeded.
        /// \return true if the last serialization operation succeeded.
        explicit operator bool() const final;

      private:
        std::ios& m_str;
    };
}

#endif
