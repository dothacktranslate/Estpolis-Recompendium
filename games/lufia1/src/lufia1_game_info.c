#include <stddef.h>

#include "common_cpu_infra.h"
#include "lufia1_runtime.h"

const RtlGameInfo kLufia1GameInfo = {
    .title = "Lufia I",

    .initialize = Lufia1RuntimeInitialize,
    .run_frame = Lufia1RunOneFrame,
    .draw_ppu_frame = Lufia1DrawPpuFrame,

    .enhanced_render_frame = NULL,

    .save_name_prefix = "lufia1",

    .state_save_extra = NULL,
    .state_load_extra = NULL,
    .on_state_loaded = NULL,

    .session_reset = NULL,

    .tier2_capture = 0,
};
