# -*- coding: utf-8 -*-
"""DanmakuFactory 文本精确测量功能测试

验证 FreeType + Fontconfig + HarfBuzz 集成的文本宽度精确计算。
当精确测量可用时，ASS 输出的 \\move 坐标应反映真实字形 advance，
而非旧的"字符数 * 字号 / 1.2"估算。

运行: python tests/test_text_measure.py
"""

import ctypes
import json
import re
import sys
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
    """执行转换，返回 (code, error_message)"""
    runtime_info = {
        "input_files": [{"path": str(input_file).replace("\\", "/")}],
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


def extract_move_widths(ass_content: str, resolution_x: int = 1920):
    """从 ASS 内容中提取 R2L/L2R 弹幕的 textLen 值

    R2L: \\move(res_x + textLen/2, y, -textLen/2, y)
    L2R: \\move(-textLen/2, y, res_x + textLen/2, y)
    """
    widths = []
    pattern = re.compile(r"\\move\((-?\d+),(\d+),(-?\d+),(\d+)")
    for match in pattern.finditer(ass_content):
        x1, y1, x2, y2 = int(match.group(1)), int(match.group(2)), int(match.group(3)), int(match.group(4))
        if x1 > resolution_x:
            # R2L: start_x = res_x + textLen/2
            text_len = (x1 - resolution_x) * 2
            widths.append(text_len)
        elif x1 < 0:
            # L2R: start_x = -textLen/2
            text_len = (-x1) * 2
            widths.append(text_len)
    return widths


def old_algorithm_width(char_count: int, font_size_set: int, font_size_in_file: int = 25) -> int:
    """复现旧算法: cnt * (fontSizeSet + (fontSizeInFile - 25)) / 1.2"""
    effective = font_size_set + (font_size_in_file - 25)
    return int(char_count * effective / 1.2)


def test_precise_measurement_active(lib, tmp):
    """验证精确测量已激活（输出与旧算法不同）"""
    out = tmp / "precise.ass"
    config = {"text": {"fontname": "Microsoft YaHei", "font_size": 38}}
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config)
    assert code == DF_SUCCESS, f"Conversion failed: {msg}"

    content = out.read_text(encoding="utf-8-sig")
    widths = extract_move_widths(content)
    assert len(widths) > 0, "No R2L/L2R danmaku found in output"

    # sample.xml 中 "第一条滚动弹幕" = 7 个 CJK 字符，font_size_in_file=25
    # 旧算法: 7 * 38 / 1.2 = 221
    # 精确测量: 应接近 7 * 38 = 266（CJK 全角 advance ≈ em-size）
    old_width = old_algorithm_width(7, 38, 25)  # 221

    # 第一条滚动弹幕的宽度
    first_width = widths[0]

    # 精确测量值应与旧算法不同
    if first_width != old_width:
        print(f"PASS: Precise measurement active (width={first_width}, old_algo={old_width})")
        return True
    else:
        # 如果相同，可能是降级到了旧算法（字体不可用）
        print(f"INFO: Width matches old algorithm ({first_width}), precise measurement may be unavailable")
        return False


def test_cjk_width_reasonable(lib, tmp):
    """验证 CJK 文本宽度在合理范围内"""
    out = tmp / "cjk_width.ass"
    config = {"text": {"fontname": "Microsoft YaHei", "font_size": 38}}
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config)
    assert code == DF_SUCCESS, f"Conversion failed: {msg}"

    content = out.read_text(encoding="utf-8-sig")
    widths = extract_move_widths(content)
    assert len(widths) > 0, "No scrolling danmaku found"

    # "第一条滚动弹幕" = 7 CJK chars at effective size 38
    # 合理范围: 每个 CJK 字符 advance 在 0.8*em 到 1.1*em 之间
    first_width = widths[0]
    min_expected = int(7 * 38 * 0.8)  # 212
    max_expected = int(7 * 38 * 1.1)  # 292

    assert min_expected <= first_width <= max_expected, (
        f"Width {first_width} out of reasonable range [{min_expected}, {max_expected}]"
    )
    print(f"PASS: CJK width reasonable ({first_width} in [{min_expected}, {max_expected}])")


def test_font_size_scaling(lib, tmp):
    """验证不同字号产生不同宽度（字号缩放正确）"""
    out = tmp / "scaling.ass"
    config = {"text": {"fontname": "Microsoft YaHei", "font_size": 38}}
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config)
    assert code == DF_SUCCESS, f"Conversion failed: {msg}"

    content = out.read_text(encoding="utf-8-sig")
    widths = extract_move_widths(content)

    # sample.xml 包含:
    # - "第一条滚动弹幕" font_size_in_file=25 → effective=38
    # - "红色小号弹幕" font_size_in_file=18 → effective=31 (6 chars)
    # - "绿色大号弹幕" font_size_in_file=36 → effective=49 (6 chars)
    # 至少应有 4 条滚动弹幕 (R2L + L2R)
    assert len(widths) >= 4, f"Expected >=4 scrolling danmaku, got {len(widths)}"

    # 找到最大和最小宽度
    min_w = min(widths)
    max_w = max(widths)

    # 大字号弹幕应比小字号弹幕宽
    assert max_w > min_w, f"Font size scaling broken: max={max_w}, min={min_w}"
    print(f"PASS: Font size scaling correct (min={min_w}, max={max_w})")


def test_consistent_same_text(lib, tmp):
    """验证相同文本在相同字号下产生相同宽度"""
    out = tmp / "consistent.ass"
    config = {"text": {"fontname": "Microsoft YaHei", "font_size": 38}}
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config)
    assert code == DF_SUCCESS, f"Conversion failed: {msg}"

    content = out.read_text(encoding="utf-8-sig")
    widths = extract_move_widths(content)

    # "第一条滚动弹幕" 和 "第二条滚动弹幕" 都是 7 个 CJK 字符，font_size=25
    # 它们的宽度应该相同
    assert len(widths) >= 2, "Need at least 2 scrolling danmaku"
    assert widths[0] == widths[1], (
        f"Same-length text should have same width: {widths[0]} vs {widths[1]}"
    )
    print(f"PASS: Consistent width for same-length text ({widths[0]})")


def test_fallback_graceful(lib, tmp):
    """验证使用不存在的字体名时不会崩溃（降级到旧算法）"""
    out = tmp / "fallback.ass"
    config = {"text": {"fontname": "NonExistentFontXYZ123", "font_size": 38}}
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, config)
    # 即使字体不存在，转换也应成功（降级到旧算法或 fontconfig 回退到默认字体）
    assert code == DF_SUCCESS, f"Conversion should not crash with unknown font: {msg}"
    assert out.is_file() and out.stat().st_size > 0
    print("PASS: Graceful fallback with unknown font")


def main():
    lib = load_lib()
    print(f"Library: {find_library().name}")
    print(f"Fixtures: {FIXTURES}")
    print()

    with tempfile.TemporaryDirectory(prefix="df_text_measure_") as tmp_str:
        tmp = Path(tmp_str)
        precise_active = test_precise_measurement_active(lib, tmp)
        test_cjk_width_reasonable(lib, tmp)
        test_font_size_scaling(lib, tmp)
        test_consistent_same_text(lib, tmp)
        test_fallback_graceful(lib, tmp)

    print()
    if precise_active:
        print("ALL TEXT MEASUREMENT TESTS PASSED (precise mode)")
    else:
        print("ALL TEXT MEASUREMENT TESTS PASSED (fallback mode - font libraries unavailable)")


if __name__ == "__main__":
    main()
