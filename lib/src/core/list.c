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

#include "core/list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __EMSCRIPTEN__
#include <pcre2.h>
#endif

#include "base/define.h"
#include "base/error.h"

/*
 * 排序整个链表（桶排序）
 * 参数：要排序的链表头
 * 返回值：
 * 0 正常退出
 * 1 弹幕池为空
 * 2 桶空间申请失败
 */
int sortList(df_danmaku_t **listHead, df_status_t *const status)
{
    /* 刷新status */
    if (status != NULL)
    {
        status->function = (void *)&sortList;
        (status->completed_number) = 0;
        status->is_done = false;
    }

    if (*listHead == NULL)
    {
#if PRINT_ERR == true
        printf("\n[X] 弹幕池为空");
#endif
        return 1;
    }

    df_danmaku_t *now = (*listHead)->next;
    df_danmaku_t *previous = *listHead;
    bool          isSorted = true;

    /* 统计弹幕数量并找出最大最小值 */
    int danmakuNum = 1; // 弹幕条数
    int max;            // 最大值
    int min;            // 最小值
    for (max = min = (*listHead)->time; now != NULL; now = now->next, ++danmakuNum)
    {
        if (now->time > max)
        {
            max = now->time;
        }
        else if (now->time < min)
        {
            min = now->time;
        }

        if (previous->time > now->time)
        {
            isSorted = false;
        }
        previous = now;
    }

    /* 如果本来就排序好了直接退出 */
    if (isSorted == true)
    {
        return 0;
    }

    /* 申请桶空间并清0 */
    int            bucketNum = danmakuNum / 128 + 1;
    df_danmaku_t **bucket = NULL;
    if ((bucket = (df_danmaku_t **)malloc(sizeof(df_danmaku_t *) * bucketNum)) == NULL)
    {
#if PRINT_ERR == true
        printf("\n[X] 申请内存空间失败");
#endif
        return 2;
    }
    memset(bucket, 0, sizeof(df_danmaku_t *) * bucketNum);

    /* 入桶 */
    int           index; // 桶号
    double        denominator = (double)max - (double)min + 1.0;
    df_danmaku_t *ptr;
    df_danmaku_t *last;
    do
    {
        now = *listHead;
        index = (int)(bucketNum * (((double)now->time - (double)min) / denominator));
        if (index >= bucketNum || index < 0)
        {
            /* 溢出非法索引处理 */
            index = bucketNum - 1;
        }

        *listHead = (*listHead)->next;
        if (bucket[index] == NULL)
        { /* 如果该桶为空则将新节点指针填入 */
            bucket[index] = now;
            now->next = NULL;
        }
        else
        {
            ptr = last = bucket[index];
            if (now->time <= ptr->time)
            { /* 判断是否为该桶最小值 */
                bucket[index] = now;
            }
            else
            {
                while ((ptr = ptr->next) != NULL && now->time > ptr->time)
                {
                    last = ptr;
                }
                last->next = now;
            }
            now->next = ptr;
        }

        /* 刷新status */
        if (status != NULL)
        {
            (status->completed_number)++;
        }
    } while (*listHead != NULL);

    /* 出桶 */
    now = *listHead = bucket[0];
    while (now->next != NULL)
    {
        now = now->next;
    }
    for (int cnt = 1; cnt < bucketNum; cnt++)
    {
        ptr = bucket[cnt];
        if (ptr != NULL)
        {
            now->next = ptr;
            while (ptr->next != NULL)
            {
                ptr = ptr->next;
            }
            now = ptr;
        }
    }
    free(bucket);

    /* 刷新status */
    if (status != NULL)
    {
        status->is_done = true;
    }
    return 0;
}

/*
 * 弹幕按类型屏蔽
 * 参数：弹幕链表头/屏蔽模式/屏蔽关键字串集/关键词屏蔽是否启用正则表达式
 * 返回值：DF_SUCCESS 正常 / DF_ERROR_INTERNAL_OUT_OF_MEMORY 内存不足
 * 注意：keyStrings 的生命周期由调用方管理，本函数不释放。
 */
df_error_code_t blockByType(df_danmaku_t *const danmakuHead, const int mode, char **keyStrings,
                            bool block_list_regex_enabled)
{
    if (mode == 0 && keyStrings == NULL)
    {
        return DF_SET_SUCCESS();
    }

    // 先编译所有正则表达式
#ifndef __EMSCRIPTEN__
    pcre2_code **regCodes      = NULL;
    int         *regCodeMap    = NULL; /* regCodeMap[j] = 对应 keyStrings 的下标 */
    int          regCodeCount  = 0;

    if (block_list_regex_enabled && keyStrings != NULL)
    {
        for (int i = 0; keyStrings[i] != NULL; i++)
        {
            int         regexErrorCode;
            PCRE2_SIZE  regexErrorOffset;
            pcre2_code *code = pcre2_compile((PCRE2_SPTR)keyStrings[i], PCRE2_ZERO_TERMINATED, PCRE2_UTF,
                                             &regexErrorCode, &regexErrorOffset, NULL);
            if (code == NULL)
            {
                // 正则表达式编译失败，静默跳过该条目
                continue;
            }

            pcre2_code **newRegCodes = (pcre2_code **)realloc(regCodes, sizeof(pcre2_code *) * (regCodeCount + 1));
            /* realloc 成功后立即提交，后续失败清理仍持有有效地址。 */
            if (newRegCodes != NULL)
            {
                regCodes = newRegCodes;
            }
            int *newRegMap = (int *)realloc(regCodeMap, sizeof(int) * (regCodeCount + 1));
            if (newRegMap != NULL)
            {
                regCodeMap = newRegMap;
            }
            if (newRegCodes != NULL && newRegMap != NULL)
            {
                regCodes[regCodeCount]     = code;
                regCodeMap[regCodeCount]   = i;
                regCodeCount++;
            }
            else
            {
                // 内存分配失败，释放已编译的正则并返回错误
                pcre2_code_free(code);
                for (int j = 0; j < regCodeCount; j++)
                {
                    pcre2_code_free(regCodes[j]);
                }
                free(regCodes);
                free(regCodeMap);
                return DF_SET_ERROR(DF_ERROR_INTERNAL_OUT_OF_MEMORY,
                                    "Out of memory while compiling block list regex");
            }
        }
    }
#endif

    df_danmaku_t *ptr = (df_danmaku_t *)danmakuHead;
    while (ptr != NULL)
    {
        if ((mode & DF_BLOCK_MODE_COLOR) && !IS_SPECIAL(ptr) && ptr->color != 0xFFFFFF)
        {
            if (ptr->type > 0)
            {
                ptr->type *= -1;
            }
        }
        else if ((mode & DF_BLOCK_MODE_R2L) && IS_R2L(ptr))
        {
            if (ptr->type > 0)
            {
                ptr->type *= -1;
            }
        }
        else if ((mode & DF_BLOCK_MODE_L2R) && IS_L2R(ptr))
        {
            if (ptr->type > 0)
            {
                ptr->type *= -1;
            }
        }
        else if ((mode & DF_BLOCK_MODE_TOP) && IS_TOP(ptr))
        {
            if (ptr->type > 0)
            {
                ptr->type *= -1;
            }
        }
        else if ((mode & DF_BLOCK_MODE_BOTTOM) && IS_BTM(ptr))
        {
            if (ptr->type > 0)
            {
                ptr->type *= -1;
            }
        }
        else if ((mode & DF_BLOCK_MODE_SPECIAL) && IS_SPECIAL(ptr))
        {
            if (ptr->type > 0)
            {
                ptr->type *= -1;
            }
        }
        // 如果有关键字串集
        if (keyStrings != NULL && ptr->text != NULL)
        {
            // 逐个检查关键字串
            for (int i = 0; keyStrings[i] != NULL; i++)
            {
#ifndef __EMSCRIPTEN__
                if (block_list_regex_enabled)
                {
                    // 在 regCodeMap 中查找当前 keyStrings[i] 对应的已编译正则
                    pcre2_code *matchedCode = NULL;
                    for (int j = 0; j < regCodeCount; j++)
                    {
                        if (regCodeMap[j] == i)
                        {
                            matchedCode = regCodes[j];
                            break;
                        }
                    }

                    if (matchedCode != NULL)
                    {
                        pcre2_match_data *matchData = pcre2_match_data_create_from_pattern(matchedCode, NULL);
                        if (matchData == NULL)
                        {
                            // OOM，跳过本条匹配
                            continue;
                        }
                        int matchNum =
                            pcre2_match(matchedCode, (PCRE2_SPTR)ptr->text, PCRE2_ZERO_TERMINATED, 0, 0, matchData,
                                        NULL);
                        pcre2_match_data_free(matchData);

                        if (matchNum >= 0)
                        {
                            if (ptr->type > 0)
                            {
                                ptr->type *= -1;
                            }
                            break;
                        }
                    }
                    else
                    {
                        // 该模式编译失败被跳过，退化为字符串匹配
                        if (strstr(ptr->text, keyStrings[i]) != NULL)
                        {
                            if (ptr->type > 0)
                            {
                                ptr->type *= -1;
                            }
                            break;
                        }
                    }
                }
                else
#endif
                {
                    // 使用简单的字符串匹配
                    if (strstr(ptr->text, keyStrings[i]) != NULL)
                    {
                        if (ptr->type > 0)
                        {
                            ptr->type *= -1;
                        }
                        break;
                    }
                }
            }
        }
        ptr = ptr->next;
    }

    // 释放编译的正则表达式
#ifndef __EMSCRIPTEN__
    if (regCodes != NULL)
    {
        for (int i = 0; i < regCodeCount; i++)
        {
            pcre2_code_free(regCodes[i]);
        }
        free(regCodes);
    }
    free(regCodeMap);
#endif

    return DF_SET_SUCCESS();
}

/*
 * 释放整个链表
 * 参数：
 * 要释放的链表头
 */
void freeList(df_danmaku_t *listHead)
{
    df_danmaku_t *ptr = listHead;
    while (ptr != NULL)
    {
        listHead = ptr->next;
        free(ptr->text);    /* 释放文本部分的空间 */
        free(ptr->user);    /* 释放用户信息部分的空间 */
        free(ptr->gift);    /* 释放礼物信息部分的空间 */
        free(ptr->special); /* 释放特殊弹幕部分的空间 */
        free(ptr);
        ptr = listHead;
    }
}

void normFontSize(df_danmaku_t *const danmakuHead, const df_config_t config)
{
    bool doNormalize = false;

    if (config.text.font_size_norm)
    {
        for (df_danmaku_t *ptr = (df_danmaku_t *)danmakuHead; ptr != NULL; ptr = ptr->next)
        {
            if (ptr->type == SPECIAL)
            {
                continue;
            }

            if (ptr->font_size <= 0 || ptr->font_size >= config.screen.resolution.y)
            {
                doNormalize = true;
                break;
            }
        }
    }

    if (config.text.font_size_strict || doNormalize)
    {
        for (df_danmaku_t *ptr = (df_danmaku_t *)danmakuHead; ptr != NULL; ptr = ptr->next)
        {
            if (ptr->type == SPECIAL)
            {
                continue;
            }

            ptr->font_size = 25;
        }
    }
}
