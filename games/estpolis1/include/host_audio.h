#ifndef ESTPOLIS1_HOST_AUDIO_H
#define ESTPOLIS1_HOST_AUDIO_H

#include <stdbool.h>

/*
 * Initialize the real-time SDL audio consumer used by the permanent
 * Estpolis Denki I host.
 *
 * SNESRecomp owns SPC/APU guest-time progression inside RtlRunFrame().
 * The host audio device only consumes the PCM produced by that guest-time
 * execution through RtlRenderAudio().
 */
bool HostAudioInit(void);

/*
 * Stop the device and release all host-side audio resources.
 */
void HostAudioShutdown(void);

#endif
