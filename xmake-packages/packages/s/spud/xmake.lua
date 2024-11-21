package("spud")
add_deps("cmake")
add_urls("https://github.com/tashcan/spud/archive/refs/tags/$(version).tar.gz",
    "https://github.com/tashcan/spud.git")
add_versions("v0.1.1", "4298ec14727080166a959051d647a2acfcdceb0170bd1d269c1c76c8e51c1dca")
on_install(function(package)
    local configs = {}
    table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
    table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
    table.insert(configs, "-DSPUD_BUILD_TESTS=OFF")
    table.insert(configs, "-DSPUD_NO_LTO=ON")
    table.insert(configs, "-DSPUD_DETOUR_TRACING=OFF")
    import("package.tools.cmake").install(package, configs)
end)
