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

#include "base/utils.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/define.h"
#include "base/error.h"
#include "danmakufactory.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

// 定义工程根目录的相对路径前缀
#define PROJECT_ROOT_DIR "DanmakuFactory"

#define INITIAL_CAPACITY 16

/**
 * @brief 可移植的 UTF-8 路径文件打开（Windows 下 fopen 按本地代码页解析路径，需转宽字符）
 * @param path UTF-8 编码的文件路径
 * @param mode 打开模式（如 "r"、"w"）
 * @return 文件指针，失败返回 NULL
 */
FILE *df_fopen(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t wmode[8];
    if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, ARRAY_SIZE(wmode)) == 0)
    {
        return NULL;
    }

    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wpath_len <= 0)
    {
        return NULL;
    }
    wchar_t *wpath = malloc(wpath_len * sizeof(wchar_t));
    if (!wpath)
    {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpath_len);

    FILE *fp = _wfopen(wpath, wmode);
    free(wpath);
    return fp;
#else
    return fopen(path, mode);
#endif
}

/**
 * @brief 可移植的字符串复制（避免依赖平台特定的 _strdup/strdup）
 * @param source 源字符串
 * @return 新分配的字符串副本，失败返回 NULL
 */
char *df_strdup(const char *source)
{
    size_t size        = strlen(source) + 1;
    char  *destination = malloc(size);
    if (destination != NULL)
    {
        memcpy(destination, source, size);
    }
    return destination;
}

/**
 * @brief 根据文件类型获取对应的文件扩展名
 * @param filetype 文件类型
 * @return 对应的文件扩展名字符串，如 ".ass"、".json"、".xml"等
 */
const char *get_extension(df_filetype_t filetype)
{
    switch (filetype)
    {
    case DF_FILETYPE_ASS:
        return ".ass";
    case DF_FILETYPE_JSON:
        return ".json";
    case DF_FILETYPE_XML:
        return ".xml";
    case DF_FILETYPE_NONE:
        return "";
    default:
        return NULL;
    }
}

/* 文件格式名称到类型的映射表（大小写不敏感匹配） */
static const flag_mapping_t filetype_mappings[] = {
    {"ASS", DF_FILETYPE_ASS},
    {"JSON", DF_FILETYPE_JSON},
    {"XML", DF_FILETYPE_XML},
};

/**
 * @brief 根据文件扩展名获取对应的文件类型
 * @param file_path 文件路径
 * @return 对应的文件类型
 */
df_filetype_t get_filetype(const char *file_path)
{
    if (!file_path)
    {
        return DF_FILETYPE_UNKNOWN;
    }

    const char *extension = strrchr(file_path, '.');
    if (!extension)
    {
        return DF_FILETYPE_UNKNOWN;
    }

    uint64_t filetype = DF_FILETYPE_NONE;

    if (get_flag_type(extension + 1, filetype_mappings, ARRAY_SIZE(filetype_mappings), &filetype) !=
        DF_SUCCESS)
    {
        return DF_FILETYPE_UNKNOWN;
    }

    return (df_filetype_t)filetype;
};

/**
 * @brief 根据格式名称直接获取文件类型（对应 df_fileinfo_t 的显式 type 设定）
 * @param name 格式名称，如 "xml"、"json"、"ass"（大小写不敏感，可带或不带前导点）
 * @return 对应的文件类型，无法识别时返回 DF_FILETYPE_UNKNOWN
 */
df_filetype_t get_filetype_by_name(const char *name)
{
    if (!name)
    {
        return DF_FILETYPE_UNKNOWN;
    }

    // 容忍 ".ass" 这类带前导点的写法
    if (name[0] == '.')
    {
        name++;
    }

    uint64_t filetype = DF_FILETYPE_NONE;

    if (get_flag_type(name, filetype_mappings, ARRAY_SIZE(filetype_mappings), &filetype) != DF_SUCCESS)
    {
        return DF_FILETYPE_UNKNOWN;
    }

    return (df_filetype_t)filetype;
}

/**
 * @brief 忽略大小写比较两个字符串
 * @param str1 第一个待比较的字符串指针
 * @param str2 第二个待比较的字符串指针
 * @return 0 - 字符串相等（忽略大小写）
 * @return 负数 - 第一个字符串小于第二个字符串（忽略大小写）
 * @return 正数 - 第一个字符串大于第二个字符串（忽略大小写）
 */
int strcmp_ignore_case(const char *str1, const char *str2)
{
    /* 非法判断 */
    if (str1 == NULL && str2 == NULL)
    {
        return 0;
    }
    else if (str1 == NULL)
    {
        return -1;
    }
    else if (str2 == NULL)
    {
        return 1;
    }

    char c1, c2;
    while (*str1 != '\0' && *str2 != '\0')
    {
        c1 = *str1;
        c2 = *str2;

        /* 转换为小写进行比较 */
        if (c1 >= 'A' && c1 <= 'Z')
        {
            c1 += 'a' - 'A';
        }
        if (c2 >= 'A' && c2 <= 'Z')
        {
            c2 += 'a' - 'A';
        }

        if (c1 != c2)
        {
            return (int)(c1 - c2);
        }

        str1++;
        str2++;
    }

    /* 检查哪个字符串先结束 */
    if (*str1 == '\0' && *str2 == '\0')
    {
        return 0;
    }
    else if (*str1 == '\0')
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief 修改文件路径的后缀名为指定文件类型对应的后缀名
 * @param original_path 原始文件路径
 * @param filetype 目标文件类型
 * @param target_path 输出的新文件路径
 * @param target_path_size 输出缓冲区大小
 * @return 操作是否成功
 */
df_error_code_t change_file_extension(const char *original_path, df_filetype_t filetype, char *target_path,
                                      size_t target_path_size)
{
    if (!original_path || !target_path || target_path_size == 0)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "NULL parameter");
    }

    const char *new_extension = get_extension(filetype);
    if (!new_extension)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "unknown filetype");
    }

    // 查找最后一个 '.' 符号的位置
    const char *last_dot            = strrchr(original_path, '.');
    const char *last_slash          = strrchr(original_path, '\\');
    const char *last_path_separator = strrchr(original_path, '/');

    // 取最后一个路径分隔符（Windows 和 Unix 风格都考虑）
    const char *last_separator = (last_slash > last_path_separator) ? last_slash : last_path_separator;

    ptrdiff_t base_length = last_dot - original_path;

    if (!last_dot || (last_separator && last_dot > last_separator) || base_length < 0)
    {
        // 如果没有扩展名或扩展名在路径分隔符之前，则使用整个路径作为基础
        base_length = strlen(original_path);
    }

    // 检查新路径是否会超出缓冲区大小
    size_t new_extension_length = strlen(new_extension);
    if (base_length + new_extension_length >= target_path_size)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "target path buffer too small");
    }

    // 复制基础路径部分
    str_copy_n(target_path, original_path, base_length);
    // 添加新的扩展名
    str_copy_n(target_path + base_length, new_extension, new_extension_length);

    return DF_SET_SUCCESS();
}

/**
 * @brief 获取文件的相对路径
 * @param full_path 完整文件路径
 * @param buffer 输出缓冲区
 * @param buffer_size 输出缓冲区大小
 * @return 操作是否成功
 */
bool get_relative_path(const char *full_path, char *buffer, size_t buffer_size)
{
    if (!full_path || !buffer || buffer_size == 0)
    {
        return false;
    }

    // 寻找工程根目录标记
    const char *project_root = strstr(full_path, PROJECT_ROOT_DIR);
    if (project_root != NULL)
    {
        // 从工程根目录开始复制路径
        const char *relative_path = project_root;
        size_t      len           = strlen(relative_path);

        // 确保不超过缓冲区大小
        if (len < buffer_size)
        {
            strcpy(buffer, relative_path);
            return true;
        }
        else
        {
            // 截断过长的路径
            strncpy(buffer, relative_path, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return true;
        }
    }

    // 如果找不到工程根目录，尝试获取文件名
    const char *filename = strrchr(full_path, '\\');
    if (filename != NULL)
    {
        filename++;
        size_t len = strlen(filename);

        if (len < buffer_size)
        {
            strcpy(buffer, filename);
            return true;
        }
        else
        {
            strncpy(buffer, filename, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return true;
        }
    }

    // 如果都不行，直接返回原始路径的截断版本
    size_t len = strlen(full_path);
    if (len < buffer_size)
    {
        strcpy(buffer, full_path);
        return true;
    }
    else
    {
        strncpy(buffer, full_path, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return true;
    }

    return true;
}

/**
 * @brief 读取文件的每一行并存储到字符串数组中
 * @param filename 要读取的文件名
 * @param content_lines 输出的行指针数组
 * @return 操作是否成功
 */
df_error_code_t read_file_lines(const char *filename, char ***content_lines)
{
    /* 非法判断 */
    if (filename == NULL || content_lines == NULL)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "Invalid parameter!");
    }

    FILE *fp = df_fopen(filename, "r");
    if (fp == NULL)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "Open block list file `%s` failed!", filename);
    }

    // 动态分配初始容量
    size_t capacity = INITIAL_CAPACITY;
    char **lines    = malloc(capacity * sizeof(char *));
    if (!lines)
    {
        fclose(fp);
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Memory allocation failed");
    }

    char   buf[BUFFER_SIZE];
    size_t len   = 0;
    int    count = 0;

    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        // 移除换行符
        len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n')
        {
            buf[len - 1] = '\0';
            len--;
        }

        // 跳过空行
        if (len == 0)
        {
            continue;
        }

        // 扩容检查
        if (count >= (int)capacity)
        {
            capacity *= 2;
            char **new_lines = realloc(lines, capacity * sizeof(char *));
            if (!new_lines)
            {
                DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Memory reallocation failed");
                goto clean;
            }
            lines = new_lines;
        }

        // 复制行内容
        lines[count] = df_strdup(buf);
        if (!lines[count])
        {
            DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "String duplication failed");
            goto clean;
        }
        count++;
    }

    fclose(fp);

    // 调整到实际大小
    if (count > 0)
    {
        char **final_lines = realloc(lines, (count + 1) * sizeof(char *));
        if (final_lines)
        {
            lines = final_lines;
        }
        lines[count] = NULL; // NULL 终止符
    }
    else
    {
        free(lines);
        lines = NULL;
    }

    *content_lines = lines;

    return DF_SET_SUCCESS();

clean:
    // 清理已分配的内存
    for (int i = 0; i < count; i++)
    {
        free(lines[i]);
    }
    free(lines);
    fclose(fp);

    return df_get_last_error_code();
}

/**
 * @brief 安全的字符串复制
 * @param destination 目的字符串
 * @param source 源字符串
 * @param count 要复制的字符数
 * @return df_error_code_t 错误码
 */
df_error_code_t str_copy_n(char *destination, const char *source, size_t count)
{
    if (destination == NULL || source == NULL || count == 0)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "invalid parameter");
    }

    size_t i = 0;
    do
    {
        destination[i] = source[i];
    } while (++i < count && source[i]);
    destination[i] = '\0';

    return DF_SET_SUCCESS();
}

/**
 * @brief 安全的字符串复制
 * @param destination 目的字符串
 * @param size 目的字符串最大长度
 * @param source 源字符串
 * @param count 要复制的字符数
 * @return df_error_code_t 错误码
 */
df_error_code_t str_copy_ns(char *destination, size_t size, const char *source, size_t count)
{
    if (size == 0)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "invalid parameter");
    }
    if (count >= size)
    {
        str_copy_n(destination, source, size - 1);
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "string is too long");
    }

    return str_copy_n(destination, source, count);
}

/**
 * @brief 将字符串转换为大写
 * @param destination 目的字符串
 * @param source 源字符串
 * @return df_error_code_t 错误码
 */
df_error_code_t str_to_upper_s(char *destination, const char *source)
{
    if (destination == NULL || source == NULL)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "Invalid parameters");
    }

    while (*source)
    {
        *destination++ = toupper(*source++);
    }
    *destination = '\0';

    return DF_SET_SUCCESS();
}

/**
 * @brief 将字符串转换为大写
 * @param destination 目的字符串
 * @param source 源字符串
 * @param count 要转换的字符数
 * @return df_error_code_t 错误码
 */
df_error_code_t str_to_upper_n(char *destination, const char *source, size_t count)
{
    if (destination == NULL || source == NULL || count == 0)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "Invalid parameters");
    }

    for (size_t i = 0; i < count; i++)
    {
        destination[i] = toupper(source[i]);
    }
    destination[count] = '\0';

    return DF_SET_SUCCESS();
}

/**
 * @brief 将字符串转换为小写
 * @param destination 目的字符串
 * @param source 源字符串
 * @param count 要转换的字符数
 * @return df_error_code_t 错误码
 */
df_error_code_t str_to_lower_n(char *destination, const char *source, size_t count)
{
    if (destination == NULL || source == NULL || count == 0)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "Invalid parameters");
    }

    for (size_t i = 0; i < count; i++)
    {
        destination[i] = tolower(source[i]);
    }
    destination[count] = '\0';

    return DF_SET_SUCCESS();
}

/**
 * @brief 将 Unicode 码点转换为 UTF-8 编码
 * @param codepoint Unicode 码点
 * @param utf8_buffer 用于存储 UTF-8 编码的缓冲区
 * @return df_error_code_t 错误码
 */
df_error_code_t unicode_to_utf8(unsigned int codepoint, char *utf8_buffer)
{
    if (codepoint <= 0x7F)
    {
        utf8_buffer[0] = (char)codepoint;
        utf8_buffer[1] = '\0';
    }
    else if (codepoint <= 0x7FF)
    {
        utf8_buffer[0] = (char)(0xC0 | (codepoint >> 6));
        utf8_buffer[1] = (char)(0x80 | (codepoint & 0x3F));
        utf8_buffer[2] = '\0';
    }
    else if (codepoint <= 0xFFFF)
    {
        utf8_buffer[0] = (char)(0xE0 | (codepoint >> 12));
        utf8_buffer[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8_buffer[2] = (char)(0x80 | (codepoint & 0x3F));
        utf8_buffer[3] = '\0';
    }
    else if (codepoint <= 0x10FFFF)
    {
        utf8_buffer[0] = (char)(0xF0 | (codepoint >> 18));
        utf8_buffer[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        utf8_buffer[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8_buffer[3] = (char)(0x80 | (codepoint & 0x3F));
        utf8_buffer[4] = '\0';
    }
    else
    {
        utf8_buffer[0] = '\0';
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "Invalid code point");
    }

    return DF_SET_SUCCESS();
}

df_error_code_t get_flag_type(const char *flag_str, const flag_mapping_t *flag_mappings, const size_t mappings_num,
                              uint64_t *out_flag)
{
    if (!flag_str || !flag_mappings || !out_flag)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "Invalid parameters");
    }

    *out_flag = 0;

    size_t flag_str_len = strlen(flag_str);
    if (flag_str_len == 0)
    {
        return DF_SET_ERROR(DF_SUCCESS, "Empty flag string");
    }

    // 将输入字符串转换为大写以进行比较（变长数组为 MSVC 不支持的 VLA，改用堆分配）
    char *flag_upper_str = malloc(flag_str_len + 1);
    if (!flag_upper_str)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Memory allocation failed");
    }
    if (str_to_upper_n(flag_upper_str, flag_str, flag_str_len) != DF_SUCCESS)
    {
        free(flag_upper_str);
        return df_get_last_error_code();
    }

    // 遍历映射表查找匹配项
    size_t i;

    for (i = 0; i < mappings_num; i++)
    {
        if (strcmp(flag_upper_str, flag_mappings[i].flag_str) == 0)
        {
            *out_flag = flag_mappings[i].flag_value;
            break;
        }
    }

    free(flag_upper_str);

    // 如果没有找到匹配项，返回错误
    if (i >= mappings_num)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Invalid flag string");
    }

    return DF_SET_SUCCESS();
}
