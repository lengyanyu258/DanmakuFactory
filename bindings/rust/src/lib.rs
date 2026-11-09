//! DanmakuFactory Rust 绑定
//!
//! 提供对 DanmakuFactory C 核心库的安全 Rust 封装。
//! 对应头文件: lib/include/danmakufactory.h
//!
//! 本 crate 暴露两套并行的 API：
//!
//! 1. 标准化句柄 API（推荐）—— create → configure → run → destroy 生命周期：
//!    ```no_run
//!    use danmakufactory_sys::Context;
//!
//!    let mut ctx = Context::new().expect("out of memory");
//!    ctx.add_input_file("input.xml", 0.0).unwrap();
//!    ctx.set_output_file("output.ass").unwrap();
//!    ctx.set_font_size(25).unwrap();
//!    ctx.run().unwrap();
//!    // ctx 在此处 Drop，自动调用 df_destroy
//!    ```
//!
//! 2. JSON 一次性 API（便捷）：
//!    ```no_run
//!    use danmakufactory_sys::run_from_json;
//!
//!    let json = r#"{"input_files": ["input.xml"], "output_file": "output.ass"}"#;
//!    run_from_json(json).unwrap();
//!    ```

use std::ffi::{c_char, c_int, CStr, CString};

/* ========================================================================
 * 错误码与错误类型
 * ======================================================================== */

/// 错误码（与 C 端 df_error_code_t 保持一致）
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DfErrorCode {
    Success = 0,
    Warning = 1,
    Error = -1,

    WarningInvalidConfig = 1000,

    ErrorInternal = -10,
    ErrorInternalNullParameter = -11,
    ErrorInvalidParameter = -12,
    ErrorInvalidValue = -13,
    ErrorInternalOutOfMemory = -14,

    ErrorInvalidJson = -100,

    ErrorInvalidConfig = -1000,
    ErrorFileNotFound = -1001,
    ErrorInvalidRuntimeInfo = -1002,
    ErrorInvalidFileFormat = -1003,
    ErrorFileWriteFailed = -1004,
    ErrorOutputNotSpecified = -1005,
}

impl DfErrorCode {
    pub fn from_raw(code: c_int) -> Self {
        match code {
            0 => Self::Success,
            1 => Self::Warning,
            -1 => Self::Error,
            1000 => Self::WarningInvalidConfig,
            -10 => Self::ErrorInternal,
            -11 => Self::ErrorInternalNullParameter,
            -12 => Self::ErrorInvalidParameter,
            -13 => Self::ErrorInvalidValue,
            -14 => Self::ErrorInternalOutOfMemory,
            -100 => Self::ErrorInvalidJson,
            -1000 => Self::ErrorInvalidConfig,
            -1001 => Self::ErrorFileNotFound,
            -1002 => Self::ErrorInvalidRuntimeInfo,
            -1003 => Self::ErrorInvalidFileFormat,
            -1004 => Self::ErrorFileWriteFailed,
            -1005 => Self::ErrorOutputNotSpecified,
            other => {
                eprintln!("WARNING: unknown error code {other}, treating as generic error");
                Self::Error
            }
        }
    }

    pub fn is_success(self) -> bool {
        (self as i32) >= 0
    }
}

/// 库调用错误，携带错误码与来自 TLS 的可读消息
#[derive(Debug, Clone)]
pub struct DfError {
    pub code: DfErrorCode,
    pub message: String,
}

impl std::fmt::Display for DfError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "[{:?}] {}", self.code, self.message)
    }
}

impl std::error::Error for DfError {}

/// 便捷 Result 别名
pub type DfResult<T = ()> = Result<T, DfError>;

/* ========================================================================
 * FFI 声明
 * ======================================================================== */

/// 不透明上下文句柄（对应 C 端 df_context_t）
#[repr(C)]
pub struct df_context_t {
    _opaque: [u8; 0],
}

extern "C" {
    // --- 基础查询 ---
    fn df_get_version() -> *const c_char;

    // --- 错误查询（线程局部） ---
    fn df_get_last_error_code() -> c_int;
    fn df_get_last_error_message() -> *const c_char;
    fn df_get_last_error_file() -> *const c_char;
    fn df_get_last_error_function() -> *const c_char;
    fn df_get_last_error_line() -> c_int;
    fn df_get_last_error_full_message() -> *const c_char;

    // --- JSON 一次性 API ---
    fn df_run_from_json(runtime_info_json_str: *const c_char) -> c_int;

    // --- 句柄生命周期 ---
    fn df_create() -> *mut df_context_t;
    fn df_destroy(ctx: *mut df_context_t);
    fn df_run(ctx: *mut df_context_t) -> c_int;

    // --- 批量配置加载 ---
    fn df_load_config_json(ctx: *mut df_context_t, json_str: *const c_char) -> c_int;
    fn df_load_config_file(ctx: *mut df_context_t, path: *const c_char) -> c_int;

    // --- 文件管理 ---
    fn df_add_input_file(ctx: *mut df_context_t, path: *const c_char, time_shift: f32) -> c_int;
    fn df_clear_input_files(ctx: *mut df_context_t) -> c_int;
    fn df_set_output_file(ctx: *mut df_context_t, path: *const c_char) -> c_int;

    // --- 屏幕配置 ---
    fn df_set_resolution(ctx: *mut df_context_t, width: c_int, height: c_int) -> c_int;
    fn df_set_display_area(ctx: *mut df_context_t, area: f32) -> c_int;
    fn df_set_scroll_area(ctx: *mut df_context_t, area: f32) -> c_int;
    fn df_set_statistics_mode(ctx: *mut df_context_t, mode: c_int) -> c_int;

    // --- 弹幕配置 ---
    fn df_set_density(ctx: *mut df_context_t, density: c_int) -> c_int;
    fn df_set_scroll_duration(ctx: *mut df_context_t, seconds: f32) -> c_int;
    fn df_set_fixed_duration(ctx: *mut df_context_t, seconds: f32) -> c_int;
    fn df_set_show_username(ctx: *mut df_context_t, show: bool) -> c_int;
    fn df_set_block_mode(ctx: *mut df_context_t, mode: c_int) -> c_int;
    fn df_set_keep_blocked(ctx: *mut df_context_t, keep: bool) -> c_int;

    // --- 文本配置 ---
    fn df_set_fontname(ctx: *mut df_context_t, fontname: *const c_char) -> c_int;
    fn df_set_font_size(ctx: *mut df_context_t, size: c_int) -> c_int;
    fn df_set_font_size_strict(ctx: *mut df_context_t, strict: bool) -> c_int;
    fn df_set_font_size_norm(ctx: *mut df_context_t, norm: bool) -> c_int;
    fn df_set_bold(ctx: *mut df_context_t, bold: bool) -> c_int;
    fn df_set_shadow(ctx: *mut df_context_t, shadow: f32) -> c_int;
    fn df_set_opacity(ctx: *mut df_context_t, opacity: c_int) -> c_int;
    fn df_set_outline(ctx: *mut df_context_t, outline: f32) -> c_int;
    fn df_set_outline_blur(ctx: *mut df_context_t, blur: f32) -> c_int;
    fn df_set_outline_opacity(ctx: *mut df_context_t, opacity: c_int) -> c_int;
    fn df_set_line_spacing(ctx: *mut df_context_t, spacing: c_int) -> c_int;

    // --- 消息框配置 ---
    fn df_set_msgbox_show(ctx: *mut df_context_t, show: bool) -> c_int;
    fn df_set_msgbox_size(ctx: *mut df_context_t, width: c_int, height: c_int) -> c_int;
    fn df_set_msgbox_pos(ctx: *mut df_context_t, x: c_int, y: c_int) -> c_int;
    fn df_set_msgbox_font_size(ctx: *mut df_context_t, size: c_int) -> c_int;
    fn df_set_msgbox_duration(ctx: *mut df_context_t, seconds: f32) -> c_int;
    fn df_set_msgbox_gift_min_price(ctx: *mut df_context_t, price: f32) -> c_int;
}

/* ========================================================================
 * 自由函数：版本 / 错误查询 / JSON 一次性 API
 * ======================================================================== */

unsafe fn ptr_to_string(ptr: *const c_char) -> String {
    if ptr.is_null() {
        String::new()
    } else {
        CStr::from_ptr(ptr)
            .to_str()
            .unwrap_or("(invalid UTF-8)")
            .to_owned()
    }
}

/// 获取库版本字符串（如 "2.0.0"）
pub fn get_version() -> &'static str {
    unsafe {
        let ptr = df_get_version();
        if ptr.is_null() {
            return "unknown";
        }
        CStr::from_ptr(ptr).to_str().unwrap_or("unknown")
    }
}

/// 获取最近一次错误码
pub fn last_error_code() -> DfErrorCode {
    DfErrorCode::from_raw(unsafe { df_get_last_error_code() })
}

/// 获取最近一次错误消息
pub fn last_error_message() -> String {
    unsafe { ptr_to_string(df_get_last_error_message()) }
}

/// 获取最近一次错误的文件名
pub fn last_error_file() -> String {
    unsafe { ptr_to_string(df_get_last_error_file()) }
}

/// 获取最近一次错误的函数名
pub fn last_error_function() -> String {
    unsafe { ptr_to_string(df_get_last_error_function()) }
}

/// 获取最近一次错误的行号
pub fn last_error_line() -> i32 {
    unsafe { df_get_last_error_line() }
}

/// 获取最近一次错误的完整描述
pub fn last_error_full_message() -> String {
    unsafe { ptr_to_string(df_get_last_error_full_message()) }
}

/// 从当前 TLS 状态构造一个 DfError
fn capture_error(code: c_int) -> DfError {
    DfError {
        code: DfErrorCode::from_raw(code),
        message: last_error_full_message(),
    }
}

/// JSON 一次性 API：从 JSON 字符串运行转换（无需管理句柄）
pub fn run_from_json(json: &str) -> DfResult {
    let c_json = CString::new(json).map_err(|_| DfError {
        code: DfErrorCode::ErrorInvalidJson,
        message: "JSON contains interior NUL byte".to_string(),
    })?;

    let raw = unsafe { df_run_from_json(c_json.as_ptr()) };
    if DfErrorCode::from_raw(raw).is_success() {
        Ok(())
    } else {
        Err(capture_error(raw))
    }
}

/* ========================================================================
 * 标准化句柄 API
 * ======================================================================== */

/// DanmakuFactory 上下文句柄（推荐）
///
/// 遵循 create → configure → run → destroy 生命周期。
/// 实现 [`Drop`]，离开作用域时自动调用 `df_destroy` 释放资源。
///
/// 线程安全性：不同 `Context` 可在不同线程独立使用；同一 `Context`
/// 不得跨线程并发使用（与 libcurl/sqlite3 句柄约定一致）。
pub struct Context {
    ptr: *mut df_context_t,
}

// Context 内部无 Rust 可见的可变共享状态，C 层保证不同句柄相互隔离。
unsafe impl Send for Context {}

impl Context {
    /// 创建上下文，所有配置项初始化为默认值
    ///
    /// 内存不足时返回 None。
    pub fn new() -> Option<Self> {
        let ptr = unsafe { df_create() };
        if ptr.is_null() {
            None
        } else {
            Some(Self { ptr })
        }
    }

    /// 检查返回码，非成功时捕获 TLS 错误并返回 Err
    fn check(&self, code: c_int) -> DfResult {
        if DfErrorCode::from_raw(code).is_success() {
            Ok(())
        } else {
            Err(capture_error(code))
        }
    }

    /// 验证当前配置并执行弹幕格式转换
    pub fn run(&mut self) -> DfResult {
        self.check(unsafe { df_run(self.ptr) })
    }

    // --- 批量配置加载 ---

    /// 从 JSON 字符串一次性加载全部配置（合并语义）
    pub fn load_config_json(&mut self, json_str: &str) -> DfResult {
        let c = CString::new(json_str).map_err(|_| DfError {
            code: DfErrorCode::ErrorInvalidJson,
            message: "config JSON contains interior NUL byte".to_string(),
        })?;
        self.check(unsafe { df_load_config_json(self.ptr, c.as_ptr()) })
    }

    /// 从 JSON 配置文件一次性加载全部配置
    pub fn load_config_file(&mut self, path: &str) -> DfResult {
        let c = to_cstring(path)?;
        self.check(unsafe { df_load_config_file(self.ptr, c.as_ptr()) })
    }

    // --- 文件管理 ---

    /// 添加一个输入文件（可多次调用）
    pub fn add_input_file(&mut self, path: &str, time_shift: f32) -> DfResult {
        let c = to_cstring(path)?;
        self.check(unsafe { df_add_input_file(self.ptr, c.as_ptr(), time_shift) })
    }

    /// 清除已添加的全部输入文件
    pub fn clear_input_files(&mut self) -> DfResult {
        self.check(unsafe { df_clear_input_files(self.ptr) })
    }

    /// 设置输出文件路径（扩展名决定输出格式）
    pub fn set_output_file(&mut self, path: &str) -> DfResult {
        let c = to_cstring(path)?;
        self.check(unsafe { df_set_output_file(self.ptr, c.as_ptr()) })
    }

    // --- 屏幕配置 ---

    pub fn set_resolution(&mut self, width: i32, height: i32) -> DfResult {
        self.check(unsafe { df_set_resolution(self.ptr, width, height) })
    }
    pub fn set_display_area(&mut self, area: f32) -> DfResult {
        self.check(unsafe { df_set_display_area(self.ptr, area) })
    }
    pub fn set_scroll_area(&mut self, area: f32) -> DfResult {
        self.check(unsafe { df_set_scroll_area(self.ptr, area) })
    }
    pub fn set_statistics_mode(&mut self, mode: i32) -> DfResult {
        self.check(unsafe { df_set_statistics_mode(self.ptr, mode) })
    }

    // --- 弹幕配置 ---

    pub fn set_density(&mut self, density: i32) -> DfResult {
        self.check(unsafe { df_set_density(self.ptr, density) })
    }
    pub fn set_scroll_duration(&mut self, seconds: f32) -> DfResult {
        self.check(unsafe { df_set_scroll_duration(self.ptr, seconds) })
    }
    pub fn set_fixed_duration(&mut self, seconds: f32) -> DfResult {
        self.check(unsafe { df_set_fixed_duration(self.ptr, seconds) })
    }
    pub fn set_show_username(&mut self, show: bool) -> DfResult {
        self.check(unsafe { df_set_show_username(self.ptr, show) })
    }
    pub fn set_block_mode(&mut self, mode: i32) -> DfResult {
        self.check(unsafe { df_set_block_mode(self.ptr, mode) })
    }
    pub fn set_keep_blocked(&mut self, keep: bool) -> DfResult {
        self.check(unsafe { df_set_keep_blocked(self.ptr, keep) })
    }

    // --- 文本配置 ---

    pub fn set_fontname(&mut self, fontname: &str) -> DfResult {
        let c = to_cstring(fontname)?;
        self.check(unsafe { df_set_fontname(self.ptr, c.as_ptr()) })
    }
    pub fn set_font_size(&mut self, size: i32) -> DfResult {
        self.check(unsafe { df_set_font_size(self.ptr, size) })
    }
    pub fn set_font_size_strict(&mut self, strict: bool) -> DfResult {
        self.check(unsafe { df_set_font_size_strict(self.ptr, strict) })
    }
    pub fn set_font_size_norm(&mut self, norm: bool) -> DfResult {
        self.check(unsafe { df_set_font_size_norm(self.ptr, norm) })
    }
    pub fn set_bold(&mut self, bold: bool) -> DfResult {
        self.check(unsafe { df_set_bold(self.ptr, bold) })
    }
    pub fn set_shadow(&mut self, shadow: f32) -> DfResult {
        self.check(unsafe { df_set_shadow(self.ptr, shadow) })
    }
    pub fn set_opacity(&mut self, opacity: i32) -> DfResult {
        self.check(unsafe { df_set_opacity(self.ptr, opacity) })
    }
    pub fn set_outline(&mut self, outline: f32) -> DfResult {
        self.check(unsafe { df_set_outline(self.ptr, outline) })
    }
    pub fn set_outline_blur(&mut self, blur: f32) -> DfResult {
        self.check(unsafe { df_set_outline_blur(self.ptr, blur) })
    }
    pub fn set_outline_opacity(&mut self, opacity: i32) -> DfResult {
        self.check(unsafe { df_set_outline_opacity(self.ptr, opacity) })
    }
    pub fn set_line_spacing(&mut self, spacing: i32) -> DfResult {
        self.check(unsafe { df_set_line_spacing(self.ptr, spacing) })
    }

    // --- 消息框配置 ---

    pub fn set_msgbox_show(&mut self, show: bool) -> DfResult {
        self.check(unsafe { df_set_msgbox_show(self.ptr, show) })
    }
    pub fn set_msgbox_size(&mut self, width: i32, height: i32) -> DfResult {
        self.check(unsafe { df_set_msgbox_size(self.ptr, width, height) })
    }
    pub fn set_msgbox_pos(&mut self, x: i32, y: i32) -> DfResult {
        self.check(unsafe { df_set_msgbox_pos(self.ptr, x, y) })
    }
    pub fn set_msgbox_font_size(&mut self, size: i32) -> DfResult {
        self.check(unsafe { df_set_msgbox_font_size(self.ptr, size) })
    }
    pub fn set_msgbox_duration(&mut self, seconds: f32) -> DfResult {
        self.check(unsafe { df_set_msgbox_duration(self.ptr, seconds) })
    }
    pub fn set_msgbox_gift_min_price(&mut self, price: f32) -> DfResult {
        self.check(unsafe { df_set_msgbox_gift_min_price(self.ptr, price) })
    }
}

impl Drop for Context {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { df_destroy(self.ptr) };
            self.ptr = std::ptr::null_mut();
        }
    }
}

/// 将 &str 转为 CString，路径中的反斜杠统一为正斜杠
fn to_cstring(s: &str) -> Result<CString, DfError> {
    let normalized = s.replace('\\', "/");
    CString::new(normalized).map_err(|_| DfError {
        code: DfErrorCode::ErrorInvalidParameter,
        message: "string contains interior NUL byte".to_string(),
    })
}

/* ========================================================================
 * 控制台 UTF-8 支持（Windows）
 * ======================================================================== */

/// 将 Windows 控制台输入/输出代码页切换为 UTF-8 (65001)
///
/// 用于让 CLI/GUI 正确打印包含 emoji 等非 ASCII 字符的文件名与日志。
/// 非 Windows 平台为空操作（终端通常已是 UTF-8）。
///
/// 应在程序启动尽早调用（main 开头）。
pub fn enable_console_utf8() {
    #[cfg(target_os = "windows")]
    {
        extern "system" {
            fn SetConsoleOutputCP(codepage: u32) -> i32;
            fn SetConsoleCP(codepage: u32) -> i32;
        }
        const CP_UTF8: u32 = 65001;
        unsafe {
            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);
        }
    }
}

