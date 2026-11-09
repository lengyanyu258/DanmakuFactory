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

/* 弹幕类型重定义规范 */
/* +-------------+---------+ */
/* |  类型名称   |  编号   | */
/* +-------------+---------+ */
/* |  右左滚动   |    1    | */
/* +-------------+---------+ */
/* |  左右滚动   |    2    | */
/* +-------------+---------+ */
/* |  上方固定   |    3    | */
/* +-------------+---------+ */
/* |  下方固定   |    4    | */
/* +-------------+---------+ */
/* | B站特殊弹幕 |    5    | */
/* +-------------+---------+ */

#include "json/file.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

#include "base/string.h"
#include "base/utils.h"
#include "core/list.h"

/*
 * 读取整个文件到内存缓冲区
 * 返回值：成功返回缓冲区（调用方需 free），失败返回 NULL
 */
static char *read_file_to_buffer(const char *filepath, size_t *out_size)
{
    FILE *fp = df_fopen(filepath, "rb");
    if (!fp)
    {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(fp);
        return NULL;
    }

    char *buffer = (char *)malloc((size_t)file_size + 1);
    if (!buffer)
    {
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, (size_t)file_size, fp);
    fclose(fp);

    buffer[read_size] = '\0';
    if (out_size)
    {
        *out_size = read_size;
    }
    return buffer;
}

/*
 * 解析 "c" 字段中的弹幕属性
 * 格式: "time,color,type,fontsize,uid,timestamp,id"
 */
static bool parse_c_field(const char *c_str, float *time, int *color, short *type, short *fontSize)
{
    double      values[4];
    const char *cursor = c_str;
    for (size_t i = 0; i < ARRAY_SIZE(values); i++)
    {
        char *end;
        errno     = 0;
        values[i] = strtod(cursor, &end);
        if (end == cursor || errno == ERANGE || !isfinite(values[i]) || (i < 3 && *end != ',') ||
            (i == 3 && *end != ',' && *end != '\0'))
        {
            return false;
        }
        cursor = end + (*end == ',' ? 1 : 0);
    }
    if (values[0] < -MAX_ASS_MS_INT / 1000.0 || values[0] > MAX_ASS_MS_INT / 1000.0 || values[1] < 0 ||
        values[1] > 0xFFFFFF || trunc(values[1]) != values[1] || values[2] < 1 || values[2] > SHRT_MAX ||
        trunc(values[2]) != values[2] || values[3] < 1 || values[3] > SHRT_MAX || trunc(values[3]) != values[3])
    {
        return false;
    }

    *time     = (float)values[0];
    *color    = (int)values[1];
    *type     = (short)values[2];
    *fontSize = (short)values[3];
    return true;
}
/*
 * 将 JSON 文件中的类型编号转换为程序内部编号
 * +----------+-----------+-----------+
 * | 类型名称 | 原类型编号| 新类型编号|
 * +----------+-----------+-----------+
 * | 右左滚动 |     1     |     1     |
 * | 左右滚动 |     6     |     2     |
 * | 上方固定 |     5     |     3     |
 * | 下方固定 |     4     |     4     |
 * +----------+-----------+-----------+
 */
static short convert_json_type(short json_type)
{
    switch (json_type)
    {
    case 1:
        return 1; /* R2L */
    case 6:
        return 2; /* L2R */
    case 5:
        return 3; /* TOP */
    case 4:
        return 4; /* BOTTOM */
    default:
        return json_type;
    }
}

/*
 * 读取JSON文件加入弹幕池
 * 参数：
 * 文件名/链表头/读取模式（"n"清空新建 / "a"尾部追加）/时轴偏移量
 * 返回值：
 * 0 正常退出
 * 1 打开文件失败
 * 2 3 4 读取文件发生错误
 * 5 节点空间申请失败
 * 6 文本部分空间申请失败
 * 7 文件未能按正确格式读入
 */
int readJson(const char *const ipFile, df_danmaku_t **head, const char *mode, const float timeShift,
             df_status_t *const status)
{
    df_danmaku_t *now = NULL, *last = NULL;
    bool          loaded_record = false;

    /* 刷新status */
    if (status != NULL)
    {
        status->function         = (void *)readJson;
        status->completed_number = 0;
    }

    /* 读取文件到内存 */
    size_t json_len = 0;
    char  *json_buf = read_file_to_buffer(ipFile, &json_len);
    if (!json_buf)
    {
        return 1;
    }

    /* 判断读入方式 */
    if (*head == NULL || *mode == 'n')
    { /* 新建模式 */
        freeList(*head);
        *head = NULL;
    }
    else if (*mode == 'a')
    { /* 追加模式 */
        last = *head;
        while (last->next != NULL)
        {
            last = last->next;
        }
    }

    /* 使用 yyjson 解析 */
    yyjson_read_err  err;
    yyjson_read_flag flags = YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_BOM;
    yyjson_doc      *doc   = yyjson_read_opts(json_buf, json_len, flags, NULL, &err);
    free(json_buf);

    if (!doc)
    {
        return 7;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_arr(root))
    {
        yyjson_doc_free(doc);
        return 7;
    }

    /* 弹幕数组在 root[2]（格式: [[], [], [...]]） */
    yyjson_val *danmaku_arr = NULL;
    size_t      root_len    = yyjson_arr_size(root);

    yyjson_val *first = yyjson_arr_get_first(root);
    if (yyjson_is_obj(first))
    {
        danmaku_arr = root;
    }
    else if (root_len >= 3)
    {
        /* 标准格式: [[], [], [danmaku...]] */
        size_t      idx = 0;
        yyjson_val *item;
        yyjson_arr_foreach(root, idx, root_len, item)
        {
            if (idx == 2)
            {
                danmaku_arr = item;
                break;
            }
        }
    }
    if (!danmaku_arr || !yyjson_is_arr(danmaku_arr))
    {
        yyjson_doc_free(doc);
        return 7;
    }

    /* 遍历弹幕数组 */
    size_t      arr_idx, arr_size;
    yyjson_val *danmaku_obj;
    yyjson_arr_foreach(danmaku_arr, arr_idx, arr_size, danmaku_obj)
    {
        if (!yyjson_is_obj(danmaku_obj))
        {
            continue;
        }

        const char *c_str = yyjson_get_str(yyjson_obj_get(danmaku_obj, "c"));
        const char *m_str = yyjson_get_str(yyjson_obj_get(danmaku_obj, "m"));

        if (!c_str || !m_str)
        {
            continue;
        }

        /* 解析属性 */
        float t_time;
        int   t_color;
        short t_type, t_fontSize;
        if (!parse_c_field(c_str, &t_time, &t_color, &t_type, &t_fontSize))
        {
            continue;
        }
        double shifted_time = (double)t_time + timeShift;
        if (!isfinite(shifted_time) || shifted_time > MAX_ASS_MS_INT / 1000.0)
        {
            yyjson_doc_free(doc);
            return 7;
        }

        /* 申请节点空间 */
        now = (df_danmaku_t *)malloc(sizeof(df_danmaku_t));
        if (!now)
        {
            yyjson_doc_free(doc);
            return 5;
        }

        /* 初始化子结构指针 */
        now->user    = NULL;
        now->gift    = NULL;
        now->special = NULL;
        now->next    = NULL;

        /* 申请文本空间 */
        now->text = (char *)malloc(strlen(m_str) + 1);
        if (!now->text)
        {
            free(now);
            yyjson_doc_free(doc);
            return 6;
        }
        strcpy(now->text, m_str);

        /* 类型转换 */
        now->type = convert_json_type(t_type);

        /* 时间偏移 */
        if (shifted_time < EPS)
        {
            shifted_time = 0.0;
        }
        now->time      = (int)(shifted_time * 1000.0 + 0.5);
        now->font_size = t_fontSize;
        now->color     = t_color;

        /* 并入链表 */
        if (*head == NULL)
        {
            *head = now;
        }
        else
        {
            last->next = now;
        }
        last          = now;
        loaded_record = true;

        /* 更新状态 */
        if (status != NULL)
        {
            status->total_number++;
            status->completed_number++;
        }
    }

    yyjson_doc_free(doc);

    if (loaded_record)
    {
        last->next = NULL;
    }
    else
    {
        return 7;
    }

    /* 刷新status */
    if (status != NULL)
    {
        status->is_done = true;
    }
    return 0;
}

/*
 * 将程序内部类型编号转换为 JSON 文件中的类型编号
 */
static int convert_to_json_type(df_danmaku_t *ptr)
{
    if (IS_R2L(ptr))
    {
        return 1;
    }
    else if (IS_L2R(ptr))
    {
        return 6;
    }
    else if (IS_TOP(ptr))
    {
        return 5;
    }
    else if (IS_BTM(ptr))
    {
        return 4;
    }
    return -1; /* 不支持的类型 */
}

/*
 * 写json文件
 * 参数：输出文件名/弹幕池/状态
 * 返回值：
 * 0 正常退出
 * 1 弹幕池为空
 * 2 创建文件失败
 * 3 写文件发生错误
 */
int writeJson(const char *const fileName, df_danmaku_t *danmakuHead, df_status_t *const status)
{
    /* 刷新status */
    if (status != NULL)
    {
        status->function         = (void *)writeJson;
        status->completed_number = 0;
    }

    if (danmakuHead == NULL)
    {
        return 1;
    }

    /* 使用 yyjson mutable API 构建 JSON */
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc)
    {
        return 2;
    }

    yyjson_mut_val *root        = yyjson_mut_arr(doc);
    yyjson_mut_val *danmaku_arr = yyjson_mut_arr(doc);
    if (root == NULL || danmaku_arr == NULL || !yyjson_mut_arr_add_val(root, yyjson_mut_arr(doc)) ||
        !yyjson_mut_arr_add_val(root, yyjson_mut_arr(doc)) || !yyjson_mut_arr_add_val(root, danmaku_arr))
    {
        yyjson_mut_doc_free(doc);
        return 3;
    }
    yyjson_mut_doc_set_root(doc, root);

    char          c_buf[256];
    char          time_buf[32];
    df_danmaku_t *ptr = danmakuHead;

    while (ptr != NULL)
    {
        int typeInJson = convert_to_json_type(ptr);
        if (typeInJson < 0)
        {
            ptr = ptr->next;
            continue;
        }

        /* 构建 "c" 字段: "time,color,type,fontsize,,," */
        intTimeToStr(time_buf, ptr->time, 3);
        snprintf(c_buf, sizeof(c_buf), "%s,%d,%d,%d,,,", time_buf, ptr->color, typeInJson, ptr->font_size);

        yyjson_mut_val *obj = yyjson_mut_obj(doc);
        if (obj == NULL || !yyjson_mut_obj_add_strcpy(doc, obj, "c", c_buf) ||
            !yyjson_mut_obj_add_str(doc, obj, "m", ptr->text ? ptr->text : "") ||
            !yyjson_mut_arr_add_val(danmaku_arr, obj))
        {
            yyjson_mut_doc_free(doc);
            return 3;
        }

        ptr = ptr->next;

        /* 刷新status */
        if (status != NULL)
        {
            status->completed_number++;
        }
    }

    size_t json_length = 0;
    char  *json_str    = yyjson_mut_write(doc, 0, &json_length);
    if (json_str == NULL)
    {
        yyjson_mut_doc_free(doc);
        return 3;
    }

    FILE *fp = df_fopen(fileName, "w");
    if (fp == NULL)
    {
        free(json_str);
        yyjson_mut_doc_free(doc);
        return 2;
    }
    bool write_failed = fwrite(json_str, 1, json_length, fp) != json_length;
    free(json_str);
    if (fclose(fp) != 0)
    {
        write_failed = true;
    }
    yyjson_mut_doc_free(doc);
    if (write_failed)
    {
        return 3;
    }
    /* 刷新status */
    if (status != NULL)
    {
        status->is_done = true;
    }
    return 0;
}
