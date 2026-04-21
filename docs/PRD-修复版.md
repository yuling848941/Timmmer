# Timmmer 代码修复与改进计划 - 产品需求文档 (PRD)

**文档版本**: v1.0  
**最后更新**: 2026-04-08  
**文档状态**: 待执行  
**产品负责人**: [待填写]  
**关联文档**: [PRD.md](./PRD.md) - 主产品需求文档 | [QA_质检报告.md](./QA_质检报告.md) - 质量检测报告

---

## 目录

1. [修复背景与目标](#1-修复背景与目标)
2. [问题分类与优先级](#2-问题分类与优先级)
3. [高严重性问题修复](#3-高严重性问题修复)
4. [代码规范改进](#4-代码规范改进)
5. [性能优化方案](#5-性能优化方案)
6. [安全隐患修复](#6-安全隐患修复)
7. [实施计划与验收](#7-实施计划与验收)
8. [风险评估](#8-风险评估)

---

## 1. 修复背景与目标

### 1.1 修复背景

根据 [QA_质检报告.md](./QA_质检报告.md) 中的质量检测结果，Timmmer 代码库存在以下需要改进的问题：

| 问题类别 | 问题数量 | 平均严重程度 |
|----------|----------|--------------|
| 高严重性问题 | 4 项 | 🔴 高 |
| 代码规范问题 | 3 项 | 🟡 中 |
| 性能问题 | 2 项 | 🟡 中 |
| 安全隐患 | 2 项 | 🟠 中高 |

**总体质量评分**: 8.1/10 - 良好，但有明确改进空间

### 1.2 修复目标

| 目标 | 描述 | 成功指标 |
|------|------|----------|
| 消除高风险 Bug | 修复所有高严重性问题 | 无崩溃风险、无缓冲区溢出 |
| 提升代码质量 | 改进代码规范和可读性 | 代码审查评分 ≥ 9.0/10 |
| 优化性能 | 改进缓存效率和系统调用 | CPU 占用 < 0.5% (当前 < 1%) |
| 消除安全隐患 | 修复所有安全问题 | 安全审计通过 |

### 1.3 修复原则

```
1. 保持功能完整性 - 修复不影响现有功能
2. 最小化改动 - 针对性修复，避免大范围重构
3. 向后兼容 - 配置文件格式保持不变
4. 可验证 - 每个修复都有明确的验收标准
```

---

## 2. 问题分类与优先级

### 2.1 问题优先级矩阵

```
优先级    │ 影响范围    │ 紧急程度 │ 修复时限
─────────┼────────────┼──────────┼──────────
P0-紧急  │ 崩溃/安全  │ 立即     │ 1 周内
P1-高    │ 功能缺陷   │ 高       │ 2 周内
P2-中    │ 代码质量   │ 中       │ 1 月内
P3-低    │ 优化建议   │ 低       │ 3 月内
```

### 2.2 问题清单总览

| ID | 问题描述 | 位置 | 优先级 | 类别 |
|----|----------|------|--------|------|
| BUG-001 | 缓冲区溢出风险 | timer_config.c:17 | P0 | 高严重性 |
| BUG-002 | 空指针解引用风险 | timer_ui.c:210 | P0 | 高严重性 |
| BUG-003 | SetWindowPos 标志问题 | timer_window.c:225 | P1 | 高严重性 |
| BUG-004 | 未初始化变量风险 | timer_dialogs.c | P1 | 高严重性 |
| CODE-001 | 魔法数字硬编码 | timer_ui.c:82 | P2 | 代码规范 |
| CODE-002 | 注释缺失 | timer_core.c | P2 | 代码规范 |
| CODE-003 | 命名不一致 | 多处 | P3 | 代码规范 |
| PERF-001 | 频繁系统调用 | timer_ui.c:94-122 | P2 | 性能 |
| PERF-002 | 缓存命中率低 | timer_ui.c:74-78 | P2 | 性能 |
| SEC-001 | 文件路径未验证 | timer_audio.c:34-38 | P1 | 安全 |
| SEC-002 | 输入验证不足 | timer_config.c:660-673 | P1 | 安全 |

---

## 3. 高严重性问题修复

### 3.1 BUG-001 - 缓冲区溢出风险

**位置**: `timer_config.c:17`

**问题描述**:
```c
// 当前代码
wcscat_s(outPath, maxLen, L"\\");  // 正确
wcscat_s(outPath, maxLen, CONFIG_FILE_PATH);  // 可能溢出
```

`CONFIG_FILE_PATH` 是宽字符串字面量，如果 `outPath` 剩余空间不足会导致缓冲区溢出。

**修复方案**:

```c
// 修复后代码
#define CONFIG_FILE_NAME L"timer_config.ini"
#define MAX_PATH_LEN (MAX_PATH * 2)  // 扩大缓冲区

// 使用更安全的拼接方式
size_t requiredLen = wcslen(outPath) + 1 + wcslen(CONFIG_FILE_NAME);
if (requiredLen >= maxLen) {
    // 错误处理：路径过长
    return FALSE;
}
wcscat_s(outPath, maxLen, L"\\");
wcscat_s(outPath, maxLen, CONFIG_FILE_NAME);
```

**验收标准**:
- [ ] 使用 AddressSanitizer 检测无缓冲区溢出
- [ ] 路径长度边界条件测试通过
- [ ] 错误路径正确返回 FALSE

**影响范围**: `timer_config.c` - `BuildConfigPath()` 函数

---

### 3.2 BUG-002 - 空指针解引用风险

**位置**: `timer_ui.c:210`

**问题描述**:
```c
// 当前代码
HFONT refFont = GetMonospaceFont(100);
if (!refFont) {
    refFont = GetDefaultTimerFont(100);  // 如果仍返回 NULL 会崩溃
}
// 没有检查 refFont 就直接使用
if (refFont) {
    HFONT oldFont = (HFONT)SelectObject(hdc, refFont);
    // ...
}
```

**修复方案**:

```c
// 修复后代码
HFONT refFont = GetMonospaceFont(100);
if (!refFont) {
    refFont = GetDefaultTimerFont(100);
}

// 添加最终检查 - 如果所有字体都不可用，使用降级方案
if (!refFont) {
    // 降级：不使用自定义字体，直接使用系统默认
    HDC hdc = GetDC(NULL);
    if (hdc) {
        // 使用当前 DC 的默认字体
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        fontSize = tm.tmHeight;
        ReleaseDC(NULL, hdc);
    }
    return;  // 提前返回，避免崩溃
}

// 安全使用 refFont
if (refFont) {
    HFONT oldFont = (HFONT)SelectObject(hdc, refFont);
    // ...
}
```

**验收标准**:
- [ ] 字体加载失败时程序不崩溃
- [ ] 降级方案正常显示文本
- [ ] 所有字体获取路径都有 NULL 检查

**影响范围**: `timer_ui.c` - `CalculateFontSize()` 函数

---

### 3.3 BUG-003 - SetWindowPos 标志问题

**位置**: `timer_window.c:225`

**问题描述**:
```c
// 当前代码 - 标志组合可能有问题
SetWindowPos(hwnd, NULL, 0, 0, 0, 0, 
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_DRAWFRAME);
```

`SWP_FRAMECHANGED` 应该单独使用，与其他标志组合可能导致未定义行为。

**修复方案**:

```c
// 修复后代码 - 分两次调用
// 第一次：应用框架变化
SetWindowPos(hwnd, NULL, 0, 0, 0, 0, 
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

// 第二次：触发重绘
SetWindowPos(hwnd, NULL, 0, 0, 0, 0, 
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_DRAWFRAME);
```

**验收标准**:
- [ ] 窗口圆角/阴影效果正常显示
- [ ] 无 GDI 错误
- [ ] Windows 10/11 兼容性测试通过

**影响范围**: `timer_window.c` - 窗口效果设置相关函数

---

### 3.4 BUG-004 - 未初始化变量风险

**位置**: `timer_dialogs.c` - 多个对话框

**问题描述**:
多个对话框过程中存在未初始化指针的风险，可能导致未定义行为。

**修复方案**:

```c
// 修复前
HFONT hFont;
int fontSize;

// 修复后
HFONT hFont = NULL;
int fontSize = 0;

// 添加初始化检查函数
static void InitializeDialogControls(HWND hDlg) {
    // 确保所有控件状态初始化为已知值
    SendMessage(GetDlgItem(hDlg, ID_EDIT_HOURS), EM_SETLIMITTEXT, 2, 0);
    SendMessage(GetDlgItem(hDlg, ID_EDIT_MINUTES), EM_SETLIMITTEXT, 2, 0);
    SendMessage(GetDlgItem(hDlg, ID_EDIT_SECONDS), EM_SETLIMITTEXT, 2, 0);
}
```

**验收标准**:
- [ ] 所有指针变量初始化为 NULL
- [ ] 所有整数变量初始化为 0 或默认值
- [ ] 使用 `/W4` 编译警告级别无未初始化警告

**影响范围**: `timer_dialogs.c` - 所有对话框函数

---

## 4. 代码规范改进

### 4.1 CODE-001 - 魔法数字消除

**位置**: 多处

**问题描述**:
```c
// timer_ui.c:82 - 16（边距计算）
int availableWidth = width - 16;
int availableHeight = height - 16;

// timer_core.c:15-16 - 300, 150（窗口默认大小）
#define DEFAULT_WIDTH 300
#define DEFAULT_HEIGHT 150
```

**修复方案**:

```c
// 在 timer_types.h 中定义常量
#define UI_TEXT_MARGIN 16          // 文本边距 (像素)
#define UI_SMALL_MARGIN 6          // 小边距 (像素)
#define WINDOW_DEFAULT_WIDTH 300   // 默认窗口宽度
#define WINDOW_DEFAULT_HEIGHT 150  // 默认窗口高度
#define WINDOW_MIN_WIDTH 100       // 最小窗口宽度
#define WINDOW_MIN_HEIGHT 50       // 最小窗口高度

// 使用常量替换魔法数字
int availableWidth = width - UI_TEXT_MARGIN;
int availableHeight = height - UI_TEXT_MARGIN;
```

**验收标准**:
- [ ] 所有魔法数字都有命名常量定义
- [ ] 常量命名清晰表达用途
- [ ] 代码审查无魔法数字警告

---

### 4.2 CODE-002 - 注释完善

**位置**: `timer_core.c`, `timer_ui.c`

**问题描述**:
关键函数如 `TimerTick` 缺乏详细注释。

**修复方案**:

```c
/**
 * @brief 计时器心跳处理函数
 * 
 * 处理倒计时、超时判断、超时正计时等逻辑
 * 根据 showMilliseconds 标志决定更新频率
 * 
 * @param hwnd 主窗口句柄
 * 
 * @note 该函数在 WM_TIMER 消息中被调用
 * @note 超时正计时模式会改变字体和背景颜色
 */
void TimerTick(HWND hwnd) {
    // 非运行状态直接返回
    if (!g_timerState.isRunning) {
        return;
    }
    
    // 倒计时逻辑 (仅非超时模式)
    if (g_timerState.seconds >= 0 && !g_timerState.isInOvertimeMode && !g_timerState.isTimeUp) {
        // ... 具体实现
    }
    
    // 超时处理逻辑
    // ...
}
```

**验收标准**:
- [ ] 所有公共函数有 Doxygen 风格注释
- [ ] 复杂逻辑有行内注释
- [ ] 边界条件有说明注释

---

### 4.3 CODE-003 - 命名规范化

**位置**: 多处

**问题描述**:
```c
// 不一致的命名风格
InitializeGlobalState()      // 动词 + 形容词 + 名词
InitializeTimerFonts()       // 动词 + 名词 + 名词
GetMenuTexts()              // Get + 名词复数
GetFontInfoByIndex()        // Get + 名词 + By + 名词
```

**修复方案**:

采用统一的命名规范：
```
[Get/Set/Is/Create/Destroy] + [形容词] + 名词 (+ [介词短语])

示例:
- InitializeState()          // 统一为 Initialize + 名词
- InitializeFonts()          // 统一为 Initialize + 名词
- GetMenuTexts()            // 保持 Get + 名词复数
- GetFontInfoByIndex()      // 保持 Get + 名词 + By + 名词 (这是合理的)
```

**验收标准**:
- [ ] 函数命名风格一致
- [ ] 变量命名遵循驼峰式
- [ ] 常量命名全大写 + 下划线

---

## 5. 性能优化方案

### 5.1 PERF-001 - 减少频繁系统调用

**位置**: `timer_ui.c:94-122`

**问题描述**:
`CalculateFontSize` 在每次 `UpdateDisplay` 时都可能调用 `GetDC(NULL)` 和 `CreateFont`。

**修复方案**:

```c
// 优化前：每次都获取 DC
HDC hdc = GetDC(NULL);
// ... 使用 hdc ...
ReleaseDC(NULL, hdc);

// 优化后：使用静态缓存 DC
static HDC s_cachedDC = NULL;
static DWORD s_lastDCTime = 0;

// 每 5 秒刷新一次缓存 DC
DWORD currentTime = GetTickCount();
if (!s_cachedDC || (currentTime - s_lastDCTime) > 5000) {
    if (s_cachedDC) {
        ReleaseDC(NULL, s_cachedDC);
    }
    s_cachedDC = GetDC(NULL);
    s_lastDCTime = currentTime;
}

// 使用缓存 DC
HDC hdc = s_cachedDC;
```

**验收标准**:
- [ ] CPU 占用降低 50% 以上
- [ ] 无明显性能回归
- [ ] 内存使用无显著增加

---

### 5.2 PERF-002 - 改进缓存命中率

**位置**: `timer_ui.c:74-78`

**问题描述**:
缓存命中条件过于严格，窗口尺寸微小变化就失效。

**修复方案**:

```c
// 优化前：严格匹配
if (width == g_timerState.cachedCalcWidth
 && height == g_timerState.cachedCalcHeight
 && charCount == g_timerState.cachedCharCount) {
    return g_timerState.cachedFontSize;
}

// 优化后：宽松匹配 (允许±2 像素变化)
#define CACHE_TOLERANCE 2

if (abs(width - g_timerState.cachedCalcWidth) <= CACHE_TOLERANCE
 && abs(height - g_timerState.cachedCalcHeight) <= CACHE_TOLERANCE
 && charCount == g_timerState.cachedCharCount
 && g_timerState.cachedFontSize > 0) {
    return g_timerState.cachedFontSize;
}
```

**验收标准**:
- [ ] 缓存命中率提升至 > 90%
- [ ] 字体大小变化无明显视觉跳跃
- [ ] 拖拽调整大小时流畅无卡顿

---

## 6. 安全隐患修复

### 6.1 SEC-001 - 文件路径验证

**位置**: `timer_audio.c:34-38`

**问题描述**:
```c
// 当前代码 - 未验证路径合法性
if (g_timerState.useCustomAudio && wcslen(g_timerState.customAudioPath) > 0) {
    DWORD fileAttributes = GetFileAttributesW(g_timerState.customAudioPath);
    // ...
}
```

未验证路径合法性，可能存在路径遍历风险。

**修复方案**:

```c
/**
 * @brief 验证音频文件路径的安全性
 */
static BOOL ValidateAudioPath(const wchar_t* path) {
    if (!path || wcslen(path) == 0) {
        return FALSE;
    }
    
    // 检查路径长度
    if (wcslen(path) >= MAX_PATH) {
        return FALSE;
    }
    
    // 禁止路径遍历
    if (wcsstr(path, L"..") != NULL) {
        return FALSE;
    }
    
    // 检查文件扩展名
    const wchar_t* ext = wcsrchr(path, L'.');
    if (!ext) {
        return FALSE;
    }
    
    // 只允许 .wav 和 .mp3
    if (_wcsicmp(ext, L".wav") != 0 && _wcsicmp(ext, L".mp3") != 0) {
        return FALSE;
    }
    
    // 检查文件是否存在
    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    // 确保是文件不是目录
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        return FALSE;
    }
    
    return TRUE;
}

// 使用验证函数
if (g_timerState.useCustomAudio && ValidateAudioPath(g_timerState.customAudioPath)) {
    // 安全播放
}
```

**验收标准**:
- [ ] 路径遍历攻击被阻止
- [ ] 非音频文件被拒绝
- [ ] 超长路径被拒绝

---

### 6.2 SEC-002 - 输入验证增强

**位置**: `timer_config.c:660-673`

**问题描述**:
加载窗口配置时，虽然做了范围检查，但逻辑可以更严格。

**修复方案**:

```c
// 优化前：宽松的范围检查
g_timerState.windowX = GetPrivateProfileIntW(L"Window", L"X", 100, configPath);
if (g_timerState.windowX < 0) g_timerState.windowX = 100;

// 优化后：严格的边界验证
#define SCREEN_COORD_MIN -32768
#define SCREEN_COORD_MAX 32767

int windowX = GetPrivateProfileIntW(L"Window", L"X", -1, configPath);
if (windowX < SCREEN_COORD_MIN || windowX > SCREEN_COORD_MAX) {
    windowX = 100;  // 使用默认值
}
g_timerState.windowX = windowX;

// 添加配置文件完整性检查
static BOOL ValidateConfigIntegrity(const wchar_t* configPath) {
    // 检查配置文件大小 (防止异常大的文件)
    HANDLE hFile = CreateFileW(configPath, GENERIC_READ, FILE_SHARE_READ, 
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    DWORD fileSize = GetFileSize(hFile, NULL);
    CloseHandle(hFile);
    
    // 配置文件不应超过 64KB
    if (fileSize > 64 * 1024) {
        return FALSE;
    }
    
    return TRUE;
}
```

**验收标准**:
- [ ] 异常配置值被正确拒绝
- [ ] 配置文件完整性验证通过
- [ ] 边界条件测试全部通过

---

## 7. 实施计划与验收

### 7.1 实施阶段

| 阶段 | 时间 | 任务 | 交付物 |
|------|------|------|--------|
| 第一阶段 | 第 1 周 | 修复高严重性 Bug | BUG-001~004 修复完成 |
| 第二阶段 | 第 2 周 | 修复安全隐患 | SEC-001~002 修复完成 |
| 第三阶段 | 第 3 周 | 代码规范改进 | CODE-001~003 改进完成 |
| 第四阶段 | 第 4 周 | 性能优化 | PERF-001~002 优化完成 |

### 7.2 验收标准

**整体验收**:
- [ ] 所有 P0/P1 问题已修复
- [ ] 程序无崩溃、无内存泄漏
- [ ] 性能指标达标 (CPU < 0.5%, 内存 < 50MB)
- [ ] 代码审查评分 ≥ 9.0/10
- [ ] 所有功能回归测试通过

**单元测试覆盖**:
```
建议新增测试模块:
- test_buffer_overflow.c    // 缓冲区边界测试
- test_null_pointer.c       // 空指针检查测试
- test_path_validation.c    // 路径验证测试
- test_config_integrity.c   // 配置文件完整性测试
```

### 7.3 回归测试清单

| 测试项 | 测试方法 | 预期结果 |
|--------|----------|----------|
| 倒计时功能 | 设置时间并运行 | 时间准确，无溢出 |
| 超时提醒 | 等待倒计时结束 | 音频正常播放 |
| 窗口调整 | 拖拽调整窗口大小 | 流畅无卡顿 |
| 配置保存/加载 | 修改设置后重启 | 设置保持 |
| 自定义音频 | 导入外部音频 | 正常播放 |
| 路径安全 | 尝试路径遍历攻击 | 被正确拒绝 |

---

## 8. 风险评估

### 8.1 技术风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 修复引入新 Bug | 中 | 中 | 回归测试覆盖 |
| 性能优化适得其反 | 低 | 中 | 性能基准测试 |
| 兼容性问题 | 低 | 高 | 多版本 Windows 测试 |

### 8.2 进度风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 修复时间超出预期 | 中 | 低 | 分阶段交付 |
| 测试资源不足 | 低 | 中 | 自动化测试 |

### 8.3 回滚方案

如修复后出现严重问题，可回滚至修复前版本：
```bash
# Git 回滚命令
git stash                    # 暂存当前修改
git checkout <base_commit>   # 回滚到基准版本
```

---

## 附录

### A. 修复检查清单

```markdown
## P0 修复
- [ ] BUG-001: 缓冲区溢出修复
- [ ] BUG-002: 空指针检查修复
- [ ] BUG-003: SetWindowPos 修复
- [ ] BUG-004: 变量初始化修复

## P1 修复
- [ ] SEC-001: 路径验证修复
- [ ] SEC-002: 输入验证修复

## P2 改进
- [ ] CODE-001: 魔法数字消除
- [ ] CODE-002: 注释完善
- [ ] PERF-001: 系统调用优化
- [ ] PERF-002: 缓存优化

## P3 改进
- [ ] CODE-003: 命名规范化
```

### B. 参考资料

- [QA_质检报告.md](./QA_质检报告.md) - 原始质量检测报告
- [PRD.md](./PRD.md) - 主产品需求文档
- [Windows API 安全编程指南](https://learn.microsoft.com/en-us/windows/win32/security/security)
- [C 语言安全编码规范](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)

---

**文档结束**

*本 PRD 文档为 Timmmer 项目代码修复与改进计划，开发团队应参照此文档进行修复工作。*
