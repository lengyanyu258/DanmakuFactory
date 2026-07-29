#include <CLI/CLI.hpp>

#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <winnls.h>

struct Resolution
{
    int width;
    int height;

    Resolution(int w = 0, int h = 0) : width(w), height(h)
    {
    }

    std::string to_string() const
    {
        return std::to_string(width) + "x" + std::to_string(height);
    }
};

// 解析分辨率字符串（支持 "1920x1080" 格式）
Resolution parse_resolution_str(const std::string &res_str)
{
    if (res_str.empty())
    {
        throw std::invalid_argument("Empty resolution string");
    }

    size_t pos = res_str.find_first_of("xX");
    if (pos == std::string::npos)
    {
        throw std::invalid_argument("Invalid resolution format. Expected '1920x1080'");
    }

    std::string w_str = res_str.substr(0, pos);
    std::string h_str = res_str.substr(pos + 1);

    try
    {
        int width  = std::stoi(w_str);
        int height = std::stoi(h_str);
        if (width <= 0 || height <= 0)
        {
            throw std::invalid_argument("Resolution values must be positive");
        }
        return Resolution(width, height);
    }
    catch (...)
    {
        throw std::invalid_argument("Invalid resolution format. Expected '1920x1080'");
    }
}

// 解析分辨率参数（支持 "1920x1080" 或 1920, 1080）
Resolution parse_resolution_args(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        throw std::invalid_argument("No resolution arguments provided");
    }

    printf("args size: %lld\n", args.size());
    // 单参数：-r 1920x1080
    if (args.size() == 1)
    {
        return parse_resolution_str(args[0]);
    }
    // 双参数：-r 1920 1080
    else if (args.size() == 2)
    {
        try
        {
            int width  = std::stoi(args[0]);
            int height = std::stoi(args[1]);
            if (width <= 0 || height <= 0)
            {
                throw std::invalid_argument("Resolution values must be positive");
            }
            return Resolution(width, height);
        }
        catch (...)
        {
            throw std::invalid_argument("Invalid resolution format. Expected two integers");
        }
    }
    // 其他情况
    else
    {
        throw std::invalid_argument("Invalid number of resolution arguments. Expected 1 or 2");
    }
}

int main(int argc, char **argv)
{
    CLI::App app{"DanmakuFactory", "df.exe"};
    argv = app.ensure_utf8(argv); // new argv memory is held by app

    // 分辨率相关变量
    Resolution resolution;

    // 输入输出路径
    std::string input_path, output_path;

    // 1. 添加 -r/--resolution 选项（支持单参数和多参数）
    auto *res_opt = app.add_option(
                           "-r,--resolution",
                           [&](const std::vector<std::string> &args) {
                               try
                               {
                                   resolution = parse_resolution_args(args);
                                   return true;
                               }
                               catch (const std::exception &e)
                               {
                                   throw CLI::ValidationError(e.what());
                               }
                           },
                           "Resolution (format: 1920x1080 or 1920 1080)")
                        ->expected(1, 2)
                        ->check(CLI::Validator(
                            [](const std::string &input) {
                                printf("input: %s\n", input.c_str());
                                if (input == "error")
                                {
                                    return "error is not a valid value";
                                }
                                else if (input == "unexpected")
                                {
                                    throw CLI::ValidationError{"Unexpected error"};
                                }
                                return "";
                            },
                            "VALIDATOR DESCRIPTION", "Validator name"));

    app.add_option("-f,--file", input_path, "Require an existing file")->required()->check(CLI::ExistingFile);
    // ->transform(CLI::Validator(
    //     [](std::string &utf8_name) {
    //         std::filesystem::path file_path = CLI::to_path(utf8_name);

    //         if (!std::filesystem::exists(file_path))
    //         {
    //             return "File does not exist: " + utf8_name;
    //         }
    //         if (std::filesystem::is_directory(file_path))
    //         {
    //             return "File is actually a directory: " + utf8_name;
    //         }
    //         return std::string();
    //     },
    //     "VALIDATOR DESCRIPTION", "Validator name"));
    app.add_option("-o,--output", output_path, "Output path");

    CLI11_PARSE(app, argc, argv);

    /*
    // 5. 快速验证（调用 C 验证函数）
    int w, h;
    if (validate_resolution(resolution.to_string().c_str(), &w, &h) != 0) {
        throw CLI::ValidationError("Invalid resolution format. Expected '1920x1080'");
    }

    // 6. 生成 JSON
    nlohmann::json json_obj = {
        {"resolution", resolution.to_string()},
        {"input_path", input_path},
        {"output_path", output_path}
    };
    const char* json_str = json_obj.dump().c_str();

    // 7. 调用 LIB
    int ret = df_run_from_json(json_str);
    if (ret != 0) {
        const char* error = df_get_last_error();
        throw CLI::ValidationError(error ? error : "Unknown error");
    }
    */
    std::cout << "input_path: " << input_path.size() << std::endl;
    std::cout << "Success! Resolution: " << resolution.to_string() << std::endl;
    return 0;
}