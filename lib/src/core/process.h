#ifndef BASE_PROCESS_H_
#define BASE_PROCESS_H_

#include "config/define.h"
#include "danmakufactory.h"

typedef struct single_task_status_s
{
    void *function;
    int   total_number;
    int   completed_number;

    bool is_done;
} df_status_t;

df_error_code_t df_process_convert_files(df_config_t *config, df_fileinfos_t *input_files, df_fileinfo_t *output_file);

df_error_code_t df_process_convert_individual_files(df_config_t *config, df_fileinfos_t *input_files,
                                                    df_filetype_t output_filetype);

df_error_code_t df_process_read_files(df_config_t *config, df_fileinfos_t *input_files);

df_error_code_t df_process_danmaku_pool(df_config_t *config, df_fileinfos_t *input_files, df_danmaku_t **danmakuPool);

df_error_code_t df_process_write_file(df_config_t *config, df_fileinfos_t *input_files, df_fileinfo_t *output_file);
#endif
