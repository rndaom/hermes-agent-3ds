#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#define VOICE_INPUT_MAX_SECONDS 6
#define VOICE_INPUT_SAMPLE_RATE_HZ 16360
#define VOICE_INPUT_BITS_PER_SAMPLE 16
#define VOICE_INPUT_CHANNELS 1

bool voice_input_record_prompt(
    PrintConsole* top_console,
    PrintConsole* bottom_console,
    u8** out_wav_data,
    size_t* out_wav_size,
    char* status_line,
    size_t status_line_size
);
