// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "buffer_serializer.hpp"

#include "uhs/sentinel/interface.hpp"
#include "uhs/transaction/messages.hpp"
#include "uhs/transaction/transaction.hpp"

#include <cstring>
#include <typeinfo>

namespace cbdc {
    buffer_serializer::buffer_serializer(cbdc::buffer& pkt) : m_pkt(pkt) {}

    buffer_serializer::operator bool() const {
        return m_valid;
    }

    void buffer_serializer::advance_cursor(size_t len) {
        m_cursor += len;
    }

    void buffer_serializer::reset() {
        m_cursor = 0;
        m_valid = true;
    }

    [[nodiscard]] auto buffer_serializer::end_of_buffer() const -> bool {
        return m_cursor >= m_pkt.size();
    }

    auto buffer_serializer::write(const void* data, size_t len) -> bool {
        if(m_cursor + len > m_pkt.size()) {
            m_pkt.extend(m_cursor + len - m_pkt.size());
        }
        std::memcpy(m_pkt.data_at(m_cursor), data, len);
        m_cursor += len;
        return true;
    }

    auto buffer_serializer::read(void* data, size_t len) -> bool {
        if(m_cursor + len > m_pkt.size()) {
            m_valid = false;
            return false;
        }
        std::memcpy(data, m_pkt.data_at(m_cursor), len);
        m_cursor += len;
        return true;
    }

    auto string_for_error(cbdc::transaction::validation::tx_error tx_err)
        -> std::string {
        switch(tx_err.index()) {
            case 0: {
                cbdc::transaction::validation::input_error err
                    = std::get<cbdc::transaction::validation::input_error>(
                        tx_err);

                switch(err.m_code) {
                    case cbdc::transaction::validation::input_error_code::
                        duplicate:
                        return "input_duplicate";
                        break;
                    case cbdc::transaction::validation::input_error_code::
                        data_error:
                        return "input_data_error";
                        break;
                }
                break;
            }
            case 1: {
                return "output_zero_value";
                break;
            }
            case 2: {
                cbdc::transaction::validation::witness_error err
                    = std::get<cbdc::transaction::validation::witness_error>(
                        tx_err);
                std::stringstream ss;

                ss << "witness_" << err.m_idx << "_";
                switch(err.m_code) {
                    case cbdc::transaction::validation::witness_error_code::
                        missing_witness_program_type:
                        ss << "missing_witness_program_type";
                        break;
                    case cbdc::transaction::validation::witness_error_code::
                        unknown_witness_program_type:
                        ss << "unknown_witness_program_type";
                        break;
                    case cbdc::transaction::validation::witness_error_code::
                        malformed:
                        ss << "malformed";
                        break;
                    case cbdc::transaction::validation::witness_error_code::
                        program_mismatch:
                        ss << "program_mismatch";
                        break;
                    case cbdc::transaction::validation::witness_error_code::
                        invalid_public_key:
                        ss << "invalid_public_key";
                        break;
                    case cbdc::transaction::validation::witness_error_code::
                        invalid_signature:
                        ss << "invalid_signature";
                        break;
                }
                return ss.str();
                break;
            }
            case 3: {
                cbdc::transaction::validation::tx_error_code err
                    = std::get<cbdc::transaction::validation::tx_error_code>(
                        tx_err);

                switch(err) {
                    case cbdc::transaction::validation::tx_error_code::
                        no_inputs:
                        return "no_inputs";
                        break;
                    case cbdc::transaction::validation::tx_error_code::
                        no_outputs:
                        return "no_outputs";
                        break;
                    case cbdc::transaction::validation::tx_error_code::
                        missing_witness:
                        return "missing_witness";
                        break;
                    case cbdc::transaction::validation::tx_error_code::
                        asymmetric_values:
                        return "asymmetric_values";
                        break;
                    case cbdc::transaction::validation::tx_error_code::
                        value_overflow:
                        return "value_overflow";
                        break;
                }
                break;
            }
        }
        return "unknown_error";
    }

    auto string_for_response(cbdc::sentinel::response resp) -> std::string {
        if(resp.m_tx_error) {
            return string_for_error(*(resp.m_tx_error));
        } else {
            switch(resp.m_tx_status) {
                case cbdc::sentinel::tx_status::pending:
                    return "pending";
                    break;
                case cbdc::sentinel::tx_status::static_invalid:
                    return "static_invalid";
                    break;
                case cbdc::sentinel::tx_status::state_invalid:
                    return "state_invalid";
                    break;
                case cbdc::sentinel::tx_status::confirmed:
                    return "confirmed";
                    break;
            }
        }
        return "unknown_status";
    }
}
