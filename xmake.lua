set_project("stfc-community-mod")

includes("xmake/options.lua")
includes("xmake/dependencies/common.lua")

set_languages("c++23")
set_runtimes("MT")

includes("xmake/rules/stfc_identity.lua")

if is_plat("windows") then
    includes("xmake/dependencies/windows.lua")
    includes("win-proxy-dll")
    add_links("rpcrt4", "runtimeobject")
elseif is_plat("macosx") then
    includes("xmake/dependencies/macos.lua")
    includes("macos-dylib")
    includes("macos-loader")
    includes("macos-launcher")
end

add_rules("mode.debug")
add_rules("mode.release")
add_rules("mode.releasedbg")

includes("xmake/rules/protobuf_sccache.lua")
includes("xmake/rules/cxx_sccache.lua")
includes("mods")

target("il2cpp-runtime-tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/il2cpp_runtime.cc")
    add_includedirs("mods/src")
    add_packages("libil2cpp", "eastl")
    set_exceptions("cxx")
    if is_plat("windows") then
        add_linkdirs("mods/src/il2cpp")
    end
