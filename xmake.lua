local PROJECT_NAME = "DanmakuFactory"
local LIB_VERSION = "2.0.0"
local CLI_VERSION = "2.0.0"

set_project(PROJECT_NAME)
set_version("2.0.0")

------------------------------------------------------------------------------
-- 构建选项
------------------------------------------------------------------------------

option("musl")
    set_default(false)
    set_showmenu(true)
    set_description("Linux musl libc fully-static build (no glibc dependency)")
    set_category("build")
option_end()

local use_musl = is_plat("linux") and has_config("musl")

------------------------------------------------------------------------------
-- 依赖声明（musl 模式下强制静态）
------------------------------------------------------------------------------

if use_musl then
    add_requires("pcre2", {configs = {shared = false}})
    add_requires("yyjson", {configs = {shared = false}})
    add_requires("freetype", {configs = {shared = false}})
    add_requires("harfbuzz", {configs = {shared = false}})
    add_requires("fontconfig", {configs = {shared = false}})
else
    add_requires("pcre2")
    add_requires("yyjson")
    add_requires("freetype")
    add_requires("harfbuzz")
    if is_plat("wasm") then
        add_requires("emscripten")
        set_toolchains("emcc@emscripten")
    else
        add_requires("fontconfig")
    end
end

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

-- 源码为 UTF-8 编码，MSVC 默认按本地代码页解析会误判中文注释后的代码
if is_plat("windows") then
    add_cxflags("/utf-8")
end

-- musl 全静态链接：所有二进制产物不依赖任何 .so
if use_musl then
    add_cxflags("-static")
    add_ldflags("-static", {force = true})
end

------------------------------------------------------------------------------
-- 核心库目标
------------------------------------------------------------------------------

target("lib-static")
    -- Windows 下静态库与动态库导入库同为 .lib，同名会互相覆盖
    if is_plat("windows") then
        set_basename(PROJECT_NAME .. "-static")
    else
        set_basename(PROJECT_NAME)
    end
    -- musl 模式下静态库为主要产物
    set_default(use_musl)
    set_kind("static")
    set_languages("c11")
    set_version(LIB_VERSION)

    if is_plat("windows") then
        add_defines("_CRT_SECURE_NO_WARNINGS")
        add_defines("DANMAKU_FACTORY_STATIC", {public = true})
    end
    add_defines('DF_LIB_VERSION="' .. LIB_VERSION .. '"')

    -- 配置文件与自定义模板模块已重新纳入编译
    add_files("lib/src/**.c", {sourcekind = "cc"})
    add_includedirs("lib/include", {public = true})
    add_includedirs("lib/src")
    add_packages("pcre2")
    add_packages("yyjson")
    add_packages("freetype", "harfbuzz")
    if not is_plat("wasm") then
        add_packages("fontconfig")
        add_defines("DF_HAS_FONTCONFIG")
    end
    add_defines("DF_HAS_TEXT_MEASURE")

target("lib-shared")
    set_basename(PROJECT_NAME)
    -- musl 全静态模式下 .so 无意义，不作为默认构建目标
    set_default(not use_musl)
    set_kind("shared")
    set_languages("c11")
    set_version(LIB_VERSION)

    -- 导出符号（关键！直接编译源码而非链接静态库，确保 dllexport 生效）
    if is_plat("windows") then
        add_defines("_CRT_SECURE_NO_WARNINGS")
        add_defines("DANMAKU_FACTORY_EXPORTS")
    else
        add_cxflags("-fvisibility=hidden")
    end
    add_defines('DF_LIB_VERSION="' .. LIB_VERSION .. '"')

    add_files("lib/src/**.c", {sourcekind = "cc"})
    add_includedirs("lib/include", {public = true})
    add_includedirs("lib/src")
    add_packages("pcre2")
    add_packages("yyjson")
    add_packages("freetype", "harfbuzz")
    if not is_plat("wasm") then
        add_packages("fontconfig")
        add_defines("DF_HAS_FONTCONFIG")
    end
    add_defines("DF_HAS_TEXT_MEASURE")

------------------------------------------------------------------------------
-- WASM 目标（需要 Emscripten 工具链: xmake f -p wasm）
------------------------------------------------------------------------------

target("lib-wasm")
    set_plat("wasm")
    set_default(false)
    set_kind("binary")
    set_languages("c11")
    set_basename("danmakufactory")
    set_extension(".js")

    add_files("lib/src/**.c", {sourcekind = "cc"})
    add_includedirs("lib/include", "lib/src")
    add_packages("yyjson")
    add_packages("freetype", "harfbuzz")
    add_defines("__EMSCRIPTEN__")
    add_defines("DF_HAS_TEXT_MEASURE")
    add_defines('DF_LIB_VERSION="' .. LIB_VERSION .. '"')

    -- Emscripten 链接选项（使用 -sKEY=VALUE 合并格式避免 xmake 过滤）
    add_ldflags("-sMODULARIZE=1", {force = true})
    add_ldflags("-sEXPORT_NAME=DanmakuFactory", {force = true})
    add_ldflags("-sALLOW_MEMORY_GROWTH=1", {force = true})
    add_ldflags("-sINITIAL_MEMORY=16777216", {force = true})
    add_ldflags("-sENVIRONMENT=web,node", {force = true})
    add_ldflags("-sFILESYSTEM=1", {force = true})
    add_ldflags("-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','UTF8ToString','stringToUTF8']", {force = true})
    add_ldflags("-sEXPORTED_FUNCTIONS=['_df_get_version','_df_run_from_json','_df_get_last_error_code','_df_get_last_error_message','_df_get_last_error_file','_df_get_last_error_function','_df_get_last_error_line','_df_get_last_error_full_message','_df_create','_df_destroy','_df_run','_df_load_config_json','_df_load_config_file','_df_add_input_file','_df_add_input_file_typed','_df_clear_input_files','_df_set_output_file','_df_set_output_file_typed','_df_set_resolution','_df_set_display_area','_df_set_scroll_area','_df_set_statistics_mode','_df_set_density','_df_set_scroll_duration','_df_set_fixed_duration','_df_set_show_username','_df_set_block_mode','_df_set_keep_blocked','_df_set_fontname','_df_set_font_size','_df_set_font_size_strict','_df_set_font_size_norm','_df_set_bold','_df_set_shadow','_df_set_opacity','_df_set_outline','_df_set_outline_blur','_df_set_outline_opacity','_df_set_line_spacing','_df_set_msgbox_show','_df_set_msgbox_size','_df_set_msgbox_pos','_df_set_msgbox_font_size','_df_set_msgbox_duration','_df_set_msgbox_gift_min_price','_df_text_register_font','_malloc','_free']", {force = true})

------------------------------------------------------------------------------
-- 任务调度器：Rust 构建（具体逻辑见 scripts/rust.lua）
------------------------------------------------------------------------------

task("cli")
    set_category("build")
    set_menu {
        usage = "xmake cli",
        description = "Build the Rust CLI (auto-builds lib-shared if needed).",
    }
    on_run(function ()
        import("core.project.config")
        config.load()
        import("scripts.rust").build_crate(path.join(os.projectdir(), "cli"), is_mode("release"))
    end)

task("run-cli")
    set_category("run")
    set_menu {
        usage = "xmake run-cli",
        description = "Run the built Rust CLI binary.",
    }
    on_run(function ()
        import("scripts.rust").run_crate(path.join(os.projectdir(), "cli"), os.argv())
    end)

task("gui")
    set_category("build")
    set_menu {
        usage = "xmake gui",
        description = "Build the Rust + Slint GUI (auto-builds lib-shared if needed).",
    }
    on_run(function ()
        import("core.project.config")
        config.load()
        import("scripts.rust").build_crate(path.join(os.projectdir(), "gui", "slint"), is_mode("release"))
    end)

task("run-gui")
    set_category("run")
    set_menu {
        usage = "xmake run-gui",
        description = "Run the built Rust GUI binary.",
    }
    on_run(function ()
        import("scripts.rust").run_crate(path.join(os.projectdir(), "gui", "slint"))
    end)

task("package-cli")
    set_category("package")
    set_menu {
        usage = "xmake package-cli",
        description = "Build release CLI and create distribution package.",
    }
    on_run(function ()
        import("scripts.rust").build_crate(path.join(os.projectdir(), "cli"), true)
        import("scripts.package").package_release_cli()
    end)

task("deploy-web")
    set_category("package")
    set_menu {
        usage = "xmake deploy-web",
        description = "Build WASM and deploy to docs/wasm/ for GitHub Pages.",
    }
    on_run(function ()
        import("core.project.config")
        config.load()

        -- 定位 WASM 构建产物
        local wasm_dir = path.join(os.projectdir(), "build", "wasm", "wasm32", "release")
        local js_file = path.join(wasm_dir, "danmakufactory.js")
        local wasm_file = path.join(wasm_dir, "danmakufactory.wasm")
        if not os.isfile(js_file) or not os.isfile(wasm_file) then
            raise("WASM build not found. Run `xmake f -p wasm -a wasm32 -m release && xmake build lib-wasm` first.")
        end

        -- 复制到 docs/wasm/
        local deploy_dir = path.join(os.projectdir(), "docs", "wasm")
        os.mkdir(deploy_dir)
        os.cp(js_file, deploy_dir)
        os.cp(wasm_file, deploy_dir)
        print("Deployed WASM to docs/wasm/:")
        print("  - danmakufactory.js")
        print("  - danmakufactory.wasm")
        print("GitHub Pages site: docs/index.html")
    end)

------------------------------------------------------------------------------
-- 工具任务
------------------------------------------------------------------------------

task("clang-format")
    set_category("format")
    set_menu {
        usage = "xmake clang-format",
        description = "Format C source files with clang-format.",
    }
    on_run(function ()
        import("lib.detect.find_tool")

        local tool = find_tool("clang-format")
        if not tool then
            raise("clang-format not found!")
        end

        local files = {"lib/include/**", "lib/src/**"}
        for _, pattern in ipairs(files) do
            local filelist = os.files(pattern)
            for _, file in ipairs(filelist) do
                print("Formatting file:", file)
                os.execv(tool.program, {"-i", file})
            end
        end
    end)

------------------------------------------------------------------------------
-- 测试目标
------------------------------------------------------------------------------

target("test_yyjson")
    set_kind("binary")
    set_group("tests")
    set_languages("c11")
    add_files("tests/yyjson.c", {sourcekind = "cc"})
    add_packages("yyjson")

------------------------------------------------------------------------------
-- 测试任务（具体逻辑见 scripts/test.lua）
------------------------------------------------------------------------------

task("test")
    set_category("test")
    set_menu {
        usage = "xmake test",
        description = "Run all core tests (smoke + format regression + config).",
    }
    on_run(function ()
        import("scripts.test").run_all()
    end)

task("test-python")
    set_category("test")
    set_menu {
        usage = "xmake test-python",
        description = "Run Python FFI binding tests.",
    }
    on_run(function ()
        import("scripts.test").run_python()
    end)

task("test-lua")
    set_category("test")
    set_menu {
        usage = "xmake test-lua",
        description = "Run Lua FFI binding tests (requires LuaJIT).",
    }
    on_run(function ()
        import("scripts.test").run_lua()
    end)

task("test-csharp")
    set_category("test")
    set_menu {
        usage = "xmake test-csharp",
        description = "Run C# P/Invoke binding tests (requires dotnet SDK).",
    }
    on_run(function ()
        import("scripts.test").run_csharp()
    end)

task("test-wasm")
    set_category("test")
    set_menu {
        usage = "xmake test-wasm",
        description = "Run JavaScript/WASM binding tests (requires Node.js + built WASM).",
    }
    on_run(function ()
        import("scripts.test").run_wasm()
    end)
