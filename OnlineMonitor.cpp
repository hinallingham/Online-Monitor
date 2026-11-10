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

    config_.setDefault<int>("display_max_bins_1d", 2000);
    config_.setDefault<int>("display_max_bins_2d", 300);
    config_.setDefault<int>("target_events", 0);
    config_.setDefault<int>("auto_save_interval", 0);
    config_.setDefault<std::string>("auto_save_dir", "./");

    canvasTitle = config_.get<std::string>("canvas_title");
    updateNumber = config_.get<int>("update");
    ignoreAux = config_.get<bool>("ignore_aux");
    clusteringModule = config_.get<std::string>("clustering_module");
    trackingModule = config_.get<std::string>("tracking_module");

    // EventLoaderMALTA builds one Corryvreckan event per l1id(C) and only puts hits carrying that
    // l1id on the clipboard, so "every reference plane has a hit in this event" is the same as "the
    // hits on every reference plane share one l1id". DUTs are exempt, as in EventLoaderMALTA's own
    // coincidence requirement: requiring them would stop the count whenever the DUT is inefficient.
    for(auto& det : get_regular_detectors(false)) {
        coincidencePlanes_.push_back(det->getName());
    }
    if(coincidencePlanes_.empty()) {
        throw ModuleError("Counting coincident l1ids requires at least one non-DUT reference plane");
    }
    LOG(STATUS) << "Event counter: l1ids coincident on all " << coincidencePlanes_.size() << " reference planes";

    targetEvents_ = config_.get<int>("target_events");
    autoSaveInterval_ = config_.get<int>("auto_save_interval");
    autoSaveDir_ = config_.get<std::string>("auto_save_dir");

    config_.setDefaultMatrix<std::string>("overview",
                                          {{trackingModule + "/trackChi2ndof"},
                                           {clusteringModule + "/%REFERENCE%/clusterCharge"},
                                           {"Correlations/%REFERENCE%/hitmap", "colz"},
                                           {trackingModule + "/%REFERENCE%/local_residuals/LocalResidualsX"}});
    // MALTA2 DUT: where it was hit, how well it lines up with the tracks, and how efficient it is
    config_.setDefaultMatrix<std::string>("dut_plots",
                                          {{"Correlations/%DUT%/hitmap", "colz"},
                                           {"OnlineMonitor/dut_residual_x_%DUT%"},
                                           {"OnlineMonitor/dut_residual_y_%DUT%"},
                                           {"AnalysisEfficiency/%DUT%/chipEfficiencyMap_trackPos_TProfile", "colz"}});

    config_.setDefaultMatrix<std::string>("tracking",
                                          {{trackingModule + "/trackChi2"},
                                           {trackingModule + "/trackAngleX"},
                                           {trackingModule + "/trackAngleY"},
                                           {trackingModule + "/trackChi2ndof"},
                                           {trackingModule + "/tracksPerEvent"},
                                           {trackingModule + "/clustersPerTrack"}});

    config_.setDefaultMatrix<std::string>("hitmaps", {{"Correlations/%DETECTOR%/hitmap", "colz"}});
    config_.setDefaultMatrix<std::string>("residuals", {{trackingModule + "/%DETECTOR%/local_residuals/LocalResidualsX"}});
    config_.setDefaultMatrix<std::string>("residuals_y", {{trackingModule + "/%DETECTOR%/local_residuals/LocalResidualsY"}});
    config_.setDefaultMatrix<std::string>("correlation_x", {{"Correlations/%DETECTOR%/correlationX"}});
    config_.setDefaultMatrix<std::string>("correlation_x2d", {{"Correlations/%DETECTOR%/correlationX_2Dlocal", "colz"}});
    config_.setDefaultMatrix<std::string>("correlation_y", {{"Correlations/%DETECTOR%/correlationY"}});
    config_.setDefaultMatrix<std::string>("correlation_y2d", {{"Correlations/%DETECTOR%/correlationY_2Dlocal", "colz"}});
    // MALTA2 is a binary readout: there is no charge, so cluster size takes the place of a charge spectrum
    config_.setDefaultMatrix<std::string>("cluster_size", {{clusteringModule + "/%DETECTOR%/clusterSize"}});
    config_.setDefaultMatrix<std::string>("event_times", {{"Correlations/%DETECTOR%/eventTimes"}});

    // Set up overview plots:
    canvas_overview = config_.getMatrix<std::string>("overview");

    // Set up individual plots for the DUT
    canvas_dutplots = config_.getMatrix<std::string>("dut_plots");
    canvas_tracking = config_.getMatrix<std::string>("tracking");
    canvas_hitmaps = config_.getMatrix<std::string>("hitmaps");
    canvas_residuals = config_.getMatrix<std::string>("residuals");
    canvas_residuals_y = config_.getMatrix<std::string>("residuals_y");

    canvas_cx = config_.getMatrix<std::string>("correlation_x");
    canvas_cx2d = config_.getMatrix<std::string>("correlation_x2d");
    canvas_cy = config_.getMatrix<std::string>("correlation_y");
    canvas_cy2d = config_.getMatrix<std::string>("correlation_y2d");

    canvas_cluster_size = config_.getMatrix<std::string>("cluster_size");
    canvas_time = config_.getMatrix<std::string>("event_times");
}

void OnlineMonitor::initialize() {
    // Residual trend TProfiles and DUT residual distributions must be booked *before* gui_run()
    // below: gui_run() hands the trends to the GUI and builds the DUT canvas immediately, and
    // AddHisto() only finds histograms that already exist in gDirectory at that point.
    {
        static constexpr int kResidualTrendNBins = 200;
        const double residualXMax = static_cast<double>(kResidualTrendNBins * updateNumber);
        for(auto& det : get_detectors()) {
            if(ignoreAux && det->isAuxiliary()) continue;
            if(det->hasRole(DetectorRole::PASSIVE)) continue;

            ResidualTrendData rd;
            rd.detectorName = det->getName();
            rd.isDut = det->isDUT();

            // No main title: the profiles are overlaid, and the legend names the plane
            std::string titleX = ";" + trendAxisLabel_ + ";Local Residual X [mm]";
            rd.profileX = new TProfile(("residual_trend_x_" + det->getName()).c_str(),
                                       titleX.c_str(), kResidualTrendNBins, 0.0, residualXMax);
            rd.profileX->SetCanExtend(TH1::kAllAxes);
            rd.profileX->SetStats(false); // mean/RMS of the event number says nothing

            std::string titleY = ";" + trendAxisLabel_ + ";Local Residual Y [mm]";
            rd.profileY = new TProfile(("residual_trend_y_" + det->getName()).c_str(),
                                       titleY.c_str(), kResidualTrendNBins, 0.0, residualXMax);
            rd.profileY->SetCanExtend(TH1::kAllAxes);
            rd.profileY->SetStats(false);

            if(rd.isDut) {
                // Same binning as Tracking4D's LocalResidualsX/Y, so the DUT reads like the other planes
                const double rx = 3 * det->getPitch().X();
                const double ry = 3 * det->getPitch().Y();
                rd.distX = new TH1F(("dut_residual_x_" + det->getName()).c_str(),
                                    (det->getName() + " Local Residual X;x_{track}-x_{hit} [mm];events").c_str(),
                                    500, -rx, rx);
                rd.distY = new TH1F(("dut_residual_y_" + det->getName()).c_str(),
                                    (det->getName() + " Local Residual Y;y_{track}-y_{hit} [mm];events").c_str(),
                                    500, -ry, ry);
            }

            residualTrendData_.push_back(rd);
        }
    }

    gui_run();

    // Timeline: 200 bins, each bin = one update interval
    timelineNBins_ = 200;
    const double xMax = static_cast<double>(timelineNBins_ * updateNumber);

    for(auto& det : get_detectors()) {
        if(ignoreAux && det->isAuxiliary()) continue;
        if(det->hasRole(DetectorRole::PASSIVE)) continue;

        // X-axis is every l1id (runCount_), so a plane that stops shows as its own line dropping
        // to 0 while the others carry on; "Clusters / Event" averages over the same l1ids.
        auto* prof = new TProfile(("hits_tl_" + det->getName()).c_str(),
                                  (";" + trendAxisLabel_ + ";Clusters / Event").c_str(),
                                  timelineNBins_, 0.0, xMax);
        prof->SetCanExtend(TH1::kAllAxes);
        GuiDisplay::TimelineData td;
        td.detectorName = det->getName();
        td.profile = prof;
        td.color = gui->detectorColors_[det->getName()];
        gui->timelineHitData_.push_back(td);
    }

    profile_tracks_ = new TProfile(
        "tracks_tl", (";" + trendAxisLabel_ + ";Tracks / Event").c_str(), timelineNBins_, 0.0, xMax);
    profile_tracks_->SetCanExtend(TH1::kAllAxes);
    gui->timelineTrackProfile_ = profile_tracks_;

    // Correlation trend profiles: mean(global_pos_i - global_pos_ref) vs event number
    auto refDet = get_reference();
    if(refDet) {
        for(auto& det : get_detectors()) {
            if(ignoreAux && det->isAuxiliary()) continue;
            if(det->hasRole(DetectorRole::PASSIVE)) continue;
            if(det->getName() == refDet->getName()) continue;

            GuiDisplay::TrendData cd;
            cd.detectorName = det->getName();
            cd.color = gui->detectorColors_[det->getName()];

            std::string titleX =
                ";" + trendAxisLabel_ + ";#Delta X_{global} [mm] (" + det->getName() + " - " + refDet->getName() + ")";
            cd.profileX = new TProfile(("corr_x_tl_" + det->getName()).c_str(),
                                       titleX.c_str(), timelineNBins_, 0.0, xMax);
            cd.profileX->SetCanExtend(TH1::kAllAxes);

            std::string titleY =
                ";" + trendAxisLabel_ + ";#Delta Y_{global} [mm] (" + det->getName() + " - " + refDet->getName() + ")";
            cd.profileY = new TProfile(("corr_y_tl_" + det->getName()).c_str(),
                                       titleY.c_str(), timelineNBins_, 0.0, xMax);
            cd.profileY->SetCanExtend(TH1::kAllAxes);

            gui->correlationTrendData_.push_back(cd);
        }
    }

    gui->saveDir_ = autoSaveDir_;
    gui->maxDisplayBins1D_ = config_.get<int>("display_max_bins_1d");
    gui->maxDisplayBins2D_ = config_.get<int>("display_max_bins_2d");
    lastAutoSaveTime_ = std::chrono::steady_clock::now();
}

StatusCode OnlineMonitor::run(const std::shared_ptr<Clipboard>& clipboard) {
    fillTimeline(clipboard);
    fillCorrelation(clipboard);
    fillResidualTrend(clipboard);

    // runCount_ (one per run() call) stays the denominator for all rate-style metrics computed in
    // gui_update() below; eventNumber (displayed Events count) only advances when this event's
    // l1id coincides on every reference plane.
    runCount_++;
    if(isCoincidentEvent(clipboard)) {
        eventNumber++;
    }

    gui_update();
    return StatusCode::Success;
}

bool OnlineMonitor::isCoincidentEvent(const std::shared_ptr<Clipboard>& clipboard) const {
    return std::all_of(coincidencePlanes_.begin(), coincidencePlanes_.end(), [&](const std::string& name) {
        return !clipboard->getData<Pixel>(name).empty();
    });
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

    // dynamic_cast, not static_cast: a path can legitimately point at something that is not a
    // TH1 (AnalysisEfficiency's TEfficiency objects, for one), and casting those blindly hands
    // the GUI a bogus pointer that only shows up as a crash much later.
    TObject* object = gDirectory->Get(histoName.c_str());
    TH1* histogram = dynamic_cast<TH1*>(object);
    if(object != nullptr && histogram == nullptr) {
        LOG(WARNING) << "Object " << histoName << " is a " << object->ClassName()
                     << ", not a histogram -- cannot be shown on a canvas";
        return;
    }
    if(histogram) {
        // Allow 1D histograms to extend their x-axis when data exceeds the initial range
        if(!dynamic_cast<TH2*>(histogram)) {
            histogram->SetCanExtend(TH1::kAllAxes);
        }
        gui->histograms[canvasName].push_back(histogram);
        gui->logarithmic[gui->histograms[canvasName].back()] = logy;
        gui->styles[gui->histograms[canvasName].back()] = style;

        // Tag the histogram with its plane, so the GUI can draw it in that plane's
        // colour. A path matching no detector is a telescope-wide quantity
        // (trackChi2, tracksPerEvent, ...) and gets the accent colour instead.
        for(auto& detector : get_detectors()) {
            if(histoName.find(detector->getName()) != std::string::npos) {
                gui->histoDetector_[histogram] = detector->getName();
                break;
            }
        }
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

    // Assign each plane its colour before any plot is registered below, so AddHisto
    // can tag histograms with it. Every canvas draws from this one map.
    size_t detColorIdx = 0;
    for(auto& det : get_detectors()) {
        if(ignoreAux && det->isAuxiliary()) {
            continue;
        }
        if(det->hasRole(DetectorRole::PASSIVE)) {
            continue;
        }
        gui->detectorColors_[det->getName()] = GuiDisplay::PaletteColor(detColorIdx++);
    }

    // Make the main window object and set the attributes
    gui->buttonMenu = new TGHorizontalFrame(gui, 1200, 50);
    gui->canvas = new TRootEmbeddedCanvas("canvas", gui, 1200, 600);
    gui->AddFrame(gui->canvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 10, 10, 10, 10));
    gui->SetCleanup(kDeepCleanup);
    gui->DontCallClose();

    // Add canvases and histograms. canvasOrder_ follows the button order, so the digit keys
    // (1-9) reach the first nine canvases as they appear on screen.
    AddCanvasGroup("Tracking");
    AddCanvas("Overview", "Tracking", canvas_overview);
    gui->canvasOrder_.push_back("OverviewCanvas");
    AddCanvas("Tracking Performance", "Tracking", canvas_tracking);
    gui->canvasOrder_.push_back("Tracking PerformanceCanvas");
    AddCanvas("Residuals X", "Tracking", canvas_residuals, true);
    gui->canvasOrder_.push_back("Residuals XCanvas");
    AddCanvas("Residuals Y", "Tracking", canvas_residuals_y, true);
    gui->canvasOrder_.push_back("Residuals YCanvas");
    // Bare lines on the residual canvases: error bars and a marker per bin cost ~10x the redraw
    // time on these many-pad canvases. Plots given an explicit draw option keep it.
    for(const auto* canvasName : {"Residuals XCanvas", "Residuals YCanvas"}) {
        for(auto* h : gui->histograms[canvasName]) {
            if(gui->styles[h].empty()) {
                gui->styles[h] = GuiDisplay::LineStyle();
            }
        }
    }

    // Residual-vs-event trend: OnlineMonitor's own live TProfiles (residualTrendData_, filled in
    // fillResidualTrend()), every plane including DUTs overlaid on one canvas like Corr Trends --
    // X on top, Y below -- so the planes can be compared at a glance.
    for(auto& rd : residualTrendData_) {
        GuiDisplay::TrendData td;
        td.detectorName = rd.detectorName;
        td.profileX = rd.profileX;
        td.profileY = rd.profileY;
        td.color = gui->detectorColors_[rd.detectorName];
        gui->residualTrendData_.push_back(td);
    }
    gui->buttons["ResTrends"] = new TGTextButton(gui->buttonGroups["Tracking"], "Res Trends");
    gui->buttonGroups["Tracking"]->AddFrame(gui->buttons["ResTrends"],
                                            new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 0, 0));
    gui->buttons["ResTrends"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "DisplayResidualTrend()");
    gui->canvasOrder_.push_back("ResidualTrendCanvas");

    gui->buttons["Timeline"] = new TGTextButton(gui->buttonGroups["Tracking"], "Timeline");
    gui->buttonGroups["Tracking"]->AddFrame(gui->buttons["Timeline"],
                                            new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 0, 0));
    gui->buttons["Timeline"]->Connect("Pressed()", "corryvreckan::GuiDisplay", gui, "DisplayTimeline()");
    gui->canvasOrder_.push_back("TimelineCanvas");

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
    AddCanvas("Cluster Size", "Detectors", canvas_cluster_size);
    gui->canvasOrder_.push_back("Cluster SizeCanvas");
    // Cluster sizes 1-8 only: the histogram runs to 99, and MALTA2 clusters are rarely above a few pixels
    for(auto* h : gui->histograms["Cluster SizeCanvas"]) {
        gui->fixedXRange_[h] = {0.5, 8.5};
    }

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

    // Full Screen button. Pale enough that the black label stays readable -- and via
    // GetColorByName, since the previous steel blue was a colour index cast to Pixel_t,
    // which is a different thing entirely and only happened to come out blue.
    gClient->GetColorByName("#a8c8e8", color);
    gui->buttons["fullscreen"] = new TGTextButton(gui->buttonGroups["Controls"], "&Full Screen");
    gui->buttons["fullscreen"]->ChangeBackground(color);
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
    Int_t statusParts[] = {45, 35, 20};
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

    // Keyboard shortcuts: bind keys and store keycode->action map
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

    // Open on the Overview
    gui->Display(const_cast<char*>("OverviewCanvas"));

    gui->canvas->GetCanvas()->Paint();
    gui->canvas->GetCanvas()->Update();
    gSystem->ProcessEvents();

    // Initialise member variables
    eventNumber = 0;
    runCount_ = 0;
    lastUpdateTime_ = std::chrono::steady_clock::now();
    lastUpdateRunCount_ = 0;
    lastRedrawRunCount_ = 0;
    eventRate_ = 0.0;
    lastProcessEvents_ = std::chrono::steady_clock::now();
    lastRedrawTime_ = lastProcessEvents_;
}

void OnlineMonitor::fillTimeline(const std::shared_ptr<Clipboard>& clipboard) {
    double ev = static_cast<double>(runCount_); // every l1id, see runCount_

    for(auto& td : gui->timelineHitData_) {
        td.profile->Fill(ev, static_cast<double>(clipboard->getData<Cluster>(td.detectorName).size()));
    }

    if(profile_tracks_) {
        profile_tracks_->Fill(ev, static_cast<double>(clipboard->getData<Track>().size()));
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

    double ev = static_cast<double>(runCount_); // every l1id, see runCount_

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
    double ev = static_cast<double>(runCount_); // every l1id, see runCount_

    for(auto& rd : residualTrendData_) {
        for(auto& track : tracks) {
            if(rd.isDut) {
                // DUTs are excluded from the fit, so Track has no residual_local_ entry for them
                // (getLocalResidual() would throw). Compute the same quantity AnalysisDUT does:
                // track intercept vs. the closest DUTAssociation-associated cluster, in local
                // coordinates. No associated cluster this event -> nothing to plot (e.g. genuine
                // DUT inefficiency), not a 0. getClosestCluster() throws rather than returning
                // null for a track that has none, so this has to be guarded up front.
                if(!track->hasClosestCluster(rd.detectorName)) {
                    continue;
                }
                auto closest = track->getClosestCluster(rd.detectorName);
                auto detector = get_detector(rd.detectorName);
                auto intercept = detector->getLocalIntercept(track.get());
                const double resX = intercept.X() - closest->local().x();
                const double resY = intercept.Y() - closest->local().y();
                rd.profileX->Fill(ev, resX);
                rd.profileY->Fill(ev, resY);
                rd.distX->Fill(resX);
                rd.distY->Fill(resY);
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

    // Redraw once updateNumber l1ids have been processed since the last redraw -- every l1id, not
    // only coincident ones, so the screen keeps moving when a plane stops -- but at most once a
    // second, so the drawing cannot outgrow the reconstruction as the rate rises. Status bar
    // (Events count, rate) refreshes every 5 s regardless.
    bool eventCycle = !gui->isPaused() && (runCount_ - lastRedrawRunCount_ >= updateNumber) &&
                      (now - lastRedrawTime_ >= std::chrono::seconds(1));
    bool wallClock = (wallElapsed >= 5.0);

    bool redrawn = false;
    if(eventCycle || wallClock) {
        redrawn = true;
        if(wallElapsed > 0) {
            eventRate_ = static_cast<double>(runCount_ - lastUpdateRunCount_) / wallElapsed;
        }
        lastUpdateTime_ = now;
        lastUpdateRunCount_ = runCount_;

        gui->updateStatus(eventNumber, eventRate_);
        gui->updateProgress(eventNumber, targetEvents_);
        if(eventCycle) {
            gui->Update();
            lastRedrawTime_ = now;
            lastRedrawRunCount_ = runCount_;
        }

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

    // Hand control to the GUI (button presses, window exposes, flushing the drawing above to the
    // X server) at most every 20 ms, and right after a redraw. Doing it on every run() call meant
    // a round of X traffic per event -- thousands per second, all through the SSH tunnel.
    auto afterDraw = std::chrono::steady_clock::now();
    if(redrawn || afterDraw - lastProcessEvents_ >= std::chrono::milliseconds(20)) {
        gSystem->ProcessEvents();
        lastProcessEvents_ = afterDraw;
    }
}
