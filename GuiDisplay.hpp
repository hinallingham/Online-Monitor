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
#include <ctime>
#include <iostream>
#include "signal.h"

// ROOT includes
#include <RQ_OBJECT.h>
#include <TCanvas.h>
#include <TGLabel.h>
#include <TGProgressBar.h>
#include <TGStatusBar.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TLegend.h>
#include <TPad.h>
#include <TProfile.h>
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

        // Button functions
        inline void Display(char* canvasNameC) {
            std::string canvasName(canvasNameC);
            currentCanvas_ = canvasName;
            if(histograms[canvasName].size() == 0) {
                LOG(ERROR) << "Canvas does not have any histograms, exiting";
                return;
            }
            size_t nHistograms = histograms[canvasName].size();
            canvas->GetCanvas()->Clear();
            canvas->GetCanvas()->cd();

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
                canvas->GetCanvas()->Divide(cols, rows, 0.003f, 0.003f);
            }

            for(size_t i = 0; i < nHistograms; i++) {
                if(!stackedCanvas[canvas]) {
                    canvas->GetCanvas()->cd(static_cast<int>(i + 1));
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
                }
                if(!is2D && style.empty()) {
                    h->SetFillColorAlpha(kAzure + 7, 0.35f);
                    h->SetLineColor(kAzure - 2);
                    h->SetLineWidth(2);
                }

                if(stackedCanvas[canvas]) {
                    style = "same";
                    h->SetLineColor(static_cast<short>(i + 1));
                    h->SetFillStyle(0);
                }
                h->Draw(style.c_str());
            }
            canvas->GetCanvas()->Paint();
            canvas->GetCanvas()->Update();
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
            canvas->GetCanvas()->Paint();
            canvas->GetCanvas()->Update();
        }

        inline void DisplayTimeline() {
            if(timelineHitData_.empty() && !timelineTrackProfile_) return;
            currentCanvas_ = "TimelineCanvas";

            canvas->GetCanvas()->Clear();
            canvas->GetCanvas()->cd();

            // Top pad (65%): clusters/event per detector
            TPad* padHits = new TPad("padHits", "", 0.0, 0.33, 1.0, 1.0);
            padHits->SetMargin(0.10f, 0.02f, 0.02f, 0.10f);
            padHits->SetGridx();
            padHits->SetGridy();
            padHits->Draw();
            padHits->cd();

            bool first = true;
            for(auto& td : timelineHitData_) {
                td.profile->SetLineColor(td.color);
                td.profile->SetLineWidth(2);
                td.profile->SetStats(false);
                if(first) {
                    td.profile->GetYaxis()->SetTitle("Clusters / Event");
                    td.profile->GetYaxis()->SetTitleOffset(1.1f);
                    td.profile->GetXaxis()->SetLabelSize(0.0f);
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
                timelineTrackProfile_->GetXaxis()->SetTitle("Event");
                timelineTrackProfile_->GetYaxis()->SetTitle("Tracks / Event");
                timelineTrackProfile_->GetYaxis()->SetTitleOffset(1.1f);
                timelineTrackProfile_->GetXaxis()->SetTitleSize(0.08f);
                timelineTrackProfile_->GetXaxis()->SetLabelSize(0.07f);
                timelineTrackProfile_->GetYaxis()->SetTitleSize(0.07f);
                timelineTrackProfile_->GetYaxis()->SetLabelSize(0.07f);
                timelineTrackProfile_->Draw("HIST");
            }

            canvas->GetCanvas()->Paint();
            canvas->GetCanvas()->Update();
        }

        Bool_t ProcessMessage(Long_t msg, Long_t parm1, Long_t /*parm2*/) override {
            if(GET_MSG(msg) == kC_KEYBOARD && GET_SUBMSG(msg) == kKP_KeyDown) {
                auto it = keycodeActions_.find(parm1);
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
                            else Display(const_cast<char*>(name.c_str()));
                        }
                    }
                }
            }
            return TGMainFrame::ProcessMessage(msg, parm1, 0);
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
                bool first = true;
                for(auto& td : timelineHitData_) {
                    td.profile->SetLineColor(td.color);
                    td.profile->SetLineWidth(2);
                    td.profile->SetStats(false);
                    if(first) { td.profile->Draw("HIST"); first = false; }
                    else        td.profile->Draw("HIST SAME");
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
                    timelineTrackProfile_->Draw("HIST");
                }
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
                h->Draw(style.c_str());
            }
            fc->Paint(); fc->Update();
        }

        inline void updateStatus(int eventCount, double evtRate) {
            if(!statusBar) return;

            statusBar->SetText(Form("  Events: %d   |   Rate: %.1f evt/s", eventCount, evtRate), 0);

            std::string dispName = currentCanvas_;
            if(dispName.size() > 6 && dispName.substr(dispName.size() - 6) == "Canvas") {
                dispName = dispName.substr(0, dispName.size() - 6);
            }
            statusBar->SetText(Form("  Canvas: %s", dispName.c_str()), 1);

            time_t now = time(nullptr);
            char buf[32];
            strftime(buf, sizeof(buf), "  %H:%M:%S", localtime(&now));
            statusBar->SetText(buf, 2);
        }

        ClassDef(GuiDisplay, 0);
    };
} // namespace corryvreckan

#endif // CORRYVRECKAN_GUIDISPLAY_H
