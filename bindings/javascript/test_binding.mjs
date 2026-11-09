/**
 * DanmakuFactory JavaScript/WASM 绑定测试
 *
 * 前置条件:
 *   1. 构建 WASM: xmake f -p wasm -a wasm32 -m release && xmake build lib-wasm
 *   2. 运行: node bindings/javascript/test_binding.mjs
 */

import assert from 'node:assert/strict';
import { readFileSync, existsSync } from 'fs';
import { fileURLToPath, pathToFileURL } from 'url';
import { dirname, join } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const REPO_ROOT = join(__dirname, '..', '..');

// 定位 xmake 构建的 WASM 产物（优先 build 输出目录，其次 wasm/ 目录）
function locateWasmJs() {
  const candidates = [
    join(REPO_ROOT, 'build', 'wasm', 'wasm32', 'release', 'danmakufactory.js'),
    join(REPO_ROOT, 'build', 'wasm', 'wasm32', 'debug', 'danmakufactory.js'),
    join(REPO_ROOT, 'wasm', 'danmakufactory.js'),
  ];
  for (const c of candidates) {
    if (existsSync(c)) return c;
  }
  return null;
}

async function main() {
  // 动态导入 WASM 模块
  const wasmPath = locateWasmJs();
  if (!wasmPath) {
    console.error('FAIL: Cannot locate danmakufactory.js. Run `xmake f -p wasm -a wasm32 -m release && xmake build lib-wasm` first.');
    process.exit(1);
  }
  let factory;
  try {
    factory = (await import(pathToFileURL(wasmPath).href)).default;
  } catch (e) {
    console.error('FAIL: Cannot load WASM module.');
    console.error(`  Path: ${wasmPath}`);
    console.error(`  Error: ${e.message}`);
    process.exit(1);
  }

  const Module = await factory();

  // 确保 /tmp 存在
  try { Module.FS.mkdir('/tmp'); } catch (_) {}

  const { DanmakuFactoryInstance } = await import('./danmakufactory.mjs');
  const df = new DanmakuFactoryInstance(Module);

  // 测试 1: 版本查询
  const version = df.version;
  assert(version && version.includes('.'), `version should contain dot: ${version}`);
  console.log(`PASS: version = ${version}`);

  // 测试 2: XML → ASS 转换
  const testXmlPath = join(REPO_ROOT, 'tests', 'test🚩.xml');
  const xmlContent = readFileSync(testXmlPath, 'utf-8');
  const assContent = df.convert(xmlContent, 'xml', 'ass');
  assert(assContent.length > 0, 'ASS output should not be empty');
  assert(assContent.includes('[Script Info]'), 'ASS should contain [Script Info]');
  console.log(`PASS: xml -> ass, output length = ${assContent.length}`);

  // 测试 3: 带配置的转换
  const config = { text: { font_size: 25 }, danmaku: { scroll_duration: 8.0 } };
  const assWithConfig = df.convert(xmlContent, 'xml', 'ass', config);
  assert(assWithConfig.length > 0, 'ASS with config should not be empty');
  console.log(`PASS: convert with config, output length = ${assWithConfig.length}`);

  // 测试 4: 错误路径
  try {
    df.convert('<invalid>not xml</invalid>', 'xml', 'ass');
    // 可能不抛错（空弹幕池），但不应崩溃
    console.log('PASS: invalid xml handled gracefully');
  } catch (e) {
    assert(e.message.includes('['), 'error should contain code');
    console.log(`PASS: error path, message = ${e.message.substring(0, 80)}`);
  }

  // ---- 标准化句柄 API 测试 ----
  const { DanmakuFactoryContext } = await import('./danmakufactory.mjs');
  const fixtureXmlPath = join(REPO_ROOT, 'tests', 'fixtures', 'sample.xml');
  const fixtureContent = readFileSync(fixtureXmlPath, 'utf-8');

  // 测试 5: 句柄 API 基础转换（基于 VFS）
  {
    const FS = Module.FS;
    FS.writeFile('/tmp/h_input.xml', fixtureContent);
    const ctx = new DanmakuFactoryContext(Module);
    try {
      ctx.addInputFile('/tmp/h_input.xml');
      ctx.setOutputFile('/tmp/h_output.ass');
      ctx.setFontSize(25);
      ctx.run();
      const out = FS.readFile('/tmp/h_output.ass', { encoding: 'utf8' });
      assert(out.length > 0, 'handle API output should not be empty');
      console.log(`PASS: context convert, output length = ${out.length}`);
    } finally {
      ctx.destroy();
      try { FS.unlink('/tmp/h_input.xml'); } catch (_) {}
      try { FS.unlink('/tmp/h_output.ass'); } catch (_) {}
    }
  }

  // 测试 6: 句柄 API 多 setter + 批量配置
  {
    const FS = Module.FS;
    FS.writeFile('/tmp/h2_input.xml', fixtureContent);
    const ctx = new DanmakuFactoryContext(Module);
    try {
      ctx.addInputFile('/tmp/h2_input.xml');
      ctx.setOutputFile('/tmp/h2_output.ass');
      ctx.setResolution(1280, 720);
      ctx.setScrollDuration(9.0);
      ctx.setBold(true);
      ctx.setOpacity(200);
      ctx.setMsgboxShow(true);
      ctx.loadConfigJson('{"text": {"font_size": 30}}');
      ctx.run();
      const out = FS.readFile('/tmp/h2_output.ass', { encoding: 'utf8' });
      assert(out.length > 0, 'handle API setters output should not be empty');
      console.log(`PASS: context setters + loadConfigJson, output length = ${out.length}`);
    } finally {
      ctx.destroy();
      try { FS.unlink('/tmp/h2_input.xml'); } catch (_) {}
      try { FS.unlink('/tmp/h2_output.ass'); } catch (_) {}
    }
  }

  // 测试 7: 句柄 API 非法配置
  {
    const ctx = new DanmakuFactoryContext(Module);
    try {
      ctx.loadConfigJson('{invalid json!!!');
      console.error('FAIL: invalid config should throw');
      process.exit(1);
    } catch (e) {
      assert(e.message.includes('['), 'error should contain code');
      console.log(`PASS: context invalid config, message = ${e.message.substring(0, 60)}`);
    } finally {
      ctx.destroy();
    }
  }

  // 测试 8: emoji 文件名转换（VFS 支持 UTF-8 路径）
  {
    const FS = Module.FS;
    const emojiIn = '/tmp/输入🚩.xml';
    const emojiOut = '/tmp/输出🎉.ass';
    FS.writeFile(emojiIn, fixtureContent);

    // JSON 一次性 API（基于 VFS 路径）
    const runtimeInfo = { input_files: [{ path: emojiIn }], output_file: emojiOut };
    const code = df.convertRawJson(JSON.stringify(runtimeInfo));
    assert(code === 0, `emoji convert should succeed, code = ${code}`);
    const out = FS.readFile(emojiOut, { encoding: 'utf8' });
    assert(out.length > 0, 'emoji output should not be empty');

    // 句柄 API 再验证一次
    const emojiOut2 = '/tmp/输出2🚀.ass';
    const ctx = new DanmakuFactoryContext(Module);
    try {
      ctx.addInputFile(emojiIn);
      ctx.setOutputFile(emojiOut2);
      ctx.run();
      const out2 = FS.readFile(emojiOut2, { encoding: 'utf8' });
      assert(out2.length > 0, 'emoji output2 should not be empty');
    } finally {
      ctx.destroy();
      try { FS.unlink(emojiIn); } catch (_) {}
      try { FS.unlink(emojiOut); } catch (_) {}
      try { FS.unlink(emojiOut2); } catch (_) {}
    }
    console.log('PASS: emoji filename convert (input + output)');
  }

  console.log('\nALL TESTS PASSED');
}

main().catch((e) => {
  console.error('FAIL:', e.message);
  process.exit(1);
});
