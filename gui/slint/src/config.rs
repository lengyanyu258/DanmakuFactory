//! GUI 配置管理 — 与 C 库 runtime_info JSON 模式对齐
//!
//! 配置文件格式与 CLI 的 config.json 兼容：{ "config": { ... } }

use serde::{Deserialize, Serialize};
use serde_json::{json, Map, Value};

/// GUI 配置（默认值参考 VBGUI 版本）
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GuiConfig {
    // === 屏幕设置 ===
    pub resolution_x: i32,
    pub resolution_y: i32,
    pub display_area: f32,
    pub scroll_area: f32,
    /// 统计模式标志: HISTOGRAM / TABLE
    pub statistics: Vec<String>,

    // === 弹幕设置 ===
    pub scroll_duration: f32,
    pub fixed_duration: f32,
    /// -1 不重叠, 0 无限制, >0 自定义数量
    pub density: i32,
    /// 屏蔽模式标志: L2R / R2L / TOP / BOTTOM / SPECIAL / COLOR / REPEAT
    pub block_mode: Vec<String>,
    pub show_username: bool,
    /// 是否保留被屏蔽的弹幕
    pub keep_blocked: bool,
    /// 黑名单文件路径（每行一条规则）
    pub blacklist_path: String,
    /// 黑名单是否启用正则匹配
    pub blacklist_regex: bool,
    /// Rules embedded in an imported CLI configuration.
    pub block_list: Vec<String>,

    // === 文本设置 ===
    pub font_size: i32,
    pub fontname: String,
    pub opacity: i32,
    pub outline: f32,
    pub shadow: f32,
    pub bold: bool,
    /// 行间距（像素）
    pub line_spacing: i32,
    /// 描边模糊半径
    pub outline_blur: f32,
    /// 描边不透明度 (0-255)
    pub outline_opacity: i32,
    /// 严格保持指定字号
    pub font_size_strict: bool,
    /// 修正字号（归一化）
    pub font_size_norm: bool,

    // === 消息框设置 ===
    pub show_msgbox: bool,
    pub msgbox_width: i32,
    pub msgbox_height: i32,
    pub msgbox_pos_x: i32,
    pub msgbox_pos_y: i32,
    pub msgbox_font_size: i32,
    pub msgbox_duration: f32,
    pub gift_min_price: f32,
}

impl Default for GuiConfig {
    fn default() -> Self {
        Self {
            resolution_x: 1920,
            resolution_y: 1080,
            display_area: 1.0,
            scroll_area: 1.0,
            statistics: Vec::new(),
            scroll_duration: 12.0,
            fixed_duration: 5.0,
            density: 0,
            block_mode: Vec::new(),
            show_username: false,
            keep_blocked: true,
            blacklist_path: String::new(),
            blacklist_regex: false,
            block_list: Vec::new(),
            font_size: 38,
            fontname: "Microsoft YaHei".to_string(),
            opacity: 180,
            outline: 0.0,
            shadow: 1.0,
            bold: false,
            line_spacing: 0,
            outline_blur: 0.0,
            outline_opacity: 255,
            font_size_strict: false,
            font_size_norm: false,
            show_msgbox: true,
            msgbox_width: 500,
            msgbox_height: 1080,
            msgbox_pos_x: 20,
            msgbox_pos_y: 0,
            msgbox_font_size: 38,
            msgbox_duration: 0.0,
            gift_min_price: 0.0,
        }
    }
}

impl GuiConfig {
    /// 构建传递给 df_run_from_json 的 runtime_info JSON
    pub fn build_runtime_json(&self, input_file: &str, output_file: &str, time_shift: f32) -> Result<Value, String> {
        let mut root = Map::new();

        // 输入文件
        let input_entry = if time_shift != 0.0 {
            json!({ "path": normalize_path(input_file), "time_shift": time_shift })
        } else {
            json!(normalize_path(input_file))
        };
        root.insert("input_files".to_string(), json!([input_entry]));

        // 输出文件
        root.insert("output_file".to_string(), json!(normalize_path(output_file)));

        // 配置
        root.insert("config".to_string(), self.build_config_json()?);

        Ok(Value::Object(root))
    }

    /// 构建 config 部分 JSON（与 CLI --config 文件格式兼容）
    fn build_config_json(&self) -> Result<Value, String> {
        let mut config = Map::new();

        // === screen ===
        let mut screen = Map::new();
        screen.insert(
            "resolution".into(),
            json!([self.resolution_x, self.resolution_y]),
        );
        screen.insert("display_area".into(), json!(self.display_area));
        screen.insert("scroll_area".into(), json!(self.scroll_area));
        if !self.statistics.is_empty() {
            screen.insert("statistics_mode".into(), json!(self.statistics));
        }
        config.insert("screen".into(), Value::Object(screen));

        // === danmaku ===
        let mut danmaku = Map::new();
        danmaku.insert("scroll_duration".into(), json!(self.scroll_duration));
        danmaku.insert("fixed_duration".into(), json!(self.fixed_duration));
        danmaku.insert("density".into(), json!(self.density));
        if !self.block_mode.is_empty() {
            danmaku.insert("block_mode".into(), json!(self.block_mode));
        }
        danmaku.insert("show_username".into(), json!(self.show_username));
        danmaku.insert("keep_blocked".into(), json!(self.keep_blocked));
        // A selected file overrides embedded rules; unreadable files must not
        // silently disable filtering and report a successful conversion.
        let rules = if self.blacklist_path.is_empty() {
            self.block_list.clone()
        } else {
            let content = std::fs::read_to_string(&self.blacklist_path)
                .map_err(|error| format!("读取黑名单失败 ({}): {error}", self.blacklist_path))?;
            content.lines().filter(|line| !line.trim().is_empty())
                .map(String::from).collect()
        };
        danmaku.insert("block_list".into(), json!(rules));
        danmaku.insert("block_list_regex_enabled".into(), json!(self.blacklist_regex));
        config.insert("danmaku".into(), Value::Object(danmaku));

        // === text ===
        let mut text = Map::new();
        text.insert("font_size".into(), json!(self.font_size));
        text.insert("fontname".into(), json!(self.fontname));
        text.insert("opacity".into(), json!(self.opacity));
        text.insert("outline".into(), json!(self.outline));
        text.insert("shadow".into(), json!(self.shadow));
        text.insert("bold".into(), json!(self.bold));
        text.insert("line_spacing".into(), json!(self.line_spacing));
        text.insert("outline_blur".into(), json!(self.outline_blur));
        text.insert("outline_opacity".into(), json!(self.outline_opacity));
        text.insert("font_size_strict".into(), json!(self.font_size_strict));
        text.insert("font_size_norm".into(), json!(self.font_size_norm));
        config.insert("text".into(), Value::Object(text));

        // === msgbox ===
        let mut msgbox = Map::new();
        msgbox.insert("show".into(), json!(self.show_msgbox));
        msgbox.insert(
            "size".into(),
            json!([self.msgbox_width, self.msgbox_height]),
        );
        msgbox.insert(
            "pos".into(),
            json!([self.msgbox_pos_x, self.msgbox_pos_y]),
        );
        msgbox.insert("font_size".into(), json!(self.msgbox_font_size));
        msgbox.insert("duration".into(), json!(self.msgbox_duration));
        msgbox.insert("gift_min_price".into(), json!(self.gift_min_price));
        config.insert("msgbox".into(), Value::Object(msgbox));

        Ok(Value::Object(config))
    }

    /// 保存配置到文件（与 CLI config.json 格式兼容）
    pub fn save_to_file(&self, path: &std::path::Path) -> std::io::Result<()> {
        let mut config = self.build_config_json()
            .map_err(|message| std::io::Error::new(std::io::ErrorKind::InvalidInput, message))?;
        // 额外保存黑名单路径供 GUI 往返读取（C 库只消费 block_list，会忽略此键）
        if !self.blacklist_path.is_empty() {
            if let Some(danmaku) = config.get_mut("danmaku").and_then(|v| v.as_object_mut()) {
                danmaku.insert("blacklist_path".into(), json!(self.blacklist_path));
            }
        }
        let wrapper = json!({ "config": config });
        let content = serde_json::to_string_pretty(&wrapper)
            .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?;
        std::fs::write(path, content)
    }

    /// 从文件加载配置（兼容 CLI config.json 格式）
    pub fn load_from_file(path: &std::path::Path) -> Option<Self> {
        let content = std::fs::read_to_string(path).ok()?;
        let root: Value = serde_json::from_str(&content).ok()?;
        let config = root.get("config").unwrap_or(&root);
        config.as_object()?;

        let mut cfg = GuiConfig::default();

        // screen
        if let Some(screen) = config.get("screen") {
            if let Some((x, y)) = screen.get("resolution").and_then(parse_coord) {
                cfg.resolution_x = x;
                cfg.resolution_y = y;
            }
            if let Some(v) = screen.get("display_area").and_then(|v| v.as_f64()) {
                cfg.display_area = v as f32;
            }
            if let Some(v) = screen.get("scroll_area").and_then(|v| v.as_f64()) {
                cfg.scroll_area = v as f32;
            }
            if let Some(value) = screen.get("statistics_mode") {
                cfg.statistics = parse_modes(value, &["TABLE", "HISTOGRAM"]);
            }
        }

        // danmaku
        if let Some(d) = config.get("danmaku") {
            if let Some(v) = d.get("scroll_duration").and_then(|v| v.as_f64()) {
                cfg.scroll_duration = v as f32;
            }
            if let Some(v) = d.get("fixed_duration").and_then(|v| v.as_f64()) {
                cfg.fixed_duration = v as f32;
            }
            if let Some(v) = d.get("density").and_then(|v| v.as_i64()) {
                cfg.density = v as i32;
            }
            if let Some(value) = d.get("block_mode") {
                cfg.block_mode = parse_modes(value, &["R2L", "L2R", "TOP", "BOTTOM", "SPECIAL", "COLOR", "REPEAT"]);
            }
            if let Some(rules) = d.get("block_list").and_then(Value::as_array) {
                cfg.block_list = rules.iter().filter_map(Value::as_str).map(String::from).collect();
            } else if let Some(path) = d.get("block_list").and_then(Value::as_str) {
                cfg.blacklist_path = path.to_string();
            }
            if let Some(v) = d.get("show_username").and_then(|v| v.as_bool()) {
                cfg.show_username = v;
            }
            if let Some(v) = d.get("keep_blocked").and_then(|v| v.as_bool()) {
                cfg.keep_blocked = v;
            }
            if let Some(v) = d.get("blacklist_path").and_then(|v| v.as_str()) {
                cfg.blacklist_path = v.to_string();
            }
            if let Some(v) = d.get("block_list_regex_enabled").and_then(|v| v.as_bool()) {
                cfg.blacklist_regex = v;
            }
        }

        // text
        if let Some(t) = config.get("text") {
            if let Some(v) = t.get("font_size").and_then(|v| v.as_i64()) {
                cfg.font_size = v as i32;
            }
            if let Some(v) = t.get("fontname").and_then(|v| v.as_str()) {
                cfg.fontname = v.to_string();
            }
            if let Some(v) = t.get("opacity").and_then(|v| v.as_i64()) {
                cfg.opacity = v as i32;
            }
            if let Some(v) = t.get("outline").and_then(|v| v.as_f64()) {
                cfg.outline = v as f32;
            }
            if let Some(v) = t.get("shadow").and_then(|v| v.as_f64()) {
                cfg.shadow = v as f32;
            }
            if let Some(v) = t.get("bold").and_then(|v| v.as_bool()) {
                cfg.bold = v;
            }
            if let Some(v) = t.get("line_spacing").and_then(|v| v.as_i64()) {
                cfg.line_spacing = v as i32;
            }
            if let Some(v) = t.get("outline_blur").and_then(|v| v.as_f64()) {
                cfg.outline_blur = v as f32;
            }
            if let Some(v) = t.get("outline_opacity").and_then(|v| v.as_i64()) {
                cfg.outline_opacity = v as i32;
            }
            if let Some(v) = t.get("font_size_strict").and_then(|v| v.as_bool()) {
                cfg.font_size_strict = v;
            }
            if let Some(v) = t.get("font_size_norm").and_then(|v| v.as_bool()) {
                cfg.font_size_norm = v;
            }
        }

        // msgbox
        if let Some(m) = config.get("msgbox") {
            if let Some(v) = m.get("show").and_then(|v| v.as_bool()) {
                cfg.show_msgbox = v;
            }
            if let Some((w, h)) = m.get("size").and_then(parse_coord) {
                cfg.msgbox_width = w;
                cfg.msgbox_height = h;
            }
            if let Some((x, y)) = m.get("pos").and_then(parse_coord) {
                cfg.msgbox_pos_x = x;
                cfg.msgbox_pos_y = y;
            }
            if let Some(v) = m.get("font_size").and_then(|v| v.as_i64()) {
                cfg.msgbox_font_size = v as i32;
            }
            if let Some(v) = m.get("duration").and_then(|v| v.as_f64()) {
                cfg.msgbox_duration = v as f32;
            }
            if let Some(v) = m.get("gift_min_price").and_then(|v| v.as_f64()) {
                cfg.gift_min_price = v as f32;
            }
        }

        Some(cfg)
    }
}

fn parse_modes(value: &Value, names: &[&str]) -> Vec<String> {
    if let Some(mask) = value.as_u64() {
        return names.iter().enumerate().filter(|(index, _)| mask & (1u64 << *index) != 0)
            .map(|(_, name)| (*name).to_string()).collect();
    }
    let values: Vec<&str> = if let Some(name) = value.as_str() {
        vec![name]
    } else {
        value.as_array().map(|items| items.iter().filter_map(Value::as_str).collect())
            .unwrap_or_default()
    };
    values.into_iter().map(|name| {
        let name = name.to_ascii_uppercase();
        if name == "COLOUR" { "COLOR".to_string() } else { name }
    }).collect()
}

fn parse_coord(value: &Value) -> Option<(i32, i32)> {
    if let Some(text) = value.as_str() {
        return parse_wxh(text);
    }
    let coordinates = value.as_array()?;
    if coordinates.len() != 2 { return None; }
    Some((i32::try_from(coordinates[0].as_i64()?).ok()?,
          i32::try_from(coordinates[1].as_i64()?).ok()?))
}

/// 解析 "WxH" 格式字符串
fn parse_wxh(s: &str) -> Option<(i32, i32)> {
    let parts: Vec<&str> = s.split('x').collect();
    if parts.len() == 2 {
        let a = parts[0].trim().parse().ok()?;
        let b = parts[1].trim().parse().ok()?;
        Some((a, b))
    } else {
        None
    }
}

/// 路径规范化：将反斜杠统一为正斜杠（JSON 中正斜杠更安全）
fn normalize_path(path: &str) -> String {
    path.replace('\\', "/")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn imported_cli_config_keeps_coordinates_filters_and_decimals() {
        let path = std::env::temp_dir().join(format!("df_gui_config_{}.json",
            std::process::id()));
        std::fs::write(&path, r#"{
            "config": {
                "screen": {"resolution": [1280, 720], "statistics_mode": ["table"]},
                "danmaku": {"block_mode": 3, "block_list": ["广告", "spam"],
                           "block_list_regex_enabled": true, "scroll_duration": 7.25},
                "text": {"outline": 0.5},
                "msgbox": {"size": [320, 600], "pos": [4, 8], "gift_min_price": 1.25}
            }
        }"#).unwrap();
        let config = GuiConfig::load_from_file(&path).unwrap();
        std::fs::remove_file(&path).unwrap();
        assert_eq!((config.resolution_x, config.resolution_y), (1280, 720));
        assert_eq!((config.msgbox_width, config.msgbox_height), (320, 600));
        assert_eq!(config.statistics, vec!["TABLE"]);
        assert_eq!(config.block_mode, vec!["R2L", "L2R"]);
        assert_eq!(config.scroll_duration, 7.25);
        assert_eq!(config.outline, 0.5);
        let runtime = config.build_runtime_json("input.xml", "output.ass", 0.0).unwrap();
        assert_eq!(runtime["config"]["danmaku"]["block_list"], json!(["广告", "spam"]));
        assert_eq!(runtime["config"]["danmaku"]["block_list_regex_enabled"], true);
        assert_eq!(runtime["config"]["msgbox"]["gift_min_price"], 1.25);
    }

    #[test]
    fn unreadable_blacklist_is_reported_instead_of_disabled() {
        let mut config = GuiConfig::default();
        config.blacklist_path = std::env::temp_dir()
            .join(format!("df_gui_missing_{}_blacklist.txt", std::process::id()))
            .to_string_lossy().into_owned();
        assert!(config.build_runtime_json("input.xml", "output.ass", 0.0).is_err());
    }
}
