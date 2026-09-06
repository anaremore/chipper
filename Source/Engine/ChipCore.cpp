#include "Engine/ChipCore.h"
#include "Cores/CoreHelpers.h"
#include "Cores/CoreFactories.h"

namespace chipper
{
using core_detail::lower;

std::unique_ptr<ChipCore> createChipCore(ChipMode mode, AccuracyMode accuracy)
{
    switch (mode)
    {
        case ChipMode::nes: return core_detail::makeNesApuCore(accuracy);
        case ChipMode::nesVrc6: return core_detail::makeNesApuCore(accuracy, ChipMode::nesVrc6);
        case ChipMode::nesFds: return core_detail::makeNesApuCore(accuracy, ChipMode::nesFds);
        case ChipMode::nesSunsoft5b: return core_detail::makeNesApuCore(accuracy, ChipMode::nesSunsoft5b);
        case ChipMode::nesMmc5: return core_detail::makeNesApuCore(accuracy, ChipMode::nesMmc5);
        case ChipMode::nesVrc7: return core_detail::makeNesApuCore(accuracy, ChipMode::nesVrc7);
        case ChipMode::dmg: return core_detail::makeDmgApuCore(accuracy);
        case ChipMode::sid: return core_detail::makeSidCore(accuracy);
        case ChipMode::ym2149: return core_detail::makeYm2149Core(accuracy);
        case ChipMode::sn76489: return core_detail::makeSn76489Core(accuracy);
        case ChipMode::ym2612: return core_detail::makeYm2612Core(accuracy);
        case ChipMode::opl3: return core_detail::makeOpl3Core(accuracy);
        case ChipMode::ym2151: return core_detail::makeYm2151Core(accuracy);
        case ChipMode::spc700: return core_detail::makeSpc700Core(accuracy);
        case ChipMode::pokey: return core_detail::makePokeyCore(accuracy);
        case ChipMode::paula: return core_detail::makePaulaCore(accuracy);
        case ChipMode::huc6280: return core_detail::makeHuc6280Core(accuracy);
        case ChipMode::namcoWsg: return core_detail::makeNamcoWsgCore(accuracy);
        case ChipMode::ym2413: return core_detail::makeYm2413Core(accuracy);
        case ChipMode::scc: return core_detail::makeSccCore(accuracy);
        case ChipMode::ym2203: return core_detail::makeYm2203Core(accuracy);
        case ChipMode::ym2608: return core_detail::makeYm2608Core(accuracy);
        case ChipMode::ym2610: return core_detail::makeYm2610Core(accuracy);
        case ChipMode::ym2610b: return core_detail::makeYm2610Core(accuracy, true);
        case ChipMode::saa1099: return core_detail::makeSaa1099Core(accuracy);
        case ChipMode::pcSpeaker: return core_detail::makePcSpeakerCore(accuracy);
        case ChipMode::zxSpectrumBeeper: return core_detail::makeZxSpectrumBeeperCore(accuracy);
        default: return core_detail::makeUnsupportedCore(mode, accuracy);
    }
}

std::optional<ChipMode> parseChipMode(std::string_view text)
{
    const auto key = lower(text);
    if (key == "nes" || key == "rp2a03") return ChipMode::nes;
    if (key == "nesvrc6" || key == "nes+vrc6" || key == "vrc6" || key == "famicomvrc6" || key == "famicom+vrc6") return ChipMode::nesVrc6;
    if (key == "nesfds" || key == "nes+fds" || key == "fds" || key == "famicomfds" || key == "famicom+fds" || key == "famicomdisksystem") return ChipMode::nesFds;
    if (key == "nessunsoft5b" || key == "nes+sunsoft5b" || key == "sunsoft5b" || key == "5b" || key == "sunsoft" || key == "fme7" || key == "sunsoftfme7" || key == "nesfme7") return ChipMode::nesSunsoft5b;
    if (key == "nesmmc5" || key == "nes+mmc5" || key == "mmc5" || key == "famicommmc5" || key == "famicom+mmc5") return ChipMode::nesMmc5;
    if (key == "nesvrc7" || key == "nes+vrc7" || key == "vrc7" || key == "famicomvrc7" || key == "famicom+vrc7" || key == "familynoraebang" || key == "noraebang" || key == "nesopll") return ChipMode::nesVrc7;
    if (key == "dmg" || key == "gameboy" || key == "gameboydmg") return ChipMode::dmg;
    if (key == "sid" || key == "c64" || key == "commodore64") return ChipMode::sid;
    if (key == "ym2149" || key == "ay" || key == "ay38910" || key == "ay38910") return ChipMode::ym2149;
    if (key == "sn76489" || key == "segapsg" || key == "psg") return ChipMode::sn76489;
    if (key == "ym2612" || key == "opn2" || key == "genesis") return ChipMode::ym2612;
    if (key == "opl2" || key == "opl3" || key == "ym3812" || key == "ymf262") return ChipMode::opl3;
    if (key == "spc700" || key == "snes") return ChipMode::spc700;
    if (key == "pokey" || key == "atari") return ChipMode::pokey;
    if (key == "paula" || key == "amiga") return ChipMode::paula;
    if (key == "huc6280" || key == "pcengine" || key == "turbografx") return ChipMode::huc6280;
    if (key == "namco" || key == "namcowsg" || key == "wsg") return ChipMode::namcoWsg;
    if (key == "ym2151" || key == "opm") return ChipMode::ym2151;
    if (key == "ym2413" || key == "opll") return ChipMode::ym2413;
    if (key == "scc" || key == "konamiscc") return ChipMode::scc;
    if (key == "ym2203" || key == "opn" || key == "pc88" || key == "pc8801") return ChipMode::ym2203;
    if (key == "ym2608" || key == "opna" || key == "pc98" || key == "pc9801") return ChipMode::ym2608;
    if (key == "ym2610" || key == "opnb" || key == "neogeo") return ChipMode::ym2610;
    if (key == "ym2610b" || key == "opnb2" || key == "neogeomvs2") return ChipMode::ym2610b;
    if (key == "saa1099" || key == "saa" || key == "samcoupe" || key == "philipspsg") return ChipMode::saa1099;
    if (key == "pcspeaker" || key == "pcspkr" || key == "speaker" || key == "pit" || key == "beeper") return ChipMode::pcSpeaker;
    if (key == "zxspectrum" || key == "zxspectrumbeeper" || key == "zx" || key == "zx48" || key == "zxbeeper" || key == "spectrumbeeper") return ChipMode::zxSpectrumBeeper;
    return std::nullopt;
}

std::optional<AccuracyMode> parseAccuracyMode(std::string_view text)
{
    const auto key = lower(text);
    if (key == "inspired") return AccuracyMode::inspired;
    if (key == "hybrid") return AccuracyMode::hybrid;
    if (key == "authentic") return AccuracyMode::authentic;
    return std::nullopt;
}

std::optional<MacroKind> parseMacroKind(std::string_view text)
{
    const auto key = lower(text);
    if (key == "manual") return MacroKind::manual;
    if (key == "coin") return MacroKind::coin;
    if (key == "bass") return MacroKind::bass;
    if (key == "lead") return MacroKind::lead;
    if (key == "arp") return MacroKind::arp;
    if (key == "drum") return MacroKind::drum;
    if (key == "hit") return MacroKind::hit;
    if (key == "laser") return MacroKind::laser;
    if (key == "jump") return MacroKind::jump;
    if (key == "powerup") return MacroKind::powerUp;
    return std::nullopt;
}

std::optional<PlayMode> parsePlayMode(std::string_view text)
{
    const auto key = lower(text);
    if (key == "stack" || key == "bigmono" || key == "mono") return PlayMode::stack;
    if (key == "chippoly" || key == "allocate" || key == "allocation") return PlayMode::chipPoly;
    if (key == "manual" || key == "multi") return PlayMode::manual;
    if (key == "clone" || key == "hybridpoly") return PlayMode::clone;
    return std::nullopt;
}

std::string toString(ChipMode mode)
{
    switch (mode)
    {
        case ChipMode::nes: return "NES / RP2A03";
        case ChipMode::nesVrc6: return "NES + VRC6";
        case ChipMode::nesFds: return "NES + FDS";
        case ChipMode::nesSunsoft5b: return "NES + Sunsoft 5B";
        case ChipMode::nesMmc5: return "NES + MMC5";
        case ChipMode::nesVrc7: return "NES + VRC7";
        case ChipMode::dmg: return "Game Boy / DMG APU";
        case ChipMode::sid: return "SID / C64";
        case ChipMode::ym2149: return "YM2149 / AY";
        case ChipMode::sn76489: return "SN76489 / Sega PSG";
        case ChipMode::ym2612: return "YM2612 / Genesis FM";
        case ChipMode::opl3: return "OPL2/OPL3 / DOS FM";
        case ChipMode::spc700: return "SNES SPC700-style";
        case ChipMode::pokey: return "Atari POKEY";
        case ChipMode::paula: return "Amiga Paula";
        case ChipMode::huc6280: return "PC Engine HuC6280";
        case ChipMode::namcoWsg: return "Namco arcade WSG";
        case ChipMode::ym2151: return "YM2151 arcade/X68000 FM";
        case ChipMode::ym2413: return "YM2413 / OPLL";
        case ChipMode::scc: return "Konami SCC";
        case ChipMode::ym2203: return "YM2203 / OPN";
        case ChipMode::ym2608: return "YM2608 / OPNA";
        case ChipMode::ym2610: return "YM2610 / OPNB";
        case ChipMode::ym2610b: return "YM2610B / OPNB2";
        case ChipMode::saa1099: return "Philips SAA1099";
        case ChipMode::pcSpeaker: return "PC Speaker";
        case ChipMode::zxSpectrumBeeper: return "ZX Spectrum Beeper";
    }
    return "Unknown";
}

std::string toString(AccuracyMode mode)
{
    switch (mode)
    {
        case AccuracyMode::inspired: return "Inspired";
        case AccuracyMode::hybrid: return "Hybrid";
        case AccuracyMode::authentic: return "Authentic";
    }
    return "Unknown";
}

std::string toString(MacroKind macro)
{
    switch (macro)
    {
        case MacroKind::manual: return "Manual";
        case MacroKind::coin: return "Coin";
        case MacroKind::bass: return "Bass";
        case MacroKind::lead: return "Lead";
        case MacroKind::arp: return "Arp";
        case MacroKind::drum: return "Drum";
        case MacroKind::hit: return "Hit";
        case MacroKind::laser: return "Laser";
        case MacroKind::jump: return "Jump";
        case MacroKind::powerUp: return "Power-Up";
    }
    return "Unknown";
}

std::string toString(PlayMode playMode)
{
    switch (playMode)
    {
        case PlayMode::stack: return "Big Mono";
        case PlayMode::chipPoly: return "Chip Poly";
        case PlayMode::manual: return "Manual";
        case PlayMode::clone: return "Clone";
    }
    return "Unknown";
}

double midiNoteToHz(int midiNote)
{
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

} // namespace chipper
