/**
 * @file
 * @brief Implementation of module OnlineMonitor
 *
 * @copyright Copyright (c) 2017-2024 CERN and the Corryvreckan authors.
 * This software is distributed under the terms of the MIT License, copied verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 * SPDX-License-Identifier: MIT
 */

#include "OnlineMonitor.h"
#include <TColor.h>
#include <TGButtonGroup.h>
#include <TStyle.h>
#include <TVirtualPadEditor.h>
#include <fstream>
#include <iomanip>
#include <regex>

using namespace corryvreckan;
using namespace std;

OnlineMonitor::OnlineMonitor(Configuration& config, std::vector<std::shared_ptr<Detector>> detectors)
    : Module(config, std::move(detectors)) {

    config_.setDefault<std::string>("canvas_title", "Corryvreckan Testbeam Monitor");
    config_.setDefault<int>("update", 200);
    config_.setDefault<bool>("ignore_aux", true);
    config_.setDefault<std::string>("clustering_module", "Clustering4D");
    config_.setDefault<std::string>("tracking_module", "Tracking4D");

    config_.setDefault<std::string>("discord_webhook", "");
    config_.setDefault<double>("discord_min_rate", 100.0);
    config_.setDefault<int>("discord_alert_duration", 30);
    config_.setDefault<double>("discord_max_hit_rate", 0.0);
    config_.setDefault<double>("discord_min_hit_rate", 0.0);
    config_.setDefault<int>("target_events", 0);
    config_.setDefault<int>("auto_save_interval", 0);
    config_.setDefault<std::string>("auto_save_dir", "./");

    canvasTitle = config_.get<std::string>("canvas_title");
    updateNumber = config_.get<int>("update");
    ignoreAux = config_.get<bool>("ignore_aux");
    clusteringModule = config_.get<std::string>("clustering_module");
    trackingModule = config_.get<std::string>("tracking_module");

    discordWebhook_ = config_.get<std::string>("discord_webhook");
    discordMinRate_ = config_.get<double>("discord_min_rate");
    discordAlertDuration_ = config_.get<int>("discord_alert_duration");
    discordMaxHitRate_ = config_.get<double>("discord_max_hit_rate");
    discordMinHitRate_ = config_.get<double>("discord_min_hit_rate");
    targetEvents_ = config_.get<int>("target_events");
    autoSaveInterval_ = config_.get<int>("auto_save_interval");
    autoSaveDir_ = config_.get<std::string>("auto_save_dir");

    config_.setDefaultMatrix<std::string>("overview",
                                          {{trackingModule + "/trackChi2ndof"},
                                           {clusteringModule + "/%REFERENCE%/clusterCharge"},
                                           {"Correlations/%REFERENCE%/hitmap", "colz"},
                                           {trackingModule + "/%REFERENCE%/local_residuals/LocalResidualsX"}});
    config_.setDefaultMatrix<std::string>("dut_plots",
                                          {{"EventLoaderEUDAQ2/%DUT%/hitmap", "colz"},
                                           {"EventLoaderEUDAQ2/%DUT%/hPixelTimes"},
                                           {"EventLoaderEUDAQ2/%DUT%/hPixelRawValues"},
                                           {"EventLoaderEUDAQ2/%DUT%/hPixelMultiplicityPerCorryEvent", "log"},
                                           {"AnalysisDUT/%DUT%/clusterChargeAssociated"},
                                           {"AnalysisDUT/%DUT%/associatedTracksVersusTime"}});

    config_.setDefaultMatrix<std::string>("tracking",
                                          {{trackingModule + "/trackChi2"},
                                           {trackingModule + "/trackAngleX"},
                                           {trackingModule + "/trackAngleY"},
                                           {trackingModule + "/trackChi2ndof"},
                                           {trackingModule + "/tracksPerEvent"},
                                           {trackingModule + "/clustersPerTrack"}});

    config_.setDefaultMatrix<std::string>("hitmaps", {{"Correlations/%DETECTOR%/hitmap", "colz"}});
    config_.setDefaultMatrix<std::string>("residuals", {{trackingModule + "/%DETECTOR%/local_residuals/LocalResidualsX"}});
    config_.setDefaultMatrix<std::string>("correlation_x", {{"Correlations/%DETECTOR%/correlationX"}});
    config_.setDefaultMatrix<std::string>("correlation_x2d", {{"Correlations/%DETECTOR%/correlationX_2Dlocal", "colz"}});
    config_.setDefaultMatrix<std::string>("correlation_y", {{"Correlations/%DETECTOR%/correlationY"}});
    config_.setDefaultMatrix<std::string>("correlation_y2d", {{"Correlations/%DETECTOR%/correlationY_2Dlocal", "colz"}});
    config_.setDefaultMatrix<std::string>("charge_distributions", {{clusteringModule + "/%DETECTOR%/clusterCharge"}});
    config_.setDefaultMatrix<std::string>("event_times", {{"Correlations/%DETECTOR%/eventTimes"}});

    // Set up overview plots:
    canvas_overview = config_.getMatrix<std::string>("overview");

    // Set up individual plots for the DUT
    canvas_dutplots = config_.getMatrix<std::string>("dut_plots");
    canvas_tracking = config_.getMatrix<std::string>("tracking");
    canvas_hitmaps = config_.getMatrix<std::string>("hitmaps");
    canvas_residuals = config_.getMatrix<std::string>("residuals");

    canvas_cx = config_.getMatrix<std::string>("correlation_x");
    canvas_cx2d = config_.getMatrix<std::string>("correlation_x2d");
    canvas_cy = config_.getMatrix<std::string>("correlation_y");
    canvas_cy2d = config_.getMatrix<std::string>("correlation_y2d");

    canvas_charge = config_.getMatrix<std::string>("charge_distributions");
    canvas_time = config_.getMatrix<std::string>("event_times");
}

void OnlineMonitor::initialize() {
    gui_run();

    // Timeline: 200 bins, each bin = one update interval
    const int nBins = 200;
    const double xMax = static_cast<double>(nBins * updateNumber);

    static const Color_t kDetColors[] = {
        kRed + 1, kBlue + 1, kGreen + 2, kOrange + 7, kMagenta + 1, kCyan + 2, kViolet + 1, kTeal + 1};
    int colorIdx = 0;

    for(auto& det : get_detectors()) {
        if(ignoreAux && det->isAuxiliary()) continue;
        if(det->hasRole(DetectorRole::PASSIVE)) continue;

        auto* prof = new TProfile(("hits_tl_" + det->getName()).c_str(),
                                  ";Event;Clusters / Event",
                                  nBins, 0.0, xMax);
        GuiDisplay::TimelineData td;
        td.detectorName = det->getName();
        td.profile = prof;
        td.color = kDetColors[colorIdx++ % 8];
        gui->timelineHitData_.push_back(td);
    }

    profile_tracks_ = new TProfile("tracks_tl", ";Event;Tracks / Event", nBins, 0.0, xMax);
    gui->timelineTrackProfile_ = profile_tracks_;

    gui->saveDir_ = autoSaveDir_;
    lastAutoSaveTime_ = std::chrono::steady_clock::now();
}

StatusCode OnlineMonitor::run(const std::shared_ptr<Clipboard>& clipboard) {
    fillTimeline(clipboard);
    gui_update();
    eventNumber++;
    return StatusCode::Success;
}

void OnlineMonitor::AddCanvasGroup(std::string group_title) {
    gui->buttonGroups[group_title] = new TGVButtonGroup(gui->buttonMenu, group_title.c_str());
    gui->buttonMenu->AddFrame(gui->buttonGroups[group_title], new TGLayoutHints(kLHintsLeft | kLHintsTop, 10, 10, 10, 10));
    gui->buttonGroups[group_title]->Show();
}

// Need special function to get scroll bar working
void OnlineMonitor::AddDUTGroup(uint64_t num_planes) {
    std::string group_title = "DUTs";

    // Dynamic sizing of DUT section for less than 3 planes
    auto vert_size = static_cast<UInt_t>((num_planes >= 3) ? 150 : 40 + num_planes * 40);

    // Adding an outer frame to place canvas inside
    gui->dutFrame = new TGMainFrame(gui->buttonMenu, 150, vert_size, kVerticalFrame | kFixedSize);
    gui->dutFrame->SetCleanup(kDeepCleanup);
    gui->buttonMenu->AddFrame(gui->dutFrame, new TGLayoutHints(kLHintsLeft, 10, 10, 10, 10));

    // Create canvas to attach to scrollbar
    gui->dutCanvas = new TGCanvas(gui->dutFrame, 71, 28, kFixedSize);
    gui->dutFrame->AddFrame(gui->dutCanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    // Creat Inner Frame
    gui->dutInnerFrame = new TGVerticalFrame(gui->dutCanvas->GetViewPort(), 71, 28);
    gui->dutCanvas->SetContainer(gui->dutInnerFrame);

    // Create ButtonGroup
    gui->buttonGroups[group_title] = new TGVButtonGroup(gui->dutInnerFrame, group_title.c_str());
    gui->dutInnerFrame->AddFrame(gui->buttonGroups[group_title], new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    gui->buttonGroups[group_title]->Show();
}

void OnlineMonitor::AddCanvas(std::string canvas_title,
                              std::string canvasGroup,
                              Matrix<std::string> canvas_plots,
                              bool ignoreDut,
                              std::string detector_name) {
    std::string canvas_name = canvas_title + "Canvas";

    if(canvasGroup.empty()) {
        gui->buttons[canvas_title] = new TGTextButton(gui->buttonMenu, canvas_title.c_str());
        gui->buttonMenu->AddFrame(gui->buttons[canvas_title], new TGLayoutHints(kLHintsLeft, 10, 10, 10, 10));
    } else {
        gui->buttons[canvas_title] = new TGTextButton(gui->buttonGroups[canvasGroup], canvas_title.c_str());
        gui->buttonGroups[canvasGroup]->AddFrame(gui->buttons[canvas_title],
                                                 new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 0, 0));
    }

    string command = "Display(=\"" + canvas_name + "\")";
    LOG(INFO) << "Connecting button with command " << command.c_str();
    gui->buttons[canvas_title]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, command.c_str());

    AddPlots(canvas_name, canvas_plots, ignoreDut, detector_name);
}

void OnlineMonitor::AddPlots(std::string canvas_name,
                             Matrix<std::string> canvas_plots,
                             bool ignoreDut,
                             std::string detector_name) {
    for(auto plot : canvas_plots) {

        // Add default plotting style if not set:
        plot.resize(2, "");

        // Do we need to plot with a LogY scale?
        bool log_scale = (plot.back().find("log") != std::string::npos) ? true : false;

        // Replace reference placeholders and add histogram
        std::string name = std::regex_replace(plot.front(), std::regex("%REFERENCE%"), get_reference()->getName());

        // Parse other placeholders:
        if(name.find("%DUT%") != std::string::npos) {
            // Do we have a DUT placeholder?
            if(!detector_name.empty()) {
                LOG(DEBUG) << "Adding plot " << name << " for detector " << detector_name;
                auto detector = get_detector(detector_name);
                AddHisto(
                    canvas_name, std::regex_replace(name, std::regex("%DUT%"), detector->getName()), plot.back(), log_scale);

            } else {
                LOG(DEBUG) << "Adding plot " << name << " for all DUTs.";
                for(auto& detector : get_duts()) {
                    AddHisto(canvas_name,
                             std::regex_replace(name, std::regex("%DUT%"), detector->getName()),
                             plot.back(),
                             log_scale);
                }
            }
        } else if(name.find("%DETECTOR%") != std::string::npos) {
            // Do we have a detector placeholder?
            if(!detector_name.empty()) {
                LOG(DEBUG) << "Adding plot " << name << " for detector " << detector_name;
                auto detector = get_detector(detector_name);
                AddHisto(canvas_name,
                         std::regex_replace(name, std::regex("%DETECTOR%"), detector->getName()),
                         plot.back(),
                         log_scale);
            } else {
                LOG(DEBUG) << "Adding plot " << name << " for all detectors.";
                for(auto& detector : get_detectors()) {
                    // Ignore AUX detectors
                    if(ignoreAux && detector->isAuxiliary()) {
                        continue;
                    }

                    // Ignore DUTs if configured that way:
                    if(ignoreDut && detector->isDUT()) {
                        continue;
                    }

                    if(detector->hasRole(DetectorRole::PASSIVE)) {
                        continue;
                    }

                    AddHisto(canvas_name,
                             std::regex_replace(name, std::regex("%DETECTOR%"), detector->getName()),
                             plot.back(),
                             log_scale);
                }
            }
        } else {
            // Single histogram only.
            AddHisto(canvas_name, name, plot.back(), log_scale);
        }
    }
}

void OnlineMonitor::AddHisto(string canvasName, string histoName, string style, bool logy) {

    // Add root directory to path:
    histoName = "/" + histoName;

    TH1* histogram = static_cast<TH1*>(gDirectory->Get(histoName.c_str()));
    if(histogram) {
        gui->histograms[canvasName].push_back(static_cast<TH1*>(gDirectory->Get(histoName.c_str())));
        gui->logarithmic[gui->histograms[canvasName].back()] = logy;
        gui->styles[gui->histograms[canvasName].back()] = style;
    } else {
        LOG(WARNING) << "Histogram " << histoName << " does not exist";
    }
}

void OnlineMonitor::gui_run() {

    // Global ROOT style
    gStyle->SetPalette(kBird);
    gStyle->SetOptStat("enmr");
    gStyle->SetStatFont(42);
    gStyle->SetStatBorderSize(1);
    gStyle->SetStatX(0.96f);
    gStyle->SetStatY(0.92f);
    gStyle->SetFrameBorderMode(0);
    gStyle->SetCanvasBorderMode(0);
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetTitleSize(0.05f, "XYZ");
    gStyle->SetLabelSize(0.04f, "XYZ");
    gStyle->SetHistLineWidth(2);
    gStyle->SetGridStyle(3);
    gStyle->SetGridColor(kGray + 1);

    // TApplication keeps the canvases persistent
    app = new TApplication("example", nullptr, nullptr);

    // Make the GUI
    gui = new GuiDisplay(gClient->GetRoot(), 1200, 600);

    // Make the main window object and set the attributes
    gui->buttonMenu = new TGHorizontalFrame(gui, 1200, 50);
    gui->canvas = new TRootEmbeddedCanvas("canvas", gui, 1200, 600);
    gui->AddFrame(gui->canvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 10, 10, 10, 10));
    gui->SetCleanup(kDeepCleanup);
    gui->DontCallClose();

    // Add canvases and histograms
    AddCanvasGroup("Tracking");
    AddCanvas("Overview", "Tracking", canvas_overview);
    gui->canvasOrder_.push_back("OverviewCanvas");
    AddCanvas("Tracking Performance", "Tracking", canvas_tracking);
    gui->canvasOrder_.push_back("Tracking PerformanceCanvas");
    AddCanvas("Residuals", "Tracking", canvas_residuals, true);
    gui->canvasOrder_.push_back("ResidualsCanvas");

    gui->buttons["Timeline"] = new TGTextButton(gui->buttonGroups["Tracking"], "Timeline");
    gui->buttonGroups["Tracking"]->AddFrame(gui->buttons["Timeline"],
                                            new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 0, 0));
    gui->buttons["Timeline"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "DisplayTimeline()");
    gui->canvasOrder_.push_back("TimelineCanvas");

    AddCanvasGroup("Detectors");
    AddCanvas("Hitmaps", "Detectors", canvas_hitmaps);
    gui->canvasOrder_.push_back("HitmapsCanvas");
    AddCanvas("Event Times", "Detectors", canvas_time);
    gui->canvasOrder_.push_back("Event TimesCanvas");
    AddCanvas("Charge Distributions", "Detectors", canvas_charge);
    gui->canvasOrder_.push_back("Charge DistributionsCanvas");

    AddCanvasGroup("Correlations 1D");
    AddCanvas("1D X", "Correlations 1D", canvas_cx);
    gui->canvasOrder_.push_back("1D XCanvas");
    AddCanvas("1D Y", "Correlations 1D", canvas_cy);
    gui->canvasOrder_.push_back("1D YCanvas");

    AddCanvasGroup("Correlations 2D");
    AddCanvas("2D X", "Correlations 2D", canvas_cx2d);
    AddCanvas("2D Y", "Correlations 2D", canvas_cy2d);

    if(!get_duts().empty()) {
        AddDUTGroup(get_duts().size());
        for(auto& detector : get_duts()) {
            AddCanvas(detector->getName(), "DUTs", canvas_dutplots, false, detector->getName());
        }

        gui->buttonGroups["DUTs"]->SetWidth(55);
        gui->buttonGroups["DUTs"]->SetHeight(8);
    }

    // Set up the main frame before drawing
    AddCanvasGroup("Controls");
    ULong_t color;

    // Pause button
    gClient->GetColorByName("green", color);
    gui->buttons["pause"] = new TGTextButton(gui->buttonGroups["Controls"], "   &Pause Monitoring   ");
    gui->buttons["pause"]->ChangeBackground(color);
    gui->buttons["pause"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "TogglePause()");
    gui->buttonGroups["Controls"]->AddFrame(gui->buttons["pause"], new TGLayoutHints(kLHintsTop | kLHintsExpandX));

    // Full Screen button
    gui->buttons["fullscreen"] = new TGTextButton(gui->buttonGroups["Controls"], "&Full Screen");
    gui->buttons["fullscreen"]->ChangeBackground(static_cast<Pixel_t>(TColor::GetColor(70, 130, 180)));
    gui->buttons["fullscreen"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "FullScreen()");
    gui->buttonGroups["Controls"]->AddFrame(gui->buttons["fullscreen"], new TGLayoutHints(kLHintsTop | kLHintsExpandX));

    // Exit button
    gClient->GetColorByName("yellow", color);
    gui->buttons["exit"] = new TGTextButton(gui->buttonGroups["Controls"], "&Exit Monitor");
    gui->buttons["exit"]->ChangeBackground(color);
    gui->buttons["exit"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "Exit()");
    gui->buttonGroups["Controls"]->AddFrame(gui->buttons["exit"], new TGLayoutHints(kLHintsTop | kLHintsExpandX));

    // Status bar at the bottom
    gui->statusBar = new TGStatusBar(gui, 1200, 22, kHorizontalFrame);
    Int_t statusParts[] = {50, 30, 20};
    gui->statusBar->SetParts(statusParts, 3);
    gui->statusBar->SetText("  Events: 0   |   Rate: 0.0 evt/s", 0);
    gui->statusBar->SetText("  Canvas: Overview", 1);
    gui->statusBar->SetText("  --:--:--", 2);

    // Progress bar row (between button menu and status bar)
    gui->progressFrame = new TGHorizontalFrame(gui, 1200, 26);
    gui->progressBar = new TGHProgressBar(gui->progressFrame, TGProgressBar::kFancy, 850);
    gui->progressBar->SetRange(0.0f, 100.0f);
    gui->progressBar->SetBarColor("SteelBlue");
    gui->progressBar->ShowPosition(kTRUE, kFALSE, "%.0f%%");
    gui->progressLabel = new TGLabel(gui->progressFrame, "  0 evt");
    gui->progressFrame->AddFrame(gui->progressBar,
                                 new TGLayoutHints(kLHintsExpandX | kLHintsCenterY, 5, 5, 2, 2));
    gui->progressFrame->AddFrame(gui->progressLabel,
                                 new TGLayoutHints(kLHintsRight | kLHintsCenterY, 10, 5, 2, 2));

    // Main frame resizing
    gui->AddFrame(gui->buttonMenu, new TGLayoutHints(kLHintsLeft, 10, 10, 10, 10));
    gui->AddFrame(gui->progressFrame, new TGLayoutHints(kLHintsExpandX, 5, 5, 2, 2));
    gui->AddFrame(gui->statusBar, new TGLayoutHints(kLHintsBottom | kLHintsExpandX));

    // Keyboard shortcuts: bind keys and store keycode→action map
    auto bindKey = [&](EKeySym sym, const std::string& action) {
        auto kc = static_cast<Long_t>(gVirtualX->KeysymToKeycode(sym));
        gui->BindKey(gui, static_cast<Int_t>(kc), 0);
        gui->keycodeActions_[kc] = action;
    };
    bindKey(kKey_Space, "pause");
    bindKey(kKey_f,     "fullscreen");
    bindKey(kKey_F,     "fullscreen");
    bindKey(kKey_s,     "save");
    bindKey(kKey_S,     "save");
    for(int i = 1; i <= 9; i++) {
        bindKey(static_cast<EKeySym>(kKey_0 + i), "canvas" + std::to_string(i - 1));
    }

    gui->SetWindowName(canvasTitle.c_str());
    gui->MapSubwindows();
    gui->Resize(gui->GetDefaultSize());

    // Draw the main frame
    gui->MapWindow();

    // Plot the overview tab (if it exists)
    if(gui->histograms["OverviewCanvas"].size() != 0) {
        gui->Display(const_cast<char*>(std::string("OverviewCanvas").c_str()));
    }

    gui->canvas->GetCanvas()->Paint();
    gui->canvas->GetCanvas()->Update();
    gSystem->ProcessEvents();

    // Initialise member variables
    eventNumber = 0;
    lastUpdateTime_ = std::chrono::steady_clock::now();
    lastUpdateEventNumber_ = 0;
    eventRate_ = 0.0;
}

void OnlineMonitor::sendDiscordPayload(const std::string& jsonPath) {
    static const std::string snapPath = "/tmp/corry_snap.png";
    gui->canvas->GetCanvas()->Print(snapPath.c_str());
    std::string cmd = "curl -s -X POST"
                      " -F \"payload_json=$(cat '" + jsonPath + "')\""
                      " -F \"file=@'" + snapPath + "';filename=corry_snap.png\""
                      " \"" + discordWebhook_ + "\" > /dev/null 2>&1 &";
    if(system(cmd.c_str()) != 0) {
        LOG(WARNING) << "Discord command returned non-zero";
    }
}

void OnlineMonitor::checkDiscordAlert() {
    if(discordWebhook_.empty()) return;

    auto now = std::chrono::steady_clock::now();

    if(eventRate_ < discordMinRate_) {
        if(!discordAlertActive_) {
            discordAlertActive_ = true;
            discordBelowSince_ = now;
            discordNotificationSent_ = false;
            LOG(INFO) << "Trigger rate " << std::fixed << std::setprecision(1) << eventRate_
                      << " evt/s dropped below threshold " << discordMinRate_ << " evt/s";
        } else if(!discordNotificationSent_) {
            double elapsed = std::chrono::duration<double>(now - discordBelowSince_).count();
            if(elapsed >= static_cast<double>(discordAlertDuration_)) {
                sendDiscordAlert();
                discordNotificationSent_ = true;
            }
        }
    } else {
        if(discordAlertActive_ && discordNotificationSent_) {
            sendDiscordRecovery();
        } else if(discordAlertActive_) {
            LOG(INFO) << "Trigger rate recovered before alert was sent";
        }
        discordAlertActive_ = false;
        discordNotificationSent_ = false;
    }
}

void OnlineMonitor::sendDiscordAlert() {
    std::string tmpPath = "/tmp/corry_discord_alert.json";
    {
        std::ofstream f(tmpPath);
        if(!f) {
            LOG(ERROR) << "Cannot write Discord payload to " << tmpPath;
            return;
        }
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - discordBelowSince_).count();
        f << std::fixed << std::setprecision(1);
        f << "{\"embeds\":[{";
        f << "\"title\":\"\\u26a0\\ufe0f Low Trigger Rate\",";
        f << "\"color\":16711680,";
        f << "\"fields\":[";
        f << "{\"name\":\"Current Rate\",\"value\":\"" << eventRate_ << " evt\\/s\",\"inline\":true},";
        f << "{\"name\":\"Threshold\",\"value\":\"" << discordMinRate_ << " evt\\/s\",\"inline\":true},";
        f << "{\"name\":\"Duration\",\"value\":\"" << static_cast<int>(elapsed) << " s\",\"inline\":true}";
        f << "],\"image\":{\"url\":\"attachment://corry_snap.png\"}}]}";
    }
    sendDiscordPayload(tmpPath);
    LOG(INFO) << "Discord alert sent: " << std::fixed << std::setprecision(1)
              << eventRate_ << " evt/s < " << discordMinRate_ << " evt/s";
}

void OnlineMonitor::sendDiscordRecovery() {
    std::string tmpPath = "/tmp/corry_discord_recovery.json";
    {
        std::ofstream f(tmpPath);
        if(!f) return;
        f << std::fixed << std::setprecision(1);
        f << "{\"embeds\":[{";
        f << "\"title\":\"\\u2705 Trigger Rate Recovered\",";
        f << "\"color\":3066993,";
        f << "\"fields\":[";
        f << "{\"name\":\"Current Rate\",\"value\":\"" << eventRate_ << " evt\\/s\",\"inline\":true},";
        f << "{\"name\":\"Threshold\",\"value\":\"" << discordMinRate_ << " evt\\/s\",\"inline\":true}";
        f << "],\"image\":{\"url\":\"attachment://corry_snap.png\"}}]}";
    }
    sendDiscordPayload(tmpPath);
    LOG(INFO) << "Discord recovery sent: " << std::fixed << std::setprecision(1) << eventRate_ << " evt/s";
}

void OnlineMonitor::checkPlaneAlerts() {
    if(discordWebhook_.empty()) return;
    if(discordMaxHitRate_ <= 0.0 && discordMinHitRate_ <= 0.0) return;

    auto now = std::chrono::steady_clock::now();

    // Compute rates from accumulated data and reset counters
    for(auto& td : gui->timelineHitData_) {
        int ev = planeEventAccum_[td.detectorName];
        if(ev > 0) {
            planeHitRate_[td.detectorName] =
                static_cast<double>(planeHitAccum_[td.detectorName]) / ev;
        }
        planeHitAccum_[td.detectorName] = 0;
        planeEventAccum_[td.detectorName] = 0;
    }

    for(auto& td : gui->timelineHitData_) {
        const std::string& name = td.detectorName;
        double rate = planeHitRate_[name];
        auto& alert = planeAlerts_[name];

        bool tooHigh = (discordMaxHitRate_ > 0.0) && (rate > discordMaxHitRate_);
        bool tooLow  = (discordMinHitRate_ > 0.0) && (rate < discordMinHitRate_)
                       && (eventNumber > updateNumber * 2);

        if(tooHigh || tooLow) {
            if(!alert.active) {
                alert.active = true;
                alert.sent = false;
                alert.since = now;
                alert.isHighRate = tooHigh;
            } else if(!alert.sent) {
                double elapsed = std::chrono::duration<double>(now - alert.since).count();
                if(elapsed >= static_cast<double>(discordAlertDuration_)) {
                    sendPlaneAlert(name, rate, tooHigh);
                    alert.sent = true;
                }
            }
        } else {
            if(alert.active && alert.sent) {
                sendPlaneRecovery(name, rate);
            }
            alert.active = false;
            alert.sent = false;
        }
    }
}

void OnlineMonitor::sendPlaneAlert(const std::string& detName, double rate, bool isHigh) {
    std::string tmpPath = "/tmp/corry_plane_alert.json";
    {
        std::ofstream f(tmpPath);
        if(!f) return;
        f << std::fixed << std::setprecision(2);
        double threshold = isHigh ? discordMaxHitRate_ : discordMinHitRate_;
        std::string title = isHigh ? "\\u2b06 High Hit Rate: " : "\\u2b07 Low Hit Rate: ";
        int color = isHigh ? 15158332 : 3447003;  // red : blue
        f << "{\"embeds\":[{";
        f << "\"title\":\"" << title << detName << "\",";
        f << "\"color\":" << color << ",";
        f << "\"fields\":[";
        f << "{\"name\":\"Plane\",\"value\":\"" << detName << "\",\"inline\":true},";
        f << "{\"name\":\"Hit Rate\",\"value\":\"" << rate << " clusters\\/evt\",\"inline\":true},";
        f << "{\"name\":\"Threshold\",\"value\":\"" << threshold << " clusters\\/evt\",\"inline\":true}";
        f << "],\"image\":{\"url\":\"attachment://corry_snap.png\"}}]}";
    }
    sendDiscordPayload(tmpPath);
    LOG(INFO) << "Plane " << detName << " hit rate " << std::fixed << std::setprecision(2) << rate
              << " clusters/evt is " << (isHigh ? "above max" : "below min") << " threshold";
}

void OnlineMonitor::sendPlaneRecovery(const std::string& detName, double rate) {
    std::string tmpPath = "/tmp/corry_plane_recovery.json";
    {
        std::ofstream f(tmpPath);
        if(!f) return;
        f << std::fixed << std::setprecision(2);
        f << "{\"embeds\":[{";
        f << "\"title\":\"\\u2705 Hit Rate Recovered: " << detName << "\",";
        f << "\"color\":3066993,";
        f << "\"fields\":[";
        f << "{\"name\":\"Plane\",\"value\":\"" << detName << "\",\"inline\":true},";
        f << "{\"name\":\"Hit Rate\",\"value\":\"" << rate << " clusters\\/evt\",\"inline\":true}";
        f << "],\"image\":{\"url\":\"attachment://corry_snap.png\"}}]}";
    }
    sendDiscordPayload(tmpPath);
    LOG(INFO) << "Plane " << detName << " hit rate recovered to " << std::fixed
              << std::setprecision(2) << rate << " clusters/evt";
}

void OnlineMonitor::fillTimeline(const std::shared_ptr<Clipboard>& clipboard) {
    double ev = static_cast<double>(eventNumber);

    for(auto& td : gui->timelineHitData_) {
        auto& clusters = clipboard->getData<Cluster>(td.detectorName);
        double n = static_cast<double>(clusters.size());
        td.profile->Fill(ev, n);
        planeHitAccum_[td.detectorName] += static_cast<int>(clusters.size());
        planeEventAccum_[td.detectorName]++;
    }

    if(profile_tracks_) {
        auto& tracks = clipboard->getData<Track>();
        profile_tracks_->Fill(ev, static_cast<double>(tracks.size()));
    }
}

void OnlineMonitor::gui_update() {
    auto now = std::chrono::steady_clock::now();
    double wallElapsed = std::chrono::duration<double>(now - lastUpdateTime_).count();

    // Trigger display update every updateNumber events, rate refresh every 5 s (for Discord)
    bool eventCycle = !gui->isPaused() && (eventNumber > 0) && (eventNumber % updateNumber == 0);
    bool wallClock = (wallElapsed >= 5.0);

    if(eventCycle || wallClock) {
        if(wallElapsed > 0) {
            eventRate_ = static_cast<double>(eventNumber - lastUpdateEventNumber_) / wallElapsed;
        }
        lastUpdateTime_ = now;
        lastUpdateEventNumber_ = eventNumber;

        if(eventCycle) {
            gui->Update();
        }
        gui->updateStatus(eventNumber, eventRate_);
        gui->updateProgress(eventNumber, targetEvents_);
        checkDiscordAlert();
        checkPlaneAlerts();

        // Auto-save current canvas
        if(autoSaveInterval_ > 0) {
            double sinceLastSave = std::chrono::duration<double>(now - lastAutoSaveTime_).count();
            if(sinceLastSave >= static_cast<double>(autoSaveInterval_)) {
                time_t t = time(nullptr);
                char ts[32];
                strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", localtime(&t));
                gui->SaveCanvas(autoSaveDir_ + "corry_" + ts + ".png");
                lastAutoSaveTime_ = now;
            }
        }
    }

    gSystem->ProcessEvents();
}
