#include <csics/radio/RadioRx.hpp>

#ifdef CSICS_USE_UHD
#include <csics/radio/usrp/USRPRadioRx.hpp>
#endif

namespace csics::radio {

#ifdef CSICS_USE_UHD
inline bool find_usrp() {
    auto devs = uhd::device::find("");
    return !devs.empty();
}
#endif

std::unique_ptr<IRadioRx> IRadioRx::create_radio_rx(
    const RadioDeviceArgs& device_args, const RadioConfiguration& config) {
    switch (device_args.device_type) {
#ifdef CSICS_USE_UHD
        case DeviceType::USRP:
            return std::make_unique<USRPRadioRx>(device_args);
#endif
        case DeviceType::DEFAULT:
        default: {
#ifdef CSICS_USE_UHD
            if (find_usrp()) {
                auto pRadio = std::make_unique<USRPRadioRx>(device_args);
                pRadio->set_configuration(config);
                return pRadio;
            }
#endif
            return nullptr;
        }
    }
}
};  // namespace csics::radio
