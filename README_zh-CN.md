# Open Remote ID Parser (ORIP)

<p align="center">
  <strong>高性能、跨平台的无人机 Remote ID 信号解析库</strong>
</p>

<p align="center">
  <a href="#特性">特性</a> •
  <a href="#安装">安装</a> •
  <a href="#快速开始">快速开始</a> •
  <a href="#api-参考">API</a> •
  <a href="#从源码构建">构建</a> •
  <a href="#贡献">贡献</a>
</p>

<p align="center">
  <a href="README.md">English</a> | <b>中文</b>
</p>

---

**Open Remote ID Parser** 是一个轻量级 C++ 库，用于解码无人机 Remote ID 广播。它支持多种协议（ASTM F3411、ASD-STAN）和传输层（蓝牙 Legacy/Extended、WiFi Beacon/NAN），非常适合在移动设备、嵌入式系统或桌面平台上构建无人机侦测应用。

> Remote ID 是无人机的"电子牌照"，受全球各地法规强制要求（美国 FAA、欧洲 EASA）。本库使任何人都能利用智能手机或树莓派等普通硬件构建无人机侦测解决方案。

## 特性

- **多协议支持**
  - ASTM F3411-22a（美国/国际）
  - ASD-STAN EN 4709-002（欧盟）
  - GB/T（中国）- 接口预留

- **多传输方式支持**
  - 蓝牙 4.x Legacy 广播
  - 蓝牙 5.x Extended 广播 / 长距离（Coded PHY）
  - WiFi Beacon（802.11 厂商自定义 IE）
  - WiFi NAN（邻居感知网络）

- **高级分析**
  - 异常检测（欺骗、重放攻击、不可能的速度）
  - 轨迹分析（平滑、预测、模式分类）
  - 会话管理（去重、超时处理）

- **跨平台绑定**
  - C++（核心库）
  - C API（用于 FFI 集成）
  - Android/Kotlin（通过 JNI）
  - Python（通过 ctypes）

- **性能**
  - 使用位域的零拷贝解析
  - 最小化内存分配
  - 适合移动设备上的实时处理

## 支持的消息类型

| 消息类型 | 描述 |
| --------- | ------ |
| Basic ID (0x0) | 无人机序列号、注册 ID |
| Location (0x1) | 经纬度、高度、速度、航向 |
| Authentication (0x2) | 加密认证数据 |
| Self-ID (0x3) | 操作员定义的描述文本 |
| System (0x4) | 操作员位置、作业区域 |
| Operator ID (0x5) | 操作员注册号 |
| Message Pack (0xF) | 单次广播中的多条消息 |

## 安装

### C++ (CMake)

```cmake
include(FetchContent)
FetchContent_Declare(
    orip
    GIT_REPOSITORY https://github.com/iannil/open-remote-id-parser.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(orip)

target_link_libraries(your_target PRIVATE orip)
```

### Python

```bash
cd python
pip install .

# 或用于开发
pip install -e ".[dev]"
```

### Android (Gradle)

```kotlin
// settings.gradle.kts
include(":orip")
project(":orip").projectDir = file("path/to/open-remote-id-parser/android/orip")

// app/build.gradle.kts
dependencies {
    implementation(project(":orip"))
}
```

## 快速开始

### C++

```cpp
#include <orip/orip.h>

int main() {
    orip::RemoteIDParser parser;
    parser.init();

    // 设置回调
    parser.setOnNewUAV([](const orip::UAVObject& uav) {
        std::cout << "发现新无人机: " << uav.id << std::endl;
    });

    // 解析传入的 BLE 广播
    std::vector<uint8_t> ble_data = /* 来自扫描器 */;
    auto result = parser.parse(ble_data, rssi, orip::TransportType::BT_LEGACY);

    if (result.success) {
        std::cout << "无人机 ID: " << result.uav.id << std::endl;
        std::cout << "位置: " << result.uav.location.latitude
                  << ", " << result.uav.location.longitude << std::endl;
    }

    return 0;
}
```

### Python

```python
from orip import RemoteIDParser, TransportType

with RemoteIDParser() as parser:
    parser.set_on_new_uav(lambda uav: print(f"发现新无人机: {uav.id}"))

    # 解析 BLE 广播数据
    result = parser.parse(ble_data, rssi=-70, transport=TransportType.BT_LEGACY)

    if result.success:
        print(f"无人机: {result.uav.id}")
        print(f"位置: {result.uav.location.latitude}, {result.uav.location.longitude}")
```

### Kotlin (Android)

```kotlin
import com.orip.RemoteIDParser
import com.orip.TransportType

class DroneScanner {
    private val parser = RemoteIDParser()

    init {
        parser.setOnNewUAV { uav ->
            Log.d("DroneScanner", "发现新无人机: ${uav.id}")
        }
    }

    // 在 BLE 扫描回调中
    fun onScanResult(result: ScanResult) {
        val scanRecord = result.scanRecord ?: return

        val parseResult = parser.parse(
            scanRecord.bytes,
            result.rssi,
            TransportType.BT_LEGACY
        )

        if (parseResult.success) {
            updateMap(parseResult.uav)
        }
    }

    fun cleanup() {
        parser.close()
    }
}
```

### C API

```c
#include <orip/orip_c.h>

int main() {
    orip_parser_t* parser = orip_create();
    orip_result_t result;

    uint8_t payload[] = { /* BLE 数据 */ };

    orip_parse(parser, payload, sizeof(payload), -70,
               ORIP_TRANSPORT_BT_LEGACY, &result);

    if (result.success) {
        printf("无人机: %s\n", result.uav.id);
        printf("纬度: %f, 经度: %f\n",
               result.uav.location.latitude,
               result.uav.location.longitude);
    }

    orip_destroy(parser);
    return 0;
}
```

## 高级功能

### 异常检测

检测欺骗尝试和不可能的飞行模式：

```cpp
#include <orip/anomaly_detector.h>

orip::analysis::AnomalyDetector detector;

// 分析每次无人机更新
auto anomalies = detector.analyze(uav, rssi);

for (const auto& anomaly : anomalies) {
    switch (anomaly.type) {
        case AnomalyType::REPLAY_ATTACK:
            std::cerr << "警告: 检测到可能的重放攻击！" << std::endl;
            break;
        case AnomalyType::SPEED_IMPOSSIBLE:
            std::cerr << "警告: 检测到不可能的速度！" << std::endl;
            break;
        // ...
    }
}
```

### 轨迹分析

追踪飞行路径并预测未来位置：

```cpp
#include <orip/trajectory_analyzer.h>

orip::analysis::TrajectoryAnalyzer analyzer;

// 添加位置更新
analyzer.addPosition(uav.id, uav.location);

// 获取飞行模式
auto pattern = analyzer.classifyPattern(uav.id);
// 返回: LINEAR（直线）, CIRCULAR（环形）, PATROL（巡逻）, STATIONARY（悬停）等

// 预测 5 秒后的位置
auto prediction = analyzer.predictPosition(uav.id, 5000);
std::cout << "预测位置: " << prediction.latitude
          << ", " << prediction.longitude << std::endl;

// 获取轨迹统计
auto stats = analyzer.getStats(uav.id);
std::cout << "总距离: " << stats.total_distance_m << " 米" << std::endl;
std::cout << "最大速度: " << stats.max_speed_mps << " 米/秒" << std::endl;
```

## API 参考

### 核心类

| 类 | 描述 |
| --- | ------ |
| `RemoteIDParser` | 主解析器类，处理所有协议 |
| `UAVObject` | 完整的无人机数据（ID、位置、操作员信息） |
| `ParseResult` | 解析操作的结果 |
| `LocationVector` | 位置、高度、速度、航向 |
| `SystemInfo` | 操作员位置、作业区域 |

### 分析类

| 类 | 描述 |
| --- | ------ |
| `AnomalyDetector` | 检测欺骗和不可能的模式 |
| `TrajectoryAnalyzer` | 追踪飞行路径、预测位置 |

### 协议解码器

| 类 | 描述 |
| --- | ------ |
| `ASTM_F3411_Decoder` | ASTM F3411-22a（美国/国际） |
| `ASD_STAN_Decoder` | ASD-STAN EN 4709-002（欧盟） |
| `WiFiDecoder` | WiFi Beacon 和 NAN 帧 |
| `CN_RID_Decoder` | GB/T 中国标准（预留） |

## 从源码构建

### 环境要求

- CMake 3.16+
- C++17 兼容编译器（GCC 8+、Clang 7+、MSVC 2019+）
- （可选）Android NDK 用于 Android 构建
- （可选）Python 3.8+ 用于 Python 绑定

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/iannil/open-remote-id-parser.git
cd open-remote-id-parser

# 创建构建目录
mkdir build && cd build

# 配置
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build . --parallel

# 运行测试
ctest --output-on-failure

# 安装（可选）
sudo cmake --install .
```

### 构建选项

| 选项 | 默认值 | 描述 |
| ----- | ------- | ------ |
| `ORIP_BUILD_TESTS` | ON | 构建单元测试 |
| `ORIP_BUILD_EXAMPLES` | ON | 构建示例程序 |
| `ORIP_BUILD_SHARED` | OFF | 构建共享库（.so/.dll） |

### Android 构建

```bash
cd android
./gradlew :orip:assembleRelease
```

AAR 将生成在 `android/orip/build/outputs/aar/`。

### Python 构建

```bash
cd python
pip install build
python -m build
```

## 项目结构

```
open-remote-id-parser/
├── include/orip/           # 公共 C++ 头文件
│   ├── orip.h              # 主包含文件
│   ├── parser.h            # RemoteIDParser 类
│   ├── types.h             # 数据结构
│   ├── astm_f3411.h        # ASTM 解码器
│   ├── asd_stan.h          # ASD-STAN 解码器
│   ├── wifi_decoder.h      # WiFi 解码器
│   ├── anomaly_detector.h  # 异常检测
│   ├── trajectory_analyzer.h # 轨迹分析
│   └── orip_c.h            # C API
├── src/
│   ├── core/               # 核心实现
│   ├── protocols/          # 协议解码器
│   ├── analysis/           # 分析模块
│   └── utils/              # 工具类
├── tests/                  # 单元测试
├── examples/               # 示例程序
├── android/                # Android 库
│   └── orip/
│       ├── src/main/java/  # Kotlin 类
│       └── src/main/cpp/   # JNI 封装
├── python/                 # Python 绑定
│   ├── orip/               # Python 包
│   ├── tests/              # Python 测试
│   └── examples/           # Python 示例
└── docs/                   # 文档
```

## 硬件推荐

### 入门级（移动侦测）

- 任何支持蓝牙 5.0+ 的 Android 手机
- 侦测距离：300-800米（取决于环境条件）

### 专业级（固定站点）

- 树莓派 4 + ESP32-C3（BLE 嗅探器）
- 外置高增益天线
- 侦测距离：2-5公里

### 企业级

- 软件定义无线电（SDR）方案
- 多接收器用于三角定位
- 与现有安防系统集成

## 贡献

欢迎贡献！请在提交 PR 前阅读我们的贡献指南。

1. Fork 本仓库
2. 创建功能分支（`git checkout -b feature/amazing-feature`）
3. 提交更改（`git commit -m 'Add amazing feature'`）
4. 推送到分支（`git push origin feature/amazing-feature`）
5. 开启 Pull Request

### 开发环境设置

```bash
# 安装开发依赖
pip install -e "python/.[dev]"

# 运行 C++ 测试
cd build && ctest

# 运行 Python 测试
cd python && pytest

# 代码格式化（如果安装了 clang-format）
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 开发路线图

### 已完成

- [x] **核心引擎**: 零拷贝设计的 C++ 解析库
- [x] **ASTM F3411**: 完整支持全部 7 种消息类型（Basic ID、Location、Authentication、Self-ID、System、Operator ID、Message Pack）
- [x] **ASD-STAN EN 4709-002**: 欧洲标准，含 EU Operator ID 验证
- [x] **GB/T 接口**: 中国国标预留（等待规范发布）
- [x] **多传输方式**: 蓝牙 4.x Legacy、蓝牙 5.x Extended/Long Range、WiFi Beacon、WiFi NAN
- [x] **C API**: 完整的 FFI 接口，支持回调
- [x] **Android 绑定**: Kotlin/JNI 封装，AAR 打包
- [x] **Python 绑定**: 基于 ctypes，支持上下文管理器
- [x] **会话管理器**: 去重、超时处理、事件回调
- [x] **异常检测**: 8 种检测类型（速度、位置、重放攻击、信号等）
- [x] **轨迹分析**: 平滑、预测、模式分类
- [x] **单元测试**: 70+ 测试用例覆盖所有模块
- [x] **文档**: README（中/英）、CONTRIBUTING、CHANGELOG

### 进行中

- [ ] **发布产物**: 构建 `/release` 目录（静态库/共享库）
- [ ] **CI/CD 完善**: GitHub Actions 构建验证
- [ ] **性能基准测试**: 解析延迟、内存占用分析

### 计划中

- [ ] **v0.1.0 发布**: 首个正式版本
- [ ] **Android 示例应用**: 完整的集成示例
- [ ] **真机测试**: 使用真实无人机抓包数据验证
- [ ] **API 文档**: 自动生成参考文档（Doxygen）
- [ ] **iOS 绑定**: 通过 C API 的 Swift 封装

## 致谢

- [ASTM F3411](https://www.astm.org/f3411-22a.html) - Remote ID 标准规范
- [ASD-STAN EN 4709-002](https://asd-stan.org/) - 欧洲 Remote ID 标准
- [OpenDroneID](https://github.com/opendroneid) - 参考实现
