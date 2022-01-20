// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "error_cache.hpp"

#include "common/variant_overloaded.hpp"

namespace cbdc::watchtower {
    error_cache::error_cache(size_t k) : m_k_errs(k) {
        m_tx_id_errs.reserve(k);
        m_uhs_errs.reserve(k);
    }

    void error_cache::push_errors(std::vector<tx_error>&& errs) {
        for(auto&& err : errs) {
            if((m_k_errs != 0) && (m_errs.size() == m_k_errs)) {
                auto& old_err = m_errs.front();
                m_tx_id_errs.erase(old_err->tx_id());

                std::visit(overloaded{
                               [](const tx_error_sync& /* unused */) {},
                               [&](const tx_error_inputs_dne& arg) {
                                   for(auto& uhs_id : arg.input_uhs_ids()) {
                                       m_uhs_errs.erase(uhs_id);
                                   }
                               },
                               [&](const tx_error_inputs_spent& arg) {
                                   for(const auto& uhs_id :
                                       arg.input_uhs_ids()) {
                                       m_uhs_errs.erase(uhs_id);
                                   }
                               },
                               [](const tx_error_stxo_range& /* unused */) {},
                               [](const tx_error_incomplete& /* unused */) {},
                           },
                           old_err->info());

                m_errs.pop();
            }

            auto new_err = std::make_shared<tx_error>(std::move(err));
            m_errs.push(new_err);
            m_tx_id_errs.insert({new_err->tx_id(), new_err});
            std::visit(overloaded{
                           [](const tx_error_sync& /* unused */) {},
                           [&](const tx_error_inputs_dne& arg) {
                               for(auto& uhs_id : arg.input_uhs_ids()) {
                                   m_uhs_errs.insert({uhs_id, new_err});
                               }
                           },
                           [&](const tx_error_inputs_spent& arg) {
                               for(const auto& uhs_id : arg.input_uhs_ids()) {
                                   m_uhs_errs.insert({uhs_id, new_err});
                               }
                           },
                           [](const tx_error_stxo_range& /* unused */) {},
                           [](const tx_error_incomplete& /* unused */) {},
                       },
                       new_err->info());
        }
    }

    auto error_cache::check_tx_id(const hash_t& tx_id) const
        -> std::optional<tx_error> {
        auto res = m_tx_id_errs.find(tx_id);
        if(res == m_tx_id_errs.end()) {
            return std::nullopt;
        }
        return *res->second;
    }

    auto error_cache::check_uhs_id(const hash_t& uhs_id) const
        -> std::optional<tx_error> {
        auto res = m_uhs_errs.find(uhs_id);
        if(res == m_uhs_errs.end()) {
            return std::nullopt;
        }
        return *res->second;
    }
}
