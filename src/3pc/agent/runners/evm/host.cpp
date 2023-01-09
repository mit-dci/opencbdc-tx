// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "host.hpp"

#include "address.hpp"
#include "crypto/sha256.h"
#include "format.hpp"
#include "hash.hpp"
#include "math.hpp"
#include "rlp.hpp"
#include "serialization.hpp"
#include "util.hpp"

#include <cassert>
#include <evmc/hex.hpp>
#include <evmone/evmone.h>
#include <future>

namespace cbdc::threepc::agent::runner {
    evm_host::evm_host(std::shared_ptr<logging::log> log,
                       interface::try_lock_callback_type try_lock_callback,
                       evmc_tx_context tx_context,
                       evm_tx tx,
                       bool dry_run,
                       interface::ticket_number_type ticket_number)
        : m_log(std::move(log)),
          m_try_lock_callback(std::move(try_lock_callback)),
          m_tx_context(tx_context),
          m_tx(std::move(tx)),
          m_dry_run(dry_run),
          m_ticket_number(ticket_number) {
        m_receipt.m_tx = m_tx;
        m_receipt.m_ticket_number = m_ticket_number;
    }

    auto evm_host::get_account(const evmc::address& addr, bool write) const
        -> std::optional<evm_account> {
        m_log->trace(this,
                     "EVM request account:",
                     to_hex(addr),
                     "- Write:",
                     write);

        if(is_precompile(addr)) {
            // Precompile contract, return empty account
            m_accessed_addresses.insert(addr);
            return evm_account{};
        }

        auto it = m_accounts.find(addr);
        if(it != m_accounts.end() && (it->second.second || !write)) {
            return it->second.first;
        }

        m_log->trace(
            this,
            "EVM request account not in cache or wrong lock, getting [",
            to_hex(addr),
            "]");

        auto addr_key = make_buffer(addr);
        auto maybe_v = get_key(addr_key, write);
        if(!maybe_v.has_value()) {
            return std::nullopt;
        }

        m_accessed_addresses.insert(addr);
        auto& v = maybe_v.value();
        if(v.size() == 0) {
            m_accounts[addr] = {std::nullopt, write};
            return std::nullopt;
        }
        auto maybe_acc = from_buffer<evm_account>(v);
        assert(maybe_acc.has_value());
        auto& acc = maybe_acc.value();
        m_accounts[addr] = {acc, write};
        return acc;
    }

    auto evm_host::account_exists(const evmc::address& addr) const noexcept
        -> bool {
        const auto* log_str = "evm_account_exists";
        m_log->trace(log_str, to_hex(addr));
        auto maybe_acc = get_account(addr, false);
        if(!maybe_acc.has_value()) {
            return false;
        }
        auto& acc = maybe_acc.value();
        return !acc.m_destruct;
    }

    auto evm_host::get_storage(const evmc::address& addr,
                               const evmc::bytes32& key) const noexcept
        -> evmc::bytes32 {
        const auto* log_str = "evm_get_storage";
        m_log->trace(log_str, to_hex(addr), to_hex(key));
        auto maybe_storage = get_account_storage(addr, key, false);
        return maybe_storage.value_or(evmc::bytes32{});
    }

    auto evm_host::set_storage(const evmc::address& addr,
                               const evmc::bytes32& key,
                               const evmc::bytes32& value) noexcept
        -> evmc_storage_status {
        const auto* log_str = "evm_set_storage";
        m_log->trace(log_str, to_hex(addr), to_hex(key), to_hex(value));

        auto ret_val = std::optional<evmc_storage_status>();
        auto maybe_acc = get_account(addr, false);
        if(!maybe_acc.has_value()) {
            if(!m_dry_run) {
                maybe_acc = get_account(addr, true);
                assert(!maybe_acc.has_value());
            }
            maybe_acc = evm_account();
            ret_val = EVMC_STORAGE_ADDED;
            maybe_acc.value().m_modified.insert(key);
            m_accounts[addr] = {maybe_acc.value(), !m_dry_run};
        }
        auto& acc = maybe_acc.value();

        auto maybe_storage = get_account_storage(addr, key, !m_dry_run);
        auto prev_value = maybe_storage.value_or(evmc::bytes32{});

        auto modified = acc.m_modified.find(key) != acc.m_modified.end();
        if(!ret_val.has_value()) {
            if(prev_value == value) {
                ret_val = EVMC_STORAGE_UNCHANGED;
            } else if(evmc::is_zero(value) && !modified) {
                ret_val = EVMC_STORAGE_DELETED;
            } else if(modified) {
                ret_val = EVMC_STORAGE_MODIFIED_AGAIN;
            } else {
                ret_val = EVMC_STORAGE_MODIFIED;
                acc.m_modified.insert(key);
            }
        }
        m_account_storage[addr][key] = {value, !m_dry_run};
        m_accounts[addr].first = acc;
        assert(ret_val.has_value());
        return ret_val.value();
    }

    auto evm_host::get_balance(const evmc::address& addr) const noexcept
        -> evmc::uint256be {
        const auto* log_str = "evm_get_balance";
        m_log->trace(log_str, to_hex(addr));
        auto maybe_acc = get_account(addr, false);
        if(!maybe_acc.has_value()) {
            return {};
        }
        auto& acc = maybe_acc.value();
        return acc.m_balance;
    }

    auto evm_host::get_code_size(const evmc::address& addr) const noexcept
        -> size_t {
        const auto* log_str = "evm_get_code_size";
        m_log->trace(log_str, to_hex(addr));
        if(is_precompile(addr)) {
            // Precompiles have no code, but this should be
            // non-zero for the call to work
            return 1;
        }
        auto maybe_code = get_account_code(addr, false);
        return maybe_code.value_or(evm_account_code{}).size();
    }

    auto evm_host::get_code_hash(const evmc::address& addr) const noexcept
        -> evmc::bytes32 {
        const auto* log_str = "evm_get_code_hash";
        m_log->trace(log_str, to_hex(addr));

        auto maybe_code = get_account_code(addr, false);
        if(!maybe_code.has_value()) {
            return {};
        }
        auto& code = maybe_code.value();
        auto sha = CSHA256();
        sha.Write(code.data(), code.size());
        auto ret = evmc::bytes32();
        sha.Finalize(&ret.bytes[0]);
        return ret;
    }

    auto evm_host::copy_code(const evmc::address& addr,
                             size_t code_offset,
                             uint8_t* buffer_data,
                             size_t buffer_size) const noexcept -> size_t {
        const auto* log_str = "evm_copy_code";
        m_log->trace(log_str, to_hex(addr), code_offset);

        auto maybe_code = get_account_code(addr, false);
        if(!maybe_code.has_value()) {
            return 0;
        }

        const auto& code = maybe_code.value();

        if(code_offset >= code.size()) {
            return 0;
        }

        const auto n = std::min(buffer_size, code.size() - code_offset);
        if(n > 0) {
            std::copy_n(&code[code_offset], n, buffer_data);
        }
        return n;
    }

    void evm_host::selfdestruct(const evmc::address& addr,
                                const evmc::address& beneficiary) noexcept {
        m_log->trace("EVM selfdestruct:", to_hex(addr), to_hex(beneficiary));
        // TODO: delete storage keys and code
        transfer(addr, beneficiary, evmc::uint256be{});
    }

    auto evm_host::create(const evmc_message& msg) noexcept -> evmc::result {
        auto maybe_sender_acc = get_account(msg.sender, false);
        assert(maybe_sender_acc.has_value());
        auto& sender_acc = maybe_sender_acc.value();

        auto new_addr = evmc::address();
        if(msg.kind == EVMC_CREATE) {
            new_addr = contract_address(msg.sender, sender_acc.m_nonce);
        } else {
            auto bytecode_hash
                = cbdc::keccak_data(msg.input_data, msg.input_size);
            new_addr = contract_address2(msg.sender,
                                         msg.create2_salt,
                                         bytecode_hash);
        }

        // Transfer endowment to deployed contract account
        if(!evmc::is_zero(msg.value)) {
            transfer(msg.sender, new_addr, msg.value);
        }

        if(msg.depth == 0) {
            m_receipt.m_create_address = new_addr;
        }

        auto call_msg = evmc_message();
        call_msg.depth = msg.depth;
        call_msg.sender = msg.sender;
        call_msg.value = msg.value;
        call_msg.recipient = new_addr;
        call_msg.kind = EVMC_CALL;
        // TODO: do we need to deduct some gas for contract creation
        // here?
        call_msg.gas = msg.gas;

        auto res = execute(call_msg, msg.input_data, msg.input_size);

        if(res.status_code == EVMC_SUCCESS) {
            auto maybe_acc = get_account(new_addr, !m_dry_run);
            if(!maybe_acc.has_value()) {
                maybe_acc = evm_account();
            }
            auto& acc = maybe_acc.value();
            m_accounts[new_addr] = {acc, !m_dry_run};

            auto maybe_code = get_account_code(new_addr, !m_dry_run);
            if(!maybe_code.has_value()) {
                maybe_code = evm_account_code();
            }
            auto& code = maybe_code.value();
            code.resize(res.output_size);
            std::memcpy(code.data(), res.output_data, res.output_size);
            m_account_code[new_addr] = {code, !m_dry_run};
        }

        if(msg.depth == 0) {
            m_receipt.m_output_data.resize(res.output_size);
            std::memcpy(m_receipt.m_output_data.data(),
                        res.output_data,
                        res.output_size);
            m_receipt.m_success
                = res.status_code == evmc_status_code::EVMC_SUCCESS;
        }

        return res;
    }

    auto evm_host::call(const evmc_message& msg) noexcept -> evmc::result {
        if(msg.kind == EVMC_CREATE2 || msg.kind == EVMC_CREATE) {
            return create(msg);
        }

        // Transfer message value from sender account to recipient
        if(!evmc::is_zero(msg.value) && msg.kind == EVMC_CALL) {
            // TODO: do DELETEGATECALL and CALLCODE transfer value as
            // well?
            transfer(msg.sender, msg.recipient, msg.value);
        }

        auto code_addr
            = msg.kind == EVMC_DELEGATECALL || msg.kind == EVMC_CALLCODE
                ? msg.code_address
                : msg.recipient;

        auto code_size = get_code_size(code_addr);
        if(code_size == 0) {
            // TODO: deduct simple send fixed gas amount
            auto res = evmc::make_result(evmc_status_code::EVMC_SUCCESS,
                                         msg.gas,
                                         nullptr,
                                         0);
            return evmc::result(res);
        }

        auto code_buf = std::vector<uint8_t>(code_size);
        [[maybe_unused]] auto n
            = copy_code(code_addr, 0, code_buf.data(), code_buf.size());
        assert(n == code_size);

        auto inp = cbdc::buffer();
        inp.append(msg.input_data, msg.input_size);
        m_log->trace("EVM call:",
                     to_hex(code_addr),
                     msg.kind,
                     msg.flags,
                     msg.depth,
                     inp.to_hex());

        auto res = execute(msg, code_buf.data(), code_buf.size());

        if(msg.depth == 0) {
            // TODO: refactor branch into call epilog method
            m_receipt.m_output_data.resize(res.output_size);
            std::memcpy(m_receipt.m_output_data.data(),
                        res.output_data,
                        res.output_size);
            m_receipt.m_success
                = res.status_code == evmc_status_code::EVMC_SUCCESS;
        }

        return res;
    }

    auto evm_host::get_tx_context() const noexcept -> evmc_tx_context {
        return m_tx_context;
    }

    auto evm_host::get_block_hash(int64_t /* number */) const noexcept
        -> evmc::bytes32 {
        // TODO: there are no blocks for this host. Ensure it's okay to
        // always return 0.
        return {};
    }

    void evm_host::emit_log(
        const evmc::address& addr,
        const uint8_t* data,
        size_t data_size,
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        const evmc::bytes32 topics[],
        size_t topics_count) noexcept {
        auto l = evm_log();
        l.m_addr = addr;
        l.m_data.resize(data_size);
        std::memcpy(l.m_data.data(), data, data_size);
        for(size_t i = 0; i < topics_count; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            l.m_topics.push_back(topics[i]);
        }
        m_receipt.m_logs.push_back(l);
    }

    auto evm_host::access_account(const evmc::address& addr) noexcept
        -> evmc_access_status {
        m_log->trace("EVM access_account:", to_hex(addr));
        if(m_accessed_addresses.find(addr) != m_accessed_addresses.end()) {
            return EVMC_ACCESS_WARM;
        }
        m_accessed_addresses.insert(addr);
        return EVMC_ACCESS_COLD;
    }

    auto evm_host::access_storage(const evmc::address& addr,
                                  const evmc::bytes32& key) noexcept
        -> evmc_access_status {
        m_log->trace("EVM access_storage:", to_hex(addr), to_hex(key));
        auto elem = std::make_pair(addr, key);
        if(m_accessed_storage_keys.find(elem)
           != m_accessed_storage_keys.end()) {
            return EVMC_ACCESS_WARM;
        }
        m_accessed_storage_keys.insert(elem);
        return EVMC_ACCESS_COLD;
    }

    auto evm_host::ticket_number_key(
        std::optional<interface::ticket_number_type> tn) const
        -> cbdc::buffer {
        if(!tn) {
            tn = m_ticket_number;
        }
        auto tn_buf = cbdc::make_buffer(tn.value());
        CSHA256 sha;
        hash_t tn_hash;

        sha.Write(tn_buf.c_ptr(), tn_buf.size());
        sha.Finalize(tn_hash.data());

        return make_buffer(tn_hash);
    }

    auto evm_host::log_index_key(
        evmc::address addr,
        std::optional<interface::ticket_number_type> tn) const
        -> cbdc::buffer {
        if(!tn) {
            tn = m_ticket_number;
        }
        auto tn_buf = cbdc::make_buffer(tn.value());
        CSHA256 sha;
        hash_t log_index_hash;
        sha.Write(addr.bytes, sizeof(addr.bytes));
        sha.Write(tn_buf.c_ptr(), tn_buf.size());
        sha.Finalize(log_index_hash.data());

        return make_buffer(log_index_hash);
    }

    auto evm_host::get_log_index_keys() const -> std::vector<cbdc::buffer> {
        auto logs = get_sorted_logs();
        auto keys = std::vector<cbdc::buffer>();
        for(auto& log : logs) {
            keys.push_back(log_index_key(log.first));
        }
        return keys;
    }

    auto evm_host::get_sorted_logs() const
        -> std::unordered_map<evmc::address, std::vector<evm_log>> {
        auto ret = std::unordered_map<evmc::address, std::vector<evm_log>>();
        for(const auto& log : m_receipt.m_logs) {
            if(ret.find(log.m_addr) == ret.end()) {
                ret.insert({log.m_addr, {}});
            }
            ret[log.m_addr].push_back(log);
        }
        return ret;
    }

    auto evm_host::get_state_updates() const
        -> runtime_locking_shard::state_update_type {
        auto ret = runtime_locking_shard::state_update_type();
        for(auto& [addr, acc_data] : m_accounts) {
            auto& [acc, write] = acc_data;
            if(!acc.has_value() || !write) {
                continue;
            }
            auto key = make_buffer(addr);
            auto val = cbdc::buffer();
            if(!acc->m_destruct) {
                val = make_buffer(*acc);
            }
            ret[key] = val;
        }

        for(auto& [addr, acc_code] : m_account_code) {
            auto& [code, write] = acc_code;
            if(!code.has_value() || !write) {
                continue;
            }
            auto key = make_buffer(code_key{addr});
            auto val = make_buffer(*code);
            ret[key] = val;
        }

        for(auto& [addr, acc_storage] : m_account_storage) {
            for(auto& [k, elem] : acc_storage) {
                auto& [value, write] = elem;
                if(!value.has_value() || !write) {
                    continue;
                }
                auto key = make_buffer(storage_key{addr, k});
                auto val = make_buffer(*value);
                ret[key] = val;
            }
        }

        const auto tx = std::make_shared<evm_tx>(m_tx);
        auto txid = tx_id(*tx);
        auto tid = make_buffer(txid);
        auto r = make_buffer(m_receipt);
        ret[tid] = r;
        ret[ticket_number_key()] = tid;

        auto ordered_logs = get_sorted_logs();
        for(auto& addr_log : ordered_logs) {
            auto log_idx = evm_log_index();
            log_idx.m_ticket_number = m_ticket_number;
            log_idx.m_txid = txid;
            log_idx.m_logs = addr_log.second;
            ret[log_index_key(addr_log.first)] = make_buffer(log_idx);
        }
        return ret;
    }

    auto evm_host::should_retry() const -> bool {
        return m_retry;
    }

    void evm_host::insert_account(const evmc::address& addr,
                                  const evm_account& acc) {
        m_accounts.insert({addr, {acc, !m_dry_run}});
        m_accessed_addresses.insert(addr);
        m_init_state = m_accounts;
    }

    void evm_host::transfer(const evmc::address& from,
                            const evmc::address& to,
                            const evmc::uint256be& value) {
        const auto* log_str = "evm_transfer";
        m_log->trace(log_str, to_hex(from), to_hex(to));
        auto maybe_acc = get_account(from, !m_dry_run);
        assert(maybe_acc.has_value());
        auto& acc = maybe_acc.value();
        auto val = value;
        if(evmc::is_zero(value)) {
            // Special case: destruct the from account if we're
            // transfering the entire account balance
            val = acc.m_balance;
            acc.m_destruct = true;
        }
        acc.m_balance = acc.m_balance - val;
        m_accounts[from] = {acc, !m_dry_run};

        auto maybe_to_acc = get_account(to, !m_dry_run);
        if(!maybe_to_acc.has_value()) {
            // Create the to account if it doesn't exist
            maybe_to_acc = evm_account();
        }
        auto& to_acc = maybe_to_acc.value();
        to_acc.m_balance = to_acc.m_balance + val;
        m_accounts[to] = {to_acc, !m_dry_run};
    }

    void evm_host::finalize(int64_t gas_left, int64_t gas_used) {
        if(!m_dry_run) {
            auto maybe_acc = get_account(m_tx_context.tx_origin, true);
            assert(maybe_acc.has_value());
            auto& acc = maybe_acc.value();
            auto gas_refund = evmc::uint256be(static_cast<uint64_t>(gas_left))
                            * m_tx_context.tx_gas_price;
            acc.m_balance = acc.m_balance + gas_refund;
            m_accounts[m_tx_context.tx_origin] = {acc, true};
        }
        m_receipt.m_gas_used
            = evmc::uint256be(static_cast<uint64_t>(gas_used));
        m_receipt.m_timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    void evm_host::revert() {
        m_accounts = m_init_state;
    }

    auto evm_host::get_tx_receipt() const -> evm_tx_receipt {
        return m_receipt;
    }

    auto evm_host::is_precompile(const evmc::address& addr) -> bool {
        auto addr_copy = addr;
        constexpr auto precompile_suffix_sz = sizeof(uint16_t);
        std::memset(
            &addr_copy.bytes[sizeof(addr_copy.bytes) - precompile_suffix_sz],
            0,
            precompile_suffix_sz);
        return evmc::is_zero(addr_copy)
            && addr.bytes[sizeof(addr_copy.bytes) - 1] != 0;
    }

    auto evm_host::get_account_storage(const evmc::address& addr,
                                       const evmc::bytes32& key,
                                       bool write) const
        -> std::optional<evmc::bytes32> {
        m_log->trace("EVM request account storage:",
                     to_hex(addr),
                     to_hex(key));

        if(is_precompile(addr)) {
            // Precompile contract, return empty account
            m_accessed_addresses.insert(addr);
            return std::nullopt;
        }

        auto it = m_account_storage.find(addr);
        if(it != m_account_storage.end()) {
            auto& m = it->second;
            auto itt = m.find(key);
            if(itt != m.end() && (itt->second.second || !write)) {
                return itt->second.first;
            }
        }

        auto elem_key = make_buffer(storage_key{addr, key});
        auto maybe_v = get_key(elem_key, write);
        if(!maybe_v.has_value()) {
            return std::nullopt;
        }

        m_accessed_addresses.insert(addr);
        auto& v = maybe_v.value();
        if(v.size() == 0) {
            m_account_storage[addr][key] = {std::nullopt, write};
            return std::nullopt;
        }
        auto maybe_data = from_buffer<evmc::bytes32>(v);
        assert(maybe_data.has_value());
        auto& data = maybe_data.value();
        m_account_storage[addr][key] = {data, write};
        return data;
    }

    auto evm_host::get_account_code(const evmc::address& addr,
                                    bool write) const
        -> std::optional<evm_account_code> {
        m_log->trace("EVM request account code:", to_hex(addr));

        if(is_precompile(addr)) {
            // Precompile contract, return empty account
            m_accessed_addresses.insert(addr);
            return std::nullopt;
        }

        auto it = m_account_code.find(addr);
        if(it != m_account_code.end() && (it->second.second || !write)) {
            return it->second.first;
        }

        auto elem_key = make_buffer(code_key{addr});
        auto maybe_v = get_key(elem_key, write);
        if(!maybe_v.has_value()) {
            return std::nullopt;
        }

        m_accessed_addresses.insert(addr);
        auto& v = maybe_v.value();
        if(v.size() == 0) {
            m_account_code[addr] = {std::nullopt, write};
            return std::nullopt;
        }
        auto maybe_code = from_buffer<evm_account_code>(v);
        assert(maybe_code.has_value());
        auto& code = maybe_code.value();
        m_account_code[addr] = {code, write};
        return code;
    }

    auto evm_host::get_key(const cbdc::buffer& key, bool write) const
        -> std::optional<broker::value_type> {
        const auto* log_str = "get_key";
        auto res_prom
            = std::promise<broker::interface::try_lock_return_type>();
        auto res_fut = res_prom.get_future();

        m_log->trace(m_ticket_number, log_str, key.to_hex(), "write =", write);

        auto ret = m_try_lock_callback(
            key,
            write ? broker::lock_type::write : broker::lock_type::read,
            [&](const broker::interface::try_lock_return_type& res) {
                res_prom.set_value(res);
            });

        if(!ret) {
            m_log->trace(m_ticket_number,
                         "failed to make try_lock request, retrying");
            m_retry = true;
            return std::nullopt;
        }

        for(size_t i = 0;; i++) {
            auto status = res_fut.wait_for(std::chrono::seconds(1));
            if(status == std::future_status::ready) {
                break;
            }
            m_log->trace(m_ticket_number,
                         "still waits for",
                         key.to_hex(),
                         write,
                         i);
        }

        auto res = res_fut.get();

        m_log->trace(m_ticket_number, "got key", key.to_hex());

        return std::visit(
            overloaded{
                [&](broker::value_type& val) -> std::optional<cbdc::buffer> {
                    return val;
                },
                [&](broker::interface::error_code& /* err */)
                    -> std::optional<cbdc::buffer> {
                    m_retry = true;
                    return std::nullopt;
                },
                [&](runtime_locking_shard::shard_error& /* err */)
                    -> std::optional<cbdc::buffer> {
                    m_retry = true;
                    return std::nullopt;
                },
            },
            res);
    }

    auto evm_host::execute(const evmc_message& msg,
                           const uint8_t* code,
                           size_t code_size) -> evmc::result {
        // Make VM instance if we didn't already
        if(!m_vm) {
            m_vm = std::make_unique<evmc::VM>(evmc_create_evmone());
            if(!(*m_vm)) {
                m_log->error("Unable to load EVM implementation");
                auto res = evmc::make_result(evmc_status_code::EVMC_FAILURE,
                                             msg.gas,
                                             nullptr,
                                             0);
                return evmc::result(res);
            }
        }

        auto res = m_vm->execute(*this,
                                 EVMC_LATEST_STABLE_REVISION,
                                 msg,
                                 code,
                                 code_size);

        return res;
    }
}
