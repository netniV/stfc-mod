target("mods")
do
    add_ldflags("-v")
    set_kind("static")

    -- C++ sources
    add_files("src/**.cc")
    add_headerfiles("src/**.h")
    add_includedirs("src", { public = true })

    -- Packages
    add_packages("spud", "nlohmann_json", "protobuf", "libil2cpp", "eastl", "toml++", "spdlog", "simdutf", "libcurl", "capstone", "cpr")
    add_rules("protobuf.cpp")
    add_files("src/prime/proto/*.proto")

    set_exceptions("cxx")
    add_defines("NOMINMAX")
    
    if is_mode("releasedbg") then
        add_defines("_MODDBG")  -- enable your debug flag
    end

    -- Platform-specific settings
    if is_plat("windows") then
        add_cxflags("/bigobj")
        add_linkdirs("src/il2cpp")
    elseif is_plat("macosx") then
        add_cxflags("-fms-extensions")
        -- Add Objective-C++ source
        add_files("src/*.mm")
        -- Link Cocoa framework
        add_frameworks("Cocoa")
    elseif is_plat("linux") then
        add_packages("x11")
    end

    set_policy("build.optimization.lto", true)
end
