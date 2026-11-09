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

#include "danmakufactory.h"

#include <stdlib.h>
#include <string.h>

#include "base/error.h"
#include "base/utils.h"
#include "config/define.h"
#include "config/parse.h"
#include "config/validate.h"
#include "core/process.h"

/* ========================================================================
 * 内部工具
 * ======================================================================== */

static void df_free_block_list(char **block_list)
{
    if (block_list)
    {
        for (char **line = block_list; *line; ++line)
        {
            free(*line);
        }
        free(block_list);
    }
}

// 释放解析阶段在 runtime_info 内部分配的内存
static void df_free_runtime_info(df_runtime_info_t *runtime_info)
{
    if (!runtime_info)
    {
        return;
    }

    // 黑名单：NULL 结尾的字符串数组，逐项释放
    df_free_block_list(runtime_info->config.danmaku.block_list);
    runtime_info->config.danmaku.block_list = NULL;

    // 输入文件信息数组
    free(runtime_info->input_files.fileinfos);
    runtime_info->input_files.fileinfos = NULL;
    runtime_info->input_files.number    = 0;
}

/* ========================================================================
 * JSON 一次性 API
 * ======================================================================== */

df_error_code_t df_run_from_json(const char *runtime_info_json_str)
{
    DF_SET_SUCCESS(); /* 清除本线程上一次调用遗留的错误状态 */

    df_runtime_info_t runtime_info = *df_get_default_runtime_info_internal();

    df_error_code_t code = df_parse_runtime_info_json_str(runtime_info_json_str, &runtime_info);
    if (code != DF_SUCCESS)
    {
        goto end;
    }

    code = df_validate_runtime_info(&runtime_info);
    if (code != DF_SUCCESS)
    {
        goto end;
    }

    code = df_process_convert_files(&runtime_info.config, &runtime_info.input_files, &runtime_info.output_file);

end:
    df_free_runtime_info(&runtime_info);
    return code;
}

/* ========================================================================
 * 标准化句柄 API 实现
 * ======================================================================== */

struct df_context_s
{
    df_runtime_info_t runtime_info;
};

/* --- 生命周期 --- */

df_context_t *df_create(void)
{
    df_context_t *ctx = (df_context_t *)malloc(sizeof(df_context_t));
    if (!ctx)
    {
        DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Failed to allocate df_context_t");
        return NULL;
    }

    ctx->runtime_info = *df_get_default_runtime_info_internal();
    DF_SET_SUCCESS();
    return ctx;
}

void df_destroy(df_context_t *ctx)
{
    if (!ctx)
    {
        return;
    }
    df_free_runtime_info(&ctx->runtime_info);
    free(ctx);
}

/* --- 执行 --- */

df_error_code_t df_run(df_context_t *ctx)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }

    DF_SET_SUCCESS(); /* 清除本线程上一次调用遗留的错误状态 */

    df_error_code_t code = df_validate_runtime_info(&ctx->runtime_info);
    if (code != DF_SUCCESS)
    {
        return code;
    }

    return df_process_convert_files(&ctx->runtime_info.config, &ctx->runtime_info.input_files,
                                    &ctx->runtime_info.output_file);
}

/* --- 批量配置加载 --- */

df_error_code_t df_load_config_json(df_context_t *ctx, const char *json_str)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    if (!json_str || !json_str[0])
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "json_str is NULL or empty");
    }

    size_t json_str_len = strlen(json_str);

    yyjson_read_err  err;
    yyjson_read_flag flags = YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_BOM |
                             YYJSON_READ_ALLOW_SINGLE_QUOTED_STR | YYJSON_READ_ALLOW_UNQUOTED_KEY;
    yyjson_doc *doc = yyjson_read_opts((char *)json_str, json_str_len, flags, NULL, &err);
    if (!doc)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Config JSON parse error: %s (code %u at byte %lu)", err.msg,
                            err.code, err.pos);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !unsafe_yyjson_is_obj(root))
    {
        yyjson_doc_free(doc);
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Config JSON root is not an object");
    }

    // 支持两种格式：{"config": {...}} 或裸配置 {...}
    yyjson_val *config_val = df_json_get_member(root, "config");
    if (!config_val)
    {
        config_val = root;
    }

    // 临时副本共享旧列表；解析器只在新列表完整后提交新指针。
    df_config_t temp_config = ctx->runtime_info.config;
    char **old_block_list = ctx->runtime_info.config.danmaku.block_list;

    df_error_code_t code = df_parse_config_json_val(config_val, &temp_config);
    if (code == DF_SUCCESS)
    {
        code = df_validate_config(&temp_config);
    }
    if (code != DF_SUCCESS)
    {
        if (temp_config.danmaku.block_list != old_block_list)
        {
            df_free_block_list(temp_config.danmaku.block_list);
        }
        yyjson_doc_free(doc);
        return code;
    }

    // 校验通过后接管新列表，并释放被替换的旧列表。
    if (temp_config.danmaku.block_list != old_block_list)
    {
        df_free_block_list(old_block_list);
    }
    ctx->runtime_info.config = temp_config;

    yyjson_doc_free(doc);
    return DF_SET_SUCCESS();
}

df_error_code_t df_load_config_file(df_context_t *ctx, const char *path)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    if (!path || !path[0])
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "path is NULL or empty");
    }

    FILE *fp = df_fopen(path, "rb");
    if (!fp)
    {
        return DF_SET_ERROR(DF_ERROR_FILE_NOT_FOUND, "Cannot open config file \"%s\"", path);
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(fp);
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "Config file \"%s\" is empty", path);
    }

    char *buffer = (char *)malloc((size_t)file_size + 1);
    if (!buffer)
    {
        fclose(fp);
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Memory allocation failed");
    }

    size_t read_size = fread(buffer, 1, (size_t)file_size, fp);
    fclose(fp);
    buffer[read_size] = '\0';

    df_error_code_t code = df_load_config_json(ctx, buffer);
    free(buffer);
    return code;
}

/* --- 文件管理 --- */

// 解析显式格式名称，非法时设置错误并返回 DF_FILETYPE_UNKNOWN
static df_filetype_t parse_explicit_filetype(const char *type)
{
    if (!type || !type[0])
    {
        DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "type is NULL or empty");
        return DF_FILETYPE_UNKNOWN;
    }

    df_filetype_t filetype = get_filetype_by_name(type);
    if (filetype != DF_FILETYPE_ASS && filetype != DF_FILETYPE_JSON && filetype != DF_FILETYPE_XML)
    {
        DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "type is invalid: \"%s\" (available: xml, json, ass)", type);
        return DF_FILETYPE_UNKNOWN;
    }

    return filetype;
}

// 内部实现：添加输入文件，has_explicit_type 为 false 时按扩展名推断
static df_error_code_t add_input_file_impl(df_context_t *ctx, const char *path, float time_shift,
                                           df_filetype_t explicit_type, bool has_explicit_type)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    if (!path || !path[0])
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "path is NULL or empty");
    }

    df_fileinfos_t *files     = &ctx->runtime_info.input_files;
    size_t          new_count = files->number + 1;

    df_fileinfo_t *new_array = (df_fileinfo_t *)realloc(files->fileinfos, new_count * sizeof(df_fileinfo_t));
    if (!new_array)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Failed to grow input file array");
    }

    df_fileinfo_t *entry = &new_array[new_count - 1];
    memset(entry, 0, sizeof(*entry));

    if (str_copy_ns(entry->path, sizeof(entry->path), path, strlen(path)) != DF_SUCCESS)
    {
        /* realloc 已成功，旧数据已迁移到 new_array；保留已有条目，仅不增加计数 */
        files->fileinfos = new_array;
        return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "path too long (max %d)", (int)sizeof(entry->path) - 1);
    }

    entry->type       = has_explicit_type ? explicit_type : get_filetype(path);
    entry->time_shift = time_shift;

    files->fileinfos = new_array;
    files->number    = new_count;

    return DF_SET_SUCCESS();
}

df_error_code_t df_add_input_file(df_context_t *ctx, const char *path, float time_shift)
{
    return add_input_file_impl(ctx, path, time_shift, DF_FILETYPE_NONE, false);
}

df_error_code_t df_add_input_file_typed(df_context_t *ctx, const char *path, float time_shift, const char *type)
{
    df_filetype_t filetype = parse_explicit_filetype(type);
    if (filetype == DF_FILETYPE_UNKNOWN)
    {
        return df_get_last_error_code();
    }
    return add_input_file_impl(ctx, path, time_shift, filetype, true);
}

df_error_code_t df_clear_input_files(df_context_t *ctx)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }

    free(ctx->runtime_info.input_files.fileinfos);
    ctx->runtime_info.input_files.fileinfos = NULL;
    ctx->runtime_info.input_files.number    = 0;

    return DF_SET_SUCCESS();
}

df_error_code_t df_set_output_file(df_context_t *ctx, const char *path)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    if (!path || !path[0])
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "path is NULL or empty");
    }

    df_fileinfo_t *out = &ctx->runtime_info.output_file;

    if (str_copy_ns(out->path, sizeof(out->path), path, strlen(path)) != DF_SUCCESS)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "path too long (max %d)", (int)sizeof(out->path) - 1);
    }

    out->type = get_filetype(path);
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_output_file_typed(df_context_t *ctx, const char *path, const char *type)
{
    df_filetype_t filetype = parse_explicit_filetype(type);
    if (filetype == DF_FILETYPE_UNKNOWN)
    {
        return df_get_last_error_code();
    }

    df_error_code_t code = df_set_output_file(ctx, path);
    if (code != DF_SUCCESS)
    {
        return code;
    }

    ctx->runtime_info.output_file.type = filetype;
    return DF_SET_SUCCESS();
}

/* --- 屏幕配置 --- */

df_error_code_t df_set_resolution(df_context_t *ctx, int width, int height)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.screen.resolution.x = width;
    ctx->runtime_info.config.screen.resolution.y = height;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_display_area(df_context_t *ctx, float area)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.screen.display_area = area;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_scroll_area(df_context_t *ctx, float area)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.screen.scroll_area = area;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_statistics_mode(df_context_t *ctx, int mode)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.screen.statistics_mode = (df_stats_flag_t)mode;
    return DF_SET_SUCCESS();
}

/* --- 弹幕配置 --- */

df_error_code_t df_set_density(df_context_t *ctx, int density)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.danmaku.density = density;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_scroll_duration(df_context_t *ctx, float seconds)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.danmaku.scroll_duration = seconds;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_fixed_duration(df_context_t *ctx, float seconds)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.danmaku.fixed_duration = seconds;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_show_username(df_context_t *ctx, bool show)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.danmaku.show_username = show;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_block_mode(df_context_t *ctx, int mode)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.danmaku.block_mode = (df_block_flag_t)mode;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_keep_blocked(df_context_t *ctx, bool keep)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.danmaku.keep_blocked = keep;
    return DF_SET_SUCCESS();
}

/* --- 文本配置 --- */

df_error_code_t df_set_fontname(df_context_t *ctx, const char *fontname)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    if (!fontname)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "fontname is NULL");
    }
    return str_copy_ns(ctx->runtime_info.config.text.fontname, sizeof(ctx->runtime_info.config.text.fontname),
                       fontname, strlen(fontname));
}

df_error_code_t df_set_font_size(df_context_t *ctx, int size)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.font_size = size;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_font_size_strict(df_context_t *ctx, bool strict)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.font_size_strict = strict;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_font_size_norm(df_context_t *ctx, bool norm)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.font_size_norm = norm;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_bold(df_context_t *ctx, bool bold)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.bold = bold;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_shadow(df_context_t *ctx, float shadow)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.shadow = shadow;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_opacity(df_context_t *ctx, int opacity)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.opacity = opacity;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_outline(df_context_t *ctx, float outline)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.outline = outline;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_outline_blur(df_context_t *ctx, float blur)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.outline_blur = blur;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_outline_opacity(df_context_t *ctx, int opacity)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.outline_opacity = opacity;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_line_spacing(df_context_t *ctx, int spacing)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.text.line_spacing = spacing;
    return DF_SET_SUCCESS();
}

/* --- 消息框配置 --- */

df_error_code_t df_set_msgbox_show(df_context_t *ctx, bool show)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.msgbox.show = show;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_msgbox_size(df_context_t *ctx, int width, int height)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.msgbox.size.x = width;
    ctx->runtime_info.config.msgbox.size.y = height;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_msgbox_pos(df_context_t *ctx, int x, int y)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.msgbox.pos.x = x;
    ctx->runtime_info.config.msgbox.pos.y = y;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_msgbox_font_size(df_context_t *ctx, int size)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.msgbox.font_size = size;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_msgbox_duration(df_context_t *ctx, float seconds)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.msgbox.duration = seconds;
    return DF_SET_SUCCESS();
}

df_error_code_t df_set_msgbox_gift_min_price(df_context_t *ctx, float price)
{
    if (!ctx)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_PARAMETER, "ctx is NULL");
    }
    ctx->runtime_info.config.msgbox.gift_min_price = price;
    return DF_SET_SUCCESS();
}
