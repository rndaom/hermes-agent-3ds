#include "voice_input.h"

#include <limits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOICE_INPUT_SHARED_BUFFER_SIZE 0x30000
#define VOICE_INPUT_BYTES_PER_SAMPLE (VOICE_INPUT_BITS_PER_SAMPLE / 8)
#define VOICE_INPUT_MAX_PCM_BYTES (VOICE_INPUT_SAMPLE_RATE_HZ * VOICE_INPUT_CHANNELS * VOICE_INPUT_BYTES_PER_SAMPLE * VOICE_INPUT_MAX_SECONDS)

static void set_status(char* status_line, size_t status_line_size, const char* message)
{
    if (status_line == NULL || status_line_size == 0)
        return;

    snprintf(status_line, status_line_size, "%s", message != NULL ? message : "Voice input error.");
}

static void write_le16(u8* out, u16 value)
{
    out[0] = (u8)(value & 0xFF);
    out[1] = (u8)((value >> 8) & 0xFF);
}

static void write_le32(u8* out, u32 value)
{
    out[0] = (u8)(value & 0xFF);
    out[1] = (u8)((value >> 8) & 0xFF);
    out[2] = (u8)((value >> 16) & 0xFF);
    out[3] = (u8)((value >> 24) & 0xFF);
}

static void write_wav_header(u8* out_header, u32 pcm_size)
{
    const u32 byte_rate = VOICE_INPUT_SAMPLE_RATE_HZ * VOICE_INPUT_CHANNELS * VOICE_INPUT_BYTES_PER_SAMPLE;
    const u16 block_align = VOICE_INPUT_CHANNELS * VOICE_INPUT_BYTES_PER_SAMPLE;

    memcpy(out_header + 0, "RIFF", 4);
    write_le32(out_header + 4, pcm_size + 36);
    memcpy(out_header + 8, "WAVE", 4);
    memcpy(out_header + 12, "fmt ", 4);
    write_le32(out_header + 16, 16);
    write_le16(out_header + 20, 1);
    write_le16(out_header + 22, VOICE_INPUT_CHANNELS);
    write_le32(out_header + 24, VOICE_INPUT_SAMPLE_RATE_HZ);
    write_le32(out_header + 28, byte_rate);
    write_le16(out_header + 32, block_align);
    write_le16(out_header + 34, VOICE_INPUT_BITS_PER_SAMPLE);
    memcpy(out_header + 36, "data", 4);
    write_le32(out_header + 40, pcm_size);
}

static void copy_ring_audio(const u8* mic_buffer, u32 mic_data_size, u32* io_mic_pos, u8* pcm_buffer, size_t* io_pcm_size)
{
    u32 read_pos;
    u32 write_pos;

    if (mic_buffer == NULL || io_mic_pos == NULL || pcm_buffer == NULL || io_pcm_size == NULL)
        return;

    read_pos = *io_mic_pos;
    write_pos = micGetLastSampleOffset();
    while (*io_pcm_size < VOICE_INPUT_MAX_PCM_BYTES && read_pos != write_pos) {
        pcm_buffer[*io_pcm_size] = mic_buffer[read_pos];
        (*io_pcm_size)++;
        read_pos = (read_pos + 1) % mic_data_size;
    }

    *io_mic_pos = write_pos;
}

static unsigned long recording_tenths_elapsed(u64 start_ms)
{
    return (unsigned long)((osGetTime() - start_ms) / 100ULL);
}

static void render_recording_shell(PrintConsole* top_console, PrintConsole* bottom_console)
{
    consoleSelect(top_console);
    consoleClear();
    printf("Hermes Agent 3DS\n");
    printf("Record mic\n");
    printf("==========\n");
    printf("Mic is recording now.\n");
    printf("Time: 0.0s\n");
    printf("Audio: 0 bytes\n");
    printf("\n");
    printf("Press UP to stop and send.\n");
    printf("Press B to cancel.\n");

    consoleSelect(bottom_console);
    consoleClear();
    printf("Mic controls\n");
    printf("============\n");
    printf("UP: stop + send\n");
    printf("B: cancel\n");
    printf("5 min safety timeout\n");
    printf("START: exit app\n");
}

static void update_recording_status(PrintConsole* top_console, size_t pcm_size, unsigned long tenths)
{
    unsigned long seconds = tenths / 10;
    unsigned long tenth_digit = tenths % 10;

    consoleSelect(top_console);
    printf("\x1b[5;1HTime: %lu.%lus            ", seconds, tenth_digit);
    printf("\x1b[6;1HAudio: %lu bytes        ", (unsigned long)pcm_size);
}

bool voice_input_record_prompt(
    PrintConsole* top_console,
    PrintConsole* bottom_console,
    u8** out_wav_data,
    size_t* out_wav_size,
    char* status_line,
    size_t status_line_size
)
{
    u8* mic_buffer = NULL;
    u8* pcm_buffer = NULL;
    u8* wav_buffer = NULL;
    u32 mic_data_size = 0;
    u32 mic_pos = 0;
    size_t pcm_size = 0;
    bool sampling_started = false;
    bool canceled = false;
    bool success = false;
    bool waiting_for_up_release = true;
    u64 start_ms = 0;
    unsigned long last_render_tenths = ULONG_MAX;

    if (out_wav_data == NULL || out_wav_size == NULL || top_console == NULL || bottom_console == NULL) {
        set_status(status_line, status_line_size, "Mic input is not available.");
        return false;
    }

    *out_wav_data = NULL;
    *out_wav_size = 0;

    mic_buffer = memalign(0x1000, VOICE_INPUT_SHARED_BUFFER_SIZE);
    pcm_buffer = (u8*)malloc(VOICE_INPUT_MAX_PCM_BYTES);
    if (mic_buffer == NULL || pcm_buffer == NULL) {
        set_status(status_line, status_line_size, "Could not allocate mic buffers.");
        goto cleanup;
    }

    if (R_FAILED(micInit(mic_buffer, VOICE_INPUT_SHARED_BUFFER_SIZE))) {
        set_status(status_line, status_line_size, "Could not initialize the 3DS microphone.");
        goto cleanup;
    }

    mic_data_size = micGetSampleDataSize();
    if (mic_data_size == 0) {
        set_status(status_line, status_line_size, "Microphone buffer was invalid.");
        goto cleanup;
    }

    if (R_FAILED(MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0, mic_data_size, true))) {
        set_status(status_line, status_line_size, "Could not start mic recording.");
        goto cleanup;
    }
    sampling_started = true;
    start_ms = osGetTime();

    render_recording_shell(top_console, bottom_console);
    update_recording_status(top_console, pcm_size, 0);
    gfxFlushBuffers();
    gfxSwapBuffers();
    last_render_tenths = 0;

    while (aptMainLoop()) {
        u32 kDown;
        unsigned long current_tenths;

        gspWaitForVBlank();
        hidScanInput();
        kDown = hidKeysDown();

        copy_ring_audio(mic_buffer, mic_data_size, &mic_pos, pcm_buffer, &pcm_size);
        current_tenths = recording_tenths_elapsed(start_ms);
        if (current_tenths != last_render_tenths) {
            update_recording_status(top_console, pcm_size, current_tenths);
            gfxFlushBuffers();
            gfxSwapBuffers();
            last_render_tenths = current_tenths;
        }

        if (waiting_for_up_release) {
            if ((hidKeysHeld() & KEY_UP) == 0)
                waiting_for_up_release = false;
            continue;
        }

        if ((kDown & KEY_START) != 0) {
            canceled = true;
            set_status(status_line, status_line_size, "Mic input canceled.");
            goto cleanup;
        }
        if ((kDown & KEY_B) != 0) {
            canceled = true;
            set_status(status_line, status_line_size, "Mic input canceled.");
            break;
        }
        if ((kDown & KEY_UP) != 0) {
            set_status(status_line, status_line_size, "Mic captured. Sending to Hermes...");
            break;
        }
        if ((osGetTime() - start_ms) >= (VOICE_INPUT_MAX_SECONDS * 1000ULL) || pcm_size >= VOICE_INPUT_MAX_PCM_BYTES) {
            set_status(status_line, status_line_size, "Mic safety timeout reached. Sending to Hermes...");
            break;
        }
    }

    if (sampling_started) {
        MICU_StopSampling();
        sampling_started = false;
        copy_ring_audio(mic_buffer, mic_data_size, &mic_pos, pcm_buffer, &pcm_size);
    }

    if (canceled)
        goto cleanup;

    if ((pcm_size % 2U) != 0)
        pcm_size--;

    if (pcm_size == 0) {
        set_status(status_line, status_line_size, "No mic audio was captured.");
        goto cleanup;
    }

    wav_buffer = (u8*)malloc(pcm_size + 44);
    if (wav_buffer == NULL) {
        set_status(status_line, status_line_size, "Could not package mic audio.");
        goto cleanup;
    }

    write_wav_header(wav_buffer, (u32)pcm_size);
    memcpy(wav_buffer + 44, pcm_buffer, pcm_size);
    *out_wav_data = wav_buffer;
    *out_wav_size = pcm_size + 44;
    wav_buffer = NULL;
    success = true;

cleanup:
    if (sampling_started)
        MICU_StopSampling();
    micExit();
    if (mic_buffer != NULL)
        free(mic_buffer);
    if (pcm_buffer != NULL)
        free(pcm_buffer);
    if (wav_buffer != NULL)
        free(wav_buffer);
    return success;
}
