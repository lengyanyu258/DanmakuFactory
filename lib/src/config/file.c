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

#include "base/string.h"

static Coord jsonGetCoord(const char *const jsonStr);

/*
 * 从文件读配置文件
 * 参数:配置文件名/默认配置
 * 返回值:配置结构体
 */
df_config_t readConfig(const char *const configFileName, const df_config_t defaultConfig)
{
    int         cnt;
    char        ch;
    char        buf[SIZE_OF_BUF], *bufPtr, *bufCopyPtr;
    df_config_t configOnfile = defaultConfig;

    FILE *fptr = fopen(configFileName, "r");
    if (fptr == NULL)
    {
        return configOnfile;
    }

    /* 找到可读区域 */
    while (fgetc(fptr) != '{' && !feof(fptr))
    {
    }

    /* 读取 */
    cnt = 0;
    while ((ch = fgetc(fptr)) != '}' && !feof(fptr) && cnt < SIZE_OF_BUF - 1)
    {
        buf[cnt] = ch;
        cnt++;
    }
    buf[cnt] = '\0';
    fclose(fptr);

    /* 解析 */
    bufPtr = buf;
    char key[KEY_LEN];
    char value[VALUE_LEN], *valuePtr;
    char typeName[TYPENAME_LEN];
    while (*bufPtr != '\0')
    {
        /* 解析key */
        if (*bufPtr == ',')
        { /* 跳过逗号 */
            bufPtr++;
        }
        strGetLeftPart(key, &bufPtr, ':', KEY_LEN);
        deQuotMarks(key);
        toLower(NULL, key);

        /* 解析value */
        valuePtr   = value;
        bufCopyPtr = bufPtr;

        while (*bufPtr != '\0' && *bufPtr != ',')
        {
            if (*bufPtr == '[')
            {
                while (*bufPtr != ']' && *bufPtr != '\0')
                {
                    *valuePtr = *bufPtr;
                    valuePtr++;
                    bufPtr++;
                }
            }

            *valuePtr = *bufPtr;
            valuePtr++;

            if (*bufPtr != '\0')
            {
                bufPtr++;
            }
        }
        *valuePtr = '\0';
        deQuotMarks(value);

        /* 匹配 */
        valuePtr = value;
        if (strcmp("block_mode", key) == 0)
        {
            int block_mode = 0;
            strGetLeftPart(NULL, &valuePtr, '[', VALUE_LEN);
            while (*valuePtr != '\0')
            {
                /* 拆分中括号内用逗号分割的多个值 */
                if (strchr(valuePtr, ',') != NULL)
                { /* 后半段有逗号 */
                    strGetLeftPart(typeName, &valuePtr, ',', TYPENAME_LEN);
                }
                else
                {                                 /* 后半段无逗号 */
                    strGetLeftPart(typeName, &valuePtr, ']', TYPENAME_LEN);
                    valuePtr += strlen(typeName); /* 将指针移到字串末尾以下一个循环跳出 */
                }

                deQuotMarks(typeName);
                toLower(NULL, typeName);

                if (strcmp("r2l", typeName) == 0)
                {
                    block_mode += BLK_R2L;
                }
                else if (strcmp("l2r", typeName) == 0)
                {
                    block_mode += BLK_L2R;
                }
                else if (strcmp("top", typeName) == 0)
                {
                    block_mode += BLK_TOP;
                }
                else if (strcmp("bottom", typeName) == 0)
                {
                    block_mode += BLK_BOTTOM;
                }
                else if (strcmp("special", typeName) == 0)
                {
                    block_mode += BLK_SPECIAL;
                }
                else if (strcmp("color", typeName) == 0 || strcmp("colour", typeName) == 0)
                { /* 兼容英式拼写的color */
                    block_mode += BLK_COLOR;
                }
                else if (strcmp("repeat", typeName) == 0)
                {
                    block_mode += BLK_REPEAT;
                }
            }
            configOnfile.block_mode = block_mode;
        }
        else if (strcmp("statistics_mode", key) == 0)
        {
            /* 解析value */
            int statistics_mode = 0;
            strGetLeftPart(NULL, &valuePtr, '[', VALUE_LEN);
            while (*valuePtr != '\0')
            {
                /* 解析中括号内用逗号分割的多个值 */
                if (strchr(valuePtr, ',') != NULL)
                { /* 后半段有逗号 */
                    strGetLeftPart(typeName, &valuePtr, ',', TYPENAME_LEN);
                }
                else
                { /* 后半段无逗号 */
                    strGetLeftPart(typeName, &valuePtr, ']', TYPENAME_LEN);
                    /* 将指针移到字串末尾以下一个循环跳出 */
                    valuePtr += strlen(typeName);
                }
                deQuotMarks(typeName);
                toLower(NULL, typeName);

                if (strcmp("table", typeName) == 0)
                {
                    statistics_mode += TABLE;
                }
                else if (strcmp("histogram", typeName) == 0)
                {
                    statistics_mode += HISTOGRAM;
                }
            }
            configOnfile.statistics_mode = statistics_mode;
        }
        else if (strcmp("resolution", key) == 0)
        {
            configOnfile.resolution = jsonGetCoord(value);
        }
        else if (strcmp("resx", key) == 0)
        {
            configOnfile.resolution.x = atoi(value);
        }
        else if (strcmp("resy", key) == 0)
        {
            configOnfile.resolution.y = atoi(value);
        }
        else if (strcmp("scroll_duration", key) == 0)
        {
            configOnfile.scroll_duration = atof(value);
        }
        else if (strcmp("fixed_duration", key) == 0)
        {
            configOnfile.fixed_duration = atof(value);
        }
        else if (strcmp("density", key) == 0)
        {
            configOnfile.density = atoi(value);
        }
        else if (strcmp("linespacing", key) == 0)
        {
            configOnfile.line_spacing = atoi(value);
        }
        else if (strcmp("fontname", key) == 0)
        {
            str_copy_n(configOnfile.fontname, value, FONTNAME_LEN);
        }
        else if (strcmp("font_size", key) == 0)
        {
            configOnfile.font_size = atoi(value);
        }
        else if (strcmp("opacity", key) == 0)
        {
            configOnfile.opacity = atoi(value);
        }
        else if (strcmp("outline", key) == 0)
        {
            configOnfile.outline = atof(value);
        }
        else if (strcmp("shadow", key) == 0)
        {
            configOnfile.shadow = atof(value);
        }
        else if (strcmp("bold", key) == 0)
        {
            configOnfile.bold = strToBool(value);
        }
        else if (strcmp("display_area", key) == 0)
        {
            configOnfile.display_area = atof(value);
        }
        else if (strcmp("scroll_area", key) == 0)
        {
            configOnfile.scroll_area = atof(value);
        }
        else if (strcmp("saveblocked", key) == 0)
        {
            configOnfile.keep_blocked = strToBool(value);
        }
        else if (strcmp("showusernames", key) == 0)
        {
            configOnfile.show_username = strToBool(value);
        }
        else if (strcmp("showmsgbox", key) == 0)
        {
            configOnfile.show = strToBool(value);
        }
        else if (strcmp("msgboxfontsize", key) == 0)
        {
            configOnfile.font_size = atoi(value);
        }
        else if (strcmp("msgboxduration", key) == 0)
        {
            configOnfile.duration = atof(value);
        }
        else if (strcmp("giftminprice", key) == 0)
        {
            configOnfile.gift_min_price = atof(value);
        }
        else if (strcmp("msgboxsize", key) == 0)
        {
            configOnfile.size = jsonGetCoord(value);
        }
        else if (strcmp("msgboxpos", key) == 0)
        {
            configOnfile.pos = jsonGetCoord(value);
        }
    }

    return configOnfile;
}

/*
 * 写配置文件
 * 参数：配置文件名/配置信息
 * 返回值：布尔值 是否成功
 */
bool writeConfig(const char *const configFileName, const df_config_t newConfig)
{
    FILE *fptr = fopen(configFileName, "w");

    char        tempStr[MAX_TEXT_LENGTH];
    df_config_t newConfigCopy = newConfig;
    if (fptr == NULL)
    {
        return false;
    }

    fprintf(fptr,
            "{\n"
            "    \"resolution\": [%d, %d],\n"
            "    \"display_area\": %f,\n"
            "    \"scroll_area\": %f,\n"
            "    \"scroll_duration\": %.3f,\n"
            "    \"fixed_duration\": %.3f,\n"

            "    \"density\": %d,\n"
            "    \"line_spacing\": %d,\n"
            "    \"font_size\": %d,\n"
            "    \"fontname\": \"%s\",\n"
            "    \"opacity\": %d,\n"
            "    \"outline\": %.1f,\n"
            "    \"shadow\": %.1f,\n"
            "    \"bold\": %s,\n",
            newConfigCopy.resolution.x, newConfigCopy.resolution.y, newConfigCopy.display_area,
            newConfigCopy.scroll_area, newConfigCopy.scroll_duration, newConfigCopy.fixed_duration,

            newConfigCopy.density, newConfigCopy.line_spacing, newConfigCopy.font_size, newConfigCopy.fontname,
            newConfigCopy.opacity, newConfigCopy.outline, newConfigCopy.shadow, boolToStr(tempStr, newConfigCopy.bold));

    /* 写是否保存屏蔽部分 */
    if (newConfigCopy.keep_blocked == false)
    {
        fprintf(fptr, "    \"saveblocked\": false, \n");
    }

    // 为防止一次性写入相同的值，此处分开对同一变量赋值输出。
    fprintf(fptr, "    \"showUsernames\": %s,\n", boolToStr(tempStr, newConfigCopy.show_username));

    fprintf(fptr,
            "    \"showMsgbox\": %s,\n"

            "    \"msgboxSize\": [%d, %d],\n"
            "    \"msgboxPos\": [%d, %d],\n"
            "    \"font_size\": %d,\n"
            "    \"duration\": %.2f,\n"
            "    \"gift_min_price\": %.2f,\n",
            boolToStr(tempStr, newConfigCopy.show),

            newConfigCopy.size.x, newConfigCopy.size.y, newConfigCopy.pos.x, newConfigCopy.pos.y,
            newConfigCopy.font_size, newConfigCopy.duration, newConfigCopy.gift_min_price);

    /* 写屏蔽模式 */
    {
        /* L2R, R2L, TOP, BOTTOM, SPECIAL, COLOR, REPEAT */
        fprintf(fptr, "    \"block_mode\": [");
        if (newConfigCopy.block_mode & BLK_L2R)
        {
            fprintf(fptr, "L2R");
            /* 判断是否需要逗号 */
            newConfigCopy.block_mode -= BLK_L2R;
            if (newConfigCopy.block_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }
        if (newConfigCopy.block_mode & BLK_R2L)
        {
            fprintf(fptr, "R2L");
            /* 判断是否需要逗号 */
            newConfigCopy.block_mode -= BLK_R2L;
            if (newConfigCopy.block_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }
        if (newConfigCopy.block_mode & BLK_TOP)
        {
            fprintf(fptr, "top");
            /* 判断是否需要逗号 */
            newConfigCopy.block_mode -= BLK_TOP;
            if (newConfigCopy.block_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }
        if (newConfigCopy.block_mode & BLK_BOTTOM)
        {
            fprintf(fptr, "bottom");
            /* 判断是否需要逗号 */
            newConfigCopy.block_mode -= BLK_BOTTOM;
            if (newConfigCopy.block_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }
        if (newConfigCopy.block_mode & BLK_SPECIAL)
        {
            fprintf(fptr, "special");
            /* 判断是否需要逗号 */
            newConfigCopy.block_mode -= BLK_SPECIAL;
            if (newConfigCopy.block_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }
        if (newConfigCopy.block_mode & BLK_COLOR)
        {
            fprintf(fptr, "color");
            /* 判断是否需要逗号 */
            newConfigCopy.block_mode -= BLK_COLOR;
            if (newConfigCopy.block_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }
        if (newConfigCopy.block_mode & BLK_REPEAT)
        {
            fprintf(fptr, "repeat");
            /* 判断是否需要逗号 */
            newConfigCopy.block_mode -= BLK_REPEAT;
            if (newConfigCopy.block_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }

        fprintf(fptr, "],\n");
    }

    /* 写统计模式 */
    {
        /* TABLE, HISTOGRAM */
        fprintf(fptr, "    \"statistics_mode\": [");
        if (newConfigCopy.statistics_mode & TABLE)
        {
            fprintf(fptr, "table");
            /* 判断是否需要逗号 */
            newConfigCopy.statistics_mode -= TABLE;
            if (newConfigCopy.statistics_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }
        if (newConfigCopy.statistics_mode & HISTOGRAM)
        {
            fprintf(fptr, "histogram");
            /* 判断是否需要逗号 */
            newConfigCopy.statistics_mode -= HISTOGRAM;
            if (newConfigCopy.statistics_mode != 0)
            {
                fprintf(fptr, ", ");
            }
        }

        fprintf(fptr, "]\n");
    }

    fprintf(fptr, "}");

    if (ferror(fptr))
    {
        fclose(fptr);
        return false;
    }

    fclose(fptr);

    return true;
}

/* 打印配置信息 */
void printConfig(df_config_t config)
{
    printf("\n"
           "Configuration:\n"
           "Resolution: %dx%d",
           config.resolution.x, config.resolution.y);

    printf(" | DisplayArea: %.3f", config.display_area);
    if (fabs(config.display_area - 1) < EPS)
    {
        printf("(full)");
    }

    printf(" | ScrollArea: %.3f", config.scroll_area);
    if (fabs(config.scroll_area - 1) < EPS)
    {
        printf("(full)");
    }

    printf(" | ScrollTime: %.3f sec | FixTime: %.3f sec", config.scroll_duration, config.fixed_duration);

    printf("\nDensity: %d", config.density);
    if (config.density == -1)
    {
        printf("(non-overlap)");
    }
    else if (config.density == 0)
    {
        printf("(unlimit)");
    }

    printf(" | LineSpacing: %d | Fontsize: %d | Fontname: \"%s\" | Opacity: %d | Outline: %.1f", config.line_spacing,
           config.font_size, config.fontname, config.opacity, config.outline);
    if (fabs(config.outline) < EPS)
    {
        printf("(disable)");
    }

    printf(" | Shadow: %.1f", config.shadow);
    if (fabs(config.shadow) < EPS)
    {
        printf("(disable)");
    }

    printf(" | Bold: ");
    if (config.bold == false)
    {
        printf("false");
    }
    else
    {
        printf("true");
    }

    printf("\nSaveBlocked: ");
    if (config.keep_blocked == false)
    {
        printf("false");
    }
    else
    {
        printf("true");
    }

    printf(" | ShowUsernames: ");
    if (config.show_username == false)
    {
        printf("false");
    }
    else
    {
        printf("true");
    }

    printf(" | ShowMessageBox: ");
    if (config.show == false)
    {
        printf("false");
    }
    else
    {
        printf("true");
    }

    printf("\nMessageBoxSize: %dx%d | MessageBoxPosition: (%d, %d) | MessageBoxFontsize: %d | MessageBoxDuration: %.2f",
           config.size.x, config.size.y, config.pos.x, config.pos.y, config.font_size, config.duration);
    if (config.duration < EPS)
    {
        printf("(default)");
    }
    else
    {
        printf(" sec");
    }

    printf(" | GiftMinPrice: CNY %.2f", config.gift_min_price);

    printf("\nBlockMode: ");
    if (config.block_mode == 0)
    {
        printf("null(disable)");
    }
    else
    {
        if (config.block_mode & BLK_L2R)
        {
            printf("L2R(left to right) ");
        }
        if (config.block_mode & BLK_R2L)
        {
            printf("R2L(right to left) ");
        }
        if (config.block_mode & BLK_TOP)
        {
            printf("top ");
        }
        if (config.block_mode & BLK_BOTTOM)
        {
            printf("bottom ");
        }
        if (config.block_mode & BLK_SPECIAL)
        {
            printf("special ");
        }
        if (config.block_mode & BLK_COLOR)
        {
            printf("color ");
        }
        if (config.block_mode & BLK_REPEAT)
        {
            printf("repeat ");
        }
    }

    printf("\nStatMode:  ");
    if (config.statistics_mode == 0)
    {
        printf("null(disable)");
    }
    else
    {
        if (config.statistics_mode & TABLE)
        {
            printf("table ");
        }
        if (config.statistics_mode & HISTOGRAM)
        {
            printf("histogram ");
        }
    }

    printf("\n");
    return;
}

/* 文本获取坐标 */
static Coord jsonGetCoord(const char *const jsonStr)
{
    char *jsonStrPtr = (char *)jsonStr;
    char  number[MAX_TEXT_LENGTH];
    Coord returnValue;
    strGetLeftPart(NULL, &jsonStrPtr, '[', VALUE_LEN);
    strGetLeftPart(number, &jsonStrPtr, ',', MAX_TEXT_LENGTH);
    returnValue.x = atoi(number);
    strGetLeftPart(number, &jsonStrPtr, ']', MAX_TEXT_LENGTH);
    returnValue.y = atoi(number);

    return returnValue;
}
