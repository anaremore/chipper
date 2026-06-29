#include "Parameters.h"

namespace chipper::parameters
{

juce::StringArray chipModeChoices()
{
    return {
        "NES / RP2A03",
        "Game Boy / DMG",
        "SID / C64",
        "YM2149 / AY",
        "SN76489 / Sega PSG",
        "YM2612 / Genesis FM",
        "OPL2/OPL3 / DOS FM",
        "SNES SPC700-style",
        "Atari POKEY",
        "Amiga Paula",
        "PC Engine HuC6280",
        "Namco arcade WSG",
        "YM2151 arcade FM",
        "YM2413 / OPLL",
        "Konami SCC",
        "YM2203 / OPN",
        "NES + VRC6",
        "YM2608 / OPNA",
        "YM2610 / OPNB",
        "NES + FDS",
        "NES + Sunsoft 5B",
        "NES + MMC5",
        "NES + VRC7",
        "Philips SAA1099"
    };
}

juce::StringArray accuracyChoices()
{
    return { "Inspired", "Hybrid", "Authentic" };
}

juce::StringArray macroChoices()
{
    return { "Manual", "Coin", "Bass", "Lead", "Arp", "Drum", "Hit", "Laser", "Jump", "Power-Up" };
}

juce::StringArray playModeChoices()
{
    return { "Big Mono", "Chip Poly" };
}

juce::StringArray waveShapeChoices()
{
    return {
        "Follow/RAM",
        "Tri",
        "Saw",
        "Pulse",
        "Noise/Steps",
        "Tri+Saw",
        "Tri+Pulse",
        "Saw+Pulse",
        "Tri+Saw+Pulse",
        "OPLL Horn",
        "OPLL Synth",
        "OPLL Harpsi",
        "OPLL Vibes",
        "OPLL Synth Bass",
        "OPLL Ac Bass",
        "OPLL E.Guitar"
    };
}

juce::StringArray sidVoiceWaveShapeChoices()
{
    return { "Follow", "Tri", "Saw", "Pulse", "Noise", "Tri+Saw", "Tri+Pulse", "Saw+Pulse", "Tri+Saw+Pulse" };
}

juce::StringArray sidAdsrNibbleChoices()
{
    return {
        "Follow",
        "0", "1", "2", "3",
        "4", "5", "6", "7",
        "8", "9", "10", "11",
        "12", "13", "14", "15"
    };
}

juce::StringArray sidFilterRoutingChoices()
{
    return {
        "Follow",
        "All",
        "V1",
        "V2",
        "V3",
        "V1+V2",
        "V1+V3",
        "V2+V3",
        "None"
    };
}

juce::StringArray pulse2DutyChoices()
{
    return { "Preset", "12.5%", "25%", "50%", "75%" };
}

juce::StringArray dmgWaveLevelChoices()
{
    return { "Follow", "Mute", "100%", "50%", "25%" };
}

juce::StringArray dmgStereoRouteChoices()
{
    return { "Follow", "Both", "Left", "Right", "Split" };
}

juce::StringArray ymEnvelopeShapeChoices()
{
    return {
        "Fixed / Follow",
        "Fall / LP",
        "Rise / BP",
        "Saw / HP",
        "Triangle / Off",
        "Code 0x00 / Notch",
        "Code 0x01 / LP+BP",
        "Code 0x02 / BP+HP",
        "Code 0x03 / All",
        "Code 0x04",
        "Code 0x05",
        "Code 0x06",
        "Code 0x07",
        "Code 0x08",
        "Code 0x09",
        "Code 0x0A",
        "Code 0x0B",
        "Code 0x0C",
        "Code 0x0D",
        "Code 0x0E",
        "Code 0x0F"
    };
}

juce::StringArray ymChannelMixChoices()
{
    return { "Follow", "Tone", "Noise", "Both", "Off" };
}

juce::StringArray snNoiseModeChoices()
{
    return { "Follow", "Mode 1", "Mode 2", "Mode 3", "Mode 4" };
}

juce::StringArray fmOperatorMultiplierChoices()
{
    return {
        "Follow",
        "0.5x",
        "1x",
        "2x",
        "3x",
        "4x",
        "5x",
        "6x",
        "7x",
        "8x",
        "9x",
        "10x",
        "11x",
        "12x",
        "13x",
        "14x",
        "15x"
    };
}

juce::StringArray fmOperatorAttackRateChoices()
{
    juce::StringArray choices;
    choices.add("Follow");
    for (int rate = 0; rate <= 31; ++rate)
        choices.add(juce::String(rate));
    return choices;
}

juce::StringArray fmOperatorDecayRateChoices()
{
    juce::StringArray choices;
    choices.add("Follow");
    for (int rate = 0; rate <= 31; ++rate)
        choices.add(juce::String(rate));
    return choices;
}

juce::StringArray fmOperatorSustainRateChoices()
{
    juce::StringArray choices;
    choices.add("Follow");
    for (int rate = 0; rate <= 31; ++rate)
        choices.add(juce::String(rate));
    return choices;
}

juce::StringArray fmOperatorReleaseRateChoices()
{
    juce::StringArray choices;
    choices.add("Follow");
    for (int rate = 0; rate <= 15; ++rate)
        choices.add(juce::String(rate));
    return choices;
}

juce::StringArray nesDmcRateChoices()
{
    return {
        "0 4.18 kHz",
        "1 4.71 kHz",
        "2 5.26 kHz",
        "3 5.59 kHz",
        "4 6.26 kHz",
        "5 7.05 kHz",
        "6 7.92 kHz",
        "7 8.36 kHz",
        "8 9.42 kHz",
        "9 11.19 kHz",
        "10 12.60 kHz",
        "11 13.98 kHz",
        "12 16.88 kHz",
        "13 21.10 kHz",
        "14 24.86 kHz",
        "15 33.14 kHz"
    };
}

juce::StringArray nesDmcPlaybackModeChoices()
{
    return { "Manual Slot", "Note Map", "Sample Map Only" };
}

juce::StringArray samplePlaybackModeChoices(ChipMode mode)
{
    if (mode == ChipMode::spc700)
        return { "Manual Slot", "Note Map", "Drum Map" };

    if (mode == ChipMode::paula)
        return { "Manual Slot", "Key Map", "Tracker Map" };

    return nesDmcPlaybackModeChoices();
}

juce::StringArray midiNoteChoices()
{
    static constexpr std::array<const char*, 12> names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::StringArray choices;
    for (int note = 0; note < 128; ++note)
        choices.add(juce::String(names[static_cast<size_t>(note % 12)]) + juce::String((note / 12) - 2));

    return choices;
}

const MidiCcMappingList& midiCcMappings()
{
    return ::chipper::midiCcMappings();
}

const char* parameterIdForMidiController(int controllerNumber)
{
    return ::chipper::parameterIdForMidiController(controllerNumber);
}

int midiControllerForParameterId(std::string_view parameterId)
{
    return ::chipper::midiControllerForParameterId(parameterId);
}

const char* parameterIdForChipParameterRole(ChipParameterRole role)
{
    return ::chipper::parameterIdForChipParameterRole(role);
}

int midiControllerForChipParameterRole(ChipParameterRole role)
{
    return ::chipper::midiControllerForChipParameterRole(role);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::chipMode, 1 },
        "Chip Mode",
        chipModeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::accuracy, 1 },
        "Behavior Strictness",
        accuracyChoices(),
        1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::clockHz, 1 },
        "Clock Rate Override",
        juce::NormalisableRange<float> { 0.0f, 8000000.0f, 1.0f, 0.35f },
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::outputDb, 1 },
        "Output Level",
        juce::NormalisableRange<float> { -48.0f, 6.0f, 0.01f },
        -9.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::macro, 1 },
        "Preset Recipe",
        macroChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::playMode, 1 },
        "Play Mode",
        playModeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::macroControl1, 1 },
        "Native Control 1",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::macroControl2, 1 },
        "Native Control 2",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::macroControl3, 1 },
        "Native Control 3",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::macroControl4, 1 },
        "Native Control 4",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::fmOperator1Level, 1 },
        "FM Operator 1 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::fmOperator2Level, 1 },
        "FM Operator 2 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::fmOperator3Level, 1 },
        "FM Operator 3 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::fmOperator4Level, 1 },
        "FM Operator 4 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator1Multiplier, 1 },
        "FM Operator 1 Multiplier",
        fmOperatorMultiplierChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator2Multiplier, 1 },
        "FM Operator 2 Multiplier",
        fmOperatorMultiplierChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator3Multiplier, 1 },
        "FM Operator 3 Multiplier",
        fmOperatorMultiplierChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator4Multiplier, 1 },
        "FM Operator 4 Multiplier",
        fmOperatorMultiplierChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator1AttackRate, 1 },
        "FM Operator 1 Attack Rate",
        fmOperatorAttackRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator2AttackRate, 1 },
        "FM Operator 2 Attack Rate",
        fmOperatorAttackRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator3AttackRate, 1 },
        "FM Operator 3 Attack Rate",
        fmOperatorAttackRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator4AttackRate, 1 },
        "FM Operator 4 Attack Rate",
        fmOperatorAttackRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator1DecayRate, 1 },
        "FM Operator 1 Decay Rate",
        fmOperatorDecayRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator2DecayRate, 1 },
        "FM Operator 2 Decay Rate",
        fmOperatorDecayRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator3DecayRate, 1 },
        "FM Operator 3 Decay Rate",
        fmOperatorDecayRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator4DecayRate, 1 },
        "FM Operator 4 Decay Rate",
        fmOperatorDecayRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator1SustainRate, 1 },
        "FM Operator 1 Sustain Rate",
        fmOperatorSustainRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator2SustainRate, 1 },
        "FM Operator 2 Sustain Rate",
        fmOperatorSustainRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator3SustainRate, 1 },
        "FM Operator 3 Sustain Rate",
        fmOperatorSustainRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator4SustainRate, 1 },
        "FM Operator 4 Sustain Rate",
        fmOperatorSustainRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator1ReleaseRate, 1 },
        "FM Operator 1 Release Rate",
        fmOperatorReleaseRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator2ReleaseRate, 1 },
        "FM Operator 2 Release Rate",
        fmOperatorReleaseRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator3ReleaseRate, 1 },
        "FM Operator 3 Release Rate",
        fmOperatorReleaseRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::fmOperator4ReleaseRate, 1 },
        "FM Operator 4 Release Rate",
        fmOperatorReleaseRateChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source1Enabled, 1 },
        "Source 1 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source2Enabled, 1 },
        "Source 2 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source3Enabled, 1 },
        "Source 3 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source4Enabled, 1 },
        "Source 4 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source5Enabled, 1 },
        "Source 5 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source6Enabled, 1 },
        "Source 6 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source7Enabled, 1 },
        "Source 7 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source8Enabled, 1 },
        "Source 8 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::source9Enabled, 1 },
        "Source 9 Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source1Level, 1 },
        "Source 1 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source2Level, 1 },
        "Source 2 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source3Level, 1 },
        "Source 3 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source4Level, 1 },
        "Source 4 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source5Level, 1 },
        "Source 5 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source6Level, 1 },
        "Source 6 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source7Level, 1 },
        "Source 7 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source8Level, 1 },
        "Source 8 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::source9Level, 1 },
        "Source 9 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::stereoSpread, 1 },
        "Stereo Spread",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::envelopeDecay, 1 },
        "Envelope Decay",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidAttack, 1 },
        "SID Voice 1 Attack",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidDecay, 1 },
        "SID Voice 1 Decay",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidSustain, 1 },
        "SID Voice 1 Sustain",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidRelease, 1 },
        "SID Voice 1 Release",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice2Attack, 1 },
        "SID Voice 2 Attack",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice2Decay, 1 },
        "SID Voice 2 Decay",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice2Sustain, 1 },
        "SID Voice 2 Sustain",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice2Release, 1 },
        "SID Voice 2 Release",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice3Attack, 1 },
        "SID Voice 3 Attack",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice3Decay, 1 },
        "SID Voice 3 Decay",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice3Sustain, 1 },
        "SID Voice 3 Sustain",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice3Release, 1 },
        "SID Voice 3 Release",
        sidAdsrNibbleChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidFilterRouting, 1 },
        "SID Filter Routing",
        sidFilterRoutingChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::sidVoice2PulseWidth, 1 },
        "SID Voice 2 Pulse Width",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::sidVoice3PulseWidth, 1 },
        "SID Voice 3 Pulse Width",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::waveShape, 1 },
        "Wave Shape",
        waveShapeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice2WaveShape, 1 },
        "SID Voice 2 Waveform",
        sidVoiceWaveShapeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::sidVoice3WaveShape, 1 },
        "SID Voice 3 Waveform",
        sidVoiceWaveShapeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::pulse2Duty, 1 },
        "Pulse 2 Duty",
        pulse2DutyChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::dmgWaveLevel, 1 },
        "DMG Wave Level",
        dmgWaveLevelChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::dmgStereoRoute, 1 },
        "Chip Choice / Route",
        dmgStereoRouteChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::ymEnvelopeShape, 1 },
        "YM Envelope Shape",
        ymEnvelopeShapeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::ymChannelAMix, 1 },
        "YM Channel A Mix",
        ymChannelMixChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::ymChannelBMix, 1 },
        "YM Channel B Mix",
        ymChannelMixChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::ymChannelCMix, 1 },
        "YM Channel C Mix",
        ymChannelMixChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::snNoiseMode, 1 },
        "Noise Mode",
        snNoiseModeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::nesDmcDirectLevel, 1 },
        "NES DMC Direct Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { id::nesDmcSampleSlot, 1 },
        "Sample Slot",
        0,
        31,
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::nesDmcRateIndex, 1 },
        "NES DMC Rate",
        nesDmcRateChoices(),
        15));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::nesDmcPlaybackMode, 1 },
        "Sample Playback Mode",
        nesDmcPlaybackModeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::nesDmcMapRoot, 1 },
        "Sample Bank Root",
        midiNoteChoices(),
        36));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id::nesDmcLoop, 1 },
        "NES DMC Loop",
        false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::spc700LoopStart, 1 },
        "SPC700 Loop Start",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::spc700LoopEnd, 1 },
        "SPC700 Loop End",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    const std::array<const char*, 8> spc700VoiceSampleIds {
        id::spc700Voice1SampleSlot,
        id::spc700Voice2SampleSlot,
        id::spc700Voice3SampleSlot,
        id::spc700Voice4SampleSlot,
        id::spc700Voice5SampleSlot,
        id::spc700Voice6SampleSlot,
        id::spc700Voice7SampleSlot,
        id::spc700Voice8SampleSlot
    };

    for (size_t i = 0; i < spc700VoiceSampleIds.size(); ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { spc700VoiceSampleIds[i], 1 },
            "Sample Voice " + juce::String(static_cast<int>(i + 1u)) + " Slot",
            0,
            32,
            0));
    }

    return { params.begin(), params.end() };
}

ChipMode chipModeFromChoice(int choice)
{
    switch (choice)
    {
        case 0: return ChipMode::nes;
        case 1: return ChipMode::dmg;
        case 2: return ChipMode::sid;
        case 3: return ChipMode::ym2149;
        case 4: return ChipMode::sn76489;
        case 5: return ChipMode::ym2612;
        case 6: return ChipMode::opl3;
        case 7: return ChipMode::spc700;
        case 8: return ChipMode::pokey;
        case 9: return ChipMode::paula;
        case 10: return ChipMode::huc6280;
        case 11: return ChipMode::namcoWsg;
        case 12: return ChipMode::ym2151;
        case 13: return ChipMode::ym2413;
        case 14: return ChipMode::scc;
        case 15: return ChipMode::ym2203;
        case 16: return ChipMode::nesVrc6;
        case 17: return ChipMode::ym2608;
        case 18: return ChipMode::ym2610;
        case 19: return ChipMode::nesFds;
        case 20: return ChipMode::nesSunsoft5b;
        case 21: return ChipMode::nesMmc5;
        case 22: return ChipMode::nesVrc7;
        case 23: return ChipMode::saa1099;
        default: return ChipMode::nes;
    }
}

AccuracyMode accuracyFromChoice(int choice)
{
    switch (choice)
    {
        case 0: return AccuracyMode::inspired;
        case 2: return AccuracyMode::authentic;
        case 1:
        default: return AccuracyMode::hybrid;
    }
}

MacroKind macroFromChoice(int choice)
{
    switch (choice)
    {
        case 1: return MacroKind::coin;
        case 2: return MacroKind::bass;
        case 3: return MacroKind::lead;
        case 4: return MacroKind::arp;
        case 5: return MacroKind::drum;
        case 6: return MacroKind::hit;
        case 7: return MacroKind::laser;
        case 8: return MacroKind::jump;
        case 9: return MacroKind::powerUp;
        case 0:
        default: return MacroKind::manual;
    }
}

PlayMode playModeFromChoice(int choice)
{
    switch (choice)
    {
        case 1: return PlayMode::chipPoly;
        case 0:
        default: return PlayMode::stack;
    }
}

double defaultClockForMode(ChipMode mode)
{
    switch (mode)
    {
        case ChipMode::nes:
        case ChipMode::nesVrc6:
        case ChipMode::nesFds:
        case ChipMode::nesSunsoft5b:
        case ChipMode::nesMmc5:
        case ChipMode::nesVrc7:
            return 1789773.0;
        case ChipMode::dmg: return 4194304.0;
        case ChipMode::sid: return 985248.0;
        case ChipMode::ym2149: return 2000000.0;
        case ChipMode::sn76489: return 3579545.0;
        case ChipMode::ym2612: return 7670454.0;
        case ChipMode::opl3: return 14318180.0;
        case ChipMode::spc700: return 32000.0;
        case ChipMode::pokey: return 1789790.0;
        case ChipMode::paula: return 3546895.0;
        case ChipMode::huc6280: return 3579545.0;
        case ChipMode::namcoWsg: return 96000.0;
        case ChipMode::ym2151: return 3579545.0;
        case ChipMode::ym2413: return 3579545.0;
        case ChipMode::scc: return 3579545.0;
        case ChipMode::ym2203: return 3993600.0;
        case ChipMode::ym2608: return 7987200.0;
        case ChipMode::ym2610: return 8000000.0;
        case ChipMode::saa1099: return 8000000.0;
    }

    return 3579545.0;
}

} // namespace chipper::parameters
