# 4.58" 424×1280 TFT MIPI module (JD9261) — documentation & samples

**简体中文：** [`README.md`](README.md)

---

> This repository provides **sample projects** for this module, together with datasheets, specifications, and interface / bring-up documentation for selection reference and integration.

## Product overview

| Item | Description |
|:--|:--|
| Module | 4.58-inch **TFT** panel, **424×1280** resolution |
| Interface | **MIPI** |
| Driver IC | **JD9261** |
| Spec ID | **`4.58-tft-424x1280-mipi-jd9261`** is the common product designation in documentation |

---

## Repository layout

### Top-level

| Path | Contents |
|:--|:--|
| `docs/` | Datasheets, specifications, initialization documentation |
| `examples/` | **Sample projects** |

### `examples/` layout

| Location | Description (internal package folder) |
|:--|:--|
| `examples/` root | **ESP-IDF代码** (esp-lvgl-port + LVGL9) |

### Sample project paths

| Description | Path |
|:--|:--|
| esp-lvgl-port + LVGL9 | `examples/P4-IDF_JD9261-MIPI_ESP-LVGL-PORT_V9/` |
