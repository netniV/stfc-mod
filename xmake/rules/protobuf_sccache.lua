-- Narrow CI experiment for XMake 3.1.0's protobuf.cpp rule.
-- Generation behavior stays the same; only generated C++ compilation is
-- wrapped with sccache so the experiment does not affect ordinary C++/Swift.
--
-- Keep all runtime logic inside XMake rule callbacks. XMake sandboxes/forks
-- callback functions, and top-level helper functions do not reliably retain
-- sandbox globals such as assert/imported modules in XMake 3.1.0.

rule("stfc.protobuf.cpp.sccache")
    add_deps("c++")
    set_extensions(".proto")

    after_load(function(target)
        local sourcebatch = target:sourcebatches()["stfc.protobuf.cpp.sccache"]
        for _, sourcefile_proto in ipairs(sourcebatch and sourcebatch.sourcefiles or {}) do
            local fileconfig = target:fileconfig(sourcefile_proto)
            assert(not (fileconfig and fileconfig.proto_grpc_cpp_plugin),
                "stfc.protobuf.cpp.sccache does not support proto_grpc_cpp_plugin")

            local prefixdir = fileconfig and fileconfig.proto_rootdir
            local autogendir = fileconfig and fileconfig.proto_autogendir
            local rootdir = autogendir or path.join(target:autogendir(), "rules", "protobuf")
            local filename = path.basename(sourcefile_proto) .. ".pb.cc"
            local sourcefile_cx = target:autogenfile(sourcefile_proto, {
                rootdir = rootdir,
                filename = filename
            })
            local sourcefile_dir = prefixdir and path.join(rootdir, prefixdir)
                or path.directory(sourcefile_cx)

            target:add("includedirs", sourcefile_dir, {
                public = fileconfig and fileconfig.proto_public or nil
            })
            table.insert(target:objectfiles(), target:objectfile(sourcefile_cx))
        end
    end)

    on_preparecmd_file(function(target, batchcmds, sourcefile_proto, opt)
        local find_tool = import("lib.detect.find_tool")

        local fileconfig = target:fileconfig(sourcefile_proto)
        assert(not (fileconfig and fileconfig.proto_grpc_cpp_plugin),
            "stfc.protobuf.cpp.sccache does not support proto_grpc_cpp_plugin")

        local prefixdir = fileconfig and fileconfig.proto_rootdir
        local autogendir = fileconfig and fileconfig.proto_autogendir
        local rootdir = autogendir or path.join(target:autogendir(), "rules", "protobuf")
        local filename = path.basename(sourcefile_proto) .. ".pb.cc"
        local sourcefile_cx = target:autogenfile(sourcefile_proto, {
            rootdir = rootdir,
            filename = filename
        })
        local sourcefile_dir = prefixdir and path.join(rootdir, prefixdir)
            or path.directory(sourcefile_cx)

        local protoc = target:data("stfc.protobuf.protoc")
        if not protoc then
            local tool = find_tool("protoc", {envs = target:pkgenvs()})
            protoc = assert(tool and tool.program, "protoc not found!")
            target:data_set("stfc.protobuf.protoc", protoc)
        end

        local protoc_args = {
            sourcefile_proto,
            "-I" .. (prefixdir or path.directory(sourcefile_proto)),
            "--cpp_out=" .. sourcefile_dir
        }
        if fileconfig and fileconfig.proto_flags then
            table.join2(protoc_args, fileconfig.proto_flags)
        end

        batchcmds:mkdir(sourcefile_dir)
        batchcmds:show_progress(opt.progress,
            "${color.build.object}compiling.proto.c++ %s", sourcefile_proto)
        batchcmds:vrunv(protoc, protoc_args, {envs = target:pkgenvs()})

        -- Preserve XMake 3.1.0's protobuf generation dependency behavior.
        batchcmds:add_depfiles(sourcefile_proto)
        batchcmds:set_depcache(target:dependfile(sourcefile_cx))
        batchcmds:set_depmtime(os.mtime(sourcefile_cx))
    end)

    on_buildcmd_file(function(target, batchcmds, sourcefile_proto, opt)
        local compiler = import("core.tool.compiler")
        local find_tool = import("lib.detect.find_tool")

        local fileconfig = target:fileconfig(sourcefile_proto)
        assert(not (fileconfig and fileconfig.proto_grpc_cpp_plugin),
            "stfc.protobuf.cpp.sccache does not support proto_grpc_cpp_plugin")

        local prefixdir = fileconfig and fileconfig.proto_rootdir
        local autogendir = fileconfig and fileconfig.proto_autogendir
        local rootdir = autogendir or path.join(target:autogendir(), "rules", "protobuf")
        local filename = path.basename(sourcefile_proto) .. ".pb.cc"
        local sourcefile_cx = target:autogenfile(sourcefile_proto, {
            rootdir = rootdir,
            filename = filename
        })
        local sourcefile_dir = prefixdir and path.join(rootdir, prefixdir)
            or path.directory(sourcefile_cx)
        local objectfile = target:objectfile(sourcefile_cx)

        local compiler_inst = compiler.load("cxx", {target = target})

        -- rawargs is important on Windows: sccache, not cl.exe, is the immediate
        -- child process and must receive the logical compiler arguments once.
        local compiler_program, compiler_argv = compiler_inst:compargv(
            sourcefile_cx,
            path(objectfile),
            {
                target = target,
                configs = {includedirs = sourcefile_dir},
                rawargs = true
            })

        -- XMake 3.1.0 includes target.frameworks in C++ object flags, so the
        -- macOS target contributes "-framework Cocoa" even when compiling a
        -- generated protobuf translation unit. clang accepts that link-only
        -- pair during -c, but sccache 0.17.0 does not model -framework as an
        -- option-with-value and therefore mistakes "Cocoa" for another input
        -- file. Strip only these link-only framework pairs from protobuf
        -- compilation; keep -F/-iframework search paths and target linkage.
        if target:is_plat("macosx") then
            local filtered_argv = {}
            local skip_framework_name = false
            for _, arg in ipairs(compiler_argv) do
                if skip_framework_name then
                    skip_framework_name = false
                elseif arg == "-framework" then
                    skip_framework_name = true
                else
                    table.insert(filtered_argv, arg)
                end
            end
            assert(not skip_framework_name, "missing framework name after -framework")
            compiler_argv = filtered_argv
        end

        local sccache = target:data("stfc.protobuf.sccache")
        if not sccache then
            -- sccache-action exports the exact executable path. Prefer it over
            -- PATH probing so compiler-specific run environments cannot hide it.
            sccache = os.getenv("SCCACHE_PATH")
            if not sccache or sccache == "" then
                local tool = find_tool("sccache", {norun = true})
                sccache = tool and tool.program or nil
            end
            sccache = assert(sccache,
                "STFC_PROTOBUF_SCCACHE=1 but sccache was not found")
            target:data_set("stfc.protobuf.sccache", sccache)
        end

        local sccache_args = {compiler_program}
        table.join2(sccache_args, compiler_argv)

        batchcmds:mkdir(path.directory(objectfile))
        batchcmds:show_progress(opt.progress,
            "${color.build.object}sccache compiling.proto.$(mode) %s", sourcefile_cx)
        batchcmds:vrunv(sccache, sccache_args, {envs = compiler_inst:runenvs()})

        -- Preserve the built-in rule's incremental metadata. Cross-run reuse is
        -- owned by sccache, whose key is based on compiler + args + preprocessed
        -- input, rather than these mtimes.
        batchcmds:add_depfiles(sourcefile_proto)
        batchcmds:set_depcache(target:dependfile(objectfile))
        batchcmds:set_depmtime(os.mtime(objectfile))
    end)
