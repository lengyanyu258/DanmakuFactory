/* MIT License
 *
 * Copyright (c) 2022 hkm
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

#include "config/file.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

#include "base/string.h"
#include "base/utils.h"

/* 从 JSON 数组 [x, y] 中获取坐标 */
static df_coord_t json_get_coord(yyjson_val *val)
{
    df_coord_t coord = {0, 0};
    if (!val)
    {
        return coord;
    }

    if (yyjson_is_arr(val) && yyjson_arr_size(val) == 2)
    {
        yyjson_val *x = yyjson_arr_get_first(val);
        yyjson_val *y = yyjson_arr_get_last(val);
        if (x && yyjson_is_num(x))
        {
            coord.x = (int)yyjson_get_num(x);
        }
        if (y && yyjson_is_num(y))
        {
            coord.y = (int)yyjson_get_num(y);
        }
    }
    return coord;
}

/* 从 JSON 数组中解析屏蔽模式标志 */
static df_block_flag_t json_get_block_mode(yyjson_val *val)
{
    df_block_flag_t mode = DF_BLOCK_MODE_NONE;
    if (!val)
    {
        return mode;
    }

    if (yyjson_is_num(val))
    {
        return (df_block_flag_t)yyjson_get_num(val);
    }

    if (!yyjson_is_arr(val))
    {
        return mode;
    }

    size_t      idx, size;
    yyjson_val *item;
    yyjson_arr_foreach(val, idx, size, item)
    {
        const char *str = yyjson_get_str(item);
        if (!str)
        {
            continue;
        }
        if (strcmp_ignore_case(str, "r2l") == 0)
        {
            mode |= DF_BLOCK_MODE_R2L;
        }
        else if (strcmp_ignore_case(str, "l2r") == 0)
        {
            mode |= DF_BLOCK_MODE_L2R;
        }
        else if (strcmp_ignore_case(str, "top") == 0)
        {
            mode |= DF_BLOCK_MODE_TOP;
        }
        else if (strcmp_ignore_case(str, "bottom") == 0)
        {
            mode |= DF_BLOCK_MODE_BOTTOM;
        }
        else if (strcmp_ignore_case(str, "special") == 0)
        {
            mode |= DF_BLOCK_MODE_SPECIAL;
        }
        else if (strcmp_ignore_case(str, "color") == 0 || strcmp_ignore_case(str, "colour") == 0)
        {
            mode |= DF_BLOCK_MODE_COLOR;
        }
        else if (strcmp_ignore_case(str, "repeat") == 0)
        {
            mode |= DF_BLOCK_MODE_REPEAT;
        }
    }
    return mode;
}

/* 从 JSON 数组中解析统计模式标志 */
static df_stats_flag_t json_get_statistics_mode(yyjson_val *val)
{
    df_stats_flag_t mode = DF_STATISTICS_MODE_NONE;
    if (!val)
    {
        return mode;
    }

    if (yyjson_is_num(val))
    {
        return (df_stats_flag_t)yyjson_get_num(val);
    }

    if (!yyjson_is_arr(val))
    {
        return mode;
    }

    size_t      idx, size;
    yyjson_val *item;
    yyjson_arr_foreach(val, idx, size, item)
    {
        const char *str = yyjson_get_str(item);
        if (!str)
        {
            continue;
        }
        if (strcmp_ignore_case(str, "table") == 0)
        {
            mode |= DF_STATISTICS_MODE_TABLE;
        }
        else if (strcmp_ignore_case(str, "histogram") == 0)
        {
            mode |= DF_STATISTICS_MODE_HISTOGRAM;
        }
    }
    return mode;
}

/*
 * 从文件读配置文件（使用 yyjson 解析）
 * 参数:配置文件名/默认配置
 * 返回值:配置结构体
 */
df_config_t readConfig(const char *const configFileName, const df_config_t defaultConfig)
{
    df_config_t config = defaultConfig;

    FILE *fptr = df_fopen(configFileName, "rb");
    if (fptr == NULL)
    {
        return config;
    }

    /* 读取整个文件 */
    fseek(fptr, 0, SEEK_END);
    long file_size = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(fptr);
        return config;
    }

    char *buf = (char *)malloc((size_t)file_size + 1);
    if (!buf)
    {
        fclose(fptr);
        return config;
    }

    size_t read_size = fread(buf, 1, (size_t)file_size, fptr);
    fclose(fptr);
    buf[read_size] = '\0';

    /* 使用 yyjson 解析（宽松模式，兼容旧格式） */
    yyjson_read_err  err;
    yyjson_read_flag flags = YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_BOM |
                             YYJSON_READ_ALLOW_SINGLE_QUOTED_STR | YYJSON_READ_ALLOW_UNQUOTED_KEY;
    yyjson_doc *doc = yyjson_read_opts(buf, read_size, flags, NULL, &err);
    free(buf);

    if (!doc)
    {
        return config;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root))
    {
        yyjson_doc_free(doc);
        return config;
    }

    yyjson_val *val;

    /* resolution → screen.resolution */
    val = yyjson_obj_get(root, "resolution");
    if (val)
    {
        config.screen.resolution = json_get_coord(val);
    }

    /* display_area / displayArea → screen.display_area */
    val = yyjson_obj_get(root, "display_area");
    if (!val)
    {
        val = yyjson_obj_get(root, "displayarea");
    }
    if (val && yyjson_is_num(val))
    {
        config.screen.display_area = (float)yyjson_get_num(val);
    }

    /* scroll_area / scrollArea → screen.scroll_area */
    val = yyjson_obj_get(root, "scroll_area");
    if (!val)
    {
        val = yyjson_obj_get(root, "scrollarea");
    }
    if (val && yyjson_is_num(val))
    {
        config.screen.scroll_area = (float)yyjson_get_num(val);
    }

    /* scroll_duration / scrolltime → danmaku.scroll_duration */
    val = yyjson_obj_get(root, "scroll_duration");
    if (!val)
    {
        val = yyjson_obj_get(root, "scrolltime");
    }
    if (val && yyjson_is_num(val))
    {
        config.danmaku.scroll_duration = (float)yyjson_get_num(val);
    }

    /* fixed_duration / fixtime → danmaku.fixed_duration */
    val = yyjson_obj_get(root, "fixed_duration");
    if (!val)
    {
        val = yyjson_obj_get(root, "fixtime");
    }
    if (val && yyjson_is_num(val))
    {
        config.danmaku.fixed_duration = (float)yyjson_get_num(val);
    }

    /* density → danmaku.density */
    val = yyjson_obj_get(root, "density");
    if (val && yyjson_is_num(val))
    {
        config.danmaku.density = (int)yyjson_get_num(val);
    }

    /* line_spacing / linespacing → text.line_spacing */
    val = yyjson_obj_get(root, "line_spacing");
    if (!val)
    {
        val = yyjson_obj_get(root, "linespacing");
    }
    if (val && yyjson_is_num(val))
    {
        config.text.line_spacing = (int)yyjson_get_num(val);
    }

    /* fontname → text.fontname */
    val = yyjson_obj_get(root, "fontname");
    if (val && yyjson_is_str(val))
    {
        str_copy_n(config.text.fontname, yyjson_get_str(val), FONTNAME_LEN);
    }

    /* font_size / fontsize → text.font_size */
    val = yyjson_obj_get(root, "font_size");
    if (!val)
    {
        val = yyjson_obj_get(root, "fontsize");
    }
    if (val && yyjson_is_num(val))
    {
        config.text.font_size = (int)yyjson_get_num(val);
    }

    /* opacity → text.opacity */
    val = yyjson_obj_get(root, "opacity");
    if (val && yyjson_is_num(val))
    {
        config.text.opacity = (int)yyjson_get_num(val);
    }

    /* outline → text.outline */
    val = yyjson_obj_get(root, "outline");
    if (val && yyjson_is_num(val))
    {
        config.text.outline = (float)yyjson_get_num(val);
    }

    /* shadow → text.shadow */
    val = yyjson_obj_get(root, "shadow");
    if (val && yyjson_is_num(val))
    {
        config.text.shadow = (float)yyjson_get_num(val);
    }

    /* bold → text.bold */
    val = yyjson_obj_get(root, "bold");
    if (val && yyjson_is_bool(val))
    {
        config.text.bold = yyjson_get_bool(val);
    }

    /* show_usernames / showUsernames → danmaku.show_username */
    val = yyjson_obj_get(root, "show_usernames");
    if (!val)
    {
        val = yyjson_obj_get(root, "showusernames");
    }
    if (val && yyjson_is_bool(val))
    {
        config.danmaku.show_username = yyjson_get_bool(val);
    }

    /* show_msgbox / showMsgbox → msgbox.show */
    val = yyjson_obj_get(root, "show_msgbox");
    if (!val)
    {
        val = yyjson_obj_get(root, "showmsgbox");
    }
    if (val && yyjson_is_bool(val))
    {
        config.msgbox.show = yyjson_get_bool(val);
    }

    /* msgbox_size / msgboxSize → msgbox.size */
    val = yyjson_obj_get(root, "msgbox_size");
    if (!val)
    {
        val = yyjson_obj_get(root, "msgboxsize");
    }
    if (val)
    {
        config.msgbox.size = json_get_coord(val);
    }

    /* msgbox_pos / msgboxPos → msgbox.pos */
    val = yyjson_obj_get(root, "msgbox_pos");
    if (!val)
    {
        val = yyjson_obj_get(root, "msgboxpos");
    }
    if (val)
    {
        config.msgbox.pos = json_get_coord(val);
    }

    /* msgbox_fontsize / msgboxFontsize → msgbox.font_size */
    val = yyjson_obj_get(root, "msgbox_fontsize");
    if (!val)
    {
        val = yyjson_obj_get(root, "msgboxfontsize");
    }
    if (val && yyjson_is_num(val))
    {
        config.msgbox.font_size = (int)yyjson_get_num(val);
    }

    /* msgbox_duration / msgboxDuration / duration → msgbox.duration */
    val = yyjson_obj_get(root, "msgbox_duration");
    if (!val)
    {
        val = yyjson_obj_get(root, "msgboxduration");
    }
    if (!val)
    {
        val = yyjson_obj_get(root, "duration");
    }
    if (val && yyjson_is_num(val))
    {
        config.msgbox.duration = (float)yyjson_get_num(val);
    }

    /* gift_min_price / giftMinPrice → msgbox.gift_min_price */
    val = yyjson_obj_get(root, "gift_min_price");
    if (!val)
    {
        val = yyjson_obj_get(root, "giftminprice");
    }
    if (val && yyjson_is_num(val))
    {
        config.msgbox.gift_min_price = (float)yyjson_get_num(val);
    }

    /* block_mode / blockmode → danmaku.block_mode */
    val = yyjson_obj_get(root, "block_mode");
    if (!val)
    {
        val = yyjson_obj_get(root, "blockmode");
    }
    if (val)
    {
        config.danmaku.block_mode = json_get_block_mode(val);
    }

    /* statistics_mode / statmode → screen.statistics_mode */
    val = yyjson_obj_get(root, "statistics_mode");
    if (!val)
    {
        val = yyjson_obj_get(root, "statmode");
    }
    if (val)
    {
        config.screen.statistics_mode = json_get_statistics_mode(val);
    }

    /* saveblocked / keep_blocked → danmaku.keep_blocked */
    val = yyjson_obj_get(root, "saveblocked");
    if (!val)
    {
        val = yyjson_obj_get(root, "keep_blocked");
    }
    if (val && yyjson_is_bool(val))
    {
        config.danmaku.keep_blocked = yyjson_get_bool(val);
    }

    yyjson_doc_free(doc);
    return config;
}

/*
 * 写配置文件（使用 yyjson 构建 JSON）
 * 参数：配置文件名/配置信息
 * 返回值：布尔值 是否成功
 */
bool writeConfig(const char *const configFileName, const df_config_t newConfig)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc)
    {
        return false;
    }

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    /* resolution */
    yyjson_mut_val *resolution = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_int(doc, resolution, newConfig.screen.resolution.x);
    yyjson_mut_arr_add_int(doc, resolution, newConfig.screen.resolution.y);
    yyjson_mut_obj_add_val(doc, root, "resolution", resolution);

    /* display_area */
    yyjson_mut_obj_add_real(doc, root, "display_area", newConfig.screen.display_area);

    /* scroll_area */
    yyjson_mut_obj_add_real(doc, root, "scroll_area", newConfig.screen.scroll_area);

    /* scroll_duration */
    yyjson_mut_obj_add_real(doc, root, "scroll_duration", newConfig.danmaku.scroll_duration);

    /* fixed_duration */
    yyjson_mut_obj_add_real(doc, root, "fixed_duration", newConfig.danmaku.fixed_duration);

    /* density */
    yyjson_mut_obj_add_int(doc, root, "density", newConfig.danmaku.density);

    /* line_spacing */
    yyjson_mut_obj_add_int(doc, root, "line_spacing", newConfig.text.line_spacing);

    /* font_size */
    yyjson_mut_obj_add_int(doc, root, "font_size", newConfig.text.font_size);

    /* fontname */
    yyjson_mut_obj_add_str(doc, root, "fontname", newConfig.text.fontname);

    /* opacity */
    yyjson_mut_obj_add_int(doc, root, "opacity", newConfig.text.opacity);

    /* outline */
    yyjson_mut_obj_add_real(doc, root, "outline", newConfig.text.outline);

    /* shadow */
    yyjson_mut_obj_add_real(doc, root, "shadow", newConfig.text.shadow);

    /* bold */
    yyjson_mut_obj_add_bool(doc, root, "bold", newConfig.text.bold);

    /* keep_blocked */
    yyjson_mut_obj_add_bool(doc, root, "keep_blocked", newConfig.danmaku.keep_blocked);

    /* show_usernames */
    yyjson_mut_obj_add_bool(doc, root, "show_usernames", newConfig.danmaku.show_username);

    /* show_msgbox */
    yyjson_mut_obj_add_bool(doc, root, "show_msgbox", newConfig.msgbox.show);

    /* msgbox_size */
    yyjson_mut_val *msgbox_size = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_int(doc, msgbox_size, newConfig.msgbox.size.x);
    yyjson_mut_arr_add_int(doc, msgbox_size, newConfig.msgbox.size.y);
    yyjson_mut_obj_add_val(doc, root, "msgbox_size", msgbox_size);

    /* msgbox_pos */
    yyjson_mut_val *msgbox_pos = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_int(doc, msgbox_pos, newConfig.msgbox.pos.x);
    yyjson_mut_arr_add_int(doc, msgbox_pos, newConfig.msgbox.pos.y);
    yyjson_mut_obj_add_val(doc, root, "msgbox_pos", msgbox_pos);

    /* msgbox_font_size */
    yyjson_mut_obj_add_int(doc, root, "msgbox_font_size", newConfig.msgbox.font_size);

    /* msgbox_duration */
    yyjson_mut_obj_add_real(doc, root, "msgbox_duration", newConfig.msgbox.duration);

    /* gift_min_price */
    yyjson_mut_obj_add_real(doc, root, "gift_min_price", newConfig.msgbox.gift_min_price);

    /* block_mode */
    yyjson_mut_val *block_mode = yyjson_mut_arr(doc);
    if (newConfig.danmaku.block_mode & DF_BLOCK_MODE_L2R)
    {
        yyjson_mut_arr_add_str(doc, block_mode, "L2R");
    }
    if (newConfig.danmaku.block_mode & DF_BLOCK_MODE_R2L)
    {
        yyjson_mut_arr_add_str(doc, block_mode, "R2L");
    }
    if (newConfig.danmaku.block_mode & DF_BLOCK_MODE_TOP)
    {
        yyjson_mut_arr_add_str(doc, block_mode, "top");
    }
    if (newConfig.danmaku.block_mode & DF_BLOCK_MODE_BOTTOM)
    {
        yyjson_mut_arr_add_str(doc, block_mode, "bottom");
    }
    if (newConfig.danmaku.block_mode & DF_BLOCK_MODE_SPECIAL)
    {
        yyjson_mut_arr_add_str(doc, block_mode, "special");
    }
    if (newConfig.danmaku.block_mode & DF_BLOCK_MODE_COLOR)
    {
        yyjson_mut_arr_add_str(doc, block_mode, "color");
    }
    if (newConfig.danmaku.block_mode & DF_BLOCK_MODE_REPEAT)
    {
        yyjson_mut_arr_add_str(doc, block_mode, "repeat");
    }
    yyjson_mut_obj_add_val(doc, root, "block_mode", block_mode);

    /* statistics_mode */
    yyjson_mut_val *statistics_mode = yyjson_mut_arr(doc);
    if (newConfig.screen.statistics_mode & DF_STATISTICS_MODE_TABLE)
    {
        yyjson_mut_arr_add_str(doc, statistics_mode, "table");
    }
    if (newConfig.screen.statistics_mode & DF_STATISTICS_MODE_HISTOGRAM)
    {
        yyjson_mut_arr_add_str(doc, statistics_mode, "histogram");
    }
    yyjson_mut_obj_add_val(doc, root, "statistics_mode", statistics_mode);

    /* 写入文件（带缩进格式） */
    FILE *fptr = df_fopen(configFileName, "w");
    if (!fptr)
    {
        yyjson_mut_doc_free(doc);
        return false;
    }

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (!json_str)
    {
        fclose(fptr);
        yyjson_mut_doc_free(doc);
        return false;
    }

    fwrite(json_str, 1, strlen(json_str), fptr);
    free(json_str);

    bool has_error = ferror(fptr) != 0;
    fclose(fptr);
    yyjson_mut_doc_free(doc);

    return !has_error;
}

/* 打印配置信息 */
void printConfig(df_config_t config)
{
    printf("\n"
           "Configuration:\n"
           "Resolution: %dx%d",
           config.screen.resolution.x, config.screen.resolution.y);

    printf(" | DisplayArea: %.3f", config.screen.display_area);
    if (fabs(config.screen.display_area - 1) < EPS)
    {
        printf("(full)");
    }

    printf(" | ScrollArea: %.3f", config.screen.scroll_area);
    if (fabs(config.screen.scroll_area - 1) < EPS)
    {
        printf("(full)");
    }

    printf(" | ScrollTime: %.3f sec | FixTime: %.3f sec", config.danmaku.scroll_duration,
           config.danmaku.fixed_duration);

    printf("\nDensity: %d", config.danmaku.density);
    if (config.danmaku.density == -1)
    {
        printf("(non-overlap)");
    }
    else if (config.danmaku.density == 0)
    {
        printf("(unlimit)");
    }

    printf(" | LineSpacing: %d | Fontsize: %d | Fontname: \"%s\" | Opacity: %d | Outline: %.1f",
           config.text.line_spacing, config.text.font_size, config.text.fontname, config.text.opacity,
           config.text.outline);
    if (fabs(config.text.outline) < EPS)
    {
        printf("(disable)");
    }

    printf(" | Shadow: %.1f", config.text.shadow);
    if (fabs(config.text.shadow) < EPS)
    {
        printf("(disable)");
    }

    printf(" | Bold: ");
    if (config.text.bold == false)
    {
        printf("false");
    }
    else
    {
        printf("true");
    }

    printf("\nSaveBlocked: ");
    if (config.danmaku.keep_blocked == false)
    {
        printf("false");
    }
    else
    {
        printf("true");
    }

    printf(" | ShowUsernames: ");
    if (config.danmaku.show_username == false)
    {
        printf("false");
    }
    else
    {
        printf("true");
    }

    printf(" | ShowMessageBox: ");
    if (config.msgbox.show == false)
    {
        printf("false");
    }
    else
    {
        printf("true");
    }

    printf("\nMessageBoxSize: %dx%d | MessageBoxPosition: (%d, %d) | MessageBoxFontsize: %d | MessageBoxDuration: %.2f",
           config.msgbox.size.x, config.msgbox.size.y, config.msgbox.pos.x, config.msgbox.pos.y,
           config.msgbox.font_size, config.msgbox.duration);
    if (config.msgbox.duration < EPS)
    {
        printf("(default)");
    }
    else
    {
        printf(" sec");
    }

    printf(" | GiftMinPrice: CNY %.2f", config.msgbox.gift_min_price);

    printf("\nBlockMode: ");
    if (config.danmaku.block_mode == 0)
    {
        printf("null(disable)");
    }
    else
    {
        if (config.danmaku.block_mode & DF_BLOCK_MODE_L2R)
        {
            printf("L2R(left to right) ");
        }
        if (config.danmaku.block_mode & DF_BLOCK_MODE_R2L)
        {
            printf("R2L(right to left) ");
        }
        if (config.danmaku.block_mode & DF_BLOCK_MODE_TOP)
        {
            printf("top ");
        }
        if (config.danmaku.block_mode & DF_BLOCK_MODE_BOTTOM)
        {
            printf("bottom ");
        }
        if (config.danmaku.block_mode & DF_BLOCK_MODE_SPECIAL)
        {
            printf("special ");
        }
        if (config.danmaku.block_mode & DF_BLOCK_MODE_COLOR)
        {
            printf("color ");
        }
        if (config.danmaku.block_mode & DF_BLOCK_MODE_REPEAT)
        {
            printf("repeat ");
        }
    }

    printf("\nStatMode:  ");
    if (config.screen.statistics_mode == 0)
    {
        printf("null(disable)");
    }
    else
    {
        if (config.screen.statistics_mode & DF_STATISTICS_MODE_TABLE)
        {
            printf("table ");
        }
        if (config.screen.statistics_mode & DF_STATISTICS_MODE_HISTOGRAM)
        {
            printf("histogram ");
        }
    }

    printf("\n");
    return;
}
