// DanmakuFactory C# 绑定测试
//
// 运行方式:
//   cd DanmakuFactory
//   dotnet run --project bindings/csharp
//   # 或编译后直接运行:
//   csc /out:test.exe bindings/csharp/DanmakuFactory.cs bindings/csharp/TestBinding.cs
//   test.exe

using System;
using System.IO;
using System.Text;
using DanmakuFactory;

class TestBinding
{
    /// <summary>向上查找仓库根目录（以包含 xmake.lua 为标志），兼容不同构建输出层级</summary>
    static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "xmake.lua")))
            {
                return dir.FullName;
            }
            dir = dir.Parent;
        }
        throw new DirectoryNotFoundException("Cannot locate repo root (xmake.lua not found upward from base directory)");
    }

    static string RepoRoot = FindRepoRoot();
    static string TestXml => Path.Combine(RepoRoot, "tests", "test\U0001F6A9.xml");
    static string FixtureXml => Path.Combine(RepoRoot, "tests", "fixtures", "sample.xml");

    static void TestVersion()
    {
        var df = new DfLibrary();
        string version = df.Version;
        Assert(!string.IsNullOrEmpty(version), "version should not be empty");
        Assert(version.Contains('.'), $"version should contain dot: {version}");
        Console.WriteLine($"PASS: version = {version}");
    }

    static void TestConvert()
    {
        var df = new DfLibrary();
        string output = Path.Combine(Path.GetTempPath(), $"df_test_{Guid.NewGuid()}.ass");

        try
        {
            df.Convert(TestXml, output);
            Assert(File.Exists(output), "output file not generated");
            long size = new FileInfo(output).Length;
            Assert(size > 0, "output file is empty");
            Console.WriteLine($"PASS: xml -> ass, size = {size} bytes");
        }
        finally
        {
            if (File.Exists(output)) File.Delete(output);
        }
    }

    static void TestConvertWithConfig()
    {
        var df = new DfLibrary();
        string output = Path.Combine(Path.GetTempPath(), $"df_test_{Guid.NewGuid()}.ass");

        try
        {
            var config = new { text = new { font_size = 25 }, danmaku = new { scroll_duration = 8.0 } };
            df.Convert(TestXml, output, config);
            Assert(File.Exists(output), "output file not generated");
            Console.WriteLine($"PASS: convert with config");
        }
        finally
        {
            if (File.Exists(output)) File.Delete(output);
        }
    }

    static void TestErrorPath()
    {
        var df = new DfLibrary();
        string output = Path.Combine(Path.GetTempPath(), $"df_test_{Guid.NewGuid()}.ass");

        try
        {
            df.Convert("/nonexistent/file.xml", output);
            Assert(false, "should have thrown DfException");
        }
        catch (DfException e)
        {
            Assert(e.Code != 0, "error code should be non-zero");
            Assert(!string.IsNullOrEmpty(e.Message), "error message should not be empty");
            Console.WriteLine($"PASS: error path, code = {e.Code}");
        }
        finally
        {
            if (File.Exists(output)) File.Delete(output);
        }
    }

    static void TestInvalidJson()
    {
        var df = new DfLibrary();
        int code = df.ConvertRawJson("{invalid json!!!");
        Assert(code != 0, "should return non-zero for invalid JSON");
        Console.WriteLine($"PASS: invalid json, code = {code}");
    }

    // ---- 标准化句柄 API 测试 ----

    static void TestContextConvert()
    {
        Assert(File.Exists(FixtureXml), $"fixture missing: {FixtureXml}");
        string output = Path.Combine(Path.GetTempPath(), $"df_ctx_{Guid.NewGuid()}.ass");
        try
        {
            using (var ctx = new DfContext())
            {
                ctx.AddInputFile(FixtureXml);
                ctx.SetOutputFile(output);
                ctx.SetFontSize(25);
                ctx.Run();
            }
            Assert(File.Exists(output), "output file not generated");
            Assert(new FileInfo(output).Length > 0, "output file is empty");
            Console.WriteLine($"PASS: context convert, size = {new FileInfo(output).Length} bytes");
        }
        finally { if (File.Exists(output)) File.Delete(output); }
    }

    static void TestContextSetters()
    {
        string output = Path.Combine(Path.GetTempPath(), $"df_ctx_setters_{Guid.NewGuid()}.ass");
        try
        {
            using (var ctx = new DfContext())
            {
                ctx.AddInputFile(FixtureXml);
                ctx.SetOutputFile(output);
                // 覆盖各分组 setter
                ctx.SetResolution(1280, 720);
                ctx.SetDisplayArea(0.8f);
                ctx.SetDensity(-1);
                ctx.SetScrollDuration(9.0f);
                ctx.SetFixedDuration(4.0f);
                ctx.SetShowUsername(true);
                ctx.SetFontname("Microsoft YaHei");
                ctx.SetFontSize(30);
                ctx.SetBold(true);
                ctx.SetOpacity(200);
                ctx.SetOutline(1.0f);
                ctx.SetLineSpacing(2);
                ctx.SetMsgboxShow(true);
                ctx.SetMsgboxSize(400, 800);
                ctx.Run();
            }
            Assert(File.Exists(output), "output file not generated");
            Console.WriteLine("PASS: context setters (all groups)");
        }
        finally { if (File.Exists(output)) File.Delete(output); }
    }

    static void TestContextLoadConfigJson()
    {
        string output = Path.Combine(Path.GetTempPath(), $"df_ctx_cfg_{Guid.NewGuid()}.ass");
        try
        {
            using (var ctx = new DfContext())
            {
                ctx.AddInputFile(FixtureXml);
                ctx.SetOutputFile(output);
                ctx.LoadConfigJson("{\"text\": {\"font_size\": 30}}");
                ctx.LoadConfigJson("{\"config\": {\"danmaku\": {\"scroll_duration\": 7.0}}}");
                ctx.Run();
            }
            Assert(File.Exists(output), "output file not generated");
            Console.WriteLine("PASS: context load_config_json");
        }
        finally { if (File.Exists(output)) File.Delete(output); }
    }

    static void TestContextInvalidConfig()
    {
        using (var ctx = new DfContext())
        {
            try
            {
                ctx.LoadConfigJson("{invalid json!!!");
                Assert(false, "should have thrown DfException");
            }
            catch (DfException e)
            {
                Assert(e.Code != 0, "error code should be non-zero");
                Assert(!string.IsNullOrEmpty(e.Message), "error message should not be empty");
                Console.WriteLine($"PASS: context invalid config, code = {e.Code}");
            }
        }
    }

    static void TestEmojiFilename()
    {
        // emoji 文件名转换：输入与输出均含 emoji，验证 UTF-8 路径支持
        Assert(File.Exists(FixtureXml), $"fixture missing: {FixtureXml}");

        string tmpDir = Path.Combine(Path.GetTempPath(), $"df_cs_emoji_{Guid.NewGuid():N}");
        Directory.CreateDirectory(tmpDir);
        try
        {
            string emojiInput = Path.Combine(tmpDir, "输入🚩.xml");
            string emojiOutput = Path.Combine(tmpDir, "输出🎉.ass");
            File.Copy(FixtureXml, emojiInput);

            // JSON 一次性 API
            var df = new DfLibrary();
            df.Convert(emojiInput, emojiOutput);
            Assert(File.Exists(emojiOutput), "emoji output not generated");
            Assert(new FileInfo(emojiOutput).Length > 0, "emoji output is empty");

            // 句柄 API 再验证一次
            string emojiOutput2 = Path.Combine(tmpDir, "输出2🚀.ass");
            using (var ctx = new DfContext())
            {
                ctx.AddInputFile(emojiInput);
                ctx.SetOutputFile(emojiOutput2);
                ctx.Run();
            }
            Assert(File.Exists(emojiOutput2), "emoji output2 not generated");
            Assert(new FileInfo(emojiOutput2).Length > 0, "emoji output2 is empty");

            Console.WriteLine("PASS: emoji filename convert (input + output)");
        }
        finally
        {
            if (Directory.Exists(tmpDir)) Directory.Delete(tmpDir, true);
        }
    }

    static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception($"ASSERTION FAILED: {message}");
        }
    }

    static void Main()
    {
        // 启用 UTF-8 控制台，避免打印 emoji/非 ASCII 时乱码或异常
        try
        {
            Console.OutputEncoding = Encoding.UTF8;
        }
        catch { /* 非控制台环境忽略 */ }

        Console.WriteLine("Running DanmakuFactory C# binding tests...");
        Console.WriteLine($"Test XML exists: {File.Exists(TestXml)}");
        Console.WriteLine();

        TestVersion();
        TestConvert();
        TestConvertWithConfig();
        TestErrorPath();
        TestInvalidJson();

        // 标准化句柄 API
        TestContextConvert();
        TestContextSetters();
        TestContextLoadConfigJson();
        TestContextInvalidConfig();

        // emoji 文件名（UTF-8 路径）
        TestEmojiFilename();

        Console.WriteLine();
        Console.WriteLine("ALL TESTS PASSED");
    }
}
