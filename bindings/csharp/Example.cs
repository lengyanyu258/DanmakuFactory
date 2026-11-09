// DanmakuFactory C# 使用示例
//
// 演示如何使用 DanmakuFactory C# 绑定进行弹幕格式转换。
//
// 前置条件:
//   1. 构建动态库: xmake build lib-shared
//   2. 将 DanmakuFactory.dll 复制到输出目录
//
// 运行:
//   dotnet run --project bindings/csharp -- example

using System;
using System.IO;
using DanmakuFactory;

class Example
{
    static void Main()
    {
        var df = new DfLibrary();

        // 示例 1: 基础转换
        Console.WriteLine("=== 基础转换：XML → ASS ===");
        Console.WriteLine($"库版本: {df.Version}");

        string repoRoot = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "..", "..", ".."));
        string inputXml = Path.Combine(repoRoot, "tests", "test\U0001F6A9.xml");
        string outputAss = Path.Combine(Path.GetTempPath(), "df_example_output.ass");

        try
        {
            df.Convert(inputXml, outputAss);
            long size = new FileInfo(outputAss).Length;
            Console.WriteLine($"转换成功，输出 {size} 字节");
            Console.WriteLine($"前 100 字符: {File.ReadAllText(outputAss).Substring(0, Math.Min(100, (int)size))}");
        }
        catch (DfException e)
        {
            Console.WriteLine($"转换失败: {e.Message}");
        }
        Console.WriteLine();

        // 示例 2: 带配置的转换
        Console.WriteLine("=== 带配置的转换 ===");
        var config = new
        {
            screen = new { resolution = new[] { 1920, 1080 } },
            danmaku = new { scroll_duration = 10.0, density = -1 },
            text = new { font_size = 38, bold = true, opacity = 200 },
        };

        try
        {
            df.Convert(inputXml, outputAss, config);
            Console.WriteLine("带配置转换成功");
        }
        catch (DfException e)
        {
            Console.WriteLine($"转换失败: {e.Message}");
        }
        Console.WriteLine();

        // 示例 3: 错误处理
        Console.WriteLine("=== 错误处理 ===");
        try
        {
            df.Convert("nonexistent_file.xml", "output.ass");
        }
        catch (DfException e)
        {
            Console.WriteLine($"捕获错误: [{e.Code}] {e.Message}");
        }
        Console.WriteLine();

        // 示例 4: 标准化句柄 API（推荐）
        Console.WriteLine("=== 句柄 API：create → configure → run → destroy ===");
        string fixtureXml = Path.Combine(repoRoot, "tests", "fixtures", "sample.xml");
        string handleOutput = Path.Combine(Path.GetTempPath(), "df_cs_handle.ass");
        try
        {
            // using 语句自动调用 Dispose -> df_destroy
            using (var ctx = new DfContext())
            {
                ctx.AddInputFile(fixtureXml);
                ctx.SetOutputFile(handleOutput);
                ctx.SetResolution(1920, 1080);
                ctx.SetFontSize(30);
                ctx.SetScrollDuration(10.0f);
                ctx.SetBold(true);
                ctx.Run();
            }
            Console.WriteLine($"句柄 API 转换成功: {new FileInfo(handleOutput).Length} bytes");
        }
        catch (DfException e)
        {
            Console.WriteLine($"句柄 API 转换失败: {e.Message}");
        }
        Console.WriteLine();

        // 清理
        if (File.Exists(outputAss)) File.Delete(outputAss);
        if (File.Exists(handleOutput)) File.Delete(handleOutput);
        Console.WriteLine("所有示例执行完毕。");
    }
}
