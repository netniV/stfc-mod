add_rules("mode.debug", "mode.release")

-- Function to extract version from version.h
function get_version_from_header()
    local version_file = path.join(os.scriptdir(), "../mods/src/version.h")
    if not os.isfile(version_file) then
        return "1.0.0.0"
    end
    
    local content = io.readfile(version_file)
    local major = content:match("#define VERSION_MAJOR%s+(%d+)")
    local minor = content:match("#define VERSION_MINOR%s+(%d+)")
    local revision = content:match("#define VERSION_REVISION%s+(%d+)")
    local patch = content:match("#define VERSION_PATCH%s+(%d+)")
    
    if major and minor and revision and patch then
        return major .. "." .. minor .. "." .. revision .. "." .. patch
    end
    return "1.0.0.0"
end

target("macOSLauncher")
do
    add_rules("xcode.application")
    add_files("src/**/*.swift")
    add_files("src/*.swift", "src/*.xcassets")
    add_files("src/*.cc")
    add_packages("7z", "lzma", "librsync")
    add_ldflags("-lc++")
    add_values("xcode.bundle_identifier", "com.tashcan.startrekpatch")
    add_scflags("-Xcc -fmodules", "-Xcc -fmodule-map-file=macos-launcher/src/module.modulemap", "-D SWIFT_PACKAGE",
        { force = true })
    add_values("xcode.bundle_display_name", "Star Trek Fleet Command Community Patch")
    
    add_files("deps/PlzmaSDK/swift/*.swift")
    add_files("deps/PlzmaSDK/src/*.cpp")
    add_files("deps/PlzmaSDK/src/**/*.c")
    add_files("deps/PlzmaSDK/src/**/*.cpp")
    
    -- Generate Info.plist from template during configuration
    on_config(function (target)
        local version = get_version_from_header()
        local info_plist_template = path.join(os.scriptdir(), "src/Info.plist.template")
        local info_plist_output = path.join(os.scriptdir(), "src/Info.plist")
        
        if os.isfile(info_plist_template) then
            -- Read the template
            local content = io.readfile(info_plist_template)
            -- Replace ${VERSION} placeholder with actual version
            content = content:gsub("${VERSION}", version)
            
            -- Write the processed Info.plist
            io.writefile(info_plist_output, content)
            print("Generated Info.plist with version: " .. version)
        else
            print("Warning: Info.plist.template not found at " .. info_plist_template)
        end
    end)
    
    -- Add the generated Info.plist file
    add_files("src/Info.plist")
    
    -- Clean up generated file after build (optional)
    after_clean(function (target)
        local info_plist_output = path.join(os.scriptdir(), "src/Info.plist")
        if os.isfile(info_plist_output) then
            os.rm(info_plist_output)
            print("Cleaned generated Info.plist")
        end
    end)
end
