/**
 * DanmakuFactory WASM 冒烟测试（Node.js）
 *
 * 运行: node tests/wasm_smoke.mjs
 */
import { createRequire } from 'module';
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = join(__dirname, '..');
const WASM_DIR = join(REPO_ROOT, 'build', 'wasm', 'wasm32', 'release');

// 加载 Emscripten 模块
const require = createRequire(import.meta.url);
const factory = require(join(WASM_DIR, 'danmakufactory.js'));

const SAMPLE_XML = `<?xml version="1.0" encoding="UTF-8"?>
<i>
    <chatserver>chat.bilibili.com</chatserver>
    <chatid>12345678</chatid>
    <d p="1.000,1,25,16777215,1609459200,0,abcdef12,1234567890">第一条滚动弹幕</d>
    <d p="2.500,1,25,16777215,1609459201,0,abcdef13,1234567891">第二条滚动弹幕</d>
    <d p="3.000,5,25,16777215,1609459202,0,abcdef14,1234567892">顶部固定弹幕</d>
    <d p="4.000,4,25,16777215,1609459203,0,abcdef15,1234567893">底部固定弹幕</d>
    <d p="5.000,6,25,255,1609459204,0,abcdef16,1234567894">左到右蓝色弹幕</d>
    <d p="6.500,1,18,16711680,1609459205,0,abcdef17,1234567895">红色小号弹幕</d>
    <d p="8.000,1,36,65280,1609459206,0,abcdef18,1234567896">绿色大号弹幕</d>
</i>`;

let passed = 0;
let failed = 0;

function assert(cond, msg) {
    if (cond) {
        console.log(`PASS: ${msg}`);
        passed++;
    } else {
        console.error(`FAIL: ${msg}`);
        failed++;
    }
}

async function main() {
    console.log('=== DanmakuFactory WASM Smoke Test ===\n');

    // 1. 初始化模块
    const Module = await factory();
    assert(Module !== null, 'Module initialized');

    // 2. 版本探活
    const version = Module.ccall('df_get_version', 'string', [], []);
    assert(version === '2.0.0', `df_get_version() = ${version}`);

    // 3. 准备 VFS
    try { Module.FS.mkdir('/tmp'); } catch (_) {}

    // 4. 基本转换：XML -> ASS（无字体，降级到旧算法）
    Module.FS.writeFile('/tmp/input.xml', SAMPLE_XML);
    const runtimeInfo = JSON.stringify({
        input_files: [{ path: '/tmp/input.xml' }],
        output_file: '/tmp/output.ass',
        config: { text: { fontname: 'Microsoft YaHei', font_size: 38 } }
    });
    const rc = Module.ccall('df_run_from_json', 'number', ['string'], [runtimeInfo]);
    assert(rc === 0, `XML -> ASS conversion (rc=${rc})`);

    if (rc === 0) {
        const ass = Module.FS.readFile('/tmp/output.ass', { encoding: 'utf8' });
        assert(ass.includes('[Script Info]'), 'ASS has [Script Info]');
        assert(ass.includes('[Events]'), 'ASS has [Events]');
        assert(ass.includes('\\move('), 'ASS has scrolling danmaku (\\move)');
        assert(ass.includes('\\pos('), 'ASS has fixed danmaku (\\pos)');
        assert(ass.includes('PlayResX: 1920'), 'Default resolution 1920x1080');

        // 提取 \move 坐标验证文本宽度计算
        const moveMatch = ass.match(/\\move\((\d+),(\d+),(-?\d+),(\d+)\)/);
        if (moveMatch) {
            const startX = parseInt(moveMatch[1]);
            const textLen = (startX - 1920) * 2;
            assert(textLen > 0, `Text width calculated: ${textLen}px (startX=${startX})`);
            console.log(`  INFO: First R2L danmaku textLen = ${textLen}px`);
        }
    } else {
        const err = Module.ccall('df_get_last_error_full_message', 'string', [], []);
        console.error(`  Error: ${err}`);
    }

    // 5. 字体注册 + 精确测量测试
    const fontPath = 'C:\\Windows\\Fonts\\msyh.ttc';
    let fontData;
    try {
        fontData = readFileSync(fontPath);
    } catch (_) {
        console.log('  INFO: msyh.ttc not found, skipping font registration test');
    }

    if (fontData) {
        // 注册字体
        try { Module.FS.mkdir('/fonts'); } catch (_) {}
        Module.FS.writeFile('/fonts/msyh.ttc', new Uint8Array(fontData));
        const regRc = Module.ccall('df_text_register_font', 'number',
            ['string', 'string'], ['Microsoft YaHei', '/fonts/msyh.ttc']);
        assert(regRc === 0, `Font registered (rc=${regRc})`);

        // 重新转换（现在应该使用精确测量）
        const rc2 = Module.ccall('df_run_from_json', 'number', ['string'], [runtimeInfo]);
        assert(rc2 === 0, `XML -> ASS with font (rc=${rc2})`);

        if (rc2 === 0) {
            const ass2 = Module.FS.readFile('/tmp/output.ass', { encoding: 'utf8' });
            const moveMatch2 = ass2.match(/\\move\((\d+),(\d+),(-?\d+),(\d+)\)/);
            if (moveMatch2) {
                const startX2 = parseInt(moveMatch2[1]);
                const textLen2 = (startX2 - 1920) * 2;
                assert(textLen2 > 0, `Precise text width: ${textLen2}px`);
                console.log(`  INFO: With font loaded, textLen = ${textLen2}px`);
            }
        }
    }

    // 6. 错误处理测试
    const badInfo = JSON.stringify({
        input_files: [{ path: '/tmp/nonexistent.xml' }],
        output_file: '/tmp/bad.ass'
    });
    const rcBad = Module.ccall('df_run_from_json', 'number', ['string'], [badInfo]);
    assert(rcBad !== 0, `Error path works (rc=${rcBad})`);
    const errMsg = Module.ccall('df_get_last_error_full_message', 'string', [], []);
    assert(errMsg.length > 0, `Error message: "${errMsg.substring(0, 60)}..."`);

    // 7. JSON -> XML 转换
    const sampleJson = '[[],[],[{"c":"1.000,16777215,1,25,,,","m":"JSON弹幕测试"}]]';
    Module.FS.writeFile('/tmp/input.json', sampleJson);
    const jsonInfo = JSON.stringify({
        input_files: [{ path: '/tmp/input.json' }],
        output_file: '/tmp/output.xml'
    });
    const rcJson = Module.ccall('df_run_from_json', 'number', ['string'], [jsonInfo]);
    assert(rcJson === 0, `JSON -> XML conversion (rc=${rcJson})`);
    if (rcJson === 0) {
        const xml = Module.FS.readFile('/tmp/output.xml', { encoding: 'utf8' });
        assert(xml.includes('<d p='), 'XML output has danmaku elements');
    }

    // 结果汇总
    console.log(`\n=== Results: ${passed} passed, ${failed} failed ===`);
    if (failed > 0) process.exit(1);
}

main().catch(e => {
    console.error('FATAL:', e);
    process.exit(1);
});
