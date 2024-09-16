// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;
 
contract MyEscrow {
    
    struct EscrowDeal {
        address buyer;
        address seller;
        address arbiter;
        uint256 amount;
        bool isFunded;
        bool isReleased;
    }

    // Mapping to keep track of all escrow deals
    mapping(uint256 => EscrowDeal) public escrowDeals;
    uint256 public dealCounter;

    event Deposited(uint256 indexed dealId, address indexed buyer, uint256 amount);
    event Released(uint256 indexed dealId, address indexed seller, uint256 amount);
    event Refunded(uint256 indexed dealId, address indexed buyer, uint256 amount);

    // Modifier to restrict function access to the arbiter
    modifier onlyArbiter(uint256 dealId) {
        require(msg.sender == escrowDeals[dealId].arbiter, "Only arbiter can call this function.");
        _;
    }

    // Function to create a new escrow deal and deposit funds
    function deposit(address _seller, address _arbiter) external payable returns(uint256) {
        require(msg.value > 0, "Deposit amount must be greater than zero.");

        // Increment the deal counter for a unique deal ID
        dealCounter++;

        // Create the escrow deal
        escrowDeals[dealCounter] = EscrowDeal({
            buyer: msg.sender,
            seller: _seller,
            arbiter: _arbiter,
            amount: msg.value,
            isFunded: true,
            isReleased: false
        });

        emit Deposited(dealCounter, msg.sender, msg.value);
        return dealCounter; // Returns the deal ID
    }

    // Function to release funds to the seller
    function release(uint256 dealId) external onlyArbiter(dealId) {
        EscrowDeal storage deal = escrowDeals[dealId];
        require(deal.isFunded, "Deal is not funded.");
        require(!deal.isReleased, "Deal has already been released.");

        // Mark the deal as released and transfer funds to the seller
        deal.isReleased = true;
        payable(deal.seller).transfer(deal.amount);

        emit Released(dealId, deal.seller, deal.amount);
    }

    // Function to refund the buyer
    function refund(uint256 dealId) external onlyArbiter(dealId) {
        EscrowDeal storage deal = escrowDeals[dealId];
        require(deal.isFunded, "Deal is not funded.");
        require(!deal.isReleased, "Deal has already been released.");

        // Mark the deal as no longer funded and refund the buyer
        deal.isFunded = false;
        payable(deal.buyer).transfer(deal.amount);

        emit Refunded(dealId, deal.buyer, deal.amount);
    }
}