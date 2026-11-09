--[[
    rust.lua — Rust crate 构建调度

    xmake 的职责仅限于：确保 C 库已构建，然后调用 cargo。
    库路径推断、链接方式等细节由 danmakufactory-sys/build.rs 自行处理。
]]

--- 确保 C 库已构建（musl 模式下构建静态库，否则构建动态库）
local function configure_linking()
    import("core.project.config")
    import("core.project.project")
    config.load()
    local use_musl = is_plat("linux") and config.get("musl")
    local target_name = use_musl and "lib-static" or "lib-shared"
    local lib_target = project.target(target_name)
    local linkdirs, runtime_dirs = {}, {}
    for _, pkg in pairs(lib_target:pkgs()) do
        for _, dir in ipairs(table.wrap(pkg:get("linkdirs"))) do
            table.insert(linkdirs, path.absolute(dir))
            table.insert(runtime_dirs, path.absolute(dir))
        end
        local install_dir = pkg:installdir()
        if install_dir then
            table.insert(runtime_dirs, path.join(install_dir, "bin"))
        end
    end
    for _, dir in ipairs(path.splitenv(os.getenv("DF_XREPO_LIB_DIR") or "")) do
        table.insert(linkdirs, dir)
    end
    os.setenv("DF_XREPO_LIB_DIR", path.joinenv(table.unique(linkdirs)))
    os.setenv("DF_RUNTIME_DLL_DIRS", path.joinenv(table.unique(runtime_dirs)))
    os.setenv("DF_LIB_DIR", path.absolute(lib_target:targetdir()))
    os.setenv("DF_LIB_KIND", use_musl and "static" or "shared")
    return target_name, use_musl
end

local function cargo_args(command, manifest_dir, release)
    local args = {command, "--manifest-path", path.join(manifest_dir, "Cargo.toml")}
    if release then
        table.insert(args, "--release")
    end
    local _, use_musl = configure_linking()
    if use_musl then
        local targets = {x86_64 = "x86_64-unknown-linux-musl", x64 = "x86_64-unknown-linux-musl",
                         arm64 = "aarch64-unknown-linux-musl", aarch64 = "aarch64-unknown-linux-musl"}
        local rust_target = targets[config.arch()]
        if not rust_target then
            raise("Unsupported Rust musl architecture: " .. tostring(config.arch()))
        end
        table.insert(args, "--target")
        table.insert(args, rust_target)
    end
    return args
end

--- 构建指定 crate
-- @param manifest_dir string 包含 Cargo.toml 的目录
-- @param release boolean 是否 release 模式
function build_crate(manifest_dir, release)
    local target_name = configure_linking()
    os.execv("xmake", {"build", target_name})
    os.execv("cargo", cargo_args("build", manifest_dir, release))
end

--- 运行指定 crate 的二进制
-- @param manifest_dir string 包含 Cargo.toml 的目录
-- @param extra_args table 额外传递给二进制的参数
function run_crate(manifest_dir, extra_args)
    local target_name = configure_linking()
    os.execv("xmake", {"build", target_name})
    local args = cargo_args("run", manifest_dir, is_mode("release"))
    if extra_args and #extra_args > 0 then
        table.insert(args, "--")
        for _, arg in ipairs(extra_args) do
            table.insert(args, arg)
        end
    end
    os.execv("cargo", args)
end
