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

#include "core/process.h"

#include <stddef.h>
#include <stdint.h>

#include "ass/file.h"
#include "base/error.h"
#include "base/utils.h"
#include "core/list.h"
#include "xml/file.h"
#include "json/file.h"

/*
 * 读取单个输入文件（追加模式）并把旧模块的 int 错误码映射为 df_error_code_t。
 * 出错时保证 *danmaku_pool 处于已释放且为 NULL 的状态。
 */
static df_error_code_t read_one_file(const df_fileinfo_t *fileinfo, df_danmaku_t **danmaku_pool)
{
    int code;

    switch (fileinfo->type)
    {
    case DF_FILETYPE_XML:
        code = readXml(fileinfo->path, danmaku_pool, "a", fileinfo->time_shift, NULL);
        if (code == 0)
        {
            return DF_SUCCESS;
        }
        if (code == 1)
        { /* 打开失败，弹幕池未被修改 */
            freeList(*danmaku_pool);
            *danmaku_pool = NULL;
            return DF_SET_ERROR(DF_ERROR_FILE_NOT_FOUND, "Failed to open file \"%s\" [code rx%d]", fileinfo->path,
                                code);
        }
        /* 其余错误路径 readXml 内部已释放弹幕池 */
        *danmaku_pool = NULL;
        if (code >= 5 && code <= 7)
        {
            return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Out of memory while reading \"%s\" [code rx%d]",
                                fileinfo->path, code);
        }
        return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Could not load file \"%s\" as a xml file [code rx%d]",
                            fileinfo->path, code);

    case DF_FILETYPE_JSON:
        code = readJson(fileinfo->path, danmaku_pool, "a", fileinfo->time_shift, NULL);
        if (code == 0)
        {
            return DF_SUCCESS;
        }
        /* readJson 所有错误路径均不释放弹幕池，由本层统一释放 */
        freeList(*danmaku_pool);
        *danmaku_pool = NULL;
        if (code == 1)
        {
            return DF_SET_ERROR(DF_ERROR_FILE_NOT_FOUND, "Failed to open file \"%s\" [code rj%d]", fileinfo->path,
                                code);
        }
        if (code == 5 || code == 6)
        {
            return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Out of memory while reading \"%s\" [code rj%d]",
                                fileinfo->path, code);
        }
        return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Could not load file \"%s\" as a json file [code rj%d]",
                            fileinfo->path, code);

    case DF_FILETYPE_ASS:
        code = readAss(fileinfo->path, danmaku_pool, "a", NULL, fileinfo->time_shift, NULL);
        if (code == 0)
        {
            return DF_SUCCESS;
        }
        /* readAss 错误路径不释放弹幕池，由本层统一释放 */
        freeList(*danmaku_pool);
        *danmaku_pool = NULL;
        if (code / 10 == 1)
        { /* 十位为 readAssFile 的返回值：1 打开文件失败 */
            return DF_SET_ERROR(DF_ERROR_FILE_NOT_FOUND, "Failed to open file \"%s\" [code ra%d]", fileinfo->path,
                                code);
        }
        if (code / 10 >= 5 || code == 7)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Invalid ASS time in \"%s\" [code ra%d]", fileinfo->path,
                                code);
        }
        /* 十位 2-4 与个位 1-6 均为内存申请失败 */
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Out of memory while reading \"%s\" [code ra%d]",
                            fileinfo->path, code);

    default:
        freeList(*danmaku_pool);
        *danmaku_pool = NULL;
        return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Unsupported input file type of \"%s\"", fileinfo->path);
    }
}

df_error_code_t df_process_read_files(const df_config_t *config, const df_fileinfos_t *input_files,
                                      df_danmaku_t **danmaku_pool)
{
    if (!config || !input_files || !danmaku_pool)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_NULL_PARAMETER, "NULL parameter");
    }

    for (size_t i = 0; i < input_files->number; ++i)
    {
        df_error_code_t code = read_one_file(&input_files->fileinfos[i], danmaku_pool);
        if (code != DF_SUCCESS)
        {
            return code;
        }
    }

    return DF_SET_SUCCESS();
}

df_error_code_t df_process_danmaku_pool(const df_config_t *config, df_danmaku_t **danmaku_pool)
{
    if (!config || !danmaku_pool)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_NULL_PARAMETER, "NULL parameter");
    }

    /* 屏蔽 */
    df_error_code_t block_code = blockByType(*danmaku_pool, config->danmaku.block_mode, config->danmaku.block_list,
                                             config->danmaku.block_list_regex_enabled);
    if (block_code != DF_SUCCESS)
    {
        return block_code;
    }

    /* 正则化字号 */
    normFontSize(*danmaku_pool, *config);

    /* 排序 */
    int code = sortList(danmaku_pool, NULL);
    if (code != 0)
    {
        if (code == 2)
        {
            return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Out of memory while sorting [code s%d]", code);
        }
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "Failed to sort danmaku pool [code s%d]", code);
    }

    return DF_SET_SUCCESS();
}

/* 写入前以宽整数检查组合时间，防止有效的起点与时长相加越界。 */
static df_error_code_t validate_ass_time_bounds(const df_config_t *config, const df_danmaku_t *head)
{
    int64_t scroll_ms = GET_ASS_MS_FLT(config->danmaku.scroll_duration);
    int64_t fixed_ms  = GET_ASS_MS_FLT(config->danmaku.fixed_duration);
    int64_t msg_ms    = GET_ASS_MS_FLT(config->msgbox.duration);
    for (const df_danmaku_t *node = head; node; node = node->next)
    {
        int64_t start = node->time;
        if (start < 0 || start > MAX_ASS_MS_INT)
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Danmaku time exceeds the supported ASS range");
        }
        start            = (start + 5) / 10 * 10;
        int64_t duration = 0;
        if (IS_R2L(node) || IS_L2R(node))
        {
            duration = scroll_ms;
        }
        else if (IS_TOP(node) || IS_BTM(node))
        {
            duration = fixed_ms;
        }
        else if (IS_SPECIAL(node))
        {
            if (!node->special || node->special->existTime < 0 || node->special->moveTime < 0 ||
                node->special->pauseTime < 0 ||
                (int64_t)node->special->moveTime + node->special->pauseTime > MAX_ASS_MS_INT)
            {
                return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Special danmaku duration is invalid");
            }
            duration = ((int64_t)node->special->existTime + 5) / 10 * 10;
        }
        else if (IS_MSG(node) && config->msgbox.show)
        {
            if (!node->gift || node->gift->duration < 0)
            {
                return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Message duration is invalid");
            }
            duration = msg_ms >= 10 ? msg_ms : ((int64_t)node->gift->duration + 5) / 10 * 10;
            if (start + 250 > MAX_ASS_MS_INT)
            {
                return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Message animation exceeds the supported ASS range");
            }
        }
        if (start + duration > MAX_ASS_MS_INT || (config->screen.statistics_mode && start + 30000 > MAX_ASS_MS_INT))
        {
            return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Danmaku end time exceeds the supported ASS range");
        }
    }
    return DF_SUCCESS;
}
df_error_code_t df_process_write_file(const df_config_t *config, df_danmaku_t *danmaku_pool,
                                      const df_fileinfo_t *output_file)
{
    if (!config || !output_file)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_NULL_PARAMETER, "NULL parameter");
    }

    int code;

    switch (output_file->type)
    {
    case DF_FILETYPE_ASS: {
        df_error_code_t time_code = validate_ass_time_bounds(config, danmaku_pool);
        if (time_code != DF_SUCCESS)
        {
            return time_code;
        }
    }
        code = writeAss(output_file->path, danmaku_pool, *config, NULL, NULL);
        if (code == 0)
        {
            return DF_SET_SUCCESS();
        }
        if (code / 100 == 1)
        { /* 百位 1 创建文件失败 */
            return DF_SET_ERROR(DF_ERROR_FILE_WRITE_FAILED, "Failed to create file \"%s\" [code wa%d]",
                                output_file->path, code);
        }
        if (code / 10 % 10 == 9)
        { /* 十位 9 写文件失败 */
            return DF_SET_ERROR(DF_ERROR_FILE_WRITE_FAILED, "Failed to write file \"%s\" [code wa%d]",
                                output_file->path, code);
        }
        /* 十位 3-8 内存申请失败 */
        return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY, "Out of memory while writing \"%s\" [code wa%d]",
                            output_file->path, code);

    case DF_FILETYPE_XML:
        code = writeXml(output_file->path, danmaku_pool, NULL);
        if (code == 0)
        {
            return DF_SET_SUCCESS();
        }
        return DF_SET_ERROR(DF_ERROR_FILE_WRITE_FAILED, "Failed to %s file \"%s\" [code wx%d]",
                            code == 2 ? "create" : "write", output_file->path, code);

    case DF_FILETYPE_JSON:
        code = writeJson(output_file->path, danmaku_pool, NULL);
        if (code == 0)
        {
            return DF_SET_SUCCESS();
        }
        return DF_SET_ERROR(DF_ERROR_FILE_WRITE_FAILED, "Failed to %s file \"%s\" [code wj%d]",
                            code == 2 ? "create" : "write", output_file->path, code);

    default:
        return DF_SET_ERROR(DF_ERROR_INVALID_FILE_FORMAT, "Unsupported output file type of \"%s\"", output_file->path);
    }
}

df_error_code_t df_process_convert_files(const df_config_t *config, const df_fileinfos_t *input_files,
                                         const df_fileinfo_t *output_file)
{
    if (!config || !input_files || !output_file)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_NULL_PARAMETER, "NULL parameter");
    }

    /* 无输出文件路径，为每个输入文件生成一个输出文件 */
    if (!output_file->path[0])
    {
        return df_process_convert_individual_files(config, input_files, output_file->type);
    }

    df_danmaku_t   *danmaku_pool = NULL;
    df_error_code_t code;

    code = df_process_read_files(config, input_files, &danmaku_pool);
    if (code == DF_SUCCESS)
    {
        code = df_process_danmaku_pool(config, &danmaku_pool);
    }
    if (code == DF_SUCCESS)
    {
        /* 输出文件已存在时直接覆盖 */
        code = df_process_write_file(config, danmaku_pool, output_file);
    }

    freeList(danmaku_pool);

    return code;
}

df_error_code_t df_process_convert_individual_files(const df_config_t *config, const df_fileinfos_t *input_files,
                                                    df_filetype_t output_filetype)
{
    if (!config || !input_files)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL_NULL_PARAMETER, "NULL parameter");
    }

    if (output_filetype == DF_FILETYPE_NONE || output_filetype == DF_FILETYPE_UNKNOWN)
    {
        return DF_SET_ERROR(DF_ERROR_OUTPUT_NOT_SPECIFIED, "Output file type is not specified");
    }

    for (size_t i = 0; i < input_files->number; ++i)
    {
        df_fileinfos_t input_file = {
            .number    = 1,
            .fileinfos = &input_files->fileinfos[i],
        };

        df_fileinfo_t output_file = {
            .type = output_filetype,
        };

        if (change_file_extension(input_file.fileinfos[0].path, output_filetype, output_file.path,
                                  sizeof(output_file.path)) != DF_SUCCESS)
        {
            return DF_SET_ERROR(DF_ERROR_INTERNAL, "Failed to change file extension for \"%s\"",
                                input_file.fileinfos[0].path);
        }

        df_error_code_t code = df_process_convert_files(config, &input_file, &output_file);
        if (code != DF_SUCCESS)
        {
            return code;
        }
    }

    return DF_SET_SUCCESS();
}
