#include "orip/parser.h"
#include "orip/session_manager.h"
#include "orip/astm_f3411.h"
#include "orip/wifi_decoder.h"
#include "orip/asd_stan.h"
#include "orip/cn_rid.h"

namespace orip {

struct RemoteIDParser::Impl {
    ParserConfig config;
    SessionManager session_manager;
    astm::ASTM_F3411_Decoder astm_decoder;
    wifi::WiFiDecoder wifi_decoder;
    asdstan::ASD_STAN_Decoder asd_decoder;
    cnrid::CN_RID_Decoder cn_decoder;

    explicit Impl(const ParserConfig& cfg)
        : config(cfg)
        , session_manager(cfg.uav_timeout_ms) {}
};

RemoteIDParser::RemoteIDParser()
    : RemoteIDParser(ParserConfig{}) {}

RemoteIDParser::RemoteIDParser(const ParserConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

RemoteIDParser::~RemoteIDParser() = default;

RemoteIDParser::RemoteIDParser(RemoteIDParser&&) noexcept = default;
RemoteIDParser& RemoteIDParser::operator=(RemoteIDParser&&) noexcept = default;

void RemoteIDParser::init() {
    // Currently no initialization required
    // Reserved for future use (e.g., loading lookup tables)
}

ParseResult RemoteIDParser::parse(const RawFrame& frame) {
    ParseResult result;
    result.success = false;
    result.is_remote_id = false;

    if (frame.payload.empty()) {
        result.error = "Empty payload";
        return result;
    }

    // Try ASTM F3411 decoder for Bluetooth transports
    if (impl_->config.enable_astm) {
        if (impl_->astm_decoder.isRemoteID(frame.payload)) {
            result.is_remote_id = true;

            UAVObject uav;
            uav.transport = frame.transport;
            uav.rssi = frame.rssi;
            uav.last_seen = frame.timestamp;

            auto decode_result = impl_->astm_decoder.decode(frame.payload, uav);

            if (decode_result.success) {
                result.success = true;
                result.protocol = ProtocolType::ASTM_F3411;
                result.uav = uav;

                // Update session manager if deduplication is enabled
                if (impl_->config.enable_deduplication && !uav.id.empty()) {
                    impl_->session_manager.update(uav);
                }
            } else {
                result.error = decode_result.error;
            }

            return result;
        }
    }

    // Try WiFi decoder for WiFi transports
    if (impl_->config.enable_astm) {
        if (impl_->wifi_decoder.isRemoteID(frame.payload)) {
            result.is_remote_id = true;

            UAVObject uav;
            uav.transport = frame.transport;
            uav.rssi = frame.rssi;
            uav.last_seen = frame.timestamp;

            wifi::WiFiDecodeResult decode_result;

            // Try appropriate WiFi decode method based on transport hint
            if (frame.transport == TransportType::WIFI_NAN) {
                decode_result = impl_->wifi_decoder.decodeNAN(frame.payload, uav);
            } else {
                // Try Beacon first, then NAN
                decode_result = impl_->wifi_decoder.decodeBeacon(frame.payload, uav);
                if (!decode_result.success) {
                    decode_result = impl_->wifi_decoder.decodeNAN(frame.payload, uav);
                }
            }

            if (decode_result.success) {
                result.success = true;
                result.protocol = uav.protocol;
                result.uav = uav;

                if (impl_->config.enable_deduplication && !uav.id.empty()) {
                    impl_->session_manager.update(uav);
                }
            } else {
                result.error = decode_result.error;
            }

            return result;
        }
    }

    // Try ASD-STAN decoder (EU standard)
    // Note: ASD-STAN uses the same format as ASTM, so check after ASTM
    if (impl_->config.enable_asd) {
        if (impl_->asd_decoder.isRemoteID(frame.payload)) {
            result.is_remote_id = true;

            UAVObject uav;
            uav.transport = frame.transport;
            uav.rssi = frame.rssi;
            uav.last_seen = frame.timestamp;

            auto decode_result = impl_->asd_decoder.decode(frame.payload, uav);

            if (decode_result.success) {
                result.success = true;
                result.protocol = ProtocolType::ASD_STAN;
                result.uav = uav;

                if (impl_->config.enable_deduplication && !uav.id.empty()) {
                    impl_->session_manager.update(uav);
                }
            } else {
                result.error = decode_result.error;
            }

            return result;
        }
    }

    // Try CN-RID decoder (Chinese standard)
    if (impl_->config.enable_cn) {
        if (impl_->cn_decoder.isRemoteID(frame.payload)) {
            result.is_remote_id = true;

            UAVObject uav;
            uav.transport = frame.transport;
            uav.rssi = frame.rssi;
            uav.last_seen = frame.timestamp;

            auto decode_result = impl_->cn_decoder.decode(frame.payload, uav);

            if (decode_result.success) {
                result.success = true;
                result.protocol = ProtocolType::CN_RID;
                result.uav = uav;

                if (impl_->config.enable_deduplication && !uav.id.empty()) {
                    impl_->session_manager.update(uav);
                }
            } else {
                result.error = decode_result.error;
            }

            return result;
        }
    }

    result.error = "No matching protocol decoder";
    return result;
}

ParseResult RemoteIDParser::parse(const std::vector<uint8_t>& payload, int8_t rssi,
                                   TransportType transport) {
    RawFrame frame;
    frame.payload = payload;
    frame.rssi = rssi;
    frame.transport = transport;
    frame.timestamp = std::chrono::steady_clock::now();

    return parse(frame);
}

std::vector<UAVObject> RemoteIDParser::getActiveUAVs() const {
    return impl_->session_manager.getActiveUAVs();
}

const UAVObject* RemoteIDParser::getUAV(const std::string& id) const {
    return impl_->session_manager.getUAV(id);
}

size_t RemoteIDParser::getActiveCount() const {
    return impl_->session_manager.count();
}

void RemoteIDParser::clear() {
    impl_->session_manager.clear();
}

void RemoteIDParser::setOnNewUAV(UAVCallback callback) {
    impl_->session_manager.setOnNewUAV(std::move(callback));
}

void RemoteIDParser::setOnUAVUpdate(UAVCallback callback) {
    impl_->session_manager.setOnUAVUpdate(std::move(callback));
}

void RemoteIDParser::setOnUAVTimeout(UAVCallback callback) {
    impl_->session_manager.setOnUAVTimeout(std::move(callback));
}

void RemoteIDParser::cleanup() {
    impl_->session_manager.cleanup();
}

} // namespace orip
