/*
 * Builds a redistribution-safe reference tone with the independently
 * maintained MIT-licensed Ayumi YM2149 emulator.
 *
 * Reproduction:
 *   git clone https://github.com/true-grue/ayumi.git
 *   git -C ayumi checkout 07c08b4874c359169e4a028edf73f046d8b763e2
 *   cc -O2 -I ayumi generate_ym2149_ayumi_reference.c ayumi/ayumi.c -lm -o generate_reference
 *   ./generate_reference ym2149-ayumi-tone-a.wav
 */

#include "ayumi.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

enum
{
    sample_rate = 48000,
    sample_count = 12000,
    channel_count = 2,
    bytes_per_sample = 2
};

static void write_u16_le(FILE* output, uint16_t value)
{
    const uint8_t bytes[2] = {
        (uint8_t) (value & 0xffu),
        (uint8_t) ((value >> 8u) & 0xffu)
    };
    fwrite(bytes, sizeof(bytes), 1, output);
}

static void write_u32_le(FILE* output, uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t) (value & 0xffu),
        (uint8_t) ((value >> 8u) & 0xffu),
        (uint8_t) ((value >> 16u) & 0xffu),
        (uint8_t) ((value >> 24u) & 0xffu)
    };
    fwrite(bytes, sizeof(bytes), 1, output);
}

static int16_t pcm16(double sample)
{
    const double limited = fmax(-1.0, fmin(1.0, sample));
    return (int16_t) lrint(limited * 32767.0);
}

static int write_header(FILE* output)
{
    const uint32_t data_size = sample_count * channel_count * bytes_per_sample;
    if (fwrite("RIFF", 4, 1, output) != 1)
        return 0;
    write_u32_le(output, 36u + data_size);
    if (fwrite("WAVEfmt ", 8, 1, output) != 1)
        return 0;
    write_u32_le(output, 16u);
    write_u16_le(output, 1u);
    write_u16_le(output, channel_count);
    write_u32_le(output, sample_rate);
    write_u32_le(output, sample_rate * channel_count * bytes_per_sample);
    write_u16_le(output, channel_count * bytes_per_sample);
    write_u16_le(output, bytes_per_sample * 8);
    if (fwrite("data", 4, 1, output) != 1)
        return 0;
    write_u32_le(output, data_size);
    return ferror(output) == 0;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: generate_reference output.wav\n");
        return 1;
    }

    struct ayumi chip;
    if (! ayumi_configure(&chip, 1, 1773400.0, sample_rate))
    {
        fprintf(stderr, "Ayumi rejected the YM2149 clock/sample-rate pair\n");
        return 1;
    }

    ayumi_set_pan(&chip, 0, 0.5, 0);
    ayumi_set_pan(&chip, 1, 0.5, 0);
    ayumi_set_pan(&chip, 2, 0.5, 0);
    ayumi_set_tone(&chip, 0, 0x120);
    ayumi_set_tone(&chip, 1, 1);
    ayumi_set_tone(&chip, 2, 1);
    ayumi_set_noise(&chip, 1);
    ayumi_set_mixer(&chip, 0, 0, 1, 0);
    ayumi_set_mixer(&chip, 1, 1, 1, 0);
    ayumi_set_mixer(&chip, 2, 1, 1, 0);
    ayumi_set_volume(&chip, 0, 15);
    ayumi_set_volume(&chip, 1, 0);
    ayumi_set_volume(&chip, 2, 0);
    ayumi_set_envelope(&chip, 1);
    ayumi_set_envelope_shape(&chip, 9);

    FILE* output = fopen(argv[1], "wb");
    if (output == NULL || ! write_header(output))
    {
        fprintf(stderr, "Could not create reference WAV\n");
        if (output != NULL)
            fclose(output);
        return 1;
    }

    for (int sample = 0; sample < sample_count; ++sample)
    {
        ayumi_process(&chip);
        ayumi_remove_dc(&chip);
        write_u16_le(output, (uint16_t) pcm16(chip.left * 0.8));
        write_u16_le(output, (uint16_t) pcm16(chip.right * 0.8));
    }

    const int failed = ferror(output);
    fclose(output);
    return failed ? 1 : 0;
}
