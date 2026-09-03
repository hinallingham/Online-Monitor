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
| 📊 **Multi-canvas display** | 13 canvases covering hitmaps, tracking, correlations, charge, timing, L1idC, and alignment drift |
| 📈 **Timeline canvas** | Rolling TProfile: clusters/event per detector (top) + tracks/event (bottom) |
| 📉 **Corr Trends canvas** | Per-event ΔX / ΔY global [mm] vs event number — early warning for alignment drift |
| 🔢 **L1idC canvas** | l1idC vs event, wall-clock time vs event, inter-plane sync delta, coincidence rate — DAQ health at a glance |
| 🔭 **Telescope canvas** | Live 3D track intercepts and cluster hits in Z-X / Z-Y projections |
| ⚠️ **Warning mode** | GUI-only low-rate warning (status bar turns red) when clusters/event or tracks/event drops below threshold |
| 📏 **Progress bar** | Color-coded event counter (blue → orange → green) toward a configurable target |
| 📺 **Full-screen mode** | Expand any canvas to full display resolution with one click or keypress |
| 💾 **Auto-save** | Periodic canvas export as timestamped PNG |
| ⌨️ **Keyboard shortcuts** | Space, F, S, and digit keys for all common actions |
| 📋 **Status bar** | Live event count, trigger rate, current canvas, and wall-clock time |

---

### 🖼️ Canvases

| Canvas | Group | Default content |
|---|---|---|
| **Overview** | Tracking | Reference: cluster charge, hitmap, residual X |
| **Tracking Performance** | Tracking | χ², angle X/Y, χ²/ndf, tracks/event, clusters/track |
| **Residuals** | Tracking | Local residual X per detector, plus a live Residual X/Y vs. event trend per detector (incl. DUTs) — see **Residual Trend** below |
| **Timeline** | Tracking | Clusters/event per detector (top) + tracks/event (bottom), rolling TProfile |
| **Telescope** | Tracking | Live track intercepts and cluster hit positions in Z-X / Z-Y projections |
| **Corr Trends** | Tracking | ΔX / ΔY global [mm] vs event number per non-reference detector — alignment drift monitor |
| **Hitmaps** | Detectors | 2D hitmap per detector |
| **Event Times** | Detectors | Event time distribution per detector (`Correlations` module output) |
| **Charge Distributions** | Detectors | Cluster charge per detector |
| **L1idC** | Detectors | l1idC vs event, wall-clock timer vs event, inter-plane l1idC delta, coincidence rate |
| **Correlation 1D X/Y** | Correlations 1D | 1D X and Y correlations per detector |
| **Correlation 2D X/Y** | Correlations 2D | 2D local X and Y correlations per detector |
| **DUTs** | DUTs | Per-DUT canvas — fully user-configurable |

Plot paths support three substitution keywords:
- `%DETECTOR%` — expanded for every non-auxiliary, non-passive detector
- `%DUT%` — replaced with each configured DUT name
- `%REFERENCE%` — replaced with the reference detector name

Add `"log"` as the draw option to switch the Y axis to a logarithmic scale.

---

### 🔢 Event counter

There are two independent notions of "event" in this module:

- **Displayed Events count** — status bar, progress bar, and the X-axis of the Timeline/Corr Trends
  canvases. Selected by `event_counter`: one per `run()` call as before (`"run_count"`), cumulative
  `Tracking4D` tracks (`"tracks"`), or cumulative tracks associated to a DUT cluster
  (`"dut_associated_tracks"`, e.g. via `DUTAssociation`). With either track-based mode, this count can
  advance by more than one — or not at all — per `run()` call, since one Corryvreckan event can contain
  several tracks or none.
- **Rate-style metrics' denominator** — the status bar's `Rate: X evt/s` and warning mode's
  `clusters/event`/`tracks/event` thresholds. These always use one per `run()` call, regardless of
  `event_counter`: if "event" itself meant "a track", `tracks/event` would be meaningless (numerator and
  denominator would be the same quantity).

This split exists because the two `Tracking4D`-based `event_counter` modes were added specifically to
work around a Corryvreckan event definition that can overcount (see the module header comment in
`EventLoaderMALTA` and its README) — the displayed count needed to change, but the rate metrics deriving
from it should not silently change meaning.

---

### 📐 Residual Trend

The Residuals canvas includes a live `Residual X vs. Event` / `Residual Y vs. Event` TProfile pair
per detector, in addition to the static `LocalResidualsX` distribution. Unlike the rest of that
canvas (config-driven via `residuals`, which excludes DUTs), these two trends are added for every
detector including DUTs, and are computed by `OnlineMonitor` itself rather than read from another
module's histogram:

- **Non-DUT (reference/tracking) planes**: `Track::getLocalResidual()` — the same quantity
  `Tracking4D` itself fills into `LocalResidualsX`/`Y` — for every track that actually used that
  plane in its fit.
- **DUT planes**: not part of the fit, so `getLocalResidual()` has no entry for them. Computed the
  same way `AnalysisDUT` does instead: the track's local intercept at the DUT minus the closest
  `DUTAssociation`-associated cluster's local position (`Track::getClosestCluster()`). A track with
  no associated DUT cluster that event contributes nothing (not a spurious 0) — a flat or gappy DUT
  trend is a genuine efficiency signal, not missing data.

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
| `3` | Residuals |
| `4` | Timeline |
| `5` | Hitmaps |
| `6` | Event Times |
| `7` | Charge Distributions |
| `8` | Correlation 1D X |
| `9` | Correlation 1D Y |

---

### ⚙️ Parameters

#### General

| Parameter | Default | Description |
|---|---|---|
| `update` | `200` | Events between canvas refreshes (in whatever unit `event_counter` selects) |
| `canvas_title` | `"Corryvreckan Testbeam Monitor"` | GUI window title |
| `ignore_aux` | `true` | Exclude detectors with auxiliary role |
| `clustering_module` | `"Clustering4D"` | Source module for clustering plots |
| `tracking_module` | `"Tracking4D"` | Source module for tracking plots |
| `event_counter` | `"run_count"` | What the displayed Events count (status bar, progress bar, Timeline/Corr Trends X-axis) counts: `"run_count"` (one per module `run()` call, i.e. one per Corryvreckan event as usual), `"tracks"` (cumulative `Tracking4D` tracks found), or `"dut_associated_tracks"` (cumulative tracks with an associated cluster on `event_counter_dut`, e.g. via `DUTAssociation`). Does **not** affect the status bar rate or warning-mode "per event" metrics, which always use one-per-`run()` as their denominator regardless of this setting — see "Event counter" below. |
| `event_counter_dut` | `""` | Which DUT's associated tracks to count when `event_counter = "dut_associated_tracks"`. Required if the geometry has more than one DUT; auto-selected if there is exactly one. Throws a configuration error at startup if `event_counter = "dut_associated_tracks"` but no DUT is present, or if this names a detector that isn't a DUT. |
| `target_events` | `0` | Target count for progress bar, in `event_counter` units (`0` = disabled) |
| `auto_save_interval` | `0` | Auto-save interval in seconds (`0` = disabled) |
| `auto_save_dir` | `"./"` | Output directory for auto-saved PNGs |
| `warning_min_clusters_per_event` | `0.0` | Warning mode threshold: mean clusters/event across planes (`0` = disabled) |
| `warning_min_tracks_per_event` | `0.0` | Warning mode threshold: tracks/event (`0` = disabled) |
| `warning_duration` | `10` | Seconds below either warning threshold before warning mode activates |

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
event_counter = "dut_associated_tracks"
event_counter_dut = "MALTA_1"
target_events = 1000000
auto_save_interval = 60
auto_save_dir = "/data/snapshots/"
warning_min_tracks_per_event = 0.5
warning_duration = 20

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
| 📊 **マルチキャンバス表示** | ヒットマップ・トラッキング・相関・電荷・タイミング・L1idC・アライメントドリフトをカバーする 13 キャンバス |
| 📈 **タイムラインキャンバス** | ローリングTProfile：上段に検出器ごとのclusters/event、下段にtracks/event |
| 📉 **Corr Trends キャンバス** | イベント番号に対する ΔX / ΔY グローバル [mm] — アライメントドリフトの早期検出 |
| 🔢 **L1idC キャンバス** | l1idC vs イベント・実時間 vs イベント・プレーン間 l1idC ズレ・coincidence rate — DAQ 品質を一画面で確認 |
| 🔭 **Telescope キャンバス** | ライブトラック交点とクラスターヒットを Z-X / Z-Y 投影で表示 |
| ⚠️ **Warning mode** | clusters/eventまたはtracks/eventが閾値を下回るとステータスバーが赤くなるGUI限定の低レート警告 |
| 📏 **プログレスバー** | 目標イベント数に向けた色付きカウンター（青→橙→緑） |
| 📺 **全画面モード** | ワンクリックまたはキー操作で任意のキャンバスをフルディスプレイ解像度に拡大 |
| 💾 **自動保存** | タイムスタンプ付きPNGへの定期キャンバスエクスポート |
| ⌨️ **キーボードショートカット** | よく使う操作をすべてSpace・F・S・数字キーで実行可能 |
| 📋 **ステータスバー** | イベント数・トリガーレート・現在のキャンバス・現在時刻をリアルタイム表示 |

---

### 🖼️ キャンバス一覧

| キャンバス | グループ | デフォルト表示内容 |
|---|---|---|
| **Overview** | Tracking | リファレンス：クラスター電荷、ヒットマップ、残差X |
| **Tracking Performance** | Tracking | χ²、角度X/Y、χ²/ndf、tracks/event、clusters/track |
| **Residuals** | Tracking | 検出器ごとのローカル残差X、および検出器ごと(DUT含む)のResidual X/Y vs eventトレンド — 詳細は下記「Residualトレンド」参照 |
| **Timeline** | Tracking | 検出器ごとのclusters/event（上段）+ tracks/event（下段）ローリング |
| **Telescope** | Tracking | ライブトラック交点・クラスターヒット位置をZ-X / Z-Y投影で表示 |
| **Corr Trends** | Tracking | 非リファレンス検出器ごとの ΔX / ΔY グローバル [mm] vs イベント番号 — アライメントドリフト検出 |
| **Hitmaps** | Detectors | 検出器ごとの2Dヒットマップ |
| **Event Times** | Detectors | 検出器ごとのイベント時刻分布（`Correlations` モジュール出力） |
| **Charge Distributions** | Detectors | 検出器ごとのクラスター電荷 |
| **L1idC** | Detectors | l1idC vs イベント、実時間 vs イベント、プレーン間 l1idC ズレ、coincidence rate — DAQ 品質一覧 |
| **Correlation 1D X/Y** | Correlations 1D | 検出器ごとの1D相関（X・Y） |
| **Correlation 2D X/Y** | Correlations 2D | 検出器ごとの2Dローカル相関（X・Y） |
| **DUTs** | DUTs | DUTごとのキャンバス（完全にユーザー設定可能） |

プロットパスには以下の置換キーワードが使用できます：
- `%DETECTOR%` — Auxiliary・Passive以外の全検出器に展開
- `%DUT%` — 設定された各DUT名に置換
- `%REFERENCE%` — リファレンス検出器名に置換

描画オプションに`"log"`を指定するとY軸を対数スケールに切り替えます。

---

### 🔢 Eventカウンタ

このモジュールには独立した2種類の「event」概念があります。

- **表示上のEvents数** — ステータスバー・プログレスバー・Timeline/Corr TrendsキャンバスのX軸。
  `event_counter`で選択: 従来通り`run()`1回につき1(`"run_count"`)、`Tracking4D`が見つけたtrackの累積数
  (`"tracks"`)、DUTのクラスタに紐付いたtrackの累積数(`"dut_associated_tracks"`、`DUTAssociation`等が対象)。
  track系のモードでは、1回の`run()`で複数trackが見つかれば2以上進み、0本なら進まない。
- **レート系指標の分母** — ステータスバーの`Rate: X evt/s`、warning modeの`clusters/event`/`tracks/event`。
  `event_counter`の設定に関わらず、常に`run()`1回につき1を分母として使う。もし「event」自体が
  「1本のtrack」を意味してしまうと、`tracks/event`が分子と分母が同じものになり意味を失うため。

この分離が存在する理由は、track系の`event_counter`モードが「Corryvreckanのevent定義が実際の数より
増えてしまう問題」(`EventLoaderMALTA`のREADMEおよびモジュール冒頭コメント参照)への対処として
追加されたものだからです — 表示上のカウントは変える必要があった一方、そこから派生するレート系指標の
意味までは変えたくありませんでした。

---

### 📐 Residualトレンド

Residualsキャンバスには、静的な`LocalResidualsX`分布に加えて、検出器ごとの
`Residual X vs Event` / `Residual Y vs Event` TProfileがライブで表示される。このキャンバスの
他の項目(config駆動の`residuals`、DUTを除外)とは異なり、この2つのトレンドは**DUTも含めた
全検出器**に対して追加され、他モジュールのヒストグラムを読むのではなく`OnlineMonitor`自身が
計算する:

- **非DUT(参照/トラッキング)プレーン**: `Track::getLocalResidual()` — `Tracking4D`自身が
  `LocalResidualsX`/`Y`に埋めているのと同じ量 — を、そのプレーンが実際にフィットに使われた
  track全てについて使用
- **DUTプレーン**: フィットに使われないため`getLocalResidual()`にはエントリが無い。
  `AnalysisDUT`と同じ方法で計算: trackのDUTでのローカル交点から、最も近い
  `DUTAssociation`紐付けクラスタ(`Track::getClosestCluster()`)のローカル位置を引く。
  そのeventで紐付けクラスタが無いtrackは何も寄与しない(見せかけの0にはならない) —
  DUTトレンドが平坦・途切れ途切れになるのは、データ欠損ではなく本物の効率低下のサイン

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
| `3` | Residuals へ切り替え |
| `4` | Timeline へ切り替え |
| `5` | Hitmaps へ切り替え |
| `6` | Event Times へ切り替え |
| `7` | Charge Distributions へ切り替え |
| `8` | Correlation 1D X へ切り替え |
| `9` | Correlation 1D Y へ切り替え |

---

### ⚙️ パラメータ

#### 基本設定

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `update` | `200` | キャンバス更新間隔（`event_counter`が選ぶ単位でのイベント数） |
| `canvas_title` | `"Corryvreckan Testbeam Monitor"` | GUIウィンドウのタイトル |
| `ignore_aux` | `true` | Auxiliaryロールの検出器を除外する |
| `clustering_module` | `"Clustering4D"` | クラスタリングプロットのソースモジュール名 |
| `tracking_module` | `"Tracking4D"` | トラッキングプロットのソースモジュール名 |
| `event_counter` | `"run_count"` | 表示上のEvents数(ステータスバー・プログレスバー・Timeline/Corr TrendsのX軸)が何を数えるか: `"run_count"`(従来通り`run()`1回につき1)、`"tracks"`(`Tracking4D`が見つけたtrackの累積数)、`"dut_associated_tracks"`(`event_counter_dut`にクラスタが紐付いたtrackの累積数、`DUTAssociation`等を想定)。ステータスバーのレート表示・warning modeの"per event"系指標には影響しない(常に`run()`1回=1を分母に使う。詳細は下の「Eventカウンタ」参照) |
| `event_counter_dut` | `""` | `event_counter = "dut_associated_tracks"`の時に、どのDUTへの紐付けtrackを数えるか。ジオメトリにDUTが複数あれば必須、1枚だけなら自動選択。DUTが1枚もない、または指定した検出器がDUTでない場合は起動時に設定エラーになる |
| `target_events` | `0` | プログレスバーの目標値（`event_counter`の単位、`0`で無効） |
| `auto_save_interval` | `0` | 自動保存の間隔（秒、`0`で無効） |
| `auto_save_dir` | `"./"` | 自動保存PNGの出力先ディレクトリ |
| `warning_min_clusters_per_event` | `0.0` | Warning mode閾値: 全プレーン平均のclusters/event（`0`で無効） |
| `warning_min_tracks_per_event` | `0.0` | Warning mode閾値: tracks/event（`0`で無効） |
| `warning_duration` | `10` | いずれかのwarning閾値を下回ってからwarning modeが有効になるまでの秒数 |

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
event_counter = "dut_associated_tracks"
event_counter_dut = "MALTA_1"
target_events = 1000000
auto_save_interval = 60
auto_save_dir = "/data/snapshots/"
warning_min_tracks_per_event = 0.5
warning_duration = 20

dut_plots = [["EventLoaderEUDAQ2/%DUT%/hitmap",                          "colz"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelTimes"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelRawValues"],
             ["EventLoaderEUDAQ2/%DUT%/hPixelMultiplicityPerCorryEvent",  "log"],
             ["AnalysisDUT/%DUT%/clusterChargeAssociated"],
             ["AnalysisDUT/%DUT%/associatedTracksVersusTime"]]
```
