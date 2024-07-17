# Copyright (c) 2022 MIT Digital Currency Initiative,
#                    Federal Reserve Bank of Boston
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import os
import json
import re

copyright_license = [
    '// Copyright (c) 2022 MIT Digital Currency Initiative,',
    '//                    Federal Reserve Bank of Boston',
    '// Distributed under the MIT software license, see the accompanying',
    '// file COPYING or http://www.opensource.org/licenses/mit-license.php.'
]

# contracts specifies a dictionary of the compiled artifact location for
# each contract we want to have included in contracts.hpp - with its short
# name as value. The shortnames cannot collide and must be unique. The
# resulting header file will contain a function data_<shortname>_deploy()
# that returns the m_input data for deploying the contract, and
# data_<shortname>_<methodname> for generating the input data necessary to
# call the given method on the contract
contracts_dict = {'artifacts/contracts/ERC20.sol/Token.json':'erc20'}

# helper functions
def create_loaded_contracts(contracts: dict) -> dict:
    '''
    Load the JSON outputs of the hardhat compilation for
    each contract we want to include in the header file
    '''
    loaded_contracts = {}
    contracts_read = 0
    for k, v in contracts.items():
        try:
            with open(k, 'r', encoding='utf-8') as file:
                loaded_contracts[v] = json.load(file)
                contracts_read += 1
        except FileNotFoundError:
            print(f'File {k} not found, skipping')
            continue
        except IOError:
            print(f'Error reading {k}, skipping')
            continue

    if contracts_read == 0:
        print('No contracts loaded, exiting')
        exit(1)

    return loaded_contracts

def camel_to_snake(name) -> str:
    '''
    Function to convert camelCase to snake_case
    '''
    snake_convert_pattern = re.compile(r'(?<!^)(?=[A-Z])')
    return snake_convert_pattern.sub('_', name).lower()

# main function
def write_header_file(loaded_contracts: dict) -> None:
    '''
    Function to write the header file
    '''
    # Make sure our output folder exists
    output_folder = 'cpp_header'
    output_file = f'{output_folder}/contracts.hpp'
    os.makedirs(output_folder, exist_ok=True)

    with open(output_file, 'w+', encoding='utf-8') as f:
        # Write the standard copyright header in the header file
        for line in copyright_license:
            f.write(f'{line}\n')
        f.write('\n')

        f.write('#ifndef OPENCBDC_TX_TOOLS_BENCH_PARSEC_EVM_CONTRACTS_H_\n')
        f.write('#define OPENCBDC_TX_TOOLS_BENCH_PARSEC_EVM_CONTRACTS_H_\n\n')
        f.write('#include "util/common/buffer.hpp"\n\n')
        f.write('#include "parsec/agent/runners/evm/hash.hpp"\n\n')

        # Write the namespace for the contracts
        f.write('namespace cbdc::parsec::evm_contracts {\n')

        # The first 4 bytes of the input data sent to a contract is the
        # method selector in ETH. It is the first 4 bytes of
        # keccak256(<method signature>)
        f.write('  static constexpr size_t selector_size = 4;\n')

        # Parameters in a method call are always 32 bytes
        f.write('  static constexpr size_t param_size = 32;\n\n')

        # Since params are 32 bytes, addrs must be copied at a 12 bytes offset
        f.write('  static constexpr size_t address_param_offset = 12; // in ABIs addresses are also 32 bytes\n')

        # Generate methods for all contracts
        for k, v in loaded_contracts.items():
            # The data needed to deploy the contract, which is essentially the
            # byte code parameter in the compiled asset JSON
            f.write(f'  auto data_{k}_deploy() -> cbdc::buffer {{\n')
            f.write(f'    auto buf = cbdc::buffer::from_hex("{v["bytecode"][2:]}");\n')
            f.write('    return buf.value();\n')
            f.write('  }\n\n')

            # Loop over the functions in the ABI
            for abi in v['abi']:
                # Only make methods for functions, ignore events (for now)
                if abi['type'] == 'function':
                    # Write the method name data_<shortname>_<methodname>
                    f.write(f'auto data_{k}_{camel_to_snake(abi["name"])}(')
                    # Write all parameters as function arguments
                    for idx, inp in enumerate(abi['inputs']):
                        tp = 'bytes32'
                        if inp['type'] == 'uint256':
                            tp = 'uint256be'
                        if inp['type'] == 'address':
                            tp = 'address'
                        if idx > 0:
                            f.write(', ')
                        f.write(f'evmc::{tp} {camel_to_snake(inp["name"])}')

                    # Write the return method and creation of the empty buffer
                    f.write(') -> cbdc::buffer {\n')
                    f.write('    auto buf = cbdc::buffer();\n')

                    # Write the method selector calculation
                    f.write('    const auto selector_{name} = std::string("{name_raw}('.format_map(dict({'name':camel_to_snake(abi['name']),'name_raw':abi['name']})))
                    for idx, inp in enumerate(abi['inputs']):
                        if idx > 0:
                            f.write(',')
                        f.write(inp['type'])
                    f.write(')");\n')

                    # Write calculation of the selector hash and appending it to the buffer
                    f.write('    auto selector_hash = cbdc::keccak_data(selector_{name}.data(), selector_{name}.size());\n'.format_map(dict({'name':camel_to_snake(abi['name'])})))
                    f.write('    buf.append(selector_hash.data(), selector_size);\n')

                    # Write code that appends the params to the buffer (if any)
                    if len(abi['inputs']) > 0:
                        for inp in abi['inputs']:
                            if inp['type'] == 'address':
                                f.write('  buf.extend(address_param_offset);\n')
                            f.write('  buf.append({name}.bytes, sizeof({name}.bytes));\n'.format_map(dict({'name':camel_to_snake(inp['name'])})))
                    f.write('    return buf;\n  }\n\n')

        f.write('}\n\n')
        f.write('#endif // OPENCBDC_TX_TOOLS_BENCH_PARSEC_EVM_CONTRACTS_H_\n\n')


if __name__ == '__main__':

    # Load the contracts
    loaded_contracts_dict = create_loaded_contracts(contracts_dict)

    # Write the header file
    write_header_file(loaded_contracts_dict)
