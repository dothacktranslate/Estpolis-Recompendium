#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "common_rtl.h"
#include "host_audio.h"

/*
 * Current Recompendium desktop host audio settings.
 *
 * The SNESRecomp audio consumer resamples the native 32040 Hz S-DSP stream
 * to the actual device rate reported through RtlSetAudioOutputRate().
 */
enum {
    HOST_AUDIO_REQUESTED_RATE = 32040,
    HOST_AUDIO_CHANNELS       = 2,
    HOST_AUDIO_CALLBACK_FRAMES = 1024,

    SNES_AUDIO_NATIVE_RATE    = 32040,
    SNES_AUDIO_NATIVE_BLOCK   = 534
};

static SDL_mutex *g_audio_mutex = NULL;
static SDL_AudioDeviceID g_audio_device = 0;

static uint8_t *g_audio_buffer = NULL;
static uint8_t *g_audio_buffer_cur = NULL;
static uint8_t *g_audio_buffer_end = NULL;

static int g_audio_frames_per_block = 0;
static int g_audio_channels = HOST_AUDIO_CHANNELS;


/*
 * These hooks are required by the shared SNESRecomp runtime.
 *
 * The CPU thread and SDL audio callback can both access the SPC/APU/DSP
 * state, so the former no-op probe shims must become real synchronization
 * once the audio callback exists.
 *
 * RtlRenderAudio() itself also enters this lock. SDL mutexes support
 * recursive locking by the owning thread, matching the stock SNESRecomp
 * desktop host's locking structure.
 */
void RtlApuLock(void)
{
    if (g_audio_mutex)
        SDL_LockMutex(g_audio_mutex);
}

void RtlApuUnlock(void)
{
    if (g_audio_mutex)
        SDL_UnlockMutex(g_audio_mutex);
}


/*
 * Fill SDL's requested byte range from fixed-size SNESRecomp render blocks.
 *
 * Keeping this block-buffer structure mirrors the stock desktop host and
 * lets RtlRenderAudio's native-rate occupancy servo operate with the same
 * kind of consumer cadence.
 */
static void FillAudioBuffer(Uint8 *stream, int len)
{
    if (!stream || len <= 0)
        return;

    if (!g_audio_mutex || !g_audio_buffer) {
        SDL_memset(stream, 0, (size_t)len);
        return;
    }

    if (SDL_LockMutex(g_audio_mutex) != 0) {
        SDL_memset(stream, 0, (size_t)len);
        return;
    }

    while (len > 0) {
        if (g_audio_buffer_cur == g_audio_buffer_end) {
            RtlRenderAudio(
                (int16_t *)g_audio_buffer,
                g_audio_frames_per_block,
                g_audio_channels);

            g_audio_buffer_cur = g_audio_buffer;
            g_audio_buffer_end =
                g_audio_buffer +
                (size_t)g_audio_frames_per_block *
                (size_t)g_audio_channels *
                sizeof(int16_t);
        }

        size_t available =
            (size_t)(g_audio_buffer_end - g_audio_buffer_cur);

        size_t requested = (size_t)len;
        size_t copy_size =
            requested < available ? requested : available;

        memcpy(stream, g_audio_buffer_cur, copy_size);

        g_audio_buffer_cur += copy_size;
        stream += copy_size;
        len -= (int)copy_size;
    }

    SDL_UnlockMutex(g_audio_mutex);
}


static void SDLCALL AudioCallback(
    void *userdata,
    Uint8 *stream,
    int len)
{
    (void)userdata;

    FillAudioBuffer(stream, len);
}


bool HostAudioInit(void)
{
    if (g_audio_device != 0)
        return true;

    g_audio_mutex = SDL_CreateMutex();

    if (!g_audio_mutex) {
        fprintf(
            stderr,
            "[audio] SDL_CreateMutex failed: %s\n",
            SDL_GetError());

        return false;
    }

    SDL_AudioSpec want;
    SDL_AudioSpec have;

    SDL_zero(want);
    SDL_zero(have);

    want.freq = HOST_AUDIO_REQUESTED_RATE;
    want.format = AUDIO_S16;
    want.channels = HOST_AUDIO_CHANNELS;
    want.samples = HOST_AUDIO_CALLBACK_FRAMES;
    want.callback = AudioCallback;
    want.userdata = NULL;

    /*
     * Keep the format/channel contract strict, matching the stock host.
     * SDL therefore either gives us the requested format or fails cleanly.
     */
    g_audio_device =
        SDL_OpenAudioDevice(
            NULL,
            0,
            &want,
            &have,
            0);

    if (g_audio_device == 0) {
        fprintf(
            stderr,
            "[audio] SDL_OpenAudioDevice failed: %s\n",
            SDL_GetError());

        SDL_DestroyMutex(g_audio_mutex);
        g_audio_mutex = NULL;

        return false;
    }

    if (have.channels != HOST_AUDIO_CHANNELS ||
        have.format != AUDIO_S16) {

        fprintf(
            stderr,
            "[audio] unexpected device format: "
            "freq=%d channels=%u format=0x%04x\n",
            have.freq,
            (unsigned)have.channels,
            (unsigned)have.format);

        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;

        SDL_DestroyMutex(g_audio_mutex);
        g_audio_mutex = NULL;

        return false;
    }

    g_audio_channels = have.channels;

    /*
     * Tell SNESRecomp the rate the device actually opened at BEFORE audio
     * starts. RtlRenderAudio uses this to convert native 32040 Hz DSP output
     * to the host device clock.
     */
    RtlSetAudioOutputRate(have.freq);

    /*
     * One native S-DSP block is 534 samples at 32040 Hz.
     *
     * Scale that block to the actual host rate using the same calculation
     * as the stock SNESRecomp desktop runner.
     */
    g_audio_frames_per_block =
        (SNES_AUDIO_NATIVE_BLOCK * have.freq +
         SNES_AUDIO_NATIVE_RATE / 2) /
        SNES_AUDIO_NATIVE_RATE;

    const size_t buffer_bytes =
        (size_t)g_audio_frames_per_block *
        (size_t)g_audio_channels *
        sizeof(int16_t);

    g_audio_buffer =
        (uint8_t *)calloc(buffer_bytes, 1);

    if (!g_audio_buffer) {
        fprintf(
            stderr,
            "[audio] unable to allocate %zu-byte audio buffer\n",
            buffer_bytes);

        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;

        SDL_DestroyMutex(g_audio_mutex);
        g_audio_mutex = NULL;

        return false;
    }

    /*
     * Equal pointers mean the first callback immediately asks
     * RtlRenderAudio() for a fresh block.
     */
    g_audio_buffer_cur = g_audio_buffer;
    g_audio_buffer_end = g_audio_buffer;

    fprintf(
        stderr,
        "[audio] device opened: "
        "freq=%d channels=%u callback=%u render_block=%d\n",
        have.freq,
        (unsigned)have.channels,
        (unsigned)have.samples,
        g_audio_frames_per_block);

    /*
     * SDL2 opens devices paused. Everything above, including
     * RtlSetAudioOutputRate(), is established before the callback begins.
     */
    SDL_PauseAudioDevice(g_audio_device, 0);

    fprintf(stderr, "[audio] playback started\n");

    return true;
}


void HostAudioShutdown(void)
{
    if (g_audio_device != 0) {
        SDL_PauseAudioDevice(g_audio_device, 1);
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }

    free(g_audio_buffer);

    g_audio_buffer = NULL;
    g_audio_buffer_cur = NULL;
    g_audio_buffer_end = NULL;

    g_audio_frames_per_block = 0;

    if (g_audio_mutex) {
        SDL_DestroyMutex(g_audio_mutex);
        g_audio_mutex = NULL;
    }
}
