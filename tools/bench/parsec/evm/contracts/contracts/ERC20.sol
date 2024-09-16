// contracts/GLDToken.sol
// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import "@openzeppelin/contracts/token/ERC20/ERC20.sol";

contract Token is ERC20 {
    constructor() ERC20("Tokens", "TOK") {
        _mint(msg.sender, 1000000000000000000000000); // 1M Coins with 18 decimals
    }
}
