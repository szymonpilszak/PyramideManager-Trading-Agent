# Pyramid Manager EA for MetaTrader 4

[![Platform](https://img.shields.io/badge/Platform-MetaTrader%204-blue.svg)](https://www.metatrader4.com/)
[![Language](https://img.shields.io/badge/Language-MQL4-orange.svg)](https://docs.mql4.com/)

## Overview
A professional trade management tool for Grid and Pyramid strategies. The EA automates risk management and execution, allowing traders to focus on market direction while the bot handles position scaling and basket protection. It features intelligent market detection and sophisticated exit logic.

## Key Features
* **Adaptive Grid Engine**: Automatically detects asset types (BTC, Gold, Silver, Forex) and suggests optimal grid steps based on market volatility.
* **Each Position Net Zero**: Individual position protection that closes any profitable trade if it reverts to "break-even" after hitting the `ActivationPips` threshold.
* **Pyramid Net Zero**: A "hard kill" switch for the entire basket. If total net profit drops to zero or less, all positions are closed immediately to protect account equity.
* **Money SL (Smart Target)**: Global basket management based on a specific USD amount (e.g., -$10 limit or +$50 target).
* **Anti-Spam Execution**: Throttles server requests (max 1 modify per 2 seconds) and filters market noise to ensure broker compliance.
* **Emergency Controls**: One-click "Panic Button" (CLOSE ALL) for immediate market exit and pending order deletion.

## Grid Setup & Range
The EA employs a dynamic scaling system to handle different market environments:
* **Forex (5/3 Digits)**: Steps are calculated in standard pips (1 pip = 10 points).
* **Crypto/Indices/Metals**: Steps are treated as raw price units (e.g., a 100.0 step on BTC moves the entry by exactly $100).
* **Market Autodetect**: Default suggestions include BTC (100.0), XAU (10.0), XAG (50.0), and Forex (10.0 pips).

## Input Parameters
| Parameter | Description |
| :--- | :--- |
| **ActivationPips** | Profit threshold in pips required to activate "Each Position Net Zero" logic. |

## Usage Instructions
### 1. Deploying a Grid
Click **SET GRID** to reveal the configuration panel:
* **Mode**: Toggle between `BUY STOP` and `SELL STOP`.
* **Lot Size**: Define volume for individual orders.
* **Step**: Set distance between orders (auto-adjusted per asset).
* **Count**: Define quantity of pending orders (max 50).
* **START GRID**: Executes the sequence on the server.

### 2. Money SL Mode
Enter a USD value in the input field:
* **Negative (e.g., -20.00)**: Global basket Stop Loss.
* **Positive (e.g., 100.00)**: Global basket Take Profit.
* Click **Money SL Mode** to synchronize SL levels for all positions.

### 3. Emergency Controls
* **REMOVE ALL SL**: Wipes Stop Loss from all open positions and deactivates management modes.
* **CLOSE ALL**: Immediately closes all market positions and deletes pending orders for the current symbol.

## Installation
1. Open MetaTrader 4 and go to `File -> Open Data Folder`.
2. Navigate to `MQL4/Experts/`.
3. Place `PyramidManager.mq4` in the folder.
4. Refresh the **Navigator** in MT4 and attach the EA to a chart.
5. Ensure **"Allow DLL imports"** and **"Allow live trading"** are checked in the EA settings.

## Future Plans
* Integration of Trailing Stop-Loss for the entire USD basket.
* Multi-symbol dashboard for managing different grids from one chart.
* Performance analytics export to CSV for strategy backtesting.

## Disclaimer
This software is for educational and utility purposes only. Trading involves significant risk. Always test your settings on a **Demo Account** before moving to Live trading. The author is not responsible for any financial losses.