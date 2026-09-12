#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lufia1_runtime.h"

#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "cpu_state.h"

#include "snes/dma.h"
#include "snes/interp_bridge.h"
#include "snes/ppu.h"
#include "snes/snes.h"

/*
 * Lufia & the Fortress of Doom (USA)
 *
 * These vectors come directly from the untouched SNESRecomp
 * auto_vectors analysis:
 *
 * RESET = $00:8000
 * NMI   = $00:8201
 * IRQ   = $00:8678
 */
enum {
    LUFIA1_RESET_PC = 0x008000u,
    LUFIA1_NMI_PC   = 0x008201u,
    LUFIA1_IRQ_PC   = 0x008678u,
};

/* NTSC SNES frame duration in master clocks. */
#define LUFIA1_MASTER_CLOCKS_PER_FRAME 357368ull

extern uint8 g_ram[0x20000];

extern Ppu *g_ppu;
extern Dma *g_dma;
extern Snes *g_snes;

extern uint8 g_snesrecomp_last_hdmaen;
extern int snes_frame_counter;
extern bool g_fail;

static bool s_started;
static uint32_t s_resume_pc;

static bool s_last_boundary_was_wai;
static bool s_last_boundary_was_quiet;

static uint64_t s_boundaries;
static uint64_t s_nmis;
static uint64_t s_irqs;

void Lufia1RuntimeInitialize(void) {
    s_started = false;
    s_resume_pc = LUFIA1_RESET_PC;

    s_last_boundary_was_wai = false;
    s_last_boundary_was_quiet = false;

    s_boundaries = 0;
    s_nmis = 0;
    s_irqs = 0;

    interp_bridge_reset_dynamic_cache();
}

static bool Lufia1RunToBoundary(uint32_t entry_pc) {
    const uint64_t deadline =
        g_cpu.master_cycles + LUFIA1_MASTER_CLOCKS_PER_FRAME;

    interp_bridge_set_master_deadline(deadline);

    const int ok =
        interp_bridge_run_until_quiescent(&g_cpu, entry_pc);

    interp_bridge_set_master_deadline(0);

    if (!ok) {
        fprintf(
            stderr,
            "[lufia1] LLE boundary failed at/after $%06X "
            "frame=%d S=$%04X M=%u X=%u\n",
            (unsigned)(entry_pc & 0xFFFFFFu),
            snes_frame_counter,
            g_cpu.S,
            (unsigned)(g_cpu.m_flag & 1),
            (unsigned)(g_cpu.x_flag & 1));

        g_fail = true;
        return false;
    }

    s_resume_pc =
        interp_bridge_lle_resume_pc() & 0xFFFFFFu;

    s_last_boundary_was_wai =
        interp_bridge_lle_took_wai() != 0;

    s_last_boundary_was_quiet =
        interp_bridge_lle_took_quiescent() != 0;

    s_boundaries++;

    return true;
}

static bool Lufia1RunInterrupt(uint32_t vector_pc) {
    /*
     * Preserve the interrupted guest PC so RTI returns to the
     * whole-program LLE continuation.
     */
    cpu_push_interrupt_frame_at(&g_cpu, s_resume_pc);

    if (!interp_bridge_run_interrupt(&g_cpu, vector_pc)) {
        fprintf(
            stderr,
            "[lufia1] interrupt failed: "
            "vector=$%06X resume=$%06X frame=%d\n",
            (unsigned)vector_pc,
            (unsigned)s_resume_pc,
            snes_frame_counter);

        g_fail = true;
        return false;
    }

    return true;
}

void Lufia1RunOneFrame(void) {
    if (!s_started) {
        /*
         * Start from the real ROM reset vector.
         *
         * No Lufia-specific AOT root has been introduced here:
         * RESET remains on the interpreter tier exactly as generated.
         */
        cpu_state_init(&g_cpu, g_ram);

        s_started = true;

        fprintf(
            stderr,
            "[lufia1] starting untouched hybrid LLE/AOT boot "
            "at $%06X\n",
            LUFIA1_RESET_PC);

        (void)Lufia1RunToBoundary(LUFIA1_RESET_PC);
        return;
    }

    /*
     * If the guest enabled NMI, deliver one at the frame boundary
     * before resuming the blocked CPU.
     */
    if (g_snes && g_snes->nmiEnabled) {
        g_snes->inNmi = true;

        if (!Lufia1RunInterrupt(LUFIA1_NMI_PC))
            return;

        s_nmis++;
    }

    (void)Lufia1RunToBoundary(s_resume_pc);
}

void Lufia1DrawPpuFrame(void) {
    if (!g_ppu || !g_dma || !g_snes)
        return;

    SimpleHdma hdma[8];

    dma_startDma(
        g_dma,
        g_snesrecomp_last_hdmaen,
        true);

    for (int ch = 0; ch < 8; ch++)
        SimpleHdma_Init(&hdma[ch], &g_dma->channel[ch]);

    /*
     * This first probe models the normal vertical IRQ location.
     * H-only IRQ timing is deliberately not being repaired here.
     */
    int trigger =
        g_snes->vIrqEnabled
            ? (int)g_snes->vTimer + 1
            : -1;

    for (int line = 0; line <= 224; line++) {
        ppu_runLine(g_ppu, line);

        for (int ch = 0; ch < 8; ch++)
            SimpleHdma_DoLine(&hdma[ch]);

        if (line == trigger) {
            g_snes->inIrq = true;

            if (Lufia1RunInterrupt(LUFIA1_IRQ_PC))
                s_irqs++;

            trigger =
                g_snes->vIrqEnabled
                    ? (int)g_snes->vTimer + 1
                    : -1;
        }
    }
}

void Lufia1PrintDiagnostics(void) {
    long tier_hits = interp_tier_hit_count();

    fprintf(
        stderr,
        "[lufia1] f=%d resume=$%06X %s "
        "A=%04X X=%04X Y=%04X S=%04X D=%04X "
        "PB=%02X DB=%02X M=%u Xf=%u "
        "NMI=%llu IRQ=%llu boundaries=%llu "
        "tier2=%ld NMIen=%u VIrq=%u HIrq=%u\n",
        snes_frame_counter,
        (unsigned)s_resume_pc,
        s_last_boundary_was_wai
            ? "WAI"
            : (s_last_boundary_was_quiet ? "QUIET" : "DEADLINE"),
        g_cpu.A,
        g_cpu.X,
        g_cpu.Y,
        g_cpu.S,
        g_cpu.D,
        g_cpu.PB,
        g_cpu.DB,
        (unsigned)(g_cpu.m_flag & 1),
        (unsigned)(g_cpu.x_flag & 1),
        (unsigned long long)s_nmis,
        (unsigned long long)s_irqs,
        (unsigned long long)s_boundaries,
        tier_hits,
        g_snes ? (unsigned)g_snes->nmiEnabled : 0,
        g_snes ? (unsigned)g_snes->vIrqEnabled : 0,
        g_snes ? (unsigned)g_snes->hIrqEnabled : 0);
}
