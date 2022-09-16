// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.hpp"

#include <unordered_map>

namespace cbdc::threepc {
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

        return cfg;
    }
}
