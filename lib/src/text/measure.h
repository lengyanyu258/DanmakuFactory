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

#ifndef DF_TEXT_MEASURE_H_
#define DF_TEXT_MEASURE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* 文本度量上下文（不透明类型） */
typedef struct df_text_context_s df_text_context_t;

/**
 * @brief 创建文本度量上下文
 *
 * 初始化 FreeType、Fontconfig、HarfBuzz 环境。
 * 一次转换运行只需创建一次，可在多条弹幕间复用。
 * 若底层库初始化失败，返回的上下文仍有效但 df_text_is_available() 为 false。
 *
 * @return 上下文指针；内存分配失败时返回 NULL
 */
df_text_context_t *df_text_create(void);

/**
 * @brief 销毁文本度量上下文，释放所有字体缓存与库资源
 * @param ctx 由 df_text_create 返回的指针，允许 NULL
 */
void df_text_destroy(df_text_context_t *ctx);

/**
 * @brief 测量 UTF-8 文本的像素宽度
 *
 * 使用 HarfBuzz 进行文本整形（shaping），累加各字形 advance，
 * 不包含 libass 的多字体回退；缺失字形时返回 -1。
 *
 * @param ctx        文本度量上下文
 * @param text       UTF-8 编码的文本字符串
 * @param font_name  字体族名（如 "Microsoft YaHei"）
 * @param pixel_size 像素字号（对应 ASS \fs 值）
 * @return 文本宽度（像素），ctx 不可用或参数非法时返回 -1
 */
int df_text_measure_width(df_text_context_t *ctx, const char *text, const char *font_name, int pixel_size);

/**
 * @brief 查询当前上下文是否具备精确测量能力
 * @param ctx 文本度量上下文
 * @return true 表示 FreeType/Fontconfig/HarfBuzz 均已就绪
 */
bool df_text_is_available(const df_text_context_t *ctx);

/**
 * @brief 注册字体文件（WASM / 无 Fontconfig 环境使用）
 *
 * 将字体族名映射到文件系统路径（Emscripten VFS 或本地路径）。
 * 调用方需先将字体文件写入对应路径。
 * 在具有 Fontconfig 的原生平台上此函数无实际效果（返回 0）。
 *
 * @note 线程安全性：本函数修改全局注册表，应在启动阶段（创建工作线程之前）
 *       完成全部注册。运行阶段（df_run / df_run_from_json 并发执行期间）
 *       不得调用本函数。
 *
 * @param font_name 字体族名（如 "Microsoft YaHei"）
 * @param font_path 字体文件路径（如 "/fonts/msyh.ttc"）
 * @return 0 成功，-1 失败
 */
int df_text_register_font(const char *font_name, const char *font_path);

#ifdef __cplusplus
}
#endif

#endif /* DF_TEXT_MEASURE_H_ */
