# -*- coding: utf-8 -*-
"""DanmakuFactory 动态库 FFI 冒烟测试（Python ctypes）。

验证内容：
1. 动态库可被加载，所有公开符号均存在且签名可用；
2. df_get_version 返回非空版本串；
3. df_run_from_json 将 tests/test🚩.xml 转换为 ass，返回 DF_SUCCESS 且输出文件生成；
4. 错误路径下返回非零错误码，且 df_get_last_error_full_message 返回可读消息；
5. 标准化句柄 API（df_create/df_run/df_destroy）完整生命周期测试。

用法：python tests/ffi_smoke.py [dll_path]
"""

import ctypes
import json
import sys
import tempfile
import threading
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

DF_SUCCESS = 0

EXPORTED_SYMBOLS = [
    "df_get_version",
    "df_run_from_json",
    "df_get_last_error_code",
    "df_get_last_error_message",
    "df_get_last_error_file",
    "df_get_last_error_function",
    "df_get_last_error_line",
    "df_get_last_error_full_message",
    # 标准化句柄 API
    "df_create",
    "df_destroy",
    "df_run",
    "df_load_config_json",
    "df_load_config_file",
    "df_add_input_file",
    "df_add_input_file_typed",
    "df_clear_input_files",
    "df_set_output_file",
    "df_set_output_file_typed",
    "df_set_resolution",
    "df_set_display_area",
    "df_set_scroll_area",
    "df_set_statistics_mode",
    "df_set_density",
    "df_set_scroll_duration",
    "df_set_fixed_duration",
    "df_set_show_username",
    "df_set_block_mode",
    "df_set_keep_blocked",
    "df_set_fontname",
    "df_set_font_size",
    "df_set_font_size_strict",
    "df_set_font_size_norm",
    "df_set_bold",
    "df_set_shadow",
    "df_set_opacity",
    "df_set_outline",
    "df_set_outline_blur",
    "df_set_outline_opacity",
    "df_set_line_spacing",
    "df_set_msgbox_show",
    "df_set_msgbox_size",
    "df_set_msgbox_pos",
    "df_set_msgbox_font_size",
    "df_set_msgbox_duration",
    "df_set_msgbox_gift_min_price",
]


def find_library() -> Path:
    if len(sys.argv) > 1:
        return Path(sys.argv[1])
    patterns = ["DanmakuFactory.dll", "libDanmakuFactory.so", "libDanmakuFactory.dylib"]
    for pattern in patterns:
        for candidate in sorted((REPO_ROOT / "build").rglob(pattern)):
            return candidate
    raise SystemExit("FAIL: shared library not found under build/, run `xmake build lib-shared` first")


def load_library(lib_path: Path) -> ctypes.CDLL:
    lib = ctypes.CDLL(str(lib_path))

    # 符号存在性检查
    for symbol in EXPORTED_SYMBOLS:
        assert hasattr(lib, symbol), f"missing exported symbol: {symbol}"

    # 签名声明
    lib.df_get_version.restype = ctypes.c_char_p
    lib.df_get_version.argtypes = []
    lib.df_run_from_json.restype = ctypes.c_int
    lib.df_run_from_json.argtypes = [ctypes.c_char_p]
    lib.df_get_last_error_code.restype = ctypes.c_int
    lib.df_get_last_error_line.restype = ctypes.c_int
    for symbol in ("df_get_last_error_message", "df_get_last_error_file",
                   "df_get_last_error_function", "df_get_last_error_full_message"):
        getattr(lib, symbol).restype = ctypes.c_char_p

    # 标准化句柄 API 签名
    lib.df_create.restype = ctypes.c_void_p
    lib.df_create.argtypes = []
    lib.df_destroy.restype = None
    lib.df_destroy.argtypes = [ctypes.c_void_p]
    lib.df_run.restype = ctypes.c_int
    lib.df_run.argtypes = [ctypes.c_void_p]
    lib.df_add_input_file.restype = ctypes.c_int
    lib.df_add_input_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float]
    lib.df_clear_input_files.restype = ctypes.c_int
    lib.df_clear_input_files.argtypes = [ctypes.c_void_p]
    lib.df_set_output_file.restype = ctypes.c_int
    lib.df_set_output_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.df_set_font_size.restype = ctypes.c_int
    lib.df_set_font_size.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.df_set_resolution.restype = ctypes.c_int
    lib.df_set_resolution.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
    lib.df_load_config_json.restype = ctypes.c_int
    lib.df_load_config_json.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.df_load_config_file.restype = ctypes.c_int
    lib.df_load_config_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.df_add_input_file_typed.restype = ctypes.c_int
    lib.df_add_input_file_typed.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float, ctypes.c_char_p]
    lib.df_set_output_file_typed.restype = ctypes.c_int
    lib.df_set_output_file_typed.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

    return lib


def to_json_bytes(runtime_info: dict) -> bytes:
    return json.dumps(runtime_info, ensure_ascii=False).encode("utf-8")


def test_version(lib: ctypes.CDLL) -> None:
    version = lib.df_get_version()
    assert version, "df_get_version returned NULL/empty"
    print(f"PASS: df_get_version() = {version.decode()}")


def test_convert_success(lib: ctypes.CDLL, work_dir: Path) -> None:
    input_xml = REPO_ROOT / "tests" / "test🚩.xml"
    assert input_xml.is_file(), f"test resource missing: {input_xml}"
    output_ass = work_dir / "ffi_smoke_out.ass"

    runtime_info = {
        "input_files": [str(input_xml).replace("\\", "/")],
        "output_file": str(output_ass).replace("\\", "/"),
    }
    code = lib.df_run_from_json(to_json_bytes(runtime_info))
    if code != DF_SUCCESS:
        message = (lib.df_get_last_error_full_message() or b"").decode("utf-8", "replace")
        raise AssertionError(f"df_run_from_json failed with code {code}: {message}")
    assert output_ass.is_file(), f"output file not generated: {output_ass}"
    assert output_ass.stat().st_size > 0, "output file is empty"
    print(f"PASS: xml -> ass converted, output size = {output_ass.stat().st_size} bytes")


def test_error_path(lib: ctypes.CDLL, work_dir: Path) -> None:
    runtime_info = {
        "input_files": [str(work_dir / "no_such_file.xml").replace("\\", "/")],
        "output_file": str(work_dir / "never.ass").replace("\\", "/"),
    }
    code = lib.df_run_from_json(to_json_bytes(runtime_info))
    assert code != DF_SUCCESS, "expected failure for nonexistent input file"
    assert code == lib.df_get_last_error_code(), "df_get_last_error_code mismatch"
    message = lib.df_get_last_error_full_message()
    assert message, "df_get_last_error_full_message returned NULL"
    text = message.decode("utf-8", "replace")
    assert text.strip(), "error message is blank"
    print(f"PASS: error path code = {code}, message = {text.strip()}")


def test_handle_api_success(lib: ctypes.CDLL, work_dir: Path) -> None:
    """标准化句柄 API：create → configure → run → destroy 完整生命周期"""
    input_xml = REPO_ROOT / "tests" / "test🚩.xml"
    output_ass = work_dir / "handle_api_out.ass"

    ctx = lib.df_create()
    assert ctx, "df_create() returned NULL"

    # 配置
    code = lib.df_add_input_file(ctx, str(input_xml).replace("\\", "/").encode("utf-8"), 0.0)
    assert code == DF_SUCCESS, f"df_add_input_file failed: {code}"

    code = lib.df_set_output_file(ctx, str(output_ass).replace("\\", "/").encode("utf-8"))
    assert code == DF_SUCCESS, f"df_set_output_file failed: {code}"

    code = lib.df_set_font_size(ctx, 25)
    assert code == DF_SUCCESS, f"df_set_font_size failed: {code}"

    code = lib.df_set_resolution(ctx, 1920, 1080)
    assert code == DF_SUCCESS, f"df_set_resolution failed: {code}"

    # 执行
    code = lib.df_run(ctx)
    if code != DF_SUCCESS:
        message = (lib.df_get_last_error_full_message() or b"").decode("utf-8", "replace")
        lib.df_destroy(ctx)
        raise AssertionError(f"df_run failed with code {code}: {message}")

    # 销毁
    lib.df_destroy(ctx)

    assert output_ass.is_file(), f"output file not generated: {output_ass}"
    assert output_ass.stat().st_size > 0, "output file is empty"
    print(f"PASS: handle API xml -> ass converted, output size = {output_ass.stat().st_size} bytes")


def test_handle_api_error(lib: ctypes.CDLL, work_dir: Path) -> None:
    """标准化句柄 API：错误路径（不存在的输入文件）"""
    ctx = lib.df_create()
    assert ctx, "df_create() returned NULL"

    code = lib.df_add_input_file(ctx, str(work_dir / "ghost.xml").replace("\\", "/").encode("utf-8"), 0.0)
    assert code == DF_SUCCESS

    code = lib.df_set_output_file(ctx, str(work_dir / "out.ass").replace("\\", "/").encode("utf-8"))
    assert code == DF_SUCCESS

    code = lib.df_run(ctx)
    assert code != DF_SUCCESS, "expected failure for nonexistent input file"

    message = lib.df_get_last_error_full_message()
    assert message, "error message is NULL"
    text = message.decode("utf-8", "replace")
    assert text.strip(), "error message is blank"

    lib.df_destroy(ctx)
    # 二次 destroy 应安全（NULL 无操作）
    lib.df_destroy(None)
    print(f"PASS: handle API error path code = {code}, message = {text.strip()}")


def test_load_config_json(lib: ctypes.CDLL, work_dir: Path) -> None:
    """批量配置加载：df_load_config_json / df_load_config_file"""
    input_xml = REPO_ROOT / "tests" / "test🚩.xml"
    output_ass = work_dir / "bulk_cfg_out.ass"

    ctx = lib.df_create()
    assert ctx

    # 裸配置格式
    cfg = b'{"text": {"font_size": 25, "bold": true}, "screen": {"resolution": [1280, 720]}}'
    code = lib.df_load_config_json(ctx, cfg)
    assert code == DF_SUCCESS, f"df_load_config_json failed: {code}"

    # 包装格式
    cfg2 = b'{"config": {"danmaku": {"scroll_duration": 8.0}}}'
    code = lib.df_load_config_json(ctx, cfg2)
    assert code == DF_SUCCESS, f"df_load_config_json (wrapped) failed: {code}"

    # 非法 JSON 应被拒绝
    code = lib.df_load_config_json(ctx, b"{bad json!!!")
    assert code != DF_SUCCESS, "expected error for invalid JSON"

    # 非对象 root 应被拒绝
    code = lib.df_load_config_json(ctx, b"[1,2,3]")
    assert code != DF_SUCCESS, "expected error for non-object root"

    # 正常执行转换
    lib.df_add_input_file(ctx, str(input_xml).replace("\\", "/").encode("utf-8"), 0.0)
    lib.df_set_output_file(ctx, str(output_ass).replace("\\", "/").encode("utf-8"))
    code = lib.df_run(ctx)
    if code != DF_SUCCESS:
        msg = (lib.df_get_last_error_full_message() or b"").decode("utf-8", "replace")
        lib.df_destroy(ctx)
        raise AssertionError(f"df_run after bulk config failed: {code}: {msg}")
    lib.df_destroy(ctx)
    assert output_ass.is_file() and output_ass.stat().st_size > 0
    print(f"PASS: load_config_json + run, output size = {output_ass.stat().st_size} bytes")

    # df_load_config_file：不存在的文件应报错
    ctx = lib.df_create()
    code = lib.df_load_config_file(ctx, b"no_such_config.json")
    assert code != DF_SUCCESS, "expected error for missing config file"
    lib.df_destroy(ctx)
    print(f"PASS: load_config_file error path code = {code}")


def test_explicit_filetype(lib: ctypes.CDLL, work_dir: Path) -> None:
    """显式文件格式：JSON 桥接 type 键 + 句柄 API typed 变体（对应 df_fileinfo_t.type）"""
    input_xml = REPO_ROOT / "tests" / "test🚩.xml"

    # 1. JSON 桥接：无扩展名输出文件 + 显式 type
    out_noext = work_dir / "explicit_type_noext"
    runtime_info = {
        "input_files": [{"path": str(input_xml).replace("\\", "/"), "type": "xml"}],
        "output_file": {"path": str(out_noext).replace("\\", "/"), "type": "ass"},
    }
    code = lib.df_run_from_json(to_json_bytes(runtime_info))
    if code != DF_SUCCESS:
        msg = (lib.df_get_last_error_full_message() or b"").decode("utf-8", "replace")
        raise AssertionError(f"explicit type via JSON failed: {code}: {msg}")
    assert out_noext.is_file() and out_noext.stat().st_size > 0
    print(f"PASS: JSON explicit type (no-ext output), size = {out_noext.stat().st_size}")

    # 2. JSON 桥接：非法 type 值应被拒绝
    runtime_info["output_file"] = {"path": str(out_noext).replace("\\", "/"), "type": "docx"}
    code = lib.df_run_from_json(to_json_bytes(runtime_info))
    assert code != DF_SUCCESS, "expected error for invalid explicit type"
    print(f"PASS: JSON invalid type rejected, code = {code}")

    # 3. 句柄 API：typed 变体
    out_typed = work_dir / "typed_handle_out"
    ctx = lib.df_create()
    code = lib.df_add_input_file_typed(ctx, str(input_xml).replace("\\", "/").encode("utf-8"), 0.0, b"xml")
    assert code == DF_SUCCESS, f"df_add_input_file_typed failed: {code}"
    code = lib.df_set_output_file_typed(ctx, str(out_typed).replace("\\", "/").encode("utf-8"), b"ass")
    assert code == DF_SUCCESS, f"df_set_output_file_typed failed: {code}"
    code = lib.df_run(ctx)
    if code != DF_SUCCESS:
        msg = (lib.df_get_last_error_full_message() or b"").decode("utf-8", "replace")
        lib.df_destroy(ctx)
        raise AssertionError(f"df_run with typed files failed: {code}: {msg}")
    lib.df_destroy(ctx)
    assert out_typed.is_file() and out_typed.stat().st_size > 0
    print(f"PASS: handle API typed variants, size = {out_typed.stat().st_size}")

    # 4. 句柄 API：非法 type 应被拒绝
    ctx = lib.df_create()
    code = lib.df_set_output_file_typed(ctx, b"whatever", b"pdf")
    assert code != DF_SUCCESS, "expected error for invalid type in typed API"
    lib.df_destroy(ctx)
    print(f"PASS: handle API invalid type rejected, code = {code}")


def test_thread_safety(lib: ctypes.CDLL, work_dir: Path) -> None:
    """多线程并发调用 df_run_from_json，验证 TLS 错误隔离与无共享状态竞争。"""
    input_xml = REPO_ROOT / "tests" / "test🚩.xml"
    num_threads = 8
    results = [None] * num_threads
    barrier = threading.Barrier(num_threads)

    def worker(idx: int) -> None:
        # 每个线程使用独立的输出文件
        out = work_dir / f"thread_{idx}.ass"
        if idx % 2 == 0:
            # 成功路径
            ri = {"input_files": [str(input_xml).replace("\\", "/")],
                  "output_file": str(out).replace("\\", "/")}
        else:
            # 错误路径（不存在的输入）
            ri = {"input_files": [str(work_dir / f"missing_{idx}.xml").replace("\\", "/")],
                  "output_file": str(out).replace("\\", "/")}
        payload = json.dumps(ri, ensure_ascii=False).encode("utf-8")
        barrier.wait()  # 同步起跑，最大化并发压力
        code = lib.df_run_from_json(payload)
        # 立即读取本线程 TLS 错误信息
        err_code = lib.df_get_last_error_code()
        msg = (lib.df_get_last_error_full_message() or b"").decode("utf-8", "replace")
        results[idx] = (code, err_code, msg, out)

    threads = [threading.Thread(target=worker, args=(i,)) for i in range(num_threads)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    for idx, (code, err_code, msg, out) in enumerate(results):
        assert code is not None, f"thread {idx} did not complete"
        if idx % 2 == 0:
            assert code == DF_SUCCESS, f"thread {idx} expected success, got {code}: {msg}"
            assert out.is_file() and out.stat().st_size > 0, f"thread {idx} output missing"
        else:
            assert code != DF_SUCCESS, f"thread {idx} expected failure"
            assert err_code == code, f"thread {idx} TLS error code mismatch: {err_code} != {code}"
            assert msg.strip(), f"thread {idx} error message is blank"
    print(f"PASS: {num_threads} threads concurrent (4 success + 4 error), TLS isolation verified")


def main() -> None:
    lib_path = find_library()
    print(f"Loading: {lib_path}")
    lib = load_library(lib_path)
    print(f"PASS: all {len(EXPORTED_SYMBOLS)} symbols present")

    with tempfile.TemporaryDirectory(prefix="df_ffi_smoke_") as tmp:
        work_dir = Path(tmp)
        test_version(lib)
        test_convert_success(lib, work_dir)
        test_error_path(lib, work_dir)
        test_handle_api_success(lib, work_dir)
        test_handle_api_error(lib, work_dir)
        test_load_config_json(lib, work_dir)
        test_explicit_filetype(lib, work_dir)
        test_thread_safety(lib, work_dir)

    print("ALL TESTS PASSED")


if __name__ == "__main__":
    main()
