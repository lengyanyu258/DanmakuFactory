//! CLI 参数定义 — 使用 clap derive 宏
//!
//! 完整映射原 C 版本的所有命令行选项

use clap::Parser;

/// DanmakuFactory — 弹幕转换工具
///
/// 将 XML/JSON/ASS 格式的弹幕文件互相转换，并支持丰富的渲染配置。
#[derive(Parser, Debug)]
#[command(
    name = "DanmakuFactory",
    version,
    about = "DanmakuFactory - 弹幕转换工具",
    long_about = "DanmakuFactory CLI Release by hkm (hkm@tikm.org)\nhttps://github.com/hihkm/DanmakuFactory\n\n\
                  Usage:\n  \
                  DanmakuFactory -o [输出文件格式] <输出文件名> -i [模板] <输入文件1> [-t <时间偏移1>] \
                  [-i [模板] <输入文件2> [-t <时间偏移2>]] ... [设定名称 设定值] ...\n\n\
                  Format:\n  \
                  xml, json, ass\n\n\
                  Template:\n  \
                  xml, json, ass（显式指定文件格式，优先于扩展名推断）",
    after_long_help = "Notes:\n  \
                       -o / -i 后可选的格式/模板参数取值为 xml、json、ass（大小写不敏感），\n  \
                       缺省时由文件扩展名自动推断。\n  \
                       -t 为紧邻其前的输入文件设置时间偏移（秒），可重复使用；未跟 -t 的文件偏移为 0。\n  \
                       -c 可指定多个配置文件，后者覆盖前者，命令行参数优先级最高。\n\n\
                       Examples:\n  \
                       DanmakuFactory -o outfile.ass -i infile1.xml -i infile2.json -t 5.0 -i infile3.ass -t 7.0\n  \
                       DanmakuFactory -o outfile.ass -i infile1.xml -t 0.0 -i infile2.json -t 5.0 -i infile3.ass -t 7.0\n  \
                       DanmakuFactory -o ass outfile.ass -i xml infile1.xml"
)]
pub struct Args {
    // ==================== 文件 I/O ====================
    /// 输入文件（可多个）；每个文件前可选加格式模板
    ///
    /// 支持 xml、json、ass 三种输入格式，缺省由文件扩展名自动推断。
    /// 可在文件名前加 xml/json/ass 显式指定格式（模板，大小写不敏感），
    /// 如 `-i xml infile1.xml`。可重复使用，并可与 -t 组合为每个文件单独设置时间偏移。
    #[arg(short = 'i', long = "input", num_args = 1.., value_name = "FILE")]
    pub input: Vec<String>,

    /// 输出文件；前面可选加输出格式
    ///
    /// 支持输出为 ass、xml、json，缺省由文件扩展名自动推断。
    /// 可在文件名前加 xml/json/ass 显式指定输出格式（此时文件名可不带扩展名），
    /// 如 `-o ass outfile`。
    #[arg(short = 'o', long = "output", num_args = 1..=2, value_name = "FILE")]
    pub output: Vec<String>,

    /// 时轴偏移（秒）；归属于命令行中紧邻其前的输入文件
    ///
    /// 可重复使用，每个 -t 作用于命令行中它之前最近出现的输入文件，
    /// 未跟 -t 的文件偏移为 0.0，可为负值。例如：`-i infile.xml -t 5.0`。
    #[arg(short = 't', long = "timeshift", num_args = 1, value_name = "SECONDS", allow_hyphen_values = true)]
    pub timeshift: Vec<f32>,

    // ==================== 配置文件 ====================
    /// 指定配置文件（JSON），可传多个，后者覆盖前者
    ///
    /// 从 JSON 配置文件加载基础配置，接受多个文件路径，后者的值覆盖前者。
    /// 命令行参数优先级最高。
    /// 例如：`-c "公共基础配置.json" "仅含个别值的特化配置.json"`
    #[arg(short = 'c', long = "config", num_args = 1.., value_name = "FILE")]
    pub config: Vec<String>,

    /// 将最终的命令行设置保存为配置文件
    ///
    /// 将配置文件与命令行参数合并后的最终配置保存到当前目录。
    #[arg(long = "save")]
    pub save: bool,

    // ==================== 屏幕设置 ====================
    /// 分辨率，如 "1920x1080"
    ///
    /// 用任意字符连接宽和高，如 "1920x1080"，须为两个大于 0 的整数。
    /// 默认值：1920x1080
    #[arg(short = 'r', long = "resolution", value_name = "WxH")]
    pub resolution: Option<String>,

    /// 全部弹幕的显示区域比例
    ///
    /// 全部弹幕在屏幕上的显示区域百分比，范围 0.0-1.0。
    /// 默认值：1.00
    #[arg(long = "display-area", value_name = "RATIO")]
    pub display_area: Option<f32>,

    /// 滚动弹幕的显示区域比例
    ///
    /// 滚动弹幕在屏幕上的显示区域百分比，范围 0.0-1.0。
    /// 默认值：1.00
    #[arg(long = "scroll-area", value_name = "RATIO")]
    pub scroll_area: Option<f32>,

    /// 统计模式，如 "TABLE-HISTOGRAM"
    ///
    /// 指定屏幕上显示的统计图类型，用 '-' 连接多个类型。
    /// 可用值：TABLE, HISTOGRAM。默认值：不显示
    #[arg(long = "statistics-mode", alias = "statistics_mode", value_name = "MODE")]
    pub statistics_mode: Option<String>,

    // ==================== 弹幕设置 ====================
    /// 滚动弹幕横跨屏幕的时间（秒）
    ///
    /// 滚动弹幕从屏幕一侧滚动到另一侧的通过时间，须大于 0.0。
    /// 默认值：12.0
    #[arg(short = 's', long = "scroll-duration", value_name = "SECONDS")]
    pub scroll_duration: Option<f32>,

    /// 固定弹幕的显示时间（秒）
    ///
    /// 固定（顶部/底部）弹幕在屏幕上的停留时间，须大于 0.0。
    /// 默认值：5.0
    #[arg(short = 'f', long = "fixed-duration", value_name = "SECONDS")]
    pub fixed_duration: Option<f32>,

    /// 同屏最大弹幕数量
    ///
    /// 同一时刻屏幕上允许显示的最大弹幕条数。
    /// 特殊值：-1 不重叠，0 无限制；其他正整数表示限定条数。
    /// 默认值：0
    #[arg(short = 'd', long = "density", value_name = "N", allow_hyphen_values = true)]
    pub density: Option<i32>,

    /// 按弹幕类型屏蔽，如 "L2R-TOP-BOTTOM"
    ///
    /// 指定不在屏幕上显示的弹幕类型，用 '-' 连接多个类型。
    /// 可用值：L2R, R2L, TOP, BOTTOM, SPECIAL, COLOR, REPEAT。
    /// 默认值：不屏蔽
    #[arg(short = 'b', long = "block-mode", alias = "block_mode", value_name = "MODE")]
    pub block_mode: Option<String>,

    /// 是否保存被屏蔽的弹幕
    ///
    /// 为 false 时被屏蔽的内容不会保留在输出文件中。
    /// 可用值：true, false。默认值：true
    #[arg(long = "saveblocked", value_name = "BOOL")]
    pub save_blocked: Option<bool>,

    /// 是否显示用户名（如有）
    ///
    /// 可用值：true, false。默认值：false
    #[arg(long = "showusernames", value_name = "BOOL")]
    pub show_usernames: Option<bool>,

    /// 弹幕内容黑名单文件
    ///
    /// 纯文本文件，内容以换行分隔（不超过 4096 条），匹配的弹幕不会显示。
    /// 例如：`--blacklist black.txt`
    #[arg(long = "blacklist", value_name = "FILE")]
    pub blacklist: Option<String>,

    /// 黑名单是否启用正则表达式匹配
    ///
    /// 为 true 时黑名单文件中的每一行按正则表达式处理。
    /// 可用值：true, false。默认值：false
    #[arg(long = "blacklist-regex", value_name = "BOOL")]
    pub blacklist_regex: Option<bool>,

    // ==================== 文本设置 ====================
    /// 普通弹幕的行间距（像素）
    ///
    /// 可为正负整数。默认值：0
    #[arg(long = "line-spacing", value_name = "PX", allow_hyphen_values = true)]
    pub line_spacing: Option<i32>,

    /// 普通弹幕的字号（像素）
    ///
    /// 须为大于 0 的整数。默认值：38
    #[arg(short = 'S', long = "font-size", value_name = "SIZE")]
    pub font_size: Option<i32>,

    /// 严格保持 -S 指定的字号
    ///
    /// 使普通弹幕始终保持 -S 指定的字号大小。
    /// 例如：`-S 25 --font-size-strict`
    #[arg(long = "font-size-strict")]
    pub font_size_strict: bool,

    /// 修正（归一化）普通弹幕的字号
    ///
    /// 采用归一化方法修正普通弹幕的字号。
    #[arg(long = "font-size-norm")]
    pub font_size_norm: bool,

    /// 普通弹幕的字体名称
    ///
    /// 默认值："Microsoft YaHei"
    #[arg(short = 'N', long = "fontname", value_name = "NAME")]
    pub fontname: Option<String>,

    /// 弹幕不透明度
    ///
    /// 适用于特殊弹幕以外的所有弹幕，范围 1-255，越小越透明。
    /// 默认值：180
    #[arg(short = 'O', long = "opacity", value_name = "N")]
    pub opacity: Option<i32>,

    /// 描边宽度
    ///
    /// 每条弹幕的描边宽度，范围 0-4。默认值：0
    #[arg(short = 'L', long = "outline", value_name = "WIDTH")]
    pub outline: Option<f32>,

    /// 描边模糊半径
    ///
    /// 可用值：0 或正数。默认值：0
    #[arg(long = "outline-blur", value_name = "RADIUS")]
    pub outline_blur: Option<f32>,

    /// 描边不透明度
    ///
    /// 适用于特殊弹幕以外的所有弹幕，范围 0-255。
    /// 默认值：255（完全不透明）
    #[arg(long = "outline-opacity", value_name = "N")]
    pub outline_opacity: Option<i32>,

    /// 阴影深度
    ///
    /// 每条弹幕的阴影深度，范围 0-4。默认值：1
    #[arg(short = 'D', long = "shadow", value_name = "DEPTH")]
    pub shadow: Option<f32>,

    /// 是否使用粗体字
    ///
    /// 可用值：true, false。默认值：false
    #[arg(short = 'B', long = "bold", value_name = "BOOL")]
    pub bold: Option<bool>,

    // ==================== 消息框设置 ====================
    /// 是否显示消息框（礼物框）
    ///
    /// 可用值：true, false。默认值：true
    #[arg(long = "showmsgbox", value_name = "BOOL")]
    pub show_msgbox: Option<bool>,

    /// 消息框尺寸，如 "400x1000"
    ///
    /// 用任意字符连接宽和高。注意：宽+posX、高+posY 不应超过分辨率。
    /// 默认值：500x1080
    #[arg(long = "msgboxsize", value_name = "WxH")]
    pub msgbox_size: Option<String>,

    /// 消息框位置，如 "50x50"
    ///
    /// 用任意字符连接 posX 和 posY。注意：宽+posX、高+posY 不应超过分辨率。
    /// 默认值：20x0
    #[arg(long = "msgboxpos", value_name = "XxY")]
    pub msgbox_pos: Option<String>,

    /// 消息框文字大小（像素）
    ///
    /// 默认值：38
    #[arg(long = "msgboxfontsize", value_name = "SIZE")]
    pub msgbox_font_size: Option<i32>,

    /// 消息框持续时长（秒）
    ///
    /// 设置后将覆盖从 XML 中读取的每条消息自带时长，须大于等于 0。
    /// 默认值：0.0（读取 XML 自带时长）
    #[arg(long = "msgboxduration", value_name = "SECONDS")]
    pub msgbox_duration: Option<f32>,

    /// 显示礼物的最小价值（元）
    ///
    /// 低于该价格的礼物不会显示。例如：`--giftminprice 5.20`
    /// 默认值：0.0
    #[arg(long = "giftminprice", value_name = "PRICE")]
    pub gift_min_price: Option<f32>,

    // ==================== 其他 ====================
    /// 忽略所有警告
    ///
    /// 忽略诸如“是否覆盖文件”等警告。
    #[arg(long = "ignore-warnings")]
    pub ignore_warnings: bool,
}
