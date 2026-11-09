--[[
DanmakuFactory Lua 使用示例

演示如何使用 DanmakuFactory Lua 绑定进行弹幕格式转换。

前置条件:
    1. 安装 LuaJIT
    2. 构建动态库: xmake build lib-shared

运行:
    luajit bindings/lua/example.lua
]]

package.path = package.path .. ";" .. debug.getinfo(1, "S").source:match("@?(.*[/\\])") .. "?.lua"

local df = require("danmakufactory")

-- 示例 1: 基础转换
print("=== 基础转换：XML → ASS ===")
print("库版本: " .. df.version())

local script_dir = debug.getinfo(1, "S").source:match("@?(.*[/\\])") or "./"
local input_xml = script_dir .. "../../tests/test\240\159\154\169.xml"
local output_ass = os.tmpname() .. ".ass"

local ok, err = df.convert(input_xml, output_ass)
if ok then
    local f = io.open(output_ass, "r")
    local content = f:read("*a")
    f:close()
    print(string.format("转换成功，输出 %d 字节", #content))
    print("前 100 字符: " .. content:sub(1, 100))
    os.remove(output_ass)
else
    print("转换失败: " .. err)
end
print()

-- 示例 2: 带配置的转换
print("=== 带配置的转换 ===")
local config = {
    screen = { resolution = {1920, 1080} },
    danmaku = { scroll_duration = 10.0, density = -1 },
    text = { font_size = 38, bold = true, opacity = 200 },
}

output_ass = os.tmpname() .. ".ass"
ok, err = df.convert(input_xml, output_ass, config)
if ok then
    print("带配置转换成功")
    os.remove(output_ass)
else
    print("转换失败: " .. err)
end
print()

-- 示例 3: 错误处理
print("=== 错误处理 ===")
ok, err = df.convert("nonexistent_file.xml", "output.ass")
if not ok then
    print("捕获错误: " .. err)
end
print()

-- 示例 4: 标准化句柄 API（推荐）
print("=== 句柄 API：create → configure → run → destroy ===")
local fixture = script_dir .. "../../tests/fixtures/sample.xml"
local handle_output = os.tmpname() .. ".ass"
local ctx = df.context()
ctx:add_input_file(fixture)
ctx:set_output_file(handle_output)
ctx:set_resolution(1920, 1080)
ctx:set_font_size(30)
ctx:set_scroll_duration(10.0)
ctx:set_bold(true)
ctx:run()
ctx:destroy()

local hf = io.open(handle_output, "r")
if hf then
    local content = hf:read("*a")
    hf:close()
    print(string.format("句柄 API 转换成功: %d bytes", #content))
    os.remove(handle_output)
else
    print("句柄 API 转换失败")
end
print()

print("所有示例执行完毕。")
