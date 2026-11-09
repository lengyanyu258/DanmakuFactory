function package_release(target, runtime_files)
    -- 创建临时工作目录
    local tmp_dir = path.join(os.tmpdir(), "danmaku_package")
    os.mkdir(tmp_dir)
    os.rm(path.join(tmp_dir, "*")) -- 清空临时目录

    -- 复制可执行文件
    os.cp(target:targetfile(), tmp_dir)
    for _, file in ipairs(runtime_files or {}) do
        os.cp(file, tmp_dir)
    end

    -- 复制文档文件
    local docs_dir = path.join(os.projectdir(), "docs")
    if os.isdir(docs_dir) then
        os.cp(path.join(docs_dir, "*"), tmp_dir)
    end

    -- 列出临时目录内容，便于调试
    print(os.filedirs(tmp_dir .. "/**"))

    -- 创建打包目录
    local package_dir = path.join(os.projectdir(), 'build', "packages")
    os.mkdir(package_dir)

    -- 生成压缩包名称
    -- TODO: 这里的版本号需要从分支名称中获取
    local package_name = ("%s-dev-%s-%s-CLI"):format(target:basename(), os.host(), os.arch())
    local package_path = path.join(package_dir, package_name)

    -- 执行打包（跨平台兼容）
    print("Packaging %s...", package_name)
    if is_plat("windows") then
        package_path = package_path .. ".zip"
        os.execv("powershell",
            { "-Command", string.format(
                "Compress-Archive -Path '%s\\*' -DestinationPath '%s' -Force -CompressionLevel Optimal",
                tmp_dir, package_path) })
    else
        package_path = package_path .. ".tar.gz"
        os.execv("tar", { "--use-compress-program", "gzip -9", "-cvf", package_path, "-C", tmp_dir, "." })
    end

    -- 清理临时目录
    os.rm(tmp_dir)

    print("Package created: %s", package_path)
end

-- Rust CLI 替代原 xmake binary target 后，需要独立定位 Cargo 产物。
function package_release_cli()
    import("core.project.config")
    import("core.project.project")
    config.load()
    local use_musl = is_plat("linux") and config.get("musl")
    local cargo_dir = os.getenv("CARGO_TARGET_DIR") or path.join(os.projectdir(), "cli", "target")
    cargo_dir = path.absolute(cargo_dir)
    if use_musl then
        local triples = {x86_64 = "x86_64-unknown-linux-musl", x64 = "x86_64-unknown-linux-musl",
                         arm64 = "aarch64-unknown-linux-musl", aarch64 = "aarch64-unknown-linux-musl"}
        local triple = triples[config.arch()]
        if not triple then
            raise("Unsupported Rust musl architecture: " .. tostring(config.arch()))
        end
        cargo_dir = path.join(cargo_dir, triple)
    end
    local binary = path.join(cargo_dir, "release", is_plat("windows") and "DanmakuFactory.exe" or "DanmakuFactory")
    if not os.isfile(binary) then
        raise("Rust CLI release binary not found: " .. binary)
    end
    local runtime_files = {}
    if not use_musl then
        local lib_target = project.target("lib-shared")
        -- DLL 同时被 build.rs 放到 Cargo 输出目录；包含传递的 Windows DLL。
        if is_plat("windows") then
            for _, file in ipairs(os.files(path.join(cargo_dir, "release", "*.dll"))) do
                table.insert(runtime_files, file)
            end
        else
            local pattern = is_plat("macosx") and "*.dylib" or "*.so*"
            for _, file in ipairs(os.files(path.join(lib_target:targetdir(), pattern))) do
                table.insert(runtime_files, file)
            end
        end
    end
    package_release({
        targetfile = function () return binary end,
        basename = function () return "DanmakuFactory" end,
    }, runtime_files)
end

return package_release
