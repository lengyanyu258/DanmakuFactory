/// DanmakuFactory Dart/Flutter 使用示例
///
/// 演示如何使用 DanmakuFactory Dart 绑定进行弹幕格式转换。
///
/// 前置条件:
///   1. 构建动态库: xmake build lib-shared
///   2. 将 DanmakuFactory.dll 复制到可执行目录（或系统 PATH）
///   3. 在 bindings/dart 下执行: dart pub get
///
/// 运行:
///   cd bindings/dart
///   dart run example/example.dart
library;

import 'dart:io';

import 'package:danmakufactory/danmakufactory.dart';

/// 定位项目自带的测试弹幕文件
File _testXml() {
  final scriptDir = File.fromUri(Platform.script).parent;
  final repoRoot =
      scriptDir.parent.parent.parent; // example -> dart -> bindings -> root
  return File(
      '${repoRoot.path}${Platform.pathSeparator}tests${Platform.pathSeparator}test🚩.xml');
}

/// 定位 fixtures 下的 sample.xml
File _fixtureXml() {
  final scriptDir = File.fromUri(Platform.script).parent;
  final repoRoot = scriptDir.parent.parent.parent;
  return File(
      '${repoRoot.path}${Platform.pathSeparator}tests${Platform.pathSeparator}fixtures${Platform.pathSeparator}sample.xml');
}

void exampleBasicConvert(Directory tmp) {
  print('=== 基础转换：XML → ASS ===');

  final df = DanmakuFactory();
  print('库版本: ${df.version}');

  final inputXml = _testXml();
  final outputAss = File('${tmp.path}${Platform.pathSeparator}output.ass');

  df.convert(inputXml.path, outputAss.path);
  print('转换成功: ${inputXml.uri.pathSegments.last} → output.ass');
  print('输出大小: ${outputAss.lengthSync()} bytes');
  print('前 200 字符:\n${outputAss.readAsStringSync().substring(0, 200)}');
  print('');
}

void exampleWithConfig(Directory tmp) {
  print('=== 带配置的转换 ===');

  final df = DanmakuFactory();
  final inputXml = _testXml();
  final outputAss =
      File('${tmp.path}${Platform.pathSeparator}styled_output.ass');

  final config = <String, dynamic>{
    'screen': {
      'resolution': [1920, 1080],
      'display_area': 0.75,
    },
    'danmaku': {
      'scroll_duration': 10.0,
      'fixed_duration': 5.0,
      'density': -1, // 不重叠模式
    },
    'text': {
      'fontname': 'Microsoft YaHei',
      'font_size': 38,
      'opacity': 200,
      'outline': 1.0,
      'shadow': 0.5,
      'bold': true,
    },
  };

  df.convert(inputXml.path, outputAss.path, config: config);
  print('转换成功（自定义样式）');
  print('输出大小: ${outputAss.lengthSync()} bytes');
  print('');
}

void exampleErrorHandling() {
  print('=== 错误处理 ===');

  final df = DanmakuFactory();

  try {
    df.convert('nonexistent_file.xml', 'output.ass');
  } on DfException catch (e) {
    print('捕获错误:');
    print('  错误码: ${e.code}');
    print('  消息: ${e.message}');
  }
  print('');
}

void exampleBatchConvert(Directory tmp) {
  print('=== 批量转换 ===');

  final df = DanmakuFactory();
  final inputXml = _testXml();

  // 同一输入转换为多种格式
  final outputs = <String, String>{
    'ASS': '${tmp.path}${Platform.pathSeparator}output.ass',
    'XML': '${tmp.path}${Platform.pathSeparator}output.xml',
    'JSON': '${tmp.path}${Platform.pathSeparator}output.json',
  };

  outputs.forEach((fmt, outputPath) {
    try {
      df.convert(inputXml.path, outputPath);
      print('  $fmt: ${File(outputPath).lengthSync()} bytes');
    } on DfException catch (e) {
      print('  $fmt: 失败 - ${e.message}');
    }
  });
  print('');
}

void exampleHandleApi(Directory tmp) {
  print('=== 句柄 API：create → configure → run → destroy ===');

  final fixture = _fixtureXml();
  final output = File('${tmp.path}${Platform.pathSeparator}handle_output.ass');

  final ctx = DfContext();
  try {
    ctx.addInputFile(fixture.path);
    ctx.setOutputFile(output.path);
    ctx.setResolution(1920, 1080);
    ctx.setFontSize(30);
    ctx.setScrollDuration(10.0);
    ctx.setBold(true);
    ctx.run();
    print('句柄 API 转换成功: ${output.lengthSync()} bytes');
  } on DfException catch (e) {
    print('句柄 API 转换失败: ${e.message}');
  } finally {
    ctx.dispose();
  }
  print('');
}

void main() {
  final tmp = Directory.systemTemp.createTempSync('df_dart_example_');
  try {
    exampleBasicConvert(tmp);
    exampleWithConfig(tmp);
    exampleHandleApi(tmp);
    exampleErrorHandling();
    exampleBatchConvert(tmp);
    print('所有示例执行完毕。');
  } finally {
    tmp.deleteSync(recursive: true);
  }
}
