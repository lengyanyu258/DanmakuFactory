/* MIT License
 *
 * Copyright (c) 2022 hkm
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

#ifndef DF_CONFIG_DEFINE_H_
#define DF_CONFIG_DEFINE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/define.h"

#define KEY_LEN   128
#define VALUE_LEN 512
#define PATH_LEN  1024

#define LIB_CONFIG_VERSION 2

typedef struct coordinate_s
{
    int x;
    int y;
} df_coord_t;

#define DF_STATISTICS_MODE_ENUM_ITEMS                                                                                  \
    X(DF_STATISTICS_MODE_NONE, 0)                                                                                      \
    X(DF_STATISTICS_MODE_TABLE, 1 << 0)                                                                                \
    X(DF_STATISTICS_MODE_HISTOGRAM, 1 << 1)

/* 枚举底层类型语法（enum : uint8_t）为 C23 特性，为兼容 C11 编译器改用普通枚举 + 定宽 typedef */
enum
{
#define X(name, value) name = value,
    DF_STATISTICS_MODE_ENUM_ITEMS
#undef X
};
typedef uint8_t df_stats_flag_t;

#define DF_BLOCK_MODE_ENUM_ITEMS                                                                                       \
    X(DF_BLOCK_MODE_NONE, 0)                                                                                           \
    X(DF_BLOCK_MODE_R2L, 1 << 0)                                                                                       \
    X(DF_BLOCK_MODE_L2R, 1 << 1)                                                                                       \
    X(DF_BLOCK_MODE_TOP, 1 << 2)                                                                                       \
    X(DF_BLOCK_MODE_BOTTOM, 1 << 3)                                                                                    \
    X(DF_BLOCK_MODE_SPECIAL, 1 << 4)                                                                                   \
    X(DF_BLOCK_MODE_COLOR, 1 << 5)                                                                                     \
    X(DF_BLOCK_MODE_REPEAT, 1 << 6)

enum
{
#define X(name, value) name = value,
    DF_BLOCK_MODE_ENUM_ITEMS
#undef X
};
typedef uint8_t df_block_flag_t;

typedef enum
{
    DF_FILETYPE_NONE,
    DF_FILETYPE_ASS,
    DF_FILETYPE_JSON,
    DF_FILETYPE_XML,
    DF_FILETYPE_UNKNOWN,
} df_filetype_t;

typedef struct screen_config_s
{
    df_coord_t      resolution;      /* 分辨率 */
    float           display_area;    /* 全部弹幕显示区域 */
    float           scroll_area;     /* 滚动弹幕显示区域 */
    df_stats_flag_t statistics_mode; /* 统计模式 */
} df_screen_config_t;

typedef struct danmaku_config_s
{
    int             density;                  /* 密度 */
    float           scroll_duration;          /* 滚动时间 */
    float           fixed_duration;           /* 固定时间 */
    bool            show_username;            /* 是否显示用户名 */
    df_block_flag_t block_mode;               /* 屏蔽模式 */
    bool            keep_blocked;             /* 是否保存屏蔽部分 */
    char          **block_list;               /* 弹幕黑名单文件，文件内以换行分隔 */
    bool            block_list_regex_enabled; /* 黑名单是否启用正则表达式匹配 */
} df_danmaku_config_t;

typedef struct text_config_s
{
    char  fontname[FONTNAME_LEN]; /* 字体 */
    int   font_size;              /* 字号 */
    bool  font_size_strict;       /* 是否严格保持指定的字号大小 */
    bool  font_size_norm;         /* 是否修正字号 */
    bool  bold;                   /* 是否加粗 */
    float shadow;                 /* 阴影 */
    int   opacity;                /* 不透明度 */
    float outline;                /* 描边 */
    float outline_blur;           /* 描边模糊半径 */
    int   outline_opacity;        /* 描边不透明度 */
    int   line_spacing;           /* 行间距(像素) */
} df_text_config_t;

typedef struct msgbox_config_s
{
    bool       show;           /* 是否显示消息框 */
    df_coord_t size;           /* 消息框大小 */
    df_coord_t pos;            /* 消息框位置 */
    int        font_size;      /* 消息框内文字大小 */
    float      duration;       /* 消息框持续时长(秒数) */
    float      gift_min_price; /* 消息框礼物最低价格限制(元) */
} df_msgbox_config_t;

// 配置结构体 - 包含可持久化数据
typedef struct configuration_s
{
    int                 version;
    df_screen_config_t  screen;
    df_danmaku_config_t danmaku;
    df_text_config_t    text;
    df_msgbox_config_t  msgbox;
} df_config_t;

typedef struct fileinfo_s
{
    char          path[PATH_LEN];
    df_filetype_t type;
    float         time_shift; // 时轴偏移量（单位：秒）
} df_fileinfo_t;

typedef struct fileinfos_s
{
    size_t         number;
    df_fileinfo_t *fileinfos;
} df_fileinfos_t;

// 运行时配置结构体 - 同时包含配置和文件信息
typedef struct runtime_info_s
{

    df_config_t    config;
    df_fileinfos_t input_files;
    df_fileinfo_t  output_file;
} df_runtime_info_t;

// 默认运行时配置（定义在 config/define.c，避免每个编译单元各持一份拷贝）
const df_runtime_info_t *df_get_default_runtime_info_internal(void);

#endif
