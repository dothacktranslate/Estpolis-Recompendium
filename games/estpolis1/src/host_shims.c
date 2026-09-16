#include <stdio.h>
#include <stdlib.h>

#include "spc_player.h"

/*
 * The MMX reference host supplies a game-specific HLE SPC player.
 *
 * Estpolis Denki I currently uses SNESRecomp's real SPC/APU hardware path instead,
 * so no game-specific HLE player is installed here.
 *
 * The shared runner still references this legacy pointer from RtlReset().
 * The current Estpolis Denki I startup/runtime path does not call that reset path.
 */
SpcPlayer *g_spc_player = NULL;


/*
 * Generic fatal-error hook expected by shared runner utilities.
 */
void Die(const char *error)
{
    fprintf(
        stderr,
        "[host] fatal error: %s\n",
        error ? error : "(unknown)");

    fflush(stderr);
    exit(EXIT_FAILURE);
}
