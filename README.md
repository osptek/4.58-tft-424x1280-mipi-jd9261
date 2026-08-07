<p align="left"><img alt="OSPTEK" src="./images/logo.png" width="200" /></p>

<h1 align="center">OSPTEK 4.58″ TFT 424×1280（JD9261 · MIPI）</h1>

<p align="center"><b>条状 TFT 模组 · MIPI · JD9261 · 电容触摸</b></p>

<p align="center"><a href="./README_EN.md">English</a> | 简体中文</p>

<p align="center">
  <img alt="Size: 4.58 inch" src="https://img.shields.io/badge/Size-4.58%22-3498DB?style=flat-square" />
  <img alt="Resolution: 424x1280" src="https://img.shields.io/badge/Resolution-424%C3%971280-8E44AD?style=flat-square" />
  <img alt="Interface: MIPI" src="https://img.shields.io/badge/Interface-MIPI-27AE60?style=flat-square" />
  <img alt="Driver: JD9261" src="https://img.shields.io/badge/Driver-JD9261-E7352C?style=flat-square" />
</p>

## 目录

- [产品简介](#产品简介)
- [规格参数](#规格参数)
- [示例工程](#示例工程)
- [仓库结构](#仓库结构)
- [相关资料](#相关资料)
- [购买链接](#购买链接)
- [技术支持](#技术支持)

---

## 产品简介

OSPTEK **4.58 寸 424×1280 TFT** 是一款 **MIPI** 接口彩色条状显示模组，显示驱动与触摸均为 **JD9261**（电容触摸经 I2C）。适合条状 HMI、侧边信息条与窄条交互面板等场景。

规格标识（仓库名）：`4.58-tft-424x1280-mipi-jd9261`

当前模组版本：**YDP458B002-V13**。电气与外形细节以 [`docs/YDP458B002-V13_外形图.pdf`](./docs/YDP458B002-V13_%E5%A4%96%E5%BD%A2%E5%9B%BE.pdf) 及驱动手册为准。

## 规格参数

| 项目 | 规格 |
| ---- | ---- |
| 尺寸 | 4.58 英寸 |
| 类型 | TFT（彩色） |
| 分辨率 | 424×1280 |
| 接口 | MIPI |
| 驱动 IC | JD9261 |
| 触摸驱动 | JD9261 |

> 完整外形尺寸、FPC 定义、供电与时序以产品规格书 / 驱动手册为准。

## 示例工程

| 说明 | 路径 |
| ---- | ---- |
| ESP32-P4 · JD9261 MIPI + esp-lvgl-port / LVGL9 | [`examples/P4-IDF_JD9261-MIPI_ESP-LVGL-PORT_V9/`](./examples/P4-IDF_JD9261-MIPI_ESP-LVGL-PORT_V9/) |

## 仓库结构

```text
4.58-tft-424x1280-mipi-jd9261/
├── README.md
├── README_EN.md
├── MODULE_VERSION.md
├── LICENSE
├── images/          # README 用图
├── docs/            # 外形图、驱动手册、初始化等
└── examples/        # 示例工程
```

## 相关资料

### 本产品资料

| 资料 | 链接 |
| ---- | ---- |
| 外形图（YDP458B002-V13） | [`docs/YDP458B002-V13_外形图.pdf`](./docs/YDP458B002-V13_%E5%A4%96%E5%BD%A2%E5%9B%BE.pdf) |
| 驱动 IC 数据手册（JD9261T） | [`docs/jd9261t数据手册.pdf`](./docs/jd9261t%E6%95%B0%E6%8D%AE%E6%89%8B%E5%86%8C.pdf) |
| 初始化序列（文本） | [`docs/HI8565（JD9261T）_Truly_4p58_LV_2L_424x1280_20240729.txt`](./docs/HI8565%EF%BC%88JD9261T%EF%BC%89_Truly_4p58_LV_2L_424x1280_20240729.txt) |

### 示例工程

- [ESP32-P4 JD9261 MIPI + esp-lvgl-port / LVGL9](./examples/P4-IDF_JD9261-MIPI_ESP-LVGL-PORT_V9/)

## 购买链接

<p align="center">
  <a href="https://shop110742373.taobao.com/"><img alt="淘宝官方店铺" src="https://img.shields.io/badge/淘宝-官方店铺-FF6A00?style=for-the-badge" /></a>
  &nbsp;&nbsp;
  <a href="https://www.aliexpress.com/store/1105701619"><img alt="速卖通官方店铺" src="https://img.shields.io/badge/速卖通-官方店铺-FF6A00?style=for-the-badge" /></a>
</p>

**国内（淘宝）**

- 店铺：[鱼鹰光电工厂店](https://shop110742373.taobao.com/)

**海外（AliExpress）**

- 店铺：[OSPTEK Official Store](https://www.aliexpress.com/store/1105701619)

## 技术支持

- 技术支持 / 产品咨询：<luyu@osptek.com>
- QQ 技术交流群：**985881096**
- 公司官网：<https://osptek.com/>
- 有任何问题，都可以在本仓库 Issues 中提问

---

<p align="center"><sub>© 2026 OSPTEK 鱼鹰光电 · 本仓库资料采用 CC BY 4.0 许可</sub></p>
