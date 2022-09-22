// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "interface.hpp"

namespace cbdc::threepc::agent {
    interface::interface(runtime_locking_shard::key_type function,
                         parameter_type param,
                         exec_callback_type result_callback)
        : m_function(std::move(function)),
          m_param(std::move(param)),
          m_result_callback(std::move(result_callback)) {}

    auto interface::get_function() const -> runtime_locking_shard::key_type {
        return m_function;
    }

    auto interface::get_param() const -> parameter_type {
        return m_param;
    }

    auto interface::get_result_callback() const -> exec_callback_type {
        return m_result_callback;
    }
}
