/* Exercise the private C arithmetic kernel directly, including under UBSan.
   This target deliberately compiles the vendored implementation on its own. */
#include "../ThirdParty/emu2413/emu2413.c"

int main(void) {
  unsigned i;
  for (i = 0; i < 0x8000; ++i) {
    const unsigned exponent = (i >> 8) & 0x7f;
    const unsigned magnitude = exp_table[(i & 0xff) ^ 0xff] + 1024;
    /* Large attenuation must underflow, not shift by more than the type width. */
    const unsigned expected = exponent >= 11 ? 0 : magnitude / (1u << exponent);
    const int positive = lookup_exp_table((uint16_t)i);
    const int negative = lookup_exp_table((uint16_t)(i | 0x8000));
    if (positive != (int)(expected * 2) || negative != -(int)(expected * 2) - 2) {
      fprintf(stderr, "OPLL exponential conversion failed at %u: %d / %d\n", i, positive, negative);
      return 1;
    }
  }
  puts("All 65536 signed OPLL logarithmic inputs produce defined, bounded output.");
  return 0;
}
