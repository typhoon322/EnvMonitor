# MQTT 遥测（Phase 1）

阳台 / 远端 EnvMonitor 通过家里 Wi‑Fi 把环境数据发到 MQTT Broker，电脑、手机、Home Assistant 都可以订阅同一主题。

## 一次配置（串口 115200）

```text
wifi set YourSSID YourPassword
mqtt set 192.168.1.10 1883
mqtt interval 10
wifi status
mqtt status
```

有用户名密码时：

```text
mqtt set 192.168.1.10 1883 mqttuser mqttpass
```

自定义设备名（默认用 MAC 后 6 位）：

```text
mqtt id balcony
mqtt prefix envmonitor
```

主题示例：`envmonitor/balcony/state`、`envmonitor/balcony/status`。

## Broker（Mosquitto 最小示例）

Debian / Ubuntu / 树莓派：

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto
```

仅本机测试可先不设密码；局域网生产环境请配置账号并关掉匿名访问。

## 订阅验证

```bash
mosquitto_sub -h 192.168.1.10 -t 'envmonitor/+/state' -v
```

期望载荷：

```json
{"t":30.1,"rh":55.2,"eco2":450,"tvoc":40,"aqi":1,"state":"OK","ts":123456}
```

在线状态主题（retain + LWT）：

```bash
mosquitto_sub -h 192.168.1.10 -t 'envmonitor/+/status' -v
```

值为 `online` / `offline`。

## 电脑追加 CSV（示例）

```bash
mosquitto_sub -h 192.168.1.10 -t 'envmonitor/+/state' |
while read -r topic payload; do
  echo "$(date -Iseconds),$topic,$payload" >> envmonitor.csv
done
```

## 说明

- 未配置 Wi‑Fi / MQTT 时，本地 TFT 与采样照常工作
- MQTT 连不上时指数退避重试：5s → 10s → 20s → …，**最长 5 分钟**；连上或改配置 / Wi‑Fi 恢复后重置
- `mqtt status` **不回显**密码
- Home Assistant Discovery、局域网 HTTP 状态页见后续 Phase（设计文档）

设计：`docs/superpowers/specs/2026-08-09-mqtt-telemetry-design.md`
