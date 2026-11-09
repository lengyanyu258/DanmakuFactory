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

#include "config/define.h"

// 默认运行时配置
static const df_runtime_info_t default_runtime_info = {
    // 默认配置
    .config =
        {
            // 显示设置
            .screen =
                {
                    .resolution      = {1920, 1080},            /* 分辨率 */
                    .display_area    = 1.00,                    /* 显示区域 */
                    .scroll_area     = 1.00,                    /* 滚动区域 */
                    .statistics_mode = DF_STATISTICS_MODE_NONE, /* 统计模式 */
                },
            // 弹幕设置
            .danmaku =
                {
                    .density                  = 0,                  /* 弹幕密度 */
                    .scroll_duration          = 12.0,               /* 滚动时间 */
                    .fixed_duration           = 5.0,                /* 固定时间 */
                    .show_username            = false,              /* 是否显示用户名 */
                    .block_mode               = DF_BLOCK_MODE_NONE, /* 屏蔽模式 */
                    .keep_blocked             = true,               /* 是否保存屏蔽部分 */
                    .block_list               = NULL,               /* 弹幕黑名单 */
                    .block_list_regex_enabled = false,              /* 弹幕黑名单是否启用正则表达式匹配 */
                },
            // 文本设置
            .text =
                {
                    .fontname         = "Microsoft YaHei", /* 字体 */
                    .font_size        = 38,                /* 字号 */
                    .font_size_strict = false,             /* 是否严格保持指定的字号大小 */
                    .font_size_norm   = false,             /* 是否修正字号 */
                    .bold             = false,             /* 是否加粗 */
                    .shadow           = 1.0f,              /* 阴影 */
                    .opacity          = 180,               /* 不透明度 */
                    .outline          = 0.0f,              /* 描边 */
                    .outline_blur     = 0.0f,              /* 描边模糊半径 */
                    .outline_opacity  = 255,               /* 描边不透明度 */
                    .line_spacing     = 0,                 /* 行间距 */
                },
            // 消息框设置
            .msgbox =
                {
                    .show           = true,        /* 是否显示消息框 */
                    .size           = {500, 1080}, /* 消息框大小 */
                    .pos            = {20, 0},     /* 消息框位置 */
                    .font_size      = 38,          /* 消息框内文字大小 */
                    .duration       = 0.0f,        /* 消息框持续时长 */
                    .gift_min_price = 0.0f,        /* 消息框礼物最低价格限制 */
                },
        },

    .input_files =
        {
            .number    = 0,
            .fileinfos = NULL,
        },
    .output_file =
        {
            .path       = "",
            .type       = DF_FILETYPE_NONE,
            .time_shift = 0.0f,
        },
};

const df_runtime_info_t *df_get_default_runtime_info_internal(void)
{
    return &default_runtime_info;
}
