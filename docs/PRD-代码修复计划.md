# Timmmer 代码修复与改进计划 - 产品需求文档 (PRD)

**文档版本**: v2.0  
**最后更新**: 2026-04-08  
**文档状态**: 待执行  
**产品负责人**: [待填写]  

**前置说明**: 本 PRD 基于对实际代码的审查分析，替代原 v1.0 版本（该版本基于不准确的 QA 报告）。

---

## 目录

1. [修复背景与目标](#1-修复背景与目标)
2. [实际问题清单](#2-实际问题清单)
3. [修复方案](#3-修复方案)
4. [实施计划](#4-实施计划)
5. [验收标准](#5-验收标准)

---

## 1. 修复背景与目标

### 1.1 修复背景

通过对 Timmmer 项目代码的实际审查，发现以下**真实存在的问题**：

| 问题类别 | 问题数量 | 严重程度 |
|----------|----------|----------|
| 安全隐患 | 2 项 | 🟠 中 |
| 代码规范 | 3 项 | 🟡 低 |
| 性能优化 | 1 项 | 🟡 低 |

**原 v1.0 PRD 中的误判**：
- ❌ BUG-001（缓冲区溢出）：`wcscat_s` 已提供边界保护，风险极低
- ❌ BUG-002（空指针解引用）：代码已有 NULL 检查
- ❌ BUG-003（SetWindowPos 标志）：标志组合是 Windows API 标准用法
- ❌ BUG-004（未初始化变量）：static 变量自动初始化

### 1.2 修复目标

| 目标 | 描述 | 成功指标 |
|------|------|----------|
| 消除安全隐患 | 修复路径验证缺失问题 | 路径遍历攻击被阻止 |
| 提升代码质量 | 消除魔法数字、完善注释 | 代码审查无规范警告 |
| 优化性能 | 改进缓存命中率 | 窗口拖拽更流畅 |

### 1.3 修复原则

```
1. 保持功能完整性 - 修复不影响现有功能
2. 最小化改动 - 针对性修复，避免大范围重构
3. 向后兼容 - 配置文件格式保持不变
4. 可验证 - 每个修复都有明确的验收标准
```

---

## 2. 实际问题清单

### 2.1 安全隐患

| ID | 问题描述 | 位置 | 优先级 | 风险说明 |
|----|----------|------|--------|----------|
| SEC-001 | 自定义音频路径未验证 | `timer_audio.c:34-38` | P1 | 路径遍历风险 |
| SEC-002 | 配置文件路径拼接无长度检查 | `timer_config.c:17` | P2 | 极端情况下可能溢出 |

### 2.2 代码规范问题

| ID | 问题描述 | 位置 | 优先级 |
|----|----------|------|--------|
| CODE-001 | 魔法数字未定义常量 | `timer_ui.c:82-88` | P2 |
| CODE-002 | 关键函数缺少注释 | `timer_core.c`, `timer_ui.c` | P2 |
| CODE-003 | 部分局部变量未显式初始化 | `timer_dialogs.c` | P3 |

### 2.3 性能优化

| ID | 问题描述 | 位置 | 优先级 |
|----|----------|------|--------|
| PERF-001 | 缓存命中条件过严 | `timer_ui.c:74-78` | P2 |

---

## 3. 修复方案

### 3.1 SEC-001 - 自定义音频路径验证

**位置**: `timer_audio.c` - `PlayTimeoutAlert()` 函数

**当前代码**:
```c
// timer_audio.c:34-38
if (g_timerState.useCustomAudio && wcslen(g_timerState.customAudioPath) > 0) {
    DWORD fileAttributes = GetFileAttributesW(g_timerState.customAudioPath);
    if (fileAttributes != INVALID_FILE_ATTRIBUTES) {
        playedSuccessfully = PlaySoundW(g_timerState.customAudioPath, NULL, 
                                        SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}
```

**问题**: 未验证路径安全性，存在路径遍历风险（如 `../../../evil.exe`）

**修复方案**:

```c
// 在 timer_audio.h 中添加验证函数声明
static BOOL ValidateAudioPath(const wchar_t* path);

// 在 timer_audio.c 中实现
static BOOL ValidateAudioPath(const wchar_t* path) {
    if (!path || wcslen(path) == 0) {
        return FALSE;
    }
    
    // 检查路径长度
    if (wcslen(path) >= MAX_PATH) {
        return FALSE;
    }
    
    // 禁止路径遍历（检查连续的 ..）
    if (wcsstr(path, L"..") != NULL) {
        return FALSE;
    }
    
    // 检查文件扩展名（只允许音频格式）
    const wchar_t* ext = wcsrchr(path, L'.');
    if (!ext) {
        return FALSE;
    }
    
    // 只允许常见音频格式
    if (_wcsicmp(ext, L".wav") != 0 && 
        _wcsicmp(ext, L".mp3") != 0 &&
        _wcsicmp(ext, L".wma") != 0) {
        return FALSE;
    }
    
    // 检查文件是否存在且为文件（非目录）
    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        return FALSE;
    }
    
    return TRUE;
}

// 修改 PlayTimeoutAlert 中的调用
if (g_timerState.useCustomAudio && ValidateAudioPath(g_timerState.customAudioPath)) {
    // 安全播放
}
```

**验收标准**:
- [ ] 路径 `../../../evil.exe` 被拒绝
- [ ] 非音频文件（.exe, .bat 等）被拒绝
- [ ] 超长路径（>= MAX_PATH）被拒绝
- [ ] 正常音频文件路径（如 `C:\Music\alert.wav`）正常播放

---

### 3.2 SEC-002 - 配置路径拼接安全检查

**位置**: `timer_config.c` - `BuildConfigPath()` 函数

**当前代码**:
```c
// timer_config.c:6-18
static void BuildConfigPath(wchar_t* outPath, DWORD maxLen) {
    DWORD result = GetEnvironmentVariableW(L"USERPROFILE", outPath, maxLen);
    if (result == 0 || result >= maxLen) {
        if (FAILED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, outPath))) {
            GetCurrentDirectoryW(maxLen, outPath);
        }
    }
    size_t len = wcslen(outPath);
    if (len > 0 && outPath[len - 1] != L'\\') {
        wcscat_s(outPath, maxLen, L"\\");
    }
    wcscat_s(outPath, maxLen, CONFIG_FILE_PATH);  // CONFIG_FILE_PATH = "timer_config.ini"
}
```

**问题**: 未检查拼接后总长度，极端情况下（profile 路径接近 MAX_PATH）可能溢出

**修复方案**:

```c
static void BuildConfigPath(wchar_t* outPath, DWORD maxLen) {
    // 获取用户 profile 路径
    DWORD result = GetEnvironmentVariableW(L"USERPROFILE", outPath, maxLen);
    if (result == 0 || result >= maxLen) {
        if (FAILED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, outPath))) {
            GetCurrentDirectoryW(maxLen, outPath);
        }
    }
    
    // 计算拼接所需总长度
    size_t currentLen = wcslen(outPath);
    size_t requiredLen = currentLen + 1 + wcslen(CONFIG_FILE_PATH);  // 路径 + \ + 文件名
    
    // 检查是否会溢出
    if (requiredLen >= maxLen) {
        // 降级：使用当前目录
        GetCurrentDirectoryW(maxLen, outPath);
        currentLen = wcslen(outPath);
        requiredLen = currentLen + 1 + wcslen(CONFIG_FILE_PATH);
        
        if (requiredLen >= maxLen) {
            // 仍然不够，直接使用文件名（不安全的降级，但不会崩溃）
            wcscpy_s(outPath, maxLen, CONFIG_FILE_PATH);
            return;
        }
    }
    
    // 安全拼接
    if (currentLen > 0 && outPath[currentLen - 1] != L'\\') {
        wcscat_s(outPath, maxLen, L"\\");
    }
    wcscat_s(outPath, maxLen, CONFIG_FILE_PATH);
}
```

**验收标准**:
- [ ] 正常 profile 路径下配置文件正确生成
- [ ] 超长 profile 路径下程序不崩溃，使用降级方案

---

### 3.3 CODE-001 - 魔法数字消除

**位置**: `timer_ui.c` - `CalculateFontSize()` 函数

**当前代码**:
```c
// timer_ui.c:82-88
int availableWidth = width - 16;
int availableHeight = height - 16;

if (availableWidth <= 0) availableWidth = width - 6;
if (availableHeight <= 0) availableHeight = height - 6;
if (availableWidth <= 0) availableWidth = width;
if (availableHeight <= 0) availableHeight = height;
```

**修复方案**:

```c
// 在 timer_types.h 中添加常量定义
#define UI_TEXT_MARGIN          16    // 文本边距 (像素)
#define UI_SMALL_MARGIN         6     // 小边距 (像素)
#define UI_NO_MARGIN            0     // 无边距

// timer_ui.c:82-88 修改为
int availableWidth = width - UI_TEXT_MARGIN;
int availableHeight = height - UI_TEXT_MARGIN;

if (availableWidth <= 0) availableWidth = width - UI_SMALL_MARGIN;
if (availableHeight <= 0) availableHeight = height - UI_SMALL_MARGIN;
if (availableWidth <= 0) availableWidth = width - UI_NO_MARGIN;
if (availableHeight <= 0) availableHeight = height - UI_NO_MARGIN;
```

**验收标准**:
- [ ] 所有魔法数字都有命名常量定义
- [ ] 编译无警告
- [ ] 字体大小计算功能正常

---

### 3.4 CODE-002 - 关键函数注释完善

**位置**: `timer_ui.c`, `timer_core.c`

**修复方案**:

```c
/**
 * @brief 计算适合窗口大小的字体大小
 * 
 * 使用缓存机制避免重复计算。首次测量参考文本尺寸，
 * 之后通过线性公式计算目标字体大小。
 * 
 * @param width 窗口宽度 (像素)
 * @param height 窗口高度 (像素)
 * @param charCount 要显示的字符数
 * @return 计算出的字体大小 (像素)
 * 
 * @note 缓存命中条件：窗口尺寸和字符数均未变化
 * @note 字体大小限制范围：[16, 800]
 */
int CalculateFontSize(int width, int height, int charCount) {
    // ... 函数实现
}

/**
 * @brief 计时器心跳处理函数
 * 
 * 处理倒计时、超时判断、超时正计时等逻辑。
 * 根据 showMilliseconds 标志决定更新频率。
 * 
 * @param hwnd 主窗口句柄
 * 
 * @note 该函数在 WM_TIMER 消息中被调用
 * @note 超时正计时模式会改变字体和背景颜色
 */
void TimerTick(HWND hwnd) {
    // ... 函数实现
}
```

**验收标准**:
- [ ] 所有公共函数有 Doxygen 风格注释
- [ ] 复杂逻辑有行内注释

---

### 3.5 CODE-003 - 变量显式初始化

**位置**: `timer_dialogs.c` - 对话框函数

**当前代码**:
```c
// timer_dialogs.c 中部分局部变量未显式初始化
HFONT hFont;
int fontSize;
```

**修复方案**:

```c
// 修改为
HFONT hFont = NULL;
int fontSize = 0;
```

**验收标准**:
- [ ] 使用 `/W4` 编译警告级别无未初始化警告

---

### 3.6 PERF-001 - 缓存命中条件优化

**位置**: `timer_ui.c` - `CalculateFontSize()` 函数

**当前代码**:
```c
// timer_ui.c:74-78 - 严格匹配
if (width == g_timerState.cachedCalcWidth
 && height == g_timerState.cachedCalcHeight
 && charCount == g_timerState.cachedCharCount
 && g_timerState.cachedFontSize > 0) {
    return g_timerState.cachedFontSize;
}
```

**问题**: 窗口拖拽时像素级微小变化导致缓存失效，重复计算

**修复方案**:

```c
// 在 timer_types.h 中添加
#define CACHE_TOLERANCE 2  // 缓存容差 (像素)

// timer_ui.c:74-78 修改为
if (abs(width - g_timerState.cachedCalcWidth) <= CACHE_TOLERANCE
 && abs(height - g_timerState.cachedCalcHeight) <= CACHE_TOLERANCE
 && charCount == g_timerState.cachedCharCount
 && g_timerState.cachedFontSize > 0) {
    return g_timerState.cachedFontSize;
}
```

**验收标准**:
- [ ] 窗口拖拽时 CPU 占用降低
- [ ] 字体大小变化无明显视觉跳跃

---

## 4. 实施计划

### 4.1 实施阶段

| 阶段 | 时间 | 任务 | 交付物 |
|------|------|------|--------|
| 第一阶段 | 第 1 周 | 修复安全隐患 | SEC-001, SEC-002 完成 |
| 第二阶段 | 第 2 周 | 代码规范改进 | CODE-001~003 完成 |
| 第三阶段 | 第 3 周 | 性能优化 | PERF-001 完成 |

### 4.2 改动文件清单

| 文件 | 改动内容 |
|------|----------|
| `timer_types.h` | 添加常量定义 `UI_TEXT_MARGIN`, `CACHE_TOLERANCE` |
| `timer_audio.c` | 添加 `ValidateAudioPath()` 函数 |
| `timer_config.c` | 修改 `BuildConfigPath()` 增加长度检查 |
| `timer_ui.c` | 魔法数字替换、缓存优化、添加注释 |

---

## 5. 验收标准

### 5.1 整体验收

- [ ] 所有 P1/P2 问题已修复
- [ ] 程序无崩溃、无内存泄漏
- [ ] 所有功能回归测试通过

### 5.2 安全测试

| 测试项 | 测试方法 | 预期结果 |
|--------|----------|----------|
| 路径遍历攻击 | 设置自定义音频路径为 `../../../evil.exe` | 被拒绝 |
| 非音频文件 | 设置自定义音频路径为 `malware.bat` | 被拒绝 |
| 超长路径 | 设置自定义音频路径为 300+ 字符 | 被拒绝 |
| 配置文件溢出 | 设置 USERPROFILE 为超长路径 | 程序不崩溃，使用降级方案 |

### 5.3 功能回归测试

| 测试项 | 测试方法 | 预期结果 |
|--------|----------|----------|
| 倒计时功能 | 设置时间并运行 | 时间准确 |
| 超时提醒 | 等待倒计时结束 | 音频正常播放 |
| 窗口调整 | 拖拽调整窗口大小 | 流畅无卡顿 |
| 配置保存/加载 | 修改设置后重启 | 设置保持 |
| 自定义音频 | 导入合法音频文件 | 正常播放 |

---

## 附录：原 v1.0 PRD 误判说明

### 误判问题列表

| 原 ID | 问题描述 | 误判原因 |
|-------|----------|----------|
| BUG-001 | 缓冲区溢出 | `wcscat_s` 已提供边界保护 |
| BUG-002 | 空指针解引用 | 代码已有 `if (refFont)` 检查 |
| BUG-003 | SetWindowPos 标志 | 标志组合是 Windows API 标准用法 |
| BUG-004 | 未初始化变量 | static 变量自动初始化为 0/NULL |

### 教训总结

1. **QA 报告必须基于真实工具扫描**，而非主观判断
2. **修复方案必须对照实际代码**，而非假设的代码
3. **Windows API 用法应参考官方文档**，而非猜测

---

**文档结束**

*本 PRD 文档为 Timmmer 项目代码修复与改进计划，开发团队应参照此文档进行修复工作。*
