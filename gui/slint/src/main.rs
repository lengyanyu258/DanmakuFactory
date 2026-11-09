//! DanmakuFactory GUI — Rust + Slint 实现
//!
//! 通过 FFI 直接调用 C 核心库的 df_run_from_json() 完成弹幕格式转换，
//! 不经过 CLI 中间层。

mod config;

use std::collections::HashSet;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use danmakufactory_sys as ffi;
use slint::{Model, ModelRc, VecModel};

use config::GuiConfig;

slint::include_modules!();

fn main() {
    // 启用 UTF-8 控制台，确保 emoji 等非 ASCII 文件名日志正确显示
    ffi::enable_console_utf8();

    let app = MainWindow::new().expect("Failed to create Slint window");

    // 显示核心库版本
    app.set_lib_version(ffi::get_version().into());

    // 启动时自动加载配置（若存在）
    let config_path = default_config_path();
    if config_path.exists() {
        if let Some(cfg) = GuiConfig::load_from_file(&config_path) {
            apply_config_to_ui(&app, &cfg);
        }
    }

    // ==================== 文件管理回调 ====================

    app.on_add_files({
        let app_weak = app.as_weak();
        move || {
            let Some(app) = app_weak.upgrade() else { return };
            let files = rfd::FileDialog::new()
                .add_filter("弹幕文件", &["ass", "xml", "json"])
                .add_filter("所有文件", &["*"])
                .pick_files();
            if let Some(paths) = files {
                let mut items: Vec<FileEntry> = app
                    .get_files_model()
                    .iter()
                    .collect();
                for p in paths {
                    items.push(FileEntry {
                        path: p.to_string_lossy().into_owned().into(),
                        offset: "0".into(),
                    });
                }
                app.set_files_model(ModelRc::new(Rc::new(VecModel::from(items))));
            }
        }
    });

    app.on_remove_file({
        let app_weak = app.as_weak();
        move |idx| {
            let Some(app) = app_weak.upgrade() else { return };
            let mut items: Vec<FileEntry> = app.get_files_model().iter().collect();
            if (idx as usize) < items.len() {
                items.remove(idx as usize);
                app.set_files_model(ModelRc::new(Rc::new(VecModel::from(items))));
            }
        }
    });

    app.on_set_offset({
        let app_weak = app.as_weak();
        move |idx, new_offset| {
            let Some(app) = app_weak.upgrade() else { return };
            let mut items: Vec<FileEntry> = app.get_files_model().iter().collect();
            if let Some(item) = items.get_mut(idx as usize) {
                item.offset = new_offset;
                app.set_files_model(ModelRc::new(Rc::new(VecModel::from(items))));
            }
        }
    });

    app.on_clear_files({
        let app_weak = app.as_weak();
        move || {
            let Some(app) = app_weak.upgrade() else { return };
            app.set_files_model(ModelRc::new(Rc::new(VecModel::<FileEntry>::default())));
        }
    });

    app.on_browse_output_dir({
        let app_weak = app.as_weak();
        move || {
            let Some(app) = app_weak.upgrade() else { return };
            if let Some(dir) = rfd::FileDialog::new().pick_folder() {
                app.set_output_dir(dir.to_string_lossy().into_owned().into());
            }
        }
    });

    app.on_browse_blacklist({
        let app_weak = app.as_weak();
        move || {
            let Some(app) = app_weak.upgrade() else { return };
            let file = rfd::FileDialog::new()
                .add_filter("黑名单文件", &["txt"])
                .add_filter("所有文件", &["*"])
                .pick_file();
            if let Some(f) = file {
                app.set_blacklist_path(f.to_string_lossy().into_owned().into());
            }
        }
    });

    // ==================== 配置持久化回调 ====================

    app.on_save_config({
        let app_weak = app.as_weak();
        move || {
            let Some(app) = app_weak.upgrade() else { return };
            let cfg = match read_config_from_ui(&app) {
                Ok(cfg) => cfg,
                Err(message) => {
                    app.set_status_text(message.clone().into());
                    let log = app.get_log_text().to_string();
                    app.set_log_text(format!("{log}{message}\n").into());
                    return;
                }
            };
            let path = default_config_path();
            match cfg.save_to_file(&path) {
                Ok(()) => {
                    let log = app.get_log_text().to_string();
                    app.set_log_text(
                        format!("{log}配置已保存到 {}\n", path.display()).into(),
                    );
                }
                Err(e) => {
                    let log = app.get_log_text().to_string();
                    app.set_log_text(format!("{log}保存配置失败: {e}\n").into());
                }
            }
        }
    });

    app.on_load_config({
        let app_weak = app.as_weak();
        move || {
            let Some(app) = app_weak.upgrade() else { return };

            // 优先尝试默认路径，否则弹出文件选择
            let path = {
                let default = default_config_path();
                if default.exists() {
                    default
                } else {
                    match rfd::FileDialog::new()
                        .add_filter("JSON 配置", &["json"])
                        .pick_file()
                    {
                        Some(p) => p,
                        None => return,
                    }
                }
            };

            match GuiConfig::load_from_file(&path) {
                Some(cfg) => {
                    apply_config_to_ui(&app, &cfg);
                    let log = app.get_log_text().to_string();
                    app.set_log_text(
                        format!("{log}已加载配置: {}\n", path.display()).into(),
                    );
                }
                None => {
                    let log = app.get_log_text().to_string();
                    app.set_log_text(
                        format!("{log}加载配置失败: {}\n", path.display()).into(),
                    );
                }
            }
        }
    });

    // ==================== 转换回调 ====================

    app.on_convert({
        let app_weak = app.as_weak();
        move || {
            let Some(app) = app_weak.upgrade() else { return };

            // 验证
            let file_count = app.get_files_model().row_count();
            if file_count == 0 {
                app.set_status_text("请先添加输入文件".into());
                return;
            }

            // 收集文件列表（含每文件时轴偏移）
            let files: Vec<FileEntry> = app
                .get_files_model()
                .iter()
                .collect();

            let time_shifts: Vec<f32> = match files.iter().map(|entry| {
                entry.offset.as_str().trim().parse::<f32>()
                    .ok().filter(|value| value.is_finite())
                    .ok_or_else(|| format!("时轴偏移必须是有限数值: {}", entry.path))
            }).collect::<Result<Vec<_>, _>>() {
                Ok(shifts) => shifts,
                Err(message) => {
                    app.set_status_text(message.clone().into());
                    app.set_log_text(format!("{message}\n").into());
                    return;
                }
            };

            // 收集配置
            let cfg = match read_config_from_ui(&app) {
                Ok(cfg) => cfg,
                Err(message) => {
                    app.set_status_text(message.clone().into());
                    let log = app.get_log_text().to_string();
                    app.set_log_text(format!("{log}{message}\n").into());
                    return;
                }
            };
            let output_dir = app.get_output_dir().to_string();
            let format_index = app.get_output_format_index();

            // 重置状态
            app.set_running(true);
            app.set_progress(0);
            app.set_progress_max(file_count as i32);
            app.set_status_text("正在转换...".into());
            app.set_log_text("".into());

            // 后台线程执行转换
            let stop_flag = Arc::new(AtomicBool::new(false));
            let app_weak_thread = app.as_weak();

            std::thread::spawn(move || {
                let mut done = 0i32;
                let mut failed = 0i32;

                let total = files.len();
                let mut reserved_outputs = HashSet::new();
                let output_files: Vec<String> = files.iter().map(|entry| {
                    make_output_path(entry.path.as_str(), &output_dir, format_index, &mut reserved_outputs)
                }).collect();
                for (i, entry) in files.iter().enumerate() {
                    if stop_flag.load(Ordering::Relaxed) {
                        update_ui(&app_weak_thread, move |app| {
                            let log = app.get_log_text().to_string();
                            app.set_log_text(format!("{log}用户终止了转换\n").into());
                        });
                        break;
                    }

                    let input_file = entry.path.to_string();
                    let time_shift = time_shifts[i];
                    let output_file = output_files[i].clone();

                    // 状态更新（闭包需 'static，故克隆所需数据）
                    let file_for_log = input_file.clone();
                    update_ui(&app_weak_thread, move |app| {
                        app.set_status_text(
                            format!("正在转换 ({}/{})...", i + 1, total).into(),
                        );
                        let log = app.get_log_text().to_string();
                        app.set_log_text(
                            format!("{log}[{}/{}] 正在转换: {file_for_log}\n", i + 1, total).into(),
                        );
                    });

                    // 构建 runtime JSON 并调用核心库
                    let json = match cfg.build_runtime_json(&input_file, &output_file, time_shift) {
                        Ok(json) => json,
                        Err(message) => {
                            failed += 1;
                            update_ui(&app_weak_thread, move |app| {
                                let log = app.get_log_text().to_string();
                                app.set_log_text(format!("{log}  ✗ 失败: {message}\n").into());
                                app.set_progress((i + 1) as i32);
                            });
                            continue;
                        }
                    };
                    let json_str = serde_json::to_string(&json).expect("JSON serialization");

                    match ffi::run_from_json(&json_str) {
                        Ok(()) => {
                            done += 1;
                            let out_for_log = output_file.clone();
                            update_ui(&app_weak_thread, move |app| {
                                let log = app.get_log_text().to_string();
                                app.set_log_text(
                                    format!("{log}  ✓ 成功 → {out_for_log}\n").into(),
                                );
                            });
                        }
                        Err(e) => {
                            failed += 1;
                            let msg = e.message;
                            update_ui(&app_weak_thread, move |app| {
                                let log = app.get_log_text().to_string();
                                app.set_log_text(
                                    format!("{log}  ✗ 失败: {msg}\n").into(),
                                );
                            });
                        }
                    }

                    let prog = (i + 1) as i32;
                    update_ui(&app_weak_thread, move |app| {
                        app.set_progress(prog);
                    });
                }

                // 完成
                update_ui(&app_weak_thread, move |app| {
                    app.set_running(false);
                    let status = if failed == 0 {
                        format!("转换完成！成功 {done} 个文件")
                    } else {
                        format!("转换完成：成功 {done}，失败 {failed}")
                    };
                    app.set_status_text(status.into());
                    let log = app.get_log_text().to_string();
                    app.set_log_text(
                        format!("{log}\n===== 转换结束：成功 {done}，失败 {failed} =====\n").into(),
                    );
                });
            });
        }
    });

    app.run().expect("Failed to run Slint event loop");
}

// ============================================================================
// 辅助函数
// ============================================================================

/// 在 Slint 事件循环中安全更新 UI
fn update_ui(app_weak: &slint::Weak<MainWindow>, f: impl FnOnce(&MainWindow) + Send + 'static) {
    let app_weak = app_weak.clone();
    let _ = slint::invoke_from_event_loop(move || {
        if let Some(app) = app_weak.upgrade() {
            f(&app);
        }
    });
}

/// 根据输入文件路径和输出设置生成输出文件路径
fn make_output_path(input: &str, output_dir: &str, format_index: i32,
                    reserved: &mut HashSet<String>) -> String {
    let ext = match format_index {
        0 => "ass",
        1 => "xml",
        2 => "json",
        _ => "ass",
    };

    let input_path = Path::new(input);
    let stem = input_path
        .file_stem()
        .map(|s| s.to_string_lossy().into_owned())
        .unwrap_or_else(|| "output".to_string());

    let dir = if output_dir.is_empty() {
        input_path.parent().unwrap_or(Path::new("."))
    } else {
        Path::new(output_dir)
    };
    // Avoid replacing inputs, existing outputs or another file in this batch.
    let mut suffix = 0usize;
    loop {
        let filename = if suffix == 0 {
            format!("{stem}.{ext}")
        } else {
            format!("{stem} ({suffix}).{ext}")
        };
        let candidate = dir.join(filename);
        let output = candidate.to_string_lossy().into_owned();
        let key = if cfg!(target_os = "windows") { output.to_lowercase() } else { output.clone() };
        if !candidate.exists() && reserved.insert(key) {
            return output;
        }
        suffix += 1;
    }
}

/// 默认配置文件路径（与可执行文件同目录）
fn default_config_path() -> PathBuf {
    std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|d| d.to_path_buf()))
        .unwrap_or_else(|| PathBuf::from("."))
        .join("DanmakuFactoryGUIConfig.json")
}

/// 从 UI 属性读取当前配置
fn read_config_from_ui(app: &MainWindow) -> Result<GuiConfig, String> {
    // 密度: 0=无限制, 1=不重叠, 2=自定义
    let density = match app.get_density_mode() {
        1 => -1,
        2 => app.get_density_value(),
        _ => 0,
    };

    // 屏蔽模式
    let mut block_mode = Vec::new();
    if app.get_block_l2r() {
        block_mode.push("L2R".to_string());
    }
    if app.get_block_r2l() {
        block_mode.push("R2L".to_string());
    }
    if app.get_block_top() {
        block_mode.push("TOP".to_string());
    }
    if app.get_block_bottom() {
        block_mode.push("BOTTOM".to_string());
    }
    if app.get_block_special() {
        block_mode.push("SPECIAL".to_string());
    }
    if app.get_block_color() {
        block_mode.push("COLOR".to_string());
    }
    if app.get_block_repeat() {
        block_mode.push("REPEAT".to_string());
    }

    // 统计模式
    let mut statistics = Vec::new();
    if app.get_stat_histogram() {
        statistics.push("HISTOGRAM".to_string());
    }
    if app.get_stat_table() {
        statistics.push("TABLE".to_string());
    }

    Ok(GuiConfig {
        resolution_x: app.get_screen_res_x(),
        resolution_y: app.get_screen_res_y(),
        display_area: parse_ui_float(app.get_screen_display_area().as_str(), "显示区域")? / 100.0,
        scroll_area: parse_ui_float(app.get_screen_scroll_area().as_str(), "滚动区域")? / 100.0,
        statistics,
        scroll_duration: parse_ui_float(app.get_danmaku_scroll_duration().as_str(), "滚动弹幕时长")?,
        fixed_duration: parse_ui_float(app.get_danmaku_fixed_duration().as_str(), "固定弹幕时长")?,
        density,
        block_mode,
        show_username: app.get_danmaku_show_username(),
        keep_blocked: app.get_danmaku_keep_blocked(),
        blacklist_path: app.get_blacklist_path().to_string(),
        blacklist_regex: app.get_blacklist_regex(),
        block_list: app.get_blacklist_rules().iter().map(|rule| rule.to_string()).collect(),
        font_size: app.get_text_font_size(),
        fontname: app.get_text_fontname().to_string(),
        opacity: app.get_text_opacity(),
        outline: parse_ui_float(app.get_text_outline().as_str(), "描边")?,
        shadow: parse_ui_float(app.get_text_shadow().as_str(), "阴影")?,
        bold: app.get_text_bold(),
        line_spacing: app.get_text_line_spacing(),
        outline_blur: parse_ui_float(app.get_text_outline_blur().as_str(), "描边模糊")?,
        outline_opacity: app.get_text_outline_opacity(),
        font_size_strict: app.get_text_font_size_strict(),
        font_size_norm: app.get_text_font_size_norm(),
        show_msgbox: app.get_msgbox_show(),
        msgbox_width: app.get_msgbox_width(),
        msgbox_height: app.get_msgbox_height(),
        msgbox_pos_x: app.get_msgbox_pos_x(),
        msgbox_pos_y: app.get_msgbox_pos_y(),
        msgbox_font_size: app.get_msgbox_font_size(),
        msgbox_duration: parse_ui_float(app.get_msgbox_duration().as_str(), "消息框时长")?,
        gift_min_price: parse_ui_float(app.get_msgbox_gift_min_price().as_str(), "礼物最低价格")?,
    })
}

fn parse_ui_float(value: &str, label: &str) -> Result<f32, String> {
    value.trim().parse::<f32>().ok().filter(|value| value.is_finite())
        .ok_or_else(|| format!("{label}必须是有限数值"))
}

/// 将配置应用到 UI 属性
fn apply_config_to_ui(app: &MainWindow, cfg: &GuiConfig) {
    app.set_screen_res_x(cfg.resolution_x);
    app.set_screen_res_y(cfg.resolution_y);
    app.set_screen_display_area((cfg.display_area * 100.0).to_string().into());
    app.set_screen_scroll_area((cfg.scroll_area * 100.0).to_string().into());

    app.set_danmaku_scroll_duration(cfg.scroll_duration.to_string().into());
    app.set_danmaku_fixed_duration(cfg.fixed_duration.to_string().into());

    // 密度: 0=无限制, 1=不重叠, 2=自定义
    match cfg.density {
        0 => app.set_density_mode(0),
        -1 => app.set_density_mode(1),
        n => {
            app.set_density_mode(2);
            app.set_density_value(n);
        }
    }

    // 屏蔽模式
    app.set_block_l2r(cfg.block_mode.iter().any(|m| m == "L2R"));
    app.set_block_r2l(cfg.block_mode.iter().any(|m| m == "R2L"));
    app.set_block_top(cfg.block_mode.iter().any(|m| m == "TOP"));
    app.set_block_bottom(cfg.block_mode.iter().any(|m| m == "BOTTOM"));
    app.set_block_special(cfg.block_mode.iter().any(|m| m == "SPECIAL"));
    app.set_block_color(cfg.block_mode.iter().any(|m| m == "COLOR"));
    app.set_block_repeat(cfg.block_mode.iter().any(|m| m == "REPEAT"));

    app.set_danmaku_show_username(cfg.show_username);
    app.set_danmaku_keep_blocked(cfg.keep_blocked);
    app.set_blacklist_path(cfg.blacklist_path.as_str().into());
    app.set_blacklist_regex(cfg.blacklist_regex);
    let rules: Vec<slint::SharedString> = cfg.block_list.iter().map(|rule| rule.as_str().into()).collect();
    app.set_blacklist_rules(ModelRc::new(Rc::new(VecModel::from(rules))));

    app.set_text_font_size(cfg.font_size);
    app.set_text_fontname(cfg.fontname.as_str().into());
    app.set_text_opacity(cfg.opacity);
    app.set_text_outline(cfg.outline.to_string().into());
    app.set_text_shadow(cfg.shadow.to_string().into());
    app.set_text_bold(cfg.bold);
    app.set_text_line_spacing(cfg.line_spacing);
    app.set_text_outline_blur(cfg.outline_blur.to_string().into());
    app.set_text_outline_opacity(cfg.outline_opacity);
    app.set_text_font_size_strict(cfg.font_size_strict);
    app.set_text_font_size_norm(cfg.font_size_norm);

    app.set_msgbox_show(cfg.show_msgbox);
    app.set_msgbox_width(cfg.msgbox_width);
    app.set_msgbox_height(cfg.msgbox_height);
    app.set_msgbox_pos_x(cfg.msgbox_pos_x);
    app.set_msgbox_pos_y(cfg.msgbox_pos_y);
    app.set_msgbox_font_size(cfg.msgbox_font_size);
    app.set_msgbox_duration(cfg.msgbox_duration.to_string().into());
    app.set_msgbox_gift_min_price(cfg.gift_min_price.to_string().into());

    // 统计
    app.set_stat_histogram(cfg.statistics.iter().any(|s| s == "HISTOGRAM"));
    app.set_stat_table(cfg.statistics.iter().any(|s| s == "TABLE"));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn generated_outputs_preserve_inputs_and_other_batch_outputs() {
        let unique = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH)
            .unwrap().as_nanos();
        let dir = std::env::temp_dir().join(format!("df_gui_paths_{}_{}", std::process::id(), unique));
        std::fs::create_dir(&dir).unwrap();
        let input = dir.join("sample.xml");
        std::fs::write(&input, "source content").unwrap();
        let mut reserved = HashSet::new();
        let first = make_output_path(input.to_str().unwrap(), "", 1, &mut reserved);
        let second = make_output_path(input.to_str().unwrap(), "", 1, &mut reserved);
        assert_ne!(Path::new(&first), input.as_path());
        assert_ne!(first, second);
        assert_eq!(std::fs::read_to_string(&input).unwrap(), "source content");
        std::fs::remove_file(&input).unwrap();
        std::fs::remove_dir(&dir).unwrap();
    }

    #[test]
    fn numeric_inputs_keep_decimals_and_reject_nonfinite_values() {
        assert_eq!(parse_ui_float(" 7.25 ", "duration").unwrap(), 7.25);
        assert!(parse_ui_float("NaN", "duration").is_err());
        assert!(parse_ui_float("inf", "duration").is_err());
        assert!(parse_ui_float("invalid", "duration").is_err());
    }
}
