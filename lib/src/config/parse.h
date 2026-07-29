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

#ifndef CONFIG_PARSE_H_
#define CONFIG_PARSE_H_

#include <yyjson.h>

#include "base/utils.h"
#include "config/define.h"
#include "danmakufactory.h"

// 解析完整的JSON字符串（包含配置和文件信息）
df_error_code_t df_parse_runtime_info_json_str(const char *json_str, df_runtime_info_t *runtime_info);
df_error_code_t df_parse_config_json_val(yyjson_val *json_val, df_config_t *config);
df_error_code_t df_parse_input_files_json_val(yyjson_val *json_val, df_fileinfos_t *input_files);
df_error_code_t df_parse_output_file_json_val(yyjson_val *json_val, df_fileinfo_t *output_file);

// Screen config parsing functions
df_error_code_t df_parse_screen_resolution_json_val(yyjson_val *json_val, df_coord_t *resolution);
df_error_code_t df_parse_screen_display_area_json_val(yyjson_val *json_val, float *display_area);
df_error_code_t df_parse_screen_scroll_area_json_val(yyjson_val *json_val, float *scroll_area);
df_error_code_t df_parse_screen_statistics_mode_json_val(yyjson_val *json_val, df_stats_flag_t *statistics_mode);

// Danmaku config parsing functions
df_error_code_t df_parse_danmaku_density_json_val(yyjson_val *json_val, int *density);
df_error_code_t df_parse_danmaku_scroll_duration_json_val(yyjson_val *json_val, float *scroll_duration);
df_error_code_t df_parse_danmaku_fixed_duration_json_val(yyjson_val *json_val, float *fixed_duration);
df_error_code_t df_parse_danmaku_show_username_json_val(yyjson_val *json_val, bool *show_username);
df_error_code_t df_parse_danmaku_block_mode_json_val(yyjson_val *json_val, df_block_flag_t *block_mode);
df_error_code_t df_parse_danmaku_keep_blocked_json_val(yyjson_val *json_val, bool *keep_blocked);
df_error_code_t df_parse_danmaku_block_list_json_val(yyjson_val *json_val, char ***block_list);
df_error_code_t df_parse_danmaku_block_list_regex_enabled_json_val(yyjson_val *json_val,
                                                                   bool       *block_list_regex_enabled);

// Text config parsing functions
df_error_code_t df_parse_text_fontname_json_val(yyjson_val *json_val, char *fontname);
df_error_code_t df_parse_text_font_size_json_val(yyjson_val *json_val, int *font_size);
df_error_code_t df_parse_text_font_size_strict_json_val(yyjson_val *json_val, bool *font_size_strict);
df_error_code_t df_parse_text_font_size_norm_json_val(yyjson_val *json_val, bool *font_size_norm);
df_error_code_t df_parse_text_bold_json_val(yyjson_val *json_val, bool *bold);
df_error_code_t df_parse_text_shadow_json_val(yyjson_val *json_val, float *shadow);
df_error_code_t df_parse_text_opacity_json_val(yyjson_val *json_val, int *opacity);
df_error_code_t df_parse_text_outline_json_val(yyjson_val *json_val, float *outline);
df_error_code_t df_parse_text_outline_blur_json_val(yyjson_val *json_val, float *outline_blur);
df_error_code_t df_parse_text_outline_opacity_json_val(yyjson_val *json_val, int *outline_opacity);
df_error_code_t df_parse_text_line_spacing_json_val(yyjson_val *json_val, int *line_spacing);

// Msgbox config parsing functions
df_error_code_t df_parse_msgbox_show_json_val(yyjson_val *json_val, bool *show);
df_error_code_t df_parse_msgbox_size_json_val(yyjson_val *json_val, df_coord_t *size);
df_error_code_t df_parse_msgbox_pos_json_val(yyjson_val *json_val, df_coord_t *pos);
df_error_code_t df_parse_msgbox_font_size_json_val(yyjson_val *json_val, int *font_size);
df_error_code_t df_parse_msgbox_duration_json_val(yyjson_val *json_val, float *duration);
df_error_code_t df_parse_msgbox_gift_min_price_json_val(yyjson_val *json_val, float *gift_min_price);

// Parsing Utility Functions
df_error_code_t df_parse_coordinate_json_val(yyjson_val *json_val, df_coord_t *coordinate);
df_error_code_t df_parse_mode_json_val(yyjson_val *json_val, const flag_mapping_t *flag_mappings,
                                       const size_t mappings_num, uint64_t *mode);
df_error_code_t df_parse_string_array_json_val(yyjson_val *json_val, char ***string_array);
df_error_code_t df_parse_fileinfo_json_val(yyjson_val *json_val, df_fileinfo_t *fileinfo);

#endif
