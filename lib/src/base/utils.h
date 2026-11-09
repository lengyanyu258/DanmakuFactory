/*
 * Copyright (c) 2023 Shawn Sun <lengyanyu258@outlook.com>
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

#ifndef DF_BASE_UTILS_H_
#define DF_BASE_UTILS_H_

#include <stdio.h>

#include "config/define.h"
#include "danmakufactory.h"

// 使用驱动表模式，定义字符串到枚举值的映射关系
typedef struct
{
    const char *flag_str;
    uint64_t    flag_value;
} flag_mapping_t;

const char *get_extension(df_filetype_t filetype);
bool        get_relative_path(const char *full_path, char *buffer, size_t buffer_size);
int         strcmp_ignore_case(const char *str1, const char *str2);

df_filetype_t get_filetype(const char *file_path);
df_filetype_t get_filetype_by_name(const char *name);

df_error_code_t read_file_lines(const char *filename, char ***content_lines);
FILE           *df_fopen(const char *path, const char *mode);
char           *df_strdup(const char *source);
df_error_code_t str_copy_n(char *destination, const char *source, size_t count);
df_error_code_t str_copy_ns(char *destination, size_t size, const char *source, size_t count);
df_error_code_t str_to_upper_s(char *destination, const char *source);
df_error_code_t str_to_upper_n(char *destination, const char *source, size_t count);
df_error_code_t str_to_lower_n(char *destination, const char *source, size_t count);
df_error_code_t unicode_to_utf8(unsigned int codepoint, char *utf8_buffer);
df_error_code_t change_file_extension(const char *original_path, df_filetype_t filetype, char *target_path,
                                      size_t target_path_size);
df_error_code_t get_flag_type(const char *flag_str, const flag_mapping_t *flag_mappings, const size_t mappings_num,
                              uint64_t *out_flag);

#endif // DF_BASE_UTILS_H_