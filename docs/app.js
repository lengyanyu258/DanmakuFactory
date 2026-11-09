/**
 * DanmakuFactory Web - 应用逻辑
 *
 * 加载 WASM 模块并提供弹幕格式转换的完整交互。
 * WASM 文件位于 ./wasm/ 目录（由 `xmake deploy-web` 任务从构建产物复制）。
 */

const WASM_PATH = new URL('./wasm/danmakufactory.js', import.meta.url).href;

// xmake 产物为经典/CommonJS 模块；浏览器以 script 加载其全局工厂。
function loadWasmFactory() {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = WASM_PATH;
    script.onload = () => {
      if (typeof window.DanmakuFactory === 'function') resolve(window.DanmakuFactory);
      else reject(new Error('WASM 工厂未导出'));
    };
    script.onerror = () => reject(new Error('无法加载 WASM 脚本'));
    document.head.appendChild(script);
  });
}

let Module = null;
let fontLoaded = false;
let outputFormat = null;

const $ = (id) => document.getElementById(id);
const setStatus = (msg, type) => {
  const el = $('status');
  el.textContent = '';
  const div = document.createElement('div');
  div.className = `status ${type}`;
  div.textContent = msg;
  el.appendChild(div);
};

/* ------------------------------------------------------------------ */
/* WASM 初始化                                                         */
/* ------------------------------------------------------------------ */

async function initWasm() {
  setStatus('正在加载 WASM 模块...', 'info');
  try {
    const factory = await loadWasmFactory();
    Module = await factory();
    try { Module.FS.mkdir('/tmp'); } catch (_) { /* 已存在 */ }
    try { Module.FS.mkdir('/fonts'); } catch (_) { /* 已存在 */ }
    const ver = Module.ccall('df_get_version', 'string', [], []);
    setStatus(`WASM 模块就绪 (v${ver})，可以开始转换`, 'ok');
  } catch (e) {
    setStatus(`WASM 加载失败: ${e.message}`, 'err');
  }
}

/* ------------------------------------------------------------------ */
/* 字体加载                                                            */
/* ------------------------------------------------------------------ */

function registerFont(name, data, fileName) {
  const vfsPath = `/fonts/${fileName || 'font.ttf'}`;
  Module.FS.writeFile(vfsPath, data);
  const rc = Module.ccall('df_text_register_font', 'number', ['string', 'string'], [name, vfsPath]);
  if (rc === 0) {
    fontLoaded = true;
    $('fontBadge').textContent = '精确测量';
    $('fontBadge').className = 'badge precise';
    const sizeMb = (data.length / 1024 / 1024).toFixed(1);
    setStatus(`字体 "${name}" 已注册 (${sizeMb} MB)，精确测量已启用`, 'ok');
  } else {
    setStatus(`字体注册失败 (rc=${rc})`, 'err');
  }
}

// 方式一：文件选择
$('fontFile').addEventListener('change', async (e) => {
  const file = e.target.files[0];
  if (!file || !Module) return;
  const data = new Uint8Array(await file.arrayBuffer());
  registerFont($('fontName').value, data, file.name);
});

// 方式二：queryLocalFonts (Chrome 103+)
$('btnLocalFonts').addEventListener('click', async () => {
  if (!Module) { setStatus('WASM 尚未加载', 'err'); return; }
  if (!window.queryLocalFonts) {
    setStatus('当前浏览器不支持 queryLocalFonts API（需 Chrome 103+ 且 HTTPS 环境）', 'err');
    return;
  }
  try {
    const fonts = await window.queryLocalFonts();
    const target = $('fontName').value.toLowerCase();
    const match = fonts.find((f) => f.family.toLowerCase().includes(target));
    if (!match) {
      setStatus(`未找到 "${$('fontName').value}"，共扫描 ${fonts.length} 个系统字体`, 'err');
      return;
    }
    const blob = await match.blob();
    const data = new Uint8Array(await blob.arrayBuffer());
    registerFont(match.family, data, 'local-font.ttf');
    $('fontName').value = match.family;
  } catch (e) {
    setStatus(`字体发现失败: ${e.message}`, 'err');
  }
});

/* ------------------------------------------------------------------ */
/* 示例数据                                                            */
/* ------------------------------------------------------------------ */

const SAMPLE_XML = `<?xml version="1.0" encoding="UTF-8"?>
<i>
  <chatserver>chat.bilibili.com</chatserver>
  <chatid>12345678</chatid>
  <d p="1.000,1,25,16777215,1609459200,0,ab01,01">这是一条从右到左的滚动弹幕</d>
  <d p="2.000,1,25,16777215,1609459201,0,ab02,02">第二条滚动弹幕 Hello World</d>
  <d p="3.000,5,25,16777215,1609459202,0,ab03,03">顶部固定弹幕</d>
  <d p="4.000,4,25,16777215,1609459203,0,ab04,04">底部固定弹幕</d>
  <d p="5.000,6,25,255,1609459204,0,ab05,05">从左到右的蓝色弹幕</d>
  <d p="6.500,1,18,16711680,1609459205,0,ab06,06">红色小号弹幕</d>
  <d p="8.000,1,36,65280,1609459206,0,ab07,07">绿色大号弹幕</d>
  <d p="9.000,1,25,16776960,1609459207,0,ab08,08">黄色弹幕 (゜-゜)つロ 干杯~</d>
</i>`;

$('btnSample').addEventListener('click', () => {
  $('inputFormat').value = 'xml';
  $('inputData').value = SAMPLE_XML;
  setStatus('已载入示例数据（含滚动/固定/彩色/多字号弹幕）', 'info');
});

/* ------------------------------------------------------------------ */
/* 转换                                                                */
/* ------------------------------------------------------------------ */

$('btnConvert').addEventListener('click', () => {
  if (!Module) { setStatus('WASM 未加载', 'err'); return; }
  const input = $('inputData').value.trim();
  if (!input) { setStatus('请输入弹幕数据', 'err'); return; }

  const inFmt = $('inputFormat').value;
  const outFmt = $('outputFormat').value;
  const [resX, resY] = $('resolution').value.split(',').map(Number);
  const fontSize = parseInt($('fontSize').value, 10);
  const scrollDur = parseFloat($('scrollDuration').value);
  const fixedDur = parseFloat($('fixedDuration').value);

  const inPath = `/tmp/input.${inFmt}`;
  const outPath = `/tmp/output.${outFmt}`;
  Module.FS.writeFile(inPath, input);

  const config = {
    screen: { resolution: [resX, resY] },
    text: { fontname: $('fontName').value, font_size: fontSize },
    danmaku: { scroll_duration: scrollDur, fixed_duration: fixedDur },
  };

  const runtimeInfo = JSON.stringify({
    input_files: [{ path: inPath }],
    output_file: outPath,
    config,
  });

  const t0 = performance.now();
  const rc = Module.ccall('df_run_from_json', 'number', ['string'], [runtimeInfo]);
  const elapsed = (performance.now() - t0).toFixed(1);

  if (rc === 0) {
    const output = Module.FS.readFile(outPath, { encoding: 'utf8' });
    $('output').textContent = output;
    outputFormat = outFmt;
    $('btnDownload').disabled = false;
    const mode = fontLoaded ? '精确测量' : '估算模式';
    setStatus(`转换成功 (${elapsed} ms · ${mode} · ${output.length.toLocaleString()} bytes)`, 'ok');
  } else {
    const err = Module.ccall('df_get_last_error_full_message', 'string', [], []);
    $('output').textContent = `错误 [${rc}]: ${err}`;
    $('btnDownload').disabled = true;
    setStatus(`转换失败 (rc=${rc})`, 'err');
  }
});

/* ------------------------------------------------------------------ */
/* 下载                                                                */
/* ------------------------------------------------------------------ */

$('btnDownload').addEventListener('click', () => {
  const content = $('output').textContent;
  const fmt = outputFormat;
  const blob = new Blob([content], { type: 'text/plain;charset=utf-8' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `danmaku_output.${fmt}`;
  a.click();
  URL.revokeObjectURL(a.href);
});

/* ------------------------------------------------------------------ */
/* 拖放支持                                                            */
/* ------------------------------------------------------------------ */

document.addEventListener('dragover', (e) => e.preventDefault());
document.addEventListener('drop', async (e) => {
  e.preventDefault();
  const file = e.dataTransfer.files[0];
  if (!file) return;
  const text = await file.text();
  $('inputData').value = text;
  $('inputFormat').value = file.name.toLowerCase().endsWith('.json') ? 'json' : 'xml';
  setStatus(`已载入文件: ${file.name} (${(file.size / 1024).toFixed(1)} KB)`, 'info');
});

/* ------------------------------------------------------------------ */
/* 启动                                                                */
/* ------------------------------------------------------------------ */

initWasm();
