add_rules("mode.debug", "mode.release")

target("macOSLauncher")
do
    add_rules("xcode.application")
    add_files("src/**/*.swift")
    add_files("src/*.swift", "src/*.xcassets")
    add_files("src/Info.plist")
    add_files("deps/PlzmaSDK/swift/*.swift")
    add_files("deps/PlzmaSDK/src/*.cpp")
    add_files("deps/PlzmaSDK/src/**/*.c")
    add_files("deps/PlzmaSDK/src/**/*.cpp")
    add_files("src/*.cc")
    add_packages("7z", "lzma", "librsync")
    add_ldflags("-lc++")
    add_values("xcode.bundle_identifier", "com.stfcmod.startrekpatch")
    add_scflags("-Xcc -fmodules", "-Xcc -fmodule-map-file=macos-launcher/src/module.modulemap", "-D SWIFT_PACKAGE",
        { force = true })
    add_values("xcode.bundle_display_name", "Star Trek Fleet Command Community Mod")
end
