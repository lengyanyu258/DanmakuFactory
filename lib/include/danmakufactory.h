/* MIT License
 *
 * Copyright (c) 2025 Shawn Sun <lengyanyu258@outlook.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef DANMAKU_FACTORY_H_
#define DANMAKU_FACTORY_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
    #if defined(DANMAKU_FACTORY_STATIC)
        #define DF_API
    #elif defined(DANMAKU_FACTORY_EXPORTS)
        #define DF_API __declspec(dllexport)
    #else
        #define DF_API __declspec(dllimport)
    #endif
#else
    #define DF_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* ========================================================================
 * 线程安全性契约
 *
 * 本库的全部公开 API 满足以下多线程保证：
 *
 * 1. df_run_from_json() 可从任意数量的线程并发调用，无需任何外部同步。
 *    每次调用内部使用独立的栈/堆状态，错误信息存储在线程局部存储（TLS）中。
 *
 * 2. 句柄 API（df_create / df_set_* / df_run / df_destroy）：
 *    - 不同 df_context_t 句柄可在不同线程中独立使用，完全线程安全。
 *    - 同一句柄不得在多线程间并发使用（与 libcurl easy handle、
 *      sqlite3* 句柄的约定一致）。若需并发，请为每个线程创建独立句柄。
 *
 * 3. 错误查询函数族（df_get_last_error_*）读取调用线程自身的 TLS 状态，
 *    任意时刻、任意线程调用均安全。
 *
 * 4. df_get_version() 返回编译期常量字符串，永远线程安全。
 *
 * 5. 库内部不持有任何跨调用的全局可变状态（转换路径上无 static 变量），
 *    无需调用初始化/反初始化函数，加载即可用。
 * ======================================================================== */

/* ========================================================================
 * 错误码
 * ======================================================================== */

typedef enum
{
    DF_SUCCESS = 0,
    DF_WARNING = 1,
    DF_ERROR   = -1,

    DF_WARNING_INVALID_CONFIG = 1000,

    DF_ERROR_INTERNAL                = -10,
    DF_ERROR_INTERNAL_NULL_PARAMETER = -11,
    DF_ERROR_INVALID_PARAMETER       = -12,
    DF_ERROR_INVALID_VALUE           = -13,
    DF_ERROR_INTERNAL_OUT_OF_MEMORY  = -14,

    DF_ERROR_INVALID_JSON = -100,

    DF_ERROR_INVALID_CONFIG = -DF_WARNING_INVALID_CONFIG,
    DF_ERROR_FILE_NOT_FOUND = -1001,

    DF_ERROR_INVALID_RUNTIME_INFO = -1002,

    DF_ERROR_INVALID_FILE_FORMAT  = -1003,
    DF_ERROR_FILE_WRITE_FAILED    = -1004,
    DF_ERROR_OUTPUT_NOT_SPECIFIED = -1005,
} df_error_code_t;

/* ========================================================================
 * 基础查询 API
 * ======================================================================== */

/**
 * @brief 获取库版本字符串（如 "2.0.0"），可用于 FFI 探活
 * @return 静态字符串，调用方不得释放
 */
DF_API const char *df_get_version(void);

/* ========================================================================
 * 错误查询 API（线程局部）
 *
 * FFI 约定：返回的 const char * 指向库内线程局部缓冲区，
 * 调用方不得释放；其内容在同一线程下一次调用本库 API 后失效。
 * ======================================================================== */

DF_API df_error_code_t df_get_last_error_code(void);
DF_API const char     *df_get_last_error_message(void);
DF_API const char     *df_get_last_error_file(void);
DF_API const char     *df_get_last_error_function(void);
DF_API int             df_get_last_error_line(void);
DF_API const char     *df_get_last_error_full_message(void);

/* ========================================================================
 * 标准化句柄 API（推荐）
 *
 * 遵循业界标准的 create → configure → run → destroy 生命周期，
 * 与 libcurl (curl_easy_init/setopt/perform/cleanup)、sqlite3 等
 * 知名 C 库的对外接口范式一致。
 *
 * 典型用法（Python ctypes 伪代码）：
 *     ctx = lib.df_create()
 *     lib.df_load_config_file(ctx, b"DanmakuFactoryConfig.json")  # 批量加载
 *     lib.df_add_input_file(ctx, b"input.xml", 0.0)
 *     lib.df_set_output_file(ctx, b"output.ass")
 *     lib.df_set_font_size(ctx, 25)  # 可继续用单项 setter 微调
 *     code = lib.df_run(ctx)
 *     lib.df_destroy(ctx)
 * ======================================================================== */

/** 不透明上下文句柄 */
typedef struct df_context_s df_context_t;

/* --- 生命周期 --- */

/**
 * @brief 创建上下文，所有配置项初始化为默认值
 * @return 成功返回非 NULL 句柄；内存不足时返回 NULL
 */
DF_API df_context_t *df_create(void);

/**
 * @brief 销毁上下文并释放所有内部资源
 * @param ctx 由 df_create 返回的句柄；传入 NULL 时安全地无操作
 */
DF_API void df_destroy(df_context_t *ctx);

/* --- 执行 --- */

/**
 * @brief 验证当前配置并执行弹幕格式转换
 * @param ctx 已配置好的上下文句柄
 * @return DF_SUCCESS 或相应错误码
 */
DF_API df_error_code_t df_run(df_context_t *ctx);

/* --- 批量配置加载 --- */

/**
 * @brief 从 JSON 字符串一次性加载全部配置（合并语义，仅覆盖 JSON 中出现的字段）
 *
 * 支持两种 JSON 格式：
 *   - 裸配置：  {"text": {"font_size": 25}, "screen": {"resolution": [1280, 720]}}
 *   - 包装格式：{"config": {"text": {"font_size": 25}}}（与 DanmakuFactoryConfig.json 兼容）
 *
 * 加载后立即执行配置校验，若存在非法值则返回错误码且不修改上下文。
 *
 * @param ctx      上下文句柄
 * @param json_str 配置 JSON 字符串（UTF-8）
 * @return DF_SUCCESS / DF_ERROR_INVALID_JSON / DF_ERROR_INVALID_CONFIG 等
 */
DF_API df_error_code_t df_load_config_json(df_context_t *ctx, const char *json_str);

/**
 * @brief 从 JSON 配置文件一次性加载全部配置
 *
 * 等价于读取文件内容后调用 df_load_config_json。
 *
 * @param ctx  上下文句柄
 * @param path 配置文件路径（UTF-8）
 * @return DF_SUCCESS / DF_ERROR_FILE_NOT_FOUND / DF_ERROR_INVALID_JSON 等
 */
DF_API df_error_code_t df_load_config_file(df_context_t *ctx, const char *path);

/* --- 文件管理 --- */

/**
 * @brief 添加一个输入文件（可多次调用以添加多个）
 * @param ctx        上下文句柄
 * @param path       文件路径（UTF-8，库内部拷贝）
 * @param time_shift 时轴偏移量（秒），通常传 0.0
 * @return DF_SUCCESS 或相应错误码
 */
DF_API df_error_code_t df_add_input_file(df_context_t *ctx, const char *path, float time_shift);

/**
 * @brief 添加一个输入文件并显式指定文件格式（对应旧版 CLI 的输入模板参数）
 * @param ctx        上下文句柄
 * @param path       文件路径（UTF-8，库内部拷贝）
 * @param time_shift 时轴偏移量（秒）
 * @param type       格式名称："xml"、"json" 或 "ass"（大小写不敏感）
 * @return DF_SUCCESS 或相应错误码
 */
DF_API df_error_code_t df_add_input_file_typed(df_context_t *ctx, const char *path, float time_shift,
                                               const char *type);

/**
 * @brief 清除已添加的全部输入文件
 */
DF_API df_error_code_t df_clear_input_files(df_context_t *ctx);

/**
 * @brief 设置输出文件路径
 * @param ctx  上下文句柄
 * @param path 输出文件路径（UTF-8，库内部拷贝）；扩展名决定输出格式
 * @return DF_SUCCESS 或相应错误码
 */
DF_API df_error_code_t df_set_output_file(df_context_t *ctx, const char *path);

/**
 * @brief 设置输出文件路径并显式指定输出格式（对应旧版 CLI 的 -o [格式] 参数）
 * @param ctx  上下文句柄
 * @param path 输出文件路径（UTF-8，库内部拷贝），可无扩展名
 * @param type 格式名称："xml"、"json" 或 "ass"（大小写不敏感）
 * @return DF_SUCCESS 或相应错误码
 */
DF_API df_error_code_t df_set_output_file_typed(df_context_t *ctx, const char *path, const char *type);

/* --- 屏幕配置 --- */

DF_API df_error_code_t df_set_resolution(df_context_t *ctx, int width, int height);
DF_API df_error_code_t df_set_display_area(df_context_t *ctx, float area);
DF_API df_error_code_t df_set_scroll_area(df_context_t *ctx, float area);
DF_API df_error_code_t df_set_statistics_mode(df_context_t *ctx, int mode);

/* --- 弹幕配置 --- */

DF_API df_error_code_t df_set_density(df_context_t *ctx, int density);
DF_API df_error_code_t df_set_scroll_duration(df_context_t *ctx, float seconds);
DF_API df_error_code_t df_set_fixed_duration(df_context_t *ctx, float seconds);
DF_API df_error_code_t df_set_show_username(df_context_t *ctx, bool show);
DF_API df_error_code_t df_set_block_mode(df_context_t *ctx, int mode);
DF_API df_error_code_t df_set_keep_blocked(df_context_t *ctx, bool keep);

/* --- 文本配置 --- */

DF_API df_error_code_t df_set_fontname(df_context_t *ctx, const char *fontname);
DF_API df_error_code_t df_set_font_size(df_context_t *ctx, int size);
DF_API df_error_code_t df_set_font_size_strict(df_context_t *ctx, bool strict);
DF_API df_error_code_t df_set_font_size_norm(df_context_t *ctx, bool norm);
DF_API df_error_code_t df_set_bold(df_context_t *ctx, bool bold);
DF_API df_error_code_t df_set_shadow(df_context_t *ctx, float shadow);
DF_API df_error_code_t df_set_opacity(df_context_t *ctx, int opacity);
DF_API df_error_code_t df_set_outline(df_context_t *ctx, float outline);
DF_API df_error_code_t df_set_outline_blur(df_context_t *ctx, float blur);
DF_API df_error_code_t df_set_outline_opacity(df_context_t *ctx, int opacity);
DF_API df_error_code_t df_set_line_spacing(df_context_t *ctx, int spacing);

/* --- 消息框配置 --- */

DF_API df_error_code_t df_set_msgbox_show(df_context_t *ctx, bool show);
DF_API df_error_code_t df_set_msgbox_size(df_context_t *ctx, int width, int height);
DF_API df_error_code_t df_set_msgbox_pos(df_context_t *ctx, int x, int y);
DF_API df_error_code_t df_set_msgbox_font_size(df_context_t *ctx, int size);
DF_API df_error_code_t df_set_msgbox_duration(df_context_t *ctx, float seconds);
DF_API df_error_code_t df_set_msgbox_gift_min_price(df_context_t *ctx, float price);

/* ========================================================================
 * JSON 一次性 API（便捷）
 *
 * 适合脚本/命令行等一次性调用场景，无需管理句柄生命周期。
 * ======================================================================== */

/**
 * @brief 从 JSON 字符串运行 DanmakuFactory（一次性调用，无需 create/destroy）
 * @param runtime_info_json_str 包含了配置和文件信息的 JSON 字符串
 * @return 错误码
 */
DF_API df_error_code_t df_run_from_json(const char *runtime_info_json_str);

#ifdef __cplusplus
}
#endif

#endif
