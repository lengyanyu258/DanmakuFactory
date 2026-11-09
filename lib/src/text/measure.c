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

#include "text/measure.h"
#include "base/utils.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DF_HAS_TEXT_MEASURE

/*============================================================================
 * 精确实现：FreeType + HarfBuzz（+ Fontconfig 可选）
 *============================================================================*/

    #include <ft2build.h>
    #include FT_FREETYPE_H
    #include <hb-ft.h>
    #include <hb.h>

    #ifdef DF_HAS_FONTCONFIG
        #include <fontconfig/fontconfig.h>
    #endif

    #define FONT_CACHE_SIZE 16
    #define FONT_NAME_MAX   128

/* 字体缓存条目 */
typedef struct font_cache_entry_s
{
    char       name[FONT_NAME_MAX]; /* 字体族名 */
    int        pixel_size;          /* 像素字号 */
    FT_Face    face;                /* FreeType 字体面 */
    hb_font_t *hb_font;             /* HarfBuzz 字体对象 */
    unsigned   access_tick;         /* 最近访问时间戳（LRU 用） */
} font_cache_entry_t;

/* 文本度量上下文 */
struct df_text_context_s
{
    FT_Library ft_lib;
    #ifdef DF_HAS_FONTCONFIG
    FcConfig *fc_config;
    #endif
    hb_buffer_t       *hb_buf;
    font_cache_entry_t cache[FONT_CACHE_SIZE];
    int                cache_count;
    unsigned           tick_counter;
    bool               available;
};

/*============================================================================
 * 字体路径解析
 *============================================================================*/

    #ifdef DF_HAS_FONTCONFIG

/*---------------------------------------------------------------------------
 * Fontconfig 路径：通过字体族名匹配系统字体文件
 *---------------------------------------------------------------------------*/
static char *resolve_font_path(FcConfig *fc_config, const char *font_name, int *face_index)
{
    FcPattern *pat = FcNameParse((const FcChar8 *)font_name);
    if (pat == NULL)
    {
        return NULL;
    }

    FcConfigSubstitute(fc_config, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult   result;
    FcPattern *match = FcFontMatch(fc_config, pat, &result);
    FcPatternDestroy(pat);

    if (match == NULL || result != FcResultMatch)
    {
        if (match != NULL)
        {
            FcPatternDestroy(match);
        }
        return NULL;
    }

    FcChar8 *file_path = NULL;
    if (FcPatternGetString(match, FC_FILE, 0, &file_path) != FcResultMatch)
    {
        FcPatternDestroy(match);
        return NULL;
    }

    *face_index = 0;
    FcPatternGetInteger(match, FC_INDEX, 0, face_index);
    char *path_copy = df_strdup((const char *)file_path);
    FcPatternDestroy(match);

    return path_copy;
}

/*---------------------------------------------------------------------------
 * 初始化 Fontconfig 配置（跨平台）
 *---------------------------------------------------------------------------*/
static FcConfig *init_fontconfig(void)
{
    FcConfig *config = FcInitLoadConfigAndFonts();
    if (config != NULL)
    {
        FcPattern *test_pat = FcNameParse((const FcChar8 *)"sans-serif");
        if (test_pat != NULL)
        {
            FcConfigSubstitute(config, test_pat, FcMatchPattern);
            FcDefaultSubstitute(test_pat);
            FcResult   result;
            FcPattern *match = FcFontMatch(config, test_pat, &result);
            FcPatternDestroy(test_pat);
            if (match != NULL)
            {
                FcPatternDestroy(match);
                return config;
            }
        }
        FcConfigDestroy(config);
        config = NULL;
    }

    config = FcConfigCreate();
    if (config == NULL)
    {
        return NULL;
    }

        #ifdef _WIN32
    {
        const char *windir = getenv("WINDIR");
        char        font_dir[512];
        if (windir != NULL && windir[0] != '\0')
        {
            snprintf(font_dir, sizeof(font_dir), "%s\\Fonts", windir);
        }
        else
        {
            strcpy(font_dir, "C:\\Windows\\Fonts");
        }
        FcConfigAppFontAddDir(config, (const FcChar8 *)font_dir);
    }
        #else
    FcConfigAppFontAddDir(config, (const FcChar8 *)"/usr/share/fonts");
    FcConfigAppFontAddDir(config, (const FcChar8 *)"/usr/local/share/fonts");
    {
        const char *home = getenv("HOME");
        if (home != NULL)
        {
            char user_font_dir[512];
            snprintf(user_font_dir, sizeof(user_font_dir), "%s/.local/share/fonts", home);
            FcConfigAppFontAddDir(config, (const FcChar8 *)user_font_dir);
        }
    }
        #endif

    if (!FcConfigBuildFonts(config))
    {
        FcConfigDestroy(config);
        return NULL;
    }

    return config;
}

/* Fontconfig 平台：df_text_register_font 无实际操作（系统自动发现字体） */
int df_text_register_font(const char *font_name, const char *font_path)
{
    (void)font_name;
    (void)font_path;
    return 0;
}

    #else /* !DF_HAS_FONTCONFIG */

        /*---------------------------------------------------------------------------
         * 字体注册表路径（WASM / 无 Fontconfig 环境）
         *
         * 调用方通过 df_text_register_font() 将字体族名映射到 VFS 文件路径。
         *---------------------------------------------------------------------------*/
        #define FONT_REGISTRY_SIZE 32
        #define FONT_PATH_MAX      512

typedef struct font_registry_entry_s
{
    char name[FONT_NAME_MAX];
    char path[FONT_PATH_MAX];
} font_registry_entry_t;

static font_registry_entry_t g_font_registry[FONT_REGISTRY_SIZE];
static int                   g_font_registry_count = 0;

int df_text_register_font(const char *font_name, const char *font_path)
{
    if (font_name == NULL || font_path == NULL || font_name[0] == '\0' || font_path[0] == '\0' ||
        strlen(font_name) >= FONT_NAME_MAX || strlen(font_path) >= FONT_PATH_MAX)
    {
        return -1;
    }

    /* 检查是否已注册（更新路径） */
    for (int i = 0; i < g_font_registry_count; i++)
    {
        if (strcmp(g_font_registry[i].name, font_name) == 0)
        {
            strncpy(g_font_registry[i].path, font_path, FONT_PATH_MAX - 1);
            g_font_registry[i].path[FONT_PATH_MAX - 1] = '\0';
            return 0;
        }
    }

    /* 新增注册 */
    if (g_font_registry_count >= FONT_REGISTRY_SIZE)
    {
        return -1; /* 注册表已满 */
    }

    strncpy(g_font_registry[g_font_registry_count].name, font_name, FONT_NAME_MAX - 1);
    g_font_registry[g_font_registry_count].name[FONT_NAME_MAX - 1] = '\0';
    strncpy(g_font_registry[g_font_registry_count].path, font_path, FONT_PATH_MAX - 1);
    g_font_registry[g_font_registry_count].path[FONT_PATH_MAX - 1] = '\0';
    g_font_registry_count++;

    return 0;
}

/* 从注册表查找字体路径（未找到时回退到第一个注册字体） */
static const char *resolve_font_path_from_registry(const char *font_name)
{
    for (int i = 0; i < g_font_registry_count; i++)
    {
        if (strcmp(g_font_registry[i].name, font_name) == 0)
        {
            return g_font_registry[i].path;
        }
    }
    /* 回退：使用注册表中第一个字体作为默认 */
    if (g_font_registry_count > 0)
    {
        return g_font_registry[0].path;
    }
    return NULL;
}

    #endif /* DF_HAS_FONTCONFIG */

/*============================================================================
 * 字体缓存与加载
 *============================================================================*/

static font_cache_entry_t *get_or_load_font(df_text_context_t *ctx, const char *font_name, int pixel_size)
{
    /* 查找缓存 */
    for (int i = 0; i < ctx->cache_count; i++)
    {
        if (ctx->cache[i].pixel_size == pixel_size && strcmp(ctx->cache[i].name, font_name) == 0)
        {
            ctx->cache[i].access_tick = ++ctx->tick_counter;
            return ctx->cache[i].hb_font != NULL ? &ctx->cache[i] : NULL;
        }
    }

    /* 缓存未命中，需要加载 */
    int slot;
    if (ctx->cache_count < FONT_CACHE_SIZE)
    {
        slot = ctx->cache_count++;
    }
    else
    {
        /* LRU 驱逐：找 access_tick 最小的槽位 */
        slot              = 0;
        unsigned min_tick = ctx->cache[0].access_tick;
        for (int i = 1; i < FONT_CACHE_SIZE; i++)
        {
            if (ctx->cache[i].access_tick < min_tick)
            {
                min_tick = ctx->cache[i].access_tick;
                slot     = i;
            }
        }
        /* 释放旧资源（hb_font 持有 face 引用，销毁时自动 Done_Face） */
        if (ctx->cache[slot].hb_font != NULL)
        {
            hb_font_destroy(ctx->cache[slot].hb_font);
        }
        else if (ctx->cache[slot].face != NULL)
        {
            FT_Done_Face(ctx->cache[slot].face);
        }
    }

    font_cache_entry_t *entry = &ctx->cache[slot];
    memset(entry, 0, sizeof(*entry));

    strncpy(entry->name, font_name, FONT_NAME_MAX - 1);
    entry->name[FONT_NAME_MAX - 1] = '\0';
    entry->pixel_size              = pixel_size;

    /* 解析字体文件路径 */
    const char *font_path  = NULL;
    char       *alloc_path = NULL; /* Fontconfig 路径需要 free */
    int         face_index = 0;

    #ifdef DF_HAS_FONTCONFIG
    alloc_path = resolve_font_path(ctx->fc_config, font_name, &face_index);
    if (alloc_path == NULL)
    {
        alloc_path = resolve_font_path(ctx->fc_config, "sans-serif", &face_index);
    }
    font_path = alloc_path;
    #else
    font_path = resolve_font_path_from_registry(font_name);
    #endif

    if (font_path == NULL)
    {
        return NULL;
    }

    /* FreeType 加载字体 */
    FT_Face face = NULL;
    if (FT_New_Face(ctx->ft_lib, font_path, face_index, &face) != 0)
    {
        free(alloc_path);
        return NULL;
    }
    free(alloc_path);

    /* 设置请求的像素字号。 */
    if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)pixel_size) != 0)
    {
        FT_Done_Face(face);
        return NULL;
    }

    /* 创建 HarfBuzz 字体（引用 FT_Face，销毁 hb_font 时自动 Done_Face） */
    hb_font_t *hb_font = hb_ft_font_create_referenced(face);
    if (hb_font == NULL || hb_font == hb_font_get_empty())
    {
        FT_Done_Face(face);
        return NULL;
    }

    /* HarfBuzz owns a new reference; release the reference from FT_New_Face. */
    FT_Done_Face(face);
    entry->face        = face;
    entry->hb_font     = hb_font;
    entry->access_tick = ++ctx->tick_counter;

    return entry;
}

/*============================================================================
 * 公共 API 实现
 *============================================================================*/

df_text_context_t *df_text_create(void)
{
    df_text_context_t *ctx = (df_text_context_t *)calloc(1, sizeof(df_text_context_t));
    if (ctx == NULL)
    {
        return NULL;
    }

    ctx->available = false;

    /* 初始化 FreeType */
    if (FT_Init_FreeType(&ctx->ft_lib) != 0)
    {
        return ctx;
    }

    #ifdef DF_HAS_FONTCONFIG
    /* 初始化 Fontconfig（跨平台） */
    ctx->fc_config = init_fontconfig();
    if (ctx->fc_config == NULL)
    {
        FT_Done_FreeType(ctx->ft_lib);
        ctx->ft_lib = NULL;
        return ctx;
    }
    #endif

    /* 创建 HarfBuzz 缓冲区（全局复用） */
    ctx->hb_buf = hb_buffer_create();
    if (ctx->hb_buf == NULL || !hb_buffer_allocation_successful(ctx->hb_buf))
    {
    #ifdef DF_HAS_FONTCONFIG
        FcConfigDestroy(ctx->fc_config);
        ctx->fc_config = NULL;
    #endif
        FT_Done_FreeType(ctx->ft_lib);
        ctx->ft_lib = NULL;
        return ctx;
    }

    ctx->available = true;
    return ctx;
}

void df_text_destroy(df_text_context_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    /* 释放所有缓存的字体（hb_font 持有 face 引用，销毁时自动 Done_Face） */
    for (int i = 0; i < ctx->cache_count; i++)
    {
        if (ctx->cache[i].hb_font != NULL)
        {
            hb_font_destroy(ctx->cache[i].hb_font);
        }
        else if (ctx->cache[i].face != NULL)
        {
            FT_Done_Face(ctx->cache[i].face);
        }
    }

    if (ctx->hb_buf != NULL)
    {
        hb_buffer_destroy(ctx->hb_buf);
    }
    #ifdef DF_HAS_FONTCONFIG
    if (ctx->fc_config != NULL)
    {
        FcConfigDestroy(ctx->fc_config);
    }
    #endif
    if (ctx->ft_lib != NULL)
    {
        FT_Done_FreeType(ctx->ft_lib);
    }

    free(ctx);
}

int df_text_measure_width(df_text_context_t *ctx, const char *text, const char *font_name, int pixel_size)
{
    if (ctx == NULL || !ctx->available || text == NULL || font_name == NULL || pixel_size <= 0)
    {
        return -1;
    }

    if (text[0] == '\0')
    {
        return 0;
    }

    /* 获取字体（缓存命中或加载） */
    font_cache_entry_t *entry = get_or_load_font(ctx, font_name, pixel_size);
    if (entry == NULL)
    {
        return -1;
    }

    /* HarfBuzz 文本整形 */
    hb_buffer_t *buf = ctx->hb_buf;
    hb_buffer_reset(buf);
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);

    hb_shape(entry->hb_font, buf, NULL, 0);
    if (!hb_buffer_allocation_successful(buf))
    {
        return -1;
    }

    unsigned int     info_count = 0;
    hb_glyph_info_t *infos      = hb_buffer_get_glyph_infos(buf, &info_count);
    for (unsigned int i = 0; i < info_count; i++)
    {
        if (infos[i].codepoint == 0)
        {
            return -1; /* Missing glyphs require the caller's fallback width. */
        }
    }

    /* 累加字形 advance（26.6 定点 → 像素） */
    unsigned int         glyph_count = 0;
    hb_glyph_position_t *positions   = hb_buffer_get_glyph_positions(buf, &glyph_count);

    int64_t total_advance = 0;
    for (unsigned int i = 0; i < glyph_count; i++)
    {
        total_advance += positions[i].x_advance;
    }

    /* 26.6 定点数转像素（四舍五入） */
    if (total_advance < 0 || total_advance > (int64_t)INT_MAX * 64 - 32)
    {
        return -1;
    }
    int width = (int)((total_advance + 32) / 64);

    return width;
}

bool df_text_is_available(const df_text_context_t *ctx)
{
    if (ctx == NULL)
    {
        return false;
    }
    return ctx->available;
}

#else  /* !DF_HAS_TEXT_MEASURE */

/*============================================================================
 * 降级实现：底层库不可用时返回空壳上下文
 *============================================================================*/

struct df_text_context_s
{
    bool available;
};

df_text_context_t *df_text_create(void)
{
    df_text_context_t *ctx = (df_text_context_t *)calloc(1, sizeof(df_text_context_t));
    if (ctx != NULL)
    {
        ctx->available = false;
    }
    return ctx;
}

void df_text_destroy(df_text_context_t *ctx)
{
    free(ctx);
}

int df_text_measure_width(df_text_context_t *ctx, const char *text, const char *font_name, int pixel_size)
{
    (void)ctx;
    (void)text;
    (void)font_name;
    (void)pixel_size;
    return -1;
}

bool df_text_is_available(const df_text_context_t *ctx)
{
    (void)ctx;
    return false;
}

int df_text_register_font(const char *font_name, const char *font_path)
{
    (void)font_name;
    (void)font_path;
    return -1;
}

#endif /* DF_HAS_TEXT_MEASURE */
