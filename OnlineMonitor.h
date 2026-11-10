/**
 * @file
 * @brief Definition of module OnlineMonitor
 *
 * @copyright Copyright (c) 2017-2024 CERN and the Corryvreckan authors.
 * This software is distributed under the terms of the MIT License, copied verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 * SPDX-License-Identifier: MIT
 */

#ifndef OnlineMonitor_H
#define OnlineMonitor_H 1

#include <RQ_OBJECT.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TGCanvas.h>
#include <TGDockableFrame.h>
#include <TGFrame.h>
#include <TGMenu.h>
#include <TGTextEntry.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TROOT.h>
#include <TRootEmbeddedCanvas.h>
#include <TSystem.h>
#include <TProfile.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "GuiDisplay.hpp"
#include "core/module/Module.hpp"
#include "objects/Cluster.hpp"
#include "objects/Pixel.hpp"
#include "objects/Track.hpp"

namespace corryvreckan {

    /** @ingroup Modules
     */
    class OnlineMonitor : public Module {

    public:
        // Constructors and destructors
        OnlineMonitor(Configuration& config, std::vector<std::shared_ptr<Detector>> detectors);
        ~OnlineMonitor() {}

        // Functions
        void initialize() override;
        StatusCode run(const std::shared_ptr<Clipboard>& clipboard) override;

        // Application to allow display persistancy
        TApplication* app = nullptr;
        GuiDisplay* gui = nullptr;

    private:
        void AddCanvasGroup(std::string group_title);
        void AddCanvas(std::string canvas_title,
                       std::string canvasGroup,
                       Matrix<std::string> canvas_plots,
                       bool ignoreDut = false,
                       std::string detector_name = "");
        // Function specifically for adding "DUT" ButtonGroup to enable scroll bar for large number of DUTs
        void AddDUTGroup(uint64_t num_planes);
        void AddPlots(std::string canvas_name,
                      Matrix<std::string> canvas_plots,
                      bool ignoreDut = false,
                      std::string detector_name = "");
        void AddHisto(std::string, std::string, std::string style = "", bool logy = false);

        // Whether the event just processed counts towards eventNumber: its l1id coincides on every
        // plane in coincidencePlanes_
        bool isCoincidentEvent(const std::shared_ptr<Clipboard>& clipboard) const;

        // Member variables
        int eventNumber;
        int updateNumber;
        bool ignoreAux;

        std::string canvasTitle;

        std::string clusteringModule;
        std::string trackingModule;

        // eventNumber (displayed Events count, progress bar, target_events) counts only l1ids whose
        // hits coincide on every plane listed here: the regular (non-DUT, non-auxiliary,
        // non-passive) reference planes -- how much usable statistics the run has collected.
        std::vector<std::string> coincidencePlanes_;

        // runCount_ counts every l1id (one per run() call), coincident or not. It drives the trend
        // X-axes, the redraw cadence and the status bar rate: when a plane stops, eventNumber stops
        // too, and anything tied to it would freeze exactly when the screen should show which
        // plane dropped out.
        int runCount_ = 0;
        int lastUpdateRunCount_ = 0;
        int lastRedrawRunCount_ = 0;
        const std::string trendAxisLabel_ = "l1id"; // X-axis label of the trends (runCount_)

        // Canvases and their plots:
        Matrix<std::string> canvas_dutplots, canvas_overview, canvas_tracking, canvas_hitmaps, canvas_residuals,
            canvas_residuals_y, canvas_cx, canvas_cy, canvas_cx2d, canvas_cy2d, canvas_cluster_size, canvas_time;

        // Event rate tracking
        std::chrono::steady_clock::time_point lastUpdateTime_;
        double eventRate_;

        // Timeline profiles (filled in run())
        TProfile* profile_tracks_ = nullptr;
        int timelineNBins_;

        // Residual-vs-event trend (Res Trends tab): one X/Y TProfile pair per detector, including
        // DUTs (unlike the config-driven Residuals canvases, which exclude them). Booked in
        // initialize() *before* gui_run(), since gui_run() hands them to the GUI and the DUT
        // canvas finds the DUT distributions below through AddHisto()/gDirectory. Non-DUT residuals come from Track::getLocalResidual() (same quantity
        // Tracking4D itself fills into LocalResidualsX/Y); DUT residuals are computed the same way
        // AnalysisDUT does, from the closest DUTAssociation-associated cluster.
        struct ResidualTrendData {
            std::string detectorName;
            bool isDut = false;
            TProfile* profileX = nullptr;
            TProfile* profileY = nullptr;
            // DUTs only: the residual distributions for the DUT canvas, filled from the same values
            // as the trend. Done here rather than read from AnalysisDUT, which books ~4 GB of
            // histograms the monitor never shows.
            TH1F* distX = nullptr;
            TH1F* distY = nullptr;
        };
        std::vector<ResidualTrendData> residualTrendData_;
        void fillResidualTrend(const std::shared_ptr<Clipboard>& clipboard);

        // GUI responsiveness: see gui_update()
        std::chrono::steady_clock::time_point lastProcessEvents_;
        std::chrono::steady_clock::time_point lastRedrawTime_;

        // Progress bar
        int targetEvents_;

        // Auto-save
        int autoSaveInterval_;
        std::string autoSaveDir_;
        std::chrono::steady_clock::time_point lastAutoSaveTime_;

        // Additional methods
        void gui_run();
        void gui_update();
        void fillTimeline(const std::shared_ptr<Clipboard>& clipboard);
        void fillCorrelation(const std::shared_ptr<Clipboard>& clipboard);
    };
} // namespace corryvreckan
#endif // OnlineMonitor_H
