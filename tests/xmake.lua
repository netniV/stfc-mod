target("keyboard-layout-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("keyboard_layout_tests.cc")
    add_includedirs("../mods/src")
    if is_plat("windows") then
        add_syslinks("user32")
    end
end

target("shortcut-layout-dispatch-tests")
do
    set_kind("binary")
    set_default(false)
    add_deps("mods")
    add_files("shortcut_hint_cache.cc")
    add_packages("libil2cpp", "eastl", "toml++", "spdlog")
end

target("keyboard-chord-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("keyboard_chord_tests.cc")
    add_includedirs("../mods/src")
    if is_plat("windows") then
        add_syslinks("user32")
    end
end
