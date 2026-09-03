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
#include <KeySymbols.h>
#include <TColor.h>
#include <TGButtonGroup.h>
#include <TStyle.h>
#include <TVirtualPadEditor.h>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <regex>
#include <vector>

using namespace corryvreckan;
using namespace std;

OnlineMonitor::OnlineMonitor(Configuration& config, std::vector<std::shared_ptr<Detector>> detectors)
    : Module(config, std::move(detectors)) {

    config_.setDefault<std::string>("canvas_title", "Corryvreckan Testbeam Monitor");
    config_.setDefault<int>("update", 200);
    config_.setDefault<bool>("ignore_aux", true);
    config_.setDefault<std::string>("clustering_module", "Clustering4D");
    config_.setDefault<std::string>("tracking_module", "Tracking4D");
    config_.setDefault<std::string>("event_counter", "run_count");
    config_.setDefault<std::string>("event_counter_dut", "");

    config_.setDefault<int>("target_events", 0);
    config_.setDefault<int>("auto_save_interval", 0);
    config_.setDefault<std::string>("auto_save_dir", "./");
    config_.setDefault<int>("telescope_reset_tracks", 0);
    config_.setDefault<double>("warning_min_clusters_per_event", 0.0);
    config_.setDefault<double>("warning_min_tracks_per_event", 0.0);
    config_.setDefault<int>("warning_duration", 10);

    canvasTitle = config_.get<std::string>("canvas_title");
    updateNumber = config_.get<int>("update");
    ignoreAux = config_.get<bool>("ignore_aux");
    clusteringModule = config_.get<std::string>("clustering_module");
    trackingModule = config_.get<std::string>("tracking_module");

    // event_counter selects what eventNumber (displayed Events count, progress bar, Timeline/Corr Trends
    // X-axis) counts. Rate-style metrics (status bar "Rate: X evt/s", warning mode's clusters/tracks-per-
    // event) are unaffected -- they always use one-per-run() as their denominator (runCount_), since
    // "tracks per event" would be meaningless if "event" itself already meant "a track".
    std::string event_counter_str = config_.get<std::string>("event_counter");
    if(event_counter_str == "run_count") {
        eventCounterMode_ = EventCounterMode::RunCount;
        eventCounterLabel_ = "Event";
    } else if(event_counter_str == "tracks") {
        eventCounterMode_ = EventCounterMode::Tracks;
        eventCounterLabel_ = "Track";
    } else if(event_counter_str == "dut_associated_tracks") {
        eventCounterMode_ = EventCounterMode::DutAssociatedTracks;
        eventCounterLabel_ = "DUT-associated Track";
    } else {
        throw InvalidValueError(
            config_, "event_counter", "Must be one of \"run_count\", \"tracks\", \"dut_associated_tracks\"");
    }

    if(eventCounterMode_ == EventCounterMode::DutAssociatedTracks) {
        auto duts = get_duts();
        if(duts.empty()) {
            throw InvalidValueError(
                config_, "event_counter", "event_counter=\"dut_associated_tracks\" requires at least one DUT in the geometry");
        }
        eventCounterDut_ = config_.get<std::string>("event_counter_dut");
        if(eventCounterDut_.empty()) {
            if(duts.size() > 1) {
                throw InvalidValueError(config_,
                                        "event_counter_dut",
                                        "Multiple DUTs in geometry -- event_counter_dut must specify which one to "
                                        "count associated tracks for");
            }
            eventCounterDut_ = duts.front()->getName();
        } else if(std::none_of(duts.begin(), duts.end(), [&](auto& d) { return d->getName() == eventCounterDut_; })) {
            throw InvalidValueError(config_, "event_counter_dut", "\"" + eventCounterDut_ + "\" is not a DUT in the geometry");
        }
        LOG(STATUS) << "Event counter: DUT-associated tracks on \"" << eventCounterDut_ << "\"";
    } else if(eventCounterMode_ == EventCounterMode::Tracks) {
        LOG(STATUS) << "Event counter: tracks";
    }

    targetEvents_ = config_.get<int>("target_events");
    autoSaveInterval_ = config_.get<int>("auto_save_interval");
    autoSaveDir_ = config_.get<std::string>("auto_save_dir");
    telescopeResetTracks_ = config_.get<int>("telescope_reset_tracks");
    warningMinClustersPerEvent_ = config_.get<double>("warning_min_clusters_per_event");
    warningMinTracksPerEvent_ = config_.get<double>("warning_min_tracks_per_event");
    warningDuration_ = config_.get<int>("warning_duration");

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
    // Residual-vs-event trend TProfiles must be booked *before* gui_run() below: gui_run() builds
    // the Residuals canvas immediately, and AddHisto() only finds histograms that already exist
    // in gDirectory at that point (see residualTrendData_'s doc comment in the header).
    {
        static constexpr int kResidualTrendNBins = 200;
        const double residualXMax = static_cast<double>(kResidualTrendNBins * updateNumber);
        for(auto& det : get_detectors()) {
            if(ignoreAux && det->isAuxiliary()) continue;
            if(det->hasRole(DetectorRole::PASSIVE)) continue;

            ResidualTrendData rd;
            rd.detectorName = det->getName();
            rd.isDut = det->isDUT();

            std::string titleX = det->getName() + " Residual X vs Event;Event;Local Residual X [mm]";
            rd.profileX = new TProfile(("residual_trend_x_" + det->getName()).c_str(),
                                       titleX.c_str(), kResidualTrendNBins, 0.0, residualXMax);
            rd.profileX->SetCanExtend(TH1::kAllAxes);

            std::string titleY = det->getName() + " Residual Y vs Event;Event;Local Residual Y [mm]";
            rd.profileY = new TProfile(("residual_trend_y_" + det->getName()).c_str(),
                                       titleY.c_str(), kResidualTrendNBins, 0.0, residualXMax);
            rd.profileY->SetCanExtend(TH1::kAllAxes);

            residualTrendData_.push_back(rd);
        }
    }

    gui_run();

    // Timeline: 200 bins, each bin = one update interval
    timelineNBins_ = 200;
    const double xMax = static_cast<double>(timelineNBins_ * updateNumber);

    static const Color_t kDetColors[] = {
        kRed + 1, kBlue + 1, kGreen + 2, kOrange + 7, kMagenta + 1, kCyan + 2, kViolet + 1, kTeal + 1};
    int colorIdx = 0;

    for(auto& det : get_detectors()) {
        if(ignoreAux && det->isAuxiliary()) continue;
        if(det->hasRole(DetectorRole::PASSIVE)) continue;

        // X-axis is eventCounterLabel_ (what eventNumber counts); "Clusters / Event" on the Y-axis
        // deliberately keeps "Event" meaning "one run() call" -- that is what is actually being
        // averaged per bin, independent of what eventNumber's own units are (see run()/EventCounterMode).
        auto* prof = new TProfile(("hits_tl_" + det->getName()).c_str(),
                                  (";" + eventCounterLabel_ + ";Clusters / Event").c_str(),
                                  timelineNBins_, 0.0, xMax);
        prof->SetCanExtend(TH1::kAllAxes);
        GuiDisplay::TimelineData td;
        td.detectorName = det->getName();
        td.profile = prof;
        td.color = kDetColors[colorIdx++ % 8];
        gui->timelineHitData_.push_back(td);
    }

    profile_tracks_ = new TProfile(
        "tracks_tl", (";" + eventCounterLabel_ + ";Tracks / Event").c_str(), timelineNBins_, 0.0, xMax);
    profile_tracks_->SetCanExtend(TH1::kAllAxes);
    gui->timelineTrackProfile_ = profile_tracks_;

    // Correlation trend profiles: mean(global_pos_i - global_pos_ref) vs event number
    static const Color_t kCorrColors[] = {kRed+1, kBlue+1, kGreen+2, kOrange+7, kMagenta+1, kCyan+2};
    int cIdx = 0;
    auto refDet = get_reference();
    if(refDet) {
        for(auto& det : get_detectors()) {
            if(ignoreAux && det->isAuxiliary()) continue;
            if(det->hasRole(DetectorRole::PASSIVE)) continue;
            if(det->getName() == refDet->getName()) continue;

            GuiDisplay::CorrelationTrendData cd;
            cd.detectorName = det->getName();
            cd.color = kCorrColors[cIdx++ % 6];

            std::string titleX =
                ";" + eventCounterLabel_ + ";#Delta X_{global} [mm] (" + det->getName() + " - " + refDet->getName() + ")";
            cd.profileX = new TProfile(("corr_x_tl_" + det->getName()).c_str(),
                                       titleX.c_str(), timelineNBins_, 0.0, xMax);
            cd.profileX->SetCanExtend(TH1::kAllAxes);

            std::string titleY =
                ";" + eventCounterLabel_ + ";#Delta Y_{global} [mm] (" + det->getName() + " - " + refDet->getName() + ")";
            cd.profileY = new TProfile(("corr_y_tl_" + det->getName()).c_str(),
                                       titleY.c_str(), timelineNBins_, 0.0, xMax);
            cd.profileY->SetCanExtend(TH1::kAllAxes);

            gui->correlationTrendData_.push_back(cd);
        }
    }

    // Telescope view: collect detector z positions and spatial extent
    {
        double zMin = std::numeric_limits<double>::max();
        double zMax = std::numeric_limits<double>::lowest();
        double xHalf = 0.0, yHalf = 0.0;
        for(auto& det : get_detectors()) {
            if(ignoreAux && det->isAuxiliary()) continue;
            if(det->hasRole(DetectorRole::PASSIVE)) continue;
            double z = det->displacement().z();
            telescopeDetZ_.push_back(z);
            zMin = std::min(zMin, z);
            zMax = std::max(zMax, z);
            auto sz = det->getSize();
            xHalf = std::max(xHalf, std::abs(det->displacement().x()) + sz.x() * 0.6);
            yHalf = std::max(yHalf, std::abs(det->displacement().y()) + sz.y() * 0.6);
        }
        if(!telescopeDetZ_.empty()) {
            double zMargin = std::max(1.0, (zMax - zMin) * 0.05);
            xHalf = std::max(xHalf, 5.0);
            yHalf = std::max(yHalf, 5.0);
            telescopeHitXZ_ = new TH2F("tel_xz", ";z [mm];x [mm]",
                                       300, zMin - zMargin, zMax + zMargin,
                                       200, -xHalf, xHalf);
            telescopeHitYZ_ = new TH2F("tel_yz", ";z [mm];y [mm]",
                                       300, zMin - zMargin, zMax + zMargin,
                                       200, -yHalf, yHalf);
            gui->telescopeHitXZ = telescopeHitXZ_;
            gui->telescopeHitYZ = telescopeHitYZ_;
        }
    }

    gui->saveDir_ = autoSaveDir_;
    lastAutoSaveTime_ = std::chrono::steady_clock::now();
}

StatusCode OnlineMonitor::run(const std::shared_ptr<Clipboard>& clipboard) {
    fillTimeline(clipboard);
    fillCorrelation(clipboard);
    fillResidualTrend(clipboard);

    // runCount_ (one per run() call) stays the denominator for all rate-style metrics computed in
    // gui_update() below; eventNumber (displayed Events count) advances by whatever event_counter_
    // selects, and can therefore jump by more than one -- or not at all -- per call.
    runCount_++;
    eventNumber += computeEventIncrement(clipboard);

    gui_update();
    return StatusCode::Success;
}

int OnlineMonitor::computeEventIncrement(const std::shared_ptr<Clipboard>& clipboard) {
    switch(eventCounterMode_) {
    case EventCounterMode::RunCount:
        return 1;
    case EventCounterMode::Tracks:
        return static_cast<int>(clipboard->getData<Track>().size());
    case EventCounterMode::DutAssociatedTracks: {
        int n = 0;
        for(auto& track : clipboard->getData<Track>()) {
            if(!track->getAssociatedClusters(eventCounterDut_).empty()) {
                n++;
            }
        }
        return n;
    }
    default:
        return 1; // unreachable -- all EventCounterMode values are handled above
    }
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
        // Allow 1D histograms to extend their x-axis when data exceeds the initial range
        if(!dynamic_cast<TH2*>(histogram)) {
            histogram->SetCanExtend(TH1::kAllAxes);
        }
        gui->histograms[canvasName].push_back(histogram);
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

    // Residual-vs-event trend: OnlineMonitor's own live TProfiles (residualTrendData_, filled in
    // fillResidualTrend()). Added individually rather than through canvas_residuals/%DETECTOR%,
    // since that matrix is tied to this canvas's ignoreDut=true above and DUTs should appear here.
    for(auto& rd : residualTrendData_) {
        AddHisto("ResidualsCanvas", "OnlineMonitor/residual_trend_x_" + rd.detectorName);
        AddHisto("ResidualsCanvas", "OnlineMonitor/residual_trend_y_" + rd.detectorName);
    }

    gui->buttons["Timeline"] = new TGTextButton(gui->buttonGroups["Tracking"], "Timeline");
    gui->buttonGroups["Tracking"]->AddFrame(gui->buttons["Timeline"],
                                            new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 0, 0));
    gui->buttons["Timeline"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "DisplayTimeline()");
    gui->canvasOrder_.push_back("TimelineCanvas");

    gui->buttons["Telescope"] = new TGTextButton(gui->buttonGroups["Tracking"], "Telescope");
    gui->buttonGroups["Tracking"]->AddFrame(gui->buttons["Telescope"],
                                            new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 0, 0));
    gui->buttons["Telescope"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "DisplayTelescope()");
    gui->canvasOrder_.push_back("TelescopeCanvas");

    gui->buttons["CorrTrends"] = new TGTextButton(gui->buttonGroups["Tracking"], "Corr Trends");
    gui->buttonGroups["Tracking"]->AddFrame(gui->buttons["CorrTrends"],
                                            new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 0, 0));
    gui->buttons["CorrTrends"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "DisplayCorrelation()");
    gui->canvasOrder_.push_back("CorrelationCanvas");

    AddCanvasGroup("Detectors");
    AddCanvas("Hitmaps", "Detectors", canvas_hitmaps);
    gui->canvasOrder_.push_back("HitmapsCanvas");
    AddCanvas("Event Times", "Detectors", canvas_time);
    gui->canvasOrder_.push_back("Event TimesCanvas");
    AddCanvas("Charge Distributions", "Detectors", canvas_charge);
    gui->canvasOrder_.push_back("Charge DistributionsCanvas");

    // L1idC / event-time monitoring canvas
    {
        Matrix<std::string> canvas_l1idc = {
            {"EventLoaderMALTA/hL1idCVsEvent"},
            {"EventLoaderMALTA/hTimerVsEvent"},
            {"EventLoaderMALTA/hL1idCPlaneDelta"},
            {"EventLoaderMALTA/hCoincidenceRateTrend"},
            {"EventLoaderMALTA/hL1idAcceptanceRateTrend"}
        };
        AddCanvas("L1idC", "Detectors", canvas_l1idc);
        gui->canvasOrder_.push_back("L1idCCanvas");
    }

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

    gui->windowTitle_ = canvasTitle;
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
    runCount_ = 0;
    lastUpdateTime_ = std::chrono::steady_clock::now();
    lastUpdateEventNumber_ = 0;
    lastUpdateRunCount_ = 0;
    eventRate_ = 0.0;
}

void OnlineMonitor::checkWarningMode() {
    if(warningMinClustersPerEvent_ <= 0.0 && warningMinTracksPerEvent_ <= 0.0) return;

    auto now = std::chrono::steady_clock::now();

    // Compute average clusters/event across all planes
    double avgClusters = 0.0;
    int nPlanes = 0;
    for(auto& [name, accum] : warnClusterAccum_) {
        if(warnEventAccum_ > 0) {
            avgClusters += static_cast<double>(accum) / warnEventAccum_;
            nPlanes++;
        }
        warnClusterAccum_[name] = 0;
    }
    if(nPlanes > 0) avgClusters /= nPlanes;

    // Compute tracks/event
    double tracksPerEvent = 0.0;
    if(warnEventAccum_ > 0) {
        tracksPerEvent = static_cast<double>(warnTrackAccum_) / warnEventAccum_;
    }
    warnTrackAccum_ = 0;
    warnEventAccum_ = 0;

    // Check if any enabled threshold is breached (skip very early startup). Uses runCount_, not
    // eventNumber: this is a "have we processed enough real readouts yet" guard, which stays a
    // run()-count concept regardless of eventCounterMode_ (eventNumber could advance far slower than
    // runCount_, e.g. eventCounterMode_=="tracks" with a low tracking rate).
    bool clustersBreach = (warningMinClustersPerEvent_ > 0.0) && (nPlanes > 0) &&
                          (avgClusters < warningMinClustersPerEvent_) && (runCount_ > updateNumber * 2);
    bool tracksBreach = (warningMinTracksPerEvent_ > 0.0) &&
                        (tracksPerEvent < warningMinTracksPerEvent_) && (runCount_ > updateNumber * 2);
    bool belowThreshold = clustersBreach || tracksBreach;

    if(belowThreshold) {
        if(!warningInitiated_) {
            warningInitiated_ = true;
            warningBelowSince_ = now;
            LOG(WARNING) << "Low rate detected:"
                         << " clusters/evt=" << std::fixed << std::setprecision(2) << avgClusters
                         << " tracks/evt=" << tracksPerEvent;
        } else if(!warningModeActive_) {
            double elapsed = std::chrono::duration<double>(now - warningBelowSince_).count();
            if(elapsed >= static_cast<double>(warningDuration_)) {
                warningModeActive_ = true;
                gui->warningActive = true;
                LOG(WARNING) << "Warning mode ON:"
                             << " clusters/evt=" << std::fixed << std::setprecision(2) << avgClusters
                             << " (min=" << warningMinClustersPerEvent_ << ")"
                             << " tracks/evt=" << tracksPerEvent
                             << " (min=" << warningMinTracksPerEvent_ << ")";
            }
        }
    } else {
        if(warningModeActive_) {
            warningModeActive_ = false;
            gui->warningActive = false;
            LOG(INFO) << "Warning mode OFF:"
                      << " clusters/evt=" << std::fixed << std::setprecision(2) << avgClusters
                      << " tracks/evt=" << tracksPerEvent;
        }
        warningInitiated_ = false;
    }
}

void OnlineMonitor::fillTimeline(const std::shared_ptr<Clipboard>& clipboard) {
    double ev = static_cast<double>(eventNumber);

    for(auto& td : gui->timelineHitData_) {
        auto& clusters = clipboard->getData<Cluster>(td.detectorName);
        double n = static_cast<double>(clusters.size());
        td.profile->Fill(ev, n);
        warnClusterAccum_[td.detectorName] += static_cast<int>(clusters.size());

        // Telescope: fill cluster hit positions in the z-x and z-y projections
        if(telescopeHitXZ_ && telescopeHitYZ_) {
            for(auto& cluster : clusters) {
                const auto& g = cluster->global();
                telescopeHitXZ_->Fill(g.z(), g.x());
                telescopeHitYZ_->Fill(g.z(), g.y());
            }
        }
    }
    warnEventAccum_++;

    if(profile_tracks_) {
        auto& tracks = clipboard->getData<Track>();
        profile_tracks_->Fill(ev, static_cast<double>(tracks.size()));
        warnTrackAccum_ += static_cast<int>(tracks.size());

        // Telescope: store intercept points of each track at every detector plane
        if(!telescopeDetZ_.empty()) {
            static const Color_t kTrackPalette[GuiDisplay::kNTrackColors] = {
                kRed, kBlue + 1, kGreen + 2, kMagenta, kCyan + 1, kOrange + 7, kViolet + 1, kTeal + 1};
            for(auto& track : tracks) {
                telescopeTrackCount_++;

                // Reset hit histograms and track buffer when the threshold is reached
                if(telescopeResetTracks_ > 0 && telescopeTrackCount_ >= telescopeResetTracks_) {
                    telescopeTrackCount_ = 0;
                    if(telescopeHitXZ_) telescopeHitXZ_->Reset();
                    if(telescopeHitYZ_) telescopeHitYZ_->Reset();
                    gui->telescopeTracks.clear();
                }

                GuiDisplay::TrackView tv;
                tv.color = kTrackPalette[gui->telescopeColorIdx++ % GuiDisplay::kNTrackColors];
                for(double z : telescopeDetZ_) {
                    auto pt = track->getIntercept(z);
                    tv.z.push_back(pt.z());
                    tv.x.push_back(pt.x());
                    tv.y.push_back(pt.y());
                }
                gui->telescopeTracks.push_back(std::move(tv));
                while(gui->telescopeTracks.size() > static_cast<size_t>(GuiDisplay::kTelescopeMaxTracks)) {
                    gui->telescopeTracks.pop_front();
                }
            }
        }
    }

    // After auto-extension the bin count doubles; rebin down to keep ~timelineNBins_ bins
    auto rebinIfNeeded = [this](TProfile* p) {
        if(!p) return;
        int n = p->GetNbinsX();
        if(n > timelineNBins_) p->RebinX(n / timelineNBins_);
    };
    for(auto& td : gui->timelineHitData_) rebinIfNeeded(td.profile);
    rebinIfNeeded(profile_tracks_);
}

void OnlineMonitor::fillCorrelation(const std::shared_ptr<Clipboard>& clipboard) {
    if(gui->correlationTrendData_.empty()) return;

    auto refDet = get_reference();
    if(!refDet) return;

    // Mean global X/Y of clusters on the reference plane for this event
    auto& refClusters = clipboard->getData<Cluster>(refDet->getName());
    if(refClusters.empty()) return;

    double sumXRef = 0.0, sumYRef = 0.0;
    for(auto& c : refClusters) {
        sumXRef += c->global().x();
        sumYRef += c->global().y();
    }
    double meanXRef = sumXRef / static_cast<double>(refClusters.size());
    double meanYRef = sumYRef / static_cast<double>(refClusters.size());

    double ev = static_cast<double>(eventNumber);

    for(auto& cd : gui->correlationTrendData_) {
        auto& clusters = clipboard->getData<Cluster>(cd.detectorName);
        if(clusters.empty()) continue;

        double sumX = 0.0, sumY = 0.0;
        for(auto& c : clusters) {
            sumX += c->global().x();
            sumY += c->global().y();
        }
        double n = static_cast<double>(clusters.size());
        if(cd.profileX) cd.profileX->Fill(ev, sumX / n - meanXRef);
        if(cd.profileY) cd.profileY->Fill(ev, sumY / n - meanYRef);
    }

    // Keep bin count bounded (same rebin logic as fillTimeline)
    auto rebinIfNeeded = [this](TProfile* p) {
        if(!p) return;
        int n = p->GetNbinsX();
        if(n > timelineNBins_) p->RebinX(n / timelineNBins_);
    };
    for(auto& cd : gui->correlationTrendData_) {
        rebinIfNeeded(cd.profileX);
        rebinIfNeeded(cd.profileY);
    }
}

void OnlineMonitor::fillResidualTrend(const std::shared_ptr<Clipboard>& clipboard) {
    if(residualTrendData_.empty()) return;

    auto& tracks = clipboard->getData<Track>();
    double ev = static_cast<double>(eventNumber);

    for(auto& rd : residualTrendData_) {
        for(auto& track : tracks) {
            if(rd.isDut) {
                // DUTs are excluded from the fit, so Track has no residual_local_ entry for them
                // (getLocalResidual() would throw). Compute the same quantity AnalysisDUT does:
                // track intercept vs. the closest DUTAssociation-associated cluster, in local
                // coordinates. No associated cluster this event -> nothing to plot (e.g. genuine
                // DUT inefficiency), not a 0.
                auto closest = track->getClosestCluster(rd.detectorName);
                if(!closest) continue;
                auto detector = get_detector(rd.detectorName);
                auto intercept = detector->getLocalIntercept(track.get());
                rd.profileX->Fill(ev, intercept.X() - closest->local().x());
                rd.profileY->Fill(ev, intercept.Y() - closest->local().y());
            } else {
                // Only detectors actually used in this track's fit have a residual_local_ entry.
                bool in_fit = false;
                for(auto* cluster : track->getClusters()) {
                    if(cluster->detectorID() == rd.detectorName) {
                        in_fit = true;
                        break;
                    }
                }
                if(!in_fit) continue;
                auto localRes = track->getLocalResidual(rd.detectorName);
                rd.profileX->Fill(ev, localRes.x());
                rd.profileY->Fill(ev, localRes.y());
            }
        }
    }

    // Keep bin count bounded (same rebin logic as fillTimeline/fillCorrelation)
    auto rebinIfNeeded = [this](TProfile* p) {
        if(!p) return;
        int n = p->GetNbinsX();
        if(n > timelineNBins_) p->RebinX(n / timelineNBins_);
    };
    for(auto& rd : residualTrendData_) {
        rebinIfNeeded(rd.profileX);
        rebinIfNeeded(rd.profileY);
    }
}

void OnlineMonitor::gui_update() {
    auto now = std::chrono::steady_clock::now();
    double wallElapsed = std::chrono::duration<double>(now - lastUpdateTime_).count();

    // Trigger display update once eventNumber has advanced by at least updateNumber since the last
    // update (a delta check rather than eventNumber % updateNumber == 0: eventCounterMode_ can advance
    // eventNumber by more than one -- or not at all -- per run() call, so an exact multiple could be
    // stepped over). Rate refresh (status bar "Rate: X evt/s") every 5 s regardless, using runCount_
    // (real event rate, independent of eventCounterMode_) rather than eventNumber.
    bool eventCycle = !gui->isPaused() && (eventNumber - lastUpdateEventNumber_ >= updateNumber);
    bool wallClock = (wallElapsed >= 5.0);

    if(eventCycle || wallClock) {
        if(wallElapsed > 0) {
            eventRate_ = static_cast<double>(runCount_ - lastUpdateRunCount_) / wallElapsed;
        }
        lastUpdateTime_ = now;
        lastUpdateEventNumber_ = eventNumber;
        lastUpdateRunCount_ = runCount_;

        if(eventCycle) {
            if(gui->currentCanvas_ == "TelescopeCanvas") {
                gui->DisplayTelescope();
            } else {
                gui->Update();
            }
        }
        gui->updateStatus(eventNumber, eventRate_);
        gui->updateProgress(eventNumber, targetEvents_);
        checkWarningMode();

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
