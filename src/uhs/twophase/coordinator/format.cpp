// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "controller.hpp"
#include "state_machine.hpp"
#include "uhs/transaction/messages.hpp"
#include "util/raft/messages.hpp"
#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& ser,
                    const coordinator::state_machine::coordinator_state& s)
        -> serializer& {
        return ser << s.m_prepare_txs << s.m_commit_txs << s.m_discard_txs;
    }

    auto operator>>(serializer& deser,
                    coordinator::controller::coordinator_state& s)
        -> serializer& {
        return deser >> s.m_prepare_txs >> s.m_commit_txs >> s.m_discard_txs;
    }

    auto operator<<(serializer& ser,
                    const coordinator::controller::sm_command& c)
        -> serializer& {
        ser << c.m_header;
        switch(c.m_header.m_comm) {
            case coordinator::state_machine::command::prepare: {
                const auto& data
                    = std::get<coordinator::controller::prepare_tx>(
                        c.m_data.value());
                ser << data;
                break;
            }
            case coordinator::state_machine::command::commit: {
                const auto& data
                    = std::get<coordinator::controller::commit_tx>(
                        c.m_data.value());
                ser << data;
                break;
            }
            // Discard, done and get don't have a payload
            case coordinator::state_machine::command::discard:
            case coordinator::state_machine::command::done:
            case coordinator::state_machine::command::get: {
                break;
            }
        }
        return ser;
    }

    auto operator<<(serializer& ser,
                    const coordinator::controller::sm_command_header& c)
        -> serializer& {
        return ser << static_cast<uint8_t>(c.m_comm) << c.m_dtx_id;
    }

    auto operator>>(serializer& deser,
                    coordinator::controller::sm_command_header& c)
        -> serializer& {
        uint8_t comm{};
        deser >> comm;
        c.m_comm = static_cast<coordinator::state_machine::command>(comm);
        deser >> c.m_dtx_id;
        return deser;
    }
}
