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
| 📊 **Multi-canvas display** | 16 canvases covering hitmaps, tracking, residuals, correlations, cluster size, timing, L1idC, and alignment drift |
| 📈 **Timeline canvas** | Rolling TProfile: clusters/event per detector (top) + tracks/event (bottom) |
| 📉 **Corr Trends canvas** | Per-event ΔX / ΔY global [mm] vs event number — early warning for alignment drift |
| 🔢 **L1idC canvas** | l1idC vs event, wall-clock time vs event, inter-plane sync delta, coincidence rate — DAQ health at a glance |
| 📏 **Progress bar** | Color-coded event counter (blue → orange → green) toward a configurable target |
| 📺 **Full-screen mode** | Expand any canvas to full display resolution with one click or keypress |
| 💾 **Auto-save** | Periodic canvas export as timestamped PNG |
| ⌨️ **Keyboard shortcuts** | Space, F, S, and digit keys for all common actions |
| 📋 **Status bar** | Live event count, trigger rate, current canvas with its last draw time, and wall-clock time. A draw over 100 ms is also logged as a WARNING |

---

### 🖼️ Canvases

| Canvas | Group | Default content |
|---|---|---|
| **Overview** | Tracking | Reference: cluster charge, hitmap, residual X |
| **Tracking Performance** | Tracking | χ², angle X/Y, χ²/ndf, tracks/event, clusters/track |
| **Residuals X** | Tracking | Local residual X per detector (DUTs excluded) |
| **Residuals Y** | Tracking | Local residual Y per detector (DUTs excluded) |
| **Res Trends** | Tracking | Local residual X (top) / Y (bottom) vs. l1id, all detectors incl. DUTs overlaid like Corr Trends — see **Residual Trend** below |
| **Timeline** | Tracking | Clusters/event per detector (top) + tracks/event (bottom), rolling TProfile |
| **Corr Trends** | Tracking | ΔX / ΔY global [mm] vs event number per non-reference detector — alignment drift monitor |
| **Hitmaps** | Detectors | 2D hitmap per detector |
| **Event Times** | Detectors | Event time distribution per detector (`Correlations` module output) |
| **Cluster Size** | Detectors | Cluster size per detector (MALTA2 has no charge) |
| **L1idC** | Detectors | l1idC vs event, wall-clock timer vs event, inter-plane l1idC delta, coincidence rate |
| **Correlation 1D X/Y** | Correlations 1D | 1D X and Y correlations per detector |
| **Correlation 2D X/Y** | Correlations 2D | 2D local X and Y correlations per detector |
| **DUTs** | DUTs | Per-DUT canvas: pixel hitmap, local residual X / Y (computed by OnlineMonitor, see **Residual Trend**), chip efficiency map (`AnalysisEfficiency`) — configurable via `dut_plots` |

Plot paths support three substitution keywords:
- `%DETECTOR%` — expanded for every non-auxiliary, non-passive detector
- `%DUT%` — replaced with each configured DUT name
- `%REFERENCE%` — replaced with the reference detector name

Add `"log"` as the draw option to switch the Y axis to a logarithmic scale.

---

### 🔢 Event counter

There are two independent notions of "event" in this module:

- **Displayed Events count** — status bar, progress bar and `target_events`: how much usable statistics
  the run has collected. Counts only **coincident l1ids**: it advances by one
  when every non-DUT, non-auxiliary, non-passive plane has at least one hit in the event. Because
  `EventLoaderMALTA` builds one event per l1id and puts only that l1id's hits on the clipboard, this is
  the same as "the hits on all reference planes share the same l1id". DUTs are exempt, so a DUT
  inefficiency does not stall the count. This is not configurable.
- **Every l1id** (one per `run()` call, coincident or not) — the X-axis of the Timeline / Corr Trends /
  Res Trends canvases, the redraw cadence (`update`), and the status bar's `Rate: X evt/s`. If these
  followed the coincident count, they would all freeze the moment one plane stops (no l1id is
  coincident any more) — exactly when the Timeline should show that plane's clusters/event dropping
  to 0 while the others carry on.

The coincidence-only count exists because Corryvreckan's MALTA2 event definition can overcount: every
l1id becomes an event, even one only a single plane saw (see the module header comment in
`EventLoaderMALTA` and its README).

---

### 📐 Residual Trend

The **Res Trends** canvas overlays a live residual X (top) / Y (bottom) vs. event TProfile per
detector, like Corr Trends (X-axis: every l1id). Unlike the Residuals canvas
(config-driven via `residuals` / `residuals_y`, which exclude DUTs), these trends are added for every detector
including DUTs, and are computed by `OnlineMonitor` itself rather than read from another module's
histogram. They used to share the residuals canvas, which made 20 pads too small to read and slow
to redraw:

- **Non-DUT (reference/tracking) planes**: `Track::getLocalResidual()` — the same quantity
  `Tracking4D` itself fills into `LocalResidualsX`/`Y` — for every track that actually used that
  plane in its fit.
- **DUT planes**: not part of the fit, so `getLocalResidual()` has no entry for them. Computed the
  same way `AnalysisDUT` does instead: the track's local intercept at the DUT minus the closest
  `DUTAssociation`-associated cluster's local position (`Track::getClosestCluster()`). A track with
  no associated DUT cluster that event contributes nothing (not a spurious 0) — a flat or gappy DUT
  trend is a genuine efficiency signal, not missing data.

The DUT canvas's residual X / Y distributions are filled from these same DUT values, which is why
the chain does not need `AnalysisDUT` (it books ~4 GB of histograms the monitor never shows).

Watching this trend live is a quick way to catch drifting alignment or a developing tracking
problem mid-run, without waiting for the end-of-run residual distributions.

---

### ⌨️ Keyboard Shortcuts

| Key | Action |
|---|---|
| `Space` | Pause / Resume monitoring |
| `F` | Open current canvas in full screen |
| `S` | Save current canvas as timestamped PNG |
| `1` | Overview |
| `2` | Tracking Performance |
| `3` | Residuals X |
| `4` | Residuals Y |
| `5` | Res Trends |
| `6` | Timeline |
| `7` | Corr Trends |
| `8` | Hitmaps |
| `9` | Event Times |

---

### ⚙️ Parameters

#### General

| Parameter | Default | Description |
|---|---|---|
| `update` | `200` | l1ids (coincident or not) between canvas refreshes, at most one refresh per second |
| `canvas_title` | `"Corryvreckan Testbeam Monitor"` | GUI window title |
| `ignore_aux` | `true` | Exclude detectors with auxiliary role |
| `clustering_module` | `"Clustering4D"` | Source module for clustering plots |
| `tracking_module` | `"Tracking4D"` | Source module for tracking plots |
| `target_events` | `0` | Target number of coincident l1ids for the progress bar (`0` = disabled) |
| `display_max_bins_1d` | `2000` | 1D histograms with more bins are drawn through a coarser display-only copy (`0` = never). The original histogram and the output file are unchanged |
| `display_max_bins_2d` | `300` | Same per axis for 2D histograms |
| `auto_save_interval` | `0` | Auto-save interval in seconds (`0` = disabled) |
| `auto_save_dir` | `"./"` | Output directory for auto-saved PNGs |

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
| `residuals` | Residuals X |
| `residuals_y` | Residuals Y |
| `correlation_x` | Correlation 1D X |
| `correlation_y` | Correlation 1D Y |
| `correlation_x2d` | Correlation 2D X |
| `correlation_y2d` | Correlation 2D Y |
| `cluster_size` | Cluster Size |
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

dut_plots = [["Correlations/%DUT%/hitmap", "colz"], ["OnlineMonitor/dut_residual_x_%DUT%"], ["OnlineMonitor/dut_residual_y_%DUT%"], ["AnalysisEfficiency/%DUT%/chipEfficiencyMap_trackPos_TProfile", "colz"]]
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
| 📊 **マルチキャンバス表示** | ヒットマップ・トラッキング・残差・相関・クラスターサイズ・タイミング・L1idC・アライメントドリフトをカバーする 16 キャンバス |
| 📈 **タイムラインキャンバス** | ローリングTProfile：上段に検出器ごとのclusters/event、下段にtracks/event |
| 📉 **Corr Trends キャンバス** | イベント番号に対する ΔX / ΔY グローバル [mm] — アライメントドリフトの早期検出 |
| 🔢 **L1idC キャンバス** | l1idC vs イベント・実時間 vs イベント・プレーン間 l1idC ズレ・coincidence rate — DAQ 品質を一画面で確認 |
| 📏 **プログレスバー** | 目標イベント数に向けた色付きカウンター（青→橙→緑） |
| 📺 **全画面モード** | ワンクリックまたはキー操作で任意のキャンバスをフルディスプレイ解像度に拡大 |
| 💾 **自動保存** | タイムスタンプ付きPNGへの定期キャンバスエクスポート |
| ⌨️ **キーボードショートカット** | よく使う操作をすべてSpace・F・S・数字キーで実行可能 |
| 📋 **ステータスバー** | イベント数・トリガーレート・現在のキャンバスと直近の描画時間・現在時刻をリアルタイム表示。100 msを超えた描画はWARNINGとしてログにも出る |

---

### 🖼️ キャンバス一覧

| キャンバス | グループ | デフォルト表示内容 |
|---|---|---|
| **Overview** | Tracking | リファレンス：クラスター電荷、ヒットマップ、残差X |
| **Tracking Performance** | Tracking | χ²、角度X/Y、χ²/ndf、tracks/event、clusters/track |
| **Residuals X** | Tracking | 検出器ごとのローカル残差X（DUT除く） |
| **Residuals Y** | Tracking | 検出器ごとのローカル残差Y（DUT除く） |
| **Res Trends** | Tracking | ローカル残差X(上段)/Y(下段) vs l1id。DUT含む全検出器をCorr Trendsのように重ね描き — 詳細は下記「Residualトレンド」参照 |
| **Timeline** | Tracking | 検出器ごとのclusters/event（上段）+ tracks/event（下段）ローリング |
| **Corr Trends** | Tracking | 非リファレンス検出器ごとの ΔX / ΔY グローバル [mm] vs イベント番号 — アライメントドリフト検出 |
| **Hitmaps** | Detectors | 検出器ごとの2Dヒットマップ |
| **Event Times** | Detectors | 検出器ごとのイベント時刻分布（`Correlations` モジュール出力） |
| **Cluster Size** | Detectors | 検出器ごとのクラスターサイズ（MALTA2は電荷を持たない） |
| **L1idC** | Detectors | l1idC vs イベント、実時間 vs イベント、プレーン間 l1idC ズレ、coincidence rate — DAQ 品質一覧 |
| **Correlation 1D X/Y** | Correlations 1D | 検出器ごとの1D相関（X・Y） |
| **Correlation 2D X/Y** | Correlations 2D | 検出器ごとの2Dローカル相関（X・Y） |
| **DUTs** | DUTs | DUTごとのキャンバス: ピクセルヒットマップ、ローカル残差X / Y（OnlineMonitorが計算、「Residualトレンド」参照）、チップ効率マップ（`AnalysisEfficiency`） — `dut_plots`で変更可 |

プロットパスには以下の置換キーワードが使用できます：
- `%DETECTOR%` — Auxiliary・Passive以外の全検出器に展開
- `%DUT%` — 設定された各DUT名に置換
- `%REFERENCE%` — リファレンス検出器名に置換

描画オプションに`"log"`を指定するとY軸を対数スケールに切り替えます。

---

### 🔢 Eventカウンタ

このモジュールには独立した2種類の「event」概念があります。

- **表示上のEvents数** — ステータスバー・プログレスバー・`target_events`。ランで使える統計がどれだけ
  溜まったかを表す。**l1idが揃ったeventだけ**を数える: DUT・Auxiliary・
  Passive以外の全面に1ヒット以上あるeventで+1。`EventLoaderMALTA`は1 l1id = 1 eventを作り、その
  l1idのヒットだけをclipboardに載せるため、これは「全リファレンス面のヒットのl1idが同じ」と等価。
  DUTは条件に含めないので、DUTの非効率でカウントが止まることはない。設定での切り替えはできない。
- **全l1id**(`run()`1回につき1、揃ったかに関わらず) — Timeline / Corr Trends / Res TrendsのX軸、
  再描画のタイミング(`update`)、ステータスバーの`Rate: X evt/s`。これらを揃ったevent数で進めると、
  1面が止まった瞬間(どのl1idも揃わなくなる)にすべて止まってしまう。まさにその時こそ、Timelineで
  止まった面のclusters/eventだけが0に落ちる様子を見せる必要がある。

l1idが揃ったものだけを数えるのは、MALTA2のCorryvreckan event定義が実際より多く数えてしまうため
(1面しか見ていないl1idもeventになる。`EventLoaderMALTA`のREADMEおよびモジュール冒頭コメント参照)。

---

### 📐 Residualトレンド

**Res Trends**キャンバスには、検出器ごとのresidual X(上段) / Y(下段) vs event TProfileが
Corr Trendsのように重ねてライブ表示される(X軸は全l1id)。Residuals
キャンバス(config駆動の`residuals`/`residuals_y`、DUTを除外)とは異なり、これらのトレンドは**DUTも含めた
全検出器**に対して追加され、他モジュールのヒストグラムを読むのではなく`OnlineMonitor`自身が
計算する。以前はResidualsキャンバスに同居していたが、20パッドになり小さすぎて読めず、
再描画も重かったため分離した:

- **非DUT(参照/トラッキング)プレーン**: `Track::getLocalResidual()` — `Tracking4D`自身が
  `LocalResidualsX`/`Y`に埋めているのと同じ量 — を、そのプレーンが実際にフィットに使われた
  track全てについて使用
- **DUTプレーン**: フィットに使われないため`getLocalResidual()`にはエントリが無い。
  `AnalysisDUT`と同じ方法で計算: trackのDUTでのローカル交点から、最も近い
  `DUTAssociation`紐付けクラスタ(`Track::getClosestCluster()`)のローカル位置を引く。
  そのeventで紐付けクラスタが無いtrackは何も寄与しない(見せかけの0にはならない) —
  DUTトレンドが平坦・途切れ途切れになるのは、データ欠損ではなく本物の効率低下のサイン

DUTキャンバスのresidual X / Y分布も、このDUTの値から作っている。そのためchainに`AnalysisDUT`は
不要(モニタが表示しないヒストグラムを約4 GB確保する)。

このトレンドをライブで見ることで、ラン終了後のresidual分布を待たずに、アライメントの
ドリフトやトラッキングの不調をラン中に早期発見できる。

---

### ⌨️ キーボードショートカット

| キー | 動作 |
|---|---|
| `Space` | モニタリングの一時停止 / 再開 |
| `F` | 現在のキャンバスを全画面表示 |
| `S` | 現在のキャンバスをタイムスタンプ付きPNGで保存 |
| `1` | Overview へ切り替え |
| `2` | Tracking Performance へ切り替え |
| `3` | Residuals X へ切り替え |
| `4` | Residuals Y へ切り替え |
| `5` | Res Trends へ切り替え |
| `6` | Timeline へ切り替え |
| `7` | Corr Trends へ切り替え |
| `8` | Hitmaps へ切り替え |
| `9` | Event Times へ切り替え |

---

### ⚙️ パラメータ

#### 基本設定

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `update` | `200` | キャンバス更新間隔（揃ったかに関わらず全l1idの数。ただし更新は最大でも1秒に1回） |
| `canvas_title` | `"Corryvreckan Testbeam Monitor"` | GUIウィンドウのタイトル |
| `ignore_aux` | `true` | Auxiliaryロールの検出器を除外する |
| `clustering_module` | `"Clustering4D"` | クラスタリングプロットのソースモジュール名 |
| `tracking_module` | `"Tracking4D"` | トラッキングプロットのソースモジュール名 |
| `target_events` | `0` | プログレスバーの目標値（l1idが揃ったevent数、`0`で無効） |
| `display_max_bins_1d` | `2000` | これより多いビンの1Dヒストグラムは、表示専用の粗いコピーで描画する（`0`で無効）。元のヒストグラムと出力ファイルは変わらない |
| `display_max_bins_2d` | `300` | 2Dヒストグラムの軸ごとの同様の上限 |
| `auto_save_interval` | `0` | 自動保存の間隔（秒、`0`で無効） |
| `auto_save_dir` | `"./"` | 自動保存PNGの出力先ディレクトリ |

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
| `residuals` | Residuals X |
| `residuals_y` | Residuals Y |
| `correlation_x` | Correlation 1D X |
| `correlation_y` | Correlation 1D Y |
| `correlation_x2d` | Correlation 2D X |
| `correlation_y2d` | Correlation 2D Y |
| `cluster_size` | Cluster Size |
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

dut_plots = [["Correlations/%DUT%/hitmap", "colz"], ["OnlineMonitor/dut_residual_x_%DUT%"], ["OnlineMonitor/dut_residual_y_%DUT%"], ["AnalysisEfficiency/%DUT%/chipEfficiencyMap_trackPos_TProfile", "colz"]]
```
