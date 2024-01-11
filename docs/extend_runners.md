# Add a new runner
This is a high-level guide to adding a new runner, smart contract computational environment, within PArSEC in a 
minimally invasive manner. 

One of the primary goals for the PArSEC work is to enable experimentation with unique computational environments and data representations, for a 
programmable centralized digital currency. 
This guide aims to make the process of setting up a unique computatational environment easier to understand, 
with the hope of speeding up future experiements.

This guide assumes working knowledge of C++ (C++17+), but is intended to be digestible for those who are new to OpenCBDC's PArSEC architecture.

This guide is paired with a Python runner intended to demonstrate how one might implement a new runner in PArSEC.

# High Level Overview
There are three main rqeuirements for effectively implementing
a runner in PArSEC, which are the following:

1. A mechanism for reading data from the shards.
2. An implementation for interpreting and running smart contract code.
3. A mechanism for writing data back to the shards.

Other additions may be useful within a runner for performance, accessibility, etc. We describe these key
components, as well as some other design notes and suggestions that may help to quickly and efficiently
spin up a new runner.

## Runner Interface
When a runner is spawned, it is given parameters from the agent, which are stored
as parameters in the runner instantiation. Briefly, 
we discuss key parameters to take note of when implementing a runnner. 

- `m_function`
    - This is where the code to be executed by the runner is stored. 
        The default control flow has this code retrieved from the shards.
        In the context of the EVM and Lua runners, this is bytecode.
        The Python runner expects uncompiled Python script stored in `m_function`,
        as well as some associated data about the function arguments and return values.
        - Note: EVM and Lua bytecode contain information about expected
            arguments and return types.

- `m_param`
    - This is where function parameters are expected to be stored. By default, this
    is a standard buffer. OpenCBDC provides a tool for serializing data into / deserializing data out of the buffer, but PArSEC does not have a defined standard for how arguments
    should be stored as parameters in the buffer.

- `m_result_callback`
    - This callback is defined within the main agent scope, and is intended to 
    handle the results of the runner computation. Typically, this will be used to
    write results back to the shards.

- `m_try_lock_callback`
    - This callback is defined within the main agent scope, and is intended to acquire
    a lock on a given shard key. This is required for reading data from, and writing data
    back to the shards. This callback also expects a function as a callback into
    the runner scope. This passed function can be used to bring the try_lock results
    into the runner scope.

## (1) Reading information from the shards
To read data from the shards, the agent needs to first acquire a lock on the key. This should be a write lock
if the value at the key is to be updated to avoid hazards, otherwise a read lock is sufficient. 

PArSEC runners have callbacks stored in `m_try_lock_callback` which are expected to initiate a lock request on the given
key from the agent body. When appropriate, this can be called from the runner. 

By default, `m_try_lock_callback` is defined as an `agent::runner:interface::try_lock_callback_type`, which is defined as follows:
```
using try_lock_callback_type
    = std::function<bool(broker::key_type,
                            broker::lock_type,
                            broker::interface::try_lock_callback_type)>;
```

For reference, `broker::interface::try_lock_callback_type` is defined as:

```
using try_lock_callback_type
    = std::function<void(try_lock_return_type)>; // We will refer to this callback function as the runner provided try_lock callback 

using try_lock_return_type
    = std::variant<value_type,
                    error_code,
                    runtime_locking_shard::shard_error>;
```

TLDR; `m_try_lock_callback` expects a shard key, a lock type (read/write), and a void returning callback function (we will refer to this as "the runner provided try_lock callback" below), which accepts 
a variant, which is a shard's value type on success, or either a broker or shard error if an error occurs during the lock acquisition.

The runner provided try_lock callback is useful for bringing the returned shard value into the runner's scope upon success, or for
handling errors on failure. In a successful context, the value can then be registered within the runner's computational 
environment (VM). Examples of this can be seen in `runners/lua/impl.cpp:handle_try_lock(...)` (embedded within the runner provided try_lock callback defined in `runners/lua/impl.cpp:schedule_contract()`), and in `runners/py/impl.cpp:handle_try_lock_input_arg(...)` (embedded within the runner provided try_lock callback in `runners/py/impl.cpp:run()`).


## (2) Running smart contract code

The main purpose of a runner environment is to execute some arbitrary code. Different runners
can handle different types of code, which can be advantageous. The three runners currently
defined within the OpenCBDC repository use off-the-shelf VMs, which as a result requires 
input information to be properly formatted such that it can be understood within the VM context. The Lua and Python runners both show a process of registering variables within 
the VM scope, then running the function code. The outputs are then also parsed to
be compatible with the remainder of PArSEC. 

## (3) Writing information to the shards
Writing data to the shards requires that the agent holds write locks on all the keys to be updated. When the runner is spawned 
from the agent, it is provided with another callback to the main agent body, `m_result_callback`. 

`m_result_callback` is intended to bring the results of the runner computation into the agent scope. This is useful for updating 
system state, specifically writing data to the shards.

`m_result_callback` is defined as a `runner::interface::run_callback_type`, which is defined as follows:
```
using run_callback_type = std::function<void(run_return_type)>;

using run_return_type
    = std::variant<runtime_locking_shard::state_update_type,
                    error_code>;

using runtime_locking_shard::state_update_type = std::
    unordered_map<key_type, value_type, hashing::const_sip_hash<key_type>>;
```

In summary, `m_result_callback` is a void returning function, which expects either a map of (key, value) updates to write to the
shards upon successful runner execution, or an error code if an error occured. 

Assuming that the specified `m_result_callback` uses the default agent implementation to write to the shards, all acquired locks
will be freed after the results are committed.


## Typical Runner Components
This section discusses how PArSEC's existing runners are implemented. These are 
not explicit requirements for new runners, though this information may be useful
to note for ease of use.

There are two main components of PArSEC's existing runners:
1. A server class which implements the interface defined by `agent/server_interface`.
2. A computational environment wrapper class which implements the interface defined by `agent/runners/interface.hpp`
    - This implementation handles passing parameters into the environment and processing results. It is spawned by the server.

## The Server
The runner specific server is used to handle requests from the user or other system components. The Lua and Python servers are based on OpenCBDC's asynchronous tcp server (`util/rpc/tcp_server.hpp`). This is a TCP RPC server which implements asynchronous request handling logic. The EVM server is based on OpenCBDC's JSON-RPC HTTP server (`util/rpc/http/json_rpc_http_server.hpp`). This is an asynchronous HTTP JSON-RPC server implementation. 

The server defined for the runner is spawned by the agent daemon, and spawns 
agent implementations. These spawned agents typically would spawn the runner type
associated with this server. The spawned agent processes the request handled by the 
server, and the agent's compute environment (runner) runs the requested computation.

The request format for the Lua and Python servers is defined in (`agent/messages.hpp`) as:

```
/// Request
/// Agent contract execution RPC request message.
struct exec_request {
    /// Key of function bytecode.
    /// By default is a cbdc::buffer
    runtime_locking_shard::key_type m_function;

    /// Function call parameter.
    /// By default is a cbdc::buffer
    parameter_type m_param;

    /// Whether the request should skip writing state changes.
    bool m_is_readonly_run{false};
};
```

and the response format is defined in (`agent/interface.hpp`) as:

```
/// Response
/// Note: error_code is an enum class of uint8_t values defining different agent error codes

using exec_return_type = std::variant<return_type, error_code>;

...

/// Note: broker::state_update_type is set to cbdc::buffer by default

using return_type = broker::state_update_type; 
```

Note: It is useful to define a new server as a friend class in `agent/server_interface.hpp`.

By default, main components of these messages are passed as `cbdc::buffer` types, which are essentially byte arrays with some provided operations (`util/common/buffer.hpp`).

## The Compute Environment Wrapper
The compute environment wrapper is spawned with some data recieved from the server. 
This is a good place to define the three main requirements discussed above.
This is also a good place to implement rules about input to and output from a runner.
Implementation optimizations can also be done here.

Suggested examples are in `parsec/agent/runners/lua/impl` and `parsec/agent/runners/py/impl`. The Python compute environment wrapper is annotated for educational purposes as well. 

## Specific notes about the Python runner
Below are some notes that may be useful when using the Python runner:

The Python runner can be spawned locally in a Bash environment `scripts/parsec_run_local.sh` when passed the argument `--runner_type=py`.

The Python runner was intended as an educational extension to PArSEC, as a jumping-off point for creating a new runner. 
It is less performant than either the EVM runner or Lua runner (based on a brief local benchmark comparison). However, this
can be remedied in several different ways, and the performance difference is likely due to the nature of the C/Python API and 
the interpreted nature of the Python contracts we have set up the runner to use. 

The provided Python smart contracts are purely for demonstration. 
Currently, the default `pay` Python smart contract provided with this code does not have a signature checking mechanism, which is of course unrealistic for 
a smart contract that would be supported in a production grade system. 
Further, the security of the Python runner has not been evaluated, and it has not been formally benchmarked at this time. This extension is not intended to suggest that 
supporting arbitary Python script as implemented here would be smart for a centralized digital currency. 

To use a Python library in a smart contract, it must be installed on the system which runs the agent. Any Python library installed on the agent should be valid within a smart contract. 

Another byproduct of the C/Python API is that it is not feasible at this time to pass messages between the C++ and Python environments during Python execution. All relevant state must be defined before running the contract. The Lua runner provides a good interface which allows updating
state within the contract during contract execution where necessary. Examining `schedule_contract()` in `agent/runners/lua/impl` demonstrates why this may be useful.

The Python runner expects strings as arguments, and the `pyBuffer` class extends the standard `cbdc::buffer` type, and 
allows for more straightforward formatting of contract parameters. See `tools/bench/py_bench.cpp` or `tests/integration/parsec_py_end_to_end_test.cpp` for examples. 

The design of the `pyBuffer` type and the expected function header format are intended to address the inherent string-type dependency of the C/Python API, as
well as the frequency of string usage in Python.

Specifically, the Python runner expects parameters to be stored in a buffer in the following format:
```
// CS = Comma Separated
// < and > are not included in the buffer, however the |'s are
<CS User Input Argument Strings>|<CS Shard Input Argument Strings (Keys)>|<CS Keys To Update After Exec>|
```
A pipe ('|') was arbitrarily chosen to indicate a separation of argument sections. 

Further, we provide a tool for converting a function in a Python script to the format expected by the Python runner in `pyutils::formContract(...)`. See `tools/bench/py_bench.cpp` or `tests/integration/parsec_py_end_to_end_test.cpp` for examples. 

The Python runner expects a function definition to be in the following format when retrieved from the shards:
```
// CS = Comma Separated
// < and > are not included in the buffer, however the |'s are
<Return Types>|<CS Return Arguments>|<CS Input Arguments>|<Function Code>
```
The header is simply a mechanism of communicating what the expected input and output values are from this function (similar to contract ABI with the EVM).

## Discussion: Choosing a smart contract environment

The smart contract environment chosen should reflect the intended use of programmability in the digital currency. Some contexts
will benefit from Turing complete programmability, and others may be better off non-Turing complete (or fully un-programmable). Further, optimizations can be made to improve the efficiency of certain computations. The EVM, Lua, or Python may not be suitible 
choices for all programmable digital currencies. With PArSEC, we look to encourage exploration of different types of 
programmability and their effectiveness.

## Future Work and Next Steps
The next step in this series would be to create a guide on modifying the back-end of PArSEC.
This will come soon, but would be expedited if there's significant interest.

It may prove useful to optimize the Python runner, or experiment with different contracts written in Python. It also may 
prove useful to evaluate other computational environments in PArSEC. 
