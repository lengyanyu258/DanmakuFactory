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

#include "danmakufactory.h"

#include "config/define.h"
#include "config/parse.h"
#include "config/validate.h"
#include "core/process.h"

df_error_code_t df_run_from_json(const char *runtime_info_json_str)
{
    df_runtime_info_t runtime_info = default_runtime_info;

    if (df_parse_runtime_info_json_str(runtime_info_json_str, &runtime_info) != DF_SUCCESS)
    {
        // TODO: clean up memory
        return df_get_last_error_code();
    }
    if (df_validate_runtime_info(&runtime_info) != DF_SUCCESS)
    {
        // TODO: clean up memory
        return df_get_last_error_code();
    }

    return df_process_convert_files(&runtime_info.config, &runtime_info.input_files, &runtime_info.output_file);
}
