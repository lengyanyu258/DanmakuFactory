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

#ifndef DF_BASE_ERROR_H_
#define DF_BASE_ERROR_H_

#include "danmakufactory.h"
#include <stddef.h>

// 函数名宏定义（确保在C和C++中都能正常工作）
#if defined(__cplusplus)
    #define DF_FUNCTION __func__
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    #define DF_FUNCTION __func__
#elif defined(__GNUC__)
    #define DF_FUNCTION __FUNCTION__
#else
    #define DF_FUNCTION "unknown"
#endif

// 每个线程独立的错误状态
typedef struct
{
    df_error_code_t code;
    char            message[1024]; // 错误消息
    char            file[256];     // 文件名（使用固定大小缓冲区存储相对路径）
    const char     *function;      // 函数名
    size_t          line;          // 行号
} df_thread_error_t;

// 使用线程局部存储（C11 标准）
#if defined(_MSC_VER)
    #define TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define TLS _Thread_local
#elif defined(__GNUC__)
    #define TLS __thread
#else
    #error "No TLS support!"
#endif

// 线程局部错误状态（定义在 error.c，全库共享同一份实例）
extern TLS df_thread_error_t df_tls_error;

// 内部错误设置函数
df_error_code_t df_set_error_impl(df_error_code_t code, const char *file, const char *function, size_t line,
                                  const char *format, ...);

// 公开的错误设置宏（自动填充文件、函数和行号）
#define DF_SET_ERROR(code, format, ...) df_set_error_impl(code, __FILE__, DF_FUNCTION, __LINE__, format, ##__VA_ARGS__)
#define DF_SET_SUCCESS()                df_set_error_impl(DF_SUCCESS, NULL, NULL, 0, NULL)

#endif