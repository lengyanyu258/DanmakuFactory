# -*- coding: utf-8 -*-
"""DanmakuFactory Python 绑定测试

运行方式:
    cd DanmakuFactory
    python -m pytest bindings/python/test_binding.py -v
    # 或直接运行:
    python bindings/python/test_binding.py
"""

import sys
import tempfile
from pathlib import Path


def _enable_utf8_console():
    """将 Windows 控制台切换为 UTF-8 码页并重配置 stdout/stderr。

    避免打印 emoji/中文等非 ASCII 字符时出现 UnicodeEncodeError 或乱码。
    非 Windows 平台终端通常已是 UTF-8，仅做 stdout 重配置。
    """
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

# 确保能导入绑定模块
sys.path.insert(0, str(Path(__file__).parent))

from danmakufactory import DanmakuFactory, DanmakuFactoryContext, DanmakuFactoryError

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
TEST_XML = REPO_ROOT / "tests" / "test🚩.xml"
FIXTURE_XML = REPO_ROOT / "tests" / "fixtures" / "sample.xml"


def test_version():
    """测试版本查询"""
    df = DanmakuFactory()
    version = df.version
    assert version, "version should not be empty"
    assert "." in version, f"version should contain dot: {version}"
    print(f"PASS: version = {version}")


def test_convert_xml_to_ass():
    """测试 XML → ASS 转换"""
    df = DanmakuFactory()
    assert TEST_XML.is_file(), f"test resource missing: {TEST_XML}"

    with tempfile.TemporaryDirectory(prefix="df_py_test_") as tmp:
        output = Path(tmp) / "output.ass"
        df.convert(TEST_XML, output)
        assert output.is_file(), "output file not generated"
        assert output.stat().st_size > 0, "output file is empty"
        print(f"PASS: xml -> ass, size = {output.stat().st_size} bytes")


def test_convert_with_config():
    """测试带配置的转换"""
    df = DanmakuFactory()

    with tempfile.TemporaryDirectory(prefix="df_py_test_") as tmp:
        output = Path(tmp) / "output_config.ass"
        config = {
            "text": {"font_size": 25, "fontname": "SimHei"},
            "danmaku": {"scroll_duration": 8.0},
        }
        df.convert(TEST_XML, output, config=config)
        assert output.is_file(), "output file not generated"
        assert output.stat().st_size > 0, "output file is empty"
        print(f"PASS: convert with config, size = {output.stat().st_size} bytes")


def test_error_nonexistent_file():
    """测试错误路径：不存在的输入文件"""
    df = DanmakuFactory()

    with tempfile.TemporaryDirectory(prefix="df_py_test_") as tmp:
        output = Path(tmp) / "never.ass"
        try:
            df.convert(Path(tmp) / "no_such_file.xml", output)
            assert False, "should have raised DanmakuFactoryError"
        except DanmakuFactoryError as e:
            assert e.code != 0, "error code should be non-zero"
            assert e.message, "error message should not be empty"
            print(f"PASS: error path, code = {e.code}")


def test_error_invalid_json():
    """测试错误路径：无效 JSON"""
    df = DanmakuFactory()
    code = df.convert_raw_json("{invalid json!!!")
    assert code != 0, "should return non-zero for invalid JSON"
    print(f"PASS: invalid json, code = {code}")


# ---- 标准化句柄 API 测试 ----


def test_context_convert():
    """句柄 API：基础转换"""
    assert FIXTURE_XML.is_file(), f"fixture missing: {FIXTURE_XML}"
    with tempfile.TemporaryDirectory(prefix="df_py_ctx_") as tmp:
        output = Path(tmp) / "ctx_output.ass"
        with DanmakuFactoryContext() as ctx:
            ctx.add_input_file(FIXTURE_XML)
            ctx.set_output_file(output)
            ctx.set_font_size(25)
            ctx.run()
        assert output.is_file() and output.stat().st_size > 0
        print(f"PASS: context convert, size = {output.stat().st_size} bytes")


def test_context_setters():
    """句柄 API：覆盖各分组 setter"""
    with tempfile.TemporaryDirectory(prefix="df_py_ctx_") as tmp:
        output = Path(tmp) / "ctx_setters.ass"
        with DanmakuFactoryContext() as ctx:
            ctx.add_input_file(FIXTURE_XML)
            ctx.set_output_file(output)
            # 屏幕
            ctx.set_resolution(1280, 720)
            ctx.set_display_area(0.8)
            ctx.set_scroll_area(0.9)
            # 弹幕
            ctx.set_density(-1)
            ctx.set_scroll_duration(9.0)
            ctx.set_fixed_duration(4.0)
            ctx.set_show_username(True)
            ctx.set_keep_blocked(False)
            # 文本
            ctx.set_fontname("Microsoft YaHei")
            ctx.set_font_size(30)
            ctx.set_bold(True)
            ctx.set_shadow(0.5)
            ctx.set_opacity(200)
            ctx.set_outline(1.0)
            ctx.set_outline_blur(0.5)
            ctx.set_outline_opacity(220)
            ctx.set_line_spacing(2)
            # 消息框
            ctx.set_msgbox_show(True)
            ctx.set_msgbox_size(400, 800)
            ctx.set_msgbox_pos(10, 10)
            ctx.set_msgbox_font_size(20)
            ctx.set_msgbox_duration(3.0)
            ctx.set_msgbox_gift_min_price(1.0)
            ctx.run()
        assert output.is_file() and output.stat().st_size > 0
        print("PASS: context setters (all groups)")


def test_context_load_config_json():
    """句柄 API：批量配置加载（裸格式 + 包装格式）"""
    with tempfile.TemporaryDirectory(prefix="df_py_ctx_") as tmp:
        output = Path(tmp) / "ctx_loadcfg.ass"
        with DanmakuFactoryContext() as ctx:
            ctx.add_input_file(FIXTURE_XML)
            ctx.set_output_file(output)
            ctx.load_config_json('{"text": {"font_size": 30}}')
            ctx.load_config_json('{"config": {"danmaku": {"scroll_duration": 7.0}}}')
            ctx.run()
        assert output.is_file() and output.stat().st_size > 0
        print("PASS: context load_config_json")


def test_context_invalid_config():
    """句柄 API：非法配置应抛异常"""
    with DanmakuFactoryContext() as ctx:
        try:
            ctx.load_config_json("{invalid json!!!")
            assert False, "should have raised DanmakuFactoryError"
        except DanmakuFactoryError as e:
            assert e.code != 0
            assert e.message, "error message should not be empty"
            print(f"PASS: context invalid config, code = {e.code}")


def test_context_multiple_inputs():
    """句柄 API：多输入文件 + clear"""
    with tempfile.TemporaryDirectory(prefix="df_py_ctx_") as tmp:
        output = Path(tmp) / "ctx_multi.ass"
        with DanmakuFactoryContext() as ctx:
            ctx.add_input_file(FIXTURE_XML)
            ctx.add_input_file(FIXTURE_XML, time_shift=1.0)
            ctx.set_output_file(output)
            ctx.run()
        assert output.is_file() and output.stat().st_size > 0
        print("PASS: context multiple inputs")


def test_emoji_filename():
    """emoji 文件名转换：输入与输出均含 emoji，验证 UTF-8 路径支持"""
    import shutil

    assert FIXTURE_XML.is_file(), f"fixture missing: {FIXTURE_XML}"

    with tempfile.TemporaryDirectory(prefix="df_py_emoji_") as tmp:
        tmp_path = Path(tmp)
        emoji_input = tmp_path / "输入🚩.xml"
        emoji_output = tmp_path / "输出🎉.ass"
        shutil.copy(FIXTURE_XML, emoji_input)

        # JSON 一次性 API
        df = DanmakuFactory()
        df.convert(emoji_input, emoji_output)
        assert emoji_output.is_file(), "emoji output not generated"
        assert emoji_output.stat().st_size > 0, "emoji output is empty"

        # 句柄 API 再验证一次
        emoji_output2 = tmp_path / "输出2🚀.ass"
        with DanmakuFactoryContext() as ctx:
            ctx.add_input_file(emoji_input)
            ctx.set_output_file(emoji_output2)
            ctx.run()
        assert emoji_output2.is_file() and emoji_output2.stat().st_size > 0

        print("PASS: emoji filename convert (input + output)")


def main():
    """直接运行时的入口"""
    print(f"Test XML exists: {TEST_XML.is_file()}")
    print()

    # JSON 一次性 API
    test_version()
    test_convert_xml_to_ass()
    test_convert_with_config()
    test_error_nonexistent_file()
    test_error_invalid_json()

    # 标准化句柄 API
    test_context_convert()
    test_context_setters()
    test_context_load_config_json()
    test_context_invalid_config()
    test_context_multiple_inputs()

    # emoji 文件名（UTF-8 路径）
    test_emoji_filename()

    print("\nALL TESTS PASSED")


if __name__ == "__main__":
    main()
