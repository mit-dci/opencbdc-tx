// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

import "@openzeppelin/contracts/token/ERC20/ERC20.sol";

contract WETHToken is ERC20 {
    constructor(uint256 initialSupply) ERC20("Fake Wrapped ETH", "WETH") {
        _mint(msg.sender, initialSupply);
    }
}

contract WFOOToken is ERC20 {
    constructor(uint256 initialSupply) ERC20("Foo Token", "WFOO") {
        _mint(msg.sender, initialSupply);
    }
}

contract WBARToken is ERC20 {
    constructor(uint256 initialSupply) ERC20("Bar Token", "WBAR") {
        _mint(msg.sender, initialSupply);
    }
}
