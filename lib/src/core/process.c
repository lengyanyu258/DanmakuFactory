
#include "core/process.h"

#include <stddef.h>

#include "base/error.h"
#include "base/utils.h"
#include "config/define.h"
#include "core/list.h"
#include "danmakufactory.h"

df_error_code_t df_process_convert_files(df_config_t *config, df_fileinfos_t *input_files, df_fileinfo_t *output_file)
{
    if (!config || !input_files || !output_file)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "NULL parameter");
    }

    // 无输出文件路径，为每个输入文件生成一个输出文件
    if (!output_file->path[0])
    {
        return df_process_convert_individual_files(config, input_files, output_file->type);
    }

    // 转换文件
    df_danmaku_t *danmakuPool = NULL;

    df_process_load_files(config, input_files);
    df_process_danmaku_pool(config, input_files, &danmakuPool);
    df_process_write_file(config, input_files, output_file);
    df_free_danmaku_pool(danmakuPool);

    return DF_SET_SUCCESS(); // 清空错误
}

df_error_code_t df_process_convert_individual_files(df_config_t *config, df_fileinfos_t *input_files,
                                                    df_filetype_t output_filetype)
{
    if (!config || !input_files)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "NULL parameter");
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

        if (!change_file_extension(input_file.fileinfos[i].path, output_filetype, output_file.path,
                                      sizeof(output_file.path)))
        {
            return DF_SET_ERROR(DF_ERROR_INTERNAL, "Failed to change file extension for %s",
                                input_file.fileinfos[i].path);
        }

        // TODO: 转换文件前，先检查输出文件是否存在，存在则提示用户是否覆盖
        if (access(outfile.fileName, F_OK) == 0)
        { /* 检查文件是否存在 */
            printf("\nWARNING"
                   "\nFile \"%s\" already exists, it will be overwritten when continue.\n",
                   outfile.fileName);
            if (isContinue(ignoreWarnings) == false)
            {
                return 0;
            }

            /* 权限检查 */
            if (access(outfile.fileName, W_OK) != 0)
            {
                fprintf(stderr, "\nERROR"
                                "\nPermission denied.\n");
                return 0;
            }
        }

        if (df_process_convert_files(config, &input_file, &output_file) != DF_SUCCESS)
        {
            return df_get_last_error_code();
        }
    }

    return DF_SET_SUCCESS();
}

df_error_code_t df_process_read_files(df_config_t *config, df_fileinfos_t *input_files)
{
    if (!config || !input_files)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "NULL parameter");
    }

    // 转换文件
    df_error_code_t returnValue;

    df_status_t status;
    status.total_number = 0;
    for (size_t i = 0; i < input_files->number; ++i)
    {
        if (!strcmp("xml", input_files->fileinfos[i].template))
        {
            returnValue =
                readXml(input_files->files[i].fileName, &danmakuPool, "a", input_files->files[i].timeShift, &status);

            switch (returnValue)
            {
            case 0:
                break;
            case 1:
                fprintf(stderr,
                        "\nERROR [code rx%d]"
                        "\nFailed to open file \"%s\".\n",
                        returnValue, input_files->files[cnt].fileName);
                return 0;
                break;
            case 2:
            case 3:
            case 4:
                fprintf(stderr,
                        "\nERROR [code rx%d]"
                        "\nFailed to read file \"%s\".\n",
                        returnValue, input_files->files[cnt].fileName);
                return 0;
                break;
            case 5:
            case 6:
            case 7:
                fprintf(stderr,
                        "\nERROR [code rx%d]"
                        "\nOut of memory.\n",
                        returnValue);
                return 0;
                break;
            case 8:
                printf("\nWARNING [code rx%d]"
                       "\nIncorrect file format, contiune or exit?",
                       returnValue);
                printf("\nNOTE"
                       "\nCould not load file \"%s\" as a xml file.\n",
                       infile[cnt].fileName);
                if (isContinue(ignoreWarnings) == false)
                {
                    return 0;
                }
                break;
            default:
                fprintf(stderr,
                        "\nERROR [code rx%d]"
                        "\nUndefined Error\n",
                        returnValue);
                return 0;
                break;
            }
        }
        else if (!strcmp("json", infile[cnt].template))
        {
            returnValue = readJson(infile[cnt].fileName, &danmakuPool, "a", infile[cnt].timeShift, &status);

            switch (returnValue)
            {
            case 0:
                break;
            case 1:
                fprintf(stderr,
                        "\nERROR [code rj%d]"
                        "\nFailed to open file \"%s\".\n",
                        returnValue, infile[cnt].fileName);
                return 0;
                break;
            case 2:
            case 3:
            case 4:
                fprintf(stderr,
                        "\nERROR [code rj%d]"
                        "\nFailed to read file \"%s\".\n",
                        returnValue, infile[cnt].fileName);
                return 0;
                break;
            case 5:
            case 6:
                fprintf(stderr,
                        "\nERROR [code rj%d]"
                        "\nOut of memory.\n",
                        returnValue);
                return 0;
                break;
            case 7:
                printf("\nWARNING [code rj%d]"
                       "\nIncorrect file format, contiune or exit?",
                       returnValue);
                printf("\nNOTE"
                       "\nCould not load file \"%s\" as a json file.\n",
                       infile[cnt].fileName);
                if (isContinue(ignoreWarnings) == false)
                {
                    return 0;
                }
                break;
            default:
                fprintf(stderr,
                        "\nERROR [code rj%d]"
                        "\nUndefined Error.\n",
                        returnValue);
                return 0;
                break;
            }
        }
        else if (!strcmp("ass", infile[cnt].template))
        {
            returnValue = readAss(infile[cnt].fileName, &danmakuPool, "a", NULL, infile[cnt].timeShift, &status);
            /* 解析十位错误码 */
            switch (returnValue / 10)
            {
            case 0:
                break;
            case 1:
                fprintf(stderr,
                        "\nERROR [code ra%d]"
                        "\nFailed to open file \"%s\".\n",
                        returnValue, infile[cnt].fileName);
                return 0;
                break;
            case 2:
            case 3:
            case 4:
                fprintf(stderr,
                        "\nERROR [code ra%d]"
                        "\nOut of memory.\n",
                        returnValue);
                return 0;
                break;
            default:
                fprintf(stderr,
                        "\nERROR [code ra%d]"
                        "\nUndefined Error.\n",
                        returnValue);
                return 0;
                break;
            }
            /* 解析个位 */
            switch (returnValue % 10)
            {
            case 0:
                break;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
                fprintf(stderr,
                        "\nERROR [code ra%d]"
                        "\nOut of memory.\n",
                        returnValue);
                return 0;
                break;
            default:
                fprintf(stderr,
                        "\nERROR [code ra%d]"
                        "\nUndefined Error.\n",
                        returnValue);
                return 0;
                break;
            }
        }
        else
        { /* 使用用户自定义模板 */
            returnValue = readTemplateFile(infile[cnt].fileName, infile[cnt].template, &danmakuPool, "a",
                                           infile[cnt].timeShift, &status, tempStr, MAX_TEXT_LENGTH);

            if (returnValue > 0)
            {
                switch (returnValue)
                {
                case 1:
                case 3:
                    /* 程序内部错误 */
                    fprintf(stderr,
                            "\nERROR [code rt%d]"
                            "\nFailed to open template file %s.\n",
                            returnValue, infile[cnt].template);
                    return 0;
                    break;
                case 2:
                case 4:
                case 5:
                    /* 程序内部错误 */
                    fprintf(stderr,
                            "\nERROR [code rt%d]"
                            "\nOut of memory.\n",
                            returnValue);
                    return 0;
                    break;
                default:
                    /* 程序内部错误 */
                    fprintf(stderr,
                            "\nERROR [code rt%d]"
                            "\nUndefined Error.\n",
                            returnValue);
                    return 0;
                    break;
                }
            }
            else if (returnValue < 0)
            {
                /* 模板文件语法错误 */
                /* 程序内部错误 */
                fprintf(stderr,
                        "\nERROR [code rt%d]"
                        "\nFormat File Syntax error."
                        "\nFilename: %s"
                        "\n%s",
                        returnValue, infile[cnt].template, tempStr);
                printf("\nexit...");
                return 0;
            }
        }
    }
    return 0;
}

df_error_code_t df_process_danmaku_pool(df_config_t *config, df_fileinfos_t *input_files, df_danmaku_t **danmakuPool)
{
    if (!config || !input_files || !danmakuPool)
    {
        return DF_SET_ERROR(DF_ERROR_INTERNAL, "NULL parameter");
    }

    /* 屏蔽 */
    blockByType(danmakuPool, config.block_mode, config.block_list, config.block_list_regex_enabled);
    /* 正则化字号 */
    normFontSize(danmakuPool, config);

    /* 排序 */

    returnValue = sortList(&danmakuPool, NULL);
    switch (returnValue)
    {
    case 0:
        break;
    case 2:
        fprintf(stderr,
                "\nERROR [code s%d]"
                "\nOut of memory.\n",
                returnValue);
        return 0;
        break;
    default:
        fprintf(stderr,
                "\nERROR [code s%d]"
                "\nUndefined Error.\n",
                returnValue);
        break;
    }

    return DF_SET_SUCCESS();
}

df_error_code_t df_process_write_file(df_config_t *config, df_fileinfos_t *input_files, df_fileinfo_t *output_file)
{

    /* 写文件 */

    if (!strcmp("ass", outfile.template))
    {
        returnValue = writeAss(outfile.fileName, danmakuPool, config, NULL, NULL);
        /* 解析百位 */
        switch (returnValue / 100)
        {
        case 0:
            break;
        case 1:
            fprintf(stderr,
                    "\nERROR [code wa%d]"
                    "\nFailed to create file \"%s\".\n",
                    returnValue, outfile.fileName);
            return 0;
            break;
        default:
            fprintf(stderr,
                    "\nERROR [code wa%d]"
                    "\nUndefined Error.\n",
                    returnValue);
            break;
        }
        /* 解析十位 */
        returnValue -= returnValue % 100;
        switch (returnValue % 10)
        {
        case 0:
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            fprintf(stderr,
                    "\nERROR [code wa%d]"
                    "\nOut of memory.\n",
                    returnValue);
            return 0;
            break;
        case 9:
            fprintf(stderr,
                    "\nERROR [code wa%d]"
                    "\nFailed to write file \"%s\".\n",
                    returnValue, infile[cnt].fileName);
            return 0;
            break;
        default:
            fprintf(stderr,
                    "\nERROR [code wa%d]"
                    "\nUndefined Error.\n",
                    returnValue);
            break;
        }
        /* 个位函数错误信息已经被以上过滤完毕 无需重新报错 */
    }
    else if (!strcmp("xml", outfile.template))
    {
        returnValue = writeXml(outfile.fileName, danmakuPool, NULL);
        switch (returnValue)
        {
        case 0:
            break;
        case 2:
            fprintf(stderr,
                    "\nERROR [code wx%d]"
                    "\nFailed to create file \"%s\".\n",
                    returnValue, outfile.fileName);
            return 0;
            break;
        case 3:
            fprintf(stderr,
                    "\nERROR [code wx%d]"
                    "\nFailed to write file \"%s\".\n",
                    returnValue, infile[cnt].fileName);
            return 0;
            break;
        default:
            fprintf(stderr,
                    "\nERROR [code wx%d]"
                    "\nUndefined Error.\n",
                    returnValue);
            break;
        }
    }
    else if (!strcmp("json", outfile.template))
    {
        returnValue = writeJson(outfile.fileName, danmakuPool, NULL);
        switch (returnValue)
        {
        case 0:
            break;
        case 2:
            fprintf(stderr,
                    "\nERROR [code wj%d]"
                    "\nFailed to create file \"%s\".\n",
                    returnValue, outfile.fileName);
            return 0;
            break;
        case 3:
            fprintf(stderr,
                    "\nERROR [code wj%d]"
                    "\nFailed to write file \"%s\".\n",
                    returnValue, infile[cnt].fileName);
            return 0;
            break;
        default:
            fprintf(stderr,
                    "\nERROR [code wj%d]"
                    "\nUndefined Error.\n",
                    returnValue);
            break;
        }
    }
}