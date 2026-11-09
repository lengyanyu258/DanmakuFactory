/// DanmakuFactory Dart/Flutter 绑定
///
/// 通过 dart:ffi 调用 DanmakuFactory C 核心库，采用与 CLI/GUI 一致的
/// JSON 桥接方式（df_run_from_json），三端转换逻辑完全一致。
///
/// 前置条件:
///   1. 构建 C 核心库: xmake build lib-shared
///   2. 将动态库部署到应用可加载的位置:
///      - Android: 打包为 JNI 库 (android/app/src/main/jniLibs/<abi>/libDanmakuFactory.so)
///      - iOS: 静态链接进应用 (通过 DynamicLibrary.process() 访问)
///      - Windows: DanmakuFactory.dll 与可执行文件同目录
///      - macOS: libDanmakuFactory.dylib 与可执行文件同目录
///      - Linux: libDanmakuFactory.so 与可执行文件同目录
///   3. 在 Flutter/Dart 工程的 pubspec.yaml 中添加依赖:
///      dependencies:
///        danmakufactory:
///          path: <repo>/bindings/dart
///
/// 用法:
///   import 'package:danmakufactory/danmakufactory.dart';
///
///   final df = DanmakuFactory();
///   print(df.version);
///   df.convert('input.xml', 'output.ass',
///       config: {
///         'text': {'font_size': 25},
///       });
library danmakufactory;

import 'dart:convert' show jsonEncode;
import 'dart:ffi' as ffi;
import 'dart:io' show Platform;

import 'package:ffi/ffi.dart';

/// DanmakuFactory 库调用异常
class DfException implements Exception {
  /// C 库返回的错误码（非 0）
  final int code;

  /// 错误描述
  final String message;

  DfException(this.code, this.message);

  @override
  String toString() => 'DfException($code): $message';
}

/// DanmakuFactory C 核心库封装
///
/// 通过 dart:ffi 直接调用动态库，与 CLI/GUI 共用同一 C 核心库，
/// 转换逻辑完全一致。
class DanmakuFactory {
  late final ffi.DynamicLibrary _lib;

  // ---- FFI 函数指针 ----
  late final ffi.Pointer<Utf8> Function() _dfGetVersion;
  late final int Function(ffi.Pointer<Utf8>) _dfRunFromJson;
  late final int Function() _dfGetLastErrorCode;
  late final ffi.Pointer<Utf8> Function() _dfGetLastErrorFullMessage;

  /// 创建实例并加载动态库
  ///
  /// [libPath] 可选，直接指定动态库路径；为 null 时按当前平台自动加载。
  DanmakuFactory([String? libPath]) {
    _lib = _loadLibrary(libPath);

    _dfGetVersion = _lib.lookupFunction<ffi.Pointer<Utf8> Function(),
        ffi.Pointer<Utf8> Function()>('df_get_version');

    _dfRunFromJson = _lib.lookupFunction<ffi.Int32 Function(ffi.Pointer<Utf8>),
        int Function(ffi.Pointer<Utf8>)>('df_run_from_json');

    _dfGetLastErrorCode =
        _lib.lookupFunction<ffi.Int32 Function(), int Function()>(
            'df_get_last_error_code');

    _dfGetLastErrorFullMessage = _lib.lookupFunction<
        ffi.Pointer<Utf8> Function(),
        ffi.Pointer<Utf8> Function()>('df_get_last_error_full_message');
  }

  /// 按平台加载动态库
  static ffi.DynamicLibrary _loadLibrary(String? path) => loadLibrary(path);

  /// 公开的动态库加载入口（供 [DanmakuFactory] 与 [DfContext] 共用）
  static ffi.DynamicLibrary loadLibrary(String? path) {
    if (path != null) {
      return ffi.DynamicLibrary.open(path);
    }
    if (Platform.isAndroid) {
      return ffi.DynamicLibrary.open('libDanmakuFactory.so');
    }
    if (Platform.isIOS) {
      // iOS 不允许运行时加载动态库，C 库需静态链接进应用
      return ffi.DynamicLibrary.process();
    }
    if (Platform.isWindows) {
      return ffi.DynamicLibrary.open('DanmakuFactory.dll');
    }
    if (Platform.isMacOS) {
      return ffi.DynamicLibrary.open('libDanmakuFactory.dylib');
    }
    // Linux 及其他类 Unix 平台
    return ffi.DynamicLibrary.open('libDanmakuFactory.so');
  }

  /// 库版本字符串（如 "2.0.0"），可用于 FFI 探活
  String get version => _ptrToString(_dfGetVersion());

  /// 最近一次错误码
  int get lastErrorCode => _dfGetLastErrorCode();

  /// 最近一次错误的完整描述
  String get lastErrorMessage => _ptrToString(_dfGetLastErrorFullMessage());

  /// 执行弹幕格式转换
  ///
  /// [inputFile] 输入文件路径（扩展名决定输入格式: xml/json/ass）
  /// [outputFile] 输出文件路径（扩展名决定输出格式）
  /// [config] 可选配置（结构与 DanmakuFactoryConfig.json 的 config 节一致）
  /// [timeShift] 时轴偏移量（秒）
  ///
  /// 转换失败时抛出 [DfException]。
  void convert(
    String inputFile,
    String outputFile, {
    Map<String, dynamic>? config,
    double timeShift = 0.0,
  }) {
    final runtimeInfo = <String, dynamic>{
      'input_files': [
        {
          'path': inputFile.replaceAll('\\', '/'),
          'time_shift': timeShift,
        },
      ],
      'output_file': outputFile.replaceAll('\\', '/'),
      if (config != null) 'config': config,
    };

    final code = convertRawJson(jsonEncode(runtimeInfo));
    if (code != 0) {
      throw DfException(code, lastErrorMessage);
    }
  }

  /// 直接传入完整 runtime_info JSON 字符串执行转换（底层接口）
  ///
  /// 返回错误码（0 表示成功）。
  int convertRawJson(String runtimeInfoJson) {
    final cJson = runtimeInfoJson.toNativeUtf8();
    try {
      return _dfRunFromJson(cJson);
    } finally {
      malloc.free(cJson);
    }
  }

  /// 空安全的 C 字符串转换
  static String _ptrToString(ffi.Pointer<Utf8> ptr) =>
      ptr == ffi.nullptr ? '' : ptr.toDartString();
}

/// DanmakuFactory 标准化句柄封装（推荐）
///
/// 遵循 create → configure → run → destroy 生命周期。
/// 实现释放逻辑，使用完毕后应调用 [dispose]（或通过 try/finally）。
///
/// ```dart
/// final ctx = DfContext();
/// try {
///   ctx.addInputFile('input.xml');
///   ctx.setOutputFile('output.ass');
///   ctx.setFontSize(25);
///   ctx.run();
/// } finally {
///   ctx.dispose();
/// }
/// ```
class DfContext {
  late final ffi.DynamicLibrary _lib;
  ffi.Pointer<ffi.Void> _ctx = ffi.nullptr;
  bool _disposed = false;

  // ---- FFI 函数指针（Dart 侧签名用 int/double） ----
  late final ffi.Pointer<ffi.Void> Function() _dfCreate;
  late final void Function(ffi.Pointer<ffi.Void>) _dfDestroy;
  late final int Function(ffi.Pointer<ffi.Void>) _dfRun;
  late final int Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>)
      _dfLoadConfigJson;
  late final int Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>)
      _dfLoadConfigFile;
  late final int Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, double)
      _dfAddInputFile;
  late final int Function(ffi.Pointer<ffi.Void>) _dfClearInputFiles;
  late final int Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>)
      _dfSetOutputFile;

  // 屏幕
  late final int Function(ffi.Pointer<ffi.Void>, int, int) _dfSetResolution;
  late final int Function(ffi.Pointer<ffi.Void>, double) _dfSetDisplayArea;
  late final int Function(ffi.Pointer<ffi.Void>, double) _dfSetScrollArea;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetStatisticsMode;
  // 弹幕
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetDensity;
  late final int Function(ffi.Pointer<ffi.Void>, double) _dfSetScrollDuration;
  late final int Function(ffi.Pointer<ffi.Void>, double) _dfSetFixedDuration;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetShowUsername;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetBlockMode;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetKeepBlocked;
  // 文本
  late final int Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>)
      _dfSetFontname;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetFontSize;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetFontSizeStrict;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetFontSizeNorm;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetBold;
  late final int Function(ffi.Pointer<ffi.Void>, double) _dfSetShadow;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetOpacity;
  late final int Function(ffi.Pointer<ffi.Void>, double) _dfSetOutline;
  late final int Function(ffi.Pointer<ffi.Void>, double) _dfSetOutlineBlur;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetOutlineOpacity;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetLineSpacing;
  // 消息框
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetMsgboxShow;
  late final int Function(ffi.Pointer<ffi.Void>, int, int) _dfSetMsgboxSize;
  late final int Function(ffi.Pointer<ffi.Void>, int, int) _dfSetMsgboxPos;
  late final int Function(ffi.Pointer<ffi.Void>, int) _dfSetMsgboxFontSize;
  late final int Function(ffi.Pointer<ffi.Void>, double) _dfSetMsgboxDuration;
  late final int Function(ffi.Pointer<ffi.Void>, double)
      _dfSetMsgboxGiftMinPrice;
  // 错误
  late final ffi.Pointer<Utf8> Function() _dfGetLastErrorFullMessage;

  /// 创建上下文，所有配置项初始化为默认值
  DfContext([String? libPath]) {
    _lib = DanmakuFactory.loadLibrary(libPath);
    _bindFunctions();

    _ctx = _dfCreate();
    if (_ctx == ffi.nullptr) {
      throw DfException(-14, 'df_create() returned NULL (out of memory)');
    }
  }

  void _bindFunctions() {
    _dfCreate = _lib.lookupFunction<ffi.Pointer<ffi.Void> Function(),
        ffi.Pointer<ffi.Void> Function()>('df_create');
    _dfDestroy = _lib.lookupFunction<ffi.Void Function(ffi.Pointer<ffi.Void>),
        void Function(ffi.Pointer<ffi.Void>)>('df_destroy');
    _dfRun = _lib.lookupFunction<ffi.Int32 Function(ffi.Pointer<ffi.Void>),
        int Function(ffi.Pointer<ffi.Void>)>('df_run');
    _dfLoadConfigJson = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>),
        int Function(
            ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>)>('df_load_config_json');
    _dfLoadConfigFile = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>),
        int Function(
            ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>)>('df_load_config_file');
    _dfAddInputFile = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>,
            double)>('df_add_input_file');
    _dfClearInputFiles = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>),
        int Function(ffi.Pointer<ffi.Void>)>('df_clear_input_files');
    _dfSetOutputFile = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>),
        int Function(
            ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>)>('df_set_output_file');

    _dfSetResolution = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int, int)>('df_set_resolution');
    _dfSetDisplayArea = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, double)>('df_set_display_area');
    _dfSetScrollArea = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, double)>('df_set_scroll_area');
    _dfSetStatisticsMode = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_statistics_mode');

    _dfSetDensity = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_density');
    _dfSetScrollDuration = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, double)>('df_set_scroll_duration');
    _dfSetFixedDuration = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, double)>('df_set_fixed_duration');
    _dfSetShowUsername = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Uint8),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_show_username');
    _dfSetBlockMode = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_block_mode');
    _dfSetKeepBlocked = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Uint8),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_keep_blocked');

    _dfSetFontname = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>),
        int Function(
            ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>)>('df_set_fontname');
    _dfSetFontSize = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_font_size');
    _dfSetFontSizeStrict = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Uint8),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_font_size_strict');
    _dfSetFontSizeNorm = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Uint8),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_font_size_norm');
    _dfSetBold = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Uint8),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_bold');
    _dfSetShadow = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, double)>('df_set_shadow');
    _dfSetOpacity = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_opacity');
    _dfSetOutline = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, double)>('df_set_outline');
    _dfSetOutlineBlur = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, double)>('df_set_outline_blur');
    _dfSetOutlineOpacity = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_outline_opacity');
    _dfSetLineSpacing = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_line_spacing');

    _dfSetMsgboxShow = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Uint8),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_msgbox_show');
    _dfSetMsgboxSize = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int, int)>('df_set_msgbox_size');
    _dfSetMsgboxPos = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int, int)>('df_set_msgbox_pos');
    _dfSetMsgboxFontSize = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Int32),
        int Function(ffi.Pointer<ffi.Void>, int)>('df_set_msgbox_font_size');
    _dfSetMsgboxDuration = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(ffi.Pointer<ffi.Void>, double)>('df_set_msgbox_duration');
    _dfSetMsgboxGiftMinPrice = _lib.lookupFunction<
        ffi.Int32 Function(ffi.Pointer<ffi.Void>, ffi.Float),
        int Function(
            ffi.Pointer<ffi.Void>, double)>('df_set_msgbox_gift_min_price');

    _dfGetLastErrorFullMessage = _lib.lookupFunction<
        ffi.Pointer<Utf8> Function(),
        ffi.Pointer<Utf8> Function()>('df_get_last_error_full_message');
  }

  String get _lastErrorMessage {
    final ptr = _dfGetLastErrorFullMessage();
    return ptr == ffi.nullptr ? '' : ptr.toDartString();
  }

  void _check(int code) {
    if (code != 0) {
      throw DfException(code, _lastErrorMessage);
    }
  }

  /// 用 FFI 调用字符串参数的辅助
  int _callStr(
      int Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>) fn, String s) {
    final c = s.toNativeUtf8();
    try {
      return fn(_ctx, c);
    } finally {
      malloc.free(c);
    }
  }

  // ---- 执行 ----

  /// 验证当前配置并执行弹幕格式转换
  void run() => _check(_dfRun(_ctx));

  // ---- 批量配置加载 ----

  void loadConfigJson(String jsonStr) =>
      _check(_callStr(_dfLoadConfigJson, jsonStr));
  void loadConfigFile(String path) =>
      _check(_callStr(_dfLoadConfigFile, path.replaceAll('\\', '/')));

  // ---- 文件管理 ----

  void addInputFile(String path, [double timeShift = 0.0]) {
    final c = path.replaceAll('\\', '/').toNativeUtf8();
    try {
      _check(_dfAddInputFile(_ctx, c, timeShift));
    } finally {
      malloc.free(c);
    }
  }

  void clearInputFiles() => _check(_dfClearInputFiles(_ctx));

  void setOutputFile(String path) =>
      _check(_callStr(_dfSetOutputFile, path.replaceAll('\\', '/')));

  // ---- 屏幕配置 ----

  void setResolution(int width, int height) =>
      _check(_dfSetResolution(_ctx, width, height));
  void setDisplayArea(double area) => _check(_dfSetDisplayArea(_ctx, area));
  void setScrollArea(double area) => _check(_dfSetScrollArea(_ctx, area));
  void setStatisticsMode(int mode) => _check(_dfSetStatisticsMode(_ctx, mode));

  // ---- 弹幕配置 ----

  void setDensity(int density) => _check(_dfSetDensity(_ctx, density));
  void setScrollDuration(double seconds) =>
      _check(_dfSetScrollDuration(_ctx, seconds));
  void setFixedDuration(double seconds) =>
      _check(_dfSetFixedDuration(_ctx, seconds));
  void setShowUsername(bool show) =>
      _check(_dfSetShowUsername(_ctx, show ? 1 : 0));
  void setBlockMode(int mode) => _check(_dfSetBlockMode(_ctx, mode));
  void setKeepBlocked(bool keep) =>
      _check(_dfSetKeepBlocked(_ctx, keep ? 1 : 0));

  // ---- 文本配置 ----

  void setFontname(String fontname) =>
      _check(_callStr(_dfSetFontname, fontname));
  void setFontSize(int size) => _check(_dfSetFontSize(_ctx, size));
  void setFontSizeStrict(bool strict) =>
      _check(_dfSetFontSizeStrict(_ctx, strict ? 1 : 0));
  void setFontSizeNorm(bool norm) =>
      _check(_dfSetFontSizeNorm(_ctx, norm ? 1 : 0));
  void setBold(bool bold) => _check(_dfSetBold(_ctx, bold ? 1 : 0));
  void setShadow(double shadow) => _check(_dfSetShadow(_ctx, shadow));
  void setOpacity(int opacity) => _check(_dfSetOpacity(_ctx, opacity));
  void setOutline(double outline) => _check(_dfSetOutline(_ctx, outline));
  void setOutlineBlur(double blur) => _check(_dfSetOutlineBlur(_ctx, blur));
  void setOutlineOpacity(int opacity) =>
      _check(_dfSetOutlineOpacity(_ctx, opacity));
  void setLineSpacing(int spacing) => _check(_dfSetLineSpacing(_ctx, spacing));

  // ---- 消息框配置 ----

  void setMsgboxShow(bool show) => _check(_dfSetMsgboxShow(_ctx, show ? 1 : 0));
  void setMsgboxSize(int width, int height) =>
      _check(_dfSetMsgboxSize(_ctx, width, height));
  void setMsgboxPos(int x, int y) => _check(_dfSetMsgboxPos(_ctx, x, y));
  void setMsgboxFontSize(int size) => _check(_dfSetMsgboxFontSize(_ctx, size));
  void setMsgboxDuration(double seconds) =>
      _check(_dfSetMsgboxDuration(_ctx, seconds));
  void setMsgboxGiftMinPrice(double price) =>
      _check(_dfSetMsgboxGiftMinPrice(_ctx, price));

  // ---- 资源释放 ----

  /// 销毁上下文并释放所有内部资源（幂等）
  void dispose() {
    if (!_disposed) {
      if (_ctx != ffi.nullptr) {
        _dfDestroy(_ctx);
        _ctx = ffi.nullptr;
      }
      _disposed = true;
    }
  }
}
