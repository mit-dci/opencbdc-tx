// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "watchtower.hpp"

#include "common/variant_overloaded.hpp"
#include "serialization/format.hpp"
#include "status_update.hpp"

#include <algorithm>

namespace cbdc::watchtower {
    void watchtower::add_block(cbdc::atomizer::block&& blk) {
        std::unique_lock lk(m_bc_mut);
        m_bc.push_block(std::move(blk));
    }

    void watchtower::add_errors(std::vector<tx_error>&& errs) {
        std::shared_lock lk0(m_bc_mut, std::defer_lock);
        std::unique_lock lk1(m_ec_mut, std::defer_lock);
        std::lock(lk0, lk1);
        auto repeated_tx_filter = [&](const auto& err) -> bool {
            auto res = false;
            auto check_uhs = [&](const hash_t& err_tx_id, auto&& info) {
                for(const auto& uhs : info.input_uhs_ids()) {
                    if(auto spent = m_bc.check_spent(uhs)) {
                        auto [height, tx_id] = spent.value();
                        if(err_tx_id == tx_id) {
                            res = true;
                            return;
                        }
                    }
                    if(auto unspent = m_bc.check_unspent(uhs)) {
                        auto [height, tx_id] = unspent.value();
                        if(err_tx_id == tx_id) {
                            res = true;
                            return;
                        }
                    }
                }
            };
            std::visit(overloaded{[&](tx_error_inputs_spent&& info) {
                                      check_uhs(err.tx_id(), std::move(info));
                                  },
                                  [&](tx_error_inputs_dne&& info) {
                                      check_uhs(err.tx_id(), std::move(info));
                                  },
                                  [&](auto&& /* info */) {}},
                       std::move(err.info()));
            return res;
        };
        errs.erase(
            std::remove_if(errs.begin(), errs.end(), repeated_tx_filter),
            errs.end());
        m_ec.push_errors(std::move(errs));
    }

    auto watchtower::check_uhs_id_statuses(const std::vector<hash_t>& uhs_ids,
                                           const hash_t& tx_id,
                                           bool internal_err,
                                           bool tx_err,
                                           uint64_t best_height)
        -> std::vector<status_update_state> {
        std::vector<status_update_state> states;
        states.reserve(uhs_ids.size());
        for(const auto& uhs_id : uhs_ids) {
            auto found_status = false;
            if(internal_err) {
                states.emplace_back(
                    status_update_state{search_status::internal_error,
                                        best_height,
                                        uhs_id});
                found_status = true;
            } else if(tx_err) {
                if(auto uhs_err = m_ec.check_uhs_id(uhs_id)) {
                    states.emplace_back(
                        status_update_state{search_status::invalid_input,
                                            best_height,
                                            uhs_id});
                } else {
                    states.emplace_back(
                        status_update_state{search_status::tx_rejected,
                                            best_height,
                                            uhs_id});
                }
                found_status = true;
            }
            if(auto spent = m_bc.check_spent(uhs_id)) {
                auto [height, s_tx_id] = spent.value();
                if(s_tx_id == tx_id) {
                    states.emplace_back(
                        status_update_state{search_status::spent,
                                            height,
                                            uhs_id});
                    found_status = true;
                }
            } else if(auto unspent = m_bc.check_unspent(uhs_id)) {
                auto [height, us_tx_id] = unspent.value();
                if(us_tx_id == tx_id) {
                    states.emplace_back(
                        status_update_state{search_status::unspent,
                                            height,
                                            uhs_id});
                    found_status = true;
                }
            }
            if(!found_status) {
                states.emplace_back(
                    status_update_state{search_status::no_history,
                                        best_height,
                                        uhs_id});
            }
        }
        return states;
    }

    auto
    watchtower::handle_status_update_request(const status_update_request& req)
        -> std::unique_ptr<response> {
        std::unordered_map<hash_t,
                           std::vector<status_update_state>,
                           hashing::const_sip_hash<hash_t>>
            chks;
        {
            std::shared_lock lk0(m_bc_mut, std::defer_lock);
            std::shared_lock lk1(m_ec_mut, std::defer_lock);
            std::lock(lk0, lk1);
            auto best_height = m_bc.best_block_height();
            for(const auto& [tx_id, uhs_ids] : req.uhs_ids()) {
                auto tx_err = m_ec.check_tx_id(tx_id);
                bool internal_err{false};
                if(tx_err.has_value()
                   && (std::holds_alternative<tx_error_sync>(
                           tx_err.value().info())
                       || std::holds_alternative<tx_error_stxo_range>(
                           tx_err.value().info()))) {
                    internal_err = true;
                }
                auto states = check_uhs_id_statuses(uhs_ids,
                                                    tx_id,
                                                    internal_err,
                                                    tx_err.has_value(),
                                                    best_height);
                chks.emplace(std::make_pair(tx_id, std::move(states)));
            }
        }

        return std::make_unique<response>(status_request_check_success{chks});
    }

    auto watchtower::handle_best_block_height_request(
        const best_block_height_request& /* unused */)
        -> std::unique_ptr<response> {
        std::shared_lock lk(m_bc_mut);
        return std::make_unique<response>(
            best_block_height_response{m_bc.best_block_height()});
    }

    watchtower::watchtower(size_t block_cache_size, size_t error_cache_size)
        : m_bc{block_cache_size},
          m_ec{error_cache_size} {}

    auto best_block_height_request::operator==(
        const best_block_height_request& /* unused */) const -> bool {
        return true;
    }

    best_block_height_request::best_block_height_request(serializer& pkt) {
        pkt >> *this;
    }

    auto best_block_height_response::operator==(
        const best_block_height_response& rhs) const -> bool {
        return (rhs.m_height == m_height);
    }

    best_block_height_response::best_block_height_response(uint64_t height)
        : m_height(height) {}

    best_block_height_response::best_block_height_response(serializer& pkt) {
        pkt >> *this;
    }

    auto best_block_height_response::height() const -> uint64_t {
        return m_height;
    }

    auto request::operator==(const request& rhs) const -> bool {
        return m_req == rhs.m_req;
    }

    request::request(request_t req) : m_req(std::move(req)) {}

    request::request(serializer& pkt)
        : m_req(get_variant<status_update_request, best_block_height_request>(
            pkt)) {}

    auto request::payload() const -> const request_t& {
        return m_req;
    }

    auto response::operator==(const response& rhs) const -> bool {
        return m_resp == rhs.m_resp;
    }

    response::response(response_t resp) : m_resp(std::move(resp)) {}

    response::response(serializer& pkt)
        : m_resp(get_variant<status_request_check_success,
                             best_block_height_response>(pkt)) {}

    auto response::payload() const -> const response_t& {
        return m_resp;
    }
}
