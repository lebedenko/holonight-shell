hl.monitor({ output = "", mode = "preferred", position = "auto", scale = "auto" })
hl.bind("SUPER + C", hl.dsp.window.close())
hl.bind("SUPER + E", hl.dsp.exec_cmd("uwsm app -- hn-files"))
for _, direction in ipairs({"left", "right", "up", "down"}) do
    hl.bind("SUPER + " .. direction, hl.dsp.focus({ direction = direction }))
end
for i = 1, 10 do
    local key = tostring(i % 10)
    hl.bind("SUPER + " .. key, hl.dsp.focus({ workspace = i }))
    hl.bind("SUPER + SHIFT + " .. key, hl.dsp.window.move({ workspace = i }))
end
