//! DanmakuFactory Rust 绑定集成测试
//!
//! 运行前置条件:
//!   1. 构建 C 核心库: xmake build lib-shared
//!   2. 运行: cargo test -p danmakufactory-sys
//!
//! 注意: 运行测试时需要 DanmakuFactory 动态库可被加载。

use danmakufactory_sys::{get_version, run_from_json, Context};
use std::path::PathBuf;

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .to_path_buf()
}

fn sample_xml() -> PathBuf {
    repo_root().join("tests/fixtures/sample.xml")
}

#[test]
fn test_version() {
    let v = get_version();
    assert!(!v.is_empty(), "version should not be empty");
    assert!(v.contains('.'), "version should contain a dot: {v}");
}

#[test]
fn test_handle_api_convert() {
    let input = sample_xml();
    assert!(input.exists(), "fixture missing: {}", input.display());

    let tmp = std::env::temp_dir().join("df_rust_test_handle.ass");

    let mut ctx = Context::new().expect("df_create should succeed");
    ctx.add_input_file(input.to_str().unwrap(), 0.0).unwrap();
    ctx.set_output_file(tmp.to_str().unwrap()).unwrap();
    ctx.set_font_size(25).unwrap();
    ctx.run().unwrap();

    assert!(tmp.exists(), "output file not generated");
    assert!(tmp.metadata().unwrap().len() > 0, "output file is empty");
    let _ = std::fs::remove_file(&tmp);
}

#[test]
fn test_handle_api_with_config_setters() {
    let input = sample_xml();
    let tmp = std::env::temp_dir().join("df_rust_test_setters.ass");

    let mut ctx = Context::new().expect("df_create");
    ctx.add_input_file(input.to_str().unwrap(), 0.0).unwrap();
    ctx.set_output_file(tmp.to_str().unwrap()).unwrap();

    // 覆盖各分组的 setter
    ctx.set_resolution(1280, 720).unwrap();
    ctx.set_display_area(0.8).unwrap();
    ctx.set_scroll_duration(9.0).unwrap();
    ctx.set_fixed_duration(4.0).unwrap();
    ctx.set_density(-1).unwrap();
    ctx.set_bold(true).unwrap();
    ctx.set_opacity(200).unwrap();
    ctx.set_outline(1.0).unwrap();
    ctx.set_msgbox_show(true).unwrap();
    ctx.set_msgbox_size(400, 800).unwrap();

    ctx.run().unwrap();
    assert!(tmp.exists());
    let _ = std::fs::remove_file(&tmp);
}

#[test]
fn test_handle_api_load_config_json() {
    let input = sample_xml();
    let tmp = std::env::temp_dir().join("df_rust_test_loadcfg.ass");

    let mut ctx = Context::new().expect("df_create");
    ctx.add_input_file(input.to_str().unwrap(), 0.0).unwrap();
    ctx.set_output_file(tmp.to_str().unwrap()).unwrap();

    // 裸配置格式
    ctx.load_config_json(r#"{"text": {"font_size": 30}}"#).unwrap();
    // 包装格式
    ctx.load_config_json(r#"{"config": {"danmaku": {"scroll_duration": 7.0}}}"#)
        .unwrap();

    ctx.run().unwrap();
    assert!(tmp.exists());
    let _ = std::fs::remove_file(&tmp);
}

#[test]
fn test_handle_api_invalid_config() {
    let mut ctx = Context::new().expect("df_create");
    let result = ctx.load_config_json("{invalid json!!!");
    assert!(result.is_err(), "invalid JSON should fail");
    let err = result.unwrap_err();
    assert!(!err.message.is_empty(), "error message should not be empty");
}

#[test]
fn test_json_one_shot_api() {
    let input = sample_xml();
    let tmp = std::env::temp_dir().join("df_rust_test_json.ass");

    let runtime_info = format!(
        r#"{{"input_files": [{{"path": "{}"}}], "output_file": "{}"}}"#,
        input.to_str().unwrap().replace('\\', "/"),
        tmp.to_str().unwrap().replace('\\', "/")
    );

    run_from_json(&runtime_info).unwrap();
    assert!(tmp.exists());
    let _ = std::fs::remove_file(&tmp);
}

#[test]
fn test_json_api_error_nonexistent_file() {
    let tmp_out = std::env::temp_dir().join("df_rust_test_never.ass");
    let runtime_info = format!(
        r#"{{"input_files": ["/nonexistent/no_such.xml"], "output_file": "{}"}}"#,
        tmp_out.to_str().unwrap().replace('\\', "/")
    );

    let result = run_from_json(&runtime_info);
    assert!(result.is_err(), "nonexistent input should fail");
}

#[test]
fn test_clear_input_files() {
    let mut ctx = Context::new().expect("df_create");
    ctx.add_input_file("dummy.xml", 0.0).unwrap();
    ctx.clear_input_files().unwrap();
    // 清除后无输入文件，run 应失败（无输入）
    let result = ctx.run();
    assert!(result.is_err(), "run without input files should fail");
}

#[test]
fn test_emoji_filename_convert() {
    // emoji 文件名转换：输入与输出均含 emoji，验证 UTF-8 路径支持
    let src = sample_xml();
    assert!(src.exists(), "fixture missing: {}", src.display());

    let tmp = std::env::temp_dir().join("df_rust_emoji");
    std::fs::create_dir_all(&tmp).unwrap();

    let emoji_input = tmp.join("输入🚩.xml");
    let emoji_output = tmp.join("输出🎉.ass");
    std::fs::copy(&src, &emoji_input).unwrap();

    // 句柄 API
    let mut ctx = Context::new().expect("df_create");
    ctx.add_input_file(emoji_input.to_str().unwrap(), 0.0).unwrap();
    ctx.set_output_file(emoji_output.to_str().unwrap()).unwrap();
    ctx.run().unwrap();
    assert!(emoji_output.exists(), "emoji output not generated");
    assert!(emoji_output.metadata().unwrap().len() > 0, "emoji output empty");

    // JSON 一次性 API 也验证 emoji 路径
    let emoji_output2 = tmp.join("输出2🚀.ass");
    let runtime_info = format!(
        r#"{{"input_files": [{{"path": "{}"}}], "output_file": "{}"}}"#,
        emoji_input.to_str().unwrap().replace('\\', "/"),
        emoji_output2.to_str().unwrap().replace('\\', "/")
    );
    run_from_json(&runtime_info).unwrap();
    assert!(emoji_output2.exists(), "emoji output2 not generated");

    let _ = std::fs::remove_dir_all(&tmp);
}
