// Only built in the SystemView trace build (sdkconfig.trace_uart, see main/CMakeLists.txt).
//
// At boot ESP-IDF calls esp_trace_get_user_params() to find out which trace
// encoder and transport to open. Its default (a weak function in esp_trace)
// asks for the encoder "ext", but the esp_sysview component registers itself
// as "sysview", so the default fails at boot with
//   "Encoder 'ext' or transport 'apptrace' not found" -> abort().
// Overriding it here is what ESP-IDF's own sysview_tracing example does.
#include "esp_trace.h"

esp_trace_open_params_t esp_trace_get_user_params(void)
{
    const esp_trace_open_params_t trace_params = {
        .core_cfg = NULL,
        .encoder_name = "sysview",    // registered by espressif/esp_sysview
        .encoder_cfg = NULL,          // default SystemView settings
        .transport_name = "apptrace", // ESP-IDF app trace: JTAG (CONFIG_APPTRACE_DEST_JTAG)
        .transport_cfg = NULL,        // default apptrace settings from menuconfig
    };
    return trace_params;
}
