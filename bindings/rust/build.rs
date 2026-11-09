//! build.rs — 链接 xmake 构建的 DanmakuFactory 库
//!
//! 环境变量（可选，由 xmake task 传入）：
//!   DF_LIB_DIR  — 直接指定库搜索目录
//!   DF_LIB_KIND — "static" 或 "shared"（默认 shared）

use std::{env, fs};
use std::path::PathBuf;

fn main() {
    let target_os = env::var("CARGO_CFG_TARGET_OS").expect("Cargo target OS");
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").expect("Cargo target architecture");
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // bindings/rust/ -> bindings/ -> 项目根目录
    let project_root = manifest_dir.parent().unwrap().parent().unwrap().to_path_buf();

    // 确定库搜索路径
    let lib_dir = if let Ok(dir) = env::var("DF_LIB_DIR") {
        PathBuf::from(dir)
    } else {
        // 根据平台和 profile 推断 xmake 输出目录
        let plat = if target_os == "windows" {
            "windows"
        } else if target_os == "macos" {
            "macosx"
        } else {
            "linux"
        };

        let arch = match (target_os.as_str(), target_arch.as_str()) {
            ("windows", "x86_64") => "x64",
            (_, "x86_64") => "x86_64",
            (_, "aarch64") => "arm64",
            ("windows", "x86") => "x86",
            (_, "x86") => "i386",
            (_, arch) => arch,
        };

        let mode = if env::var("PROFILE").unwrap_or_default() == "release" {
            "release"
        } else {
            "debug"
        };

        project_root.join("build").join(plat).join(arch).join(mode)
    };

    let lib_dir = fs::canonicalize(&lib_dir).unwrap_or(lib_dir);
    println!("cargo:rustc-link-search=native={}", lib_dir.display());

    // xrepo 静态库存放目录（~/.xmake/packages 或项目 .xmake 缓存）
    // 当 DF_XREPO_LIB_DIR 指定时额外搜索该目录
    if let Ok(xrepo_dir) = env::var("DF_XREPO_LIB_DIR") {
        println!("cargo:rustc-link-search=native={xrepo_dir}");
    }

    // 选择链接方式
    let kind = env::var("DF_LIB_KIND").unwrap_or_else(|_| "shared".to_string());
    let target = env::var("TARGET").unwrap_or_default();
    println!("cargo:lib_dir={}", lib_dir.display());
    println!("cargo:lib_kind={kind}");

    if kind == "static" {
        if target.contains("windows") {
            println!("cargo:rustc-link-lib=static=DanmakuFactory-static");
        } else {
            println!("cargo:rustc-link-lib=static=DanmakuFactory");
        }

        // 静态链接必须显式列出全部传递依赖（链接顺序：高层在前）
        println!("cargo:rustc-link-lib=static=yyjson");
        println!("cargo:rustc-link-lib=static=fontconfig");
        println!("cargo:rustc-link-lib=static=freetype");
        println!("cargo:rustc-link-lib=static=harfbuzz");
        println!("cargo:rustc-link-lib=static=pcre2-8");
        println!("cargo:rustc-link-lib=static=expat");
        println!("cargo:rustc-link-lib=static=png");
        println!("cargo:rustc-link-lib=static=bz2");
        println!("cargo:rustc-link-lib=static=z");
        if target_os == "linux" {
            println!("cargo:rustc-link-lib=m");
        }

        // musl 目标：链接 musl libc 与编译器内置库
        if target.contains("musl") {
            println!("cargo:rustc-link-lib=static=c");
            println!("cargo:rustc-link-lib=static=gcc");
        }
    } else {
        // 动态链接：Windows 下链接导入库 DanmakuFactory.lib
        println!("cargo:rustc-link-lib=dylib=DanmakuFactory");
        if target_os == "windows" {
            // Cargo binaries, tests and examples use separate executable directories.
            // Windows searches each executable's directory for runtime DLLs.
            let out_dir = PathBuf::from(env::var_os("OUT_DIR").expect("Cargo OUT_DIR"));
            let profile_dir = out_dir.ancestors().nth(3).expect("Cargo profile directory");
            for entry in fs::read_dir(&lib_dir).expect("DanmakuFactory library directory") {
                let path = entry.expect("library directory entry").path();
                if path.extension().and_then(|ext| ext.to_str()).is_some_and(|ext| ext.eq_ignore_ascii_case("dll")) {
                    println!("cargo:rerun-if-changed={}", path.display());
                    for dir in [profile_dir.to_path_buf(), profile_dir.join("deps"), profile_dir.join("examples")] {
                        fs::create_dir_all(&dir).expect("Cargo executable directory");
                        let destination = dir.join(path.file_name().unwrap());
                        if fs::canonicalize(&destination).ok() != fs::canonicalize(&path).ok() {
                            fs::copy(&path, destination)
                                .expect("Copy native runtime DLL beside Cargo executable");
                        }
                    }
                }
            }
        }
    }

    // 当库目录发生变化时重新运行
    println!("cargo:rerun-if-env-changed=DF_LIB_DIR");
    println!("cargo:rerun-if-env-changed=DF_LIB_KIND");
    println!("cargo:rerun-if-env-changed=DF_XREPO_LIB_DIR");
    println!("cargo:rerun-if-changed={}", lib_dir.display());
}
