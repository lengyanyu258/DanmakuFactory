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

#ifndef DF_CORE_PROCESS_H_
#define DF_CORE_PROCESS_H_

#include "config/define.h"
#include "danmakufactory.h"

#ifdef __cplusplus
extern "C"
{
#endif

// 单任务进度状态（仅库内部使用，为将来的进度回调保留）
typedef struct single_task_status_s
{
    void *function;
    int   total_number;
    int   completed_number;

    bool is_done;
} df_status_t;

// 读取全部输入文件到弹幕池（追加模式），出错时弹幕池被释放并置 NULL
df_error_code_t df_process_read_files(const df_config_t *config, const df_fileinfos_t *input_files,
                                      df_danmaku_t **danmaku_pool);

// 弹幕池后处理：屏蔽 -> 字号修正 -> 排序
df_error_code_t df_process_danmaku_pool(const df_config_t *config, df_danmaku_t **danmaku_pool);

// 按输出文件类型写出弹幕池
df_error_code_t df_process_write_file(const df_config_t *config, df_danmaku_t *danmaku_pool,
                                      const df_fileinfo_t *output_file);

// 读取 -> 处理 -> 写出 全流程（输出文件已存在时直接覆盖）
df_error_code_t df_process_convert_files(const df_config_t *config, const df_fileinfos_t *input_files,
                                         const df_fileinfo_t *output_file);

// 无输出路径时，为每个输入文件单独生成一个输出文件
df_error_code_t df_process_convert_individual_files(const df_config_t *config, const df_fileinfos_t *input_files,
                                                    df_filetype_t output_filetype);

#ifdef __cplusplus
}
#endif

#endif
