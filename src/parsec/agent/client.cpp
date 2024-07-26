// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "format.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::parsec::agent::rpc {
    client::client(std::vector<network::endpoint_t> endpoints)
        : m_client(std::make_unique<decltype(m_client)::element_type>(
              std::move(endpoints))) {}

    auto client::init() -> bool {
        return m_client->init();
    }

    auto client::exec(runtime_locking_shard::key_type function,
                      parameter_type param,
                      bool is_readonly_run,
                      const interface::exec_callback_type& result_callback)
        -> bool {
        auto req
            = request{std::move(function), std::move(param), is_readonly_run};
        return m_client->call(std::move(req),
                              [result_callback](std::optional<response> resp) {
                                  assert(resp.has_value());
                                  result_callback(resp.value());
                              });
    }
}
