// DanmakuFactory C# 绑定
//
// 通过 P/Invoke 调用 DanmakuFactory 动态库。
//
// 前置条件:
//   1. 构建动态库: xmake build lib-shared
//   2. 将 DanmakuFactory.dll 复制到可执行文件目录（或系统 PATH）
//
// 用法:
//   using DanmakuFactory;
//   var df = new DfLibrary();
//   Console.WriteLine(df.Version);
//   df.Convert("input.xml", "output.ass");

using System;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace DanmakuFactory
{
    /// <summary>DanmakuFactory 库调用异常</summary>
    public class DfException : Exception
    {
        public int Code { get; }
        public DfException(int code, string message) : base($"[{code}] {message}")
        {
            Code = code;
        }
    }

    /// <summary>DanmakuFactory 动态库封装</summary>
    public sealed class DfLibrary
    {
        private const string LibName = "DanmakuFactory";

        #region P/Invoke 声明

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr df_get_version();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int df_run_from_json([MarshalAs(UnmanagedType.LPUTF8Str)] string runtimeInfoJson);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int df_get_last_error_code();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr df_get_last_error_message();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr df_get_last_error_file();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr df_get_last_error_function();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int df_get_last_error_line();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr df_get_last_error_full_message();

        // --- 句柄 API ---

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr df_create();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void df_destroy(IntPtr ctx);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_run(IntPtr ctx);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int df_load_config_json(IntPtr ctx, [MarshalAs(UnmanagedType.LPUTF8Str)] string jsonStr);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int df_load_config_file(IntPtr ctx, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int df_add_input_file(IntPtr ctx, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, float timeShift);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_clear_input_files(IntPtr ctx);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int df_set_output_file(IntPtr ctx, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

        // --- 屏幕配置 ---
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_resolution(IntPtr ctx, int width, int height);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_display_area(IntPtr ctx, float area);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_scroll_area(IntPtr ctx, float area);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_statistics_mode(IntPtr ctx, int mode);

        // --- 弹幕配置 ---
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_density(IntPtr ctx, int density);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_scroll_duration(IntPtr ctx, float seconds);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_fixed_duration(IntPtr ctx, float seconds);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_show_username(IntPtr ctx, [MarshalAs(UnmanagedType.I1)] bool show);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_block_mode(IntPtr ctx, int mode);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_keep_blocked(IntPtr ctx, [MarshalAs(UnmanagedType.I1)] bool keep);

        // --- 文本配置 ---
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int df_set_fontname(IntPtr ctx, [MarshalAs(UnmanagedType.LPUTF8Str)] string fontname);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_font_size(IntPtr ctx, int size);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_font_size_strict(IntPtr ctx, [MarshalAs(UnmanagedType.I1)] bool strict);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_font_size_norm(IntPtr ctx, [MarshalAs(UnmanagedType.I1)] bool norm);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_bold(IntPtr ctx, [MarshalAs(UnmanagedType.I1)] bool bold);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_shadow(IntPtr ctx, float shadow);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_opacity(IntPtr ctx, int opacity);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_outline(IntPtr ctx, float outline);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_outline_blur(IntPtr ctx, float blur);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_outline_opacity(IntPtr ctx, int opacity);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_line_spacing(IntPtr ctx, int spacing);

        // --- 消息框配置 ---
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_msgbox_show(IntPtr ctx, [MarshalAs(UnmanagedType.I1)] bool show);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_msgbox_size(IntPtr ctx, int width, int height);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_msgbox_pos(IntPtr ctx, int x, int y);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_msgbox_font_size(IntPtr ctx, int size);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_msgbox_duration(IntPtr ctx, float seconds);
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int df_set_msgbox_gift_min_price(IntPtr ctx, float price);

        #endregion

        #region 辅助方法

        private static string PtrToString(IntPtr ptr)
        {
            return ptr == IntPtr.Zero ? "" : Marshal.PtrToStringUTF8(ptr) ?? "";
        }

        #endregion

        #region 公开属性

        /// <summary>获取库版本字符串</summary>
        public string Version => PtrToString(df_get_version());

        /// <summary>获取最近一次错误码</summary>
        public int LastErrorCode => df_get_last_error_code();

        /// <summary>获取最近一次错误消息</summary>
        public string LastErrorMessage => PtrToString(df_get_last_error_full_message());

        #endregion

        #region 公开方法

        /// <summary>
        /// 执行弹幕格式转换
        /// </summary>
        /// <param name="inputFile">输入文件路径</param>
        /// <param name="outputFile">输出文件路径</param>
        /// <param name="config">可选配置对象（匿名对象或 Dictionary）</param>
        /// <param name="timeShift">时轴偏移量（秒）</param>
        /// <exception cref="DfException">转换失败时抛出</exception>
        public void Convert(string inputFile, string outputFile, object? config = null, float timeShift = 0f)
        {
            var runtimeInfo = new
            {
                input_files = new[]
                {
                    new { path = inputFile.Replace('\\', '/'), time_shift = timeShift }
                },
                output_file = outputFile.Replace('\\', '/'),
                config = config,
            };

            var json = JsonSerializer.Serialize(runtimeInfo, new JsonSerializerOptions
            {
                DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
                PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
            });

            int code = df_run_from_json(json);
            if (code != 0)
            {
                throw new DfException(code, LastErrorMessage);
            }
        }

        /// <summary>
        /// 直接传入 JSON 字符串执行转换（底层接口）
        /// </summary>
        /// <param name="runtimeInfoJson">完整的 runtime_info JSON 字符串</param>
        /// <returns>错误码（0 表示成功）</returns>
        public int ConvertRawJson(string runtimeInfoJson)
        {
            return df_run_from_json(runtimeInfoJson);
        }

        #endregion
    }

    /// <summary>
    /// DanmakuFactory 标准化句柄封装（推荐）
    ///
    /// 遵循 create → configure → run → destroy 生命周期，
    /// 实现 IDisposable，可用 using 语句自动释放资源。
    /// </summary>
    /// <example>
    /// using var ctx = new DfContext();
    /// ctx.AddInputFile("input.xml");
    /// ctx.SetOutputFile("output.ass");
    /// ctx.SetFontSize(25);
    /// ctx.Run();
    /// </example>
    public sealed class DfContext : IDisposable
    {
        private IntPtr _ctx;
        private bool _disposed;

        /// <summary>创建上下文，所有配置项初始化为默认值</summary>
        /// <exception cref="DfException">内存不足导致创建失败</exception>
        public DfContext()
        {
            _ctx = DfLibrary.df_create();
            if (_ctx == IntPtr.Zero)
            {
                throw new DfException(-14, "df_create() returned NULL (out of memory)");
            }
        }

        private void Check(int code)
        {
            if (code != 0)
            {
                throw new DfException(code, LastErrorMessage);
            }
        }

        private static string LastErrorMessage
        {
            get
            {
                IntPtr ptr = DfLibrary.df_get_last_error_full_message();
                return ptr == IntPtr.Zero ? "" : Marshal.PtrToStringUTF8(ptr) ?? "";
            }
        }

        private static string Norm(string path) => path.Replace('\\', '/');

        // --- 执行 ---

        /// <summary>验证当前配置并执行弹幕格式转换</summary>
        public void Run() => Check(DfLibrary.df_run(_ctx));

        // --- 批量配置加载 ---

        /// <summary>从 JSON 字符串一次性加载全部配置（合并语义）</summary>
        public void LoadConfigJson(string jsonStr) => Check(DfLibrary.df_load_config_json(_ctx, jsonStr));

        /// <summary>从 JSON 配置文件一次性加载全部配置</summary>
        public void LoadConfigFile(string path) => Check(DfLibrary.df_load_config_file(_ctx, Norm(path)));

        // --- 文件管理 ---

        /// <summary>添加一个输入文件（可多次调用）</summary>
        public void AddInputFile(string path, float timeShift = 0f)
            => Check(DfLibrary.df_add_input_file(_ctx, Norm(path), timeShift));

        /// <summary>清除已添加的全部输入文件</summary>
        public void ClearInputFiles() => Check(DfLibrary.df_clear_input_files(_ctx));

        /// <summary>设置输出文件路径（扩展名决定输出格式）</summary>
        public void SetOutputFile(string path) => Check(DfLibrary.df_set_output_file(_ctx, Norm(path)));

        // --- 屏幕配置 ---

        public void SetResolution(int width, int height) => Check(DfLibrary.df_set_resolution(_ctx, width, height));
        public void SetDisplayArea(float area) => Check(DfLibrary.df_set_display_area(_ctx, area));
        public void SetScrollArea(float area) => Check(DfLibrary.df_set_scroll_area(_ctx, area));
        public void SetStatisticsMode(int mode) => Check(DfLibrary.df_set_statistics_mode(_ctx, mode));

        // --- 弹幕配置 ---

        public void SetDensity(int density) => Check(DfLibrary.df_set_density(_ctx, density));
        public void SetScrollDuration(float seconds) => Check(DfLibrary.df_set_scroll_duration(_ctx, seconds));
        public void SetFixedDuration(float seconds) => Check(DfLibrary.df_set_fixed_duration(_ctx, seconds));
        public void SetShowUsername(bool show) => Check(DfLibrary.df_set_show_username(_ctx, show));
        public void SetBlockMode(int mode) => Check(DfLibrary.df_set_block_mode(_ctx, mode));
        public void SetKeepBlocked(bool keep) => Check(DfLibrary.df_set_keep_blocked(_ctx, keep));

        // --- 文本配置 ---

        public void SetFontname(string fontname) => Check(DfLibrary.df_set_fontname(_ctx, fontname));
        public void SetFontSize(int size) => Check(DfLibrary.df_set_font_size(_ctx, size));
        public void SetFontSizeStrict(bool strict) => Check(DfLibrary.df_set_font_size_strict(_ctx, strict));
        public void SetFontSizeNorm(bool norm) => Check(DfLibrary.df_set_font_size_norm(_ctx, norm));
        public void SetBold(bool bold) => Check(DfLibrary.df_set_bold(_ctx, bold));
        public void SetShadow(float shadow) => Check(DfLibrary.df_set_shadow(_ctx, shadow));
        public void SetOpacity(int opacity) => Check(DfLibrary.df_set_opacity(_ctx, opacity));
        public void SetOutline(float outline) => Check(DfLibrary.df_set_outline(_ctx, outline));
        public void SetOutlineBlur(float blur) => Check(DfLibrary.df_set_outline_blur(_ctx, blur));
        public void SetOutlineOpacity(int opacity) => Check(DfLibrary.df_set_outline_opacity(_ctx, opacity));
        public void SetLineSpacing(int spacing) => Check(DfLibrary.df_set_line_spacing(_ctx, spacing));

        // --- 消息框配置 ---

        public void SetMsgboxShow(bool show) => Check(DfLibrary.df_set_msgbox_show(_ctx, show));
        public void SetMsgboxSize(int width, int height) => Check(DfLibrary.df_set_msgbox_size(_ctx, width, height));
        public void SetMsgboxPos(int x, int y) => Check(DfLibrary.df_set_msgbox_pos(_ctx, x, y));
        public void SetMsgboxFontSize(int size) => Check(DfLibrary.df_set_msgbox_font_size(_ctx, size));
        public void SetMsgboxDuration(float seconds) => Check(DfLibrary.df_set_msgbox_duration(_ctx, seconds));
        public void SetMsgboxGiftMinPrice(float price) => Check(DfLibrary.df_set_msgbox_gift_min_price(_ctx, price));

        // --- IDisposable ---

        public void Dispose()
        {
            if (!_disposed)
            {
                if (_ctx != IntPtr.Zero)
                {
                    DfLibrary.df_destroy(_ctx);
                    _ctx = IntPtr.Zero;
                }
                _disposed = true;
            }
        }
    }
}
