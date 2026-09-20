target("ship-name-matching-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("ship_name_matching_tests.cc")
    add_includedirs("../mods/src")
    add_packages("libil2cpp", "eastl", "spdlog", "simdutf")
    if is_plat("windows") then
        add_linkdirs("../mods/src/il2cpp")
    end
end
