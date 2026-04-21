# PRD: 窗口拖拽调整大小时卡顿优化

## 1. 背景与问题描述

### 1.1 用户反馈

用户在拖动窗口边缘调整大小时，界面出现明显卡顿（"一卡一卡的"），体验不流畅。

### 1.2 问题根因（已确认）

经代码分析确认，卡顿的核心原因是 **`CalculateFontSize` 在每次重绘时都执行完整的二分查找**。

完整调用链如下：

```
用户拖动边缘
  → WM_LBUTTONDOWN (timer_window.c:416) 检测边缘
  → WM_MOUSEMOVE (timer_window.c:435) 计算delta → SetWindowPos()
  → 系统发送 WM_SIZE
  → WM_SIZE (timer_window.c:696) 更新宽高 → InvalidateRect()
  → WM_PAINT (timer_window.c:371)
      → CreateCompatibleDC + CreateCompatibleBitmap  (双缓冲)
      → UpdateDisplay (timer_ui.c:10)
          → CalculateFontSize (timer_ui.c:72)  ← 瓶颈所在
              → 二分查找 16~800 (≈10次迭代)
              → 每次迭代: GetMonospaceFont() + SelectObject() + GetTextExtentPointA() + SelectObject()
      → BitBlt → 清理GDI资源
```

**每次鼠标移动事件都触发上述完整流程，无任何节流或缓存。**

### 1.3 性能瓶颈分析

| 瓶颈 | 严重程度 | 位置 | 说明 |
|------|----------|------|------|
| CalculateFontSize 无缓存执行二分查找 | **高** | timer_ui.c:72-147 | 每次WM_PAINT都完整执行，创建字体+测量文本约10次 |
| WM_ERASEBKGND 未拦截 | **低** | timer_window.c WindowProc | 双缓冲下背景擦除是多余操作，造成额外GDI开销 |
| 拖拽过程无节流 | **中** | timer_window.c:439-467 | 每个WM_MOUSEMOVE都触发完整重绘流水线 |

### 1.4 已排除的伪优化项

以下项目在方案初稿中被提出，经审视后**不应执行**：

| 项目 | 排除理由 |
|------|----------|
| 缓存双缓冲DC/Bitmap | GDI资源有限不应长期持有；标准做法是在WM_PAINT中临时创建；相比字体计算其开销微不足道；增加资源泄漏风险 |
| 额外缓存HFONT到TimerState | `timer_fonts.h` 已有 FontManager 缓存系统（FontCache[16]），`GetTimerFont`/`GetMonospaceFont` 已实现懒加载+缓存，重复缓存无意义 |
| WM_EXITSIZEMOVE 处理 | 该消息仅在系统原生边框调整时发送；本项目使用自绘边缘拖拽（WM_LBUTTONDOWN检测→WM_MOUSEMOVE手动SetWindowPos），此消息不会被触发 |

---

## 2. 优化目标

| 指标 | 优化前 | 目标 |
|------|--------|------|
| 拖拽调整大小流畅度 | 明显卡顿 | 与系统原生窗口调整体验一致 |
| CalculateFontSize 调用频率 | 每次WM_PAINT | 仅在窗口尺寸或字符数变化时 |
| WM_MOUSEMOVE重绘频率（非透明模式） | 每个事件 | 节流至≤33ms间隔（约30fps） |

---

## 3. 透明背景模式约束（关键）

### 3.1 透明背景的工作原理

透明模式使用 Windows 色键（Color Key）透明：

```
SetLayeredWindowAttributes(hwnd, RGB(1,1,1), 255, LWA_COLORKEY)
                                  ↑                    ↑
                          色键：RGB(1,1,1)        透明方式：色键模式

WM_PAINT → FillRect(RGB(1,1,1))  →  BitBlt到屏幕  →  系统把RGB(1,1,1)像素抠掉变透明
           文字用NONANTIALIASED_QUALITY渲染（避免ClearType边缘色与色键混合）
```

**色键要求每一帧的背景像素必须精确为 RGB(1,1,1)**，否则：
- 非色键像素不会被系统抠掉 → 出现可见的色块/残影
- 节流期间新暴露的窗口区域如果未被填充 → 透失败

### 3.2 对各优化项的约束

| 优化项 | 透明模式下的约束 |
|--------|-----------------|
| 缓存 CalculateFontSize | 无影响。输出是 fontSize 数值，与渲染质量无关 |
| 拦截 WM_ERASEBKGND | 必须保证双缓冲 FillRect 覆盖整个区域（当前已满足） |
| 拖拽节流 | **透明模式下禁止节流**。节流会导致新暴露区域在间隔期内未被填充 RGB(1,1,1)，产生可见色块 |

### 3.3 技术债记录（不在本次范围）

`FontManager` 的缓存key为 `(fontName, size, bold, italic)`，不包含 `fontQuality`。`CreateFontInternal`（timer_fonts.c:400）根据 `transparentBackground` 选择不同的字体渲染质量（NONANTIALIASED vs CLEARTYPE），但缓存不区分这两种质量。这意味着在透明/非透明模式切换后，可能拿到旧渲染质量的字体。这是**原有bug**，非本次优化引入，记录待后续修复。

---

## 4. 技术方案

### 4.1 优化项一：缓存 CalculateFontSize 计算结果（高优先级）

**问题**：每次 `WM_PAINT` → `UpdateDisplay` → `CalculateFontSize` 都执行完整二分查找。

**方案**：在 `TimerState` 中添加缓存字段，仅当输入参数变化时才重新计算。

#### 4.1.1 修改文件

**`timer_types.h`** — TimerState 结构体（第200行附近）添加字段：

```c
// 字体大小计算缓存
int cachedFontSize;         // 缓存的字体大小结果
int cachedCalcWidth;        // 缓存对应的窗口宽度
int cachedCalcHeight;       // 缓存对应的窗口高度
int cachedCharCount;        // 缓存对应的字符数
```

**`timer_ui.c`** — `CalculateFontSize` 函数（第72行）开头添加缓存命中判断：

```c
int CalculateFontSize(int width, int height, int charCount) {
    // 缓存命中：参数未变化则直接返回
    if (width == g_timerState.cachedCalcWidth
     && height == g_timerState.cachedCalcHeight
     && charCount == g_timerState.cachedCharCount
     && g_timerState.cachedFontSize > 0) {
        return g_timerState.cachedFontSize;
    }

    // ... 原有二分查找逻辑不变 ...

    // 在 return bestSize 之前保存缓存
    g_timerState.cachedCalcWidth = width;
    g_timerState.cachedCalcHeight = height;
    g_timerState.cachedCharCount = charCount;
    g_timerState.cachedFontSize = bestSize;
    return bestSize;
}
```

#### 4.1.2 缓存失效时机

缓存自然失效——只要 `width`、`height`、`charCount` 任一变化，缓存即不命中，自动触发重新计算。无需手动管理失效。

#### 4.1.3 初始化

`TimerState` 为全局变量，零初始化时 `cachedFontSize = 0`，首次调用必然不命中（条件 `cachedFontSize > 0` 不满足），无需特殊初始化代码。

---

### 4.2 优化项二：拦截 WM_ERASEBKGND（低优先级，一行代码）

**问题**：使用双缓冲时，系统默认的 `WM_ERASEBKGND` 处理会先擦除背景，然后双缓冲再画一遍，造成一次多余的GDI操作。

**方案**：在 `WindowProc` 的 `switch` 中添加 `WM_ERASEBKGND` 处理，直接返回 `TRUE` 告知系统背景已处理。

#### 4.2.1 修改文件

**`timer_window.c`** — `WindowProc` 函数中，在 `WM_PAINT` case 之前添加：

```c
case WM_ERASEBKGND:
    return TRUE;
```

#### 4.2.2 影响分析

- 双缓冲模式下，背景在内存DC中绘制后才 `BitBlt` 到屏幕，拦截 `WM_ERASEBKGND` 不影响视觉输出。
- 透明模式下同样安全：双缓冲的 `FillRect(RGB(1,1,1))` 保证了色键覆盖。
- 这是Win32双缓冲的标准优化手法，无副作用。

---

### 4.3 优化项三：拖拽调整大小过程节流（中优先级，仅非透明模式）

**问题**：自绘边缘拖拽中，每个 `WM_MOUSEMOVE` 都调用 `SetWindowPos` → `WM_SIZE` → `InvalidateRect` → 完整重绘。鼠标移动事件频率可达 125-1000Hz，远超渲染所需。

**方案**：在 `WM_SIZE` 中对非透明模式下的拖拽重绘做节流（33ms间隔），透明模式下不节流以保证色键正确。

#### 4.3.1 修改文件

**`timer_types.h`** — TimerState 结构体添加字段：

```c
DWORD lastResizeRedrawTime;  // 上次拖拽重绘的时间戳（GetTickCount64）
BOOL needsFinalRedraw;       // 拖拽结束后是否需要补偿一次完整重绘
```

**`timer_window.c`** — `WM_SIZE` 处理（第696-718行）修改为：

```c
case WM_SIZE: {
    g_timerState.windowWidth = LOWORD(lParam);
    g_timerState.windowHeight = HIWORD(lParam);

    // 限制最小尺寸
    if (g_timerState.windowWidth < 50) g_timerState.windowWidth = 50;
    if (g_timerState.windowHeight < 25) g_timerState.windowHeight = 25;

    // 装饰效果更新（原有逻辑不变）
    if (!g_timerState.transparentBackground) {
        SetWindowRoundedCorners(hwnd, 6);
        SetWindowShadow(hwnd, TRUE);
    } else {
        SetWindowRoundedCorners(hwnd, 0);
        SetWindowShadow(hwnd, FALSE);
    }

    // 节流：仅非透明模式下的拖拽期间限制重绘频率
    // 透明模式禁止节流——节流期间新暴露区域未填充RGB(1,1,1)会导致色键失效
    if (g_timerState.isResizing && !g_timerState.transparentBackground) {
        DWORD now = (DWORD)GetTickCount64();
        if (now - g_timerState.lastResizeRedrawTime < 33) {
            // 仅跳过InvalidateRect，宽高和装饰效果已在上方正常更新
            return 0;
        }
        g_timerState.lastResizeRedrawTime = now;
    }

    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
}
```

**注意**：节流时的 `return 0` 之前，`windowWidth/Height` 和装饰效果已经正常更新完成，所以不影响状态一致性。跳过的仅仅是 `InvalidateRect`。

**`timer_window.c`** — `WM_LBUTTONUP`（第488-496行）补偿最终重绘：

```c
case WM_LBUTTONUP: {
    if (g_timerState.isDragging || g_timerState.isResizing) {
        g_timerState.isDragging = FALSE;
        g_timerState.isResizing = FALSE;
        g_timerState.resizeEdge = 0;
        ReleaseCapture();

        // 拖拽结束后补偿一次完整重绘（确保最终尺寸精确渲染）
        if (g_timerState.needsFinalRedraw) {
            g_timerState.needsFinalRedraw = FALSE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    return 0;
}
```

**`timer_window.c`** — `WM_MOUSEMOVE` isResizing 分支中标记需要补偿重绘：

在 `SetWindowPos` 调用之后添加：
```c
g_timerState.needsFinalRedraw = TRUE;
```

#### 4.3.2 关键设计决策

- **节流仅作用于 InvalidateRect**：`windowWidth/Height` 更新和装饰效果每次都执行，保证状态一致性。
- **透明模式下不节流**：色键要求每帧都填充 RGB(1,1,1)，节流会导致新暴露区域透失败。透明模式的性能改善仅依赖优化项一（缓存字体计算）。
- **始终更新 lastMousePos**：WM_MOUSEMOVE 中即使 WM_SIZE 跳过了重绘，鼠标位置仍然更新，否则下次 delta 会错误累积。
- **mouseup 时补偿重绘**：确保用户松手后看到的是精确的最终渲染结果。

---

## 5. 修改清单汇总

| 文件 | 修改内容 | 涉及行数 |
|------|----------|----------|
| `timer_types.h` | 添加6个缓存/节流字段到 TimerState | 第250行之后 |
| `timer_ui.c` | CalculateFontSize 添加缓存命中判断和保存 | 第72-147行函数内 |
| `timer_window.c` | 添加 WM_ERASEBKGND 拦截 | WindowProc switch 内 |
| `timer_window.c` | WM_SIZE 添加非透明模式节流逻辑 | 第696-718行 |
| `timer_window.c` | WM_LBUTTONUP 添加补偿重绘 | 第488-496行 |
| `timer_window.c` | WM_MOUSEMOVE isResizing 分支标记 needsFinalRedraw | 第465行附近 |

**总修改量**：约35行新增/修改代码，0个新文件。

---

## 6. 风险评估

| 风险 | 级别 | 缓解措施 |
|------|------|----------|
| 缓存字段零初始化导致首次渲染异常 | 极低 | cachedFontSize > 0 条件保证首次不命中 |
| 节流导致拖拽过程中文字显示尺寸滞后 | 低 | 33ms间隔≈30fps，人眼基本无感知；松手后立即补偿精确渲染 |
| WM_ERASEBKGND拦截影响非双缓冲场景 | 无 | 本项目始终使用双缓冲，不存在非双缓冲渲染路径 |
| 时间格式切换（如启用/禁用毫秒）导致缓存不更新 | 低 | charCount变化时缓存自然失效 |
| 透明模式下节流导致色键失效 | 已消除 | 透明模式下跳过节流，每帧都重绘 |
| 节流return 0跳过宽高更新导致状态不一致 | 已消除 | 节流判断在宽高更新和装饰效果更新之后，仅跳过InvalidateRect |
| FontManager缓存不含fontQuality导致模式切换后渲染质量错误 | 低（技术债） | 原有bug，非本次优化引入。CalculateFontSize返回fontSize数值不含渲染质量信息，优化项一不加剧此问题 |

---

## 7. 验证方案

### 7.1 功能验证

- [ ] 拖拽边缘调整窗口大小（非透明模式）：流畅无卡顿
- [ ] 拖拽边缘调整窗口大小（透明模式）：流畅无卡顿，无色块/残影
- [ ] 滚轮缩放窗口：正常工作
- [ ] 双击切换预设时间：显示正确
- [ ] 透明背景模式切换：显示正常
- [ ] 透明模式下拖拽调整大小：无色键失效
- [ ] 超时正计时模式：颜色切换正常
- [ ] 毫秒显示模式：数字更新正常（50ms计时器）
- [ ] 启用/禁用毫秒后窗口大小调整：字号正确重算

### 7.2 性能验证

- [ ] 非透明模式拖拽调整大小时CPU占用明显降低
- [ ] 透明模式拖拽调整大小时CPU占用降低（依赖缓存优化）
- [ ] 拖拽松手后最终渲染精确无残影
- [ ] 计时器运行期间调整大小不影响计时精度
