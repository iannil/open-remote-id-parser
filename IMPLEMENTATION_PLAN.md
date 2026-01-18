# IMPLEMENTATION_PLAN.md

Open-RemoteID-Parser (ORIP) 开发计划

## 当前状态

- **阶段**: 第四阶段完成
- **已完成**: 核心引擎 + C API + Android JNI + Python 绑定 + 协议扩展 + 高级功能
- **下一步**: 发布准备

---

## 第一阶段: 核心引擎 MVP (C++) ✅

**目标**: 实现 ASTM F3411 协议的基础解析能力

- [x] 项目基础设施 (CMake, GoogleTest)
- [x] 核心数据结构 (RawFrame, UAVObject, ParseResult)
- [x] ASTM F3411 解码器 (全部消息类型)
- [x] 会话管理器 (去重、超时、列表管理)

---

## 第二阶段: 平台绑定 ✅

**目标**: 提供跨平台 API

### 2.1 C API 封装 ✅

- [x] C 风格 API (`orip_c.h`)
- [x] 内存管理接口
- [x] 回调函数支持
- [x] 单元测试

### 2.2 Android JNI 绑定 ✅

- [x] Kotlin 数据类 (Enums, DataClasses, RemoteIDParser)
- [x] JNI 包装代码 (`orip_jni.cpp`)
- [x] Android NDK CMake 配置
- [x] Gradle 构建 (Kotlin DSL)

### 2.3 Python 绑定 ✅

- [x] ctypes 封装 (`_bindings.py`)
- [x] Python 数据类 (`types.py`)
- [x] 主解析器类 (`parser.py`)
- [x] 单元测试 (`test_orip.py`)
- [x] setuptools 打包 (`pyproject.toml`)
- [x] 示例脚本 (`scanner_demo.py`)

---

## 第三阶段: 协议扩展

**目标**: 支持更多无线协议和标准

### 3.1 蓝牙扩展 ✅

- [x] 支持 BT 5.0 Long Range (CODED PHY)
- [x] 支持 BT 5.x Extended Advertising

### 3.2 WiFi 支持 ✅

- [x] 实现 WiFi Beacon 帧解析
- [x] 实现 WiFi NAN 支持

### 3.3 其他标准 ✅

- [x] 实现 ASD-STAN (欧盟标准) 解码器
- [x] 预留 GB/T (中国国标) 接口

---

## 第四阶段: 高级功能 ✅

**目标**: 增强实用性和安全性

### 4.1 异常检测 ✅

- [x] 实现速度/位置异常检测
- [x] 实现重放攻击检测
- [x] 实现信号强度分析

### 4.2 轨迹分析 ✅

- [x] 实现轨迹平滑算法
- [x] 实现历史轨迹存储
- [x] 实现轨迹预测
- [x] 实现飞行模式分类

---

## 里程碑

| 里程碑 | 内容 | 状态 |
|--------|------|------|
| M0 | 项目初始化、文档规划 | ✅ 完成 |
| M1 | C++ 核心引擎 + ASTM F3411 解析 | ✅ 完成 |
| M2 | C API + 单元测试 | ✅ 完成 |
| M3 | Android AAR 发布 | ✅ 完成 |
| M4 | Python 绑定发布 | ✅ 完成 |
| M5 | 协议扩展 (BT5/WiFi) | ✅ 完成 |
| M6 | 其他标准 (ASD-STAN/GB/T) | ✅ 完成 |
| M7 | 高级功能 | ✅ 完成 |
| M8 | 发布准备 | 🔲 待开始 |

---

## 使用示例

### Python

```python
from orip import RemoteIDParser, TransportType

with RemoteIDParser() as parser:
    parser.set_on_new_uav(lambda uav: print(f"发现: {uav.id}"))

    result = parser.parse(ble_data, rssi=-70, transport=TransportType.BT_LEGACY)
    if result.success:
        print(f"无人机: {result.uav.id} @ {result.uav.location.latitude}")
```

### Kotlin (Android)

```kotlin
val parser = RemoteIDParser()
parser.setOnNewUAV { uav -> Log.d("ORIP", "发现: ${uav.id}") }

val result = parser.parse(scanRecord.bytes, rssi, TransportType.BT_LEGACY)
if (result.success) {
    println("无人机: ${result.uav?.id}")
}
parser.close()
```

### C

```c
orip_parser_t* parser = orip_create();
orip_result_t result;

orip_parse(parser, payload, len, rssi, ORIP_TRANSPORT_BT_LEGACY, &result);
if (result.success) {
    printf("无人机: %s\n", result.uav.id);
}

orip_destroy(parser);
```

---

## 下一步行动

**选项 A**: 发布准备 (文档完善 / CI/CD / 打包发布)
**选项 B**: 继续优化 (性能测试 / 代码审查 / 边界情况处理)
