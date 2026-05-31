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
#include <chrono>
#include <iostream>

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

        // Member variables
        int eventNumber;
        int updateNumber;
        bool ignoreAux;

        std::string canvasTitle;

        std::string clusteringModule;
        std::string trackingModule;

        // Canvases and their plots:
        Matrix<std::string> canvas_dutplots, canvas_overview, canvas_tracking, canvas_hitmaps, canvas_residuals, canvas_cx,
            canvas_cy, canvas_cx2d, canvas_cy2d, canvas_charge, canvas_time;

        // Event rate tracking
        std::chrono::steady_clock::time_point lastUpdateTime_;
        int lastUpdateEventNumber_;
        double eventRate_;

        // Timeline profiles (filled in run())
        TProfile* profile_tracks_ = nullptr;
        int timelineNBins_;

        // Telescope view histograms and detector z positions
        TH2F* telescopeHitXZ_ = nullptr;
        TH2F* telescopeHitYZ_ = nullptr;
        std::vector<double> telescopeDetZ_;
        int telescopeResetTracks_ = 0;
        int telescopeTrackCount_ = 0;

        // Progress bar
        int targetEvents_;

        // Auto-save
        int autoSaveInterval_;
        std::string autoSaveDir_;
        std::chrono::steady_clock::time_point lastAutoSaveTime_;

        // Discord alert — trigger rate
        std::string discordWebhook_;
        double discordMinRate_;
        int discordAlertDuration_;
        bool discordAlertActive_ = false;
        bool discordNotificationSent_ = false;
        std::chrono::steady_clock::time_point discordBelowSince_;

        // Discord alert — per-plane hit rate
        double discordMaxHitRate_;
        double discordMinHitRate_;
        struct PlaneAlertState {
            bool active = false;
            bool sent = false;
            std::chrono::steady_clock::time_point since;
            bool isHighRate = false;
        };
        std::map<std::string, PlaneAlertState> planeAlerts_;
        std::map<std::string, int> planeHitAccum_;
        std::map<std::string, int> planeEventAccum_;
        std::map<std::string, double> planeHitRate_;

        // Warning mode — low cluster/event or track/event
        double warningMinClustersPerEvent_;
        double warningMinTracksPerEvent_;
        int warningDuration_;
        bool warningModeActive_ = false;
        bool warningInitiated_ = false;
        std::chrono::steady_clock::time_point warningBelowSince_;
        std::map<std::string, int> warnClusterAccum_;
        int warnTrackAccum_ = 0;
        int warnEventAccum_ = 0;

        // Additional methods
        void gui_run();
        void gui_update();
        void fillTimeline(const std::shared_ptr<Clipboard>& clipboard);
        void fillCorrelation(const std::shared_ptr<Clipboard>& clipboard);
        void checkDiscordAlert();
        void sendDiscordAlert();
        void sendDiscordRecovery();
        void checkPlaneAlerts();
        void sendPlaneAlert(const std::string& detName, double rate, bool isHigh);
        void sendPlaneRecovery(const std::string& detName, double rate);
        void sendDiscordPayload(const std::string& jsonPath);
        void checkWarningMode();

        // InfluxDB
        std::string influxdbUrl_;
        std::string influxdbToken_;
        std::string influxdbOrg_;
        std::string influxdbBucket_;
        std::map<std::string, int> influxHitAccum_;
        int influxTrackAccum_ = 0;
        int influxEventAccum_ = 0;
        std::chrono::steady_clock::time_point runStartTime_;
        void sendInfluxDB();
    };
} // namespace corryvreckan
#endif // OnlineMonitor_H
