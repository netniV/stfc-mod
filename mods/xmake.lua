target("mods")
do
    add_ldflags("-v")
    set_kind("static")
    add_files("src/**.cc")
    add_headerfiles("src/**.h")
    add_includedirs("src", { public = true })
    add_packages("spud", "nlohmann_json", "protobuf", "libil2cpp", "eastl", "toml++", "spdlog", "simdutf", "libcurl", "capstone")
    add_rules("protobuf.cpp")
    add_files("src/prime/proto/*.proto")
    set_exceptions("cxx")
    add_defines("NOMINMAX")
    if is_plat("windows") then
        add_cxflags("/bigobj")
        add_linkdirs("src/il2cpp")
    end
    if is_plat("macosx") then
        add_cxflags("-fms-extensions")
        add_files("src/*.mm")
    end
    set_policy("build.optimization.lto", true)
end
