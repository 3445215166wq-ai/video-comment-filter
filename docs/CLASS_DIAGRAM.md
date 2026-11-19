# 🎨 类关系与数据流图

## 类图

```
┌─────────────────────────────────────────────────┐
│       <<interface>>                             │
│    IApplicationController                       │
├─────────────────────────────────────────────────┤
│ + GetCurrentState(): FilterState                │
│ + SetFilterMode(int): void                      │
│ + AdjustPositiveThreshold(float): void          │
│ + AdjustNegativeThreshold(float): void          │
│ + SetRecordData(bool): void                     │
│ + StartFilteringLoop(): void                    │
│ + StopFilteringLoop(): void                     │
│ + SubscribeStateChange(Callback): void          │
│ + SubscribeMessage(Callback): void              │
└──────────────────────▲─────────────────────────┘
                       │ implements
         ┌─────────────┴──────────────┐
         │                            │
    ┌────────────────────────────────────────────────┐
    │      ApplicationService                        │
    ├────────────────────────────────────────────────┤
    │ - state_: StateManager                         │
    │ - config_: ConfigurationManager                │
    │ - engine_: FilterEngine                        │
    │ - filter_thread_: std::thread                  │
    ├────────────────────────────────────────────────┤
    │ + Initialize(): bool                           │
    │ + StartFilteringLoop(): void                   │
    │ + BackgroundFilterLoop(): void (private)       │
    │ + NotifyStateChange(): void (private)          │
    │ + NotifyMessage(): void (private)              │
    └────────────────────────────────────────────────┘
                │         │         │
       ┌────────┴────┬────┴────┬────┴────────┐
       │             │         │             │
    ┌──▼──────────────┐ ┌──────▼──────┐ ┌────▼─────────────┐
    │ StateManager    │ │Configuration│ │ FilterEngine    │
    │                 │ │  Manager    │ │                 │
    ├─────────────────┤ ├─────────────┤ ├─────────────────┤
    │ - mode_         │ │ - data_dir_ │ │ - initialized_  │
    │ - thresholds_   │ ├─────────────┤ ├─────────────────┤
    │ - subscribers_  │ │ + Load()    │ │ + Initialize()  │
    ├─────────────────┤ │ + Save()    │ │ + Execute()     │
    │ + SetMode()     │ └─────────────┘ │ + Shutdown()    │
    │ + SetThreshold()│                 └─────────────────┘
    │ + Subscribe()   │
    │ + Notify()      │
    └─────────────────┘
          │
          ▼
    ┌──────────────────┐
    │ FilterState      │
    ├──────────────────┤
    │ + mode: int      │
    │ + thresholds: f  │
    │ + is_running: b  │
    │ + record_data: b │
    └──────────────────┘


┌──────────────────────────────────────────────────┐
│           ConsoleUI                              │
├──────────────────────────────────────────────────┤
│ - controller_: IApplicationController            │
├──────────────────────────────────────────────────┤
│ + Run(): void                                    │
│ + ProcessCommand(string): void                   │
│ + OnStateChanged(FilterState): void (private)    │
│ + OnMessageReceived(string): void (private)      │
└──────────────────────▲─────────────────────────┘
                       │ uses
                       │
         [可以替换为 WpfUI / QtUI / RestAPI / ...]
```

---

## 数据流图

### 场景 1: 用户启动筛选

```
ConsoleUI                  ApplicationService          StateManager
    │                              │                         │
    │ StartFilteringLoop()         │                         │
    ├─────────────────────────────>│                         │
    │                              │ SetRunning(true)        │
    │                              ├────────────────────────>│
    │                              │                         │ SetRunning()
    │                              │                         │ NotifySubscribers()
    │                              │                         │
    │<─ SubscribeStateChange ──────┼─────────────────────────┤
    │                              │<── FilterState ─────────┤
    │ OnStateChange()              │
    │ UpdateUI()                   │
    │                              │ Create filter_thread_
    │                              │ BackgroundFilterLoop()
    │                              │         │
    │                              │         ▼
    │                              │ FilterEngine
    │                              │ ExecuteFilterCycle()
    │                              │         │
    │                              │         ├─> BusinessOps::CopyContent()
    │                              │         ├─> clean_douyin_text()
    │                              │         ├─> parse_to_json()
    │                              │         ├─> Encode()
    │                              │         └─> Evaluate()
    │                              │
    │<───── NotifyMessage ─────────┤
    │ OnMessage()                  │
    │ DisplayStatus()              │
    │                              │
```

### 场景 2: 用户调整阈值

```
ConsoleUI                  ApplicationService         StateManager       ConfigurationManager
    │                              │                        │                    │
    │ AdjustPositiveThreshold()    │                        │                    │
    ├─────────────────────────────>│                        │                    │
    │                              │ SetPositiveThreshold() │                    │
    │                              ├───────────────────────>│                    │
    │                              │                        │ NotifySubscribers()│
    │                              │<──── FilterState ──────┤                    │
    │<────────────────────────────────── OnStateChanged ────┤                    │
    │ UpdateUI()                   │                        │ SaveConfig()       │
    │                              │                        ├───────────────────>│
    │                              │                        │                    │ WriteJSON()
    │                              │                        │                    │ Save to Disk
```

### 场景 3: 应用启动加载配置

```
main()
  │
  ├─> ApplicationService(exe_dir, data_dir)
  │   │
  │   ├─> Initialize()
  │   │   │
  │   │   ├─> ConfigurationManager::LoadConfig()
  │   │   │   ├─> ReadJSON from Disk
  │   │   │   └─> return FilterState
  │   │   │
  │   │   ├─> StateManager::SetMode() / SetThreshold()
  │   │   │   └─> NotifySubscribers()
  │   │   │
  │   │   ├─> FilterEngine::Initialize()
  │   │   │   └─> load_onnx_model()
  │   │   │
  │   │   └─> Subscribe StateManager changes
  │   │       └─> config_->SaveConfig() on each change
  │   │
  │   └─> return shared_ptr<ApplicationService>
  │
  └─> ConsoleUI(controller)
      └─> Run()
          └─> SubscribeStateChange()
              └─> DisplayCurrentState()
```

---

## 事件流时序图

```
时间轴 ►
│
├─ User Input ─────────────────────────────────────────┐
│                                                      │
│                                                      ▼
├─ ConsoleUI::ProcessCommand("6")                      │
│  │                                                   │
│  └─> controller_->AdjustPositiveThreshold(0.05f)    │
│                                                      │
│                    ┌─────────────────────────────────┘
│                    │
│                    ▼
├─ ApplicationService::AdjustPositiveThreshold()
│  │
│  └─> state_->SetPositiveThreshold(current + 0.05f)
│
│                    ┌────────────────────────────────────┐
│                    │                                    │
│                    ▼                                    │
├─ StateManager::SetPositiveThreshold()                 │
│  │                                                     │
│  ├─> positive_threshold_ = value                      │
│  │                                                     │
│  └─> NotifySubscribers()  ◄───┐                       │
│      │                         │                       │
│      │ (loop through all)       │                       │
│      │                          │                       │
│      ├─> callback(FilterState)  │                       │
│      │   │                       │                       │
│      │   ├─> ApplicationService subscriber              │
│      │   │   └─> config_->SaveConfig()                 │
│      │   │       └─> WriteJSON()                       │
│      │   │                                              │
│      │   └─> ConsoleUI subscriber                       │
│      │       └─> OnStateChange()                        │
│      │           └─> UpdateDisplay()                    │
│      │                                                  │
│      └─> return to Loop                                │
│                                                        │
└──────────────────────────────────────────────────────┘
```

---

## 对象生命周期

### 启动流程

```
main()
  │
  ├─ Create ApplicationService
  │  │
  │  ├─ Create StateManager (empty subscribers list)
  │  ├─ Create ConfigurationManager
  │  └─ Create FilterEngine
  │
  ├─ service->Initialize()
  │  │
  │  ├─ config->LoadConfig() from disk
  │  ├─ state->SetMode/SetThreshold (from loaded config)
  │  │  └─ NotifySubscribers (but list is empty at this point)
  │  │
  │  └─ service->Subscribe(auto-save callback)
  │     └─ Now config changes will auto-save
  │
  ├─ Create ConsoleUI(service)
  │  └─ UI holds weak reference to service
  │
  └─ ui->Run()
     │
     └─ ui->SubscribeStateChange(callback)
        └─ Now UI receives state changes
```

### 运行流程

```
ConsoleUI::Run()
  │
  └─ Loop
     │
     ├─ Display menu
     │
     ├─ Read user input
     │
     ├─ controller->Command()
     │  │
     │  ├─ UpdateState (via StateManager)
     │  │  │
     │  │  └─> NotifyAllSubscribers()
     │  │     ├─> Config subscriber: SaveConfig()
     │  │     └─> UI subscriber: UpdateDisplay()
     │  │
     │  └─ if StartFiltering:
     │     └─> Create filter_thread_
     │        └─ BackgroundFilterLoop()
     │           └─ engine->ExecuteFilterCycle()
     │
     └─ Continue loop
```

### 关闭流程

```
~ApplicationService()
  │
  ├─ StopFilteringLoop()
  │  │
  │  ├─ state_->SetStopRequested(true)
  │  │
  │  ├─ if filter_thread_ is running
  │  │  └─ filter_thread_->join()  ◄─ Wait for thread to complete
  │  │
  │  └─ state_->SetRunning(false)
  │
  ├─ engine_->Shutdown()
  │
  └─ Destruction complete
```

---

## 内存管理策略

```
main()
  │
  ├─ controller: std::shared_ptr<ApplicationService>  (Count: 1)
  │  └─ Referenced by:
  │     ├─ Local variable in main
  │     └─ ConsoleUI holds shared_ptr (Count: 2)
  │
  └─ ui: ConsoleUI
     │
     └─ ui.controller_: std::shared_ptr<ApplicationService>  (Count: 2)

~ui()  ◄─ UI goes out of scope
  │
  └─ controller count: 1  (only main holds it)

~main()  ◄─ Main ends
  │
  └─ controller count: 0  ◄─ ApplicationService destroyed
     ├─ ~ApplicationService()
     │  ├─ filter_thread joined
     │  ├─ engine shutdown
     │  └─ Cleanup complete
```

---

## 错误处理流

```
ConsoleUI::ProcessCommand("1")
  │
  └─> controller_->StartFilteringLoop()
      │
      ├─ if mode == 0 (None):
      │  └─> NotifyMessage("[ERROR] No filter mode selected")
      │      │
      │      └─> ConsoleUI receives error
      │          └─> DisplayError()
      │
      └─ otherwise:
         └─> Success

BackgroundFilterLoop()
  │
  └─ Loop
     │
     ├─> engine_->ExecuteFilterCycle()
     │   │
     │   ├─ if failed:
     │   │  └─> return Result{ success: false, message: "..." }
     │   │
     │   └─ return Result{ success: true, decision: 0/1 }
     │
     ├─ if !result.success:
     │  └─> NotifyMessage(result.message)
     │      │
     │      └─> UI subscriber
     │          └─> DisplayError()
     │
     └─ Continue
```

这种架构确保：
✅ 错误处理在业务层
✅ UI 层只负责显示
✅ 清晰的信息流
✅ 易于调试和维护
