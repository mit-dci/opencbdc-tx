// Copyright (c) 2023 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "impl.hpp"

#include "crypto/sha256.h"
#include "util/common/keys.hpp"
#include "util/common/variant_overloaded.hpp"

#include <cassert>
#include <thread>

namespace cbdc::parsec::agent::runner {
    py_runner::py_runner(std::shared_ptr<logging::log> logger,
                         const cbdc::parsec::config& cfg,
                         runtime_locking_shard::value_type function,
                         parameter_type param,
                         bool is_readonly_run,
                         run_callback_type result_callback,
                         try_lock_callback_type try_lock_callback,
                         std::shared_ptr<secp256k1_context> secp,
                         std::shared_ptr<thread_pool> t_pool,
                         ticket_number_type ticket_number)
        : interface(std::move(logger),
                    cfg,
                    std::move(function),
                    std::move(param),
                    is_readonly_run,
                    std::move(result_callback),
                    std::move(try_lock_callback),
                    std::move(secp),
                    std::move(t_pool),
                    ticket_number) {}

    auto py_runner::run() -> bool {
        m_log->info("calling run");

        Py_Initialize();

        PyObject* main = PyImport_AddModule("__main__");
        PyObject* globalDictionary = PyModule_GetDict(main);
        PyObject* localDictionary = PyDict_New();

        if(m_function.size() == 0) {
            m_log->warn(
                "Contract has length 0, key may be invalid. Bailing out.");
            m_result_callback(runtime_locking_shard::state_update_type());
            return true;
        }

        // Parse contract header data, trim from m_function.
        parse_header();
        // Parse the parameters buffer into its component strings.
        auto params = parse_params();

        for(runtime_locking_shard::key_type& key : m_shard_inputs) {
            auto dest = cbdc::buffer();
            auto valid_resp = std::promise<bool>();

            /// \todo These read locks should be freed before proceeding to the next step
            auto locktype = runtime_locking_shard::lock_type::read;
            if(std::find(m_update_keys.begin(), m_update_keys.end(), key)
               != m_update_keys.end()) {
                locktype = runtime_locking_shard::lock_type::write;
            }

            // This is where the shard inputs are retrieved.
            auto success = m_try_lock_callback(key, locktype, [&](auto res) {
                auto data = handle_try_lock_input_arg(std::move(res), dest);
                valid_resp.set_value(data);
            });

            if(!success) {
                m_log->error("Failed to issue try lock command");
                m_result_callback(error_code::internal_error);
            }

            if(dest.data() == nullptr) {
                m_log->warn(key.c_str(),
                            "has no associated data. Defining value stored at",
                            key.c_str(),
                            "to be \"0\"");
                dest.append("0", 2);
            }
            params.emplace_back(dest.c_str());
        }

        if(m_input_args.size() != params.size()) {
            m_log->error("Incorrect number of arguments passed to function");
            return true;
        }

        for(unsigned long i = 0; i < params.size(); i++) {
            PyObject* value = PyUnicode_FromString(params[i].c_str());
            PyDict_SetItemString(localDictionary,
                                 m_input_args[i].c_str(),
                                 value);
        }

        auto* r = PyRun_String(m_function.c_str(),
                               Py_file_input,
                               globalDictionary,
                               localDictionary);
        if(r == nullptr) {
            m_log->error("Python VM generated error:", r);
        }
        update_state(localDictionary);
        if(Py_FinalizeEx() < 0) {
            m_log->fatal("Py not finalized correctly");
        }

        m_log->trace("Done running");
        return true;
    }

    void py_runner::parse_header() {
        /* Assumes that header is <n returns> <n inputs> | return types |
           return args | input types | input args | function code */
        auto functionString
            = std::string(static_cast<char*>(m_function.data()));

        m_input_args.clear();
        m_return_args.clear();
        m_return_types.clear();

        auto arg_delim = functionString.find('|');
        auto arg_string = functionString.substr(0, arg_delim);
        size_t pos = 0;
        m_return_types = arg_string;
        functionString.erase(0, arg_delim + 1);

        arg_delim = functionString.find('|');
        arg_string = functionString.substr(0, arg_delim);
        while((pos = arg_string.find(',', 0)) != std::string::npos) {
            m_return_args.push_back(arg_string.substr(0, pos));
            arg_string.erase(0, pos + 1);
        }
        functionString.erase(0, arg_delim + 1);

        arg_delim = functionString.find('|');
        arg_string = functionString.substr(0, arg_delim);
        while((pos = arg_string.find(',', 0)) != std::string::npos) {
            m_input_args.push_back(arg_string.substr(0, pos));
            arg_string.erase(0, pos + 1);
        }
        functionString.erase(0, arg_delim + 1);

        m_function = cbdc::buffer();
        m_function.append(functionString.c_str(), functionString.size());
        m_function.append("\0", 1);
    }

    auto py_runner::parse_params() -> std::vector<std::string> {
        std::vector<std::string> params(0);

        // Input validation to ensure that it's safe to use strlen
        if(m_param.size() == 0) {
            m_log->error("m_param contains no data");
            return params;
        }
        m_param.append("\0", 1);
        size_t pipeCount = 0;
        size_t stringCount = 0;
        auto paramString
            = std::string(static_cast<char*>(m_param.data()), m_param.size());
        for(size_t i = 0; i < m_param.size(); i++) {
            if(paramString[i] == '|') {
                pipeCount++;
            } else if(i > 0 && paramString[i] == '\0'
                      && paramString[i - 1] != '\0') {
                stringCount++;
            }
        }
        auto bail = false;
        if(pipeCount != 3) {
            m_log->error("m_param sections are improperly formatted");
            bail = true;
        } else if(stringCount
                  != m_input_args.size() + m_return_args.size() + pipeCount) {
            m_log->error("m_param contains too few arguments or arguments are "
                         "improperly formatted");
            bail = true;
        }
        if(bail) {
            return params;
        }
        // ---

        // get parameters that are user inputs
        size_t bufferOffset = 0;
        auto it = 0;
        // Known that there are 3 parts of the header
        while(it < 3) {
            auto parsedParam = std::string(
                static_cast<char*>(m_param.data_at(bufferOffset)));
            if(parsedParam[0] == '|') {
                it++;
                bufferOffset += 2;
                continue;
            }
            switch(it) {
                    /*
                    In first section, take data and place in params
                    In second section, take data and place in m_shard_inputs
                    In third section, take data and place in m_update_keys
                    */
                case 0:
                    params.emplace_back(std::string(parsedParam));
                    bufferOffset += params.back().length() + 1;
                    break;
                case 1: {
                    auto tmp = cbdc::buffer();
                    tmp.append(parsedParam.c_str(), parsedParam.length() + 1);
                    m_shard_inputs.emplace_back(tmp);
                    bufferOffset += tmp.size();
                    break;
                }
                case 2: {
                    auto tmp = cbdc::buffer();
                    tmp.append(parsedParam.c_str(), parsedParam.length() + 1);
                    m_update_keys.emplace_back(tmp);
                    bufferOffset += tmp.size();
                    break;
                }
                default:
                    m_log->fatal(
                        "Reading past the number of input group "
                        "sections. Something has gone seriously wrong.");
                    continue;
            }
        }

        return params;
    }

    void py_runner::update_state(PyObject* localDictionary) {
        auto updates = runtime_locking_shard::state_update_type();
        get_state_updates(localDictionary);
        if(m_update_keys.size() != m_return_args.size()) {
            m_log->error(m_update_keys.size(),
                         "keys found",
                         m_return_args.size(),
                         "expected");
        }
        m_log->trace("Adding updates to map");
        for(size_t i = 0; i < m_return_values.size(); i++) {
            /// Get locks that we don't already hold
            /// \todo Is this slower than just requesting the lock ?
            if(std::find(m_shard_inputs.begin(),
                         m_shard_inputs.end(),
                         m_update_keys[i])
               == m_shard_inputs.end()) {
                auto success
                    = m_try_lock_callback(m_update_keys[i],
                                          broker::lock_type::write,
                                          [&](auto res) {
                                              handle_try_lock(std::move(res));
                                          });
                if(!success) {
                    m_log->error("Failed to issue try lock command");
                    m_result_callback(error_code::internal_error);
                }
            }
            m_log->trace("Update",
                         m_update_keys[i].c_str(),
                         m_return_values[i].c_str());
            updates.emplace(m_update_keys[i], m_return_values[i]);
        }

        // Communicate updates to agent scope. Will write back to shards.
        m_result_callback(std::move(updates));
    }

    void py_runner::handle_try_lock(
        const broker::interface::try_lock_return_type& res) {
        auto maybe_error = std::visit(
            overloaded{
                [&]([[maybe_unused]] const broker::value_type& v)
                    -> std::optional<error_code> {
                    m_log->trace(
                        v.c_str(),
                        "Returned from try_lock request to handle_try_lock.");
                    m_log->warn("Discarding returned value. Use "
                                "handle_try_lock_input_arg to store result.");
                    return std::nullopt;
                },
                [&](const broker::interface::error_code& /* e */)
                    -> std::optional<error_code> {
                    m_log->error("Broker error acquiring lock");
                    return error_code::lock_error;
                },
                [&](const runtime_locking_shard::shard_error& e)
                    -> std::optional<error_code> {
                    if(e.m_error_code
                       == runtime_locking_shard::error_code::wounded) {
                        return error_code::wounded;
                    }
                    m_log->error("Shard error acquiring lock");
                    return error_code::lock_error;
                }},
            res);
        if(maybe_error.has_value()) {
            m_result_callback(maybe_error.value());
        }
    }

    auto py_runner::handle_try_lock_input_arg(
        const broker::interface::try_lock_return_type& res,
        broker::value_type& dest) -> bool {
        auto maybe_error = std::visit(
            overloaded{[&]([[maybe_unused]] const broker::value_type& v)
                           -> std::optional<error_code> {
                           m_log->trace("broker return input arg", v.c_str());
                           if(v.data() == nullptr) {
                               m_log->warn(
                                   "Value at given key accessed, but key has "
                                   "no data. Saving empty buffer.");
                           }
                           dest = v;
                           return std::nullopt;
                       },
                       [&](const broker::interface::error_code& /* e */)
                           -> std::optional<error_code> {
                           m_log->error("Broker error acquiring lock");
                           return error_code::lock_error;
                       },
                       [&](const runtime_locking_shard::shard_error& e)
                           -> std::optional<error_code> {
                           if(e.m_error_code
                              == runtime_locking_shard::error_code::wounded) {
                               return error_code::wounded;
                           }
                           m_log->error("Shard error acquiring lock");
                           return error_code::lock_error;
                       }},
            res);
        if(maybe_error.has_value()) {
            m_result_callback(maybe_error.value());
            return false;
        }
        return true;
    }

    void py_runner::get_state_updates(PyObject* localDictionary) {
        for(size_t i = 0; i < m_return_types.size(); i++) {
            auto ch = m_return_types[i];
            m_log->trace("Parsing:", m_return_args[i].c_str());
            auto* word = PyDict_GetItemString(localDictionary,
                                              m_return_args[i].c_str());
            auto buf = cbdc::buffer();
            switch(ch) {
                case 'l': {
                    m_log->trace("Parsing long");
                    auto res = PyLong_AsLong(word);
                    buf.append(&res, sizeof(long));
                    break;
                }
                case 'd': {
                    m_log->trace("Parsing double");
                    auto res = PyFloat_AsDouble(word);
                    buf.append(&res, sizeof(double));
                    break;
                }
                case 's': {
                    m_log->trace("Parsing string");
                    char* res = nullptr;
                    if(PyUnicode_Check(word)) {
                        res = PyBytes_AsString(
                            PyUnicode_AsEncodedString(word,
                                                      "UTF-8",
                                                      "strict"));
                    } else {
                        res = PyBytes_AsString(word);
                    }
                    if(res != nullptr) {
                        // PyBytes_AsString returns a null terminated string
                        buf.append(res, strlen(res) + 1);
                    }
                    break;
                }
                case '?': {
                    m_log->trace("Parsing bool");
                    auto res = PyLong_AsLong(word);
                    auto b = (res != 0);
                    buf.append(&b, sizeof(bool));
                    break;
                }
                case 'c': {
                    m_log->trace("Parsing char");
                    char* res = nullptr;
                    if(PyUnicode_Check(word)) {
                        res = PyBytes_AsString(
                            PyUnicode_AsEncodedString(word,
                                                      "UTF-8",
                                                      "strict"));
                    } else {
                        res = PyBytes_AsString(word);
                    }
                    if(res != nullptr) {
                        buf.append(res, strnlen(res, 1));
                    }
                    break;
                }
                default:
                    m_log->warn("Unsupported Return type from function");
                    break;
            }
            m_return_values.push_back(buf);
        }
    }
}
