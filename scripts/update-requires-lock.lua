-- Regenerate xmake-requires.lock for every CI platform without compiling the
-- packages or requiring the target SDK. Run from the repository root with:
--
--   xmake l scripts/update-requires-lock.lua

import("core.project.config")
import("private.action.require.impl.lock_packages")
import("private.action.require.impl.package")
import("private.action.require.impl.utils.get_requires")

local targets = {
    {plat = "windows", arch = "x64"},
    {plat = "macosx", arch = "arm64"},
    {plat = "macosx", arch = "x86_64"}
}

function main(plat, arch)
    if not plat then
        local script = path.join(os.projectdir(), "scripts", "update-requires-lock.lua")
        for _, target in ipairs(targets) do
            os.vrunv(os.programfile(), {"l", script, target.plat, target.arch}, {curdir = os.projectdir()})
        end
        return
    end

    assert(arch, "missing architecture for platform " .. plat)

    config.load()
    config.set("plat", plat, {force = true})
    config.set("arch", arch, {force = true})

    local requires, requires_extra = get_requires()
    local packages = package.load_packages(requires, {requires_extra = requires_extra})
    lock_packages(packages)

    cprint("${green}locked %d packages for %s|%s", #packages, plat, arch)
end
