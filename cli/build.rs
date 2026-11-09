use std::env;

fn main() {
    if env::var("DEP_DANMAKUFACTORY_LIB_KIND").as_deref() != Ok("shared") {
        return;
    }
    let lib_dir = env::var("DEP_DANMAKUFACTORY_LIB_DIR").expect("native library directory");
    let target_os = env::var("CARGO_CFG_TARGET_OS").expect("Cargo target OS");
    let origin = match target_os.as_str() {
        "linux" => "$ORIGIN",
        "macos" => "@loader_path",
        _ => return,
    };
    println!("cargo:rustc-link-arg-bins=-Wl,-rpath,{origin}");
    println!("cargo:rustc-link-arg-bins=-Wl,-rpath,{lib_dir}");
}
