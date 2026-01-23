# ORIP Test Data

本目录包含 ORIP 库的测试数据样本，用于验证协议解析的正确性。

## 目录结构

```
data/
├── astm_f3411/     # ASTM F3411 协议测试数据
├── asd_stan/       # ASD-STAN EN 4709-002 协议测试数据
├── wifi/           # WiFi Beacon/NAN 测试数据
├── anomaly/        # 异常检测测试数据
└── boundary/       # 边界条件测试数据
```

## 数据格式

所有测试数据使用 JSON 格式，包含以下字段：

```json
{
  "name": "测试用例名称",
  "description": "测试描述",
  "protocol": "ASTM_F3411 | ASD_STAN | WIFI_BEACON | WIFI_NAN",
  "message_type": "BASIC_ID | LOCATION | AUTH | SELF_ID | SYSTEM | OPERATOR_ID | MESSAGE_PACK",
  "payload_hex": "十六进制原始数据",
  "expected": {
    "parse_success": true,
    "field_values": { ... }
  }
}
```

## 使用方法

```cpp
#include <fstream>
#include <nlohmann/json.hpp>

// 加载测试数据
std::ifstream f("data/astm_f3411/basic_id.json");
auto test_cases = nlohmann::json::parse(f);

for (const auto& tc : test_cases) {
    auto payload = hex_to_bytes(tc["payload_hex"]);
    auto result = parser.parse(payload, -70);
    EXPECT_EQ(result.success, tc["expected"]["parse_success"]);
}
```

## 数据来源

- **合成数据**: 根据协议规范构造的测试向量
- **真实数据**: 从实际无人机设备捕获（如有）

## 注意事项

1. 真实数据可能包含设备信息，仅用于测试目的
2. 边界条件数据可能包含无效或恶意构造的数据
3. 定期更新以覆盖新发现的边界情况
