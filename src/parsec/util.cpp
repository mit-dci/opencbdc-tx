// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.hpp"

#include <future>
#include <unordered_map>

namespace cbdc::parsec {
    auto split(const std::string& s, const std::string& delim)
        -> std::vector<std::string> {
        size_t pos_start{};
        size_t pos_end{};
        std::vector<std::string> ret;

        while((pos_end = s.find(delim, pos_start)) != std::string::npos) {
            auto token = s.substr(pos_start, pos_end - pos_start);
            pos_start = pos_end + delim.size();
            ret.emplace_back(token);
        }

        ret.emplace_back(s.substr(pos_start));
        return ret;
    }

    auto parse_args(int argc, char** argv)
        -> std::optional<std::unordered_map<std::string, std::string>> {
        auto opts = std::unordered_map<std::string, std::string>();
        auto args = cbdc::config::get_args(argc, argv);
        for(size_t i = 1; i < args.size(); i++) {
            const auto& arg = args[i];
            auto arr = split(arg, "--");
            if(arr.size() != 2 || !arr[0].empty() || arr[1].empty()) {
                return std::nullopt;
            }

            auto elems = split(arr[1], "=");
            if(elems.size() != 2) {
                return std::nullopt;
            }

            opts.emplace(elems[0], elems[1]);
        }
        return opts;
    }

    auto
    read_endpoints(const std::unordered_map<std::string, std::string>& opts,
                   const std::string& component_name)
        -> std::optional<std::vector<network::endpoint_t>> {
        auto ret = std::vector<network::endpoint_t>();

        auto count_key = component_name + "_count";
        auto it = opts.find(count_key);
        if(it == opts.end()) {
            return std::nullopt;
        }
        auto count = std::stoull(it->second);

        for(size_t i = 0; i < count; i++) {
            auto ep_key = component_name + std::to_string(i) + "_endpoint";
            it = opts.find(ep_key);
            if(it == opts.end()) {
                return std::nullopt;
            }
            auto ep = cbdc::config::parse_ip_port(it->second);
            ret.emplace_back(ep);
        }

        return ret;
    }

    auto read_cluster_endpoints(
        const std::unordered_map<std::string, std::string>& opts,
        const std::string& component_name)
        -> std::optional<std::vector<std::vector<network::endpoint_t>>> {
        auto ret = std::vector<std::vector<network::endpoint_t>>();

        auto count_key = component_name + "_count";
        auto it = opts.find(count_key);
        if(it == opts.end()) {
            return std::nullopt;
        }
        auto count = std::stoull(it->second);

        for(size_t i = 0; i < count; i++) {
            auto node_name = component_name + std::to_string(i);
            auto eps = read_endpoints(opts, node_name);
            if(!eps.has_value()) {
                return std::nullopt;
            }
            ret.emplace_back(eps.value());
        }

        return ret;
    }

    auto read_config(int argc, char** argv) -> std::optional<config> {
        // TODO: refactor, make config parsing generic
        auto opts = parse_args(argc, argv);
        if(!opts.has_value()) {
            return std::nullopt;
        }

        auto cfg = config{};

        constexpr auto component_id_key = "component_id";
        auto it = opts->find(component_id_key);
        if(it == opts->end()) {
            return std::nullopt;
        }
        cfg.m_component_id = std::stoull(it->second);

        cfg.m_loglevel = logging::log_level::trace;
        constexpr auto loglevel_key = "loglevel";
        it = opts->find(loglevel_key);
        if(it != opts->end()) {
            auto maybe_loglevel = logging::parse_loglevel(it->second);
            if(maybe_loglevel.has_value()) {
                cfg.m_loglevel = maybe_loglevel.value();
            }
        }

        auto ticket_machine_endpoints
            = read_endpoints(opts.value(), "ticket_machine");
        if(!ticket_machine_endpoints.has_value()) {
            return std::nullopt;
        }
        cfg.m_ticket_machine_endpoints = ticket_machine_endpoints.value();

        constexpr auto node_id_key = "node_id";
        it = opts->find(node_id_key);
        if(it != opts->end()) {
            cfg.m_node_id = std::stoull(it->second);
        }

        auto shard_endpoints = read_cluster_endpoints(opts.value(), "shard");
        if(!shard_endpoints.has_value()) {
            return std::nullopt;
        }
        cfg.m_shard_endpoints = shard_endpoints.value();

        auto agent_endpoints = read_endpoints(opts.value(), "agent");
        if(!agent_endpoints.has_value()) {
            return std::nullopt;
        }
        cfg.m_agent_endpoints = agent_endpoints.value();

        constexpr auto loadgen_txtype_key = "loadgen_txtype";
        it = opts->find(loadgen_txtype_key);
        if(it != opts->end()) {
            const auto& val = it->second;
            if(val == "transfer") {
                cfg.m_load_type = load_type::transfer;
            } else if(val == "erc20") {
                cfg.m_load_type = load_type::erc20;
            } else {
                return std::nullopt;
            }
        }

        cfg.m_contention_rate = 0.0;
        constexpr auto contention_rate_key = "contention_rate";
        it = opts->find(contention_rate_key);
        if(it != opts->end()) {
            cfg.m_contention_rate = std::stod(it->second);
        }

        constexpr auto default_loadgen_accounts = 1000;
        cfg.m_loadgen_accounts = default_loadgen_accounts;
        constexpr auto loadgen_accounts_key = "loadgen_accounts";
        it = opts->find(loadgen_accounts_key);
        if(it != opts->end()) {
            cfg.m_loadgen_accounts = std::stoull(it->second);
        }

        constexpr auto runner_type_key = "runner_type";
        it = opts->find(runner_type_key);
        if(it != opts->end()) {
            const auto& val = it->second;
            if(val == "evm") {
                cfg.m_runner_type = runner_type::evm;
            } else if(val == "lua") {
                cfg.m_runner_type = runner_type::lua;
            } else if(val == "py") {
                cfg.m_runner_type = runner_type::py;
            } else {
                return std::nullopt;
            }
        }

        return cfg;
    }

    auto put_row(const std::shared_ptr<broker::interface>& broker,
                 broker::key_type key,
                 broker::value_type value,
                 const std::function<void(bool)>& result_callback) -> bool {
        auto begin_res = broker->begin([=](auto begin_ret) {
            if(!std::holds_alternative<
                   cbdc::parsec::ticket_machine::ticket_number_type>(
                   begin_ret)) {
                result_callback(false);
                return;
            }

            auto ticket_number
                = std::get<cbdc::parsec::ticket_machine::ticket_number_type>(
                    begin_ret);
            auto lock_res = broker->try_lock(
                ticket_number,
                key,
                cbdc::parsec::runtime_locking_shard::lock_type::write,
                [=](auto try_lock_res) {
                    if(!std::holds_alternative<cbdc::buffer>(try_lock_res)) {
                        result_callback(false);
                        return;
                    }
                    auto commit_res = broker->commit(
                        ticket_number,
                        {{key, value}},
                        [=](auto commit_ret) {
                            if(commit_ret.has_value()) {
                                result_callback(false);
                                return;
                            }
                            auto finish_res = broker->finish(
                                ticket_number,
                                [=](auto finish_ret) {
                                    result_callback(!finish_ret.has_value());
                                });
                            if(!finish_res) {
                                result_callback(false);
                                return;
                            }
                        });
                    if(!commit_res) {
                        result_callback(false);
                        return;
                    }
                });
            if(!lock_res) {
                result_callback(false);
                return;
            }
        });
        return begin_res;
    }

    auto get_row(const std::shared_ptr<broker::interface>& broker,
                 broker::key_type key,
                 const std::function<void(
                     cbdc::parsec::broker::interface::try_lock_return_type)>&
                     result_callback)
        -> cbdc::parsec::broker::interface::try_lock_return_type {
        std::promise<cbdc::parsec::broker::interface::try_lock_return_type>
            res_promise;
        auto res_future = res_promise.get_future();
        broker->begin([&](auto begin_ret) {
            if(!std::holds_alternative<
                   cbdc::parsec::ticket_machine::ticket_number_type>(
                   begin_ret)) {
                res_promise.set_value(
                    cbdc::parsec::broker::interface::error_code::
                        ticket_number_assignment);
                result_callback(cbdc::parsec::broker::interface::error_code::
                                    ticket_number_assignment);
                return;
            }

            auto ticket_number
                = std::get<cbdc::parsec::ticket_machine::ticket_number_type>(
                    begin_ret);
            auto lock_res = broker->try_lock(
                ticket_number,
                key,
                cbdc::parsec::runtime_locking_shard::lock_type::read,
                [&](auto try_lock_res) {
                    if(!std::holds_alternative<cbdc::buffer>(try_lock_res)) {
                        res_promise.set_value(
                            cbdc::parsec::broker::interface::error_code::
                                shard_unreachable);
                        result_callback(cbdc::parsec::broker::interface::
                                            error_code::shard_unreachable);
                        return;
                    }
                    res_promise.set_value(try_lock_res);
                    result_callback(try_lock_res);

                    auto commit_res = broker->commit(
                        ticket_number,
                        runtime_locking_shard::state_update_type(),
                        [=](auto commit_ret) {
                            if(commit_ret.has_value()) {
                                if(std::holds_alternative<
                                       cbdc::parsec::broker::interface::
                                           error_code>(commit_ret.value())) {
                                    result_callback(
                                        std::get<cbdc::parsec::broker::
                                                     interface::error_code>(
                                            commit_ret.value()));
                                } else {
                                    result_callback(
                                        std::get<cbdc::parsec::
                                                     runtime_locking_shard::
                                                         shard_error>(
                                            commit_ret.value()));
                                }
                                return;
                            }
                            auto finish_res = broker->finish(
                                ticket_number,
                                [=](auto finish_ret) {
                                    if(finish_ret.has_value()) {
                                        result_callback(finish_ret.value());
                                    }
                                });
                            if(!finish_res) {
                                result_callback(
                                    cbdc::parsec::broker::interface::
                                        error_code::finish_error);
                                return;
                            }
                        });
                    if(!commit_res) {
                        result_callback(cbdc::parsec::broker::interface::
                                            error_code::commit_error);
                        return;
                    }
                });
            if(!lock_res) {
                result_callback(cbdc::parsec::broker::interface::error_code::
                                    shard_unreachable);
                return;
            }
        });
        return res_future.get();
    }
}
