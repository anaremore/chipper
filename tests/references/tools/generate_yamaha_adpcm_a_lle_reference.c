/*
 * Chipper synthetic Yamaha ADPCM-A reference capture tool.
 *
 * This source contains only original Chipper test logic. It is not part of the
 * product build. Reproduction compiles it in an isolated checkout against the
 * pinned GPL YM2608-LLE oracle; neither the oracle source nor binary ships with
 * Chipper.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(CHIPPER_LLE_OPNA)
#include "fmopna_2608.h"
typedef fmopna_t oracle_chip_t;
#define ORACLE_CLOCK(chip, level) FMOPNA_Clock((chip), (level))
static const uint32_t master_clock_hz = 7987200u;
#elif defined(CHIPPER_LLE_OPNB)
#include "fmopna_2610.h"
typedef fmopna_2610_t oracle_chip_t;
#define ORACLE_CLOCK(chip, level) FMOPNA_2610_Clock((chip), (level))
static const uint32_t master_clock_hz = 8000000u;
#else
#error Define CHIPPER_LLE_OPNA or CHIPPER_LLE_OPNB.
#endif

enum
{
    output_rate_hz = 48000,
    output_frames = 9600,
    native_rate_divisor = 144
};

typedef struct
{
    oracle_chip_t chip;
    uint8_t* adpcm_a;
    size_t adpcm_a_size;
#if defined(CHIPPER_LLE_OPNB)
    uint32_t adpcm_a_address;
    int previous_rmpx;
#endif
    int previous_s;
    int previous_sh1;
    int previous_sh2;
    uint16_t serial_shift;
    int16_t sh1;
    int16_t sh2;
    int have_sh1;
    int have_sh2;
} capture_t;

static void write_u16le(FILE* file, uint16_t value)
{
    fputc((int)(value & 0xffu), file);
    fputc((int)((value >> 8u) & 0xffu), file);
}

static void write_u32le(FILE* file, uint32_t value)
{
    write_u16le(file, (uint16_t)(value & 0xffffu));
    write_u16le(file, (uint16_t)((value >> 16u) & 0xffffu));
}

static int write_wav(const char* path, const int16_t* interleaved)
{
    const uint32_t data_bytes = output_frames * 2u * (uint32_t)sizeof(int16_t);
    FILE* file = fopen(path, "wb");
    if (file == NULL)
        return 0;

    fwrite("RIFF", 1, 4, file);
    write_u32le(file, 36u + data_bytes);
    fwrite("WAVEfmt ", 1, 8, file);
    write_u32le(file, 16u);
    write_u16le(file, 1u);
    write_u16le(file, 2u);
    write_u32le(file, output_rate_hz);
    write_u32le(file, output_rate_hz * 4u);
    write_u16le(file, 4u);
    write_u16le(file, 16u);
    fwrite("data", 1, 4, file);
    write_u32le(file, data_bytes);
    fwrite(interleaved, sizeof(int16_t), output_frames * 2u, file);
    fclose(file);
    return 1;
}

static uint8_t* read_binary(const char* path, size_t expected_size)
{
    FILE* file = fopen(path, "rb");
    uint8_t* data;
    long size;
    if (file == NULL)
        return NULL;
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }
    if ((size_t)size != expected_size)
    {
        fclose(file);
        return NULL;
    }
    data = (uint8_t*)malloc(expected_size);
    if (data == NULL || fread(data, 1, expected_size, file) != expected_size)
    {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    return data;
}

static void set_bus_idle(capture_t* capture)
{
    capture->chip.input.cs = 1;
    capture->chip.input.rd = 1;
    capture->chip.input.wr = 1;
    capture->chip.input.a0 = 0;
    capture->chip.input.a1 = 0;
    capture->chip.input.data = 0;
}

static void update_memory_bus(capture_t* capture)
{
#if defined(CHIPPER_LLE_OPNB)
    oracle_chip_t* chip = &capture->chip;
    if (chip->o_rmpx && !capture->previous_rmpx)
    {
        capture->adpcm_a_address &= ~0x3ffu;
        capture->adpcm_a_address |= (uint32_t)(chip->o_rad & 0xff)
                                 | ((uint32_t)(chip->o_ra8 & 3) << 8u);
    }
    if (!chip->o_rmpx && capture->previous_rmpx)
    {
        capture->adpcm_a_address &= 0x3ffu;
        capture->adpcm_a_address |= ((uint32_t)(chip->o_rad & 0xff)
                                  | ((uint32_t)(chip->o_ra8 & 3) << 8u)
                                  | ((uint32_t)(chip->o_ra20 & 1) << 10u)) << 10u;
    }
    if (!chip->o_roe && capture->adpcm_a_size != 0u)
        chip->input.rad = capture->adpcm_a[capture->adpcm_a_address % capture->adpcm_a_size];
    capture->previous_rmpx = chip->o_rmpx;
#else
    (void)capture;
#endif
}

static void update_serial(capture_t* capture)
{
    oracle_chip_t* chip = &capture->chip;
    if (!chip->o_s && capture->previous_s)
    {
        if (!chip->o_sh1 && capture->previous_sh1)
        {
            capture->sh1 = (int16_t)(capture->serial_shift ^ 0x8000u);
            capture->have_sh1 = 1;
        }
        if (!chip->o_sh2 && capture->previous_sh2)
        {
            capture->sh2 = (int16_t)(capture->serial_shift ^ 0x8000u);
            capture->have_sh2 = 1;
        }

        capture->serial_shift >>= 1u;
        capture->serial_shift |= (uint16_t)((chip->o_opo & 1) << 15u);
        capture->previous_sh1 = chip->o_sh1;
        capture->previous_sh2 = chip->o_sh2;
    }
    capture->previous_s = chip->o_s;
}

static void clock_master(capture_t* capture, int collect_serial)
{
    ORACLE_CLOCK(&capture->chip, 0);
    ORACLE_CLOCK(&capture->chip, 1);
    update_memory_bus(capture);
    if (collect_serial)
        update_serial(capture);
}

static void clock_many(capture_t* capture, uint32_t count)
{
    uint32_t index;
    for (index = 0; index < count; ++index)
        clock_master(capture, 0);
}

static void reset_oracle(capture_t* capture)
{
    memset(&capture->chip, 0, sizeof(capture->chip));
    capture->chip.input.test = 1;
    capture->chip.input.cs = 1;
    capture->chip.input.rd = 0;
    capture->chip.input.wr = 0;
    capture->chip.input.a0 = 0;
    capture->chip.input.a1 = 0;
    capture->chip.input.data = 0;
#if defined(CHIPPER_LLE_OPNB)
    capture->chip.input.rad = 0;
    capture->chip.input.pad = 0;
    capture->chip.input.ym2610b = 0;
#endif
    capture->chip.input.ic = 1;
    clock_many(capture, 576);
    capture->chip.input.ic = 0;
    clock_many(capture, 576);
    capture->chip.input.ic = 1;
    clock_many(capture, 576);
}

typedef struct
{
    uint16_t address;
    uint8_t value;
} register_write_t;

static void run_register_writes(capture_t* capture,
                                const register_write_t* writes,
                                size_t write_count)
{
    size_t write_index = 0;
    int writing_value = 0;
    int delay = 0;

    while (write_index < write_count || writing_value || delay > 0)
    {
        const int can_write = capture->chip.prescaler_latch[1] & 1;
        if (can_write)
        {
            if (delay > 0)
            {
                if (delay == 3)
                {
                    set_bus_idle(capture);
                    delay = 0;
                }
                else
                {
                    capture->chip.input.cs = 0;
                    capture->chip.input.rd = 0;
                    capture->chip.input.wr = 1;
                    capture->chip.input.a0 = 0;
                    capture->chip.input.a1 = 0;
                    capture->chip.input.data = 0;
                    delay = 1;
                }
            }
            else if (write_index < write_count)
            {
                const register_write_t* write = &writes[write_index];
                capture->chip.input.cs = 0;
                capture->chip.input.rd = 1;
                capture->chip.input.wr = 0;
                capture->chip.input.a1 = (write->address & 0x100u) != 0u;
                capture->chip.input.a0 = writing_value;
                capture->chip.input.data = writing_value
                    ? write->value
                    : (uint8_t)(write->address & 0xffu);
                delay = write->address < 0x10u ? 3 : 2;
                if (writing_value)
                {
                    writing_value = 0;
                    ++write_index;
                }
                else
                {
                    writing_value = 1;
                }
            }
            else
            {
                set_bus_idle(capture);
            }
        }

        clock_master(capture, 0);
        if (can_write && delay == 1 && capture->chip.busy_cnt_en[1] == 0)
            delay = 0;
    }
    set_bus_idle(capture);
}

static void program_trace(capture_t* capture)
{
#if defined(CHIPPER_LLE_OPNA)
    static const register_write_t writes[] = {
        { 0x11u, 0x3fu },
        { 0x18u, 0x9fu },
        { 0x1cu, 0x5fu },
        { 0x10u, 0x11u },
    };
#else
    static const register_write_t writes[] = {
        { 0x101u, 0x3fu },
        { 0x108u, 0x9fu },
        { 0x10du, 0x5fu },
        { 0x110u, 0x00u },
        { 0x118u, 0x00u },
        { 0x120u, 0x00u },
        { 0x128u, 0x00u },
        { 0x115u, 0x02u },
        { 0x11du, 0x00u },
        { 0x125u, 0x02u },
        { 0x12du, 0x00u },
        { 0x100u, 0x21u },
    };
#endif
    run_register_writes(capture, writes, sizeof(writes) / sizeof(writes[0]));
}

static void begin_serial_capture(capture_t* capture)
{
    capture->previous_s = capture->chip.o_s;
    capture->previous_sh1 = capture->chip.o_sh1;
    capture->previous_sh2 = capture->chip.o_sh2;
    capture->serial_shift = 0;
    capture->have_sh1 = 0;
    capture->have_sh2 = 0;
}

static void next_native_frame(capture_t* capture, int16_t* left, int16_t* right)
{
    capture->have_sh1 = 0;
    capture->have_sh2 = 0;
    while (!capture->have_sh1 || !capture->have_sh2)
        clock_master(capture, 1);

    /* A direct pan test maps the serial sample-and-hold outputs as SH2=L, SH1=R. */
    *left = capture->sh2;
    *right = capture->sh1;
}

int main(int argc, char** argv)
{
    capture_t capture;
    int16_t* output;
    uint64_t accumulator = 0;
    int16_t current_left = 0;
    int16_t current_right = 0;
    int frame;
    const size_t expected_size =
#if defined(CHIPPER_LLE_OPNA)
        0x2000u;
#else
        0x300u;
#endif

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s synthetic-bank.bin output.wav\n", argv[0]);
        return 2;
    }

    memset(&capture, 0, sizeof(capture));
    capture.adpcm_a = read_binary(argv[1], expected_size);
    capture.adpcm_a_size = expected_size;
    if (capture.adpcm_a == NULL)
    {
        fprintf(stderr, "expected exactly %zu bytes in %s\n", expected_size, argv[1]);
        return 2;
    }

    output = (int16_t*)calloc(output_frames * 2u, sizeof(int16_t));
    if (output == NULL)
    {
        free(capture.adpcm_a);
        return 2;
    }

    reset_oracle(&capture);
    program_trace(&capture);
    set_bus_idle(&capture);
#if defined(CHIPPER_LLE_OPNA)
    fprintf(stderr, "programmed keymask=%d tl=%d regs0=%d regs8=%d regs12=%d\n",
            capture.chip.rss_keymask[1], capture.chip.rss_tl[1],
            capture.chip.rss_regs[1][0], capture.chip.rss_regs[1][8],
            capture.chip.rss_regs[1][12]);
#else
    fprintf(stderr, "programmed keymask=%d tl=%d pan0=0x%02x pan5=0x%02x\n",
            capture.chip.rss_keymask[1], capture.chip.rss_tl[1],
            capture.chip.rss_reg_pan_tl[1][0],
            capture.chip.rss_reg_pan_tl[1][5]);
#endif
    begin_serial_capture(&capture);

    for (frame = 0; frame < output_frames; ++frame)
    {
        accumulator += master_clock_hz;
        while (accumulator >= (uint64_t)native_rate_divisor * output_rate_hz)
        {
            next_native_frame(&capture, &current_left, &current_right);
            accumulator -= (uint64_t)native_rate_divisor * output_rate_hz;
        }
        output[frame * 2] = current_left;
        output[frame * 2 + 1] = current_right;
    }

    if (!write_wav(argv[2], output))
    {
        fprintf(stderr, "could not write %s\n", argv[2]);
        free(output);
        free(capture.adpcm_a);
        return 2;
    }

#if defined(CHIPPER_LLE_OPNA)
    fprintf(stderr,
            "captured %d frames at %d Hz; keymask=%d\n",
            output_frames,
            output_rate_hz,
            capture.chip.rss_keymask[1]);
#else
    fprintf(stderr,
            "captured %d frames at %d Hz; keymask=%d pan0=0x%02x pan5=0x%02x\n",
            output_frames,
            output_rate_hz,
            capture.chip.rss_keymask[1],
            capture.chip.rss_reg_pan_tl[1][0],
            capture.chip.rss_reg_pan_tl[1][5]);
#endif

    free(output);
    free(capture.adpcm_a);
    return 0;
}
