set_project("stfc-community-mod")

option("bg_image")
    set_showmenu(true)
    set_description("Path to a PNG to embed as the loading screen background (regenerates embedded_loading_image.h)")
    set_default("")
option_end()

set_languages("c++23")

set_runtimes("MT") -- Set the default build to multi-threaded static

add_requires("eastl")
add_requires("spdlog")
add_requires("toml++")
add_requires("nlohmann_json")
-- Sync sends HTTPS requests, so do not rely on cpr's package default of ssl=false.
add_requires("cpr", { configs = { ssl = true } })
if is_plat("macosx") then
    -- curl 8.15 removed Secure Transport. XMake's libcurl recipe still asks
    -- Apple builds for it, so force a supported TLS backend for packaged curl.
    add_requireconfs("cpr.libcurl", {
        override = true,
        system = false,
        configs = { zlib = true, libssh2 = true, openssl3 = true },
    })
    add_requireconfs("cpr.libcurl.libssh2", { configs = { backend = "openssl3" } })
    add_requireconfs("cpr.libssh2", { configs = { backend = "openssl3" } })
else
    add_requireconfs("cpr.libcurl", {
        override = true,
        configs = { zlib = true, libssh2 = false },
    })
end
add_requires("protobuf 32.1")

if is_plat("windows") then
    includes("win-proxy-dll")
    add_links('rpcrt4')
    add_links('runtimeobject')
end

if is_plat("macosx") then
    add_requires("inifile-cpp")
    add_requires("librsync")
    add_requires("PLzmaSDK")
    includes("macos-dylib")
    includes("macos-loader")
    includes("macos-launcher")
end

add_rules("mode.debug")
add_rules("mode.release")
add_rules("mode.releasedbg")

package("libil2cpp")
on_fetch(function(package, opt)
    return { includedirs = path.join(os.scriptdir(), "third_party/libil2cpp") }
end)
package_end()

add_requires("spud v0.2.0-2")
add_requires("libil2cpp")
add_requires("simdutf", { system = false })

-- includes("launcher")
includes("mods")

-- add_repositories("local-repo build")
add_repositories("stfc-community-mod-repo xmake-packages")
