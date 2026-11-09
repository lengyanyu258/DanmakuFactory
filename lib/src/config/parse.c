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

#include "config/parse.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <yyjson.h>

#include "base/define.h"
#include "base/error.h"
#include "base/utils.h"
#include "config/define.h"
#include "danmakufactory.h"

#define CHECK_JSON_AND_OUT_VAL(json_val, out_val)                                                                      \
    if (!json_val)                                                                                                     \
    {                                                                                                                  \
        return DF_SET_SUCCESS();                                                                                       \
    }                                                                                                                  \
    if (!out_val)                                                                                                      \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "Invalid " #out_val);                                                   \
    }

/* Compare field names without changing string values such as paths and patterns. */
yyjson_val *df_json_get_member(yyjson_val *object, const char *name)
{
    yyjson_val *value = yyjson_obj_get(object, name);
    if (value != NULL || !yyjson_is_obj(object))
    {
        return value;
    }

    size_t index, size;
    yyjson_val *key;
    yyjson_obj_foreach(object, index, size, key, value)
    {
        if (strcmp_ignore_case(yyjson_get_str(key), name) == 0)
        {
            return value;
        }
    }
    return NULL;
}

/* Nested settings take precedence over legacy flat keys. */
static yyjson_val *json_get_config_field(yyjson_val *config, const char *section, const char *field)
{
    yyjson_val *value = df_json_get_member(df_json_get_member(config, section), field);
    if (value != NULL)
    {
        return value;
    }

    char flat_name[96];
    if (strcmp(section, "msgbox") == 0)
    {
        snprintf(flat_name, sizeof(flat_name), "msgbox_%s", field);
    }
    else
    {
        snprintf(flat_name, sizeof(flat_name), "%s", field);
    }
    value = df_json_get_member(config, flat_name);
    if (value != NULL)
    {
        return value;
    }

    static const struct
    {
        const char *section;
        const char *field;
        const char *alias;
    } aliases[] = {
        {"screen", "display_area", "displayarea"}, {"screen", "scroll_area", "scrollarea"},
        {"screen", "statistics_mode", "statmode"},
        {"danmaku", "scroll_duration", "scrolltime"}, {"danmaku", "fixed_duration", "fixtime"},
        {"danmaku", "block_mode", "blockmode"}, {"danmaku", "keep_blocked", "saveblocked"},
        {"danmaku", "show_username", "show_usernames"}, {"danmaku", "show_username", "showusernames"},
        {"danmaku", "block_list", "blacklist"}, {"danmaku", "block_list_regex_enabled", "blacklist_regex"},
        {"text", "font_size", "fontsize"}, {"text", "line_spacing", "linespacing"},
        {"msgbox", "show", "show_msgbox"}, {"msgbox", "show", "showmsgbox"},
        {"msgbox", "size", "msgboxsize"}, {"msgbox", "pos", "msgboxpos"},
        {"msgbox", "font_size", "msgbox_fontsize"}, {"msgbox", "font_size", "msgboxfontsize"},
        {"msgbox", "duration", "msgboxduration"}, {"msgbox", "duration", "duration"},
        {"msgbox", "gift_min_price", "gift_min_price"}, {"msgbox", "gift_min_price", "giftminprice"},
    };
    for (size_t i = 0; i < ARRAY_SIZE(aliases); i++)
    {
        if (strcmp(section, aliases[i].section) == 0 && strcmp(field, aliases[i].field) == 0)
        {
            value = df_json_get_member(config, aliases[i].alias);
            if (value != NULL)
            {
                return value;
            }
        }
    }
    return NULL;
}
static df_error_code_t parse_int_json_val(yyjson_val *json_val, int *number)
{
    CHECK_JSON_AND_OUT_VAL(json_val, number);
    if (!yyjson_is_num(json_val))
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Value is not a number");
    }
    double value = yyjson_get_num(json_val);
    if (!isfinite(value) || value < INT_MIN || value > INT_MAX || trunc(value) != value)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Value is outside the integer range");
    }
    *number = (int)value;
    return DF_SET_SUCCESS();
}

static df_error_code_t parse_float_json_val(yyjson_val *json_val, float *number)
{
    CHECK_JSON_AND_OUT_VAL(json_val, number);
    if (!yyjson_is_num(json_val))
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Value is not a number");
    }
    double value = yyjson_get_num(json_val);
    if (!isfinite(value) || value < -FLT_MAX || value > FLT_MAX)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Value is outside the finite float range");
    }
    *number = (float)value;
    return DF_SET_SUCCESS();
}
#define PARSE_BOOL_JSON_VAL(json_val, bool_val)                                                                        \
    CHECK_JSON_AND_OUT_VAL(json_val, bool_val);                                                                        \
    if (unsafe_yyjson_is_bool(json_val))                                                                               \
    {                                                                                                                  \
        *bool_val = unsafe_yyjson_get_bool(json_val);                                                                  \
    }                                                                                                                  \
    else if (unsafe_yyjson_is_num(json_val))                                                                           \
    {                                                                                                                  \
        *bool_val = unsafe_yyjson_get_num(json_val) != 0;                                                         \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, #bool_val " is not a boolean or number");                           \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

// TODO: 解析尽可能保持兼容性，如遇数据错误，交给 validation 做判断。但有些格式错误需要在解析时就处理掉。
df_error_code_t df_parse_runtime_info_json_str(const char *json_str, df_runtime_info_t *runtime_info)
{
    if (!json_str)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "JSON string is NULL");
    }
    if (!runtime_info)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "Invalid runtime info");
    }

    yyjson_read_err  err;
    yyjson_read_flag flags = YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_BOM |
                             YYJSON_READ_ALLOW_SINGLE_QUOTED_STR | YYJSON_READ_ALLOW_UNQUOTED_KEY;
    yyjson_doc *doc = yyjson_read_opts((char *)json_str, strlen(json_str), flags, NULL, &err);
    if (!doc)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON,
                            "Failed to read runtime info JSON string: read error: %s, code: %u at byte position: %lu\n",
                            err.msg, err.code, err.pos);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root))
    {
        DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Failed to get root runtime info JSON value");
        goto end;
    }

    // Get root["config"]
    if (df_parse_config_json_val(df_json_get_member(root, "config"), &runtime_info->config) != DF_SUCCESS)
    {
        goto end;
    }

    // Get root["input_files"]
    if (df_parse_input_files_json_val(df_json_get_member(root, "input_files"), &runtime_info->input_files) != DF_SUCCESS)
    {
        goto end;
    }

    // Get root["output_file"]
    if (df_parse_output_file_json_val(df_json_get_member(root, "output_file"), &runtime_info->output_file) != DF_SUCCESS)
    {
        goto end;
    }

    DF_SET_SUCCESS(); // 清空状态

end:
    yyjson_doc_free(doc);
    return df_get_last_error_code();
}

df_error_code_t df_parse_config_json_val(yyjson_val *json_val, df_config_t *config)
{
    CHECK_JSON_AND_OUT_VAL(json_val, config);

    if (!yyjson_is_obj(json_val))
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Config must be an object");
    }
    const char *sections[] = {"screen", "danmaku", "text", "msgbox"};
    for (size_t i = 0; i < ARRAY_SIZE(sections); i++)
    {
        yyjson_val *section = df_json_get_member(json_val, sections[i]);
        if (section != NULL && !yyjson_is_obj(section))
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Config section %s must be an object", sections[i]);
        }
    }

#define PARSE_CONFIG_JSON_VAL(section, field, out_val)                                                                 \
    if (df_parse_##section##_##field##_json_val(json_get_config_field(json_val, #section, #field), out_val) != DF_SUCCESS) \
    {                                                                                                                  \
        return df_get_last_error_code();                                                                               \
    }

    // 解析屏幕设置
    PARSE_CONFIG_JSON_VAL(screen, resolution, &config->screen.resolution);
    if (json_get_config_field(json_val, "screen", "resolution") == NULL)
    {
        if (parse_int_json_val(df_json_get_member(json_val, "resx"), &config->screen.resolution.x) != DF_SUCCESS ||
            parse_int_json_val(df_json_get_member(json_val, "resy"), &config->screen.resolution.y) != DF_SUCCESS)
        {
            return df_get_last_error_code();
        }
    }
    PARSE_CONFIG_JSON_VAL(screen, display_area, &config->screen.display_area);
    PARSE_CONFIG_JSON_VAL(screen, scroll_area, &config->screen.scroll_area);
    PARSE_CONFIG_JSON_VAL(screen, statistics_mode, &config->screen.statistics_mode);

    // 解析弹幕设置
    PARSE_CONFIG_JSON_VAL(danmaku, density, &config->danmaku.density);
    PARSE_CONFIG_JSON_VAL(danmaku, scroll_duration, &config->danmaku.scroll_duration);
    PARSE_CONFIG_JSON_VAL(danmaku, fixed_duration, &config->danmaku.fixed_duration);
    PARSE_CONFIG_JSON_VAL(danmaku, show_username, &config->danmaku.show_username);
    PARSE_CONFIG_JSON_VAL(danmaku, block_mode, &config->danmaku.block_mode);
    PARSE_CONFIG_JSON_VAL(danmaku, keep_blocked, &config->danmaku.keep_blocked);
    PARSE_CONFIG_JSON_VAL(danmaku, block_list, &config->danmaku.block_list);
    PARSE_CONFIG_JSON_VAL(danmaku, block_list_regex_enabled, &config->danmaku.block_list_regex_enabled);

    // 解析文本设置
    PARSE_CONFIG_JSON_VAL(text, fontname, config->text.fontname);
    PARSE_CONFIG_JSON_VAL(text, font_size, &config->text.font_size);
    PARSE_CONFIG_JSON_VAL(text, font_size_strict, &config->text.font_size_strict);
    PARSE_CONFIG_JSON_VAL(text, font_size_norm, &config->text.font_size_norm);
    PARSE_CONFIG_JSON_VAL(text, bold, &config->text.bold);
    PARSE_CONFIG_JSON_VAL(text, shadow, &config->text.shadow);
    PARSE_CONFIG_JSON_VAL(text, opacity, &config->text.opacity);
    PARSE_CONFIG_JSON_VAL(text, outline, &config->text.outline);
    PARSE_CONFIG_JSON_VAL(text, outline_blur, &config->text.outline_blur);
    PARSE_CONFIG_JSON_VAL(text, outline_opacity, &config->text.outline_opacity);
    PARSE_CONFIG_JSON_VAL(text, line_spacing, &config->text.line_spacing);

    // 解析消息框设置
    PARSE_CONFIG_JSON_VAL(msgbox, show, &config->msgbox.show);
    PARSE_CONFIG_JSON_VAL(msgbox, size, &config->msgbox.size);
    PARSE_CONFIG_JSON_VAL(msgbox, pos, &config->msgbox.pos);
    PARSE_CONFIG_JSON_VAL(msgbox, font_size, &config->msgbox.font_size);
    PARSE_CONFIG_JSON_VAL(msgbox, duration, &config->msgbox.duration);
    PARSE_CONFIG_JSON_VAL(msgbox, gift_min_price, &config->msgbox.gift_min_price);

#undef PARSE_CONFIG_JSON_VAL

    return DF_SET_SUCCESS();
}

df_error_code_t df_parse_input_files_json_val(yyjson_val *json_val, df_fileinfos_t *input_files)
{
    CHECK_JSON_AND_OUT_VAL(json_val, input_files);
    if (!yyjson_is_arr(json_val))
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Input files must be an array");
    }

    size_t number = yyjson_arr_size(json_val);
    if (number == 0 || number > SIZE_MAX / sizeof(df_fileinfo_t))
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Input file array is empty or too large");
    }

    df_fileinfo_t *fileinfos = calloc(number, sizeof(*fileinfos));
    if (fileinfos == NULL)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Failed to allocate memory for input file infos");
    }

    size_t index, size;
    yyjson_val *item;
    yyjson_arr_foreach(json_val, index, size, item)
    {
        df_error_code_t result = df_parse_fileinfo_json_val(item, &fileinfos[index]);
        if (result != DF_SUCCESS)
        {
            free(fileinfos);
            return result;
        }
    }

    free(input_files->fileinfos);
    input_files->number = number;
    input_files->fileinfos = fileinfos;
    return DF_SET_SUCCESS();
}
df_error_code_t df_parse_output_file_json_val(yyjson_val *json_val, df_fileinfo_t *output_file)
{
    return df_parse_fileinfo_json_val(json_val, output_file);
}

/*==============================================================================
 * MARK: - Screen Config
 *============================================================================*/

df_error_code_t df_parse_screen_resolution_json_val(yyjson_val *json_val, df_coord_t *resolution)
{
    return df_parse_coordinate_json_val(json_val, resolution);
}

df_error_code_t df_parse_screen_display_area_json_val(yyjson_val *json_val, float *display_area)
{
    return parse_float_json_val(json_val, display_area);
}

df_error_code_t df_parse_screen_scroll_area_json_val(yyjson_val *json_val, float *scroll_area)
{
    return parse_float_json_val(json_val, scroll_area);
}

df_error_code_t df_parse_screen_statistics_mode_json_val(yyjson_val *json_val, df_stats_flag_t *statistics_mode)
{
    // 定义静态的映射表
    static const flag_mapping_t statistics_mode_mappings[] = {
        {"NONE", DF_STATISTICS_MODE_NONE},
        {"TABLE", DF_STATISTICS_MODE_TABLE},
        {"HISTOGRAM", DF_STATISTICS_MODE_HISTOGRAM},
    };

    CHECK_JSON_AND_OUT_VAL(json_val, statistics_mode);
    uint64_t mode = 0;
    df_error_code_t result =
        df_parse_mode_json_val(json_val, statistics_mode_mappings, ARRAY_SIZE(statistics_mode_mappings), &mode);
    if (result != DF_SUCCESS)
    {
        return result;
    }
    if (mode > UINT8_MAX)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Statistics mode is outside the flag range");
    }
    *statistics_mode = (df_stats_flag_t)mode;
    return DF_SET_SUCCESS();
}

/*==============================================================================
 * MARK: - Danmaku Config
 *============================================================================*/

df_error_code_t df_parse_danmaku_density_json_val(yyjson_val *json_val, int *density)
{
    return parse_int_json_val(json_val, density);
}

df_error_code_t df_parse_danmaku_scroll_duration_json_val(yyjson_val *json_val, float *scroll_duration)
{
    return parse_float_json_val(json_val, scroll_duration);
}

df_error_code_t df_parse_danmaku_fixed_duration_json_val(yyjson_val *json_val, float *fixed_duration)
{
    return parse_float_json_val(json_val, fixed_duration);
}

df_error_code_t df_parse_danmaku_show_username_json_val(yyjson_val *json_val, bool *show_username)
{
    PARSE_BOOL_JSON_VAL(json_val, show_username);
}

df_error_code_t df_parse_danmaku_block_mode_json_val(yyjson_val *json_val, df_block_flag_t *block_mode)
{
    // 定义静态的映射表
    static const flag_mapping_t block_mode_mappings[] = {
        {"NONE", DF_BLOCK_MODE_NONE},   {"R2L", DF_BLOCK_MODE_R2L},       {"L2R", DF_BLOCK_MODE_L2R},
        {"TOP", DF_BLOCK_MODE_TOP},     {"BOTTOM", DF_BLOCK_MODE_BOTTOM}, {"SPECIAL", DF_BLOCK_MODE_SPECIAL},
        {"COLOR", DF_BLOCK_MODE_COLOR}, {"COLOUR", DF_BLOCK_MODE_COLOR},  {"REPEAT", DF_BLOCK_MODE_REPEAT},
    };

    CHECK_JSON_AND_OUT_VAL(json_val, block_mode);
    uint64_t mode = 0;
    df_error_code_t result =
        df_parse_mode_json_val(json_val, block_mode_mappings, ARRAY_SIZE(block_mode_mappings), &mode);
    if (result != DF_SUCCESS)
    {
        return result;
    }
    if (mode > UINT8_MAX)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Block mode is outside the flag range");
    }
    *block_mode = (df_block_flag_t)mode;
    return DF_SET_SUCCESS();
}

df_error_code_t df_parse_danmaku_keep_blocked_json_val(yyjson_val *json_val, bool *keep_blocked)
{
    PARSE_BOOL_JSON_VAL(json_val, keep_blocked);
}

df_error_code_t df_parse_danmaku_block_list_json_val(yyjson_val *json_val, char ***block_list)
{
    CHECK_JSON_AND_OUT_VAL(json_val, block_list);

    if (unsafe_yyjson_is_str(json_val))
    {
        // 读取黑名单文件
        return read_file_lines(unsafe_yyjson_get_str(json_val), block_list);
    }
    else if (unsafe_yyjson_is_arr(json_val))
    {
        // 读取黑名单数组
        return df_parse_string_array_json_val(json_val, block_list);
    }
    else
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Blacklist file path is not a string or array");
    }

    return DF_SET_SUCCESS();
}

df_error_code_t df_parse_danmaku_block_list_regex_enabled_json_val(yyjson_val *json_val, bool *block_list_regex_enabled)
{
    PARSE_BOOL_JSON_VAL(json_val, block_list_regex_enabled);
}

/*==============================================================================
 * MARK: - Text Config
 *============================================================================*/

df_error_code_t df_parse_text_fontname_json_val(yyjson_val *json_val, char *fontname)
{
    CHECK_JSON_AND_OUT_VAL(json_val, fontname);

    if (unsafe_yyjson_is_str(json_val))
    {
        return str_copy_ns(fontname, FONTNAME_LEN, unsafe_yyjson_get_str(json_val), unsafe_yyjson_get_len(json_val));
    }
    else
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Font name is not a string");
    }

    return DF_SET_SUCCESS();
}

df_error_code_t df_parse_text_font_size_json_val(yyjson_val *json_val, int *font_size)
{
    return parse_int_json_val(json_val, font_size);
}

df_error_code_t df_parse_text_font_size_strict_json_val(yyjson_val *json_val, bool *font_size_strict)
{
    PARSE_BOOL_JSON_VAL(json_val, font_size_strict);
}

df_error_code_t df_parse_text_font_size_norm_json_val(yyjson_val *json_val, bool *font_size_norm)
{
    PARSE_BOOL_JSON_VAL(json_val, font_size_norm);
}

df_error_code_t df_parse_text_bold_json_val(yyjson_val *json_val, bool *bold)
{
    PARSE_BOOL_JSON_VAL(json_val, bold);
}

df_error_code_t df_parse_text_shadow_json_val(yyjson_val *json_val, float *shadow)
{
    return parse_float_json_val(json_val, shadow);
}

df_error_code_t df_parse_text_opacity_json_val(yyjson_val *json_val, int *opacity)
{
    return parse_int_json_val(json_val, opacity);
}

df_error_code_t df_parse_text_outline_json_val(yyjson_val *json_val, float *outline)
{
    return parse_float_json_val(json_val, outline);
}

df_error_code_t df_parse_text_outline_blur_json_val(yyjson_val *json_val, float *outline_blur)
{
    return parse_float_json_val(json_val, outline_blur);
}

df_error_code_t df_parse_text_outline_opacity_json_val(yyjson_val *json_val, int *outline_opacity)
{
    return parse_int_json_val(json_val, outline_opacity);
}

df_error_code_t df_parse_text_line_spacing_json_val(yyjson_val *json_val, int *line_spacing)
{
    return parse_int_json_val(json_val, line_spacing);
}

/*==============================================================================
 * MARK: - Msgbox Config
 *============================================================================*/

df_error_code_t df_parse_msgbox_show_json_val(yyjson_val *json_val, bool *show)
{
    PARSE_BOOL_JSON_VAL(json_val, show);
}

df_error_code_t df_parse_msgbox_size_json_val(yyjson_val *json_val, df_coord_t *size)
{
    return df_parse_coordinate_json_val(json_val, size);
}

df_error_code_t df_parse_msgbox_pos_json_val(yyjson_val *json_val, df_coord_t *pos)
{
    return df_parse_coordinate_json_val(json_val, pos);
}

df_error_code_t df_parse_msgbox_font_size_json_val(yyjson_val *json_val, int *font_size)
{
    return parse_int_json_val(json_val, font_size);
}

df_error_code_t df_parse_msgbox_duration_json_val(yyjson_val *json_val, float *duration)
{
    return parse_float_json_val(json_val, duration);
}

df_error_code_t df_parse_msgbox_gift_min_price_json_val(yyjson_val *json_val, float *gift_min_price)
{
    return parse_float_json_val(json_val, gift_min_price);
}

/*==============================================================================
 * MARK: - Parsing Utils
 *============================================================================*/

df_error_code_t df_parse_coordinate_json_val(yyjson_val *json_val, df_coord_t *coordinate)
{
    CHECK_JSON_AND_OUT_VAL(json_val, coordinate);

    if (unsafe_yyjson_is_str(json_val))
    {
        const char *coord_str = unsafe_yyjson_get_str(json_val);
        char *end;
        errno = 0;
        long x = strtol(coord_str, &end, 10);
        if (end == coord_str || errno == ERANGE || x < INT_MIN || x > INT_MAX)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Invalid coordinate format");
        }
        const char *second = end;
        while (*second != '\0' && !isdigit((unsigned char)*second) && *second != '-' && *second != '+')
        {
            second++;
        }
        if (second == end)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Missing coordinate separator");
        }
        errno = 0;
        long y = strtol(second, &end, 10);
        if (end == second || errno == ERANGE || y < INT_MIN || y > INT_MAX)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Invalid coordinate format");
        }
        while (isspace((unsigned char)*end))
        {
            end++;
        }
        if (*end != '\0')
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Invalid coordinate suffix");
        }
        coordinate->x = (int)x;
        coordinate->y = (int)y;
    }
    else if (unsafe_yyjson_is_arr(json_val) && unsafe_yyjson_arr_is_flat(json_val))
    {
        if (unsafe_yyjson_get_len(json_val) != 2)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Invalid coordinate array length");
        }

        yyjson_val *x_val = unsafe_yyjson_get_first(json_val);
        yyjson_val *y_val = x_val + 1;

        df_coord_t parsed;
        if (parse_int_json_val(x_val, &parsed.x) != DF_SUCCESS ||
            parse_int_json_val(y_val, &parsed.y) != DF_SUCCESS)
        {
            return df_get_last_error_code();
        }
        *coordinate = parsed;
    }
    else
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Coordinate is not a string or flat array");
    }

    return DF_SET_SUCCESS();
}

/**
 * @brief 解析 JSON 中的模式值
 * @param json_val JSON 值
 * @param flag_mappings 标志映射数组
 * @param mappings_num 标志映射数组长度
 * @param mode 输出的模式值
 * @return df_error_code_t 错误码
 */
df_error_code_t df_parse_mode_json_val(yyjson_val *json_val, const flag_mapping_t *flag_mappings,
                                       const size_t mappings_num, uint64_t *mode)
{
    CHECK_JSON_AND_OUT_VAL(json_val, mode);

    uint64_t flag;
    uint64_t parsed_mode = 0;

    if (unsafe_yyjson_is_num(json_val))
    {
        double value = yyjson_get_num(json_val);
        if (!isfinite(value) || value < 0 || value >= 18446744073709551616.0 || trunc(value) != value)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Mode is outside the unsigned integer range");
        }
        parsed_mode = yyjson_is_uint(json_val) ? yyjson_get_uint(json_val) : (uint64_t)value;
    }
    else if (unsafe_yyjson_is_str(json_val))
    {
        if (get_flag_type(unsafe_yyjson_get_str(json_val), flag_mappings, mappings_num, &flag) != DF_SUCCESS)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Invalid mode flag string");
        }

        parsed_mode = flag;
    }
    else if (unsafe_yyjson_is_arr(json_val))
    {
        size_t      idx, size;
        yyjson_val *item;
        yyjson_arr_foreach(json_val, idx, size, item)
        {
            if (!unsafe_yyjson_is_str(item))
            {
                return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Flag array items must be strings");
            }

            if (get_flag_type(unsafe_yyjson_get_str(item), flag_mappings, mappings_num, &flag) != DF_SUCCESS)
            {
                return DF_SET_ERROR(DF_ERROR_INVALID_VALUE, "Invalid mode flag string");
            }

            parsed_mode |= flag;
        }
    }
    else
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "Mode is not a string or number or array");
    }

    *mode = parsed_mode;
    return DF_SET_SUCCESS();
}

df_error_code_t df_parse_string_array_json_val(yyjson_val *json_val, char ***string_array)
{
    CHECK_JSON_AND_OUT_VAL(json_val, string_array);
    if (!yyjson_is_arr(json_val))
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "String list must be an array");
    }
    size_t idx = 0;
    size_t size = yyjson_arr_size(json_val);
    if (size > SIZE_MAX / sizeof(char *) - 1)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "String array is too large");
    }

    size_t count  = 0;
    char **tokens = malloc((size + 1) * sizeof(char *));
    if (!tokens)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Failed to allocate memory for string array");
    }

    for (yyjson_val *item = unsafe_yyjson_get_first(json_val); idx++ < size; item = unsafe_yyjson_get_next(item))
    {
        if (unsafe_yyjson_is_str(item))
        {
            if (unsafe_yyjson_get_len(item) == 0)
            {
                continue;
            }
            tokens[count] = df_strdup(unsafe_yyjson_get_str(item));
            if (!tokens[count])
            {
                // OOM：释放已分配的条目
                for (size_t j = 0; j < count; j++)
                {
                    free(tokens[j]);
                }
                free(tokens);
                return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Failed to duplicate string array item");
            }
            count++;
        }
    }

    // 调整到实际大小
    if (count != size)
    {
        char **final_tokens = realloc(tokens, (count + 1) * sizeof(char *));
        if (final_tokens)
        {
            tokens = final_tokens;
        }
    }

    tokens[count] = NULL;
    *string_array = tokens;

    return DF_SET_SUCCESS();
}

// TODO: 解析尽可能保持兼容性，如遇数据错误，交给 validation 做判断。但有些格式错误需要在解析时就处理掉。
df_error_code_t df_parse_fileinfo_json_val(yyjson_val *json_val, df_fileinfo_t *fileinfo)
{
    CHECK_JSON_AND_OUT_VAL(json_val, fileinfo);

    yyjson_val *path_val = json_val;

    if (unsafe_yyjson_is_obj(json_val))
    {
        path_val = df_json_get_member(json_val, "path");
    }

    const char *file_path = yyjson_get_str(path_val);
    if (file_path == NULL || yyjson_get_len(path_val) == 0 ||
        strlen(file_path) != yyjson_get_len(path_val))
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "File path must be a nonempty string without NUL characters");
    }

    // 设置文件信息
    fileinfo->time_shift = 0;
    if (parse_float_json_val(df_json_get_member(json_val, "time_shift"), &fileinfo->time_shift) != DF_SUCCESS)
    {
        return df_get_last_error_code();
    }
    fileinfo->type       = get_filetype(file_path);

    // 显式文件类型（可选，对应 df_fileinfo_t.type，覆盖扩展名推断）
    yyjson_val *type_val = df_json_get_member(json_val, "type");
    if (type_val)
    {
        const char *type_str = yyjson_get_str(type_val);
        if (!type_str)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_JSON, "fileinfo \"type\" must be a string");
        }

        df_filetype_t explicit_type = get_filetype_by_name(type_str);
        if (explicit_type != DF_FILETYPE_ASS && explicit_type != DF_FILETYPE_JSON && explicit_type != DF_FILETYPE_XML)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_JSON,
                                "fileinfo \"type\" is invalid: \"%s\" (available: xml, json, ass)", type_str);
        }
        fileinfo->type = explicit_type;
    }

    return str_copy_ns(fileinfo->path, sizeof(fileinfo->path), file_path, yyjson_get_len(path_val));
}

#undef PARSE_BOOL_JSON_VAL
#undef CHECK_JSON_AND_OUT_VAL
