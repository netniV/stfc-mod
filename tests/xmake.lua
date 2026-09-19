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

if is_plat("macosx") then
    target("macos-hook-extent-tests")
    do
        set_kind("binary")
        set_default(false)
        add_files("macos_hook_extent_tests.cc", "../mods/src/patches/native_hook_extent.cc")
        add_includedirs("../mods/src")
        add_packages("spud", "spdlog")
        set_policy("build.optimization.lto", false)
    end
end
