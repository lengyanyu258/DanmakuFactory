# -*- coding: utf-8 -*-
"""DanmakuFactory Python 绑定

通过 ctypes 调用 DanmakuFactory 动态库，提供弹幕格式转换功能。

支持两种调用模式：

1. 标准化句柄 API（推荐）—— create → configure → run → destroy 生命周期：
    from danmakufactory import DanmakuFactoryContext

    with DanmakuFactoryContext() as ctx:
        ctx.add_input_file("input.xml")
        ctx.set_output_file("output.ass")
        ctx.set_font_size(25)
        ctx.run()

2. JSON 一次性 API（便捷）：
    from danmakufactory import DanmakuFactory

    df = DanmakuFactory()
    df.convert("input.xml", "output.ass")
    df.convert("input.xml", "output.ass", config={"text": {"font_size": 25}})

构建动态库:
    xmake build lib-shared
"""

import ctypes
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

__version__ = "2.0.0"
__all__ = ["DanmakuFactory", "DanmakuFactoryContext", "DanmakuFactoryError"]


class DanmakuFactoryError(Exception):
    """DanmakuFactory 库调用异常"""

    def __init__(self, code: int, message: str):
        self.code = code
        self.message = message
        super().__init__(f"[{code}] {message}")


class DanmakuFactory:
    """DanmakuFactory 动态库封装

    Attributes:
        version: 库版本字符串
    """

    def __init__(self, lib_path: Optional[Union[str, Path]] = None):
        """初始化并加载动态库

        Args:
            lib_path: 动态库路径。为 None 时自动搜索 build/ 目录。

        Raises:
            FileNotFoundError: 找不到动态库
            OSError: 加载动态库失败
        """
        if lib_path is None:
            lib_path = self._find_library()
        self._lib = ctypes.CDLL(str(lib_path))
        self._setup_signatures()

    @staticmethod
    def _find_library() -> Path:
        """自动搜索动态库"""
        repo_root = Path(__file__).resolve().parent.parent.parent
        patterns = ["DanmakuFactory.dll", "libDanmakuFactory.so", "libDanmakuFactory.dylib"]

        for pattern in patterns:
            for candidate in sorted((repo_root / "build").rglob(pattern)):
                return candidate

        raise FileNotFoundError(
            "DanmakuFactory shared library not found under build/. "
            "Run `xmake build lib-shared` first."
        )

    def _setup_signatures(self) -> None:
        """声明 C 函数签名"""
        lib = self._lib

        lib.df_get_version.restype = ctypes.c_char_p
        lib.df_get_version.argtypes = []

        lib.df_run_from_json.restype = ctypes.c_int
        lib.df_run_from_json.argtypes = [ctypes.c_char_p]

        lib.df_get_last_error_code.restype = ctypes.c_int
        lib.df_get_last_error_code.argtypes = []

        lib.df_get_last_error_message.restype = ctypes.c_char_p
        lib.df_get_last_error_message.argtypes = []

        lib.df_get_last_error_file.restype = ctypes.c_char_p
        lib.df_get_last_error_file.argtypes = []

        lib.df_get_last_error_function.restype = ctypes.c_char_p
        lib.df_get_last_error_function.argtypes = []

        lib.df_get_last_error_line.restype = ctypes.c_int
        lib.df_get_last_error_line.argtypes = []

        lib.df_get_last_error_full_message.restype = ctypes.c_char_p
        lib.df_get_last_error_full_message.argtypes = []

    @property
    def version(self) -> str:
        """获取库版本字符串"""
        result = self._lib.df_get_version()
        return result.decode("utf-8") if result else "unknown"

    @property
    def last_error_code(self) -> int:
        """获取最近一次错误码"""
        return self._lib.df_get_last_error_code()

    @property
    def last_error_message(self) -> str:
        """获取最近一次错误消息"""
        result = self._lib.df_get_last_error_full_message()
        return result.decode("utf-8", "replace") if result else ""

    def convert(
        self,
        input_files: Union[str, Path, List[Union[str, Path]]],
        output_file: Union[str, Path],
        config: Optional[Dict[str, Any]] = None,
        time_shift: float = 0.0,
    ) -> None:
        """执行弹幕格式转换

        Args:
            input_files: 输入文件路径（支持单个或列表）
            output_file: 输出文件路径
            config: 可选配置字典，结构参见 DanmakuFactory 文档
            time_shift: 时轴偏移量（秒）

        Raises:
            DanmakuFactoryError: 转换失败
        """
        # 统一为列表
        if isinstance(input_files, (str, Path)):
            input_files = [input_files]

        # 构建 runtime_info JSON
        runtime_info: Dict[str, Any] = {
            "input_files": [
                {"path": str(f).replace("\\", "/"), "time_shift": time_shift}
                for f in input_files
            ],
            "output_file": str(output_file).replace("\\", "/"),
        }

        if config:
            runtime_info["config"] = config

        json_bytes = json.dumps(runtime_info, ensure_ascii=False).encode("utf-8")
        code = self._lib.df_run_from_json(json_bytes)

        if code != 0:
            raise DanmakuFactoryError(code, self.last_error_message)

    def convert_raw_json(self, runtime_info_json: str) -> int:
        """直接传入 JSON 字符串执行转换（底层接口）

        Args:
            runtime_info_json: 完整的 runtime_info JSON 字符串

        Returns:
            错误码（0 表示成功）
        """
        json_bytes = runtime_info_json.encode("utf-8")
        return self._lib.df_run_from_json(json_bytes)


class DanmakuFactoryContext:
    """DanmakuFactory 标准化句柄封装（推荐）

    遵循 create → configure → run → destroy 生命周期，
    支持上下文管理器（with 语句）自动释放资源。

    Example:
        with DanmakuFactoryContext() as ctx:
            ctx.add_input_file("input.xml")
            ctx.set_output_file("output.ass")
            ctx.set_font_size(25)
            ctx.run()
    """

    def __init__(self, lib_path: Optional[Union[str, Path]] = None):
        """创建上下文并加载动态库

        Args:
            lib_path: 动态库路径。为 None 时自动搜索 build/ 目录。

        Raises:
            FileNotFoundError: 找不到动态库
            DanmakuFactoryError: 创建上下文失败
        """
        self._ctx = None
        if lib_path is None:
            lib_path = DanmakuFactory._find_library()
        self._lib = ctypes.CDLL(str(lib_path))
        self._setup_signatures()

        self._ctx = self._lib.df_create()
        if not self._ctx:
            raise DanmakuFactoryError(-14, "df_create() returned NULL (out of memory)")

    def _setup_signatures(self) -> None:
        """声明 C 函数签名"""
        lib = self._lib

        lib.df_create.restype = ctypes.c_void_p
        lib.df_create.argtypes = []

        lib.df_destroy.restype = None
        lib.df_destroy.argtypes = [ctypes.c_void_p]

        lib.df_run.restype = ctypes.c_int
        lib.df_run.argtypes = [ctypes.c_void_p]

        lib.df_load_config_json.restype = ctypes.c_int
        lib.df_load_config_json.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

        lib.df_load_config_file.restype = ctypes.c_int
        lib.df_load_config_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

        lib.df_add_input_file.restype = ctypes.c_int
        lib.df_add_input_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float]

        lib.df_add_input_file_typed.restype = ctypes.c_int
        lib.df_add_input_file_typed.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float, ctypes.c_char_p
        ]

        lib.df_clear_input_files.restype = ctypes.c_int
        lib.df_clear_input_files.argtypes = [ctypes.c_void_p]

        lib.df_set_output_file.restype = ctypes.c_int
        lib.df_set_output_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

        lib.df_set_output_file_typed.restype = ctypes.c_int
        lib.df_set_output_file_typed.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

        # 屏幕配置
        lib.df_set_resolution.restype = ctypes.c_int
        lib.df_set_resolution.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        lib.df_set_display_area.restype = ctypes.c_int
        lib.df_set_display_area.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.df_set_scroll_area.restype = ctypes.c_int
        lib.df_set_scroll_area.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.df_set_statistics_mode.restype = ctypes.c_int
        lib.df_set_statistics_mode.argtypes = [ctypes.c_void_p, ctypes.c_int]

        # 弹幕配置
        lib.df_set_density.restype = ctypes.c_int
        lib.df_set_density.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.df_set_scroll_duration.restype = ctypes.c_int
        lib.df_set_scroll_duration.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.df_set_fixed_duration.restype = ctypes.c_int
        lib.df_set_fixed_duration.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.df_set_show_username.restype = ctypes.c_int
        lib.df_set_show_username.argtypes = [ctypes.c_void_p, ctypes.c_bool]
        lib.df_set_block_mode.restype = ctypes.c_int
        lib.df_set_block_mode.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.df_set_keep_blocked.restype = ctypes.c_int
        lib.df_set_keep_blocked.argtypes = [ctypes.c_void_p, ctypes.c_bool]

        # 文本配置
        lib.df_set_fontname.restype = ctypes.c_int
        lib.df_set_fontname.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        lib.df_set_font_size.restype = ctypes.c_int
        lib.df_set_font_size.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.df_set_font_size_strict.restype = ctypes.c_int
        lib.df_set_font_size_strict.argtypes = [ctypes.c_void_p, ctypes.c_bool]
        lib.df_set_font_size_norm.restype = ctypes.c_int
        lib.df_set_font_size_norm.argtypes = [ctypes.c_void_p, ctypes.c_bool]
        lib.df_set_bold.restype = ctypes.c_int
        lib.df_set_bold.argtypes = [ctypes.c_void_p, ctypes.c_bool]
        lib.df_set_shadow.restype = ctypes.c_int
        lib.df_set_shadow.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.df_set_opacity.restype = ctypes.c_int
        lib.df_set_opacity.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.df_set_outline.restype = ctypes.c_int
        lib.df_set_outline.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.df_set_outline_blur.restype = ctypes.c_int
        lib.df_set_outline_blur.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.df_set_outline_opacity.restype = ctypes.c_int
        lib.df_set_outline_opacity.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.df_set_line_spacing.restype = ctypes.c_int
        lib.df_set_line_spacing.argtypes = [ctypes.c_void_p, ctypes.c_int]

        # 消息框配置
        lib.df_set_msgbox_show.restype = ctypes.c_int
        lib.df_set_msgbox_show.argtypes = [ctypes.c_void_p, ctypes.c_bool]
        lib.df_set_msgbox_size.restype = ctypes.c_int
        lib.df_set_msgbox_size.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        lib.df_set_msgbox_pos.restype = ctypes.c_int
        lib.df_set_msgbox_pos.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        lib.df_set_msgbox_font_size.restype = ctypes.c_int
        lib.df_set_msgbox_font_size.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.df_set_msgbox_duration.restype = ctypes.c_int
        lib.df_set_msgbox_duration.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.df_set_msgbox_gift_min_price.restype = ctypes.c_int
        lib.df_set_msgbox_gift_min_price.argtypes = [ctypes.c_void_p, ctypes.c_float]

        # 错误查询
        lib.df_get_last_error_code.restype = ctypes.c_int
        lib.df_get_last_error_code.argtypes = []
        lib.df_get_last_error_full_message.restype = ctypes.c_char_p
        lib.df_get_last_error_full_message.argtypes = []

    def _check(self, code: int) -> None:
        """检查返回码，非零时抛出异常"""
        if code != 0:
            msg = self._lib.df_get_last_error_full_message()
            text = msg.decode("utf-8", "replace") if msg else ""
            raise DanmakuFactoryError(code, text)

    # --- 上下文管理器 ---

    def __enter__(self) -> "DanmakuFactoryContext":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def close(self) -> None:
        """销毁上下文，释放所有资源（幂等）"""
        if getattr(self, "_ctx", None):
            self._lib.df_destroy(self._ctx)
            self._ctx = None

    def __del__(self) -> None:
        self.close()

    # --- 文件管理 ---

    def add_input_file(self, path: Union[str, Path], time_shift: float = 0.0) -> None:
        """添加输入文件（可多次调用），格式由扩展名推断"""
        self._check(self._lib.df_add_input_file(
            self._ctx, str(path).replace("\\", "/").encode("utf-8"), time_shift))

    def add_input_file_typed(self, path: Union[str, Path], file_type: str,
                             time_shift: float = 0.0) -> None:
        """添加输入文件并显式指定格式（xml/json/ass）"""
        self._check(self._lib.df_add_input_file_typed(
            self._ctx, str(path).replace("\\", "/").encode("utf-8"),
            time_shift, file_type.encode("utf-8")))

    def clear_input_files(self) -> None:
        """清除所有已添加的输入文件"""
        self._check(self._lib.df_clear_input_files(self._ctx))

    def set_output_file(self, path: Union[str, Path]) -> None:
        """设置输出文件路径，格式由扩展名推断"""
        self._check(self._lib.df_set_output_file(
            self._ctx, str(path).replace("\\", "/").encode("utf-8")))

    def set_output_file_typed(self, path: Union[str, Path], file_type: str) -> None:
        """设置输出文件路径并显式指定输出格式（xml/json/ass），路径可无扩展名"""
        self._check(self._lib.df_set_output_file_typed(
            self._ctx, str(path).replace("\\", "/").encode("utf-8"),
            file_type.encode("utf-8")))

    # --- 批量配置加载 ---

    def load_config_json(self, json_str: Union[str, bytes]) -> None:
        """从 JSON 字符串一次性加载全部配置（合并语义）

        支持裸配置 {"text": {...}} 和包装格式 {"config": {...}}。
        加载后立即校验，非法配置会抛出异常且不修改上下文。

        Args:
            json_str: 配置 JSON 字符串

        Raises:
            DanmakuFactoryError: JSON 格式错误或配置值非法
        """
        if isinstance(json_str, str):
            json_str = json_str.encode("utf-8")
        self._check(self._lib.df_load_config_json(self._ctx, json_str))

    def load_config_file(self, path: Union[str, Path]) -> None:
        """从 JSON 配置文件一次性加载全部配置

        Args:
            path: 配置文件路径

        Raises:
            DanmakuFactoryError: 文件不存在、JSON 格式错误或配置值非法
        """
        self._check(self._lib.df_load_config_file(
            self._ctx, str(path).replace("\\", "/").encode("utf-8")))

    # --- 屏幕配置 ---

    def set_resolution(self, width: int, height: int) -> None:
        self._check(self._lib.df_set_resolution(self._ctx, width, height))

    def set_display_area(self, area: float) -> None:
        self._check(self._lib.df_set_display_area(self._ctx, area))

    def set_scroll_area(self, area: float) -> None:
        self._check(self._lib.df_set_scroll_area(self._ctx, area))

    def set_statistics_mode(self, mode: int) -> None:
        self._check(self._lib.df_set_statistics_mode(self._ctx, mode))

    # --- 弹幕配置 ---

    def set_density(self, density: int) -> None:
        self._check(self._lib.df_set_density(self._ctx, density))

    def set_scroll_duration(self, seconds: float) -> None:
        self._check(self._lib.df_set_scroll_duration(self._ctx, seconds))

    def set_fixed_duration(self, seconds: float) -> None:
        self._check(self._lib.df_set_fixed_duration(self._ctx, seconds))

    def set_show_username(self, show: bool) -> None:
        self._check(self._lib.df_set_show_username(self._ctx, show))

    def set_block_mode(self, mode: int) -> None:
        self._check(self._lib.df_set_block_mode(self._ctx, mode))

    def set_keep_blocked(self, keep: bool) -> None:
        self._check(self._lib.df_set_keep_blocked(self._ctx, keep))

    # --- 文本配置 ---

    def set_fontname(self, fontname: str) -> None:
        self._check(self._lib.df_set_fontname(self._ctx, fontname.encode("utf-8")))

    def set_font_size(self, size: int) -> None:
        self._check(self._lib.df_set_font_size(self._ctx, size))

    def set_font_size_strict(self, strict: bool) -> None:
        self._check(self._lib.df_set_font_size_strict(self._ctx, strict))

    def set_font_size_norm(self, norm: bool) -> None:
        self._check(self._lib.df_set_font_size_norm(self._ctx, norm))

    def set_bold(self, bold: bool) -> None:
        self._check(self._lib.df_set_bold(self._ctx, bold))

    def set_shadow(self, shadow: float) -> None:
        self._check(self._lib.df_set_shadow(self._ctx, shadow))

    def set_opacity(self, opacity: int) -> None:
        self._check(self._lib.df_set_opacity(self._ctx, opacity))

    def set_outline(self, outline: float) -> None:
        self._check(self._lib.df_set_outline(self._ctx, outline))

    def set_outline_blur(self, blur: float) -> None:
        self._check(self._lib.df_set_outline_blur(self._ctx, blur))

    def set_outline_opacity(self, opacity: int) -> None:
        self._check(self._lib.df_set_outline_opacity(self._ctx, opacity))

    def set_line_spacing(self, spacing: int) -> None:
        self._check(self._lib.df_set_line_spacing(self._ctx, spacing))

    # --- 消息框配置 ---

    def set_msgbox_show(self, show: bool) -> None:
        self._check(self._lib.df_set_msgbox_show(self._ctx, show))

    def set_msgbox_size(self, width: int, height: int) -> None:
        self._check(self._lib.df_set_msgbox_size(self._ctx, width, height))

    def set_msgbox_pos(self, x: int, y: int) -> None:
        self._check(self._lib.df_set_msgbox_pos(self._ctx, x, y))

    def set_msgbox_font_size(self, size: int) -> None:
        self._check(self._lib.df_set_msgbox_font_size(self._ctx, size))

    def set_msgbox_duration(self, seconds: float) -> None:
        self._check(self._lib.df_set_msgbox_duration(self._ctx, seconds))

    def set_msgbox_gift_min_price(self, price: float) -> None:
        self._check(self._lib.df_set_msgbox_gift_min_price(self._ctx, price))

    # --- 执行 ---

    def run(self) -> None:
        """验证配置并执行弹幕格式转换

        Raises:
            DanmakuFactoryError: 转换失败
        """
        self._check(self._lib.df_run(self._ctx))
