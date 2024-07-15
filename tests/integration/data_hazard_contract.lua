function gen_bytecode()
    pay_contract = function(param)
        -- take out locks on keys
        hk1 = "ticketed_key_1"
        hk2 = "ticketed_key_2"
        hk3 = "ticketed_key_3"
        uhk = "unticketed_key" -- Intentionally don't yield this key
        coroutine.yield(hk1)
        coroutine.yield(hk2)
        coroutine.yield(hk3)

        updates = {}
        updates[hk1] = string.pack("c4", "100")
        updates[hk2] = string.pack("c4", "200")
        updates[hk3] = string.pack("c4", "250")
        updates[uhk] = string.pack("c4", "255")

        return updates
    end
    c = string.dump(pay_contract, true)
    t = {}
    for i = 1, #c do
        t[#t + 1] = string.format("%02x", string.byte(c, i))
    end

    return table.concat(t)
end
