#include <stddef.h>

#include "common_cpu_infra.h"
#include "estpolis1_runtime.h"

const RtlGameInfo kEstpolis1GameInfo = {
    .title = "Estpolis Denki I",

    .initialize = Estpolis1RuntimeInitialize,
    .run_frame = Estpolis1RunOneFrame,
    .draw_ppu_frame = Estpolis1DrawPpuFrame,

    .enhanced_render_frame = NULL,

    .save_name_prefix = "estpolis1",

    .state_save_extra = NULL,
    .state_load_extra = NULL,
    .on_state_loaded = NULL,

    .session_reset = NULL,

    .tier2_capture = 0,
};
