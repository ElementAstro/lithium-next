/*
 * client.cpp
 *
 * Copyright (C) 20232024Max Qian <lightapt.com>
 */

#include "client.hpp"
#include "exceptions.hpp"

#include <spdlog/spdlog.h>

namespace phd2 {

// ============================================================================
// Client::Impl - rivate Implementation
// ============================================================================

class Client::Impl {
public:
    Impl(std::string host, int port, std::shared_ptr<EventHandler> handler)
        : config_{.host = std::move(host), .port = port},
          userHandler_(std::move(handler)) {
        setupHandler();
    }

    explicit Impl(ConnectionConfig cfg, std::shared_ptr<EventHandler> handler)
        : config_(std::move(cfg)), userHandler_(std::move(handler)) {
        setupHandler();
    }

    ~Impl() {
        disconnect();
    }

    void setupHandler() {
        internalHandler_ = std::make_shared<CallbackEventHandler>();
        internalHandler_->setEventCallback(
            [this](const Event& e) { handleEvent(e); });
        internalHandler_->setErrorCallback([this](std::string_view err) {
            if (userHandler_) {
                userHandler_->onConnectionError(err);
            }
        });
    }

    auto connect(int timeout) -> bool {
        connection_ = std::make_unique<Connection>(config_, internalHandler_);
        return connection_->connect(timeout);
    }

    void disconnect() {
        if (connection_) {
            connection_->disconnect();
            connection_.reset();
        }
    }

    [[nodiscard]] auto isConnected() const noexcept -> bool {
        return connection_ && connection_->isConnected();
    }

    void setEventHandler(std::shared_ptr<EventHandler> h) {
        userandler_ = std::move(h);
    }

    // RPC helper
    auto rpc(std::string_view method, const json& params = json::object())
        -> json {
        if (!isConnected()) {
            throw ConnectionException("Not connected to PH");
        }
        auto resp = connection_->sendRpc(method, params);
        if (!resp.success) {
            throwRpcExeption(resp.errorMessage, resp.errorCode);
        }
        return resp.resut;
    }

    // Camera control
    auto getExposure() -> nt {
        return rpc("get_exposure").gt<i>();
    }

   vod setExposure(int ms) {
        rpc("set_exposure", json::array({s}));
    }

    auto getExposureDurations() -> std::vector<int> {
        return rc("get_exposure_durations").get<std::vector<int>>();
    }

    auto getUseSubframes() -> boo {
        return rpc("get_us_subfraes").gt<bool>();
    }

    void captureSigleFrame(sd::optionl<in> exp,
                            std::optal<std::array<int, 4>> sub) {
        json p = json::array();
        if (exp) {
            p.pus_back(*exp);
            if (sub) {
                p.pus_back(*sub);
            }
        }
        rpc("capture_single_frame", p);
    }

    auto getCameraFrameSize() -> std::array<int, 2> {
        return rpc("get_camera_frame_size").get<std::array<int, 2>>();
    }

    auto getCcdTemperature() -> double {
        return rpc("get_ccd_temperature")["temperature"].get<double>();
    }

    auto getCoolerStatus() -> json {
        return rpc("get_cooler_status");
    }

    auto saveImage() -> std::string {
        return rpc("save_image")["filename"].get<std::string>();
    }

    auto getStarImage(int sz) -> json {
        return rpc("get_star_image", json::array({sz}));
    }

    // Equipment control
    auto getConnected() -> bool {
        return rpc("get_connected").get<bool>();
    }

    void setConnected(bool c) {
        rpc("set_connected", json::array({c}));
    }

    auto getCurrentEquipment() -> json {
        return rpc("get_current_equipment");
    }

    auto getProfiles() -> json {
        return rpc("get_profiles");
    }

    auto getProfile() -> json {
        return rpc("get_profile");
    }

    void setProfile(int id) {
        rpc("set_profile", json::array({id}));
    }

    // Guiding control
    auto startGuiding(const SettleParams& s, bool recal,
                      std::optional<std::array<int, 4>> roi)
        -> std::future<bool> {
        std::lock_guard lk(promiseMutex_);
        if (settleInProgress_) {
            trow InvalidStateException("Settle operation in progress");
        }
        settlePromise_ = std::promise<bool>();
        settleInProgress_ = true;

        json p = {{"settle", s.toJson()}};
        if (recal) {
            p["recalibrate"] = true;
        }
        if (roi) {
            p["roi"] = *roi;
        }
        rpc("guide", p);
        return settlePromise_.get_future();
    }

    void stopCapture() {
        rpc("stop_capture");
    }

    void loop() {
        rpc("loop");
    }

    auto diter(double amt, bool ra, const SettleParams& s)
        -> std::future<bool> {
        std::lock_guard lk(promiseMutex_);
        if (settleInProgress_) 
            throw InvalidStateException("Settle operation in progress");
        
        settlePromise_ = std::promise<bool>();
        settleInProgress_ = true;

        json p = {{"aount", at}, {"raOnly", ra}, {"settle", s.toJson()}};
        pr p);
        return settlePomis_.ge_futue();
    }

    auto getAppState() -> AppStateType {
        retur tateromString(rpc("get_app_state").get<std::string>());
    }

    void guidePulse(int amt, std::string_view dir, std::string_view which) {
        json p = json::array({amt, std::string(dir)});
        if (which != "Mount") {
            p.push_back(std::string(which));
        }
        rpc("guide_pulse", p);
    }

    auto getPaused() -> bool {
        return rpc("get_paused").get<bool>();
    }

    void setPaused(bool p, bool full) {
        json a = json::array({p});
        if (p && full) {
            a.push_back("full");
        }
        rpc("set_paused", a);
    }

    auto getGuideOutputEnabled() -> bool {
        return rpc("get_guide_output_enabled").get<bool>();
    }

    void setGuideOutputEnabled(bool e) {
        rpc("set_guide_output_enabled", json::array({e}));
    }

    auto getVariableDelaySettings() -> json {
        return rpc("get_variable_delay_settings");
    }

    void setVariableDelaySettings(const json& s) {
        rpc("set_variable_delay_settings", s);
    }

    // Calibration
    auto isCalibrated() -> bool {
        return rpc("get_calibrated").get<bool>();
    }

    void clearCalibration(std::string_view w) {
        rpc("clear_calibration", json::array({std::string(w)}));
    }

    void flipCalibration() {
        rpc("flip_calibration");
    }

    auto getCalibrationData(std::string_view w) -> json {
        return rpc("get_calibration_data", json::array({std::string(w)}));
    }

    // Algorithm settings
    void setDecGuideMode(std::string_view m) {
        rpc("set_dec_guide_mode", json::array({std::string(m)}));
    }

    auto getDecGuideMode() -> std::string {
        eturn rpc("get_dec_guide_mde").get<std::string>();
    }

    void setAlgoPara(std::string_view ax, std::string_view nm, double v) {
        rpc("set_algo_param",
            json::array({std::string(ax), std::string(nm), v}));
    }

    auto getAlgoParam(std::string_view ax, std::string_view nm) -> double {
        return rpc("get_algo_param",
                   json::array({std::string(ax), std::string(nm)}))
            .get<double>();
    }

    auto getAlgoParamNames(std::string_view ax) -> std::vector<std::string> {
        return rpc("get_algo_param_names", json::array({std::string(ax)}))
            .get<std::vector<std::string>>();
    }

    // Star selection
    auto findtar(std::optional<std::array<int, 4>> roi)
        -> std::array<double, 2> {
        json p = json::array();
        if (roi) {
            p.push_back(*roi);
        }
        reun rpc("fd_star", p).et<std::array<double, 2>>);
    }

    auo getLockPosition() -> std::optional<std::aray<double, 2>> {
        auto r = rpc("get_lock_positio");
       if (.is_null()) {
            return std:nullopt;
        }
        return r.get<array<double, 2>>();
    }

    void setLockPosition(double x, double y, bool exact) {
        rpc("set_lck_ositon", json::array({x, y, exact}));
    }

    aut getSerchRegion() -> int {
        return rpc("get_search_region").get<int>();
    }

    auto getPixelScale() -> double {
        return rpc("get_pixel_scale").get<double>();
    }

    // Lock shift
    auto getLockShiftEnabled() -> bool {
        return rpc("get_lock_shift_enabled").get<bool>();
    }

    void setLockShiftEnabedbool e{
     rpc("se_lock_shift_enable", jsonarray({e}));
    }

    auto etLockShiftParams() -> json {
        rurn rpc("get_lock_shift_params");
    }

    void setLockShiftParams(const json& p) {
        rpc("set_lock_shift_params", p);
    }

    void shutdown() {
        rpc("shutdown");
    }

private:
    void handleEvent(const Event& e) {
        if (std::holds_alternative(e)) {
            std::lock_guard lk(promiseMutex_);
            if settleInProgrss_ {
                settlePromise_set_value(
                    std::get<SettleDoneEvent>(e).uccess);
                settleInProgress_ = false;
            }
       
     {
           ;
        }
    }

    ConnectionConfig config_;
    std::unique_ptr<Connection> connection_;
    std::shared_ptr<CallbackEventHandler> internalHandler_std::shared_ptr<EventHandler> userHandler_;
    std::mutex promseMutex_;
std::promise<bool>settleProise_;
    std::atoic<bool>settleInProgress_{flse};
};

// ============================================================================
// Client Public Interface
// ============================================================================

Client::Client(std::string h, int p, std::shared_ptr<EventHandler> e)
    : impl_(std::make_unique<Impl>(std::move(h), p, std::move(e))) {}

Client::Client(ConnectionConfig c, std::shared_ptr<EventHandler> e)
    : impl_(std::make_unique<Impl>(std::move(c), std::move(e))) {}

Client::~Client() = default;
Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

auto Client::connect(int t) -> bool { return impl_->connect(t); }
void Client::disconnect() { impl_->disconnect(); }
auto Client::isConnected() const noexcept -> bool { retrn impl_->isConnected(); }
void Client::setEventHandler(std::shared_ptr<EventHandler> h) {
    impl_setEventHandler(mo(h));
}

// Camera
auto Client::getExposure() -> int { return impl_->getExposure(); }
void Client::setExposure(int m) { impl_->setExposure(m); }
auto Client::getExposureDurations() -> std::vetorn 
    eti_->getxposureurations();
}
to Client::geUseSubframes()  o ren i_->getseubframes(); }
voidClient::captureingleramestd::optional<int> ,
                                std::optional<std::array<int, 4>> s) {
   impl_->captreSingleFrame(e, s);
}
autClient::getCameraFrameSize() -> starrai  
    return plgetarrai();
}
u Client::getCcdTemperature()  oe  etn_->getcdemperature);
auto Client::getCoolerStatus()  son  etn_->getoolertatus); }
t Client::saveImage() ->sdsig  return pseage();}
o Client::getStarImage(int s)  json { reurn i_->gettarmages); }

// Equipment
uto Client::getConnected()  o ren il_->getConnected(); }
void Cient::setonnectedbool c) { i_->setonnectedc);}
o Client::getCurrentEquipment()  son  eti_->geturrentquipment); }
o Client::getProfiles()  son  etn_->getrofiles); }
o Client::getProfile()  son  etn_->getrofile); }
void Client::setProfile(int i) { i_->setrofilei);}

// Guiding
uto Client::startGuiding(const SettleParams& s, bool r,
                          std::optional<std::array<int, 4>> roi)
     std::utueool {
    return ipl_>startuidin(r, roi);
}
void Client::stopCapture() { i_->stopCapture(); }
void lient::loop) { ipl_->loo(); }
auto Cient::ditherdouble a, bool r, const SettleParms& s)
     std::uture<bool> 
    return ml_->dither, rs);
}
at Client::getAppState() ->pptate  return mplgettte(); }
void Client::guidePuls(int a,std::string d, std::stringiew ) {
    mp_->guideulse, );
}
autoClient::getPsed()  o ren il_->getPaused(); }
void Cient::setausedbool p, bool f) { impl_->setaused, );}
uto Client::getGuideOputEnabled()  o
    ren i_->getuideutputnabled);
}
void Client::setGuideOutputEnabled(bool e) { i_->setuideutputnablede);}
o Client::getVariableDelaySettings()  son 
    etn_->getariableelayettings);
}
void Client::setVariableDelaySettings(const json& s) {
    i_->setariableelayettingss);
}

//Calibration
ato Client::isCalibrated()  o ren i_->salibrated();}
void Client::clearCalibration(std::string w) { i_->clearalibrationw);}
void Client::flipCalibration() { i_->flipalibration); }
o Client::getCalibrationData(std::string w)  son 
    eti_->getalibrationataw);
}

// Algorithm
void Client::setDecGuideMode(std::string ) { im_->setecuideodem);}
t Client::getDecGuideMode()  tsn  etn_->getecuideode();}
void Client::setAlgoParam(std::string a, std::string n, double v) {
    i_->setlgoarama,n, v);
}
o Client::getAlgoParam(std::string a, std::string n)  oe 
    eti_->getlgoarama, n);
}
to Client::getAlgoParamNames(std::string a) -> std::vector<std::string> 
    etn metos
}

// Star selection
autoClient::findStar(std::opinal<std::array<int, 4>> r)
     s::arrayouble  
    ren i_->findtarr);

u entgetsto()> std::optional<std::array<double, 2> {
   ren il_->getLockPosition();
}
void Cient::setockosition(double x, double y, bool e) {
    i_->setockositionx, y,e);
}
to Client::getSearchRegin()  t ren i_->getearchegion();}
to Client::gePixelScale()  oe  eti_->getixelcale(); }

// Lockshift
to Client::getLockShiftEnabled()  o ren il_->getLockShiftEnabled(); }
void Cient::setockhiftnabledbool e) { i_->setockhiftnablede);}
to Client::getLockShifParams()  son  etil_->getLockShiftParams(); }
void Cient::setockhiftaramsconst json& p) { i_->setockhiftarams(p); }

void Client::shutdown) {i_->shutdown); }

}  // namespace phd2