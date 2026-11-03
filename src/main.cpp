#include <curses.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>   // isatty, fileno（POSIX）
#include <thread>
#include <chrono>


#include "Liner.h"
#include "Matrix.h"

namespace {

// 日志文件：默认写在当前目录，可用环境变量 DRAW_CHAR_LOG 覆盖
constexpr const char* kDefaultLogPath = "draw_char_at.log";

const char* log_path() {
    const char* p = std::getenv("DRAW_CHAR_LOG");
    return (p != nullptr && *p != '\0') ? p : kDefaultLogPath;
}

void format_timestamp(char* buf, std::size_t n) {
    const std::time_t now = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);     // 多线程下比 std::localtime 安全
#endif
    std::strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmv);
}

// 追加一行日志。任何失败（打不开文件、写失败）都静默忽略，
// 绝不让"记日志"这件事本身影响绘制主流程或抛出异常。
void log_error(const char* fmt, ...) {
    std::FILE* f = std::fopen(log_path(), "a");
    if (f == nullptr) return;

    char ts[32] = {0};
    format_timestamp(ts, sizeof ts);
    std::fprintf(f, "[%s] ERROR draw_char_at: ", ts);

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);

    std::fputc('\n', f);
    std::fclose(f);
}

// 把字符写成便于阅读的形式：可打印带引号，否则只给十六进制
void describe_char(char* buf, std::size_t n, unsigned char uc) {
    if (uc >= 0x20 && uc != 0x7F) {
        std::snprintf(buf, n, "0x%02X ('%c')", uc, uc);
    } else {
        std::snprintf(buf, n, "0x%02X (控制字符)", uc);
    }
}

}  // namespace

// 在屏幕坐标 (y, x) 处显示字符 ch
//
// 前置条件：
//   1. ncurses 已通过 initscr() 初始化（stdscr 非空）
//   2. 0 <= y < LINES 且 0 <= x < COLS
//   3. ch 不是控制字符
//
// 后置行为：
//   * 条件全部满足：写入虚拟屏幕 -> 还原光标 -> refresh()
//   * 任一条件不满足：不绘制、不 refresh，把详细信息写进日志后安静返回
void draw_char_at(int y, int x, char ch) {
    // 注意：char 可能是有符号的，必须先转 unsigned char 再比较/格式化，
    // 否则对 >= 0x80 的字节做 %02X 之类会得到符号扩展的垃圾值。
    const unsigned char uc = static_cast<unsigned char>(ch);

    // ---- 1. ncurses 是否已初始化 ----
    if (stdscr == nullptr) {
        log_error("ncurses 未初始化(stdscr == nullptr)，跳过绘制；"
                  "请求 (y=%d, x=%d)，字符=0x%02X", y, x, uc);
        return;
    }

    // ---- 2. 坐标越界：不画，记录详细上下文 ----
    const int h = LINES;
    const int w = COLS;
    if (y < 0 || y >= h || x < 0 || x >= w) {
        char desc[32];
        describe_char(desc, sizeof desc, uc);

        const bool y_bad = (y < 0 || y >= h);
        const bool x_bad = (x < 0 || x >= w);
        const char* which = (y_bad && x_bad) ? "y 与 x 均越界"
                          :  y_bad            ? "y 越界"
                                              : "x 越界";

        log_error("坐标越界，已跳过绘制。请求 (y=%d, x=%d)；"
                  "屏幕有效范围 y=[0,%d)、x=[0,%d)；判定=%s；"
                  "字符=%s；终端=%s；TERM=%s",
                  y, x, h, w, which, desc,
                  ::isatty(::fileno(stdout)) ? "tty" : "non-tty",
                  std::getenv("TERM") ? std::getenv("TERM") : "(unset)");
        return;
    }

    // ---- 3. 非法字符：同样不画，记录原因 ----
    if (uc < 0x20 || uc == 0x7F) {
        log_error("字符为控制字符，已跳过绘制。坐标 (y=%d, x=%d)，字符=0x%02X",
                  y, x, uc);
        return;
    }

    // ---- 4. 正常绘制：保存光标 -> 写入 -> 还原 -> 刷新 ----
    int old_y = 0, old_x = 0;
    getyx(stdscr, old_y, old_x);   // 只在"确定要画"时才取光标位置

    const int rc = mvaddch(y, x, static_cast<chtype>(uc));
    if (rc == ERR) {
        // ncurses 不保证设置 errno，这里的 errno 可能只是陈旧值，仅作参考
        log_error("mvaddch 返回 ERR。坐标 (y=%d, x=%d)，字符=0x%02X，"
                  "errno=%d(%s)", y, x, uc, errno, std::strerror(errno));
        move(old_y, old_x);        // 尽力还原光标
        refresh();
        return;
    }

    move(old_y, old_x);
    refresh();
}

void print(Jidao::Vector<2> pos)
{
    auto [x,y] = Jidao::map(pos, COLS, LINES).get();
    draw_char_at(y, x, '@');
}



std::array<Jidao::Vector<3>, 248> points = {
    // === 原始 8 个顶点 ===
    Jidao::Vector<3>{-1.0f, -1.0f, -1.0f}, // 0
    Jidao::Vector<3>{-1.0f, -1.0f,  1.0f}, // 1
    Jidao::Vector<3>{-1.0f,  1.0f, -1.0f}, // 2
    Jidao::Vector<3>{-1.0f,  1.0f,  1.0f}, // 3
    Jidao::Vector<3>{ 1.0f, -1.0f, -1.0f}, // 4
    Jidao::Vector<3>{ 1.0f, -1.0f,  1.0f}, // 5
    Jidao::Vector<3>{ 1.0f,  1.0f, -1.0f}, // 6
    Jidao::Vector<3>{ 1.0f,  1.0f,  1.0f}, // 7

    // === 棱 {0, 1}: (-1,-1,-1) 到 (-1,-1,1) ===
    Jidao::Vector<3>{-1.0f, -1.0f, -0.9047619f}, Jidao::Vector<3>{-1.0f, -1.0f, -0.8095238f}, 
    Jidao::Vector<3>{-1.0f, -1.0f, -0.7142857f}, Jidao::Vector<3>{-1.0f, -1.0f, -0.6190476f}, 
    Jidao::Vector<3>{-1.0f, -1.0f, -0.5238095f}, Jidao::Vector<3>{-1.0f, -1.0f, -0.4285714f}, 
    Jidao::Vector<3>{-1.0f, -1.0f, -0.3333333f}, Jidao::Vector<3>{-1.0f, -1.0f, -0.2380952f}, 
    Jidao::Vector<3>{-1.0f, -1.0f, -0.1428571f}, Jidao::Vector<3>{-1.0f, -1.0f, -0.0476190f}, 
    Jidao::Vector<3>{-1.0f, -1.0f,  0.0476190f}, Jidao::Vector<3>{-1.0f, -1.0f,  0.1428571f}, 
    Jidao::Vector<3>{-1.0f, -1.0f,  0.2380952f}, Jidao::Vector<3>{-1.0f, -1.0f,  0.3333333f}, 
    Jidao::Vector<3>{-1.0f, -1.0f,  0.4285714f}, Jidao::Vector<3>{-1.0f, -1.0f,  0.5238095f}, 
    Jidao::Vector<3>{-1.0f, -1.0f,  0.6190476f}, Jidao::Vector<3>{-1.0f, -1.0f,  0.7142857f}, 
    Jidao::Vector<3>{-1.0f, -1.0f,  0.8095238f}, Jidao::Vector<3>{-1.0f, -1.0f,  0.9047619f},

    // === 棱 {0, 2}: (-1,-1,-1) 到 (-1,1,-1) ===
    Jidao::Vector<3>{-1.0f, -0.9047619f, -1.0f}, Jidao::Vector<3>{-1.0f, -0.8095238f, -1.0f}, 
    Jidao::Vector<3>{-1.0f, -0.7142857f, -1.0f}, Jidao::Vector<3>{-1.0f, -0.6190476f, -1.0f}, 
    Jidao::Vector<3>{-1.0f, -0.5238095f, -1.0f}, Jidao::Vector<3>{-1.0f, -0.4285714f, -1.0f}, 
    Jidao::Vector<3>{-1.0f, -0.3333333f, -1.0f}, Jidao::Vector<3>{-1.0f, -0.2380952f, -1.0f}, 
    Jidao::Vector<3>{-1.0f, -0.1428571f, -1.0f}, Jidao::Vector<3>{-1.0f, -0.0476190f, -1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.0476190f, -1.0f}, Jidao::Vector<3>{-1.0f,  0.1428571f, -1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.2380952f, -1.0f}, Jidao::Vector<3>{-1.0f,  0.3333333f, -1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.4285714f, -1.0f}, Jidao::Vector<3>{-1.0f,  0.5238095f, -1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.6190476f, -1.0f}, Jidao::Vector<3>{-1.0f,  0.7142857f, -1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.8095238f, -1.0f}, Jidao::Vector<3>{-1.0f,  0.9047619f, -1.0f},

    // === 棱 {0, 4}: (-1,-1,-1) 到 (1,-1,-1) ===
    Jidao::Vector<3>{-0.9047619f, -1.0f, -1.0f}, Jidao::Vector<3>{-0.8095238f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{-0.7142857f, -1.0f, -1.0f}, Jidao::Vector<3>{-0.6190476f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{-0.5238095f, -1.0f, -1.0f}, Jidao::Vector<3>{-0.4285714f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{-0.3333333f, -1.0f, -1.0f}, Jidao::Vector<3>{-0.2380952f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{-0.1428571f, -1.0f, -1.0f}, Jidao::Vector<3>{-0.0476190f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.0476190f, -1.0f, -1.0f}, Jidao::Vector<3>{ 0.1428571f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.2380952f, -1.0f, -1.0f}, Jidao::Vector<3>{ 0.3333333f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.4285714f, -1.0f, -1.0f}, Jidao::Vector<3>{ 0.5238095f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.6190476f, -1.0f, -1.0f}, Jidao::Vector<3>{ 0.7142857f, -1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.8095238f, -1.0f, -1.0f}, Jidao::Vector<3>{ 0.9047619f, -1.0f, -1.0f},

    // === 棱 {1, 3}: (-1,-1,1) 到 (-1,1,1) ===
    Jidao::Vector<3>{-1.0f, -0.9047619f, 1.0f}, Jidao::Vector<3>{-1.0f, -0.8095238f, 1.0f}, 
    Jidao::Vector<3>{-1.0f, -0.7142857f, 1.0f}, Jidao::Vector<3>{-1.0f, -0.6190476f, 1.0f}, 
    Jidao::Vector<3>{-1.0f, -0.5238095f, 1.0f}, Jidao::Vector<3>{-1.0f, -0.4285714f, 1.0f}, 
    Jidao::Vector<3>{-1.0f, -0.3333333f, 1.0f}, Jidao::Vector<3>{-1.0f, -0.2380952f, 1.0f}, 
    Jidao::Vector<3>{-1.0f, -0.1428571f, 1.0f}, Jidao::Vector<3>{-1.0f, -0.0476190f, 1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.0476190f, 1.0f}, Jidao::Vector<3>{-1.0f,  0.1428571f, 1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.2380952f, 1.0f}, Jidao::Vector<3>{-1.0f,  0.3333333f, 1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.4285714f, 1.0f}, Jidao::Vector<3>{-1.0f,  0.5238095f, 1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.6190476f, 1.0f}, Jidao::Vector<3>{-1.0f,  0.7142857f, 1.0f}, 
    Jidao::Vector<3>{-1.0f,  0.8095238f, 1.0f}, Jidao::Vector<3>{-1.0f,  0.9047619f, 1.0f},

    // === 棱 {1, 5}: (-1,-1,1) 到 (1,-1,1) ===
    Jidao::Vector<3>{-0.9047619f, -1.0f, 1.0f}, Jidao::Vector<3>{-0.8095238f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{-0.7142857f, -1.0f, 1.0f}, Jidao::Vector<3>{-0.6190476f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{-0.5238095f, -1.0f, 1.0f}, Jidao::Vector<3>{-0.4285714f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{-0.3333333f, -1.0f, 1.0f}, Jidao::Vector<3>{-0.2380952f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{-0.1428571f, -1.0f, 1.0f}, Jidao::Vector<3>{-0.0476190f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.0476190f, -1.0f, 1.0f}, Jidao::Vector<3>{ 0.1428571f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.2380952f, -1.0f, 1.0f}, Jidao::Vector<3>{ 0.3333333f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.4285714f, -1.0f, 1.0f}, Jidao::Vector<3>{ 0.5238095f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.6190476f, -1.0f, 1.0f}, Jidao::Vector<3>{ 0.7142857f, -1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.8095238f, -1.0f, 1.0f}, Jidao::Vector<3>{ 0.9047619f, -1.0f, 1.0f},

    // === 棱 {2, 3}: (-1,1,-1) 到 (-1,1,1) ===
    Jidao::Vector<3>{-1.0f, 1.0f, -0.9047619f}, Jidao::Vector<3>{-1.0f, 1.0f, -0.8095238f}, 
    Jidao::Vector<3>{-1.0f, 1.0f, -0.7142857f}, Jidao::Vector<3>{-1.0f, 1.0f, -0.6190476f}, 
    Jidao::Vector<3>{-1.0f, 1.0f, -0.5238095f}, Jidao::Vector<3>{-1.0f, 1.0f, -0.4285714f}, 
    Jidao::Vector<3>{-1.0f, 1.0f, -0.3333333f}, Jidao::Vector<3>{-1.0f, 1.0f, -0.2380952f}, 
    Jidao::Vector<3>{-1.0f, 1.0f, -0.1428571f}, Jidao::Vector<3>{-1.0f, 1.0f, -0.0476190f}, 
    Jidao::Vector<3>{-1.0f, 1.0f,  0.0476190f}, Jidao::Vector<3>{-1.0f, 1.0f,  0.1428571f}, 
    Jidao::Vector<3>{-1.0f, 1.0f,  0.2380952f}, Jidao::Vector<3>{-1.0f, 1.0f,  0.3333333f}, 
    Jidao::Vector<3>{-1.0f, 1.0f,  0.4285714f}, Jidao::Vector<3>{-1.0f, 1.0f,  0.5238095f}, 
    Jidao::Vector<3>{-1.0f, 1.0f,  0.6190476f}, Jidao::Vector<3>{-1.0f, 1.0f,  0.7142857f}, 
    Jidao::Vector<3>{-1.0f, 1.0f,  0.8095238f}, Jidao::Vector<3>{-1.0f, 1.0f,  0.9047619f},

    // === 棱 {2, 6}: (-1,1,-1) 到 (1,1,-1) ===
    Jidao::Vector<3>{-0.9047619f, 1.0f, -1.0f}, Jidao::Vector<3>{-0.8095238f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{-0.7142857f, 1.0f, -1.0f}, Jidao::Vector<3>{-0.6190476f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{-0.5238095f, 1.0f, -1.0f}, Jidao::Vector<3>{-0.4285714f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{-0.3333333f, 1.0f, -1.0f}, Jidao::Vector<3>{-0.2380952f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{-0.1428571f, 1.0f, -1.0f}, Jidao::Vector<3>{-0.0476190f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.0476190f, 1.0f, -1.0f}, Jidao::Vector<3>{ 0.1428571f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.2380952f, 1.0f, -1.0f}, Jidao::Vector<3>{ 0.3333333f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.4285714f, 1.0f, -1.0f}, Jidao::Vector<3>{ 0.5238095f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.6190476f, 1.0f, -1.0f}, Jidao::Vector<3>{ 0.7142857f, 1.0f, -1.0f}, 
    Jidao::Vector<3>{ 0.8095238f, 1.0f, -1.0f}, Jidao::Vector<3>{ 0.9047619f, 1.0f, -1.0f},

    // === 棱 {3, 7}: (-1,1,1) 到 (1,1,1) ===
    Jidao::Vector<3>{-0.9047619f, 1.0f, 1.0f}, Jidao::Vector<3>{-0.8095238f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{-0.7142857f, 1.0f, 1.0f}, Jidao::Vector<3>{-0.6190476f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{-0.5238095f, 1.0f, 1.0f}, Jidao::Vector<3>{-0.4285714f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{-0.3333333f, 1.0f, 1.0f}, Jidao::Vector<3>{-0.2380952f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{-0.1428571f, 1.0f, 1.0f}, Jidao::Vector<3>{-0.0476190f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.0476190f, 1.0f, 1.0f}, Jidao::Vector<3>{ 0.1428571f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.2380952f, 1.0f, 1.0f}, Jidao::Vector<3>{ 0.3333333f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.4285714f, 1.0f, 1.0f}, Jidao::Vector<3>{ 0.5238095f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.6190476f, 1.0f, 1.0f}, Jidao::Vector<3>{ 0.7142857f, 1.0f, 1.0f}, 
    Jidao::Vector<3>{ 0.8095238f, 1.0f, 1.0f}, Jidao::Vector<3>{ 0.9047619f, 1.0f, 1.0f},

    // === 棱 {4, 5}: (1,-1,-1) 到 (1,-1,1) ===
    Jidao::Vector<3>{1.0f, -1.0f, -0.9047619f}, Jidao::Vector<3>{1.0f, -1.0f, -0.8095238f}, 
    Jidao::Vector<3>{1.0f, -1.0f, -0.7142857f}, Jidao::Vector<3>{1.0f, -1.0f, -0.6190476f}, 
    Jidao::Vector<3>{1.0f, -1.0f, -0.5238095f}, Jidao::Vector<3>{1.0f, -1.0f, -0.4285714f}, 
    Jidao::Vector<3>{1.0f, -1.0f, -0.3333333f}, Jidao::Vector<3>{1.0f, -1.0f, -0.2380952f}, 
    Jidao::Vector<3>{1.0f, -1.0f, -0.1428571f}, Jidao::Vector<3>{1.0f, -1.0f, -0.0476190f}, 
    Jidao::Vector<3>{1.0f, -1.0f,  0.0476190f}, Jidao::Vector<3>{1.0f, -1.0f,  0.1428571f}, 
    Jidao::Vector<3>{1.0f, -1.0f,  0.2380952f}, Jidao::Vector<3>{1.0f, -1.0f,  0.3333333f}, 
    Jidao::Vector<3>{1.0f, -1.0f,  0.4285714f}, Jidao::Vector<3>{1.0f, -1.0f,  0.5238095f}, 
    Jidao::Vector<3>{1.0f, -1.0f,  0.6190476f}, Jidao::Vector<3>{1.0f, -1.0f,  0.7142857f}, 
    Jidao::Vector<3>{1.0f, -1.0f,  0.8095238f}, Jidao::Vector<3>{1.0f, -1.0f,  0.9047619f},

    // === 棱 {4, 6}: (1,-1,-1) 到 (1,1,-1) ===
    Jidao::Vector<3>{1.0f, -0.9047619f, -1.0f}, Jidao::Vector<3>{1.0f, -0.8095238f, -1.0f}, 
    Jidao::Vector<3>{1.0f, -0.7142857f, -1.0f}, Jidao::Vector<3>{1.0f, -0.6190476f, -1.0f}, 
    Jidao::Vector<3>{1.0f, -0.5238095f, -1.0f}, Jidao::Vector<3>{1.0f, -0.4285714f, -1.0f}, 
    Jidao::Vector<3>{1.0f, -0.3333333f, -1.0f}, Jidao::Vector<3>{1.0f, -0.2380952f, -1.0f}, 
    Jidao::Vector<3>{1.0f, -0.1428571f, -1.0f}, Jidao::Vector<3>{1.0f, -0.0476190f, -1.0f}, 
    Jidao::Vector<3>{1.0f,  0.0476190f, -1.0f}, Jidao::Vector<3>{1.0f,  0.1428571f, -1.0f}, 
    Jidao::Vector<3>{1.0f,  0.2380952f, -1.0f}, Jidao::Vector<3>{1.0f,  0.3333333f, -1.0f}, 
    Jidao::Vector<3>{1.0f,  0.4285714f, -1.0f}, Jidao::Vector<3>{1.0f,  0.5238095f, -1.0f}, 
    Jidao::Vector<3>{1.0f,  0.6190476f, -1.0f}, Jidao::Vector<3>{1.0f,  0.7142857f, -1.0f}, 
    Jidao::Vector<3>{1.0f,  0.8095238f, -1.0f}, Jidao::Vector<3>{1.0f,  0.9047619f, -1.0f},

    // === 棱 {5, 7}: (1,-1,1) 到 (1,1,1) ===
    Jidao::Vector<3>{1.0f, -0.9047619f, 1.0f}, Jidao::Vector<3>{1.0f, -0.8095238f, 1.0f}, 
    Jidao::Vector<3>{1.0f, -0.7142857f, 1.0f}, Jidao::Vector<3>{1.0f, -0.6190476f, 1.0f}, 
    Jidao::Vector<3>{1.0f, -0.5238095f, 1.0f}, Jidao::Vector<3>{1.0f, -0.4285714f, 1.0f}, 
    Jidao::Vector<3>{1.0f, -0.3333333f, 1.0f}, Jidao::Vector<3>{1.0f, -0.2380952f, 1.0f}, 
    Jidao::Vector<3>{1.0f, -0.1428571f, 1.0f}, Jidao::Vector<3>{1.0f, -0.0476190f, 1.0f}, 
    Jidao::Vector<3>{1.0f,  0.0476190f, 1.0f}, Jidao::Vector<3>{1.0f,  0.1428571f, 1.0f}, 
    Jidao::Vector<3>{1.0f,  0.2380952f, 1.0f}, Jidao::Vector<3>{1.0f,  0.3333333f, 1.0f}, 
    Jidao::Vector<3>{1.0f,  0.4285714f, 1.0f}, Jidao::Vector<3>{1.0f,  0.5238095f, 1.0f}, 
    Jidao::Vector<3>{1.0f,  0.6190476f, 1.0f}, Jidao::Vector<3>{1.0f,  0.7142857f, 1.0f}, 
    Jidao::Vector<3>{1.0f,  0.8095238f, 1.0f}, Jidao::Vector<3>{1.0f,  0.9047619f, 1.0f},

    // === 棱 {6, 7}: (1,1,-1) 到 (1,1,1) ===
    Jidao::Vector<3>{1.0f, 1.0f, -0.9047619f}, Jidao::Vector<3>{1.0f, 1.0f, -0.8095238f}, 
    Jidao::Vector<3>{1.0f, 1.0f, -0.7142857f}, Jidao::Vector<3>{1.0f, 1.0f, -0.6190476f}, 
    Jidao::Vector<3>{1.0f, 1.0f, -0.5238095f}, Jidao::Vector<3>{1.0f, 1.0f, -0.4285714f}, 
    Jidao::Vector<3>{1.0f, 1.0f, -0.3333333f}, Jidao::Vector<3>{1.0f, 1.0f, -0.2380952f}, 
    Jidao::Vector<3>{1.0f, 1.0f, -0.1428571f}, Jidao::Vector<3>{1.0f, 1.0f, -0.0476190f}, 
    Jidao::Vector<3>{1.0f, 1.0f,  0.0476190f}, Jidao::Vector<3>{1.0f, 1.0f,  0.1428571f}, 
    Jidao::Vector<3>{1.0f, 1.0f,  0.2380952f}, Jidao::Vector<3>{1.0f, 1.0f,  0.3333333f}, 
    Jidao::Vector<3>{1.0f, 1.0f,  0.4285714f}, Jidao::Vector<3>{1.0f, 1.0f,  0.5238095f}, 
    Jidao::Vector<3>{1.0f, 1.0f,  0.6190476f}, Jidao::Vector<3>{1.0f, 1.0f,  0.7142857f}, 
    Jidao::Vector<3>{1.0f, 1.0f,  0.8095238f}, Jidao::Vector<3>{1.0f, 1.0f,  0.9047619f}
};


void turn_cube()
{
    Jidao::Matrix<3,3> turn_m = {
        {  0.7285014f, -0.2345697f,  0.6436325f },
        {  0.4461563f,  0.8754261f, -0.1859400f },
        { -0.5198368f,  0.4226183f,  0.7424039f }
    };
    for (auto& v: points) {
        v = turn_m*v;
    }
}

void turn_cube2()
{
    Jidao::Matrix<3,3> turn_m = {
    { 0.9998477f, -0.0174524f,  0.0f        },
    { 0.0174524f,  0.9998477f,  0.0f        },
    { 0.0f,        0.0f,        1.0f        }
    };
    for (auto& v: points) {
        v = turn_m*v;
    }
}

void show_cube()
{
    for (const auto& i: points) {
        auto pos = Jidao::get_pos(i,10);
        print(pos);
    }
}


// ---------------- 示例 ----------------
int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    turn_cube();

    while (true) {
        clear();
        turn_cube2();
        show_cube();
        std::this_thread::sleep_for(std::chrono::duration<double>(1.0 / 60.0));
        refresh();
    }

    getch();

    endwin();
    return 0;
}
