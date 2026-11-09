//! DanmakuFactory CLI — Rust + clap 实现
//!
//! 通过 FFI 调用 C 核心库的 df_run_from_json() 完成弹幕格式转换。

mod cli;

use std::fs;
use std::io::{self, IsTerminal, Write};
use std::path::Path;
use std::process;

use clap::{CommandFactory, FromArgMatches};
use danmakufactory_sys as ffi;
use serde_json::{json, Map, Value};

use cli::Args;

/// 解析后的输入文件条目（对应 C 库 df_fileinfo_t）
struct InputFile {
    /// 显式格式模板（xml/json/ass），缺省时由扩展名推断
    template: Option<String>,
    path: String,
    /// 该文件路径 token 在命令行中的全局索引，用于确定 -t 归属
    path_index: usize,
    /// 时间偏移（秒），由紧随其后的 -t 指定，缺省 0.0
    time_shift: f32,
}

/// 判断是否为格式名称（xml/json/ass，大小写不敏感）
fn is_format_token(token: &str) -> bool {
    matches!(token.to_ascii_lowercase().as_str(), "xml" | "json" | "ass")
}

/// 按命令行出现顺序解析输入文件（含格式模板），并记录每个文件路径的全局索引。
///
/// `tokens` 为 (全局索引, token) 序列，仅包含 -i 的值（模板与文件路径交错）。
fn parse_input_indexed(tokens: &[(usize, String)]) -> Vec<InputFile> {
    let mut files = Vec::new();
    let mut pending_template: Option<String> = None;

    for (pos, (gidx, value)) in tokens.iter().enumerate() {
        // 格式 token 后必须还有输入值才视为模板；否则当作文件路径处理
        if pending_template.is_none() && is_format_token(value) && pos + 1 < tokens.len() {
            pending_template = Some(value.to_ascii_lowercase());
            continue;
        }
        files.push(InputFile {
            template: pending_template.take(),
            path: value.clone(),
            path_index: *gidx,
            time_shift: 0.0,
        });
    }

    files
}

/// 解析 -o 参数：`[格式] 输出文件名`（旧版用法 `-o ass outfile`）
fn parse_output_arg(values: &[String]) -> (Option<String>, String) {
    if values.len() == 2 && is_format_token(&values[0]) {
        (Some(values[0].to_ascii_lowercase()), values[1].clone())
    } else {
        (None, values[values.len() - 1].clone())
    }
}

fn parse_input_matches(matches: &clap::ArgMatches, args: &Args) -> Vec<InputFile> {
    // 重建 -i 值的 (全局索引, token) 序列，保留模板与文件路径的交错顺序
    let input_indices: Vec<usize> = matches
        .indices_of("input")
        .map(|i| i.collect())
        .unwrap_or_default();
    let input_tokens: Vec<(usize, String)> = input_indices
        .into_iter()
        .zip(args.input.iter().cloned())
        .collect();

    // 解析输入文件（含模板），随后把每个 -t 归属于它之前最近出现的输入文件
    // 每次 -i 是独立的一组，格式 token 不得跨到下一个 -i。
    let mut input_files = Vec::new();
    let mut input_offset = 0;
    if let Some(groups) = matches.get_occurrences::<String>("input") {
        for group in groups {
            let count = group.len();
            input_files.extend(parse_input_indexed(&input_tokens[input_offset..input_offset + count]));
            input_offset += count;
        }
    }
    input_files
}

fn main() {
    // 启用 UTF-8 控制台，确保 emoji 等非 ASCII 文件名正确显示
    ffi::enable_console_utf8();

    // 需要 indices_of 重建 -i/-t 的命令行顺序，故先取 ArgMatches 再转回 derive 结构体
    let matches = Args::command().get_matches();
    let args = Args::from_arg_matches(&matches).unwrap_or_else(|e| e.exit());

    println!(
        "\nDanmakuFactory {} CLI Release by hkm (hkm@tikm.org)",
        env!("CARGO_PKG_VERSION")
    );
    println!("https://github.com/hihkm/DanmakuFactory");
    println!("Library version: {}", ffi::get_version());

    let mut input_files = parse_input_matches(&matches, &args);

    // 重建 -t 值的 (全局索引, 偏移) 序列
    let ts_indices: Vec<usize> = matches
        .indices_of("timeshift")
        .map(|i| i.collect())
        .unwrap_or_default();
    let ts_events: Vec<(usize, f32)> = ts_indices
        .into_iter()
        .zip(args.timeshift.iter().copied())
        .collect();

    for (ts_idx, ts_val) in ts_events {
        // 文件按 path_index 递增排列，从尾部向前找第一个出现在该 -t 之前的文件
        match input_files.iter_mut().rev().find(|f| f.path_index < ts_idx) {
            Some(f) => f.time_shift = ts_val,
            None => {
                eprintln!("\nERROR\n-t/--timeshift appears before any -i/--input file.");
                eprintln!("\nNOTE\n-t must follow the input file it applies to, e.g. `-i infile.xml -t 5.0`.");
                process::exit(1);
            }
        }
    }

    // 验证必要参数
    if input_files.is_empty() {
        eprintln!("\nERROR\nInput file must be specified.");
        eprintln!("\nNOTE\nUse -i or --input to specify.");
        process::exit(1);
    }
    if args.output.is_empty() {
        eprintln!("\nERROR\nOutput file must be specified.");
        eprintln!("\nNOTE\nUse -o or --output to specify.");
        process::exit(1);
    }

    if args.output.len() > 2 {
        fail("Specify one output file, optionally preceded by its format.");
    }
    if args.output.len() == 2 && !is_format_token(&args.output[0]) {
        fail("The first of two -o values must be xml, json, or ass.");
    }
    let (output_type, output) = parse_output_arg(&args.output);
    for value in args.timeshift.iter().copied().chain([
        args.display_area, args.scroll_area, args.scroll_duration, args.fixed_duration,
        args.outline, args.outline_blur, args.shadow, args.msgbox_duration, args.gift_min_price,
    ].into_iter().flatten()) {
        if !value.is_finite() {
            fail("Numeric options must be finite (NaN and infinity are invalid).");
        }
    }

    // 构建 runtime_info JSON
    let runtime_info = build_runtime_json(&args, &input_files, &output_type, &output);

    let json_str = serde_json::to_string(&runtime_info).expect("Failed to serialize JSON");

    // 打印文件信息
    println!("\nInput file(s):");
    for (i, f) in input_files.iter().enumerate() {
        let template = f.template.as_deref().unwrap_or("auto");
        println!(
            "  {}: {} (template: {}, timeshift: {:.3}s)",
            i + 1,
            f.path,
            template,
            f.time_shift
        );
    }
    let out_type = output_type.as_deref().unwrap_or("auto");
    println!("\nOutput file: {} (format: {})", output, out_type);

    confirm_overwrite(Path::new(&output), args.ignore_warnings);
    if args.save {
        let config_path = Path::new("DanmakuFactoryConfig.json");
        let output_path = Path::new(&output);
        let same_path = output_path == config_path || fs::canonicalize(output_path).ok()
            .zip(fs::canonicalize(config_path).ok()).is_some_and(|(a, b)| a == b);
        if same_path {
            fail("The output file and saved configuration must use different paths.");
        }
        confirm_overwrite(Path::new("DanmakuFactoryConfig.json"), args.ignore_warnings);
    }

    // 调用核心库
    println!("\nProcessing...");
    match ffi::run_from_json(&json_str) {
        Ok(()) => {
            if args.save {
                let config = runtime_info.get("config").cloned().unwrap_or_else(|| json!({}));
                let content = serde_json::to_string_pretty(&json!({ "config": config }))
                    .expect("Configuration is serializable");
                if let Err(e) = fs::write("DanmakuFactoryConfig.json", content) {
                    fail(&format!("Failed to save DanmakuFactoryConfig.json: {e}"));
                }
                println!("Configuration saved to DanmakuFactoryConfig.json");
            }
            println!("\nDone!");
        }
        Err(e) => {
            eprintln!("\nERROR (code: {:?})\n{}", e.code, e.message);
            process::exit(1);
        }
    }
}

/// 构建传递给 df_run_from_json 的完整 JSON
fn build_runtime_json(args: &Args, input_files: &[InputFile], output_type: &Option<String>, output: &str) -> Value {
    let mut root = Map::new();

    // 如果有配置文件，先加载为基础
    let config = if !args.config.is_empty() {
        load_config_files(&args.config)
    } else {
        json!({})
    };

    // 用 CLI 参数覆盖配置文件中的值
    let config = apply_cli_overrides(config, args);
    if !config.as_object().map_or(true, |o| o.is_empty()) {
        root.insert("config".to_string(), config);
    }

    // 输入文件（含每文件模板与时间偏移，对应 df_fileinfo_t）
    let inputs: Vec<Value> = input_files
        .iter()
        .map(|f| {
            let time_shift = f.time_shift;
            match (&f.template, time_shift != 0.0) {
                (None, false) => json!(normalize_path(&f.path)),
                (Some(t), false) => json!({ "path": normalize_path(&f.path), "type": t }),
                (None, true) => json!({ "path": normalize_path(&f.path), "time_shift": time_shift }),
                (Some(t), true) => {
                    json!({ "path": normalize_path(&f.path), "time_shift": time_shift, "type": t })
                }
            }
        })
        .collect();
    root.insert("input_files".to_string(), Value::Array(inputs));

    // 输出文件（显式格式时对应 df_fileinfo_t.type）
    let output_val = match output_type {
        Some(t) => json!({ "path": normalize_path(output), "type": t }),
        None => json!(normalize_path(output)),
    };
    root.insert("output_file".to_string(), output_val);

    Value::Object(root)
}

/// 加载并合并多个配置文件（后者覆盖前者）
fn load_config_files(paths: &[String]) -> Value {
    let mut merged = json!({});
    for path in paths {
        let content = fs::read_to_string(path)
            .unwrap_or_else(|e| fail(&format!("Failed to read config file \"{path}\": {e}")));
        let parsed: Value = serde_json::from_str(content.trim_start_matches('\u{feff}'))
            .unwrap_or_else(|e| fail(&format!("Failed to parse config file \"{path}\": {e}")));
        // 先分别解包，避免混用裸配置与 {"config": ...} 时丢失覆盖项。
        let parsed = normalize_config_keys(parsed);
        let config = parsed.get("config").unwrap_or(&parsed);
        validate_config_shape(config);
        deep_merge(&mut merged, &canonicalize_config(config.clone()));
    }
    merged
}

// 只规范配置键；路径、字体和屏蔽词的大小写必须保留。
fn normalize_config_keys(value: Value) -> Value {
    match value {
        Value::Object(obj) => Value::Object(obj.into_iter()
            .map(|(key, value)| (key.to_ascii_lowercase(), normalize_config_keys(value))).collect()),
        Value::Array(arr) => Value::Array(arr.into_iter().map(normalize_config_keys).collect()),
        value => value,
    }
}

// 每个文件独立转换为嵌套格式，使后加载的旧格式也能覆盖新格式。
fn canonicalize_config(mut config: Value) -> Value {
    let obj = config.as_object_mut().expect("Validated configuration object");
    let sections: &[(&str, &[&str])] = &[
        ("screen", &["resolution", "display_area", "scroll_area", "statistics_mode"]),
        ("danmaku", &["density", "scroll_duration", "fixed_duration", "show_username", "block_mode",
            "keep_blocked", "block_list", "block_list_regex_enabled"]),
        ("text", &["fontname", "font_size", "font_size_strict", "font_size_norm", "bold", "shadow",
            "opacity", "outline", "outline_blur", "outline_opacity", "line_spacing"]),
        ("msgbox", &["show", "size", "pos", "font_size", "duration", "gift_min_price"]),
    ];
    for (section, fields) in sections {
        for field in *fields {
            let key = if *section == "msgbox" { format!("msgbox_{field}") } else { field.to_string() };
            if let Some(value) = obj.remove(&key) {
                let target = obj.entry(section.to_string()).or_insert_with(|| json!({}));
                target.as_object_mut().expect("Validated section").entry(field.to_string()).or_insert(value);
            }
        }
    }
    for (alias, section, field) in [
        ("displayarea", "screen", "display_area"), ("scrollarea", "screen", "scroll_area"),
        ("statmode", "screen", "statistics_mode"), ("scrolltime", "danmaku", "scroll_duration"),
        ("fixtime", "danmaku", "fixed_duration"), ("blockmode", "danmaku", "block_mode"),
        ("saveblocked", "danmaku", "keep_blocked"), ("show_usernames", "danmaku", "show_username"),
        ("showusernames", "danmaku", "show_username"), ("blacklist", "danmaku", "block_list"),
        ("blacklist_regex", "danmaku", "block_list_regex_enabled"), ("fontsize", "text", "font_size"),
        ("linespacing", "text", "line_spacing"), ("show_msgbox", "msgbox", "show"),
        ("showmsgbox", "msgbox", "show"), ("msgboxsize", "msgbox", "size"),
        ("msgboxpos", "msgbox", "pos"), ("msgbox_fontsize", "msgbox", "font_size"),
        ("msgboxfontsize", "msgbox", "font_size"), ("msgboxduration", "msgbox", "duration"),
        ("duration", "msgbox", "duration"), ("gift_min_price", "msgbox", "gift_min_price"),
        ("giftminprice", "msgbox", "gift_min_price"),
    ] {
        if let Some(value) = obj.remove(alias) {
            let target = obj.entry(section).or_insert_with(|| json!({}));
            target.as_object_mut().expect("Validated section").entry(field).or_insert(value);
        }
    }
    config
}

fn fail(message: &str) -> ! {
    eprintln!("\nERROR\n{message}");
    process::exit(1);
}

fn validate_config_shape(config: &Value) {
    let obj = config.as_object().unwrap_or_else(|| fail("Configuration must be a JSON object."));
    for section in ["screen", "danmaku", "text", "msgbox"] {
        if let Some(value) = obj.get(section) {
            if !value.is_object() {
                fail(&format!("Configuration section \"{section}\" must be a JSON object."));
            }
        }
    }
}

fn confirm_overwrite(path: &Path, ignore_warnings: bool) {
    if !path.exists() || ignore_warnings {
        return;
    }
    if !io::stdin().is_terminal() {
        fail(&format!("File \"{}\" already exists; use --ignore-warnings to overwrite it.", path.display()));
    }
    eprint!("Overwrite \"{}\"? [y/N] ", path.display());
    io::stderr().flush().unwrap_or_else(|e| fail(&e.to_string()));
    let mut answer = String::new();
    io::stdin().read_line(&mut answer).unwrap_or_else(|e| fail(&e.to_string()));
    if !answer.trim().eq_ignore_ascii_case("y") {
        fail("Conversion cancelled.");
    }
}

/// 将 CLI 参数覆盖到 config JSON 上
fn apply_cli_overrides(mut config: Value, args: &Args) -> Value {
    validate_config_shape(&config);
    let obj = config.as_object_mut().expect("Validated configuration object");

    // === screen ===
    if args.resolution.is_some()
        || args.display_area.is_some()
        || args.scroll_area.is_some()
        || args.statistics_mode.is_some()
    {
        let screen = obj
            .entry("screen")
            .or_insert_with(|| json!({}));
        let s = screen.as_object_mut().unwrap();

        if let Some(ref res) = args.resolution {
            s.insert("resolution".into(), json!(res));
        }
        if let Some(v) = args.display_area {
            s.insert("display_area".into(), json!(v));
        }
        if let Some(v) = args.scroll_area {
            s.insert("scroll_area".into(), json!(v));
        }
        if let Some(ref v) = args.statistics_mode {
            s.insert("statistics_mode".into(), parse_mode_to_json(v));
        }
    }

    // === danmaku ===
    if args.density.is_some()
        || args.scroll_duration.is_some()
        || args.fixed_duration.is_some()
        || args.show_usernames.is_some()
        || args.block_mode.is_some()
        || args.save_blocked.is_some()
        || args.blacklist.is_some()
        || args.blacklist_regex.is_some()
    {
        let danmaku = obj
            .entry("danmaku")
            .or_insert_with(|| json!({}));
        let d = danmaku.as_object_mut().unwrap();

        if let Some(v) = args.density {
            d.insert("density".into(), json!(v));
        }
        if let Some(v) = args.scroll_duration {
            d.insert("scroll_duration".into(), json!(v));
        }
        if let Some(v) = args.fixed_duration {
            d.insert("fixed_duration".into(), json!(v));
        }
        if let Some(v) = args.show_usernames {
            d.insert("show_username".into(), json!(v));
        }
        if let Some(ref v) = args.block_mode {
            d.insert("block_mode".into(), parse_mode_to_json(v));
        }
        if let Some(v) = args.save_blocked {
            d.insert("keep_blocked".into(), json!(v));
        }
        if let Some(ref v) = args.blacklist {
            // 读取黑名单文件内容
            match fs::read_to_string(v) {
                Ok(content) => {
                    let lines: Vec<&str> = content
                        .lines()
                        .filter(|l| !l.trim().is_empty())
                        .collect();
                    d.insert("block_list".into(), json!(lines));
                }
                Err(e) => {
                    fail(&format!("Failed to read blacklist file \"{v}\": {e}"));
                }
            }
        }
        if let Some(v) = args.blacklist_regex {
            d.insert("block_list_regex_enabled".into(), json!(v));
        }
    }

    // === text ===
    if args.font_size.is_some()
        || args.font_size_strict
        || args.font_size_norm
        || args.fontname.is_some()
        || args.opacity.is_some()
        || args.outline.is_some()
        || args.outline_blur.is_some()
        || args.outline_opacity.is_some()
        || args.shadow.is_some()
        || args.bold.is_some()
        || args.line_spacing.is_some()
    {
        let text = obj.entry("text").or_insert_with(|| json!({}));
        let t = text.as_object_mut().unwrap();

        if let Some(v) = args.font_size {
            t.insert("font_size".into(), json!(v));
        }
        if args.font_size_strict {
            t.insert("font_size_strict".into(), json!(true));
        }
        if args.font_size_norm {
            t.insert("font_size_norm".into(), json!(true));
        }
        if let Some(ref v) = args.fontname {
            t.insert("fontname".into(), json!(v));
        }
        if let Some(v) = args.opacity {
            t.insert("opacity".into(), json!(v));
        }
        if let Some(v) = args.outline {
            t.insert("outline".into(), json!(v));
        }
        if let Some(v) = args.outline_blur {
            t.insert("outline_blur".into(), json!(v));
        }
        if let Some(v) = args.outline_opacity {
            t.insert("outline_opacity".into(), json!(v));
        }
        if let Some(v) = args.shadow {
            t.insert("shadow".into(), json!(v));
        }
        if let Some(v) = args.bold {
            t.insert("bold".into(), json!(v));
        }
        if let Some(v) = args.line_spacing {
            t.insert("line_spacing".into(), json!(v));
        }
    }

    // === msgbox ===
    if args.show_msgbox.is_some()
        || args.msgbox_size.is_some()
        || args.msgbox_pos.is_some()
        || args.msgbox_font_size.is_some()
        || args.msgbox_duration.is_some()
        || args.gift_min_price.is_some()
    {
        let msgbox = obj.entry("msgbox").or_insert_with(|| json!({}));
        let m = msgbox.as_object_mut().unwrap();

        if let Some(v) = args.show_msgbox {
            m.insert("show".into(), json!(v));
        }
        if let Some(ref v) = args.msgbox_size {
            m.insert("size".into(), json!(v));
        }
        if let Some(ref v) = args.msgbox_pos {
            m.insert("pos".into(), json!(v));
        }
        if let Some(v) = args.msgbox_font_size {
            m.insert("font_size".into(), json!(v));
        }
        if let Some(v) = args.msgbox_duration {
            m.insert("duration".into(), json!(v));
        }
        if let Some(v) = args.gift_min_price {
            m.insert("gift_min_price".into(), json!(v));
        }
    }

    config
}

/// 将 "L2R-TOP-BOTTOM" 格式的模式字符串转为 JSON 数组
fn parse_mode_to_json(mode_str: &str) -> Value {
    if mode_str.eq_ignore_ascii_case("null") || mode_str.eq_ignore_ascii_case("none") {
        return json!([]);
    }
    let flags: Vec<String> = mode_str
        .split('-')
        .filter(|s| !s.is_empty())
        .map(|s| s.to_uppercase())
        .collect();
    json!(flags)
}

/// serde_json 负责转义，保留 Unix 文件名中合法的反斜杠。
fn normalize_path(path: &str) -> String {
    path.to_owned()
}

/// 深度合并两个 JSON 对象（src 覆盖 dst）
fn deep_merge(dst: &mut Value, src: &Value) {
    match (dst, src) {
        (Value::Object(dst_map), Value::Object(src_map)) => {
            for (key, src_val) in src_map {
                let entry = dst_map.entry(key.clone()).or_insert(Value::Null);
                deep_merge(entry, src_val);
            }
        }
        (dst, src) => {
            *dst = src.clone();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn repeated_input_groups_preserve_format_named_paths() {
        let matches = Args::command().try_get_matches_from([
            "DanmakuFactory", "-i", "xml", "-i", "json", "comments.txt", "-o", "output.ass",
        ]).unwrap();
        let args = Args::from_arg_matches(&matches).unwrap();
        let files = parse_input_matches(&matches, &args);
        assert_eq!(files.len(), 2);
        assert_eq!(files[0].path, "xml");
        assert_eq!(files[0].template, None);
        assert_eq!(files[1].path, "comments.txt");
        assert_eq!(files[1].template.as_deref(), Some("json"));
    }

    #[test]
    fn nested_and_flat_configs_merge_in_file_order() {
        let mut config = canonicalize_config(json!({"text": {"font_size": 28, "fontname": "MixedCase Font"}}));
        let later = canonicalize_config(normalize_config_keys(json!({"FontSize": 42, "show_usernames": true})));
        deep_merge(&mut config, &later);
        assert_eq!(config["text"]["font_size"], 42);
        assert_eq!(config["text"]["fontname"], "MixedCase Font");
        assert_eq!(config["danmaku"]["show_username"], true);
    }

    #[test]
    fn documented_option_aliases_are_accepted() {
        let matches = Args::command().try_get_matches_from([
            "DanmakuFactory", "--block_mode", "TOP", "--statistics_mode", "TABLE",
        ]).unwrap();
        let args = Args::from_arg_matches(&matches).unwrap();
        assert_eq!(args.block_mode.as_deref(), Some("TOP"));
        assert_eq!(args.statistics_mode.as_deref(), Some("TABLE"));
    }

    #[test]
    fn unix_path_backslashes_are_preserved() {
        assert_eq!(normalize_path("comments\\part.xml"), "comments\\part.xml");
    }
}