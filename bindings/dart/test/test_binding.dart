/// DanmakuFactory Dart 绑定测试
///
/// 运行方式:
///   cd bindings/dart
///   dart pub get
///   dart run test/test_binding.dart
///
/// 前置条件: 已构建动态库 (xmake build lib-shared) 并将
/// DanmakuFactory.dll/.so/.dylib 放到可加载位置。
library;

import 'dart:io';

import 'package:danmakufactory/danmakufactory.dart';

/// 定位项目自带的测试弹幕文件
File _testXml() {
  final scriptDir = File.fromUri(Platform.script).parent;
  final repoRoot =
      scriptDir.parent.parent.parent; // test -> dart -> bindings -> root
  return File(
      '${repoRoot.path}${Platform.pathSeparator}tests${Platform.pathSeparator}test🚩.xml');
}

/// 定位 fixtures 下的 sample.xml（无特殊字符，跨平台更稳）
File _fixtureXml() {
  final scriptDir = File.fromUri(Platform.script).parent;
  final repoRoot = scriptDir.parent.parent.parent;
  return File(
      '${repoRoot.path}${Platform.pathSeparator}tests${Platform.pathSeparator}fixtures${Platform.pathSeparator}sample.xml');
}

void _check(bool condition, String message) {
  if (!condition) {
    throw StateError('Assertion failed: $message');
  }
}

void testVersion() {
  final df = DanmakuFactory();
  final version = df.version;
  _check(version.isNotEmpty, 'version should not be empty');
  _check(version.contains('.'), 'version should contain dot: $version');
  print('PASS: version = $version');
}

void testConvertXmlToAss(Directory tmp) {
  final df = DanmakuFactory();
  final testXml = _testXml();
  _check(testXml.existsSync(), 'test resource missing: ${testXml.path}');

  final output = File('${tmp.path}${Platform.pathSeparator}output.ass');
  df.convert(testXml.path, output.path);
  _check(output.existsSync(), 'output file not generated');
  _check(output.lengthSync() > 0, 'output file is empty');
  print('PASS: xml -> ass, size = ${output.lengthSync()} bytes');
}

void testConvertWithConfig(Directory tmp) {
  final df = DanmakuFactory();
  final testXml = _testXml();

  final output = File('${tmp.path}${Platform.pathSeparator}output_config.ass');
  final config = <String, dynamic>{
    'text': {'font_size': 25, 'fontname': 'SimHei'},
    'danmaku': {'scroll_duration': 8.0},
  };
  df.convert(testXml.path, output.path, config: config);
  _check(output.existsSync(), 'output file not generated');
  _check(output.lengthSync() > 0, 'output file is empty');
  print('PASS: convert with config, size = ${output.lengthSync()} bytes');
}

void testErrorNonexistentFile(Directory tmp) {
  final df = DanmakuFactory();

  final output = File('${tmp.path}${Platform.pathSeparator}never.ass');
  try {
    df.convert(
        '${tmp.path}${Platform.pathSeparator}no_such_file.xml', output.path);
    _check(false, 'should have thrown DfException');
  } on DfException catch (e) {
    _check(e.code != 0, 'error code should be non-zero');
    _check(e.message.isNotEmpty, 'error message should not be empty');
    print('PASS: error path, code = ${e.code}');
  }
}

void testErrorInvalidJson() {
  final df = DanmakuFactory();
  final code = df.convertRawJson('{invalid json!!!');
  _check(code != 0, 'should return non-zero for invalid JSON');
  print('PASS: invalid json, code = $code');
}

// ---- 标准化句柄 API 测试 ----

void testContextConvert(Directory tmp) {
  final fixture = _fixtureXml();
  _check(fixture.existsSync(), 'fixture missing: ${fixture.path}');

  final output = File('${tmp.path}${Platform.pathSeparator}ctx_output.ass');
  final ctx = DfContext();
  try {
    ctx.addInputFile(fixture.path);
    ctx.setOutputFile(output.path);
    ctx.setFontSize(25);
    ctx.run();
  } finally {
    ctx.dispose();
  }
  _check(output.existsSync(), 'output file not generated');
  _check(output.lengthSync() > 0, 'output file is empty');
  print('PASS: context convert, size = ${output.lengthSync()} bytes');
}

void testContextSetters(Directory tmp) {
  final fixture = _fixtureXml();
  final output = File('${tmp.path}${Platform.pathSeparator}ctx_setters.ass');
  final ctx = DfContext();
  try {
    ctx.addInputFile(fixture.path);
    ctx.setOutputFile(output.path);
    // 覆盖各分组 setter
    ctx.setResolution(1280, 720);
    ctx.setDisplayArea(0.8);
    ctx.setDensity(-1);
    ctx.setScrollDuration(9.0);
    ctx.setFixedDuration(4.0);
    ctx.setShowUsername(true);
    ctx.setFontname('Microsoft YaHei');
    ctx.setFontSize(30);
    ctx.setBold(true);
    ctx.setOpacity(200);
    ctx.setOutline(1.0);
    ctx.setLineSpacing(2);
    ctx.setMsgboxShow(true);
    ctx.setMsgboxSize(400, 800);
    ctx.run();
  } finally {
    ctx.dispose();
  }
  _check(output.existsSync(), 'output file not generated');
  print('PASS: context setters (all groups)');
}

void testContextLoadConfigJson(Directory tmp) {
  final fixture = _fixtureXml();
  final output = File('${tmp.path}${Platform.pathSeparator}ctx_cfg.ass');
  final ctx = DfContext();
  try {
    ctx.addInputFile(fixture.path);
    ctx.setOutputFile(output.path);
    ctx.loadConfigJson('{"text": {"font_size": 30}}');
    ctx.loadConfigJson('{"config": {"danmaku": {"scroll_duration": 7.0}}}');
    ctx.run();
  } finally {
    ctx.dispose();
  }
  _check(output.existsSync(), 'output file not generated');
  print('PASS: context load_config_json');
}

void testContextInvalidConfig() {
  final ctx = DfContext();
  try {
    try {
      ctx.loadConfigJson('{invalid json!!!');
      _check(false, 'should have thrown DfException');
    } on DfException catch (e) {
      _check(e.code != 0, 'error code should be non-zero');
      _check(e.message.isNotEmpty, 'error message should not be empty');
      print('PASS: context invalid config, code = ${e.code}');
    }
  } finally {
    ctx.dispose();
  }
}

void testEmojiFilename(Directory tmp) {
  // emoji 文件名转换：输入与输出均含 emoji，验证 UTF-8 路径支持
  final fixture = _fixtureXml();
  _check(fixture.existsSync(), 'fixture missing: ${fixture.path}');

  final emojiInput = File('${tmp.path}${Platform.pathSeparator}输入🚩.xml');
  final emojiOutput = File('${tmp.path}${Platform.pathSeparator}输出🎉.ass');
  fixture.copySync(emojiInput.path);

  // JSON 一次性 API
  final df = DanmakuFactory();
  df.convert(emojiInput.path, emojiOutput.path);
  _check(emojiOutput.existsSync(), 'emoji output not generated');
  _check(emojiOutput.lengthSync() > 0, 'emoji output is empty');

  // 句柄 API 再验证一次
  final emojiOutput2 = File('${tmp.path}${Platform.pathSeparator}输出2🚀.ass');
  final ctx = DfContext();
  try {
    ctx.addInputFile(emojiInput.path);
    ctx.setOutputFile(emojiOutput2.path);
    ctx.run();
  } finally {
    ctx.dispose();
  }
  _check(emojiOutput2.existsSync(), 'emoji output2 not generated');
  _check(emojiOutput2.lengthSync() > 0, 'emoji output2 is empty');

  print('PASS: emoji filename convert (input + output)');
}

void main() {
  final testXml = _testXml();
  print('Test XML exists: ${testXml.existsSync()}');
  print('');

  final tmp = Directory.systemTemp.createTempSync('df_dart_test_');
  try {
    // JSON 一次性 API
    testVersion();
    testConvertXmlToAss(tmp);
    testConvertWithConfig(tmp);
    testErrorNonexistentFile(tmp);
    testErrorInvalidJson();

    // 标准化句柄 API
    testContextConvert(tmp);
    testContextSetters(tmp);
    testContextLoadConfigJson(tmp);
    testContextInvalidConfig();

    // emoji 文件名（UTF-8 路径）
    testEmojiFilename(tmp);

    print('\nALL TESTS PASSED');
  } finally {
    tmp.deleteSync(recursive: true);
  }
}
