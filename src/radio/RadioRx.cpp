#include <csics/radio/RadioRx.hpp>

#ifdef CSICS_USE_UHD
#include "usrp/USRPRadioRx.hpp"
#endif

namespace csics::radio {

#ifdef CSICS_USE_UHD
inline bool find_usrp() { 
    uhd_string_vector_handle sv;
    uhd_string_vector_make(&sv);
    auto devs = uhd_usrp_find("", &sv);
    size_t size = 0;
    uhd_string_vector_size(sv, &size);
    return size != 0;
}

inline std::unique_ptr<USRPRadioRx> create_usrp(const RadioDeviceArgs& dv,
                                                const RadioConfiguration& cfg) {
    auto pRadio = std::make_unique<USRPRadioRx>(dv);
    pRadio->set_configuration(cfg);
    return pRadio;
}
#endif

std::unique_ptr<IRadioRx> IRadioRx::create_radio_rx(
    const RadioDeviceArgs& device_args, const RadioConfiguration& config) {
    switch (device_args.device_type) {
#ifdef CSICS_USE_UHD
        case DeviceType::USRP:
            return create_usrp(device_args, config);
#endif
        case DeviceType::DEFAULT:
        default: {
#ifdef CSICS_USE_UHD
            if (find_usrp()) {
                return create_usrp(device_args, config);
            }
#endif
            return nullptr;
        }
    }
}
};  // namespace csics::radio
