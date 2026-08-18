-- Windows CI experiment for ordinary C++ compilation.
--
-- Replace only XMake's cxx file-build rule for targets that opt in. This keeps
-- independent custom rules (notably protobuf generation/compilation) intact.
rule("stfc.cxx.sccache")
    add_deps("c++")
    set_sourcekinds("cxx")

    on_buildcmd_file(function(target, batchcmds, sourcefile, opt)
        local compiler = import("core.tool.compiler")
        local find_tool = import("lib.detect.find_tool")

        local objectfile = target:objectfile(sourcefile)
        local compiler_inst = compiler.load("cxx", {target = target})
        local compiler_program, compiler_argv = compiler_inst:compargv(
            sourcefile,
            path(objectfile),
            {
                target = target,
                configs = opt.configs,
                rawargs = true
            })

        local sccache = target:data("stfc.windows.sccache")
        if not sccache then
            sccache = os.getenv("SCCACHE_PATH")
            if not sccache or sccache == "" then
                local tool = find_tool("sccache", {norun = true})
                sccache = tool and tool.program or nil
            end
            sccache = assert(sccache,
                "STFC_MSVC_SCCACHE=1 but sccache was not found")
            target:data_set("stfc.windows.sccache", sccache)
        end

        local sccache_args = {compiler_program}
        table.join2(sccache_args, compiler_argv)

        batchcmds:mkdir(path.directory(objectfile))
        batchcmds:show_progress(opt.progress,
            "${color.build.object}sccache compiling.$(mode) %s", sourcefile)
        batchcmds:vrunv(sccache, sccache_args, {envs = compiler_inst:runenvs()})

        batchcmds:add_depfiles(sourcefile)
        batchcmds:set_depcache(target:dependfile(objectfile))
        batchcmds:set_depmtime(os.mtime(objectfile))
    end)
