// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_CLIENT_CLIENT_H_
#define OPENCBDC_TX_SRC_CLIENT_CLIENT_H_

#include "uhs/sentinel/client.hpp"
#include "uhs/transaction/validation.hpp"
#include "uhs/transaction/wallet.hpp"

namespace cbdc {
    /// External client for sending new transactions to the system.
    class client {
      public:
        /// Constructor.
        /// \param opts configuration options.
        /// \param logger pointer shared logger.
        /// \param wallet_file name of .dat file in which to store the wallet
        ///                    data.
        /// \param client_file name of .dat file in which to store the
        ///                    internal state data.
        client(cbdc::config::options opts,
               std::shared_ptr<logging::log> logger,
               std::string wallet_file,
               std::string client_file);

        virtual ~client() = default;

        client(const client&) = delete;
        auto operator=(const client&) -> client& = delete;

        client(client&&) = delete;
        auto operator=(client&&) -> client& = delete;

        /// \brief Initializes the client.
        ///
        /// Attempts to load the data files, and creates new ones if they do
        /// not exist. Establishes connections to the system components.
        /// \return true if initialization succeeded.
        auto init() -> bool;

        /// \brief Format a value given in currency base units as USD.
        ///
        /// Assumes the atomic (base) unit of the currency is equivalent to
        /// $0.01 (one USD cent). Ex. 10000 <=> $100.00.
        /// \param val value in currency base units.
        /// \return USD formatted value.
        static auto print_amount(uint64_t val) -> std::string;

        /// \brief Mint coins for testing.
        ///
        /// Provides a pre-calculated keypair to be used for configuration
        /// files for testing and demo environments. Use the hexed public key:
        /// 1f05f6173c4f7bef58f7e912c4cb1389097a38f1a9e24c3674d67a0f142af244 as
        /// the value for minter0 in a configuration file.
        /// \param n_outputs number of new spendable outputs to create.
        /// \param output_val value of the amount to associate with each output in the base unit of the currency.
        /// \return the transaction and sentinel response.
        auto mint_for_testing(size_t n_outputs, uint32_t output_val)
            -> std::pair<std::optional<transaction::full_tx>,
                         std::optional<cbdc::sentinel::execute_response>>;

        /// \brief Creates the specified number spendable outputs each with the
        ///        specified value.
        ///
        /// Generates a transaction with the specified outputs and no inputs.
        /// Saves the transaction in the client's internal state. Submits the
        /// transaction to the system via \ref send_mint_tx.
        /// \param n_outputs number of new spendable outputs to create.
        /// \param output_val value of the amount to associate with each output in the base unit of the currency.
        /// \return the transaction and sentinel response.
        auto mint(size_t n_outputs, uint32_t output_val)
            -> std::pair<std::optional<transaction::full_tx>,
                         std::optional<cbdc::sentinel::execute_response>>;

        /// \brief Send a specified amount from this client's wallet to a
        ///        target address.
        ///
        /// Generates, transmits, and, if possible, confirms a full
        /// transaction. Returns the transaction and system response data.
        /// \param value the amount to send, in the base unit of the currency.
        /// \param payee the destination address of the transfer.
        /// \return the transaction sent to the network for verification and
        ///         the response from the sentinel client, or std::nullopt if
        ///         generating or transmitting the transaction failed.
        auto send(uint32_t value, const pubkey_t& payee)
            -> std::pair<std::optional<transaction::full_tx>,
                         std::optional<cbdc::sentinel::execute_response>>;

        /// \brief Send a specified number of fixed-value outputs from this
        ///        client's wallet to a target address.
        ///
        /// Generates, transmits, and, if possible, confirms a full
        /// transaction. Returns the transaction and system response data.
        /// \param count number of outputs to generate.
        /// \param value the amount to send per output, in the base unit of the
        ///              currency.
        /// \param payee the destination address of the transfer.
        /// \return the transaction sent to the network for verification and
        ///         the response from the sentinel client, or std::nullopt if
        ///         generating or transmitting the transaction failed.
        auto fan(uint32_t count, uint32_t value, const pubkey_t& payee)
            -> std::pair<std::optional<transaction::full_tx>,
                         std::optional<cbdc::sentinel::execute_response>>;

        /// \brief Extracts the transaction data that recipients need from
        ///        senders to confirm pending transfers.
        ///
        /// Returns the outputs of the send transaction intended for the target
        /// recipient, converted to inputs. Senders transmit this data to
        /// recipients, who then call \ref import_send_input to receive the
        /// transfer.
        /// \param send_tx the send transaction generated by \ref send.
        /// \param payee the destination address of the transfer.
        /// \return inputs to transmit to the send transaction recipient, or
        ///         empty vector if there is no transaction output with the
        ///         corresponding payee information.
        static auto export_send_inputs(const transaction::full_tx& send_tx,
                                       const pubkey_t& payee)
            -> std::vector<transaction::input>;

        /// \brief Imports transaction data from a sender.
        ///
        /// Expects data generated by a sender's \ref export_send_inputs.
        /// Stores the provided input as a credit awaiting confirmation from
        /// the transaction processing system using \ref sync or a call to \ref
        /// confirm_transaction.
        /// \param in a spendable input generated by a sender.
        void import_send_input(const transaction::input& in);

        /// \brief Checks the status of pending transactions and updates the
        ///        wallet's balance with the result.
        ///
        /// Implementations should combine their own system querying logic with
        /// the \ref confirm_transaction \ref abandon_transaction functions to
        /// update the client and wallet state.
        /// \return false if the request failed.
        virtual auto sync() -> bool = 0;

        /// Generates a new wallet address that other clients can use to send
        /// money to this client using \ref send.
        /// \return a wallet address.
        /// \see \ref transaction::wallet::generate_key
        auto new_address() -> pubkey_t;

        /// Returns the balance in this client's wallet.
        /// \return balance in base currency units.
        /// \see \ref transaction::wallet::balance
        auto balance() -> uint64_t;

        /// \brief Returns the number of UTXOs in this client's wallet.
        ///
        /// Does not include those locked in pending transactions.
        /// \return the number of available UTXOs.
        /// \see \ref transaction::wallet::count
        auto utxo_count() -> size_t;

        /// Returns the number of unconfirmed transactions.
        /// \return number of unconfirmed transactions.
        auto pending_tx_count() -> size_t;

        /// \brief Returns the number of pending received inputs.
        ///
        /// Returns the number of inputs received from senders for which
        /// this client is awaiting confirmation from the transaction
        /// processing system.
        /// \return number of unconfirmed inputs.
        /// \see \ref import_send_input
        auto pending_input_count() -> size_t;

        /// \brief Confirms the transaction with the given ID.
        ///
        /// Searches the client's pending transactions and pending input sets
        /// for the specified transaction ID and confirms it. Erases spent
        /// outputs and adds confirmed inputs to the wallet and makes them
        /// available to spend.
        /// \note Does not query the transaction system to verify confirmation.
        ///       Implementations should define \ref sync methods to do so.
        /// \param tx_id the ID of the transaction to confirm.
        /// \return true if the transaction was confirmed.
        auto confirm_transaction(const hash_t& tx_id) -> bool;

        /// \brief Create a new transaction.
        ///
        /// Creates a signed transaction that constitutes sending a specified
        /// amount from this client's wallet to a target address. Selects a set
        /// of UTXOs to spend as transaction inputs, and locks them so they
        /// will not  be used in further calls to this function.
        /// \param value the amount to send in the base unit of the currency.
        /// \param payee the destination address of the transfer.
        /// \return the transaction created.
        auto create_transaction(uint32_t value, const pubkey_t& payee)
            -> std::optional<transaction::full_tx>;

        /// \brief Send the given transaction to the sentinel.
        ///
        /// \note This function adds the transaction to the client's pending
        /// transaction set. It does not confirm the transaction. Call \ref
        /// sync to do so.
        /// \param tx the transaction to send.
        /// \return the sentinel's response to the transaction, if any.
        auto send_transaction(const transaction::full_tx& tx)
            -> std::optional<cbdc::sentinel::execute_response>;

        /// \brief Abandons a transaction currently awaiting confirmation.
        ///
        /// Releases the lock on the transactions input UTXOs so the client can
        /// attempt to use them again in a subsequent transaction.
        /// \warning Client implementations should only call this function when
        ///          certain the transaction processing system has rejected or
        ///          otherwise failed to process the specified transaction.
        /// \param tx_id ID of the transaction to abandon.
        /// \return true if the transaction was in the client's pending
        ///         transaction set and has been removed.
        auto abandon_transaction(const hash_t& tx_id) -> bool;

        /// \brief Checks the client's pending transaction set for the
        ///        specified transaction.
        ///
        /// Searches the client's pending transaction set and returns true if
        /// there is a transaction that contains the passed input as one of its
        /// inputs
        /// \param inp the input to look for.
        /// \return true if there the client has a pending transaction that
        ///         contains inp.
        auto check_pending(const transaction::input& inp) -> bool;

        /// Signs the given transaction for as far as client's wallet contains
        /// the transaction's keys.
        /// \param tx the transaction to sign.
        void sign_transaction(transaction::full_tx& tx);

        /// \brief Client address type signifier.
        /// Prefixes client address data, indicating which addressing regime
        /// clients should use to transact with the address.
        enum class address_type : uint8_t {
            /// Pay-to-Public-Key (P2PK) address data.
            public_key = 0
        };

      protected:
        /// \brief Initializes the derived class.
        ///
        /// Called at the end of \ref init. Subclasses should define custom
        /// initialization logic here.
        /// \return true if the initialization succeeded.
        virtual auto init_derived() -> bool = 0;

        /// \brief Sends the given minting transaction to a service that will
        ///        accept and process it.
        ///
        /// TODO:  Remove. No longer needed
        ///
        /// Called by \ref mint to send the resulting transaction. Subclasses
        /// should define custom transmission logic here.
        /// \param mint_tx invalid transaction that mints new coins.
        /// \return true if the transaction was sent successfully.
        virtual auto send_mint_tx(const transaction::full_tx& mint_tx) -> bool
            = 0;

        /// \brief Returns the set of transactions pending confirmation.
        ///
        /// Returns the set of pending transactions sent to the transaction
        /// processing system via the \ref send or \ref mint methods which
        /// the system has yet to confirm.
        /// \return a map of transaction ID to pending transaction.
        [[nodiscard]] auto pending_txs() const
            -> std::unordered_map<hash_t, transaction::full_tx, hashing::null>;

        /// \brief Returns the set of imported inputs from senders.
        ///
        /// Returns the current set of pending inputs imported into this client
        /// via the \ref import_send_input method which the system has yet to
        /// confirm.
        /// \return a map of transaction ID to pending input.
        [[nodiscard]] auto pending_inputs() const
            -> std::unordered_map<hash_t, transaction::input, hashing::null>;

      private:
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        cbdc::sentinel::rpc::client m_sentinel_client;

        /// List of pending transactions submitted to the system awaiting
        /// confirmation, keyed by Tx ID.
        std::unordered_map<hash_t, transaction::full_tx, hashing::null>
            m_pending_txs;

        /// List of inputs taken from wallet but not confirmed spent or
        /// abandoned yet. Used to add inputs back to the wallet if the
        /// transaction is abandoned.
        std::unordered_map<hash_t, transaction::input, hashing::null>
            m_pending_spend;

        /// List of pending inputs added by client::import_send_input, indexed
        /// by the Tx ID of the transaction that created them.
        std::unordered_map<hash_t, transaction::input, hashing::null>
            m_pending_inputs;

        transaction::wallet m_wallet{};

        std::string m_client_file;
        std::string m_wallet_file;

        /// Add the provided transaction to the memory pool, awaiting
        /// confirmation from the network.
        /// \param tx transaction to add.
        void import_transaction(const transaction::full_tx& tx);

        void load_client_state();
        void save_client_state();

        void save();

        void register_pending_tx(const transaction::full_tx& tx);
    };
}

#endif // OPENCBDC_TX_SRC_CLIENT_CLIENT_H_
