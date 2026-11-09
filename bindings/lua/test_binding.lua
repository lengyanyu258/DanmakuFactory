--[[
DanmakuFactory Lua 绑定测试

运行方式:
    cd DanmakuFactory
    luajit bindings/lua/test_binding.lua
]]

package.path = package.path .. ";" .. debug.getinfo(1, "S").source:match("@?(.*[/\\])") .. "?.lua"

local df = require("danmakufactory")

local test_xml = debug.getinfo(1, "S").source:match("@?(.*[/\\])") .. "../../tests/test\240\159\154\169.xml"
local fixture_xml = debug.getinfo(1, "S").source:match("@?(.*[/\\])") .. "../../tests/fixtures/sample.xml"

local function test_version()
    local ver = df.version()
    assert(ver and #ver > 0, "version should not be empty")
    assert(ver:find("%."), "version should contain dot")
    print("PASS: version = " .. ver)
end

local function test_convert()
    local output = os.tmpname() .. ".ass"
    local ok, err = df.convert(test_xml, output)
    assert(ok, "convert failed: " .. (err or "unknown"))

    local f = io.open(output, "r")
    assert(f, "output file not created")
    local content = f:read("*a")
    f:close()
    assert(#content > 0, "output file is empty")
    os.remove(output)
    print("PASS: xml -> ass, size = " .. #content .. " bytes")
end

local function test_convert_with_config()
    local output = os.tmpname() .. ".ass"
    local config = {
        text = { font_size = 25 },
        danmaku = { scroll_duration = 8.0 },
    }
    local ok, err = df.convert(test_xml, output, config)
    assert(ok, "convert with config failed: " .. (err or "unknown"))
    os.remove(output)
    print("PASS: convert with config")
end

local function test_error_path()
    local output = os.tmpname() .. ".ass"
    local ok, err = df.convert("/nonexistent/file.xml", output)
    assert(not ok, "should fail for nonexistent file")
    assert(err and #err > 0, "error message should not be empty")
    os.remove(output)
    print("PASS: error path handled")
end

local function test_invalid_json()
    local code = df.convert_raw_json("{invalid json!!!")
    assert(code ~= 0, "should return non-zero for invalid JSON")
    print("PASS: invalid json, code = " .. code)
end

-- ---- 标准化句柄 API 测试 ----

local function test_context_convert()
    local output = os.tmpname() .. ".ass"
    local ctx = df.context()
    ctx:add_input_file(fixture_xml)
    ctx:set_output_file(output)
    ctx:set_font_size(25)
    ctx:run()
    ctx:destroy()

    local f = io.open(output, "r")
    assert(f, "output file not created")
    local content = f:read("*a")
    f:close()
    assert(#content > 0, "output file is empty")
    os.remove(output)
    print("PASS: context convert, size = " .. #content .. " bytes")
end

local function test_context_setters()
    local output = os.tmpname() .. ".ass"
    local ctx = df.context()
    ctx:add_input_file(fixture_xml)
    ctx:set_output_file(output)
    -- 覆盖各分组 setter
    ctx:set_resolution(1280, 720)
    ctx:set_display_area(0.8)
    ctx:set_density(-1)
    ctx:set_scroll_duration(9.0)
    ctx:set_fixed_duration(4.0)
    ctx:set_show_username(true)
    ctx:set_fontname("Microsoft YaHei")
    ctx:set_font_size(30)
    ctx:set_bold(true)
    ctx:set_opacity(200)
    ctx:set_outline(1.0)
    ctx:set_line_spacing(2)
    ctx:set_msgbox_show(true)
    ctx:set_msgbox_size(400, 800)
    ctx:run()
    ctx:destroy()

    local f = io.open(output, "r")
    assert(f, "output file not created")
    f:close()
    os.remove(output)
    print("PASS: context setters (all groups)")
end

local function test_context_load_config_json()
    local output = os.tmpname() .. ".ass"
    local ctx = df.context()
    ctx:add_input_file(fixture_xml)
    ctx:set_output_file(output)
    ctx:load_config_json('{"text": {"font_size": 30}}')
    ctx:load_config_json('{"config": {"danmaku": {"scroll_duration": 7.0}}}')
    ctx:run()
    ctx:destroy()

    local f = io.open(output, "r")
    assert(f, "output file not created")
    f:close()
    os.remove(output)
    print("PASS: context load_config_json")
end

local function test_context_invalid_config()
    local ctx = df.context()
    local ok, err = pcall(function()
        ctx:load_config_json("{invalid json!!!")
    end)
    ctx:destroy()
    assert(not ok, "should raise error for invalid JSON")
    assert(err and #err > 0, "error message should not be empty")
    print("PASS: context invalid config")
end

-- ---- emoji 文件名测试（UTF-8 路径） ----
--
-- Windows 下 Lua 标准 io.open 按本地代码页（GBK）解析路径，无法创建/读取
-- emoji 文件名；而 C 库 df_fopen 走 UTF-8。因此测试侧需用 Win32 API 做
-- UTF-8 感知的文件复制与存在性/大小校验，才能与库的行为对齐。

local ffi = require("ffi")
local kernel32 = nil
if ffi.os == "Windows" then
    pcall(ffi.cdef, [[
        int MultiByteToWideChar(unsigned int CodePage, unsigned long dwFlags,
            const char *lpMultiByteStr, int cbMultiByte,
            wchar_t *lpWideCharStr, int cchWideChar);
        int CopyFileW(const wchar_t *lpExistingFileName,
            const wchar_t *lpNewFileName, int bFailIfExists);
        typedef struct {
            unsigned long dwFileAttributes;
            unsigned long long ftCreationTime;
            unsigned long long ftLastAccessTime;
            unsigned long long ftLastWriteTime;
            unsigned long nFileSizeHigh;
            unsigned long nFileSizeLow;
        } WIN32_FILE_ATTRIBUTE_DATA;
        int GetFileAttributesExW(const wchar_t *lpFileName, int fInfoLevelId,
            WIN32_FILE_ATTRIBUTE_DATA *lpFileInformation);
        int DeleteFileW(const wchar_t *lpFileName);
    ]])
    kernel32 = ffi.load("kernel32")
end

local CP_UTF8 = 65001

local function utf8_to_wide(s)
    local len = kernel32.MultiByteToWideChar(CP_UTF8, 0, s, -1, nil, 0)
    local wbuf = ffi.new("wchar_t[?]", len)
    kernel32.MultiByteToWideChar(CP_UTF8, 0, s, -1, wbuf, len)
    return wbuf
end

-- UTF-8 感知文件复制：Windows 用 CopyFileW，其他平台用标准 io
local function copy_file(src, dst)
    if kernel32 then
        local ret = kernel32.CopyFileW(utf8_to_wide(src), utf8_to_wide(dst), 0)
        assert(ret ~= 0, "CopyFileW failed for " .. dst)
    else
        local fin = assert(io.open(src, "rb"))
        local content = fin:read("*a")
        fin:close()
        local fout = assert(io.open(dst, "wb"))
        fout:write(content)
        fout:close()
    end
end

-- UTF-8 感知文件大小查询：不存在返回 nil
local function file_size(path)
    if kernel32 then
        local data = ffi.new("WIN32_FILE_ATTRIBUTE_DATA")
        local ret = kernel32.GetFileAttributesExW(utf8_to_wide(path), 0, data)
        if ret == 0 then
            return nil
        end
        return tonumber(data.nFileSizeLow) + tonumber(data.nFileSizeHigh) * 4294967296
    else
        local f = io.open(path, "rb")
        if not f then
            return nil
        end
        local size = f:seek("end")
        f:close()
        return size
    end
end

-- UTF-8 感知删除
local function remove_file(path)
    if kernel32 then
        kernel32.DeleteFileW(utf8_to_wide(path))
    else
        os.remove(path)
    end
end

local function test_emoji_filename()
    -- emoji 文件名转换：输入与输出均含 emoji，验证 UTF-8 路径支持
    local tmp_dir = os.tmpname()
    remove_file(tmp_dir) -- os.tmpname 会创建空文件，删除后用其名字作目录前缀
    local emoji_input = tmp_dir .. "_输入🚩.xml"
    local emoji_output = tmp_dir .. "_输出🎉.ass"
    copy_file(fixture_xml, emoji_input)

    -- JSON 一次性 API
    local ok, err = df.convert(emoji_input, emoji_output)
    assert(ok, "emoji convert failed: " .. (err or "unknown"))
    local size = file_size(emoji_output)
    assert(size, "emoji output not created")
    assert(size > 0, "emoji output is empty")

    -- 句柄 API 再验证一次
    local emoji_output2 = tmp_dir .. "_输出2🚀.ass"
    local ctx = df.context()
    ctx:add_input_file(emoji_input)
    ctx:set_output_file(emoji_output2)
    ctx:run()
    ctx:destroy()
    local size2 = file_size(emoji_output2)
    assert(size2, "emoji output2 not created")
    assert(size2 > 0, "emoji output2 is empty")

    remove_file(emoji_input)
    remove_file(emoji_output)
    remove_file(emoji_output2)
    print("PASS: emoji filename convert (input + output)")
end

-- 运行测试
print("Running DanmakuFactory Lua binding tests...")
print()
-- JSON 一次性 API
test_version()
test_convert()
test_convert_with_config()
test_error_path()
test_invalid_json()
-- 标准化句柄 API
test_context_convert()
test_context_setters()
test_context_load_config_json()
test_context_invalid_config()
-- emoji 文件名（UTF-8 路径）
test_emoji_filename()
print()
print("ALL TESTS PASSED")
