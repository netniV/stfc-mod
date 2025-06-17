package("protobuf")
add_urls("https://github.com/protocolbuffers/protobuf/releases/download/v$(version)/protobuf-$(version).tar.gz")
add_versions("29.3", "008a11cc56f9b96679b4c285fd05f46d317d685be3ab524b2a310be0fbad987e")
add_versions("31.1", "12bfd76d27b9ac3d65c00966901609e020481b9474ef75c7ff4601ac06fa0b82")

add_configs("zlib", {description = "Enable zlib", default = false, type = "boolean"})

add_deps("cmake")

on_load(function (package)
    package:add("deps", "abseil")

    if package:is_plat("windows") then
        package:add("links", "libprotoc", "libprotobuf", "utf8_range", "utf8_validity", "libutf8_range", "libutf8_validity")
    else
        package:add("links", "protoc", "protobuf", "utf8_range", "utf8_validity")
    end

    if package:config("zlib") then
        package:add("deps", "zlib")
    end

    if package:is_plat("windows") and package:config("shared") then
        package:add("defines", "PROTOBUF_USE_DLLS")
    end

    package:addenv("PATH", package:installdir("bin"))
end)

-- ref: https://github.com/conan-io/conan-center-index/blob/19c9de61cce5a5089ce42b0cf15a88ade7763275/recipes/protobuf/all/conanfile.py
on_component("utf8_range", function (package, component)
    component:add("extsources", "pkgconfig::utf8_range")
    component:add("links", "utf8_validity", "utf8_range")
end)

on_component("protobuf", function (package, component)
    component:add("extsources", "pkgconfig::protobuf")
    if is_plat("windows") then
        component:add("links", "libprotobuf", "utf8_validity")
    else
        component:add("links", "protobuf", "utf8_validity")
    end
end)

on_component("protobuf-lite", function (package, component)
    component:add("extsources", "pkgconfig::protobuf-lite")
    if is_plat("windows") then
        component:add("links", "libprotobuf-lite", "utf8_validity")
    else
        component:add("links", "protobuf-lite", "utf8_validity")
    end
end)

on_component("protoc", function (package, component)
    component:add("deps", "protobuf")
    if is_plat("windows") then
        component:add("links", "libprotoc")
    else
        component:add("links", "protoc")
    end
end)

on_install(function (package)
    local configs = {
        "-DCMAKE_CXX_STANDARD=23",
        "-Dprotobuf_BUILD_TESTS=OFF",
        "-Dprotobuf_BUILD_PROTOC_BINARIES=ON"
    }

    table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))

    if package:config("shared") then
        table.insert(configs, "-Dprotobuf_BUILD_SHARED_LIBS=ON")
        table.insert(configs, "-DPROTOBUF_USE_DLLS")
    end

    if package:config("zlib") then
        table.insert(configs, "-Dprotobuf_WITH_ZLIB=ON")
        table.insert(configs, "-Dprotobuf_MSVC_STATIC_RUNTIME=OFF")
    end

    io.replace("CMakeLists.txt", "set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)", "", {plain = true})
    io.replace("CMakeLists.txt", "set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL)", "", {plain = true})

    local opt = {}
    opt.builddir = "build"
    opt.packagedeps = "abseil"

    if package:version():lt("30.0") then
        table.insert(configs, "-Dprotobuf_ABSL_PROVIDER=package")
    end

    import("package.tools.cmake").install(package, configs, opt)
end)

