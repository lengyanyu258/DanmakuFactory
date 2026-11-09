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

#include "base/error.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "base/utils.h"

// 线程局部错误状态实例（声明见 error.h）
TLS df_thread_error_t df_tls_error = {0};

// 线程局部缓冲区，用于存储完整错误消息
static TLS char df_full_error_message[2048] = {0};

// 库版本字符串（与 xmake.lua 中 LIB_VERSION 保持一致）
#ifndef DF_LIB_VERSION
    #define DF_LIB_VERSION "2.0.0"
#endif

const char *df_get_version(void)
{
    return DF_LIB_VERSION;
}

// 内部错误设置函数实现
df_error_code_t df_set_error_impl(df_error_code_t code, const char *file, const char *function, size_t line,
                                  const char *format, ...)
{
    // 保存错误信息
    df_tls_error.code = code;
    if (!file || !get_relative_path(file, df_tls_error.file, sizeof(df_tls_error.file)))
    {
        df_tls_error.file[0] = '\0';
    }
    df_tls_error.function = function;
    df_tls_error.line     = line;

    if (format && format[0])
    {
        va_list args;
        va_start(args, format);
        vsnprintf(df_tls_error.message, sizeof(df_tls_error.message), format, args);
        va_end(args);
    }
    else
    {
        switch (code)
        {
        case DF_SUCCESS:
            df_tls_error.message[0] = '\0';
            break;
        case DF_ERROR_INTERNAL:
            strncpy(df_tls_error.message, "Internal error occurred", sizeof(df_tls_error.message));
            break;
        case DF_ERROR_INVALID_JSON:
            strncpy(df_tls_error.message, "Invalid JSON format", sizeof(df_tls_error.message));
            break;

        case DF_ERROR_FILE_NOT_FOUND:
            strncpy(df_tls_error.message, "File not found", sizeof(df_tls_error.message));
            break;

        default:
            snprintf(df_tls_error.message, sizeof(df_tls_error.message), "Unknown error (%d)", code);
            break;
        }
    }

    return code;
}

// 对外 API 实现
df_error_code_t df_get_last_error_code(void)
{
    return df_tls_error.code;
}

const char *df_get_last_error_message(void)
{
    return df_tls_error.message[0] ? df_tls_error.message : "No error";
}

const char *df_get_last_error_function(void)
{
    return df_tls_error.function ? df_tls_error.function : "Unknown";
}

const char *df_get_last_error_file(void)
{
    return df_tls_error.file[0] ? df_tls_error.file : "Unknown";
}

int df_get_last_error_line(void)
{
    return df_tls_error.line;
}

const char *df_get_last_error_full_message(void)
{
    if (df_tls_error.code == DF_SUCCESS)
    {
        snprintf(df_full_error_message, sizeof(df_full_error_message), "Success");
    }
    else
    {
        const char *prefix = df_tls_error.code >= 0 ? "Warning" : "Error";

        if (df_tls_error.file[0] && df_tls_error.function && df_tls_error.line)
        {
            snprintf(df_full_error_message, sizeof(df_full_error_message), "%s %d: %s [at %s:%s:%zu]", prefix,
                     df_tls_error.code, df_tls_error.message, df_tls_error.file, df_tls_error.function,
                     df_tls_error.line);
        }
        else
        {
            snprintf(df_full_error_message, sizeof(df_full_error_message), "%s %d: %s", prefix, df_tls_error.code,
                     df_tls_error.message);
        }
    }

    return df_full_error_message;
}
