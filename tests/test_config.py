# -*- coding: utf-8 -*-
"""DanmakuFactory 配置解析回归测试

验证新旧格式配置文件解析、各配置项生效、无效配置容错。

运行: python tests/test_config.py
"""

import ctypes
import json
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FIXTURES = Path(__file__).resolve().parent / "fixtures"
DF_SUCCESS = 0


def find_library() -> Path:
    patterns = ["DanmakuFactory.dll", "libDanmakuFactory.so", "libDanmakuFactory.dylib"]
    for pattern in patterns:
        for candidate in sorted((REPO_ROOT / "build").rglob(pattern)):
            return candidate
    raise SystemExit("FAIL: shared library not found. Run `xmake build lib-shared` first.")


def load_lib() -> ctypes.CDLL:
    lib = ctypes.CDLL(str(find_library()))
    lib.df_get_version.restype = ctypes.c_char_p
    lib.df_run_from_json.restype = ctypes.c_int
    lib.df_run_from_json.argtypes = [ctypes.c_char_p]
    lib.df_get_last_error_full_message.restype = ctypes.c_char_p
    return lib


def run_convert(lib, input_file, output_file, config=None):
    runtime_info = {
        "input_files": [str(input_file).replace("\\", "/")],
        "output_file": str(output_file).replace("\\", "/"),
    }
    if config:
        runtime_info["config"] = config

    json_bytes = json.dumps(runtime_info, ensure_ascii=False).encode("utf-8")
    code = lib.df_run_from_json(json_bytes)
    msg = ""
    if code != 0:
        ptr = lib.df_get_last_error_full_message()
        msg = ptr.decode("utf-8", "replace") if ptr else ""
    return code, msg


def test_default_config(lib, tmp):
    """默认配置（不传 config）"""
    out = tmp / "default.ass"
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out)
    assert code == DF_SUCCESS, f"Default config failed: {msg}"
    content = out.read_text(encoding="utf-8-sig")
    # 默认字体应为 Microsoft YaHei
    assert "Microsoft YaHei" in content, "Default font should be Microsoft YaHei"
    print("PASS: Default config")


def test_custom_font(lib, tmp):
    """自定义字体配置"""
    out = tmp / "custom_font.ass"
    config = {"text": {"fontname": "SimHei", "font_size": 30}}
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config=config)
    assert code == DF_SUCCESS, f"Custom font failed: {msg}"
    content = out.read_text(encoding="utf-8-sig")
    assert "Style: R2L,SimHei,30," in content, "Custom font and size must reach the ASS style"
    print("PASS: Custom font config")


def test_resolution_config(lib, tmp):
    """分辨率配置"""
    out = tmp / "resolution.ass"
    config = {"screen": {"resolution": [1280, 720]}}
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config=config)
    assert code == DF_SUCCESS, f"Resolution config failed: {msg}"
    content = out.read_text(encoding="utf-8-sig")
    # ASS 中分辨率以 PlayResX/PlayResY 形式存储
    assert "PlayResX: 1280" in content and "PlayResY: 720" in content
    print("PASS: Resolution config")


def test_danmaku_config(lib, tmp):
    """弹幕参数配置"""
    out = tmp / "danmaku_cfg.ass"
    config = {
        "danmaku": {
            "scroll_duration": 8.0,
            "fixed_duration": 3.0,
            "density": -1,
        }
    }
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config=config)
    assert code == DF_SUCCESS, f"Danmaku config failed: {msg}"
    print("PASS: Danmaku config")


def test_block_mode(lib, tmp):
    """屏蔽模式配置"""
    out = tmp / "blocked.ass"
    config = {"danmaku": {"block_mode": ["R2L", "L2R"]}}
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config=config)
    assert code == DF_SUCCESS, f"Block mode failed: {msg}"
    print("PASS: Block mode config")


def test_msgbox_config(lib, tmp):
    """消息框配置"""
    out = tmp / "msgbox.ass"
    config = {
        "msgbox": {
            "show": True,
            "size": [400, 800],
            "pos": [10, 10],
            "font_size": 20,
        }
    }
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config=config)
    assert code == DF_SUCCESS, f"Msgbox config failed: {msg}"
    print("PASS: Msgbox config")


def test_old_format_config(lib, tmp):
    """真正的旧格式扁平配置与嵌套配置采用相同解析路径。"""
    out = tmp / "old_format.ass"
    config = {
        "resolution": [1280, 720],
        "fontname": "LegacyCaseFont",
        "fontsize": 30,
        "displayArea": 0.8,
        "msgboxfontsize": 20,
    }
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config=config)
    assert code == DF_SUCCESS, f"Old format config failed: {msg}"
    content = out.read_text(encoding="utf-8-sig")
    assert "PlayResX: 1280" in content and "PlayResY: 720" in content
    assert "Style: R2L,LegacyCaseFont,30," in content
    assert "Style: MSG,LegacyCaseFont,20," in content
    print("PASS: Old format config compatibility")


def test_nested_precedence(lib, tmp):
    out = tmp / "nested_precedence.ass"
    config = {
        "resolution": [1280, 720],
        "fontname": "FlatFont",
        "text": {"fontname": "NestedFont", "font_size": 32},
        "screen": {"resolution": [640, 480]},
    }
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config)
    assert code == DF_SUCCESS, msg
    content = out.read_text(encoding="utf-8-sig")
    assert "PlayResX: 640" in content and "PlayResY: 480" in content
    assert "Style: R2L,NestedFont,32," in content
    print("PASS: Nested fields override flat aliases")


def test_case_sensitive_strings(lib, tmp):
    source = tmp / "MixedCaseInput.xml"
    source.write_text(
        '<i><d p="1,1,25,16777215,0,0,0,0">KeepUpperCase</d>'
        '<d p="2,1,25,16777215,0,0,0,0">keepuppercase</d></i>',
        encoding="utf-8",
    )
    out = tmp / "MixedCaseOutput.ass"
    config = {
        "TEXT": {"FontName": "MixedCaseFont", "Font_Size": 30},
        "DANMAKU": {
            "BLOCK_LIST": ["^KeepUpperCase$"],
            "BLOCK_LIST_REGEX_ENABLED": True,
            "KEEP_BLOCKED": False,
        },
    }
    code, msg = run_convert(lib, source, out, config)
    assert code == DF_SUCCESS, msg
    content = out.read_text(encoding="utf-8-sig")
    assert "Style: R2L,MixedCaseFont,30," in content
    assert "KeepUpperCase" not in content and "keepuppercase" in content
    print("PASS: Keys ignore case while paths, fonts and regex values preserve case")


def test_invalid_numeric_boundaries(lib, tmp):
    configs = [
        {"text": {"font_size": 1e100}},
        {"screen": {"resolution": [1e100, 720]}},
        {"screen": {"resolution": "1920x-1080"}},
        {"screen": {"resolution": "999999999999999999999x1080"}},
        {"danmaku": {"block_mode": 256}},
        {"danmaku": {"scroll_duration": 1e30}},
        {"msgbox": {"duration": 1e30}},
        {"msgbox": {"gift_min_price": 1e30}},
        {"screen": 7},
    ]
    for index, config in enumerate(configs):
        out = tmp / f"invalid_boundary_{index}.ass"
        code, _ = run_convert(lib, FIXTURES / "sample.xml", out, config)
        assert code != DF_SUCCESS, f"Invalid config was accepted: {config}"
        assert not out.exists(), f"Invalid config created output: {config}"
    print("PASS: Invalid numeric ranges and coordinate signs are rejected")


def test_nonfinite_handle_values(lib, tmp):
    lib.df_create.restype = ctypes.c_void_p
    lib.df_destroy.argtypes = [ctypes.c_void_p]
    lib.df_add_input_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float]
    lib.df_set_output_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.df_set_scroll_duration.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.df_run.argtypes = [ctypes.c_void_p]
    lib.df_run.restype = ctypes.c_int

    for index, value in enumerate([float("nan"), float("inf"), -float("inf"), 1e30]):
        ctx = lib.df_create()
        assert ctx, "Context allocation failed"
        out = tmp / f"nonfinite_{index}.ass"
        try:
            assert lib.df_add_input_file(ctx, str(FIXTURES / "sample.xml").encode("utf-8"), 0) == DF_SUCCESS
            assert lib.df_set_output_file(ctx, str(out).encode("utf-8")) == DF_SUCCESS
            set_code = lib.df_set_scroll_duration(ctx, value)
            assert set_code != DF_SUCCESS or lib.df_run(ctx) != DF_SUCCESS, f"Unsafe duration accepted: {value}"
            assert not out.exists()
        finally:
            lib.df_destroy(ctx)
    print("PASS: Handle API rejects nonfinite and excessive durations")


def test_invalid_input_entries(lib, tmp):
    for entries in [[None], [{"type": "xml"}], [7], [{}], {"path": str(FIXTURES / "sample.xml")}]:
        out = tmp / "invalid_input.ass"
        payload = json.dumps({"input_files": entries, "output_file": str(out)}).encode("utf-8")
        assert lib.df_run_from_json(payload) != DF_SUCCESS, f"Invalid input entry accepted: {entries}"
        assert not out.exists()
    print("PASS: Invalid input arrays fail without partial conversion")

def test_invalid_config_graceful(lib, tmp):
    """无效配置值的容错"""
    out = tmp / "invalid_cfg.ass"
    # 传入一些无效值，库应该容错或使用默认值
    config = {
        "text": {"font_size": -999},
        "screen": {"display_area": 99.0},
    }
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config=config)
    # 可能成功（容错）或返回警告，但不应崩溃
    assert code != DF_SUCCESS and not out.exists()
    print("PASS: Invalid config graceful handling")


def test_empty_config(lib, tmp):
    """空配置对象"""
    out = tmp / "empty_cfg.ass"
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config={})
    assert code == DF_SUCCESS, f"Empty config failed: {msg}"
    print("PASS: Empty config")


def main():
    lib = load_lib()
    print(f"Library: {find_library().name}")
    print()

    with tempfile.TemporaryDirectory(prefix="df_config_test_") as tmp_str:
        tmp = Path(tmp_str)
        test_default_config(lib, tmp)
        test_custom_font(lib, tmp)
        test_resolution_config(lib, tmp)
        test_danmaku_config(lib, tmp)
        test_block_mode(lib, tmp)
        test_msgbox_config(lib, tmp)
        test_old_format_config(lib, tmp)
        test_nested_precedence(lib, tmp)
        test_case_sensitive_strings(lib, tmp)
        test_invalid_numeric_boundaries(lib, tmp)
        test_nonfinite_handle_values(lib, tmp)
        test_invalid_input_entries(lib, tmp)
        test_invalid_config_graceful(lib, tmp)
        test_empty_config(lib, tmp)

    print("\nALL CONFIG TESTS PASSED")


if __name__ == "__main__":
    main()
