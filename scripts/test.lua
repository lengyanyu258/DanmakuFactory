--[[
    test.lua — 测试调度脚本

    所有测试的具体逻辑在此实现，xmake.lua 中只做简单调用：
        import("scripts.test").run_all()
        import("scripts.test").run_python()
        import("scripts.test").run_lua()
        ...
]]

import("lib.detect.find_tool")

local function ensure_lib()
    os.execv("xmake", {"build", "lib-shared"})
end

local function find_python()
    local python = find_tool("python") or find_tool("python3")
    if not python then
        raise("python not found!")
    end
    return python.program
end

--- 运行核心功能测试（FFI 冒烟 + 格式转换 + 配置解析）
function run_all()
    ensure_lib()
    local python = find_python()
    local tests_dir = path.join(os.projectdir(), "tests")

    print("=== FFI smoke test ===")
    os.execv(python, {path.join(tests_dir, "ffi_smoke.py")})
    print("\n=== Format conversion tests ===")
    os.execv(python, {path.join(tests_dir, "test_convert.py")})
    print("\n=== Config parsing tests ===")
    os.execv(python, {path.join(tests_dir, "test_config.py")})
    print("\nALL TESTS PASSED")
end

--- Python 绑定测试
function run_python()
    ensure_lib()
    local python = find_python()
    os.execv(python, {path.join(os.projectdir(), "bindings", "python", "test_binding.py")})
end

--- Lua 绑定测试（需要 LuaJIT）
function run_lua()
    ensure_lib()
    local luajit = find_tool("luajit")
    if not luajit then
        raise("luajit not found! Please install LuaJIT.")
    end
    os.execv(luajit.program, {path.join(os.projectdir(), "bindings", "lua", "test_binding.lua")})
end

--- C# 绑定测试（需要 dotnet SDK）
function run_csharp()
    ensure_lib()
    local dotnet = find_tool("dotnet")
    if not dotnet then
        raise("dotnet not found! Please install .NET SDK.")
    end
    os.execv(dotnet.program, {"run", "--project", path.join(os.projectdir(), "bindings", "csharp")})
end

--- JavaScript/WASM 绑定测试（需要 Node.js + 已构建的 WASM）
function run_wasm()
    local node = find_tool("node")
    if not node then
        raise("node not found! Please install Node.js.")
    end
    local wasm_js = path.join(os.projectdir(), "wasm", "danmakufactory.js")
    if not os.isfile(wasm_js) then
        raise("WASM not built. Run `xmake f -p wasm && xmake build lib-wasm` first.")
    end
    os.execv(node.program, {path.join(os.projectdir(), "bindings", "javascript", "test_binding.mjs")})
end
