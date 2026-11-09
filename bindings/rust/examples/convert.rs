//! DanmakuFactory Rust 绑定使用示例
//!
//! 演示两套 API：
//!   1. 标准化句柄 API（推荐）—— create → configure → run → destroy
//!   2. JSON 一次性 API（便捷）
//!
//! 运行前置条件:
//!   1. 构建 C 核心库: xmake build lib-shared
//!   2. 运行: cargo run --example convert
//!
//! 注意: 运行时需要 DanmakuFactory 动态库可被加载（与可执行文件同目录或系统 PATH）。

use danmakufactory_sys::{get_version, run_from_json, Context};
use std::path::PathBuf;

fn repo_root() -> PathBuf {
    // bindings/rust/examples -> bindings/rust -> bindings -> repo root
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .to_path_buf()
}

fn main() {
    println!("DanmakuFactory Rust binding example");
    println!("Library version: {}", get_version());
    println!();

    let input = repo_root().join("tests/fixtures/sample.xml");
    if !input.exists() {
        eprintln!("test fixture missing: {}", input.display());
        std::process::exit(1);
    }

    example_handle_api(&input);
    example_json_api(&input);
}

/// 示例 1：标准化句柄 API
fn example_handle_api(input: &PathBuf) {
    println!("=== Handle API (create -> configure -> run -> destroy) ===");

    let tmp = std::env::temp_dir().join("df_rust_example_handle.ass");

    // create
    let mut ctx = match Context::new() {
        Some(c) => c,
        None => {
            eprintln!("df_create() failed (out of memory)");
            return;
        }
    };

    // configure：文件 + 单项 setter 微调
    if let Err(e) = ctx.add_input_file(input.to_str().unwrap(), 0.0) {
        eprintln!("add_input_file failed: {e}");
        return;
    }
    if let Err(e) = ctx.set_output_file(tmp.to_str().unwrap()) {
        eprintln!("set_output_file failed: {e}");
        return;
    }
    let _ = ctx.set_font_size(30);
    let _ = ctx.set_resolution(1920, 1080);
    let _ = ctx.set_scroll_duration(10.0);
    let _ = ctx.set_bold(true);

    // run
    match ctx.run() {
        Ok(()) => println!("  OK -> {}", tmp.display()),
        Err(e) => eprintln!("  run failed: {e}"),
    }
    // ctx 在此离开作用域，自动 df_destroy
    println!();
}

/// 示例 2：批量配置加载（load_config_json）
#[allow(dead_code)]
fn example_batch_config(input: &PathBuf) {
    let tmp = std::env::temp_dir().join("df_rust_example_cfg.ass");
    let mut ctx = Context::new().expect("df_create");
    ctx.add_input_file(input.to_str().unwrap(), 0.0).unwrap();
    ctx.set_output_file(tmp.to_str().unwrap()).unwrap();

    // 一次性加载多项配置（合并语义）
    let config_json = r#"{
        "text": { "font_size": 25, "opacity": 200 },
        "danmaku": { "scroll_duration": 8.0 }
    }"#;
    ctx.load_config_json(config_json).unwrap();
    ctx.run().unwrap();
}

/// 示例 3：JSON 一次性 API
fn example_json_api(input: &PathBuf) {
    println!("=== JSON one-shot API ===");

    let tmp = std::env::temp_dir().join("df_rust_example_json.ass");
    let runtime_info = format!(
        r#"{{"input_files": [{{"path": "{}", "time_shift": 0.0}}], "output_file": "{}"}}"#,
        input.to_str().unwrap().replace('\\', "/"),
        tmp.to_str().unwrap().replace('\\', "/")
    );

    match run_from_json(&runtime_info) {
        Ok(()) => println!("  OK -> {}", tmp.display()),
        Err(e) => eprintln!("  failed: {e}"),
    }
    println!();
}
