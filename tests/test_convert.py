# -*- coding: utf-8 -*-
"""DanmakuFactory 格式转换回归测试

覆盖所有输入/输出格式组合，验证重构后功能与原版一致。

运行: python tests/test_convert.py
"""

import ctypes
import json
import sys
import tempfile
import xml.etree.ElementTree as ET
import xml.etree.ElementTree as ET
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


def run_convert(lib, input_files, output_file, config=None, time_shift=0.0):
    """执行转换，返回 (code, error_message)"""
    if isinstance(input_files, (str, Path)):
        input_files = [input_files]

    runtime_info = {
        "input_files": [
            {"path": str(f).replace("\\", "/"), "time_shift": time_shift}
            for f in input_files
        ],
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


def test_xml_to_ass(lib, tmp):
    """XML -> ASS"""
    out = tmp / "xml_to_ass.ass"
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out)
    assert code == DF_SUCCESS, f"XML->ASS failed: {msg}"
    assert out.is_file() and out.stat().st_size > 0
    content = out.read_text(encoding="utf-8-sig")
    assert "[Script Info]" in content
    assert "[Events]" in content
    print("PASS: XML -> ASS")


def test_xml_to_json(lib, tmp):
    """XML -> JSON"""
    out = tmp / "xml_to_json.json"
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out)
    assert code == DF_SUCCESS, f"XML->JSON failed: {msg}"
    records = json.loads(out.read_text(encoding="utf-8-sig"))[2]
    source_records = ET.parse(FIXTURES / "sample.xml").getroot().findall("d")
    assert len(records) == len(source_records)
    for actual, source in zip(records, source_records):
        metadata = actual["c"].split(",")
        original = source.attrib["p"].split(",")
        assert float(metadata[0]) == float(original[0]), "JSON must preserve each timestamp"
        assert int(metadata[1]) == int(original[3]), "JSON must preserve each color"
        assert int(metadata[2]) == int(original[1]), "JSON must preserve each mode"
        assert int(metadata[3]) == int(original[2]), "JSON must preserve each font size"
        assert actual["m"] == source.text
    print("PASS: XML -> JSON preserves every record")


def test_xml_to_xml(lib, tmp):
    """XML -> XML (roundtrip)"""
    out = tmp / "xml_to_xml.xml"
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out)
    assert code == DF_SUCCESS, f"XML->XML failed: {msg}"
    assert out.is_file() and out.stat().st_size > 0
    content = out.read_text(encoding="utf-8")
    assert "<d p=" in content or "<d " in content
    print("PASS: XML -> XML")


def test_json_to_ass(lib, tmp):
    """JSON -> ASS"""
    out = tmp / "json_to_ass.ass"
    code, msg = run_convert(lib, FIXTURES / "sample.json", out)
    assert code == DF_SUCCESS, f"JSON->ASS failed: {msg}"
    assert out.is_file() and out.stat().st_size > 0
    print("PASS: JSON -> ASS")


def test_json_to_xml(lib, tmp):
    """JSON -> XML"""
    out = tmp / "json_to_xml.xml"
    code, msg = run_convert(lib, FIXTURES / "sample.json", out)
    assert code == DF_SUCCESS, f"JSON->XML failed: {msg}"
    assert out.is_file() and out.stat().st_size > 0
    print("PASS: JSON -> XML")


def test_flat_json_array(lib, tmp):
    source = tmp / "flat.json"
    records = [
        {"c": "1,16777215,1,25,,,", "m": "first"},
        {"c": "2.5,255,6,30,,,", "m": "second"},
        {"c": "3,65280,5,18,,,", "m": "third"},
    ]
    source.write_text(json.dumps(records), encoding="utf-8")
    out = tmp / "flat_roundtrip.json"
    code, msg = run_convert(lib, source, out)
    assert code == DF_SUCCESS, msg
    result = json.loads(out.read_text(encoding="utf-8-sig"))[2]
    assert result == records, "Flat arrays containing multiple comments must round-trip"
    print("PASS: Flat JSON array with multiple comments")

def test_multi_file_merge(lib, tmp):
    """多文件合并输入"""
    out = tmp / "merged.ass"
    code, msg = run_convert(lib, [FIXTURES / "sample.xml", FIXTURES / "sample.json"], out)
    assert code == DF_SUCCESS, f"Multi-file merge failed: {msg}"
    assert out.is_file() and out.stat().st_size > 0
    print("PASS: Multi-file merge")


def test_time_shift(lib, tmp):
    """时轴偏移"""
    out = tmp / "shifted.ass"
    code, msg = run_convert(lib, FIXTURES / "sample.xml", out, time_shift=10.0)
    assert code == DF_SUCCESS, f"Time shift failed: {msg}"
    assert out.is_file() and out.stat().st_size > 0
    print("PASS: Time shift")


def test_error_nonexistent(lib, tmp):
    """错误路径：不存在的文件"""
    out = tmp / "never.ass"
    code, msg = run_convert(lib, tmp / "no_such.xml", out)
    assert code != DF_SUCCESS, "Should fail for nonexistent file"
    assert msg, "Error message should not be empty"
    print("PASS: Error - nonexistent file")


def test_error_invalid_format(lib, tmp):
    """错误路径：无效格式文件"""
    bad_file = tmp / "bad.xyz"
    bad_file.write_text("not a danmaku file", encoding="utf-8")
    out = tmp / "bad_out.ass"
    code, msg = run_convert(lib, bad_file, out)
    assert code != DF_SUCCESS, "Should fail for unknown format"
    print("PASS: Error - invalid format")


def test_error_nonexistent_ass(lib, tmp):
    """ASS 读取失败必须返回文件错误，不能处理未初始化中间结构。"""
    out = tmp / "missing_ass_out.ass"
    code, msg = run_convert(lib, tmp / "no_such.ass", out)
    assert code == -1001, f"Missing ASS should report FILE_NOT_FOUND: {code}: {msg}"
    assert msg and not out.exists(), "Failed ASS input must not create an output"
    print("PASS: Error - nonexistent ASS")


def test_two_descending_danmaku(lib, tmp):
    """仅两个节点也应比较第一对，并按时间排序输出。"""
    source = tmp / "descending.xml"
    source.write_text(
        '<i><d p="10,1,25,16777215,0,0,0,0">Late</d>'
        '<d p="1,1,25,16777215,0,0,0,0">Early</d></i>',
        encoding="utf-8",
    )
    out = tmp / "sorted.xml"
    code, msg = run_convert(lib, source, out)
    assert code == DF_SUCCESS, f"Descending conversion failed: {msg}"
    nodes = ET.parse(out).getroot().findall("d")
    assert [node.text for node in nodes] == ["Early", "Late"]
    assert [float(node.attrib["p"].split(",")[0]) for node in nodes] == [1.0, 10.0]
    print("PASS: First pair sorted")


def test_long_special_ass_roundtrip(lib, tmp):
    """超过 1024 字节的特殊弹幕不应复制到固定的 ASS 转义缓冲区。"""
    long_text = "Special" + "x" * 1500
    source = tmp / "long_special.ass"
    source.write_text(
        "[Script Info]\nScriptType: v4.00+\nPlayResX: 640\nPlayResY: 480\n"
        "[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, "
        "BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, "
        "BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
        "Style: SP,Arial,25,&H00FFFFFF,&H00FFFFFF,&H00000000,&H00000000,"
        "0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1\n"
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
        "Dialogue: 0,0:00:01.00,0:00:02.00,SP,,0000,0000,0000,,{\\pos(320,240)}"
        + long_text + "\n",
        encoding="utf-8",
    )
    out = tmp / "long_special_out.ass"
    code, msg = run_convert(lib, source, out)
    assert code == DF_SUCCESS, f"Long special ASS failed: {msg}"
    content = out.read_text(encoding="utf-8-sig")
    assert long_text in content, "Special text should survive ASS round-trip without truncation"
    print("PASS: Long special ASS round-trip")


def test_small_display_area_collision(lib, tmp):
    """正比例不足一像素、固定弹幕占满区域时也须保持数组索引有效。"""
    source = tmp / "tiny_area.xml"
    records = [(1, "ScrollA"), (1, "ScrollB"), (5, "TopA"), (5, "TopB"),
               (4, "BottomA"), (4, "BottomB")]
    source.write_text(
        "<i>" + "".join(
            f'<d p="1,{mode},25,16777215,0,0,0,0">{text}</d>' for mode, text in records
        ) + "</i>",
        encoding="utf-8",
    )
    out = tmp / "tiny_area.ass"
    code, msg = run_convert(lib, source, out, config={
        "screen": {"resolution": [640, 2], "display_area": 0.001, "scroll_area": 0.001},
    })
    assert code == DF_SUCCESS, f"Tiny display area conversion failed: {msg}"
    content = out.read_text(encoding="utf-8-sig")
    for _, text in records:
        assert text in content, f"Missing collision fallback output: {text}"
    print("PASS: Small display area collision")

def test_ass_end_time_overflow(lib, tmp):
    """可表示的开始时间加上默认时长超界时，在创建 ASS 输出前拒绝。"""
    source = tmp / "near_time_limit.xml"
    source.write_text(
        '<i><d p="2147480,1,25,16777215,0,0,0,0">NearLimit</d></i>',
        encoding="utf-8",
    )
    xml_out = tmp / "near_time_limit_out.xml"
    code, msg = run_convert(lib, source, xml_out)
    assert code == DF_SUCCESS, f"Representable XML timestamp should survive: {msg}"
    ass_out = tmp / "overflow_end.ass"
    code, msg = run_convert(lib, source, ass_out)
    assert code == -1003, f"ASS end overflow should report invalid format: {code}: {msg}"
    assert msg and not ass_out.exists(), "Reject time overflow before output is created"
    print("PASS: ASS end time overflow rejected")

def main():
    lib = load_lib()
    print(f"Library: {find_library().name}")
    print(f"Fixtures: {FIXTURES}")
    print()

    with tempfile.TemporaryDirectory(prefix="df_regression_") as tmp_str:
        tmp = Path(tmp_str)
        test_xml_to_ass(lib, tmp)
        test_xml_to_json(lib, tmp)
        test_xml_to_xml(lib, tmp)
        test_json_to_ass(lib, tmp)
        test_json_to_xml(lib, tmp)
        test_flat_json_array(lib, tmp)
        test_multi_file_merge(lib, tmp)
        test_time_shift(lib, tmp)
        test_error_nonexistent(lib, tmp)
        test_error_invalid_format(lib, tmp)
        test_error_nonexistent_ass(lib, tmp)
        test_two_descending_danmaku(lib, tmp)
        test_long_special_ass_roundtrip(lib, tmp)
        test_small_display_area_collision(lib, tmp)
        test_ass_end_time_overflow(lib, tmp)

    print("\nALL CONVERSION TESTS PASSED")


if __name__ == "__main__":
    main()
