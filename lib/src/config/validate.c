/* MIT License
 *
 * Copyright (c) 2025 lengyanyu258
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

#include "config/validate.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "base/error.h"
#include "config/define.h"
#include "danmakufactory.h"

#define FLOAT_EQUAL(a, b)            (fabs(a - b) < EPS)
#define FLOAT_GREATER_OR_EQUAL(a, b) (a > b - EPS)

#define VALIDATE_FLOAT_RANGE_INCLUDE(number, left_endpoint, right_endpoint)                                            \
    if ((number < left_endpoint - EPS) || (number > right_endpoint + EPS))                                             \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "`" #number "` must be a real number in range [" #left_endpoint   \
                                                     ", " #right_endpoint "].");                                       \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

#define VALIDATE_FLOAT_RANGE_INCLUDE_RIGHT_ENDPOINT(number, left_endpoint, right_endpoint)                             \
    if ((number < left_endpoint + EPS) || (number > right_endpoint + EPS))                                             \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "`" #number "` must be a real number in range (" #left_endpoint   \
                                                     ", " #right_endpoint "].");                                       \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

#define VALIDATE_FLOAT_GREATER(number, lower_bound)                                                                    \
    if (number < lower_bound + EPS)                                                                                    \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG,                                                                   \
                            "`" #number "` must be a real number greater than " #lower_bound ".");                     \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

#define VALIDATE_FLOAT_GREATER_OR_EQUAL(number, minimum)                                                               \
    if (!FLOAT_GREATER_OR_EQUAL(number, minimum))                                                                      \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG,                                                                   \
                            "`" #number "` must be a real number greater than or equal to " #minimum ".");             \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

#define VALIDATE_INT_RANGE_INCLUDE(number, left_endpoint, right_endpoint)                                              \
    if ((number < left_endpoint) || (number > right_endpoint))                                                         \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG,                                                                   \
                            "`" #number "` must be an integer in range [" #left_endpoint ", " #right_endpoint "].");   \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

#define VALIDATE_INT_RANGE_INCLUDE_RIGHT_ENDPOINT(number, left_endpoint, right_endpoint)                               \
    if ((number <= left_endpoint) || (number > right_endpoint))                                                        \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG,                                                                   \
                            "`" #number "` must be an integer in range (" #left_endpoint ", " #right_endpoint "].");   \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

#define VALIDATE_INT_GREATER(number, lower_bound)                                                                      \
    if (number <= lower_bound)                                                                                         \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG,                                                                   \
                            "`" #number "` must be an integer greater than " #lower_bound ".");                        \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

#define VALIDATE_INT_GREATER_OR_EQUAL(number, minimum)                                                                 \
    if (number < minimum)                                                                                              \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG,                                                                   \
                            "`" #number "` must be an integer greater than or equal to " #minimum ".");                \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

#define VALIDATE_MODE_FLAGS(mode, valid_flags)                                                                         \
    if (((mode) & ~(valid_flags)) != 0)                                                                                \
    {                                                                                                                  \
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "`" #mode "` contains invalid flags.");                           \
    }                                                                                                                  \
    return DF_SET_SUCCESS()

df_error_code_t df_validate_runtime_info(const df_runtime_info_t *runtime_info)
{
    if (!runtime_info)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_RUNTIME_INFO, "runtime_info is NULL");
    }
    if (df_validate_config(&runtime_info->config) != DF_SUCCESS)
    {
        return df_get_last_error_code();
    }
    if (df_validate_input_files(&runtime_info->input_files) != DF_SUCCESS)
    {
        return df_get_last_error_code();
    }
    if (df_validate_output_file(&runtime_info->output_file) != DF_SUCCESS)
    {
        return df_get_last_error_code();
    }

    return DF_SET_SUCCESS();
}

df_error_code_t df_validate_config(const df_config_t *config)
{
    if (!config)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "config is NULL");
    }

#define VALIDATE_CONFIG(section, field, value)                                                                         \
    if (df_validate_##section##_##field(value) != DF_SUCCESS)                                                          \
    {                                                                                                                  \
        return df_get_last_error_code();                                                                               \
    }

    // 验证屏幕设置
    VALIDATE_CONFIG(screen, resolution, &config->screen.resolution);
    VALIDATE_CONFIG(screen, display_area, config->screen.display_area);
    VALIDATE_CONFIG(screen, scroll_area, config->screen.scroll_area);
    VALIDATE_CONFIG(screen, statistics_mode, config->screen.statistics_mode);

    // 验证弹幕设置
    VALIDATE_CONFIG(danmaku, density, config->danmaku.density);
    VALIDATE_CONFIG(danmaku, scroll_duration, config->danmaku.scroll_duration);
    VALIDATE_CONFIG(danmaku, fixed_duration, config->danmaku.fixed_duration);
    VALIDATE_CONFIG(danmaku, block_mode, config->danmaku.block_mode);
    VALIDATE_CONFIG(danmaku, block_list, config->danmaku.block_list);

    // 验证文本设置
    VALIDATE_CONFIG(text, fontname, config->text.fontname);
    VALIDATE_CONFIG(text, font_size, config->text.font_size);
    VALIDATE_CONFIG(text, shadow, config->text.shadow);
    VALIDATE_CONFIG(text, opacity, config->text.opacity);
    VALIDATE_CONFIG(text, outline, config->text.outline);
    VALIDATE_CONFIG(text, outline_blur, config->text.outline_blur);
    VALIDATE_CONFIG(text, outline_opacity, config->text.outline_opacity);
    VALIDATE_CONFIG(text, line_spacing, config->text.line_spacing);

    // 验证消息框设置
    VALIDATE_CONFIG(msgbox, size, &config->msgbox.size);
    VALIDATE_CONFIG(msgbox, pos, &config->msgbox.pos);
    VALIDATE_CONFIG(msgbox, font_size, config->msgbox.font_size);
    VALIDATE_CONFIG(msgbox, duration, config->msgbox.duration);
    VALIDATE_CONFIG(msgbox, gift_min_price, config->msgbox.gift_min_price);

#undef VALIDATE_CONFIG

    return DF_SET_SUCCESS();
}

df_error_code_t df_validate_input_files(const df_fileinfos_t *input_files)
{
    if (!input_files)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "input_files is NULL");
    }

    if (input_files->number <= 0)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "input_files is empty");
    }

    for (size_t i = 0; i < input_files->number; i++)
    {
        if (input_files->fileinfos[i].type == DF_FILETYPE_UNKNOWN)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "input file %zu type is unknown: %s", i,
                                input_files->fileinfos[i].path);
        }

        if (input_files->fileinfos[i].type == DF_FILETYPE_NONE)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "input file %zu type is none", i);
        }
    }

    return DF_SET_SUCCESS();
}

df_error_code_t df_validate_output_file(const df_fileinfo_t *output_file)
{
    if (!output_file)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "output_file is NULL");
    }

    if (output_file->type == DF_FILETYPE_UNKNOWN)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "output file type is unknown: %s", output_file->path);
    }

    return DF_SET_SUCCESS();
}

/*==============================================================================
 * MARK: - Screen Config
 *============================================================================*/

/* 分辨率 */
df_error_code_t df_validate_screen_resolution(const df_coord_t *resolution)
{
    if (!resolution)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "resolution is NULL");
    }
    if (resolution->x <= 0 || resolution->y <= 0)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "\"Resolution\" must be an integer greater than 0.");
    }

    return DF_SET_SUCCESS();
}

/* 显示区域 */
df_error_code_t df_validate_screen_display_area(const float display_area)
{
    VALIDATE_FLOAT_RANGE_INCLUDE_RIGHT_ENDPOINT(display_area, 0.0, 1.0);
}

/* 滚动区域 */
df_error_code_t df_validate_screen_scroll_area(const float scroll_area)
{
    VALIDATE_FLOAT_RANGE_INCLUDE_RIGHT_ENDPOINT(scroll_area, 0.0, 1.0);
}

df_error_code_t df_validate_screen_statistics_mode(const df_stats_flag_t statistics_mode)
{
#define X(name, value) name |
    VALIDATE_MODE_FLAGS(statistics_mode, DF_STATISTICS_MODE_ENUM_ITEMS 0);
#undef X
}

/*==============================================================================
 * MARK: - Danmaku Config
 *============================================================================*/

/* 密度 */
df_error_code_t df_validate_danmaku_density(const int density)
{
    VALIDATE_INT_GREATER_OR_EQUAL(density, -1);
}

/* 滚动时间 */
df_error_code_t df_validate_danmaku_scroll_duration(const float scroll_duration)
{
    VALIDATE_FLOAT_GREATER(scroll_duration, 0.0);
}

/* 固定时间 */
df_error_code_t df_validate_danmaku_fixed_duration(const float fixed_duration)
{
    VALIDATE_FLOAT_GREATER(fixed_duration, 0.0);
}

df_error_code_t df_validate_danmaku_block_mode(const df_block_flag_t block_mode)
{
#define X(name, value) name |
    VALIDATE_MODE_FLAGS(block_mode, DF_BLOCK_MODE_ENUM_ITEMS 0);
#undef X
}

df_error_code_t df_validate_danmaku_block_list(char **const block_list)
{
    //     if (!path || !*path)
    //         return true; // 空路径有效

    //     struct stat st;
    //     if (stat(path, &st) != 0)
    //         return false; // 文件不存在
    //     if (!S_ISREG(st.st_mode))
    //         return false; // 不是普通文件
    //     if (access(path, R_OK) != 0)
    //         return false; // 无读权限

    return DF_SET_SUCCESS();
}

/*==============================================================================
 * MARK: - Text Config
 *============================================================================*/

/* 字体 */
df_error_code_t df_validate_text_fontname(const char *fontname)
{
    if (!fontname)
    {
        return DF_SET_ERROR(DF_ERROR_INVALID_CONFIG, "fontname is NULL");
    }

    /* 验证是否全部为可打印的 ascii 字符 */
    for (const char *temp_ptr = fontname; *temp_ptr != '\0'; temp_ptr++)
    {
        /* ascii 非可打印字符范围 */
        if (*temp_ptr < 0x20 || *temp_ptr > 0x7e)
        {
            return DF_SET_ERROR(
                DF_WARNING_INVALID_CONFIG,
                "Some characters of fontname are non-ASCII characters, which may cause the garbled problem.");
        }
    }

    return DF_SET_SUCCESS();
}

/* 字号 */
df_error_code_t df_validate_text_font_size(const int font_size)
{
    VALIDATE_INT_GREATER(font_size, 0);
}

/* 阴影 */
df_error_code_t df_validate_text_shadow(const float shadow)
{
    VALIDATE_FLOAT_RANGE_INCLUDE(shadow, 0.0, 4.0);
}

/* 不透明度 */
df_error_code_t df_validate_text_opacity(const int opacity)
{
    VALIDATE_INT_RANGE_INCLUDE_RIGHT_ENDPOINT(opacity, 0, 255);
}

/* 描边 */
df_error_code_t df_validate_text_outline(const float outline)
{
    VALIDATE_FLOAT_RANGE_INCLUDE(outline, 0.0, 4.0);
}

/* 描边模糊半径 */
df_error_code_t df_validate_text_outline_blur(const float outline_blur)
{
    VALIDATE_FLOAT_GREATER_OR_EQUAL(outline_blur, 0.0);

    // if (FLOAT_EQUAL(config->text.outline, 0.0) && FLOAT_GREATER_OR_EQUAL(config->text.outline_blur, 0.0))
    // {
    //     return DF_SET_ERROR(DF_WARNING_INVALID_CONFIG, "\"OutlineBlur\" is ignored since \"Outline\" is set to 0.");
    //     // toPause(ignoreWarnings);
    // }
}

/* 描边不透明度 */
df_error_code_t df_validate_text_outline_opacity(const int outline_opacity)
{
    VALIDATE_INT_RANGE_INCLUDE(outline_opacity, 0, 255);
}

df_error_code_t df_validate_text_line_spacing(const int line_spacing)
{
    // PARSE_NUM_JSON_VAL(json_val, line_spacing);

    return DF_SET_SUCCESS();
}

/*==============================================================================
 * MARK: - Msgbox Config
 *============================================================================*/

/* 消息框大小 */
df_error_code_t df_validate_msgbox_size(const df_coord_t *size)
{

    // if (config->size.x <= config->font_size || config->size.y <= config->font_size)
    // {
    //     fprintf(stderr, "\nERROR"
    //                     "\n\"MsgBoxSize\" must be an integer greater than the \"font_size\".\n");
    //     return false; // 消息框大小验证失败
    // }

    return DF_SET_SUCCESS();
}

df_error_code_t df_validate_msgbox_pos(const df_coord_t *pos)
{
    // return df_validate_coordinate_json_val(json_val, pos);
    return DF_SET_SUCCESS();
}

/* 消息框文字大小 */
df_error_code_t df_validate_msgbox_font_size(const int font_size)
{
    VALIDATE_INT_GREATER(font_size, 0);
}

df_error_code_t df_validate_msgbox_duration(const float duration)
{
    // PARSE_NUM_JSON_VAL(json_val, duration);
    return DF_SET_SUCCESS();
}

df_error_code_t df_validate_msgbox_gift_min_price(const float gift_min_price)
{
    // PARSE_NUM_JSON_VAL(json_val, gift_min_price);
    return DF_SET_SUCCESS();
}
