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
