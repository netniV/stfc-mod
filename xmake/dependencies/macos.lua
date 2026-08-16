add_requireconfs("cpr.libcurl", {
    override = true,
    version = "8.21.0",
    system = false,
    configs = {
        shared = false,
        openssl = false,
        openssl3 = true,
        apple_sectrust = true,
        zlib = true
    }
})

add_requireconfs("cpr.libcurl.openssl3", {
    version = "3.6.3",
    system = false,
    configs = {
        shared = false
    }
})

add_requires("inifile-cpp")
add_requires("librsync")
add_requires("PLzmaSDK")
