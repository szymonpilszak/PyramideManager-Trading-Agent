# Pyramid Manager EA for MetaTrader 4

[![Platform](https://img.shields.io/badge/Platform-MetaTrader%204-blue.svg)](https://www.metatrader4.com/)
[![Language](https://img.shields.io/badge/Language-MQL4-orange.svg)](https://docs.mql4.com/)

## Overview
A robust Expert Advisor (EA) designed for advanced grid and pyramid trading management on the MetaTrader 4 platform. This tool automates complex position-sizing strategies, synchronizes stop-losses across multiple orders, and manages basket-wide exit strategies based on real-time monetary profit/loss targets.

## Key Features
* **Dynamic Grid Execution**: Rapid deployment of Buy/Sell Stop grids with customizable step distances and lot-sizing logic.
* **Money SL/TP Mode**: Global basket management based on total net profit/loss (USD), bypassing individual order limitations.
* **Pyramid Net Zero**: Portfolio protection logic that closes all positions in a pyramid if the cumulative profit reverts to breakeven.
* **High-Frequency Optimization**: Integrated throttling logic (2-second execution timer) and price-change thresholds to ensure broker compliance and prevent server spamming.
* **Interactive GUI**: Real-time profit tracking and manual control panel for on-the-fly grid adjustments and emergency "Panic" closures.
* **Cross-Asset Compatibility**: Precision math using `MODE_TICKVALUE` and `MODE_TICKSIZE` for universal price distance calculations (Forex, Gold, Crypto).

## Technical Specifications
* **Anti-Spam Filter**: `OrderModify` requests are throttled (max once every 2 seconds).
* **Slippage Protection**: Includes `StopLevel` check to prevent errors when price is within the server's freeze zone.
* **Order Isolation**: Uses `MagicNumber` for independent management of specific strategy instances.

## Input Parameters
| Parameter | Description |
| :--- | :--- |
| **MagicNumber** | Unique ID for the EA. Set to 0 to manage all manual trades. |
| **ActivationPips** | Minimum profit in pips required for "Each Zero" closure logic. |

## Future Plans
* Implement trailing stop-loss for the entire basket.
* Add Telegram API integration for real-time profit notifications.
* Porting core logic to MQL5 for MetaTrader 5 compatibility.

## Requirements
* **MetaTrader 4 Terminal**
* **MQL4 Compiler** (included in MetaEditor)
* **Demo Account** for initial strategy validation

## Getting Started
1. **Open MT4 Data Folder**: Go to `File -> Open Data Folder`.
2. **Deploy EA**: Navigate to `MQL4/Experts/` and copy the `PyramidManager.mq4` file.
3. **Refresh Terminal**: Restart MT4 or right-click *Experts* in the Navigator and select *Refresh*.
4. **Attach to Chart**: Drag the EA onto a chart (H1 or M1 recommended).
5. **Configure**: Enable "Allow DLL imports" and "Allow live trading" in the EA properties.

## Disclaimer
Trading involves significant risk. This software is provided "as-is" without any warranties. Always test on a Demo Account before using it on a Live account. The author is not responsible for any financial losses.