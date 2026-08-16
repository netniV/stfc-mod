add_requireconfs("cpr.libcurl", {
    override = true,
    version = "8.21.0",
    system = false,
    configs = {
        shared = false,
        openssl = false,
        openssl3 = false,
        zlib = true
    }
})
