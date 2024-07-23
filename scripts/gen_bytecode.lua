function gen_bytecode()
    pay_contract = function(param)
        -- 0 is a read lock, 1 is a write lock in C++ scope
        Locks = {
            READ = 0,
            WRITE = 1,
        }
        from, to, value, sequence, sig = string.unpack("c32 c32 I8 I8 c64", param)

        function get_account_key(name)
            account_prefix = "account_"
            account_key = account_prefix .. name
            return account_key
        end

        function get_account(name)
            account_key = get_account_key(name)

            account_data = coroutine.yield(account_key, Locks.WRITE)
            if string.len(account_data) > 0 then
                account_balance, account_sequence
                = string.unpack("I8 I8", account_data)
                return account_balance, account_sequence
            end
            return 0, 0
        end

        function pack_account(updates, name, balance, seq)
            updates[get_account_key(name)] = string.pack("I8 I8", balance, seq)
        end

        function update_accounts(from_acc, from_bal, from_seq, to_acc, to_bal, to_seq)
            ret = {}
            pack_account(ret, from_acc, from_bal, from_seq)
            if to_acc ~= nil then
                pack_account(ret, to_acc, to_bal, to_seq)
            end
            return ret
        end

        function sig_payload(to_acc, value, seq)
            return string.pack("c32 I8 I8", to_acc, value, seq)
        end

        from_balance, from_seq = get_account(from)
        payload = sig_payload(to, value, sequence)
        check_sig(from, sig, payload)
        if sequence < from_seq then
            error("sequence number too low")
        end

        if value > from_balance then
            error("insufficient balance")
        end

        if value > 0 then
            to_balance, to_seq = get_account(to)
            to_balance = to_balance + value
            from_balance = from_balance - value
        else
            to = nil
        end

        from_seq = sequence + 1
        return update_accounts(from, from_balance, from_seq, to, to_balance, to_seq)
    end
    c = string.dump(pay_contract, true)
    tot = ""
    for i = 1, string.len(c) do
        hex = string.format("%x", string.byte(c, i))
        if string.len(hex) < 2 then
            hex = "0" .. hex
        end
        tot = tot .. hex
    end

    return tot
end
