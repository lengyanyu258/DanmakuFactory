local PROJECT_NAME = path.basename(os.projectdir())
local LIB_VERSION = "2.0.0"
local CLI_VERSION = "2.0.0"

set_project(PROJECT_NAME)
set_version("2.0.0")

add_requires("pcre2")
add_requires("yyjson")
add_requires("cli11")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

target("lib-static")
    set_basename(PROJECT_NAME)
    set_default(false)
    set_kind("static")
    set_languages("c11")
    set_version(LIB_VERSION)

    if is_plat("windows") then
        add_defines("_CRT_SECURE_NO_WARNINGS")
    end

    add_files("lib/src/**.c", {sourcekind = "cc"})
    add_includedirs("lib/include", {public = true})
    add_includedirs("lib/src")
    add_packages("pcre2")
    add_packages("yyjson")

target("lib-shared")
    set_basename(PROJECT_NAME)
    set_default(false)
    set_kind("shared")
    set_languages("c11")
    set_version(LIB_VERSION)

    -- 导出符号（关键！）
    if is_plat("windows") then
        add_defines("DANMAKU_FACTORY_EXPORTS")
    -- else
    --     add_cxflags("-fvisibility=default", {public = true})
    end
    
    add_deps("lib-static")

target("cli")
    set_basename(PROJECT_NAME)
    set_kind("binary")
    set_languages("c11")
    set_version(CLI_VERSION)

    add_deps("lib-static")
    add_files("cli/**.c")

    on_package(function (target)
        if is_mode("release") then
            import("scripts.package").package_release(target)
        end
    end)

task("clang-format")
    on_run(function ()
        import("lib.detect.find_tool")
        
        local tool = find_tool("clang-format")
        if not tool then
            raise("clang-format not found!")
        end

        local files = {"include/**", "src/**"}
        for _, pattern in ipairs(files) do
            local filelist = os.files(pattern)
            for _, file in ipairs(filelist) do
                print("Formatting file:", file)
                os.execv(tool.program, {"-i", file})
            end
        end
    end)

-- 每个测试模块一个 target（可并行、可单独跑）
target("test_cli")
    set_kind("binary")
    set_group("tests")               -- `xmake run -g tests` 一键跑全部
    set_languages("c++17")
    add_files("tests/cli.cpp")
    add_packages("cli11")               

target("test_yyjson")
    set_kind("binary")
    set_group("tests")
    set_languages("c11")
    add_files("tests/yyjson.c", {sourcekind = "cc"})
    add_packages("yyjson")

target("test_argparse")
    set_kind("binary")
    set_group("tests")
    set_languages("c++17")
    add_files("tests/argparse.cpp", {sourcekind = "cxx"})
    add_packages("cli11")
