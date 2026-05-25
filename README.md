---
# SPDX-FileCopyrightText: 2017-2024 CERN and the Corryvreckan authors
# SPDX-License-Identifier: CC-BY-4.0 OR MIT
---

<div align="center">

# 🖥️ OnlineMonitor

**Real-time reconstruction monitoring GUI for Corryvreckan**

![Module Type](https://img.shields.io/badge/Module-GLOBAL-4A90D9?style=flat-square)
![Status](https://img.shields.io/badge/Status-Functional-2ECC71?style=flat-square)
![ROOT](https://img.shields.io/badge/Requires-ROOT%20GUI-EE3B3B?style=flat-square)

<br>

🌐 **Language** &nbsp;|&nbsp; 言語選択:&nbsp;&nbsp;
[🇬🇧 English](#-english) &nbsp;·&nbsp; [🇯🇵 日本語](#-日本語)

</div>

---

## 🇬🇧 English

### Overview

OnlineMonitor opens a ROOT-based GUI that displays reconstruction results in real time.
Because Linux permits concurrent read access to files, it can run alongside an active data-taking session, acting as a true online monitor.

Canvases refresh automatically every `update` events **or** every 5 seconds — whichever comes first — so the display stays live even at very low trigger rates.

---

### ✨ Feature Summary

| Feature | Description |
|---|---|
| 📊 **Multi-canvas display** | 10+ configurable canvases covering hitmaps, tracking, correlations, charge, and timing |
| 📈 **Timeline canvas** | Rolling TProfile: clusters/event per detector (top) + tracks/event (bottom) |
| 📡 **Discord alerts** | Webhook notifications for low trigger rate and per-plane hit rate anomalies, with canvas screenshot attached |
| 🔔 **Auto-recovery notify** | Automatic Discord recovery message when conditions return to normal |
| 📏 **Progress bar** | Color-coded event counter (blue → orange → green) toward a configurable target |
| 📺 **Full-screen mode** | Expand any canvas to full display resolution with one click or keypress |
| 💾 **Auto-save** | Periodic canvas export as timestamped PNG |
| ⌨️ **Keyboard shortcuts** | Space, F, S, and digit keys for all common actions |
| 📋 **Status bar** | Live event count, trigger rate, current canvas, and wall-clock time |

---

### 🖼️ Canvases

| Canvas | Default content |
|---|---|
| **Overview** | Reference: cluster charge, hitmap, residual X |
| **Tracking Performance** | χ², angle X/Y, χ²/ndf, tracks/event, clusters/track |
| **Residuals** | Local residual X per detector |
| **Timeline** | Clusters/event per detector + tracks/event (rolling) |
| **Hitmaps** | 2D hitmap per detector |
| **Event Times** | Event time distribution per detector |
| **Charge Distributions** | Cluster charge per detector |
| **Correlation 1D X/Y** | 1D X and Y correlations per detector |
| **Correlation 2D X/Y** | 2D local X and Y correlations per detector |
| **DUTs** | Per-DUT canvas — fully user-configurable |

Plot paths support three substitution keywords:
- `%DETECTOR%` — expanded for every non-auxiliary, non-passive detector
- `%DUT%` — replaced with each configured DUT name
- `%REFERENCE%` — replaced with the reference detector name

Add `"log"` as the draw option to switch the Y axis to a logarithmic scale.

---

### ⌨️ Keyboard Shortcuts

| Key | Action |
|---|---|
| `Space` | Pause / Resume monitoring |
| `F` | Open current canvas in full screen |
| `S` | Save current canvas as timestamped PNG |
| `1` | Overview |
| `2` | Tracking Performance |
| `3` | Residuals |
| `4` | Timeline |
| `5` | Hitmaps |
| `6` | Event Times |
| `7` | Charge Distributions |
| `8` | Correlation 1D X |
| `9` | Correlation 1D Y |

---

### 🔔 Discord Alerts

Set `discord_webhook` to a valid Discord webhook URL to enable all notifications.
Every message includes the current canvas as an embedded screenshot.

| Alert | Trigger | Color |
|---|---|---|
| ⚠️ Low Trigger Rate | Rate < `discord_min_rate` for ≥ `discord_alert_duration` s | 🔴 Red |
| ✅ Rate Recovered | Rate returns above threshold | 🟢 Green |
| ⬆️ High Hit Rate | Plane clusters/event > `discord_max_hit_rate` for ≥ duration | 🔴 Red |
| ⬇️ Low Hit Rate | Plane clusters/event < `discord_min_hit_rate` for ≥ duration | 🔵 Blue |
| ✅ Hit Rate Recovered | Plane rate returns to normal range | 🟢 Green |

---

### ⚙️ Parameters

#### General

| Parameter | Default | Description |
|---|---|---|
| `update` | `200` | Events between canvas refreshes |
| `canvas_title` | `"Corryvreckan Testbeam Monitor"` | GUI window title |
| `ignore_aux` | `true` | Exclude detectors with auxiliary role |
| `clustering_module` | `"Clustering4D"` | Source module for clustering plots |
| `tracking_module` | `"Tracking4D"` | Source module for tracking plots |
| `target_events` | `0` | Target event count for progress bar (`0` = disabled) |
| `auto_save_interval` | `0` | Auto-save interval in seconds (`0` = disabled) |
| `auto_save_dir` | `"./"` | Output directory for auto-saved PNGs |

#### Discord

| Parameter | Default | Description |
|---|---|---|
| `discord_webhook` | `""` | Webhook URL — empty string disables all alerts |
| `discord_min_rate` | `100.0` | Trigger rate threshold (evt/s) |
| `discord_alert_duration` | `30` | Seconds below threshold before alert fires |
| `discord_max_hit_rate` | `0.0` | Max clusters/event per plane (`0` = disabled) |
| `discord_min_hit_rate` | `0.0` | Min clusters/event per plane (`0` = disabled) |

#### Canvas overrides

All canvases accept a matrix of `["plot/path", "draw_options"]` rows.

<details>
<summary>Show canvas parameter names</summary>

| Parameter | Canvas |
|---|---|
| `overview` | Overview |
| `dut_plots` | DUTs |
| `tracking` | Tracking Performance |
| `hitmaps` | Hitmaps |
| `residuals` | Residuals |
| `correlation_x` | Correlation 1D X |
| `correlation_y` | Correlation 1D Y |
| `correlation_x2d` | Correlation 2D X |
| `correlation_y2d` | Correlation 2D Y |
| `charge_distributions` | Charge Distributions |
| `event_times` | Event Times |

</details>

---

### 📝 Configuration Example

```toml
[OnlineMonitor]
update = 200
canvas_title = "Run 00042 — MALTA2 Testbeam"
clustering_module = "ClusteringSpatial"
target_events = 1000000
auto_save_interval = 60
auto_save_dir = "/data/snapshots/"

# Discord alerts (keep webhook URL out of version control)
discord_webhook = "https://discord.com/api/webhooks/..."
discord_min_rate = 50.0
discord_alert_duration = 20
discord_max_hit_rate = 5.0
discord_min_hit_rate = 0.1

dut_plots = [["EventLoaderEUDAQ2/%DUT%/hitmap",                          "colz"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelTimes"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelRawValues"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelMultiplicityPerCorryEvent",  "log"],
             ["AnalysisDUT/%DUT%/clusterChargeAssociated"],
             ["AnalysisDUT/%DUT%/associatedTracksVersusTime"]]
```

---
---

## 🇯🇵 日本語

### 概要

OnlineMonitorはROOTベースのGUIを起動し、再構成結果をリアルタイムで表示するモジュールです。
LinuxはファイルへのConcurrent読み取りアクセスを許可しているため、データ取得中のランと並行して起動でき、真のオンラインモニタリングツールとして機能します。

各キャンバスは`update`イベントごと、または5秒ごと（どちらか早い方）に自動更新されるため、低トリガーレート時でも常に最新の状態を保ちます。

---

### ✨ 機能一覧

| 機能 | 説明 |
|---|---|
| 📊 **マルチキャンバス表示** | ヒットマップ・トラッキング・相関・電荷・タイミングをカバーする10以上のキャンバス |
| 📈 **タイムラインキャンバス** | ローリングTProfile：上段に検出器ごとのclusters/event、下段にtracks/event |
| 📡 **Discord通知** | トリガーレート低下・プレーン異常ヒットレートをキャンバスのスクリーンショット付きで通知 |
| 🔔 **自動復帰通知** | 状態が正常に戻った際に自動でDiscord復帰メッセージを送信 |
| 📏 **プログレスバー** | 目標イベント数に向けた色付きカウンター（青→橙→緑） |
| 📺 **全画面モード** | ワンクリックまたはキー操作で任意のキャンバスをフルディスプレイ解像度に拡大 |
| 💾 **自動保存** | タイムスタンプ付きPNGへの定期キャンバスエクスポート |
| ⌨️ **キーボードショートカット** | よく使う操作をすべてSpace・F・S・数字キーで実行可能 |
| 📋 **ステータスバー** | イベント数・トリガーレート・現在のキャンバス・現在時刻をリアルタイム表示 |

---

### 🖼️ キャンバス一覧

| キャンバス | デフォルト表示内容 |
|---|---|
| **Overview** | リファレンス：クラスター電荷、ヒットマップ、残差X |
| **Tracking Performance** | χ²、角度X/Y、χ²/ndf、tracks/event、clusters/track |
| **Residuals** | 検出器ごとのローカル残差X |
| **Timeline** | 検出器ごとのclusters/event + tracks/event（ローリング） |
| **Hitmaps** | 検出器ごとの2Dヒットマップ |
| **Event Times** | 検出器ごとのイベント時刻分布 |
| **Charge Distributions** | 検出器ごとのクラスター電荷 |
| **Correlation 1D X/Y** | 検出器ごとの1D相関（X・Y） |
| **Correlation 2D X/Y** | 検出器ごとの2Dローカル相関（X・Y） |
| **DUTs** | DUTごとのキャンバス（完全にユーザー設定可能） |

プロットパスには以下の置換キーワードが使用できます：
- `%DETECTOR%` — Auxiliary・Passive以外の全検出器に展開
- `%DUT%` — 設定された各DUT名に置換
- `%REFERENCE%` — リファレンス検出器名に置換

描画オプションに`"log"`を指定するとY軸を対数スケールに切り替えます。

---

### ⌨️ キーボードショートカット

| キー | 動作 |
|---|---|
| `Space` | モニタリングの一時停止 / 再開 |
| `F` | 現在のキャンバスを全画面表示 |
| `S` | 現在のキャンバスをタイムスタンプ付きPNGで保存 |
| `1` | Overview へ切り替え |
| `2` | Tracking Performance へ切り替え |
| `3` | Residuals へ切り替え |
| `4` | Timeline へ切り替え |
| `5` | Hitmaps へ切り替え |
| `6` | Event Times へ切り替え |
| `7` | Charge Distributions へ切り替え |
| `8` | Correlation 1D X へ切り替え |
| `9` | Correlation 1D Y へ切り替え |

---

### 🔔 Discord通知

`discord_webhook`に有効なDiscord WebhookのURLを設定すると全ての通知が有効になります。
全てのメッセージに現在のキャンバスのスクリーンショットが埋め込み添付されます。

| 通知 | 発火条件 | 色 |
|---|---|---|
| ⚠️ トリガーレート低下 | レート < `discord_min_rate` が `discord_alert_duration` 秒以上継続 | 🔴 赤 |
| ✅ レート復帰 | レートが閾値を上回る | 🟢 緑 |
| ⬆️ ヒットレート高すぎ | プレーンのclusters/event > `discord_max_hit_rate` が継続 | 🔴 赤 |
| ⬇️ ヒットレート低すぎ | プレーンのclusters/event < `discord_min_hit_rate` が継続 | 🔵 青 |
| ✅ ヒットレート復帰 | プレーンのレートが正常範囲に戻る | 🟢 緑 |

---

### ⚙️ パラメータ

#### 基本設定

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `update` | `200` | キャンバス更新間隔（イベント数） |
| `canvas_title` | `"Corryvreckan Testbeam Monitor"` | GUIウィンドウのタイトル |
| `ignore_aux` | `true` | Auxiliaryロールの検出器を除外する |
| `clustering_module` | `"Clustering4D"` | クラスタリングプロットのソースモジュール名 |
| `tracking_module` | `"Tracking4D"` | トラッキングプロットのソースモジュール名 |
| `target_events` | `0` | プログレスバーの目標イベント数（`0`で無効） |
| `auto_save_interval` | `0` | 自動保存の間隔（秒、`0`で無効） |
| `auto_save_dir` | `"./"` | 自動保存PNGの出力先ディレクトリ |

#### Discord設定

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `discord_webhook` | `""` | Webhook URL（空文字で全通知を無効化） |
| `discord_min_rate` | `100.0` | トリガーレート閾値（evt/s） |
| `discord_alert_duration` | `30` | アラート発火までの継続時間（秒） |
| `discord_max_hit_rate` | `0.0` | プレーンごとの最大clusters/event（`0`で無効） |
| `discord_min_hit_rate` | `0.0` | プレーンごとの最小clusters/event（`0`で無効） |

#### キャンバス設定

全てのキャンバスは`["プロットパス", "描画オプション"]`の行からなるマトリックスで上書き設定できます。

<details>
<summary>キャンバスパラメータ名一覧を表示</summary>

| パラメータ | キャンバス |
|---|---|
| `overview` | Overview |
| `dut_plots` | DUTs |
| `tracking` | Tracking Performance |
| `hitmaps` | Hitmaps |
| `residuals` | Residuals |
| `correlation_x` | Correlation 1D X |
| `correlation_y` | Correlation 1D Y |
| `correlation_x2d` | Correlation 2D X |
| `correlation_y2d` | Correlation 2D Y |
| `charge_distributions` | Charge Distributions |
| `event_times` | Event Times |

</details>

---

### 📝 設定例

```toml
[OnlineMonitor]
update = 200
canvas_title = "Run 00042 — MALTA2 Testbeam"
clustering_module = "ClusteringSpatial"
target_events = 1000000
auto_save_interval = 60
auto_save_dir = "/data/snapshots/"

# Discord通知（WebhookのURLはバージョン管理に含めないこと）
discord_webhook = "https://discord.com/api/webhooks/..."
discord_min_rate = 50.0
discord_alert_duration = 20
discord_max_hit_rate = 5.0
discord_min_hit_rate = 0.1

dut_plots = [["EventLoaderEUDAQ2/%DUT%/hitmap",                          "colz"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelTimes"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelRawValues"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelMultiplicityPerCorryEvent",  "log"],
             ["AnalysisDUT/%DUT%/clusterChargeAssociated"],
             ["AnalysisDUT/%DUT%/associatedTracksVersusTime"]]
```
