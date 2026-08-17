<p align="left"><img alt="OSPTEK" src="./images/logo.png" width="200" /></p>

<h1 align="center">OSPTEK 4.58″ TFT 424×1280 (JD9261 · MIPI)</h1>

<p align="center"><b>Bar TFT module · MIPI · JD9261 · capacitive touch</b></p>

<p align="center"><a href="./README.md">简体中文</a> | English · <a href="../../README_EN.md">Family index</a></p>

<p align="center">
  <img alt="Size: 4.58 inch" src="https://img.shields.io/badge/Size-4.58%22-3498DB?style=flat-square" />
  <img alt="Resolution: 424x1280" src="https://img.shields.io/badge/Resolution-424%C3%971280-8E44AD?style=flat-square" />
  <img alt="Interface: MIPI" src="https://img.shields.io/badge/Interface-MIPI-27AE60?style=flat-square" />
  <img alt="Driver: JD9261" src="https://img.shields.io/badge/Driver-JD9261-E7352C?style=flat-square" />
</p>

<p align="center"><img alt="OSPTEK 4.58 inch 424x1280 TFT MIPI module (JD9261) product image" src="./images/product.png" width="640" /></p>

## Contents

- [Overview](#overview)
- [Specifications](#specifications)
- [Sample projects](#sample-projects)
- [Repository layout](#repository-layout)
- [Resources](#resources)
- [Buy](#buy)
- [Support](#support)

---

## Overview

OSPTEK **4.58″ 424×1280 TFT** is a **MIPI** color bar display module with display and touch driven by **JD9261** (capacitive touch over I2C). Suited to bar HMIs, side information strips, and narrow interactive panels.

Spec ID (repository name): `4.58-tft-424x1280-mipi-jd9261`

Current module version: **YDP458B002-V13**. Electrical and mechanical details follow [`docs/YDP458B002-V13_外形图.pdf`](./docs/YDP458B002-V13_%E5%A4%96%E5%BD%A2%E5%9B%BE.pdf) and the driver IC datasheet.

## Specifications

| Item | Spec |
| ---- | ---- |
| Size | 4.58 inch |
| Type | TFT (color) |
| Resolution | 424×1280 |
| Interface | MIPI |
| Driver IC | JD9261 |
| Touch driver | JD9261 |

> Full outline, FPC definition, power, and timing follow the product datasheet / driver IC datasheet.

## Sample projects

| Description | Path |
| ---- | ---- |
| ESP32-P4 · JD9261 MIPI + esp-lvgl-port / LVGL9 | [`examples/P4-IDF_JD9261-MIPI_ESP-LVGL-PORT_V9/`](./examples/P4-IDF_JD9261-MIPI_ESP-LVGL-PORT_V9/) |

## Repository layout

```text
4.58-tft-424x1280-mipi-jd9261/                                # repo root (nav: ../../README_EN.md)
└── versions/
    └── YDP458B002-V13/                                # full materials for this part number
        ├── README.md
        ├── README_EN.md
        ├── images/
        ├── docs/
        └── examples/
```

## Resources

### Product files

| Resource | Link |
| ---- | ---- |
| Outline drawing (YDP458B002-V13) | [`docs/YDP458B002-V13_外形图.pdf`](./docs/YDP458B002-V13_%E5%A4%96%E5%BD%A2%E5%9B%BE.pdf) |
| Driver IC datasheet (JD9261T) | [`docs/jd9261t数据手册.pdf`](./docs/jd9261t%E6%95%B0%E6%8D%AE%E6%89%8B%E5%86%8C.pdf) |
| Init sequence (text) | [`docs/HI8565（JD9261T）_Truly_4p58_LV_2L_424x1280_20240729.txt`](./docs/HI8565%EF%BC%88JD9261T%EF%BC%89_Truly_4p58_LV_2L_424x1280_20240729.txt) |

### Samples

- [ESP32-P4 JD9261 MIPI + esp-lvgl-port / LVGL9](./examples/P4-IDF_JD9261-MIPI_ESP-LVGL-PORT_V9/)

## Buy

<p align="center">
  <a href="https://www.aliexpress.com/store/1105701619"><img alt="AliExpress store" src="https://img.shields.io/badge/AliExpress-Official_Store-FF6A00?style=for-the-badge" /></a>
  &nbsp;&nbsp;
  <a href="https://shop110742373.taobao.com/"><img alt="Taobao store" src="https://img.shields.io/badge/Taobao-Official_Store-FF6A00?style=for-the-badge" /></a>
</p>

**Overseas (AliExpress)**

- Store: [OSPTEK Official Store](https://www.aliexpress.com/store/1105701619)

**China (Taobao)**

- Store: [鱼鹰光电工厂店](https://shop110742373.taobao.com/)

## Support

- Technical support / product inquiry: <luyu@osptek.com>
- QQ group: **985881096**
- Website: <https://osptek.com/>
- Feel free to open an Issue in this repository with any questions

---

<p align="center"><sub>© 2026 OSPTEK · Materials in this repository are licensed under CC BY 4.0</sub></p>
