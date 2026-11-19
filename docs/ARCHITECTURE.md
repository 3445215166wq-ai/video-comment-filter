# 🏗️ 应用架构重构指南

## 概述

将单体的 `main.cpp` 重构为**微内核架构**，实现 UI 与业务逻辑的完全解耦。

```
┌─────────────────────────────────────┐
│   UI Layer                          │
│  ┌─────────────────────────────────┐│
│  │ ConsoleUI / WPF UI / Qt UI  │  │  ← 多个 UI 实现
│  └─────────────────┬───────────────┘│
└────────────────────┼────────────────┘
                     │ implements
┌────────────────────▼────────────────┐
│ IApplicationController (interface)  │  ← 统一的业务接口
├─────────────────────────────────────┤
│ ApplicationService                  │  ← 业务编排
│  ├─ StateManager                    │
│  ├─ ConfigurationManager            │
│  └─ FilterEngine                    │
├─────────────────────────────────────┤
│ Domain Layer                        │
│  ├─ BusinessOps (business logic)   │
│  ├─ Similarity Policy              │
│  └─ Filter Engine                  │
└─────────────────────────────────────┘
```

---

## 核心概念

### 1. **IApplicationController** - 业务接口契约
所有 UI 必须通过此接口与业务交互。

```cpp
class IApplicationController {
    // 查询
    FilterState GetCurrentState();

    // 命令
    void SetFilterMode(int mode);
    void StartFilteringLoop();
    void StopFilteringLoop();

    // 事件订阅
    void SubscribeStateChange(Callback cb);
    void SubscribeMessage(Callback cb);
};
```

**优势**：
- UI 与业务彻底解耦
- 可以有多个 UI 实现（Console、WPF、Qt、Web）
- 业务逻辑独立演进

### 2. **StateManager** - 观察者模式的状态管理
取代全局的 `FilterConfig` 原子变量。

```cpp
class StateManager {
    // 写操作自动通知订阅者
    void SetMode(int mode);
    void SetPositiveThreshold(float value);

    // 读操作
    FilterState GetState();

    // 观察者注册
    void Subscribe(Callback cb);
};
```

**优势**：
- 状态变化自动通知所有观察者
- UI 可以实时反应状态变化（如禁用/启用按钮）
- 类型安全，避免原子操作的复杂性

### 3. **ConfigurationManager** - 配置持久化
独立管理配置的序列化/反序列化。

```cpp
class ConfigurationManager {
    FilterState LoadConfig();
    void SaveConfig(const FilterState& state);
};
```

**优势**：
- 配置管理逻辑集中
- 易于支持多格式（JSON、XML、INI）
- 易于测试

### 4. **FilterEngine** - 核心业务逻辑
封装抓取→清洗→相似度计算→决策的全流程。

```cpp
class FilterEngine {
    Result ExecuteFilterCycle(
        HWND target_window,
        int mode,
        float positive_threshold,
        float negative_threshold,
        bool record_data
    );
};
```

**优势**：
- 业务逻辑独立于 UI
- 易于单元测试
- 易于移植到其他平台

### 5. **ApplicationService** - 业务编排
实现 `IApplicationController`，协调各组件。

```cpp
class ApplicationService : public IApplicationController {
    std::unique_ptr<StateManager> state_;
    std::unique_ptr<ConfigurationManager> config_;
    std::unique_ptr<FilterEngine> engine_;

    void StartFilteringLoop() override {
        // 创建后台线程，执行 FilterEngine
        // 订阅状态变化，自动保存配置
    }
};
```

---

## 使用流程

### 场景 1：用户按下"启动筛选"按钮

**Console UI：**
```cpp
ConsoleUI::ProcessCommand("1") {
    controller_->StartFilteringLoop();
}
```

**ApplicationService：**
```cpp
void StartFilteringLoop() {
    state_->SetRunning(true);  // 状态改变，通知观察者
    filter_thread_ = std::thread([this] { BackgroundFilterLoop(); });
}

void BackgroundFilterLoop() {
    while (loop_active_) {
        auto result = engine_->ExecuteFilterCycle(...);
        if (result.decision == 0) break;  // 匹配成功，停止
    }
}
```

**StateManager：**
```cpp
void SetRunning(bool running) {
    is_running_ = running;
    NotifySubscribers();  // 通知所有观察者
}
```

**ConsoleUI（订阅者）：**
```cpp
controller_->SubscribeStateChange([this](const FilterState& state) {
    // UI 可以在这里更新显示
    // 例如禁用启动按钮、显示进度条等
});
```

### 场景 2：用户调整阈值

```cpp
ConsoleUI::ProcessCommand("6") {
    controller_->AdjustPositiveThreshold(0.05f);
}

ApplicationService::AdjustPositiveThreshold(0.05f) {
    state_->SetPositiveThreshold(current + 0.05f);
}

StateManager::SetPositiveThreshold(...) {
    positive_threshold_ = value;
    NotifySubscribers();  // 通知 UI 和其他观察者

    // 自动触发 ApplicationService 订阅者
    config_->SaveConfig(state);  // 自动保存
}
```

---

## 扩展 UI

添加新 UI（如 WPF）非常简单：

### 步骤 1：创建 WPF UI 适配器

```cpp
// src/ui/WpfUI.h
class WpfUI {
    std::shared_ptr<IApplicationController> controller_;

public:
    WpfUI(std::shared_ptr<IApplicationController> controller);

    void OnStartButtonClicked() {
        controller_->StartFilteringLoop();
    }

    void Initialize() {
        controller_->SubscribeStateChange([this](const FilterState& state) {
            UpdateUI(state);  // 更新 WPF 控件
        });
    }

private:
    void UpdateUI(const FilterState& state);
};
```

### 步骤 2：启用 WPF 的 main

```cpp
// src/main_wpf.cpp
int WINAPI wWinMain(...) {
    auto controller = std::make_shared<ApplicationService>(exe_dir, data_dir);

    WpfUI ui(controller);
    ui.Initialize();
    ui.Run();

    return 0;
}
```

**就这么简单！** 不需要修改任何业务逻辑。

---

## 迁移清单

### Phase 1: 创建应用层（已完成）
- [x] `IApplicationController` 接口
- [x] `StateManager` 状态管理
- [x] `ConfigurationManager` 配置管理
- [x] `ApplicationService` 业务编排

### Phase 2: 迁移业务逻辑
- [ ] 实现 `FilterEngine::ExecuteFilterCycle()`
  - 复用现有的 `main_loop()` 逻辑
  - 去掉控制台输出，改为通过 `NotifyMessage()` 发送事件

- [ ] 实现 `FilterEngine::Initialize()`
  - 加载 ONNX 模型

- [ ] 实现 `ApplicationService::BackgroundFilterLoop()`
  - 调用 `FilterEngine`
  - 处理异常和中断

### Phase 3: 创建新 UI 适配器
- [ ] `ConsoleUI` 独立适配器（基于现有代码）
- [ ] `WpfUI` WPF 适配器（可选）
- [ ] `RestAPI` REST 服务（可选）

### Phase 4: 切换主程序
- [ ] 保留旧的 `main.cpp`
- [ ] 创建新的 `main_refactored.cpp`
- [ ] 测试新架构
- [ ] 逐步迁移

---

## 最佳实践

### 1. 单一职责原则
```cpp
❌ 错误：ApplicationService 既处理 UI，又处理业务逻辑
✅ 正确：
  - ApplicationService：只处理业务编排
  - ConsoleUI：只处理 UI 交互
  - StateManager：只管理状态
  - FilterEngine：只执行过滤逻辑
```

### 2. 依赖注入
```cpp
❌ 错误：在 ApplicationService 内部创建 FilterEngine
✅ 正确：在构造函数注入
class ApplicationService {
    std::unique_ptr<FilterEngine> engine_;
    ApplicationService(std::unique_ptr<FilterEngine> engine) : engine_(std::move(engine)) {}
};
```

### 3. 观察者模式
```cpp
❌ 错误：UI 通过全局变量访问状态
✅ 正确：UI 通过回调订阅状态变化
controller_->SubscribeStateChange([this](const FilterState& state) {
    UpdateUI(state);
});
```

### 4. 错误处理
```cpp
✅ 好的做法：
- `FilterEngine::Result` 包含 `success` 和 `message`
- 通过 `NotifyMessage()` 传送错误给 UI
- UI 决定如何显示错误（对话框、日志、通知等）
```

---

## 测试策略

### 单元测试示例

```cpp
// test/ApplicationServiceTest.cpp
TEST(ApplicationService, SetFilterModeNotifiesSubscribers) {
    auto controller = std::make_shared<ApplicationService>(exe_dir, data_dir);

    FilterState captured_state;
    controller->SubscribeStateChange([&](const FilterState& state) {
        captured_state = state;
    });

    controller->SetFilterMode(1);  // Positive

    ASSERT_EQ(captured_state.mode, 1);
}

TEST(ApplicationService, ConfigSavedOnStateChange) {
    auto controller = std::make_shared<ApplicationService>(exe_dir, data_dir);

    controller->AdjustPositiveThreshold(0.1f);

    // 重新加载，验证配置已保存
    auto controller2 = std::make_shared<ApplicationService>(exe_dir, data_dir);
    ASSERT_EQ(controller2->GetCurrentState().positive_threshold, 0.75f);
}
```

---

## 性能考虑

1. **线程安全**：`StateManager` 使用 `std::mutex` 保护订阅者列表
2. **回调性能**：考虑批量通知或节流，避免高频更新
3. **内存**：`std::shared_ptr` 会增加开销，但避免了管理复杂性

---

## 总结

通过此架构，你获得：

| 特性 | 好处 |
|------|------|
| 解耦 UI 与业务 | 业务逻辑可独立演进 |
| 多 UI 支持 | Console、WPF、Qt、Web 共享同一逻辑 |
| 易于测试 | 业务逻辑不依赖 UI |
| 易于扩展 | 添加新功能无需修改既有代码 |
| 响应式 UI | 通过观察者模式实时更新 |
| 配置持久化 | 自动保存/加载，易于扩展 |

---

## 下一步

1. 实现 `FilterEngine` 核心逻辑
2. 创建独立的 `ConsoleUI` 适配器
3. 编写单元测试
4. 考虑添加 WPF UI
5. 逐步迁移现有代码
