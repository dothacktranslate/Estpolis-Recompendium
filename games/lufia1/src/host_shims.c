#include <stdio.h>
#include <stdlib.h>

/*
 * Headless bootstrap-only host shims.
 *
 * No SDL audio callback/thread exists in this diagnostic executable,
 * so the APU lock is intentionally a no-op.
 */
void RtlApuLock(void) {
}

void RtlApuUnlock(void) {
}

/*
 * The shared runner still contains a legacy/HLE SPC-player pointer.
 *
 * This raw probe uses the real SNES SPC/APU hardware path and does not
 * call RtlReset(), which is the remaining code path that dereferences
 * this pointer in the current runner revision.
 *
 * This is diagnostic scaffolding only, not production integration.
 */
void *g_spc_player = NULL;

/*
 * Generic fatal-error hook expected by several shared runner utilities.
 */
void Die(const char *error) {
    fprintf(stderr,
            "[host] fatal error: %s\n",
            error ? error : "(unknown)");
    fflush(stderr);
    exit(EXIT_FAILURE);
}
