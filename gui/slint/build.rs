//! build.rs — 编译 Slint UI 文件
//!
//! 原生链接由 danmakufactory-sys 提供；此处配置 GUI 二进制的运行时搜索路径。

fn main() {
    // 编译 Slint UI 文件（生成 include_modules! 所需的代码）
    slint_build::compile("ui/main.slint").expect("Failed to compile Slint UI");
    if std::env::var("DEP_DANMAKUFACTORY_LIB_KIND").as_deref() == Ok("shared") {
        let lib_dir = std::env::var("DEP_DANMAKUFACTORY_LIB_DIR").expect("native library directory");
        let target_os = std::env::var("CARGO_CFG_TARGET_OS").expect("Cargo target OS");
        let origin = match target_os.as_str() {
            "linux" => "$ORIGIN",
            "macos" => "@loader_path",
            _ => return,
        };
        println!("cargo:rustc-link-arg-bins=-Wl,-rpath,{origin}");
        println!("cargo:rustc-link-arg-bins=-Wl,-rpath,{lib_dir}");
    }
}
