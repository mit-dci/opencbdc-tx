// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CBDC_UNIVERSE0_SRC_3PC_RUNTIME_LOCKING_SHARD_REPLICATED_SHARD_H_
#define CBDC_UNIVERSE0_SRC_3PC_RUNTIME_LOCKING_SHARD_REPLICATED_SHARD_H_

#include "replicated_shard_interface.hpp"

#include <mutex>

namespace cbdc::threepc::runtime_locking_shard {
    /// Implementation of the replicated shard interface. Stores the shard
    /// state and unfinished ticket in memory. Thread-safe.
    class replicated_shard : public replicated_shard_interface {
      public:
        /// \copydoc replicated_shard_interface::prepare
        /// \return true.
        auto prepare(ticket_number_type ticket_number,
                     broker_id_type broker_id,
                     state_type state_update,
                     callback_type result_callback) -> bool override;

        /// \copydoc replicated_shard_interface::commit
        /// \return true.
        auto commit(ticket_number_type ticket_number,
                    callback_type result_callback) -> bool override;

        /// \copydoc replicated_shard_interface::finish
        /// \return true.
        auto finish(ticket_number_type ticket_number,
                    callback_type result_callback) -> bool override;

        /// \copydoc replicated_shard_interface::get_tickets
        /// \return true.
        auto get_tickets(get_tickets_callback_type result_callback) const
            -> bool override;

        /// Return the keys and values stored by the shard.
        /// \return shard state.
        auto get_state() const -> state_type;

      private:
        mutable std::mutex m_mut;
        state_type m_state;
        tickets_type m_tickets;
    };
}

#endif
