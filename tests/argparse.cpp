#include <cstdio>  // 用于 C 标准库函数
#include <fcntl.h> // 用于 _O_U16TEXT
#include <io.h>    // 用于 _setmode

#include <filesystem>
#include <iostream>

#include <CLI/CLI.hpp>
#include <stdio.h>

void openFileWithCLib(const std::filesystem::path &path)
{
    // 转换为宽字符串
    std::wstring wide_path = path.wstring();

    // 使用宽字符版本的 C 函数（如果可用）
    FILE *file = nullptr;
    int   ret  = _wfopen_s(&file, wide_path.c_str(), L"rb"); // 注意: _wfopen 是 Microsoft 扩展

    if (ret != 0)
    {
        perror("\nCan not open file");
        return;
    }

    if (file)
    {
        printf("open file: %ls\n", wide_path.c_str());
        // 文件操作...
        fclose(file);
    }
    else
    {
        // 错误处理
        perror("\nCan not open file");
    }
}

void openFileWithUTF8(const std::filesystem::path &path)
{
    // 获取 UTF-8 编码的路径
    std::string utf8_path = path.u8string();

    // 直接使用标准 C 函数（在某些环境下可能工作）
    FILE *file = nullptr;
    int   ret  = fopen_s(&file, utf8_path.c_str(), "rb, ccs=UTF-8");

    if (ret != 0)
    {
        perror("\nCan not open file 0");
        return;
    }

    if (file)
    {
        printf("open file: %s\n", utf8_path.c_str());
        // 文件操作...
        fclose(file);
    }
    else
    {
        // 错误处理
        perror("\nCan not open file 1");
    }
}

int main(int argc, char *argv[])
{
    CLI::App    app{"...    emoji         "};
    std::string path;
    app.add_option("path", path, "... emoji...")->required();

    try
    {
        argv = app.ensure_utf8(argv);
        app.parse(argc, argv); // ✅ 直接支持 wchar_t**
    }
    catch (const CLI::ParseError &e)
    {
        return app.exit(e);
    }

    std::filesystem::path p = CLI::to_path(path);
    if (std::filesystem::exists(p))
    {
        std::cout << "1" << std::endl;
        openFileWithCLib(p);
        openFileWithUTF8(p);
    }
    else
    {
        std::cout << "0" << std::endl;
    }

    return 0;
}