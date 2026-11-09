/**
 * DanmakuFactory JavaScript/WASM 绑定
 *
 * 通过 Emscripten 编译的 WebAssembly 模块调用 DanmakuFactory。
 * 使用 Emscripten 虚拟文件系统 (VFS) 进行文件 I/O。
 *
 * 前置条件:
 *   1. 安装 Emscripten: https://emscripten.org/docs/getting_started/downloads.html
 *   2. 构建 WASM: xmake f -p wasm -a wasm32 -m release && xmake build lib-wasm
 *
 * 用法 (Node.js):
 *   import { DanmakuFactory } from './danmakufactory.mjs';
 *   const df = await DanmakuFactory.create();
 *   console.log(df.version);
 *   const result = df.convert(inputData, 'xml', 'ass');
 *
 * 用法 (浏览器):
 *   <script type="module">
 *     import { DanmakuFactory } from './danmakufactory.mjs';
 *     const df = await DanmakuFactory.create('./wasm/danmakufactory.js');
 *     const result = df.convert(xmlContent, 'xml', 'ass');
 *   </script>
 */

/**
 * DanmakuFactory WASM 封装类
 */
export class DanmakuFactoryInstance {
  /**
   * @param {object} Module - Emscripten Module 实例
   */
  constructor(Module) {
    this._module = Module;
    this._df_get_version = Module.cwrap('df_get_version', 'string', []);
    this._df_run_from_json = Module.cwrap('df_run_from_json', 'number', ['string']);
    this._df_get_last_error_code = Module.cwrap('df_get_last_error_code', 'number', []);
    this._df_get_last_error_full_message = Module.cwrap('df_get_last_error_full_message', 'string', []);
  }

  /** @returns {string} 库版本字符串 */
  get version() {
    return this._df_get_version();
  }

  /** @returns {number} 最近一次错误码 */
  get lastErrorCode() {
    return this._df_get_last_error_code();
  }

  /** @returns {string} 最近一次错误消息 */
  get lastErrorMessage() {
    return this._df_get_last_error_full_message();
  }

  /**
   * 执行弹幕格式转换（基于内存数据）
   *
   * @param {string|Uint8Array} inputData - 输入文件内容
   * @param {string} inputType - 输入格式: 'xml' | 'json' | 'ass'
   * @param {string} outputType - 输出格式: 'xml' | 'json' | 'ass'
   * @param {object} [config] - 可选配置对象
   * @param {number} [timeShift=0] - 时轴偏移量（秒）
   * @returns {string} 转换后的文件内容
   * @throws {Error} 转换失败时抛出
   */
  convert(inputData, inputType, outputType, config = null, timeShift = 0) {
    const FS = this._module.FS;
    const inputPath = `/tmp/input.${inputType}`;
    const outputPath = `/tmp/output.${outputType}`;

    try {
      // 写入输入文件到 VFS
      if (typeof inputData === 'string') {
        FS.writeFile(inputPath, inputData);
      } else {
        FS.writeFile(inputPath, inputData);
      }

      // 构建 runtime_info JSON
      const runtimeInfo = {
        input_files: [{ path: inputPath, time_shift: timeShift }],
        output_file: outputPath,
      };
      if (config) {
        runtimeInfo.config = config;
      }

      const jsonStr = JSON.stringify(runtimeInfo);
      const code = this._df_run_from_json(jsonStr);

      if (code !== 0) {
        throw new Error(`[${code}] ${this.lastErrorMessage}`);
      }

      // 读取输出文件
      const outputData = FS.readFile(outputPath, { encoding: 'utf8' });
      return outputData;
    } finally {
      // 清理 VFS
      try { FS.unlink(inputPath); } catch (_) { /* ignore */ }
      try { FS.unlink(outputPath); } catch (_) { /* ignore */ }
    }
  }

  /**
   * 直接传入 JSON 字符串执行转换（底层接口）
   *
   * @param {string} runtimeInfoJson - 完整的 runtime_info JSON
   * @returns {number} 错误码（0 表示成功）
   */
  convertRawJson(runtimeInfoJson) {
    return this._df_run_from_json(runtimeInfoJson);
  }

  /**
   * 加载字体文件到 WASM 虚拟文件系统并注册
   *
   * 在 WASM 环境中无 Fontconfig，需由调用方提供字体文件。
   * 浏览器中可通过 queryLocalFonts() 或 fetch() 获取字体数据；
   * Node.js 中可通过 fs.readFile() 读取系统字体文件。
   *
   * @param {string} fontName - 字体族名（需与 config.text.fontname 一致）
   * @param {ArrayBuffer|Uint8Array} fontData - 字体文件二进制数据
   * @param {string} [fileName] - VFS 中的文件名（默认从 fontName 推导）
   * @throws {Error} 注册失败时抛出
   */
  loadFont(fontName, fontData, fileName = null) {
    const FS = this._module.FS;
    // Keep Unicode family names distinct (e.g. 宋体 and 黑体).
    const safeName = fileName || Array.from(fontName, char =>
      char.codePointAt(0).toString(16)).join('_') + '.ttf';
    const vfsPath = `/fonts/${safeName}`;

    // 确保 /fonts 目录存在
    try { FS.mkdir('/fonts'); } catch (_) { /* 已存在 */ }

    // 写入字体文件到 VFS
    FS.writeFile(vfsPath, new Uint8Array(fontData));

    // 调用 C 层注册接口
    const ret = this._module.ccall('df_text_register_font', 'number',
      ['string', 'string'], [fontName, vfsPath]);
    if (ret !== 0) {
      throw new Error(`Failed to register font "${fontName}" at "${vfsPath}"`);
    }
  }

  /**
   * 创建一个标准化句柄上下文（推荐用于需要精细配置的场景）
   *
   * @returns {DanmakuFactoryContext}
   */
  createContext() {
    return new DanmakuFactoryContext(this._module);
  }
}

/**
 * DanmakuFactory 标准化句柄封装（推荐）
 *
 * 遵循 create → configure → run → destroy 生命周期。
 * 通过 Emscripten cwrap 调用 C 句柄 API，句柄以指针（number）表示。
 *
 * 用法:
 *   const ctx = df.createContext();
 *   try {
 *     ctx.addInputFile('/tmp/input.xml');
 *     ctx.setOutputFile('/tmp/output.ass');
 *     ctx.setFontSize(25);
 *     ctx.run();
 *   } finally {
 *     ctx.destroy();
 *   }
 */
export class DanmakuFactoryContext {
  /**
   * @param {object} Module - Emscripten Module 实例
   */
  constructor(Module) {
    this._module = Module;

    // 句柄 API cwrap 声明
    this._df_create = Module.cwrap('df_create', 'number', []);
    this._df_destroy = Module.cwrap('df_destroy', null, ['number']);
    this._df_run = Module.cwrap('df_run', 'number', ['number']);
    this._df_load_config_json = Module.cwrap('df_load_config_json', 'number', ['number', 'string']);
    this._df_load_config_file = Module.cwrap('df_load_config_file', 'number', ['number', 'string']);
    this._df_add_input_file = Module.cwrap('df_add_input_file', 'number', ['number', 'string', 'number']);
    this._df_clear_input_files = Module.cwrap('df_clear_input_files', 'number', ['number']);
    this._df_set_output_file = Module.cwrap('df_set_output_file', 'number', ['number', 'string']);
    // 屏幕
    this._df_set_resolution = Module.cwrap('df_set_resolution', 'number', ['number', 'number', 'number']);
    this._df_set_display_area = Module.cwrap('df_set_display_area', 'number', ['number', 'number']);
    this._df_set_scroll_area = Module.cwrap('df_set_scroll_area', 'number', ['number', 'number']);
    this._df_set_statistics_mode = Module.cwrap('df_set_statistics_mode', 'number', ['number', 'number']);
    // 弹幕
    this._df_set_density = Module.cwrap('df_set_density', 'number', ['number', 'number']);
    this._df_set_scroll_duration = Module.cwrap('df_set_scroll_duration', 'number', ['number', 'number']);
    this._df_set_fixed_duration = Module.cwrap('df_set_fixed_duration', 'number', ['number', 'number']);
    this._df_set_show_username = Module.cwrap('df_set_show_username', 'number', ['number', 'number']);
    this._df_set_block_mode = Module.cwrap('df_set_block_mode', 'number', ['number', 'number']);
    this._df_set_keep_blocked = Module.cwrap('df_set_keep_blocked', 'number', ['number', 'number']);
    // 文本
    this._df_set_fontname = Module.cwrap('df_set_fontname', 'number', ['number', 'string']);
    this._df_set_font_size = Module.cwrap('df_set_font_size', 'number', ['number', 'number']);
    this._df_set_font_size_strict = Module.cwrap('df_set_font_size_strict', 'number', ['number', 'number']);
    this._df_set_font_size_norm = Module.cwrap('df_set_font_size_norm', 'number', ['number', 'number']);
    this._df_set_bold = Module.cwrap('df_set_bold', 'number', ['number', 'number']);
    this._df_set_shadow = Module.cwrap('df_set_shadow', 'number', ['number', 'number']);
    this._df_set_opacity = Module.cwrap('df_set_opacity', 'number', ['number', 'number']);
    this._df_set_outline = Module.cwrap('df_set_outline', 'number', ['number', 'number']);
    this._df_set_outline_blur = Module.cwrap('df_set_outline_blur', 'number', ['number', 'number']);
    this._df_set_outline_opacity = Module.cwrap('df_set_outline_opacity', 'number', ['number', 'number']);
    this._df_set_line_spacing = Module.cwrap('df_set_line_spacing', 'number', ['number', 'number']);
    // 消息框
    this._df_set_msgbox_show = Module.cwrap('df_set_msgbox_show', 'number', ['number', 'number']);
    this._df_set_msgbox_size = Module.cwrap('df_set_msgbox_size', 'number', ['number', 'number', 'number']);
    this._df_set_msgbox_pos = Module.cwrap('df_set_msgbox_pos', 'number', ['number', 'number', 'number']);
    this._df_set_msgbox_font_size = Module.cwrap('df_set_msgbox_font_size', 'number', ['number', 'number']);
    this._df_set_msgbox_duration = Module.cwrap('df_set_msgbox_duration', 'number', ['number', 'number']);
    this._df_set_msgbox_gift_min_price = Module.cwrap('df_set_msgbox_gift_min_price', 'number', ['number', 'number']);
    // 错误
    this._df_get_last_error_full_message = Module.cwrap('df_get_last_error_full_message', 'string', []);

    this._ctx = this._df_create();
    if (!this._ctx) {
      throw new Error('df_create() returned NULL (out of memory)');
    }
  }

  _check(code) {
    if (code !== 0) {
      throw new Error(`[${code}] ${this._df_get_last_error_full_message()}`);
    }
  }

  /** 验证配置并执行转换 */
  run() { this._check(this._df_run(this._ctx)); }

  // --- 批量配置加载 ---
  loadConfigJson(jsonStr) { this._check(this._df_load_config_json(this._ctx, jsonStr)); }
  loadConfigFile(path) { this._check(this._df_load_config_file(this._ctx, path)); }

  // --- 文件管理 ---
  addInputFile(path, timeShift = 0) { this._check(this._df_add_input_file(this._ctx, path, timeShift)); }
  clearInputFiles() { this._check(this._df_clear_input_files(this._ctx)); }
  setOutputFile(path) { this._check(this._df_set_output_file(this._ctx, path)); }

  // --- 屏幕配置 ---
  setResolution(width, height) { this._check(this._df_set_resolution(this._ctx, width, height)); }
  setDisplayArea(area) { this._check(this._df_set_display_area(this._ctx, area)); }
  setScrollArea(area) { this._check(this._df_set_scroll_area(this._ctx, area)); }
  setStatisticsMode(mode) { this._check(this._df_set_statistics_mode(this._ctx, mode)); }

  // --- 弹幕配置 ---
  setDensity(density) { this._check(this._df_set_density(this._ctx, density)); }
  setScrollDuration(seconds) { this._check(this._df_set_scroll_duration(this._ctx, seconds)); }
  setFixedDuration(seconds) { this._check(this._df_set_fixed_duration(this._ctx, seconds)); }
  setShowUsername(show) { this._check(this._df_set_show_username(this._ctx, show ? 1 : 0)); }
  setBlockMode(mode) { this._check(this._df_set_block_mode(this._ctx, mode)); }
  setKeepBlocked(keep) { this._check(this._df_set_keep_blocked(this._ctx, keep ? 1 : 0)); }

  // --- 文本配置 ---
  setFontname(fontname) { this._check(this._df_set_fontname(this._ctx, fontname)); }
  setFontSize(size) { this._check(this._df_set_font_size(this._ctx, size)); }
  setFontSizeStrict(strict) { this._check(this._df_set_font_size_strict(this._ctx, strict ? 1 : 0)); }
  setFontSizeNorm(norm) { this._check(this._df_set_font_size_norm(this._ctx, norm ? 1 : 0)); }
  setBold(bold) { this._check(this._df_set_bold(this._ctx, bold ? 1 : 0)); }
  setShadow(shadow) { this._check(this._df_set_shadow(this._ctx, shadow)); }
  setOpacity(opacity) { this._check(this._df_set_opacity(this._ctx, opacity)); }
  setOutline(outline) { this._check(this._df_set_outline(this._ctx, outline)); }
  setOutlineBlur(blur) { this._check(this._df_set_outline_blur(this._ctx, blur)); }
  setOutlineOpacity(opacity) { this._check(this._df_set_outline_opacity(this._ctx, opacity)); }
  setLineSpacing(spacing) { this._check(this._df_set_line_spacing(this._ctx, spacing)); }

  // --- 消息框配置 ---
  setMsgboxShow(show) { this._check(this._df_set_msgbox_show(this._ctx, show ? 1 : 0)); }
  setMsgboxSize(width, height) { this._check(this._df_set_msgbox_size(this._ctx, width, height)); }
  setMsgboxPos(x, y) { this._check(this._df_set_msgbox_pos(this._ctx, x, y)); }
  setMsgboxFontSize(size) { this._check(this._df_set_msgbox_font_size(this._ctx, size)); }
  setMsgboxDuration(seconds) { this._check(this._df_set_msgbox_duration(this._ctx, seconds)); }
  setMsgboxGiftMinPrice(price) { this._check(this._df_set_msgbox_gift_min_price(this._ctx, price)); }

  /** 销毁上下文（幂等） */
  destroy() {
    if (this._ctx) {
      this._df_destroy(this._ctx);
      this._ctx = null;
    }
  }
}

// xmake emits a classic MODULARIZE script in browsers and CommonJS in Node.
const browserFactories = new Map();

function loadBrowserFactory(url) {
  if (!browserFactories.has(url.href)) {
    browserFactories.set(url.href, new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = url.href;
      script.onload = () => {
        if (typeof globalThis.DanmakuFactory === 'function') resolve(globalThis.DanmakuFactory);
        else reject(new Error('WASM script did not expose the DanmakuFactory factory'));
      };
      script.onerror = () => reject(new Error(`Failed to load WASM script: ${url.href}`));
      document.head.appendChild(script);
    }).catch(error => {
      browserFactories.delete(url.href);
      throw error;
    }));
  }
  return browserFactories.get(url.href);
}

/**
 * DanmakuFactory 工厂对象
 */
export const DanmakuFactory = {
  /**
   * 创建 DanmakuFactory 实例
   *
   * @param {string} [wasmPath] - danmakufactory.js 的路径（浏览器环境需要）
   * @returns {Promise<DanmakuFactoryInstance>}
   */
  async create(wasmPath = null) {
    let Module;

    const defaultUrl = new URL('../../docs/wasm/danmakufactory.js', import.meta.url);
    if (typeof document !== 'undefined') {
      const url = wasmPath ? new URL(wasmPath, document.baseURI) : defaultUrl;
      const factory = await loadBrowserFactory(url);
      Module = await factory({ locateFile: file => new URL(file, url).href });
    } else {
      // Import a file URL intact: .pathname loses its scheme on Windows and
      // leaves percent-escaped spaces/Unicode paths undecoded.
      const { pathToFileURL } = await import('node:url');
      const url = wasmPath && /^[A-Za-z]:[\\/]/.test(wasmPath)
        ? pathToFileURL(wasmPath)
        : wasmPath ? new URL(wasmPath, import.meta.url) : defaultUrl;
      const factory = (await import(url.href)).default;
      Module = await factory();
    }

    // 确保 /tmp 目录存在
    try {
      Module.FS.mkdir('/tmp');
    } catch (_) {
      // 目录已存在
    }

    return new DanmakuFactoryInstance(Module);
  },
};

export default DanmakuFactory;
