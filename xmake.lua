set_project("stfc-community-mod")

option("bg_image")
    set_showmenu(true)
    set_description("Path to a PNG to embed as the loading screen background (regenerates embedded_loading_image.h)")
    set_default("")
option_end()

option("use_original_bg")
    set_showmenu(true)
    set_description("Keep the original in-game loading screen background (no custom BG replacement, logos still shown)")
    set_default(false)
option_end()

set_languages("c++23")

set_runtimes("MT") -- Set the default build to multi-threaded static

add_requires("eastl")
add_requires("spdlog")
add_requires("toml++")
add_requires("nlohmann_json")
add_requires("protobuf 35.1")

add_requires("cpr")

if is_plat("windows") then
    add_requireconfs("cpr.libcurl", {
        configs = { shared = false, openssl3 = false, zlib = true }
    })
elseif is_plat("macosx") then
    add_requireconfs("cpr.libcurl", {
        configs = {
            shared = false,
            openssl3 = true,
            zlib = true,
            cxflags = "-DUSE_APPLE_SECTRUST=1",
            ldflags = "-framework Security -framework CoreFoundation -framework CoreServices"
        }
    })
    add_requireconfs("cpr.libcurl.openssl3", {
        configs = { shared = false }
    })
end

add_requireconfs("cpr.libcurl.zlib", {
    configs = { shared = false }
})

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

option("stfc_source_state_id")
    set_default(os.getenv("STFC_SOURCE_STATE_ID") or "unknown")
    set_showmenu(true)
    set_description("Clean commit or dirty source fingerprint embedded in Windows DLL provenance")
option_end()

option("stfc_base_commit")
    set_default(os.getenv("STFC_BASE_COMMIT") or "unknown")
    set_showmenu(true)
    set_description("Base source commit embedded in Windows DLL provenance")
option_end()

option("stfc_build_invocation_id")
    set_default(os.getenv("STFC_BUILD_INVOCATION_ID") or "xmake-direct")
    set_showmenu(true)
    set_description("Build correlation identifier embedded in Windows DLL provenance")
option_end()

option("stfc_build_channel")
    set_default(os.getenv("STFC_BUILD_CHANNEL") or "local")
    set_showmenu(true)
    set_description("Build channel embedded in Windows DLL provenance")
option_end()

if is_plat("windows") then
    rule("stfc.identity")
        on_load(function(target)
            local function identity_define(option_name, fallback)
                local value = get_config(option_name)
                if value == nil then
                    value = fallback
                end
                if type(value) ~= "string" or value == "" then
                    raise(option_name .. " must not be empty")
                end
                if #value > 160 then
                    raise(option_name .. " must not exceed 160 characters")
                end
                if not value:match("^[A-Za-z0-9%._:+/%-]+$") then
                    raise(option_name .. " contains unsupported identity characters")
                end
                return value
            end

            target:add("defines", "STFC_SOURCE_STATE_ID=\"" ..
                       identity_define("stfc_source_state_id", "unknown") .. "\"")
            target:add("defines", "STFC_BASE_COMMIT=\"" .. identity_define("stfc_base_commit", "unknown") .. "\"")
            target:add("defines", "STFC_BUILD_INVOCATION_ID=\"" ..
                       identity_define("stfc_build_invocation_id", "xmake-direct") .. "\"")
            target:add("defines", "STFC_BUILD_MODE=\"" .. identity_define("mode", "unknown") .. "\"")
            target:add("defines", "STFC_BUILD_CHANNEL=\"" .. identity_define("stfc_build_channel", "local") .. "\"")
        end)
    rule_end()
    add_rules("stfc.identity")
end

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
