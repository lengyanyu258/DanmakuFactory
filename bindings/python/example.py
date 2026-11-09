# -*- coding: utf-8 -*-
"""DanmakuFactory Python 使用示例

演示如何使用 DanmakuFactory Python 绑定进行弹幕格式转换。

前置条件:
    1. 构建动态库: xmake build lib-shared
    2. 运行: python bindings/python/example.py
"""

import sys
import tempfile
from pathlib import Path


def _enable_utf8_console():
    """将 Windows 控制台切换为 UTF-8 码页并重配置 stdout/stderr。"""
    if sys.platform == "win32":
        try:
            import ctypes

            ctypes.windll.kernel32.SetConsoleOutputCP(65001)
            ctypes.windll.kernel32.SetConsoleCP(65001)
        except Exception:
            pass
    for _stream in (sys.stdout, sys.stderr):
        if hasattr(_stream, "reconfigure"):
            _stream.reconfigure(encoding="utf-8", errors="replace")


_enable_utf8_console()

sys.path.insert(0, str(Path(__file__).parent))

from danmakufactory import DanmakuFactory, DanmakuFactoryContext, DanmakuFactoryError


def example_basic_convert():
    """基础转换示例：XML → ASS"""
    print("=== 基础转换：XML → ASS ===")

    df = DanmakuFactory()
    print(f"库版本: {df.version}")

    # 使用项目自带的测试文件
    repo_root = Path(__file__).resolve().parent.parent.parent
    input_xml = repo_root / "tests" / "fixtures" / "sample.xml"

    with tempfile.TemporaryDirectory() as tmp:
        output_ass = Path(tmp) / "output.ass"

        df.convert(input_xml, output_ass)
        print(f"转换成功: {input_xml.name} → {output_ass.name}")
        print(f"输出大小: {output_ass.stat().st_size} bytes")
        print(f"前 200 字符:\n{output_ass.read_text(encoding='utf-8-sig')[:200]}")

    print()


def example_with_config():
    """带配置的转换示例"""
    print("=== 带配置的转换 ===")

    df = DanmakuFactory()
    repo_root = Path(__file__).resolve().parent.parent.parent
    input_xml = repo_root / "tests" / "fixtures" / "sample.xml"

    # 自定义配置
    config = {
        "screen": {
            "resolution": [1920, 1080],
            "display_area": 0.75,
        },
        "danmaku": {
            "scroll_duration": 10.0,
            "fixed_duration": 5.0,
            "density": -1,  # 不重叠模式
        },
        "text": {
            "fontname": "Microsoft YaHei",
            "font_size": 38,
            "opacity": 200,
            "outline": 1.0,
            "shadow": 0.5,
            "bold": True,
        },
    }

    with tempfile.TemporaryDirectory() as tmp:
        output_ass = Path(tmp) / "styled_output.ass"

        df.convert(input_xml, output_ass, config=config)
        print(f"转换成功（自定义样式）")
        print(f"输出大小: {output_ass.stat().st_size} bytes")

    print()


def example_error_handling():
    """错误处理示例"""
    print("=== 错误处理 ===")

    df = DanmakuFactory()

    try:
        df.convert("nonexistent_file.xml", "output.ass")
    except DanmakuFactoryError as e:
        print(f"捕获错误:")
        print(f"  错误码: {e.code}")
        print(f"  消息: {e.message}")

    print()


def example_batch_convert():
    """批量转换示例"""
    print("=== 批量转换 ===")

    df = DanmakuFactory()
    repo_root = Path(__file__).resolve().parent.parent.parent
    input_xml = repo_root / "tests" / "fixtures" / "sample.xml"

    with tempfile.TemporaryDirectory() as tmp:
        # 同一输入转换为多种格式
        outputs = {
            "ASS": Path(tmp) / "output.ass",
            "XML": Path(tmp) / "output.xml",
            "JSON": Path(tmp) / "output.json",
        }

        for fmt, output_path in outputs.items():
            try:
                df.convert(input_xml, output_path)
                print(f"  {fmt}: {output_path.stat().st_size} bytes")
            except DanmakuFactoryError as e:
                print(f"  {fmt}: 失败 - {e.message}")

    print()


def example_handle_api():
    """标准化句柄 API 示例（推荐）：create → configure → run → destroy"""
    print("=== 句柄 API：create → configure → run → destroy ===")

    repo_root = Path(__file__).resolve().parent.parent.parent
    input_xml = repo_root / "tests" / "fixtures" / "sample.xml"

    with tempfile.TemporaryDirectory() as tmp:
        output_ass = Path(tmp) / "handle_output.ass"

        # with 语句自动管理生命周期（退出时调用 df_destroy）
        with DanmakuFactoryContext() as ctx:
            # configure：文件
            ctx.add_input_file(input_xml)
            ctx.set_output_file(output_ass)

            # configure：单项 setter 微调
            ctx.set_resolution(1920, 1080)
            ctx.set_font_size(30)
            ctx.set_scroll_duration(10.0)
            ctx.set_bold(True)
            ctx.set_opacity(200)

            # run
            ctx.run()

        print(f"转换成功: {output_ass.name}")
        print(f"输出大小: {output_ass.stat().st_size} bytes")

    print()


def example_handle_api_batch_config():
    """句柄 API + 批量配置加载示例"""
    print("=== 句柄 API + 批量配置加载 ===")

    repo_root = Path(__file__).resolve().parent.parent.parent
    input_xml = repo_root / "tests" / "fixtures" / "sample.xml"

    with tempfile.TemporaryDirectory() as tmp:
        output_ass = Path(tmp) / "handle_cfg_output.ass"

        with DanmakuFactoryContext() as ctx:
            ctx.add_input_file(input_xml)
            ctx.set_output_file(output_ass)

            # 一次性加载多项配置（合并语义，仅覆盖出现的字段）
            ctx.load_config_json(
                '{"text": {"font_size": 25, "opacity": 180}, '
                '"danmaku": {"scroll_duration": 8.0}}'
            )
            ctx.run()

        print(f"转换成功: {output_ass.stat().st_size} bytes")

    print()


if __name__ == "__main__":
    example_basic_convert()
    example_with_config()
    example_handle_api()
    example_handle_api_batch_config()
    example_error_handling()
    example_batch_convert()
    print("所有示例执行完毕。")
