// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "contracts.hpp"

namespace cbdc::threepc::evm_contracts {
    auto data_erc20_deploy() -> cbdc::buffer {
        auto buf = cbdc::buffer::from_hex(
            "60806040523480156200001157600080fd5b50604051806040016040528060068"
            "1526020017f546f6b656e73000000000000000000000000000000000000000000"
            "00000000008152506040518060400160405280600381526020017f544f4b00000"
            "00000000000000000000000000000000000000000000000000000815250816003"
            "90805190602001906200009692919062000257565b50806004908051906020019"
            "0620000af92919062000257565b505050620000ce3369d3c21bcecceda1000000"
            "620000d460201b60201c565b620004a5565b600073fffffffffffffffffffffff"
            "fffffffffffffffff168273ffffffffffffffffffffffffffffffffffffffff16"
            "141562000147576040517f08c379a000000000000000000000000000000000000"
            "00000000000000000000081526004016200013e906200035a565b604051809103"
            "90fd5b6200015b600083836200024d60201b60201c565b8060026000828254620"
            "0016f9190620003aa565b92505081905550806000808473ffffffffffffffffff"
            "ffffffffffffffffffffff1673fffffffffffffffffffffffffffffffffffffff"
            "f1681526020019081526020016000206000828254620001c69190620003aa565b"
            "925050819055508173ffffffffffffffffffffffffffffffffffffffff1660007"
            "3ffffffffffffffffffffffffffffffffffffffff167fddf252ad1be2c89b69c2"
            "b068fc378daa952ba7f163c4a11628f55a4df523b3ef836040516200022d91906"
            "200037c565b60405180910390a362000249600083836200025260201b60201c56"
            "5b5050565b505050565b505050565b828054620002659062000411565b9060005"
            "2602060002090601f016020900481019282620002895760008555620002d5565b"
            "82601f10620002a457805160ff1916838001178555620002d5565b82800160010"
            "185558215620002d5579182015b82811115620002d45782518255916020019190"
            "60010190620002b7565b5b509050620002e49190620002e8565b5090565b5b808"
            "2111562000303576000816000905550600101620002e9565b5090565b60006200"
            "0316601f8362000399565b91507f45524332303a206d696e7420746f207468652"
            "07a65726f2061646472657373006000830152602082019050919050565b620003"
            "548162000407565b82525050565b6000602082019050818103600083015262000"
            "3758162000307565b9050919050565b6000602082019050620003936000830184"
            "62000349565b92915050565b600082825260208201905092915050565b6000620"
            "003b78262000407565b9150620003c48362000407565b9250827fffffffffffff"
            "ffffffffffffffffffffffffffffffffffffffffffffffffffff0382111562000"
            "3fc57620003fb62000447565b5b828201905092915050565b6000819050919050"
            "565b600060028204905060018216806200042a57607f821691505b60208210811"
            "41562000441576200044062000476565b5b50919050565b7f4e487b7100000000"
            "00000000000000000000000000000000000000000000000060005260116004526"
            "0246000fd5b7f4e487b7100000000000000000000000000000000000000000000"
            "000000000000600052602260045260246000fd5b6111ff80620004b5600039600"
            "0f3fe608060405234801561001057600080fd5b50600436106100a95760003560"
            "e01c80633950935111610071578063395093511461016857806370a0823114610"
            "19857806395d89b41146101c8578063a457c2d7146101e6578063a9059cbb1461"
            "0216578063dd62ed3e14610246576100a9565b806306fdde03146100ae5780630"
            "95ea7b3146100cc57806318160ddd146100fc57806323b872dd1461011a578063"
            "313ce5671461014a575b600080fd5b6100b6610276565b6040516100c39190610"
            "ec8565b60405180910390f35b6100e660048036038101906100e19190610b6756"
            "5b610308565b6040516100f39190610ead565b60405180910390f35b610104610"
            "32b565b6040516101119190610fca565b60405180910390f35b61013460048036"
            "0381019061012f9190610b18565b610335565b6040516101419190610ead565b6"
            "0405180910390f35b610152610364565b60405161015f9190610fe5565b604051"
            "80910390f35b610182600480360381019061017d9190610b67565b61036d565b6"
            "0405161018f9190610ead565b60405180910390f35b6101b26004803603810190"
            "6101ad9190610ab3565b6103a4565b6040516101bf9190610fca565b604051809"
            "10390f35b6101d06103ec565b6040516101dd9190610ec8565b60405180910390"
            "f35b61020060048036038101906101fb9190610b67565b61047e565b604051610"
            "20d9190610ead565b60405180910390f35b610230600480360381019061022b91"
            "90610b67565b6104f5565b60405161023d9190610ead565b60405180910390f35"
            "b610260600480360381019061025b9190610adc565b610518565b60405161026d"
            "9190610fca565b60405180910390f35b606060038054610285906110fa565b806"
            "01f01602080910402602001604051908101604052809291908181526020018280"
            "546102b1906110fa565b80156102fe5780601f106102d35761010080835404028"
            "35291602001916102fe565b820191906000526020600020905b81548152906001"
            "01906020018083116102e157829003601f168201915b5050505050905090565b6"
            "0008061031361059f565b90506103208185856105a7565b600191505092915050"
            "565b6000600254905090565b60008061034061059f565b905061034d858285610"
            "772565b6103588585856107fe565b60019150509392505050565b600060129050"
            "90565b60008061037861059f565b905061039981858561038a8589610518565b6"
            "10394919061101c565b6105a7565b600191505092915050565b60008060008373"
            "ffffffffffffffffffffffffffffffffffffffff1673fffffffffffffffffffff"
            "fffffffffffffffffff168152602001908152602001600020549050919050565b"
            "6060600480546103fb906110fa565b80601f01602080910402602001604051908"
            "10160405280929190818152602001828054610427906110fa565b801561047457"
            "80601f1061044957610100808354040283529160200191610474565b820191906"
            "000526020600020905b8154815290600101906020018083116104575782900360"
            "1f168201915b5050505050905090565b60008061048961059f565b90506000610"
            "4978286610518565b9050838110156104dc576040517f08c379a0000000000000"
            "0000000000000000000000000000000000000000000081526004016104d390610"
            "faa565b60405180910390fd5b6104e982868684036105a7565b60019250505092"
            "915050565b60008061050061059f565b905061050d8185856107fe565b6001915"
            "05092915050565b6000600160008473ffffffffffffffffffffffffffffffffff"
            "ffffff1673ffffffffffffffffffffffffffffffffffffffff168152602001908"
            "15260200160002060008373ffffffffffffffffffffffffffffffffffffffff16"
            "73ffffffffffffffffffffffffffffffffffffffff16815260200190815260200"
            "160002054905092915050565b600033905090565b600073ffffffffffffffffff"
            "ffffffffffffffffffffff168373fffffffffffffffffffffffffffffffffffff"
            "fff161415610617576040517f08c379a000000000000000000000000000000000"
            "000000000000000000000000815260040161060e90610f8a565b6040518091039"
            "0fd5b600073ffffffffffffffffffffffffffffffffffffffff168273ffffffff"
            "ffffffffffffffffffffffffffffffff161415610687576040517f08c379a0000"
            "00000000000000000000000000000000000000000000000000000815260040161"
            "067e90610f0a565b60405180910390fd5b80600160008573fffffffffffffffff"
            "fffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffff"
            "ff16815260200190815260200160002060008473fffffffffffffffffffffffff"
            "fffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168152"
            "602001908152602001600020819055508173fffffffffffffffffffffffffffff"
            "fffffffffff168373ffffffffffffffffffffffffffffffffffffffff167f8c5b"
            "e1e5ebec7d5bd14f71427d1e84f3dd0314c0f7b2291e5b200ac8c7c3b92583604"
            "0516107659190610fca565b60405180910390a3505050565b600061077e848461"
            "0518565b90507ffffffffffffffffffffffffffffffffffffffffffffffffffff"
            "fffffffffffff81146107f857818110156107ea576040517f08c379a000000000"
            "00000000000000000000000000000000000000000000000081526004016107e19"
            "0610f2a565b60405180910390fd5b6107f784848484036105a7565b5b50505050"
            "565b600073ffffffffffffffffffffffffffffffffffffffff168373fffffffff"
            "fffffffffffffffffffffffffffffff16141561086e576040517f08c379a00000"
            "00000000000000000000000000000000000000000000000000008152600401610"
            "86590610f6a565b60405180910390fd5b600073ffffffffffffffffffffffffff"
            "ffffffffffffff168273ffffffffffffffffffffffffffffffffffffffff16141"
            "56108de576040517f08c379a00000000000000000000000000000000000000000"
            "000000000000000081526004016108d590610eea565b60405180910390fd5b610"
            "8e9838383610a7f565b60008060008573ffffffffffffffffffffffffffffffff"
            "ffffffff1673ffffffffffffffffffffffffffffffffffffffff1681526020019"
            "081526020016000205490508181101561096f576040517f08c379a00000000000"
            "00000000000000000000000000000000000000000000008152600401610966906"
            "10f4a565b60405180910390fd5b8181036000808673ffffffffffffffffffffff"
            "ffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168"
            "15260200190815260200160002081905550816000808573ffffffffffffffffff"
            "ffffffffffffffffffffff1673fffffffffffffffffffffffffffffffffffffff"
            "f1681526020019081526020016000206000828254610a02919061101c565b9250"
            "50819055508273ffffffffffffffffffffffffffffffffffffffff168473fffff"
            "fffffffffffffffffffffffffffffffffff167fddf252ad1be2c89b69c2b068fc"
            "378daa952ba7f163c4a11628f55a4df523b3ef84604051610a669190610fca565"
            "b60405180910390a3610a79848484610a84565b50505050565b505050565b5050"
            "50565b600081359050610a988161119b565b92915050565b600081359050610aa"
            "d816111b2565b92915050565b600060208284031215610ac557600080fd5b6000"
            "610ad384828501610a89565b91505092915050565b60008060408385031215610"
            "aef57600080fd5b6000610afd85828601610a89565b9250506020610b0e858286"
            "01610a89565b9150509250929050565b600080600060608486031215610b2d576"
            "00080fd5b6000610b3b86828701610a89565b9350506020610b4c86828701610a"
            "89565b9250506040610b5d86828701610a9e565b9150509250925092565b60008"
            "060408385031215610b7a57600080fd5b6000610b8885828601610a89565b9250"
            "506020610b9985828601610a9e565b9150509250929050565b610bac816110845"
            "65b82525050565b6000610bbd82611000565b610bc7818561100b565b9350610b"
            "d78185602086016110c7565b610be08161118a565b840191505092915050565b6"
            "000610bf860238361100b565b91507f45524332303a207472616e736665722074"
            "6f20746865207a65726f206164647260008301527f65737300000000000000000"
            "00000000000000000000000000000000000000000602083015260408201905091"
            "9050565b6000610c5e60228361100b565b91507f45524332303a20617070726f7"
            "66520746f20746865207a65726f20616464726560008301527f73730000000000"
            "00000000000000000000000000000000000000000000000000602083015260408"
            "2019050919050565b6000610cc4601d8361100b565b91507f45524332303a2069"
            "6e73756666696369656e7420616c6c6f77616e636500000060008301526020820"
            "19050919050565b6000610d0460268361100b565b91507f45524332303a207472"
            "616e7366657220616d6f756e742065786365656473206260008301527f616c616"
            "e6365000000000000000000000000000000000000000000000000000060208301"
            "52604082019050919050565b6000610d6a60258361100b565b91507f455243323"
            "03a207472616e736665722066726f6d20746865207a65726f2061646000830152"
            "7f647265737300000000000000000000000000000000000000000000000000000"
            "06020830152604082019050919050565b6000610dd060248361100b565b91507f"
            "45524332303a20617070726f76652066726f6d20746865207a65726f206164646"
            "0008301527f726573730000000000000000000000000000000000000000000000"
            "00000000006020830152604082019050919050565b6000610e3660258361100b5"
            "65b91507f45524332303a2064656372656173656420616c6c6f77616e63652062"
            "656c6f7760008301527f207a65726f00000000000000000000000000000000000"
            "00000000000000000006020830152604082019050919050565b610e98816110b0"
            "565b82525050565b610ea7816110ba565b82525050565b6000602082019050610"
            "ec26000830184610ba3565b92915050565b600060208201905081810360008301"
            "52610ee28184610bb2565b905092915050565b600060208201905081810360008"
            "30152610f0381610beb565b9050919050565b6000602082019050818103600083"
            "0152610f2381610c51565b9050919050565b60006020820190508181036000830"
            "152610f4381610cb7565b9050919050565b600060208201905081810360008301"
            "52610f6381610cf7565b9050919050565b6000602082019050818103600083015"
            "2610f8381610d5d565b9050919050565b60006020820190508181036000830152"
            "610fa381610dc3565b9050919050565b600060208201905081810360008301526"
            "10fc381610e29565b9050919050565b6000602082019050610fdf600083018461"
            "0e8f565b92915050565b6000602082019050610ffa6000830184610e9e565b929"
            "15050565b600081519050919050565b600082825260208201905092915050565b"
            "6000611027826110b0565b9150611032836110b0565b9250827ffffffffffffff"
            "fffffffffffffffffffffffffffffffffffffffffffffffffff03821115611067"
            "5761106661112c565b5b828201905092915050565b600061107d82611090565b9"
            "050919050565b60008115159050919050565b600073ffffffffffffffffffffff"
            "ffffffffffffffffff82169050919050565b6000819050919050565b600060ff8"
            "2169050919050565b60005b838110156110e55780820151818401526020810190"
            "506110ca565b838111156110f4576000848401525b50505050565b60006002820"
            "49050600182168061111257607f821691505b6020821081141561112657611125"
            "61115b565b5b50919050565b7f4e487b710000000000000000000000000000000"
            "0000000000000000000000000600052601160045260246000fd5b7f4e487b7100"
            "00000000000000000000000000000000000000000000000000000060005260226"
            "0045260246000fd5b6000601f19601f8301169050919050565b6111a481611072"
            "565b81146111af57600080fd5b50565b6111bb816110b0565b81146111c657600"
            "080fd5b5056fea26469706673582212201370002068d6a4ed9844619e4a8d1364"
            "df779469b320e4e50e3c4a6feaca165764736f6c63430008000033");
        return buf.value();
    }

    auto data_erc20_allowance(evmc::address owner, evmc::address spender)
        -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_allowance
            = std::string("allowance(address,address)");
        auto selector_hash = cbdc::keccak_data(selector_allowance.data(),
                                               selector_allowance.size());
        buf.append(selector_hash.data(), selector_size);
        buf.extend(address_param_offset);
        buf.append(owner.bytes, sizeof(owner.bytes));
        buf.extend(address_param_offset);
        buf.append(spender.bytes, sizeof(spender.bytes));
        return buf;
    }

    auto data_erc20_approve(evmc::address spender, evmc::uint256be amount)
        -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_approve = std::string("approve(address,uint256)");
        auto selector_hash = cbdc::keccak_data(selector_approve.data(),
                                               selector_approve.size());
        buf.append(selector_hash.data(), selector_size);
        buf.extend(address_param_offset);
        buf.append(spender.bytes, sizeof(spender.bytes));
        buf.append(amount.bytes, sizeof(amount.bytes));
        return buf;
    }

    auto data_erc20_balance_of(evmc::address account) -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_balance_of = std::string("balanceOf(address)");
        auto selector_hash = cbdc::keccak_data(selector_balance_of.data(),
                                               selector_balance_of.size());
        buf.append(selector_hash.data(), selector_size);
        buf.extend(address_param_offset);
        buf.append(account.bytes, sizeof(account.bytes));
        return buf;
    }

    auto data_erc20_decimals() -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_decimals = std::string("decimals()");
        auto selector_hash = cbdc::keccak_data(selector_decimals.data(),
                                               selector_decimals.size());
        buf.append(selector_hash.data(), selector_size);
        return buf;
    }

    auto data_erc20_decrease_allowance(evmc::address spender,
                                       evmc::uint256be subtracted_value)
        -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_decrease_allowance
            = std::string("decreaseAllowance(address,uint256)");
        auto selector_hash
            = cbdc::keccak_data(selector_decrease_allowance.data(),
                                selector_decrease_allowance.size());
        buf.append(selector_hash.data(), selector_size);
        buf.extend(address_param_offset);
        buf.append(spender.bytes, sizeof(spender.bytes));
        buf.append(subtracted_value.bytes, sizeof(subtracted_value.bytes));
        return buf;
    }

    auto data_erc20_increase_allowance(evmc::address spender,
                                       evmc::uint256be added_value)
        -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_increase_allowance
            = std::string("increaseAllowance(address,uint256)");
        auto selector_hash
            = cbdc::keccak_data(selector_increase_allowance.data(),
                                selector_increase_allowance.size());
        buf.append(selector_hash.data(), selector_size);
        buf.extend(address_param_offset);
        buf.append(spender.bytes, sizeof(spender.bytes));
        buf.append(added_value.bytes, sizeof(added_value.bytes));
        return buf;
    }

    auto data_erc20_name() -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_name = std::string("name()");
        auto selector_hash
            = cbdc::keccak_data(selector_name.data(), selector_name.size());
        buf.append(selector_hash.data(), selector_size);
        return buf;
    }

    auto data_erc20_symbol() -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_symbol = std::string("symbol()");
        auto selector_hash = cbdc::keccak_data(selector_symbol.data(),
                                               selector_symbol.size());
        buf.append(selector_hash.data(), selector_size);
        return buf;
    }

    auto data_erc20_total_supply() -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_total_supply = std::string("totalSupply()");
        auto selector_hash = cbdc::keccak_data(selector_total_supply.data(),
                                               selector_total_supply.size());
        buf.append(selector_hash.data(), selector_size);
        return buf;
    }

    auto data_erc20_transfer(evmc::address to, evmc::uint256be amount)
        -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_transfer
            = std::string("transfer(address,uint256)");
        auto selector_hash = cbdc::keccak_data(selector_transfer.data(),
                                               selector_transfer.size());
        buf.append(selector_hash.data(), selector_size);
        buf.extend(address_param_offset);
        buf.append(to.bytes, sizeof(to.bytes));
        buf.append(amount.bytes, sizeof(amount.bytes));
        return buf;
    }

    auto data_erc20_transfer_from(evmc::address from,
                                  evmc::address to,
                                  evmc::uint256be amount) -> cbdc::buffer {
        auto buf = cbdc::buffer();
        const auto selector_transfer_from
            = std::string("transferFrom(address,address,uint256)");
        auto selector_hash = cbdc::keccak_data(selector_transfer_from.data(),
                                               selector_transfer_from.size());
        buf.append(selector_hash.data(), selector_size);
        buf.extend(address_param_offset);
        buf.append(from.bytes, sizeof(from.bytes));
        buf.extend(address_param_offset);
        buf.append(to.bytes, sizeof(to.bytes));
        buf.append(amount.bytes, sizeof(amount.bytes));
        return buf;
    }
}
