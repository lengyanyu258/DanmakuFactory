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

#ifdef _WIN32
    #ifdef DANMAKU_FACTORY_EXPORTS
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
} df_error_code_t;

// DF_API int df_parse_args(int argc, char *argv[]);

// 公开的获取错误信息的函数
df_error_code_t df_get_last_error_code(void);
const char     *df_get_last_error_message(void);
const char     *df_get_last_error_file(void);
const char     *df_get_last_error_function(void);
int             df_get_last_error_line(void);
const char     *df_get_last_error_full_message(void);

// TODO: get default runtime info
// df_runtime_info_t *df_get_default_runtime_info(void);

/**
 * @brief 从 JSON 字符串运行 DanmakuFactory
 * @param runtime_info_json 包含了配置和文件信息的 JSON 字符串
 * @return 错误码
 */
df_error_code_t df_run_from_json(const char *runtime_info_json_str);

#ifdef __cplusplus
}
#endif

#endif
