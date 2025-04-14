function gen_bytecode(func)
    c = string.dump(func, true)
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
