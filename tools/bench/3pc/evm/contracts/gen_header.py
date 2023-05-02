# Copyright (c) 2022 MIT Digital Currency Initiative,
#                    Federal Reserve Bank of Boston
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import json
import os
import re

# Conversion method from camelCase to snake_case
snake_convert_pattern = re.compile(r'(?<!^)(?=[A-Z])')
def to_snake(name):
    return snake_convert_pattern.sub('_',name).lower()


# contracts specifies a dictionary of the compiled artifact location for
# each contract we want to have included in contracts.hpp - with its short
# name as value. The shortnames cannot collide and must be unique. The
# resulting header file will contain a function data_<shortname>_deploy()
# that returns the m_input data for deploying the contract, and
# data_<shortname>_<methodname> for generating the input data necessary to
# call the given method on the contract
contracts = {'artifacts/contracts/ERC20.sol/Token.json':'erc20'}

# Load the JSON outputs of the hardhat compilation for each contract we want
# to include in the header file
loaded_contracts = {}
for k, v in contracts.items():
    with open(k) as f:
        loaded_contracts[v] = json.load(f)

# Make sure our output folder exists
if not os.path.exists('cpp_header'):
    os.makedirs('cpp_header')

with open('cpp_header/contracts.hpp', 'w+') as f:
    # Write the standard copyright header in the header file
    f.write('// Copyright (c) 2022 MIT Digital Currency Initiative,\n')
    f.write('//                    Federal Reserve Bank of Boston\n')
    f.write('// Distributed under the MIT software license, see the accompanying\n')
    f.write('// file COPYING or http://www.opensource.org/licenses/mit-license.php.\n\n')
    f.write('#ifndef OPENCBDC_TX_TOOLS_BENCH_3PC_EVM_CONTRACTS_H_\n')
    f.write('#define OPENCBDC_TX_TOOLS_BENCH_3PC_EVM_CONTRACTS_H_\n\n')
    f.write('#include "util/common/buffer.hpp"\n\n')
    f.write('#include "3pc/agent/runners/evm/hash.hpp"\n\n')

    # The first 4 bytes of the input data sent to a contract are the method
    # selector in ETH. It is the first 4 bytes of keccak256(<method signature>)
    f.write('namespace cbdc::threepc::evm_contracts {\n')


    # The first 4 bytes of the input data sent to a contract are the method
    # selector in ETH. It is the first 4 bytes of keccak256(<method signature>)
    f.write('  static constexpr size_t selector_size = 4;\n')

    # Parameters in a method call are always 32 bytes
    f.write('  static constexpr size_t param_size = 32;\n\n')

    # Because parameters are 32 bytes, addresses need to be copied at a 12 bytes
    # offset
    f.write('  static constexpr size_t address_param_offset = 12; // in ABIs addresses are also 32 bytes\n')

    # Generate methods for all contracts
    for k, v in loaded_contracts.items():
        # The data needed to deploy the contract, which is essentially the
        # byte code parameter in the compiled asset JSON
        f.write('  auto data_{}_deploy() -> cbdc::buffer {{\n'.format(k))
        f.write('    auto buf = cbdc::buffer::from_hex("{}");\n'.format(v['bytecode'][2:]))
        f.write('    return buf.value();\n')
        f.write('  }\n\n')

        # Loop over the functions in the ABI
        for abi in v['abi']:
            # Only make methods for functions, ignore events (for now)
            if abi['type'] == 'function':
                # Write the method name data_<shortname>_<methodname>
                f.write('auto data_{}_{}('.format(k, to_snake(abi['name'])))

                # Write all parameters as function arguments
                inp_idx = 0
                for inp in abi['inputs']:
                    tp = 'bytes32'
                    if inp['type'] == 'uint256':
                        tp = 'uint256be'
                    if inp['type'] == 'address':
                        tp = 'address'
                    if inp_idx > 0:
                        f.write(', ')
                    f.write('evmc::{} {}'.format(tp, to_snake(inp['name'])))
                    inp_idx = inp_idx + 1

                # Write the return method and creation of the empty buffer
                f.write(') -> cbdc::buffer {\n')
                f.write('    auto buf = cbdc::buffer();\n')

                # Write the method selector calculation
                f.write('    const auto selector_{name} = std::string("{name_raw}('.format_map(dict({'name':to_snake(abi['name']),'name_raw':abi['name']})))
                inp_idx = 0
                for inp in abi['inputs']:
                    if inp_idx > 0:
                        f.write(',')
                    f.write(inp['type'])
                    inp_idx = inp_idx + 1
                f.write(')");\n')

                # Write calculation of the selector hash and appending it to the buffer
                f.write('    auto selector_hash = cbdc::keccak_data(selector_{name}.data(), selector_{name}.size());\n'.format_map(dict({'name':to_snake(abi['name'])})))
                f.write('    buf.append(selector_hash.data(), selector_size);\n')

                # Write code that appends the parameters to the buffer (if any)
                if len(abi['inputs']) > 0:
                    for i, inp in enumerate(abi['inputs']):
                        if inp['type'] == 'address':
                            f.write('  buf.extend(address_param_offset);\n')
                        f.write('  buf.append({name}.bytes, sizeof({name}.bytes));\n'.format_map(dict({'name':to_snake(inp['name'])})))

                # Return the buffer we built
                f.write('    return buf;\n')
                f.write('  }\n\n')

    f.write('}\n\n')
    f.write('#endif // OPENCBDC_TX_TOOLS_BENCH_3PC_EVM_CONTRACTS_H_\n\n')

