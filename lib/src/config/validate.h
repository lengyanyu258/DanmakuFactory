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

#ifndef CONFIG_VALIDATE_H_
#define CONFIG_VALIDATE_H_

#include "config/define.h"
#include "danmakufactory.h"

// 验证运行时配置和文件信息
df_error_code_t df_validate_runtime_info(const df_runtime_info_t *runtime_info);
df_error_code_t df_validate_config(const df_config_t *config);
df_error_code_t df_validate_input_files(const df_fileinfos_t *input_files);
df_error_code_t df_validate_output_file(const df_fileinfo_t *output_file);

// Screen config validation functions
df_error_code_t df_validate_screen_resolution(const df_coord_t *resolution);
df_error_code_t df_validate_screen_display_area(const float display_area);
df_error_code_t df_validate_screen_scroll_area(const float scroll_area);
df_error_code_t df_validate_screen_statistics_mode(const df_stats_flag_t statistics_mode);

// Danmaku config validation functions
df_error_code_t df_validate_danmaku_density(const int density);
df_error_code_t df_validate_danmaku_scroll_duration(const float scroll_duration);
df_error_code_t df_validate_danmaku_fixed_duration(const float fixed_duration);
df_error_code_t df_validate_danmaku_block_mode(const df_block_flag_t block_mode);
df_error_code_t df_validate_danmaku_block_list(char **const block_list);

// Text config validation functions
df_error_code_t df_validate_text_fontname(const char *fontname);
df_error_code_t df_validate_text_font_size(const int font_size);
df_error_code_t df_validate_text_shadow(const float shadow);
df_error_code_t df_validate_text_opacity(const int opacity);
df_error_code_t df_validate_text_outline(const float outline);
df_error_code_t df_validate_text_outline_blur(const float outline_blur);
df_error_code_t df_validate_text_outline_opacity(const int outline_opacity);
df_error_code_t df_validate_text_line_spacing(const int line_spacing);

// Msgbox config validation functions
df_error_code_t df_validate_msgbox_size(const df_coord_t *size);
df_error_code_t df_validate_msgbox_pos(const df_coord_t *pos);
df_error_code_t df_validate_msgbox_font_size(const int font_size);
df_error_code_t df_validate_msgbox_duration(const float duration);
df_error_code_t df_validate_msgbox_gift_min_price(const float gift_min_price);

#endif
