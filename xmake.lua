set_project("stfc-community-mod")

includes("xmake/options.lua")
includes("xmake/dependencies/common.lua")

set_languages("c++23")
set_runtimes("MT")

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
includes("mods")
