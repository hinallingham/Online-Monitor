/**
 * @file
 * @brief Implementation of GUI display object
 *
 * @copyright Copyright (c) 2017-2024 CERN and the Corryvreckan authors.
 * This software is distributed under the terms of the MIT License, copied verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 * SPDX-License-Identifier: MIT
 */

#ifndef CORRYVRECKAN_GUIDISPLAY_H
#define CORRYVRECKAN_GUIDISPLAY_H 1

// Local includes
#include "core/utils/log.h"

// Global includes
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include "signal.h"

// ROOT includes
#include <RQ_OBJECT.h>
#include <TCanvas.h>
#include <TColor.h>
#include <TGLabel.h>
#include <TGProgressBar.h>
#include <TGStatusBar.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TPad.h>
#include <TProfile.h>
#include <TProfile2D.h>
#include <TDirectory.h>
#include "TApplication.h"
#include "TGCanvas.h"
#include "TGDockableFrame.h"
#include "TGFrame.h"
#include "TGMenu.h"
#include "TGScrollBar.h"
#include "TGTextEntry.h"
#include "TROOT.h"
#include "TRootEmbeddedCanvas.h"
#include "TSystem.h"
#include "TVirtualX.h"

namespace corryvreckan {
    /**
     * @brief Display class for ROOT GUIs
     */
    class GuiDisplay : public TGMainFrame {
    public:
        // Constructors and destructors
        GuiDisplay(const TGWindow* p = nullptr, UInt_t w = 1, UInt_t h = 1) : TGMainFrame(p, w, h) {}
        ~GuiDisplay() = default;

        bool isPaused() { return !running_; }

        // Graphics associated with GUI
        TRootEmbeddedCanvas* canvas;
        TGStatusBar* statusBar = nullptr;
        std::map<std::string, std::vector<TH1*>> histograms;
        std::map<TH1*, std::string> styles;
        std::map<TH1*, bool> logarithmic;
        std::map<std::string, TGTextButton*> buttons;
        std::map<std::string, TGButtonGroup*> buttonGroups;
        std::map<TRootEmbeddedCanvas*, bool> stackedCanvas;
        TGHorizontalFrame* buttonMenu;
        TGMainFrame* dutFrame;
        TGCanvas* dutCanvas;
        TGVerticalFrame* dutInnerFrame;

        bool running_ = true;
        std::string currentCanvas_;
        std::string windowTitle_;

        // Keyboard shortcuts
        std::vector<std::string> canvasOrder_;
        std::map<Long_t, std::string> keycodeActions_;
        std::string saveDir_ = "./";

        // Progress bar
        TGHorizontalFrame* progressFrame = nullptr;
        TGHProgressBar* progressBar = nullptr;
        TGLabel* progressLabel = nullptr;

        // Timeline data - filled by OnlineMonitor::run() each event
        struct TimelineData {
            std::string detectorName;
            TProfile* profile;
            Color_t color;
        };
        std::vector<TimelineData> timelineHitData_;
        TProfile* timelineTrackProfile_ = nullptr;

        // Per-plane X/Y trends vs event number, drawn overlaid on one canvas (top X, bottom Y):
        // Corr Trends -- mean(pos_i - pos_ref) -- and Res Trends -- the local track residual.
        struct TrendData {
            std::string detectorName;
            TProfile* profileX = nullptr;
            TProfile* profileY = nullptr;
            Color_t color = kBlack;
        };
        std::vector<TrendData> correlationTrendData_;
        std::vector<TrendData> residualTrendData_;

        // One colour scheme for the whole GUI: a plane keeps the same colour in the
        // timeline legend, the correlation/residual trends and its own distributions,
        // so an unhappy plane can be followed across canvases by colour alone.
        // Filled by OnlineMonitor before any plot is registered.
        std::map<std::string, Color_t> detectorColors_;
        std::map<TH1*, std::string> histoDetector_; // empty value = telescope-wide quantity

        // Resolved lazily: TColor::GetColor() needs ROOT to be up.
        static constexpr int kNColors = 8;
        static Color_t PaletteColor(size_t index) {
            static const Int_t palette[kNColors] = {TColor::GetColor("#4C72B0"),
                                                         TColor::GetColor("#DD8452"),
                                                         TColor::GetColor("#55A868"),
                                                         TColor::GetColor("#C44E52"),
                                                         TColor::GetColor("#8172B3"),
                                                         TColor::GetColor("#937860"),
                                                         TColor::GetColor("#DA8BC3"),
                                                         TColor::GetColor("#4EA1A5")};
            return static_cast<Color_t>(palette[index % kNColors]);
        }

        // Telescope-wide quantities (trackChi2, tracksPerEvent, ...) share one accent
        // colour, kept outside the per-detector palette so they never read as a plane.
        static Color_t AccentColor() {
            static const Int_t accent = TColor::GetColor("#2C3E50");
            return static_cast<Color_t>(accent);
        }

        Color_t ColorForHisto(TH1* h) const {
            auto det = histoDetector_.find(h);
            if(det != histoDetector_.end()) {
                auto col = detectorColors_.find(det->second);
                if(col != detectorColors_.end()) {
                    return col->second;
                }
            }
            return AccentColor();
        }

        // House style for a 1D histogram read from another module. Plots that carry an
        // explicit draw option in the config are left alone -- those opt out on purpose.
        void StyleHisto(TH1* h) {
            Color_t col = ColorForHisto(h);
            h->SetLineColor(col);
            h->SetLineWidth(2);
            h->SetMarkerColor(col);
            h->SetMarkerStyle(20);
            h->SetMarkerSize(0.6f);
            h->SetFillStyle(0);
        }

        /// Draw option for a histogram styled by StyleHisto(): outline plus error bars, so the
        /// statistical weight of a bin is visible rather than implied by a filled shape.
        static const char* HistoDrawOption() { return "HIST E"; }

        /// Style for plots registered with it: house colours, but a bare line -- no error bars, no
        /// markers. For the busy many-pad trends, where those cost redraw time and add little.
        static const std::string& LineStyle() {
            static const std::string line = "line";
            return line;
        }

        // Display-only downsampling. Every primitive drawn here is sent to the X server, which is
        // usually at the far end of an SSH tunnel: a 3M-bin eventTimes or a 512x512 colz map costs
        // seconds per redraw while a ~1200 px wide canvas cannot show that many bins anyway. Such
        // histograms are drawn through a coarser copy, refilled from the original on every redraw;
        // the original (and so the written ROOT file) is never touched. Set by OnlineMonitor.
        int maxDisplayBins1D_ = 2000;
        int maxDisplayBins2D_ = 300; ///< per axis

        struct DisplayCopy {
            TH1* hist = nullptr;
            int nx = 0, ny = 0;
            double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
        };
        std::map<TH1*, DisplayCopy> displayCopy_; //!

        // X range for histograms whose useful part is a small corner of their axis (e.g. cluster
        // size), in place of the default symmetric range. Set by OnlineMonitor.
        std::map<TH1*, std::pair<double, double>> fixedXRange_; //!

        // Pads drawn by Display(), so Update() can repaint only those whose histogram changed.
        // Cleared wherever the canvas is cleared, since Clear() deletes the pads.
        struct DrawnPad {
            TVirtualPad* pad;
            TH1* source;
            TH1* drawn;
            double entries;
        };
        std::vector<DrawnPad> drawnPads_; //!

        /// Group size that brings an axis down to at most maxBins, or 1 when it already fits (or
        /// has variable bins, which cannot be merged on a regular grid).
        static int DisplayGroup(const TAxis* axis, int maxBins) {
            if(maxBins <= 0 || axis->IsVariableBinSize() || axis->GetNbins() <= maxBins) {
                return 1;
            }
            return (axis->GetNbins() + maxBins - 1) / maxBins;
        }

        /// The histogram to actually draw for h: h itself, or its refreshed coarse copy.
        TH1* ForDisplay(TH1* h) {
            // Profiles hold means, not counts -- summing their bins would be wrong.
            if(h->InheritsFrom(TProfile::Class()) || h->InheritsFrom(TProfile2D::Class()) || h->GetDimension() > 2) {
                return h;
            }
            const bool is2D = h->GetDimension() == 2;
            const TAxis* ax = h->GetXaxis();
            const TAxis* ay = h->GetYaxis();
            const int gx = DisplayGroup(ax, is2D ? maxDisplayBins2D_ : maxDisplayBins1D_);
            const int gy = is2D ? DisplayGroup(ay, maxDisplayBins2D_) : 1;
            if(gx == 1 && gy == 1) {
                return h;
            }

            auto& dc = displayCopy_[h];
            const bool stale = dc.hist == nullptr || dc.nx != ax->GetNbins() || dc.ny != ay->GetNbins() ||
                               dc.xmin != ax->GetXmin() || dc.xmax != ax->GetXmax() || dc.ymin != ay->GetXmin() ||
                               dc.ymax != ay->GetXmax();
            if(stale) {
                // The source axis changed (SetCanExtend). The old copy is deliberately not deleted:
                // a full-screen canvas may still hold it, and this happens only a handful of times.
                const int nbx = (ax->GetNbins() + gx - 1) / gx;
                const double xhi = ax->GetXmin() + nbx * gx * ax->GetBinWidth(1);
                TH1* c = nullptr;
                TDirectory::TContext noDirectory(nullptr); // not even briefly in the module's directory
                if(is2D) {
                    const int nby = (ay->GetNbins() + gy - 1) / gy;
                    const double yhi = ay->GetXmin() + nby * gy * ay->GetBinWidth(1);
                    c = new TH2D(h->GetName(), h->GetTitle(), nbx, ax->GetXmin(), xhi, nby, ay->GetXmin(), yhi);
                } else {
                    c = new TH1D(h->GetName(), h->GetTitle(), nbx, ax->GetXmin(), xhi);
                }
                c->SetDirectory(nullptr); // never ends up in the output file
                if(h->GetSumw2N() > 0) {
                    c->Sumw2();
                }
                c->GetXaxis()->SetTitle(ax->GetTitle());
                c->GetYaxis()->SetTitle(ay->GetTitle());
                c->GetZaxis()->SetTitle(h->GetZaxis()->GetTitle());
                dc.hist = c;
                dc.nx = ax->GetNbins();
                dc.ny = ay->GetNbins();
                dc.xmin = ax->GetXmin();
                dc.xmax = ax->GetXmax();
                dc.ymin = ay->GetXmin();
                dc.ymax = ay->GetXmax();
            }
            TH1* c = dc.hist;

            // Attributes follow the source, which StyleHisto() and Display() may have just changed.
            h->TAttLine::Copy(*c);
            h->TAttFill::Copy(*c);
            h->TAttMarker::Copy(*c);
            c->SetStats(!h->TestBit(TH1::kNoStats));
            c->SetMinimum(h->GetMinimumStored());
            c->SetMaximum(h->GetMaximumStored());

            // Refill. Under/overflow map onto the copy's under/overflow.
            c->Reset("ICES");
            TArrayD* srcW2 = (h->GetSumw2N() > 0) ? h->GetSumw2() : nullptr;
            TArrayD* dstW2 = (srcW2 != nullptr) ? c->GetSumw2() : nullptr;
            auto coarse = [](int i, int n, int g, int nc) { return i == 0 ? 0 : (i > n ? nc + 1 : (i - 1) / g + 1); };
            const int nx = ax->GetNbins();
            const int ny = is2D ? ay->GetNbins() : 0;
            const int ncx = c->GetNbinsX();
            const int ncy = is2D ? c->GetNbinsY() : 0;
            for(int iy = 0; iy <= (is2D ? ny + 1 : 0); iy++) {
                const int jy = is2D ? coarse(iy, ny, gy, ncy) : 0;
                for(int ix = 0; ix <= nx + 1; ix++) {
                    const int src = is2D ? h->GetBin(ix, iy) : ix;
                    const double v = h->GetBinContent(src);
                    if(v == 0.0) {
                        continue;
                    }
                    const int dst = is2D ? c->GetBin(coarse(ix, nx, gx, ncx), jy) : coarse(ix, nx, gx, ncx);
                    c->AddBinContent(dst, v);
                    if(dstW2 != nullptr) {
                        (*dstW2)[dst] += (*srcW2)[src];
                    }
                }
            }
            c->ResetStats();
            c->SetEntries(h->GetEntries());
            return c;
        }

        // Draw timing, so a slow canvas can be spotted on the real display: the last draw time is
        // shown in the status bar, and a draw over kSlowDrawMs is logged (at WARNING, so it shows
        // with the log level the online config uses).
        static constexpr double kSlowDrawMs = 100.0;
        double lastDrawMs_ = 0.0;

        /// Canvas name as shown to the user ("HitmapsCanvas" -> "Hitmaps").
        std::string CanvasLabel() const {
            std::string name = currentCanvas_;
            if(name.size() > 6 && name.substr(name.size() - 6) == "Canvas") {
                name = name.substr(0, name.size() - 6);
            }
            return name;
        }

        void ShowCanvasStatus() {
            if(statusBar != nullptr) {
                statusBar->SetText(Form("  Canvas: %s   |   Draw: %.0f ms", CanvasLabel().c_str(), lastDrawMs_), 1);
            }
        }

        /// Ends a timed draw started at `start`. Waits for the X server to finish first (one round
        /// trip), so the number is what the user actually waited, not just the time to queue it.
        void ReportDraw(const char* what, std::chrono::steady_clock::time_point start) {
            gVirtualX->Update(1);
            lastDrawMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if(lastDrawMs_ > kSlowDrawMs) {
                LOG(WARNING) << what << " of \"" << CanvasLabel() << "\" took " << static_cast<int>(lastDrawMs_) << " ms";
            }
            ShowCanvasStatus();
        }

        // Button functions
        inline void Display(char* canvasNameC) {
            auto drawStart = std::chrono::steady_clock::now();
            std::string canvasName(canvasNameC);
            currentCanvas_ = canvasName;
            drawnPads_.clear();
            if(histograms[canvasName].size() == 0) {
                LOG(ERROR) << "Canvas does not have any histograms, exiting";
                return;
            }
            size_t nHistograms = histograms[canvasName].size();
            canvas->GetCanvas()->Clear();
            canvas->GetCanvas()->cd();

            TVirtualPad* area = canvas->GetCanvas();

            if(!stackedCanvas[canvas]) {
                int cols, rows;
                if(nHistograms <= 2) {
                    cols = static_cast<int>(nHistograms); rows = 1;
                } else if(nHistograms <= 4) {
                    cols = 2; rows = 2;
                } else if(nHistograms <= 6) {
                    cols = 3; rows = 2;
                } else if(nHistograms <= 9) {
                    cols = 3; rows = 3;
                } else {
                    cols = 4; rows = (static_cast<int>(nHistograms) + 3) / 4;
                }
                area->Divide(cols, rows, 0.003f, 0.003f);
            }

            for(size_t i = 0; i < nHistograms; i++) {
                if(!stackedCanvas[canvas]) {
                    area->cd(static_cast<int>(i + 1));
                    gPad->SetMargin(0.12f, 0.05f, 0.12f, 0.07f);
                    gPad->SetGridx();
                    gPad->SetGridy();
                }
                TH1* h = histograms[canvasName][i];
                std::string style = styles[h];
                bool is2D = dynamic_cast<TH2*>(h) != nullptr;

                if(logarithmic[h]) {
                    gPad->SetLogy();
                }
                if(is2D && style.find("colz") != std::string::npos) {
                    gPad->SetRightMargin(0.15f);
                    // An efficiency map is a fraction: let the scale run 0..1 so "almost all
                    // green" and "genuinely 100%" do not look the same, and so a dip is visible
                    // against a fixed reference rather than against the run's own maximum.
                    if(std::string(h->GetName()).find("fficiency") != std::string::npos) {
                        h->SetMinimum(0.0);
                        h->SetMaximum(1.0);
                    }
                }
                if(!is2D && (style.empty() || style == LineStyle())) {
                    StyleHisto(h);
                    style = (style == LineStyle()) ? "HIST" : HistoDrawOption();
                }

                if(stackedCanvas[canvas]) {
                    style = "same";
                    h->SetLineColor(static_cast<short>(i + 1));
                    h->SetFillStyle(0);
                }

                TH1* d = ForDisplay(h);
                if(!is2D) {
                    auto fixed = fixedXRange_.find(h);
                    if(fixed != fixedXRange_.end()) {
                        d->GetXaxis()->SetRangeUser(fixed->second.first, fixed->second.second);
                    } else {
                        double xlo = h->GetXaxis()->GetXmin();
                        double xhi = h->GetXaxis()->GetXmax();
                        double absMax = std::max(std::abs(xlo), std::abs(xhi));
                        d->GetXaxis()->SetRangeUser(-absMax, absMax);
                    }
                }

                d->Draw(style.c_str());
                drawnPads_.push_back({gPad, h, d, h->GetEntries()});
            }

            canvas->GetCanvas()->Paint();
            canvas->GetCanvas()->Update();
            ReportDraw("Drawing", drawStart);
        };

        // Exit the monitoring
        inline void Exit() { raise(SIGINT); }

        // Pause monitoring
        void TogglePause() {
            running_ = !running_;

            auto button = buttons["pause"];
            button->SetState(kButtonDown);
            ULong_t color;
            if(!running_) {
                button->SetText("&Resume Monitoring");
                gClient->GetColorByName("gray", color);

            } else {
                button->SetText("&Pause Monitoring ");
                gClient->GetColorByName("green", color);
            }
            button->ChangeBackground(color);
            button->SetState(kButtonUp);
        }

        inline void Update() {
            auto drawStart = std::chrono::steady_clock::now();
            // Canvases not built by Display() are redrawn whole, which also re-fits their y range to
            // the data (it is fixed when they are drawn). They are a few cheap line plots.
            if(currentCanvas_ == "TimelineCanvas") {
                DisplayTimeline();
                return;
            }
            if(currentCanvas_ == "CorrelationCanvas") {
                DisplayCorrelation();
                return;
            }
            if(currentCanvas_ == "ResidualTrendCanvas") {
                DisplayResidualTrend();
                return;
            }
            if(drawnPads_.empty()) {
                canvas->GetCanvas()->Paint();
                canvas->GetCanvas()->Update();
                ReportDraw("Update", drawStart);
                return;
            }
            // Otherwise repaint only the pads whose histogram gained entries since the last
            // update: a pad left alone is not resent to the X server at all.
            bool any = false;
            for(auto& dp : drawnPads_) {
                double entries = dp.source->GetEntries();
                if(entries == dp.entries) {
                    continue;
                }
                dp.entries = entries;
                if(ForDisplay(dp.source) != dp.drawn) {
                    // The source axis was extended and the coarse copy rebuilt: redraw the canvas.
                    std::string name = currentCanvas_;
                    Display(const_cast<char*>(name.c_str()));
                    return;
                }
                dp.pad->Modified();
                any = true;
            }
            if(any) {
                canvas->GetCanvas()->Update();
                ReportDraw("Update", drawStart);
            }
        }

        inline void DisplayTimeline() {
            if(timelineHitData_.empty() && !timelineTrackProfile_) return;
            auto drawStart = std::chrono::steady_clock::now();
            currentCanvas_ = "TimelineCanvas";

            canvas->GetCanvas()->Clear();
            drawnPads_.clear();
            canvas->GetCanvas()->cd();

            // Top pad (65%): clusters/event per detector
            TPad* padHits = new TPad("padHits", "", 0.0, 0.33, 1.0, 1.0);
            padHits->SetMargin(0.10f, 0.02f, 0.02f, 0.10f);
            padHits->SetGridx();
            padHits->SetGridy();
            padHits->Draw();
            padHits->cd();

            // Forget the previous draw's range first (see DrawTrendCanvas)
            for(auto& td : timelineHitData_) {
                td.profile->SetMinimum();
                td.profile->SetMaximum();
            }
            if(timelineTrackProfile_) {
                timelineTrackProfile_->SetMinimum();
                timelineTrackProfile_->SetMaximum();
            }
            double hitsGlobalMax = 0.0;
            for(auto& td : timelineHitData_) {
                hitsGlobalMax = std::max(hitsGlobalMax, td.profile->GetMaximum());
            }

            bool first = true;
            for(auto& td : timelineHitData_) {
                td.profile->SetLineColor(td.color);
                td.profile->SetLineWidth(2);
                td.profile->SetStats(false);
                if(first) {
                    td.profile->GetYaxis()->SetTitle("Clusters / Event");
                    td.profile->GetYaxis()->SetTitleOffset(1.1f);
                    td.profile->GetXaxis()->SetLabelSize(0.0f);
                    td.profile->SetMinimum(0.0);
                    td.profile->SetMaximum(hitsGlobalMax * 1.1 + 1.0);
                    td.profile->Draw("HIST");
                    first = false;
                } else {
                    td.profile->Draw("HIST SAME");
                }
            }

            // Legend
            if(!timelineHitData_.empty()) {
                auto* leg = new TLegend(0.72f, 0.60f, 0.98f, 0.96f);
                leg->SetBorderSize(1);
                leg->SetFillColorAlpha(kWhite, 0.80f);
                leg->SetTextFont(42);
                leg->SetTextSize(0.040f);
                for(auto& td : timelineHitData_) {
                    leg->AddEntry(td.profile, td.detectorName.c_str(), "l");
                }
                leg->Draw();
            }

            // Bottom pad (33%): tracks/event
            canvas->GetCanvas()->cd();
            TPad* padTracks = new TPad("padTracks", "", 0.0, 0.0, 1.0, 0.33);
            padTracks->SetMargin(0.10f, 0.02f, 0.22f, 0.02f);
            padTracks->SetGridx();
            padTracks->SetGridy();
            padTracks->Draw();
            padTracks->cd();

            if(timelineTrackProfile_) {
                timelineTrackProfile_->SetLineColor(kBlack);
                timelineTrackProfile_->SetLineWidth(2);
                timelineTrackProfile_->SetFillColorAlpha(kOrange + 1, 0.30f);
                timelineTrackProfile_->SetStats(false);
                timelineTrackProfile_->GetYaxis()->SetTitle("Tracks / Event");
                timelineTrackProfile_->GetYaxis()->SetTitleOffset(1.1f);
                timelineTrackProfile_->GetXaxis()->SetTitleSize(0.08f);
                timelineTrackProfile_->GetXaxis()->SetLabelSize(0.07f);
                timelineTrackProfile_->GetYaxis()->SetTitleSize(0.07f);
                timelineTrackProfile_->GetYaxis()->SetLabelSize(0.07f);
                timelineTrackProfile_->SetMinimum(0.0);
                timelineTrackProfile_->SetMaximum(timelineTrackProfile_->GetMaximum() * 1.1 + 1.0);
                timelineTrackProfile_->Draw("HIST");
            }

            canvas->GetCanvas()->Paint();
            canvas->GetCanvas()->Update();
            ReportDraw("Drawing", drawStart);
        }

        inline void DisplayCorrelation() {
            DrawTrendCanvas("CorrelationCanvas", correlationTrendData_, "#Delta X_{global} [mm]", "#Delta Y_{global} [mm]", 0.1);
        }

        inline void DisplayResidualTrend() {
            // Residuals move by microns: a 0.1 mm floor on the margin would flatten them
            DrawTrendCanvas("ResidualTrendCanvas", residualTrendData_, "Local residual X [mm]", "Local residual Y [mm]", 0.002);
        }

        void DrawTrendCanvas(const char* canvasName,
                             std::vector<TrendData>& trends,
                             const char* yTitleX,
                             const char* yTitleY,
                             double minMargin) {
            if(trends.empty()) return;
            // Forget the range set by the previous draw: GetMaximum()/GetMinimum() would return it
            // instead of the data's, and the range would grow by a margin on every redraw.
            for(auto& cd : trends) {
                for(auto* p : {cd.profileX, cd.profileY}) {
                    if(p != nullptr) {
                        p->SetMinimum();
                        p->SetMaximum();
                    }
                }
            }
            auto drawStart = std::chrono::steady_clock::now();
            currentCanvas_ = canvasName;

            canvas->GetCanvas()->Clear();
            drawnPads_.clear();
            canvas->GetCanvas()->cd();

            // Top pad (55%): X trend per detector
            TPad* padX = new TPad("padCorrX", "", 0.0, 0.45, 1.0, 1.0);
            padX->SetMargin(0.10f, 0.02f, 0.02f, 0.10f);
            padX->SetGridx(); padX->SetGridy();
            padX->Draw();
            padX->cd();

            double xGlobalMax = 0.0, xGlobalMin = 0.0;
            for(auto& cd : trends) {
                if(!cd.profileX || cd.profileX->GetEntries() == 0) continue;
                xGlobalMax = std::max(xGlobalMax, cd.profileX->GetMaximum());
                xGlobalMin = std::min(xGlobalMin, cd.profileX->GetMinimum());
            }
            double xMargin = std::max(minMargin, (xGlobalMax - xGlobalMin) * 0.15);

            bool firstX = true;
            for(auto& cd : trends) {
                if(!cd.profileX) continue;
                cd.profileX->SetLineColor(cd.color);
                cd.profileX->SetLineWidth(2);
                cd.profileX->SetStats(false);
                if(firstX) {
                    cd.profileX->GetYaxis()->SetTitle(yTitleX);
                    cd.profileX->GetYaxis()->SetTitleOffset(1.1f);
                    cd.profileX->GetXaxis()->SetLabelSize(0.0f);
                    cd.profileX->SetMinimum(xGlobalMin - xMargin);
                    cd.profileX->SetMaximum(xGlobalMax + xMargin);
                    cd.profileX->Draw("HIST");
                    firstX = false;
                } else {
                    cd.profileX->Draw("HIST SAME");
                }
            }
            {
                auto* leg = new TLegend(0.72f, 0.60f, 0.98f, 0.96f);
                leg->SetBorderSize(1);
                leg->SetFillColorAlpha(kWhite, 0.80f);
                leg->SetTextFont(42);
                leg->SetTextSize(0.040f);
                for(auto& cd : trends)
                    if(cd.profileX) leg->AddEntry(cd.profileX, cd.detectorName.c_str(), "l");
                leg->Draw();
            }

            // Bottom pad (45%): Y trend per detector
            canvas->GetCanvas()->cd();
            TPad* padY = new TPad("padCorrY", "", 0.0, 0.0, 1.0, 0.45);
            padY->SetMargin(0.10f, 0.02f, 0.22f, 0.02f);
            padY->SetGridx(); padY->SetGridy();
            padY->Draw();
            padY->cd();

            double yGlobalMax = 0.0, yGlobalMin = 0.0;
            for(auto& cd : trends) {
                if(!cd.profileY || cd.profileY->GetEntries() == 0) continue;
                yGlobalMax = std::max(yGlobalMax, cd.profileY->GetMaximum());
                yGlobalMin = std::min(yGlobalMin, cd.profileY->GetMinimum());
            }
            double yMargin = std::max(minMargin, (yGlobalMax - yGlobalMin) * 0.15);

            bool firstY = true;
            for(auto& cd : trends) {
                if(!cd.profileY) continue;
                cd.profileY->SetLineColor(cd.color);
                cd.profileY->SetLineWidth(2);
                cd.profileY->SetStats(false);
                if(firstY) {
                    cd.profileY->GetYaxis()->SetTitle(yTitleY);
                    cd.profileY->GetYaxis()->SetTitleOffset(1.1f);
                    cd.profileY->GetXaxis()->SetTitleSize(0.08f);
                    cd.profileY->GetXaxis()->SetLabelSize(0.07f);
                    cd.profileY->GetYaxis()->SetTitleSize(0.07f);
                    cd.profileY->GetYaxis()->SetLabelSize(0.07f);
                    cd.profileY->SetMinimum(yGlobalMin - yMargin);
                    cd.profileY->SetMaximum(yGlobalMax + yMargin);
                    cd.profileY->Draw("HIST");
                    firstY = false;
                } else {
                    cd.profileY->Draw("HIST SAME");
                }
            }
            {
                auto* leg = new TLegend(0.72f, 0.60f, 0.98f, 0.96f);
                leg->SetBorderSize(1);
                leg->SetFillColorAlpha(kWhite, 0.80f);
                leg->SetTextFont(42);
                leg->SetTextSize(0.040f);
                for(auto& cd : trends)
                    if(cd.profileY) leg->AddEntry(cd.profileY, cd.detectorName.c_str(), "l");
                leg->Draw();
            }

            canvas->GetCanvas()->Paint();
            canvas->GetCanvas()->Update();
            ReportDraw("Drawing", drawStart);
        }

        Bool_t HandleKey(Event_t* event) override {
            if(event->fType == kGKeyPress) {
                auto it = keycodeActions_.find(static_cast<Long_t>(event->fCode));
                if(it != keycodeActions_.end()) {
                    const std::string& action = it->second;
                    if(action == "pause") {
                        TogglePause();
                    } else if(action == "fullscreen") {
                        FullScreen();
                    } else if(action == "save") {
                        time_t t = time(nullptr);
                        char ts[32];
                        strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", localtime(&t));
                        SaveCanvas(saveDir_ + "corry_" + ts + ".png");
                    } else if(action.size() > 6 && action.substr(0, 6) == "canvas") {
                        size_t idx = static_cast<size_t>(std::stoi(action.substr(6)));
                        if(idx < canvasOrder_.size()) {
                            const auto& name = canvasOrder_[idx];
                            if(name == "TimelineCanvas") DisplayTimeline();
                            else if(name == "CorrelationCanvas") DisplayCorrelation();
                            else if(name == "ResidualTrendCanvas") DisplayResidualTrend();
                            else Display(const_cast<char*>(name.c_str()));
                        }
                    }
                }
            }
            return TGMainFrame::HandleKey(event);
        }

        void SaveCanvas(const std::string& path) {
            canvas->GetCanvas()->Print(path.c_str());
            LOG(INFO) << "Canvas saved: " << path;
        }

        inline void updateProgress(int current, int target) {
            if(!progressBar || !progressLabel || !progressFrame) return;
            if(target > 0) {
                float pct = std::min(100.0f, static_cast<float>(current) * 100.0f / static_cast<float>(target));
                progressBar->SetPosition(pct);
                if(pct < 75.0f)       progressBar->SetBarColor("SteelBlue");
                else if(pct < 95.0f)  progressBar->SetBarColor("orange");
                else                  progressBar->SetBarColor("green");
                progressLabel->SetText(Form("  %d / %d  (%.0f%%)", current, target, static_cast<double>(pct)));
            } else {
                progressBar->SetPosition(0.0f);
                progressLabel->SetText(Form("  %d evt", current));
            }
            progressFrame->Layout();
        }

        inline void FullScreen() {
            // Timeline special case
            if(currentCanvas_ == "TimelineCanvas") {
                UInt_t sw = gClient->GetDisplayWidth();
                UInt_t sh = gClient->GetDisplayHeight();
                TCanvas* fc = new TCanvas("fs_Timeline", "Timeline",
                                          0, 0, static_cast<Int_t>(sw),
                                          static_cast<Int_t>(sh > 50 ? sh - 50 : sh));
                fc->cd();
                fc->Divide(1, 2, 0.001f, 0.01f);
                fc->cd(1);
                gPad->SetMargin(0.08f, 0.02f, 0.02f, 0.10f);
                gPad->SetGridx(); gPad->SetGridy();
                for(auto& td : timelineHitData_) {
                    td.profile->SetMinimum();
                    td.profile->SetMaximum();
                }
                if(timelineTrackProfile_) {
                    timelineTrackProfile_->SetMinimum();
                    timelineTrackProfile_->SetMaximum();
                }
                double fsHitsMax = 0.0;
                for(auto& td : timelineHitData_) {
                    fsHitsMax = std::max(fsHitsMax, td.profile->GetMaximum());
                }
                bool first = true;
                for(auto& td : timelineHitData_) {
                    td.profile->SetLineColor(td.color);
                    td.profile->SetLineWidth(2);
                    td.profile->SetStats(false);
                    if(first) {
                        td.profile->SetMinimum(0.0);
                        td.profile->SetMaximum(fsHitsMax * 1.1 + 1.0);
                        td.profile->Draw("HIST");
                        first = false;
                    } else {
                        td.profile->Draw("HIST SAME");
                    }
                }
                if(!timelineHitData_.empty()) {
                    auto* leg = new TLegend(0.72f, 0.60f, 0.98f, 0.96f);
                    leg->SetBorderSize(1);
                    leg->SetFillColorAlpha(kWhite, 0.80f);
                    for(auto& td : timelineHitData_) leg->AddEntry(td.profile, td.detectorName.c_str(), "l");
                    leg->Draw();
                }
                fc->cd(2);
                gPad->SetMargin(0.08f, 0.02f, 0.15f, 0.02f);
                gPad->SetGridx(); gPad->SetGridy();
                if(timelineTrackProfile_) {
                    timelineTrackProfile_->SetLineColor(kBlack);
                    timelineTrackProfile_->SetLineWidth(2);
                    timelineTrackProfile_->SetFillColorAlpha(kOrange + 1, 0.30f);
                    timelineTrackProfile_->SetStats(false);
                    timelineTrackProfile_->SetMinimum(0.0);
                    timelineTrackProfile_->SetMaximum(timelineTrackProfile_->GetMaximum() * 1.1 + 1.0);
                    timelineTrackProfile_->Draw("HIST");
                }
                fc->Paint(); fc->Update();
                return;
            }

            // Trend canvases (Corr Trends, Res Trends) full screen
            bool isCorr = currentCanvas_ == "CorrelationCanvas";
            if((isCorr || currentCanvas_ == "ResidualTrendCanvas") &&
               !(isCorr ? correlationTrendData_ : residualTrendData_).empty()) {
                auto& trends = isCorr ? correlationTrendData_ : residualTrendData_;
                const double minMargin = isCorr ? 0.1 : 0.002;
                for(auto& cd : trends) {
                    for(auto* p : {cd.profileX, cd.profileY}) {
                        if(p != nullptr) {
                            p->SetMinimum();
                            p->SetMaximum();
                        }
                    }
                }
                UInt_t sw = gClient->GetDisplayWidth();
                UInt_t sh = gClient->GetDisplayHeight();
                TCanvas* fc = new TCanvas(isCorr ? "fs_Correlation" : "fs_ResidualTrend",
                                          isCorr ? "Correlation Trends" : "Residual Trends",
                                          0, 0, static_cast<Int_t>(sw),
                                          static_cast<Int_t>(sh > 50 ? sh - 50 : sh));
                fc->cd();
                fc->Divide(1, 2, 0.001f, 0.01f);

                // X pad
                fc->cd(1);
                gPad->SetMargin(0.08f, 0.02f, 0.02f, 0.10f);
                gPad->SetGridx(); gPad->SetGridy();
                double fsXMax = 0.0, fsXMin = 0.0;
                for(auto& cd : trends) {
                    if(!cd.profileX || cd.profileX->GetEntries() == 0) continue;
                    fsXMax = std::max(fsXMax, cd.profileX->GetMaximum());
                    fsXMin = std::min(fsXMin, cd.profileX->GetMinimum());
                }
                double fsXMargin = std::max(minMargin, (fsXMax - fsXMin) * 0.15);
                bool firstX = true;
                for(auto& cd : trends) {
                    if(!cd.profileX) continue;
                    cd.profileX->SetLineColor(cd.color); cd.profileX->SetLineWidth(2); cd.profileX->SetStats(false);
                    if(firstX) {
                        cd.profileX->SetMinimum(fsXMin - fsXMargin);
                        cd.profileX->SetMaximum(fsXMax + fsXMargin);
                        cd.profileX->Draw("HIST"); firstX = false;
                    } else { cd.profileX->Draw("HIST SAME"); }
                }
                auto* legX = new TLegend(0.72f, 0.60f, 0.98f, 0.96f);
                legX->SetBorderSize(1); legX->SetFillColorAlpha(kWhite, 0.80f);
                for(auto& cd : trends) if(cd.profileX) legX->AddEntry(cd.profileX, cd.detectorName.c_str(), "l");
                legX->Draw();

                // Y pad
                fc->cd(2);
                gPad->SetMargin(0.08f, 0.02f, 0.15f, 0.02f);
                gPad->SetGridx(); gPad->SetGridy();
                double fsYMax = 0.0, fsYMin = 0.0;
                for(auto& cd : trends) {
                    if(!cd.profileY || cd.profileY->GetEntries() == 0) continue;
                    fsYMax = std::max(fsYMax, cd.profileY->GetMaximum());
                    fsYMin = std::min(fsYMin, cd.profileY->GetMinimum());
                }
                double fsYMargin = std::max(minMargin, (fsYMax - fsYMin) * 0.15);
                bool firstY = true;
                for(auto& cd : trends) {
                    if(!cd.profileY) continue;
                    cd.profileY->SetLineColor(cd.color); cd.profileY->SetLineWidth(2); cd.profileY->SetStats(false);
                    if(firstY) {
                        cd.profileY->SetMinimum(fsYMin - fsYMargin);
                        cd.profileY->SetMaximum(fsYMax + fsYMargin);
                        cd.profileY->Draw("HIST"); firstY = false;
                    } else { cd.profileY->Draw("HIST SAME"); }
                }
                auto* legY = new TLegend(0.72f, 0.60f, 0.98f, 0.96f);
                legY->SetBorderSize(1); legY->SetFillColorAlpha(kWhite, 0.80f);
                for(auto& cd : trends) if(cd.profileY) legY->AddEntry(cd.profileY, cd.detectorName.c_str(), "l");
                legY->Draw();

                fc->Paint(); fc->Update();
                return;
            }

            // Regular canvas
            if(currentCanvas_.empty() || histograms[currentCanvas_].empty()) return;
            std::string title = currentCanvas_;
            if(title.size() > 6 && title.substr(title.size() - 6) == "Canvas") {
                title = title.substr(0, title.size() - 6);
            }

            UInt_t sw = gClient->GetDisplayWidth();
            UInt_t sh = gClient->GetDisplayHeight();
            TCanvas* fc = new TCanvas(("fs_" + currentCanvas_).c_str(), title.c_str(),
                                      0, 0, static_cast<Int_t>(sw),
                                      static_cast<Int_t>(sh > 50 ? sh - 50 : sh));
            fc->cd();

            auto& hList = histograms[currentCanvas_];
            size_t n = hList.size();
            int cols, rows;
            if(n <= 2)      { cols = static_cast<int>(n); rows = 1; }
            else if(n <= 4) { cols = 2; rows = 2; }
            else if(n <= 6) { cols = 3; rows = 2; }
            else if(n <= 9) { cols = 3; rows = 3; }
            else            { cols = 4; rows = (static_cast<int>(n) + 3) / 4; }
            fc->Divide(cols, rows, 0.003f, 0.003f);

            for(size_t i = 0; i < n; i++) {
                fc->cd(static_cast<int>(i + 1));
                gPad->SetMargin(0.10f, 0.05f, 0.12f, 0.08f);
                gPad->SetGridx(); gPad->SetGridy();
                TH1* h = hList[i];
                std::string style = styles[h];
                bool is2D = dynamic_cast<TH2*>(h) != nullptr;
                if(logarithmic[h]) gPad->SetLogy();
                if(is2D && style.find("colz") != std::string::npos) gPad->SetRightMargin(0.15f);
                if(!is2D && (style.empty() || style == LineStyle())) {
                    StyleHisto(h);
                    style = (style == LineStyle()) ? "HIST" : HistoDrawOption();
                }
                TH1* d = ForDisplay(h);
                if(!is2D) {
                    auto fixed = fixedXRange_.find(h);
                    if(fixed != fixedXRange_.end()) {
                        d->GetXaxis()->SetRangeUser(fixed->second.first, fixed->second.second);
                    } else {
                        double xlo = h->GetXaxis()->GetXmin();
                        double xhi = h->GetXaxis()->GetXmax();
                        double absMax = std::max(std::abs(xlo), std::abs(xhi));
                        d->GetXaxis()->SetRangeUser(-absMax, absMax);
                    }
                }
                d->Draw(style.c_str());
            }
            fc->Paint(); fc->Update();
        }

        inline void updateStatus(int eventCount, double evtRate) {
            if(!statusBar) return;

            statusBar->SetText(Form("  Events: %d   |   Rate: %.1f evt/s", eventCount, evtRate), 0);

            ShowCanvasStatus();

            time_t now = time(nullptr);
            char buf[32];
            strftime(buf, sizeof(buf), "  %H:%M:%S", localtime(&now));
            statusBar->SetText(buf, 2);
        }

        ClassDefOverride(GuiDisplay, 0);
    };
} // namespace corryvreckan

#endif // CORRYVRECKAN_GUIDISPLAY_H
