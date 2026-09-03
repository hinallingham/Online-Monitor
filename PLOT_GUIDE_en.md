---
# SPDX-FileCopyrightText: 2024 CERN and the Corryvreckan authors
# SPDX-License-Identifier: CC-BY-4.0 OR MIT
---

<div align="right">
  maintainer: Hinata Nakamura
</div>

# OnlineMonitor Plot Guide

A reference describing every plot displayed by the OnlineMonitor during a beam test,
along with **what to look for** in each canvas.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Tracking Performance](#2-tracking-performance)
3. [Residuals](#3-residuals)
4. [Timeline](#4-timeline)
5. [Telescope](#5-telescope)
6. [Corr Trends](#6-corr-trends)
7. [Hitmaps](#7-hitmaps)
8. [Event Times](#8-event-times)
9. [Charge Distributions](#9-charge-distributions)
10. [L1idC](#10-l1idc)
11. [Correlation 1D X / Y](#11-correlation-1d-x--y)
12. [Correlation 2D X / Y](#12-correlation-2d-x--y)
13. [DUTs](#13-duts)
14. [Priority Checklist During Beam Tests](#priority-checklist-during-beam-tests)

---

## 1. Overview

A summary canvas for a quick assessment of tracking quality and reference detector health.

| Plot | Axes | Description |
|------|------|-------------|
| `Tracking4D/trackChi2ndof` | X: χ²/ndf, Y: events | Distribution of the track-fit χ² divided by degrees of freedom |
| `ClusteringSpatial/%REF%/clusterCharge` | X: charge, Y: events | Cluster charge distribution of the reference plane |
| `Correlations/%REF%/hitmap` | X: X [mm], Y: Y [mm] | 2D hit map of the reference plane |
| `Tracking4D/%REF%/local_residuals/LocalResidualsX` | X: residual X [mm], Y: events | Track residual in X for the reference plane |

### What to look for during beam tests

- **χ²/ndf** should peak around 1–2. A large deviation indicates misalignment or high noise occupancy.
- **clusterCharge** should follow a Landau distribution (asymmetric, with a long tail at low charge). A shift of the MPV (Most Probable Value) over time suggests a change in detector condition.
- **hitmap** should show a uniform beam spot at the expected position. A clipped or off-centre spot requires beam steering adjustment.
- **LocalResidualsX** peak should be at 0. Any offset calls for a realignment.

---

## 2. Tracking Performance

Detailed tracking quality metrics.

| Plot | Axes | Description |
|------|------|-------------|
| `trackChi2` | X: χ², Y: events | Raw χ² distribution before dividing by degrees of freedom |
| `trackAngleX` | X: angle X [rad], Y: events | Distribution of track inclination angle in X |
| `trackAngleY` | X: angle Y [rad], Y: events | Distribution of track inclination angle in Y |
| `trackChi2ndof` | X: χ²/ndf, Y: events | χ²/ndf (same as Overview) for detailed inspection |
| `tracksPerEvent` | X: number of tracks, Y: events | Distribution of the number of tracks per event |
| `clustersPerTrack` | X: number of clusters, Y: events | Number of clusters assigned to each track |

### What to look for during beam tests

- **trackAngleX / Y** should be centred at 0 and consistent with the beam design angle. An overly broad distribution indicates excessive multiple scattering or a divergent beam.
- **tracksPerEvent** should peak at 1. Frequent values ≥ 2 suggest a high beam rate or many noise tracks.
- **clustersPerTrack** should peak at the number of planes (e.g. 3 for a 3-plane telescope). A lower peak means at least one plane has reduced detection efficiency.

---

## 3. Residuals

Monitors the spatial resolution of each plane.

| Plot | Axes | Description |
|------|------|-------------|
| `Tracking4D/%DETECTOR%/local_residuals/LocalResidualsX` | X: residual X [mm], Y: events | Difference between the predicted track position and the measured cluster position in X, per plane |

### What to look for during beam tests

- **All planes should peak at 0.** A plane with an offset peak needs its alignment parameters updated.
- **The width (RMS or FWHM/2.35) corresponds to the spatial resolution.** If it is wider than the design value, review the clustering settings or noise cuts.
- A distribution that gradually broadens during a run may indicate alignment drift caused by temperature changes or mechanical movement (→ also check Corr Trends).

---

## 4. Timeline

Monitors the evolution of cluster counts and track counts as a function of event number.  
**One of the most frequently checked canvases during a beam test.**

| Pad | Plot | Axes | Description |
|-----|------|------|-------------|
| Top (65%) | `hits_tl_<detector>` (colour-coded per detector) | X: event number, Y: clusters/event | Clusters per event per plane over time |
| Bottom (33%) | `tracks_tl` | X: event number, Y: tracks/event | Tracks per event over time |

> **Cluster**: a group of adjacent pixels that fired when a particle passed through.  
> **clusters/event**: the mean number of clusters found per trigger (= per l1idC).

### What to look for during beam tests

- **All planes should show similar, stable clusters/event values.**
- A single plane dropping → check the power, cable, and bias voltage of that plane.
- All planes dropping simultaneously → beam rate has decreased (accelerator-side issue).
- All planes rising simultaneously → increased noise or higher beam rate.
- **tracks/event suddenly drops to near zero** → tracking has broken down; also check the coincidence rate in the L1idC canvas.

---

## 5. Telescope

A geometric 3D view of the telescope. Track intercepts and cluster hit positions are shown in two Z projections.

| Pad | Plot | Axes | Description |
|-----|------|------|-------------|
| Top | `tel_xz` (TH2F) + TGraph | X: Z [mm], Y: X [mm] | Accumulated cluster hit map (colz) overlaid with up to 50 live tracks |
| Bottom | `tel_yz` (TH2F) + TGraph | X: Z [mm], Y: Y [mm] | Same as above in the Y direction |

Each track is drawn as a colour-coded polyline connecting the track intercept at each detector plane.

### What to look for during beam tests

- **Tracks should coincide with cluster hits at every plane position.** A mismatch indicates misalignment.
- **Tracks should appear as straight lines.** Kinks or bends suggest a large alignment error.
- **The accumulated hit map should show a normal beam spot shape.** A spread that appears at only one Z position suggests excess scattering near that plane.

---

## 6. Corr Trends

Early detection of alignment drift. Plots the mean global-coordinate difference between each plane and the reference as a function of event number.

| Pad | Plot | Axes | Description |
|-----|------|------|-------------|
| Top (55%) | `corr_x_tl_<detector>` (colour-coded) | X: event number, Y: ΔX_global [mm] | Mean(X_plane − X_reference) per event in the global X direction |
| Bottom (45%) | `corr_y_tl_<detector>` (colour-coded) | X: event number, Y: ΔY_global [mm] | Same in the global Y direction |

### What to look for during beam tests

- **Values should be constant (flat) throughout the run.** A slope developing over time means the detector is physically drifting.
- **A sudden step change** → mechanical shock or temperature jump; realignment is required.
- If all planes drift in the same direction, the reference plane itself may be moving.

---

## 7. Hitmaps

2D spatial distribution of hits across all detectors.

| Plot | Axes | Description |
|------|------|-------------|
| `Correlations/%DETECTOR%/hitmap` | X: X [mm], Y: Y [mm] | 2D hit map per plane (colz) |

### What to look for during beam tests

- **The beam spot should be at the same position on all planes** (assuming a correct alignment).
- **Pixels, rows, or columns that are abnormally bright (hot pixels)** → consider adding them to the mask file.
- **Regions that are completely dark (dead pixels or dead columns)** → check the bias voltage and readout settings.

---

## 8. Event Times

Event time distribution, produced by the `Correlations` module.

| Plot | Axes | Description |
|------|------|-------------|
| `Correlations/%DETECTOR%/eventTimes` | X: time [ns], Y: events | Distribution of event timestamps per plane |

### What to look for during beam tests

- **All planes should show the same distribution shape.** A single plane offset in time indicates a clock synchronisation problem.
- **Periodic structure (bunch structure) should be visible** and consistent with the accelerator bunch spacing.

---

## 9. Charge Distributions

Cluster charge distribution, reflecting the operating condition of each detector.

| Plot | Axes | Description |
|------|------|-------------|
| `ClusteringSpatial/%DETECTOR%/clusterCharge` | X: charge, Y: events | Cluster charge distribution per plane |

### What to look for during beam tests

- **Should follow a Landau distribution** (asymmetric with a long low-charge tail).
- **The MPV should be consistent across planes.** A discrepancy indicates a need for gain calibration.
- **A sharp peak at low charge** → noise clusters are present; consider raising the clustering threshold.
- **A shift of the MPV over time** → check for temperature or bias voltage variation.

---

## 10. L1idC

DAQ synchronisation quality at a glance. **One of the most critical canvases during a beam test.**  
Displays histograms produced directly by EventLoaderMALTA.

| Plot | Axes | Description |
|------|------|-------------|
| `hL1idCVsEvent` (TProfile) | X: Corryvreckan event number, Y: l1idC value | Correspondence between the event number and the hardware trigger counter |
| `hTimerVsEvent` (TProfile) | X: event number, Y: elapsed time [s] | Wall-clock time from MaltaDAQ vs. event number |
| `hL1idCPlaneDelta` (TH1F) | X: max(l1idC) − min(l1idC) [triggers], Y: events | Spread of l1idC across planes per event |
| `hCoincidenceRateTrend` (TProfile) | X: event number, Y: coincidence rate [%] | Fraction of events in which all planes have at least one hit |

> **l1idC**: 32-bit non-wrapping Level-1 ID Counter. Incremented by one for every hardware trigger. Used instead of the 12-bit `l1id` (which wraps at 4096) to avoid inter-plane synchronisation failures in long runs.

### Normal vs. abnormal for each plot

**hL1idCVsEvent**
- Normal: slope exactly **1.0** (straight line)
- Abnormal: slope > 1 → MaltaDAQ is missing triggers

**hTimerVsEvent**
- Normal: **straight line** for a constant beam rate
- Abnormal: changing slope → beam rate is varying; flat segments indicate effective dead time

**hL1idCPlaneDelta**
- Normal: **sharp peak at 0** (all planes share the same l1idC)
- Abnormal: counts appear at non-zero values → inter-plane synchronisation loss; check the logs to identify which plane is out of sync

**hCoincidenceRateTrend**
- Normal: **stable at a high value (80–100%)**
- Abnormal: begins to decrease → early sign of synchronisation loss; check together with hL1idCPlaneDelta
- Sudden drop → DAQ restart or alignment check required

---

## 11. Correlation 1D X / Y

1D spatial correlations between planes. Used for a coarse alignment check.

| Plot | Axes | Description |
|------|------|-------------|
| `Correlations/%DETECTOR%/correlationX` | X: ΔX [mm], Y: events | 1D X correlation peak with respect to the reference |
| `Correlations/%DETECTOR%/correlationY` | X: ΔY [mm], Y: events | 1D Y correlation peak with respect to the reference |

### What to look for during beam tests

- **The peak should be at 0.** An offset means the `displacement_x / y` alignment parameter needs updating.
- **The peak should be sharp.** A broad distribution indicates a large geometry mismatch or high noise.
- **No peak visible at all** → no correlation found; the alignment initial values need a large correction.

---

## 12. Correlation 2D X / Y

2D correlation maps. Detect rotational or scale misalignments.

| Plot | Axes | Description |
|------|------|-------------|
| `Correlations/%DETECTOR%/correlationX_2Dlocal` | X: detector X [mm], Y: reference X [mm] | 2D local-X correlation map (colz) |
| `Correlations/%DETECTOR%/correlationY_2Dlocal` | X: detector Y [mm], Y: reference Y [mm] | 2D local-Y correlation map (colz) |

### What to look for during beam tests

- Normal: **a narrow diagonal band**
- Band shifted parallel to the diagonal → translational offset (same as 1D)
- Band tilted → rotational misalignment; fix the `orientation` parameter
- Band curved → scale error; check the pixel pitch setting

---

## 13. DUTs

Individual canvas per DUT (Device Under Test). Default plot list:

| Plot | Axes | Description |
|------|------|-------------|
| `EventLoaderEUDAQ2/%DUT%/hitmap` | X: col, Y: row | 2D hit map of the DUT (colz); check for hot pixels |
| `EventLoaderEUDAQ2/%DUT%/hPixelTimes` | X: time [ns], Y: events | Pixel hit time distribution; check timing synchronisation with the telescope |
| `EventLoaderEUDAQ2/%DUT%/hPixelRawValues` | X: raw ADC value, Y: events | Raw ADC (e.g. ToT) distribution |
| `EventLoaderEUDAQ2/%DUT%/hPixelMultiplicityPerCorryEvent` | X: pixel count, Y: events (log) | Number of pixel hits per event (log scale) |
| `AnalysisDUT/%DUT%/clusterChargeAssociated` | X: charge, Y: events | Charge of clusters **associated with a track** (signal only) |
| `AnalysisDUT/%DUT%/associatedTracksVersusTime` | X: time, Y: associated tracks | Number of track-matched clusters vs. time (detection efficiency over time) |

### What to look for during beam tests

- **hPixelMultiplicityPerCorryEvent** mostly at 0 → DUT is not synchronised with the trigger.
- **clusterChargeAssociated MPV differs from the Hitmaps charge** → noise clusters and genuine signal are separated (expected) or the association cut is too tight.
- **associatedTracksVersusTime decreasing over time** → DUT detection efficiency is degrading; check bias voltage, temperature, and radiation damage.

---

## Priority Checklist During Beam Tests

Follow this order at beam start and periodically throughout the run.

### At beam start (first ~1000 events)

1. **L1idC canvas**
   - Does `hL1idCPlaneDelta` peak at 0? → confirms synchronisation is OK
   - Is `hCoincidenceRateTrend` starting at a high value?

2. **Timeline canvas**
   - Are clusters/event consistent across all planes?

3. **Hitmaps canvas**
   - Is the beam spot at the same position on every plane?

4. **Corr Trends canvas**
   - Is ΔX / ΔY stable near 0? (alignment check)

---

### During the run (periodic checks)

| Interval | Canvas | What to check |
|----------|--------|---------------|
| Continuously | **Timeline** | Stability of clusters/event and tracks/event |
| Every 5 min | **L1idC** | Coincidence rate trend |
| Every 10 min | **Corr Trends** | Any alignment drift |
| As needed | **Residuals** | Peak remains at 0 |
| As needed | **Charge Distributions** | No shift in MPV |

---

### Troubleshooting flow for anomalies

```
tracks/event drops or reaches zero
│
├─ Is hCoincidenceRateTrend also dropping?
│   ├─ Yes → check hL1idCPlaneDelta
│   │         Non-zero entries present → sync loss → inspect DAQ
│   │         Concentrated at 0 → beam rate problem
│   └─ No  → check Tracking / Residuals (alignment has broken down)
│
└─ Only one plane's clusters/event is dropping?
    → check power, bias voltage, and cables for that plane
    → check Hitmaps to see if hits have disappeared
```

---

*This guide assumes a MALTA2 telescope configuration using EventLoaderMALTA + OnlineMonitor.*  
*Plot names may differ depending on the DUT configuration and the clustering module in use.*
