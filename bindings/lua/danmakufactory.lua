--[[
DanmakuFactory Lua 绑定

通过 LuaJIT FFI 调用 DanmakuFactory 动态库。

前置条件:
    1. 安装 LuaJIT: https://luajit.org/
    2. 构建动态库: xmake build lib-shared

用法:
    local df = require("danmakufactory")
    print(df.version())
    df.convert("input.xml", "output.ass")
    df.convert("input.xml", "output.ass", { text = { font_size = 25 } })

运行测试:
    luajit bindings/lua/test_binding.lua
]]

local ffi = require("ffi")
local json = nil -- 需要用户自行提供 JSON 库（如 dkjson, cjson, lua-resty-json）

-- C 函数声明
ffi.cdef[[
    typedef int df_error_code_t;
    typedef struct df_context_s df_context_t;

    /* 基础查询 */
    const char *df_get_version(void);

    /* 错误查询（线程局部） */
    df_error_code_t df_get_last_error_code(void);
    const char *df_get_last_error_message(void);
    const char *df_get_last_error_file(void);
    const char *df_get_last_error_function(void);
    int df_get_last_error_line(void);
    const char *df_get_last_error_full_message(void);

    /* JSON 一次性 API */
    df_error_code_t df_run_from_json(const char *runtime_info_json_str);

    /* 句柄生命周期 */
    df_context_t *df_create(void);
    void df_destroy(df_context_t *ctx);
    df_error_code_t df_run(df_context_t *ctx);

    /* 批量配置加载 */
    df_error_code_t df_load_config_json(df_context_t *ctx, const char *json_str);
    df_error_code_t df_load_config_file(df_context_t *ctx, const char *path);

    /* 文件管理 */
    df_error_code_t df_add_input_file(df_context_t *ctx, const char *path, float time_shift);
    df_error_code_t df_clear_input_files(df_context_t *ctx);
    df_error_code_t df_set_output_file(df_context_t *ctx, const char *path);

    /* 屏幕配置 */
    df_error_code_t df_set_resolution(df_context_t *ctx, int width, int height);
    df_error_code_t df_set_display_area(df_context_t *ctx, float area);
    df_error_code_t df_set_scroll_area(df_context_t *ctx, float area);
    df_error_code_t df_set_statistics_mode(df_context_t *ctx, int mode);

    /* 弹幕配置 */
    df_error_code_t df_set_density(df_context_t *ctx, int density);
    df_error_code_t df_set_scroll_duration(df_context_t *ctx, float seconds);
    df_error_code_t df_set_fixed_duration(df_context_t *ctx, float seconds);
    df_error_code_t df_set_show_username(df_context_t *ctx, bool show);
    df_error_code_t df_set_block_mode(df_context_t *ctx, int mode);
    df_error_code_t df_set_keep_blocked(df_context_t *ctx, bool keep);

    /* 文本配置 */
    df_error_code_t df_set_fontname(df_context_t *ctx, const char *fontname);
    df_error_code_t df_set_font_size(df_context_t *ctx, int size);
    df_error_code_t df_set_font_size_strict(df_context_t *ctx, bool strict);
    df_error_code_t df_set_font_size_norm(df_context_t *ctx, bool norm);
    df_error_code_t df_set_bold(df_context_t *ctx, bool bold);
    df_error_code_t df_set_shadow(df_context_t *ctx, float shadow);
    df_error_code_t df_set_opacity(df_context_t *ctx, int opacity);
    df_error_code_t df_set_outline(df_context_t *ctx, float outline);
    df_error_code_t df_set_outline_blur(df_context_t *ctx, float blur);
    df_error_code_t df_set_outline_opacity(df_context_t *ctx, int opacity);
    df_error_code_t df_set_line_spacing(df_context_t *ctx, int spacing);

    /* 消息框配置 */
    df_error_code_t df_set_msgbox_show(df_context_t *ctx, bool show);
    df_error_code_t df_set_msgbox_size(df_context_t *ctx, int width, int height);
    df_error_code_t df_set_msgbox_pos(df_context_t *ctx, int x, int y);
    df_error_code_t df_set_msgbox_font_size(df_context_t *ctx, int size);
    df_error_code_t df_set_msgbox_duration(df_context_t *ctx, float seconds);
    df_error_code_t df_set_msgbox_gift_min_price(df_context_t *ctx, float price);
]]

local M = {}

local lib = nil

--- 加载动态库
-- @param path string|nil 动态库路径，nil 时自动搜索
-- @return table DanmakuFactory 模块
local function load_library(path)
    if lib then return end

    if path then
        lib = ffi.load(path)
        return
    end

    -- 自动搜索
    local package_path = package.cpath or ""
    local search_names

    if jit.os == "Windows" then
        search_names = { "DanmakuFactory.dll", "DanmakuFactory" }
    elseif jit.os == "OSX" then
        search_names = { "libDanmakuFactory.dylib", "DanmakuFactory" }
    else
        search_names = { "libDanmakuFactory.so", "DanmakuFactory" }
    end

    -- 尝试从 build 目录加载
    local script_dir = debug.getinfo(1, "S").source:match("@?(.*[/\\])") or "./"
    local repo_root = script_dir .. "../../"

    local build_patterns = {
        repo_root .. "build/windows/x64/release/DanmakuFactory.dll",
        repo_root .. "build/windows/x64/debug/DanmakuFactory.dll",
        repo_root .. "build/linux/x86_64/release/libDanmakuFactory.so",
        repo_root .. "build/linux/x86_64/debug/libDanmakuFactory.so",
        repo_root .. "build/macosx/x86_64/release/libDanmakuFactory.dylib",
        repo_root .. "build/macosx/x86_64/debug/libDanmakuFactory.dylib",
    }

    for _, name in ipairs(build_patterns) do
        local ok, loaded = pcall(ffi.load, name)
        if ok then
            lib = loaded
            return
        end
    end

    -- 尝试系统路径
    for _, name in ipairs(search_names) do
        local ok, loaded = pcall(ffi.load, name)
        if ok then
            lib = loaded
            return
        end
    end

    error("DanmakuFactory library not found. Run `xmake build lib-shared` first.")
end

--- 获取库版本
-- @return string 版本字符串
function M.version()
    load_library()
    local ver = lib.df_get_version()
    if ver == nil then return "unknown" end
    return ffi.string(ver)
end

--- 获取最近一次错误码
-- @return number 错误码
function M.last_error_code()
    load_library()
    return lib.df_get_last_error_code()
end

--- 获取最近一次错误消息
-- @return string 错误消息
function M.last_error_message()
    load_library()
    local msg = lib.df_get_last_error_full_message()
    if msg == nil then return "" end
    return ffi.string(msg)
end

--- 简易 JSON 编码器（仅支持基本类型）
local function encode_json(tbl)
    local function quote_string(value)
        return '"' .. value:gsub('[%z\1-\31\\"]', function(char)
            if char == '\\' then return '\\\\' end
            if char == '"' then return '\\"' end
            return string.format('\\u%04x', string.byte(char))
        end) .. '"'
    end

    local function encode_value(value)
        local kind = type(value)
        if kind == "string" then
            return quote_string(value)
        elseif kind == "number" then
            if value ~= value or value == math.huge or value == -math.huge then
                error("JSON numbers must be finite")
            end
            return tostring(value)
        elseif kind == "boolean" then
            return tostring(value)
        elseif kind == "table" then
            local count, maximum, is_array = 0, 0, true
            for key in pairs(value) do
                count = count + 1
                if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then
                    is_array = false
                else
                    maximum = math.max(maximum, key)
                end
            end
            local items = {}
            if is_array and count > 0 and maximum == count then
                for index = 1, count do
                    items[#items + 1] = encode_value(value[index])
                end
                return "[" .. table.concat(items, ",") .. "]"
            end
            for key, item in pairs(value) do
                items[#items + 1] = quote_string(tostring(key)) .. ":" .. encode_value(item)
            end
            return "{" .. table.concat(items, ",") .. "}"
        elseif value == nil then
            return "null"
        end
        error("Unsupported JSON value type: " .. kind)
    end
    return encode_value(tbl)
end

--- 执行弹幕格式转换
-- @param input_file string 输入文件路径
-- @param output_file string 输出文件路径
-- @param config table|nil 可选配置表
-- @param time_shift number|nil 时轴偏移量（秒）
-- @return boolean success 是否成功
-- @return string|nil error_message 错误消息（失败时）
function M.convert(input_file, output_file, config, time_shift)
    load_library()

    -- 路径分隔符统一
    input_file = input_file:gsub("\\", "/")
    output_file = output_file:gsub("\\", "/")

    local runtime_info = {
        input_files = {
            { path = input_file, time_shift = time_shift or 0 }
        },
        output_file = output_file,
    }

    if config then
        runtime_info.config = config
    end

    local json_str
    if json then
        json_str = json.encode(runtime_info)
    else
        json_str = encode_json(runtime_info)
    end

    local code = lib.df_run_from_json(json_str)

    if code ~= 0 then
        return false, M.last_error_message()
    end

    return true, nil
end

--- 直接传入 JSON 字符串执行转换（底层接口）
-- @param runtime_info_json string JSON 字符串
-- @return number 错误码
function M.convert_raw_json(runtime_info_json)
    load_library()
    return lib.df_run_from_json(runtime_info_json)
end

--- 初始化（可选，用于指定库路径）
-- @param path string 动态库路径
function M.init(path)
    load_library(path)
end

-- ============================================================================
-- 标准化句柄 API（推荐）
-- create → configure → run → destroy
-- ============================================================================

local Context = {}
Context.__index = Context

--- 检查返回码，非零时抛出错误（携带 TLS 错误消息）
local function check(code)
    if code ~= 0 then
        error(string.format("[%d] %s", code, M.last_error_message()), 3)
    end
end

--- 创建上下文（所有配置项初始化为默认值）
-- @return table Context 对象
function M.context()
    load_library()
    local ptr = lib.df_create()
    if ptr == nil then
        error("df_create() returned NULL (out of memory)", 2)
    end
    local self = setmetatable({ _ptr = ptr }, Context)
    -- __gc 在垃圾回收时自动调用 df_destroy
    ffi.gc(ptr, function(p)
        if p ~= nil then lib.df_destroy(p) end
    end)
    return self
end

--- 手动销毁上下文（幂等；通常由 GC 自动处理）
function Context:destroy()
    if self._ptr ~= nil then
        ffi.gc(self._ptr, nil) -- 取消 GC 回调，避免双重释放
        lib.df_destroy(self._ptr)
        self._ptr = nil
    end
end

-- --- 执行 ---

function Context:run()
    check(lib.df_run(self._ptr))
end

-- --- 批量配置加载 ---

function Context:load_config_json(json_str)
    check(lib.df_load_config_json(self._ptr, json_str))
end

function Context:load_config_file(path)
    -- 括号包裹 gsub 以截断其第二个返回值（替换次数），避免误传给 C 函数
    check(lib.df_load_config_file(self._ptr, (path:gsub("\\", "/"))))
end

-- --- 文件管理 ---

function Context:add_input_file(path, time_shift)
    check(lib.df_add_input_file(self._ptr, (path:gsub("\\", "/")), time_shift or 0))
end

function Context:clear_input_files()
    check(lib.df_clear_input_files(self._ptr))
end

function Context:set_output_file(path)
    -- 括号包裹 gsub 以截断其第二个返回值（替换次数），避免误传给 C 函数
    check(lib.df_set_output_file(self._ptr, (path:gsub("\\", "/"))))
end

-- --- 屏幕配置 ---

function Context:set_resolution(width, height)
    check(lib.df_set_resolution(self._ptr, width, height))
end
function Context:set_display_area(area)
    check(lib.df_set_display_area(self._ptr, area))
end
function Context:set_scroll_area(area)
    check(lib.df_set_scroll_area(self._ptr, area))
end
function Context:set_statistics_mode(mode)
    check(lib.df_set_statistics_mode(self._ptr, mode))
end

-- --- 弹幕配置 ---

function Context:set_density(density)
    check(lib.df_set_density(self._ptr, density))
end
function Context:set_scroll_duration(seconds)
    check(lib.df_set_scroll_duration(self._ptr, seconds))
end
function Context:set_fixed_duration(seconds)
    check(lib.df_set_fixed_duration(self._ptr, seconds))
end
function Context:set_show_username(show)
    check(lib.df_set_show_username(self._ptr, show))
end
function Context:set_block_mode(mode)
    check(lib.df_set_block_mode(self._ptr, mode))
end
function Context:set_keep_blocked(keep)
    check(lib.df_set_keep_blocked(self._ptr, keep))
end

-- --- 文本配置 ---

function Context:set_fontname(fontname)
    check(lib.df_set_fontname(self._ptr, fontname))
end
function Context:set_font_size(size)
    check(lib.df_set_font_size(self._ptr, size))
end
function Context:set_font_size_strict(strict)
    check(lib.df_set_font_size_strict(self._ptr, strict))
end
function Context:set_font_size_norm(norm)
    check(lib.df_set_font_size_norm(self._ptr, norm))
end
function Context:set_bold(bold)
    check(lib.df_set_bold(self._ptr, bold))
end
function Context:set_shadow(shadow)
    check(lib.df_set_shadow(self._ptr, shadow))
end
function Context:set_opacity(opacity)
    check(lib.df_set_opacity(self._ptr, opacity))
end
function Context:set_outline(outline)
    check(lib.df_set_outline(self._ptr, outline))
end
function Context:set_outline_blur(blur)
    check(lib.df_set_outline_blur(self._ptr, blur))
end
function Context:set_outline_opacity(opacity)
    check(lib.df_set_outline_opacity(self._ptr, opacity))
end
function Context:set_line_spacing(spacing)
    check(lib.df_set_line_spacing(self._ptr, spacing))
end

-- --- 消息框配置 ---

function Context:set_msgbox_show(show)
    check(lib.df_set_msgbox_show(self._ptr, show))
end
function Context:set_msgbox_size(width, height)
    check(lib.df_set_msgbox_size(self._ptr, width, height))
end
function Context:set_msgbox_pos(x, y)
    check(lib.df_set_msgbox_pos(self._ptr, x, y))
end
function Context:set_msgbox_font_size(size)
    check(lib.df_set_msgbox_font_size(self._ptr, size))
end
function Context:set_msgbox_duration(seconds)
    check(lib.df_set_msgbox_duration(self._ptr, seconds))
end
function Context:set_msgbox_gift_min_price(price)
    check(lib.df_set_msgbox_gift_min_price(self._ptr, price))
end

return M
