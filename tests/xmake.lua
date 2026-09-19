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
