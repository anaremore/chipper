#pragma once

#include "Engine/ChipCore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace chipper
{

struct ControlDescriptor
{
    std::string id;
    std::string label;
    std::string group;
    std::string help;
};

enum class ParameterKind
{
    continuous,
    discreteChoice,
    steppedNumeric,
    booleanToggle,
    macro,
    chipRegister,
    hiddenInternal
};

enum class ControlSurface
{
    slider,
    steppedSlider,
    segmentedChoice,
    sourceCards,
    toggle,
    menu,
    hidden
};

enum class EnvelopeModel
{
    none,
    nativeAdsr,
    nativeOperatorEg,
    nativeNonAdsr,
    chipperAmpHelper
};

enum class ChipParameterRole
{
    macroControl1,
    macroControl2,
    macroControl3,
    macroControl4,
    source1Enabled,
    source2Enabled,
    source3Enabled,
    source4Enabled,
    source5Enabled,
    source6Enabled,
    source7Enabled,
    source8Enabled,
    source9Enabled,
    source1Level,
    source2Level,
    source3Level,
    source4Level,
    source5Level,
    source6Level,
    source7Level,
    source8Level,
    source9Level,
    stereoSpread,
    sidFilterRouting,
    sidVoice2PulseWidth,
    sidVoice3PulseWidth,
    envelopeDecay,
    sidAttack,
    sidDecay,
    sidSustain,
    sidRelease,
    sidVoice2Attack,
    sidVoice2Decay,
    sidVoice2Sustain,
    sidVoice2Release,
    sidVoice3Attack,
    sidVoice3Decay,
    sidVoice3Sustain,
    sidVoice3Release,
    waveShape,
    sidVoice2WaveShape,
    sidVoice3WaveShape,
    pulse2Duty,
    dmgWaveLevel,
    dmgStereoRoute,
    ymEnvelopeShape,
    ymChannelAMix,
    ymChannelBMix,
    ymChannelCMix,
    snNoiseMode,
    nesDmcDirectLevel,
    nesDmcSampleSlot,
    nesDmcRateIndex,
    nesDmcPlaybackMode,
    nesDmcMapRoot,
    nesDmcLoop,
    spc700LoopStart,
    spc700LoopEnd,
    spc700Voice1SampleSlot,
    spc700Voice2SampleSlot,
    spc700Voice3SampleSlot,
    spc700Voice4SampleSlot,
    spc700Voice5SampleSlot,
    spc700Voice6SampleSlot,
    spc700Voice7SampleSlot,
    spc700Voice8SampleSlot,
    fmOperator1Level,
    fmOperator2Level,
    fmOperator3Level,
    fmOperator4Level,
    fmOperator1Multiplier,
    fmOperator2Multiplier,
    fmOperator3Multiplier,
    fmOperator4Multiplier,
    fmOperator1AttackRate,
    fmOperator2AttackRate,
    fmOperator3AttackRate,
    fmOperator4AttackRate,
    fmOperator1DecayRate,
    fmOperator2DecayRate,
    fmOperator3DecayRate,
    fmOperator4DecayRate,
    fmOperator1SustainRate,
    fmOperator2SustainRate,
    fmOperator3SustainRate,
    fmOperator4SustainRate,
    fmOperator1ReleaseRate,
    fmOperator2ReleaseRate,
    fmOperator3ReleaseRate,
    fmOperator4ReleaseRate,
    clockHz,
    outputDb
};

struct ParameterChoiceSpec
{
    std::string label;
    std::string help;
    float normalizedValue = 0.0f;
    int choiceValue = 0;
};

struct FmEnvelopeRegisters
{
    uint8_t attackRate = 0x1f;
    uint8_t decayRate = 0x08;
    uint8_t sustainRate = 0x00;
    uint8_t sustainRelease = 0x46;
};

struct ChipParameterSpec
{
    ChipParameterRole role = ChipParameterRole::macroControl1;
    std::string id;
    std::string label;
    std::string group;
    std::string help;
    ParameterKind kind = ParameterKind::continuous;
    ControlSurface surface = ControlSurface::slider;
    std::vector<ParameterChoiceSpec> choices;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    bool automatable = true;
    bool authenticOnly = false;
    bool hybridOnly = false;
};

struct ModuleDescriptor
{
    std::string id;
    std::string title;
    std::string summary;
    std::vector<std::string> items;
};

struct AccuracyDisclosure
{
    std::string badge = "Unverified planned";
    std::string summary = "No accuracy claim: no audited audio core is integrated for this chip mode yet.";
    std::string evidence = "Renderer metadata exists, but audio behavior is not implemented for this chip mode.";
    std::vector<std::string> verifiedBehaviors;
    std::vector<std::string> knownGaps {
        "Audio core not integrated.",
        "Reference emulator and hardware comparison are not complete."
    };
    bool hardwareValidated = false;
    bool cycleAccurate = false;
};

struct MacroTemplate
{
    MacroKind macro = MacroKind::manual;
    std::string label;
    std::string help;
    std::array<float, 4> controls { 0.5f, 0.5f, 0.5f, 0.5f };
    std::array<bool, 4> sourceEnabled { true, true, true, true };
    float envelopeDecay = 0.0f;
    int waveShape = 0;
    int ymEnvelopeShape = 0;
    int snNoiseMode = 0;
    int dmgStereoRoute = 0;
    float stereoSpread = 0.0f;
    float nesDmcDirectLevel = 0.0f;
};

struct ChipDescriptor
{
    ChipMode mode = ChipMode::nes;
    std::string displayName;
    std::string summary;
    std::vector<ControlDescriptor> controls;
    std::array<ModuleDescriptor, 6> modules;
    std::vector<MacroTemplate> macros;
    bool implemented = false;
    bool supportsChipPoly = false;
    std::vector<ChipParameterSpec> parameters;
    AccuracyDisclosure verification;
};

const ChipDescriptor& descriptorFor(ChipMode mode);
const MacroTemplate& macroTemplateFor(ChipMode mode, MacroKind macro);
const ChipParameterSpec* parameterSpecFor(ChipMode mode, ChipParameterRole role);
EnvelopeModel envelopeModelFor(ChipMode mode);
const char* envelopeModelLabel(EnvelopeModel model);
bool chipHasParameterSurface(ChipMode mode, ChipParameterRole role, ControlSurface surface);
bool supportsPlayMode(ChipMode mode, PlayMode playMode);
size_t visibleSourceCountForMode(ChipMode mode);
size_t nativeSourceCountForMode(ChipMode mode);
bool hasInternalSourceLanes(ChipMode mode);
std::vector<ChipMode> chipModeOrder();
std::vector<MacroKind> macroOrder();
PatchConfig makePatchConfig(ChipMode mode,
                            MacroKind macro,
                            float control1,
                            float control2,
                            float control3,
                            float control4,
                            PlayMode playMode = PlayMode::stack,
                            std::array<bool, 9> sourceEnabled = { true, true, true, true, true, true, true, true, true },
                            std::array<float, 9> sourceLevels = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
                            float stereoSpread = 0.0f,
                            float envelopeDecay = 0.0f,
                            int waveShape = 0,
                            int pulse2Duty = 0,
                            int dmgWaveLevel = 0,
                            int dmgStereoRoute = 0,
                            int ymEnvelopeShape = 0,
                            int ymChannelAMix = 0,
                            int ymChannelBMix = 0,
                            int ymChannelCMix = 0,
                            int snNoiseMode = 0,
                            int sidVoice2WaveShape = 0,
                            int sidVoice3WaveShape = 0,
                            int sidAttack = 0,
                            int sidDecay = 0,
                            int sidSustain = 0,
                            int sidRelease = 0,
                            int sidVoice2Attack = 0,
                            int sidVoice2Decay = 0,
                            int sidVoice2Sustain = 0,
                            int sidVoice2Release = 0,
                            int sidVoice3Attack = 0,
                            int sidVoice3Decay = 0,
                            int sidVoice3Sustain = 0,
                            int sidVoice3Release = 0,
                            int sidFilterRouting = 0,
                            float sidVoice2PulseWidth = 0.5f,
                            float sidVoice3PulseWidth = 0.5f,
                            float nesDmcDirectLevel = 0.0f,
                            int nesDmcRateIndex = 15,
                            bool nesDmcLoop = false,
                            bool nesDmcOnly = false,
                            float spc700LoopStart = 0.0f,
                            float spc700LoopEnd = 1.0f,
                            std::array<int, 8> spc700VoiceSampleSlots = { 0, 0, 0, 0, 0, 0, 0, 0 },
                            std::array<float, 4> fmOperatorLevels = { 0.5f, 0.5f, 0.5f, 0.5f },
                            std::array<int, 4> fmOperatorMultipliers = { 0, 0, 0, 0 },
                            std::array<int, 4> fmOperatorAttackRates = { 0, 0, 0, 0 },
                            std::array<int, 4> fmOperatorDecayRates = { 0, 0, 0, 0 },
                            std::array<int, 4> fmOperatorSustainRates = { 0, 0, 0, 0 },
                            std::array<int, 4> fmOperatorReleaseRates = { 0, 0, 0, 0 });
uint8_t nesNoiseRegisterForPatch(const PatchConfig& patch);
uint8_t nesNoisePeriodForControl(float noisePeriodControl);
uint8_t nesDmcDirectLevelForControl(float levelControl);
uint8_t dmgSweepShiftForControl(float sweepControl);
uint8_t dmgSweepRegisterForControl(float sweepControl);
uint8_t dmgInitialEnvelopeLevelForControl(float envelopeLevelControl);
uint8_t dmgNoiseClockShiftForControl(float noiseClockControl);
uint8_t dmgNoiseRegisterForPatch(const PatchConfig& patch);
uint8_t dmgWaveOutputLevelBitsForPatch(const PatchConfig& patch, float velocity, bool velocitySensitive);
uint8_t dmgStereoRouteRegisterForPatch(const PatchConfig& patch);
uint16_t sidPulseWidthForControl(float pulseWidthControl);
uint16_t sidPulseWidthForVoice(const PatchConfig& patch, size_t voice);
uint8_t sidWaveformControlForPatch(const PatchConfig& patch);
uint8_t sidWaveformControlForVoice(const PatchConfig& patch, size_t voice);
uint8_t sidFilterModeBitsForPatch(const PatchConfig& patch);
uint8_t sidFilterRoutingBitsForPatch(const PatchConfig& patch);
uint8_t sidFilterResonanceForControl(float resonanceControl);
uint8_t sidModulationBitsForPatch(const PatchConfig& patch, size_t voice);
int sidModelChoiceForPatch(const PatchConfig& patch);
int sidModelNumberForPatch(const PatchConfig& patch);
uint8_t sidAttackNibbleForPatch(const PatchConfig& patch);
uint8_t sidDecayNibbleForPatch(const PatchConfig& patch);
uint8_t sidSustainNibbleForPatch(const PatchConfig& patch);
uint8_t sidReleaseNibbleForPatch(const PatchConfig& patch);
uint8_t sidAttackDecayForPatch(const PatchConfig& patch);
uint8_t sidSustainReleaseForPatch(const PatchConfig& patch);
uint8_t sidAttackNibbleForVoice(const PatchConfig& patch, size_t voice);
uint8_t sidDecayNibbleForVoice(const PatchConfig& patch, size_t voice);
uint8_t sidSustainNibbleForVoice(const PatchConfig& patch, size_t voice);
uint8_t sidReleaseNibbleForVoice(const PatchConfig& patch, size_t voice);
uint8_t sidAttackDecayForVoice(const PatchConfig& patch, size_t voice);
uint8_t sidSustainReleaseForVoice(const PatchConfig& patch, size_t voice);
double sidAttackSecondsForNibble(uint8_t nibble);
double sidDecayReleaseSecondsForNibble(uint8_t nibble);
uint8_t ym2149NoisePeriodForControl(float noisePitchControl);
uint8_t ym2149MixerRegisterForControl(float toneNoiseControl);
uint8_t ym2149MixerRegisterWithChannelOverrides(const PatchConfig& patch, uint8_t macroMixer);
int ym2149ChannelMixChoiceForPatch(const PatchConfig& patch, size_t channel);
uint8_t ym2149EnvelopeShapeCodeForChoice(int shapeChoice);
uint16_t ym2149EnvelopePeriodForControl(float envelopeControl);
uint8_t opnSsgNoisePeriodForPatch(const PatchConfig& patch);
uint8_t opnSsgMixerRegisterForPatch(const PatchConfig& patch);
int opnSsgEnvelopeChoiceForPatch(const PatchConfig& patch);
bool opnSsgEnvelopeEnabledForPatch(const PatchConfig& patch);
uint8_t opnSsgEnvelopeShapeCodeForPatch(const PatchConfig& patch);
uint8_t sn76489NoiseAttenuationForControl(float noiseLevelControl);
uint8_t sn76489NoiseControlForPatch(const PatchConfig& patch);
uint8_t spc700NoiseModeForPatch(const PatchConfig& patch);
uint8_t spc700NoiseClockForPatch(const PatchConfig& patch);
uint8_t pokeyAudcForPatch(const PatchConfig& patch);
uint8_t pokeyFilterChoiceForPatch(const PatchConfig& patch);
uint8_t pokeyFilterBitsForPatch(const PatchConfig& patch);
uint8_t pokeyAudctlForPatch(const PatchConfig& patch);
uint8_t pokeyAudfForNote(double clockHz, int midiNote);
uint8_t ym2612AlgorithmForPatch(const PatchConfig& patch);
uint8_t ym2612DacModeForPatch(const PatchConfig& patch);
uint8_t ym2612PanBitsForPatch(const PatchConfig& patch, size_t channel);
uint8_t ym2612LfoRateForPatch(const PatchConfig& patch);
uint8_t ym2612LfoRegisterForPatch(const PatchConfig& patch);
uint8_t ym2612LfoAmSensitivityForPatch(const PatchConfig& patch);
uint8_t ym2612LfoPmSensitivityForPatch(const PatchConfig& patch);
uint8_t ym2612LfoChannelBitsForPatch(const PatchConfig& patch);
uint8_t ym2612ChannelControlForPatch(const PatchConfig& patch, size_t channel);
bool ym2612OperatorAmEnabledForPatch(const PatchConfig& patch, size_t op);
FmEnvelopeRegisters ym2612EnvelopeRegistersForPatch(const PatchConfig& patch, size_t op);
uint8_t ym2151AlgorithmForPatch(const PatchConfig& patch);
uint8_t ym2151PanBitsForPatch(const PatchConfig& patch, size_t channel);
uint8_t ym2151NoiseRegisterForPatch(const PatchConfig& patch);
uint8_t ym2151LfoRateForPatch(const PatchConfig& patch);
uint8_t ym2151LfoAmDepthForPatch(const PatchConfig& patch);
uint8_t ym2151LfoPmDepthForPatch(const PatchConfig& patch);
uint8_t ym2151LfoWaveformForPatch(const PatchConfig& patch);
uint8_t ym2151LfoPmSensitivityForPatch(const PatchConfig& patch);
uint8_t ym2151LfoAmSensitivityForPatch(const PatchConfig& patch);
uint8_t ym2151LfoChannelRegisterForPatch(const PatchConfig& patch);
bool ym2151OperatorAmEnabledForPatch(const PatchConfig& patch, size_t op);
uint8_t fmFeedbackForPatch(const PatchConfig& patch);
uint8_t fmOperatorMultipleForPatch(ChipMode mode, const PatchConfig& patch, size_t op);
bool fmOperatorIsCarrierForAlgorithm(uint8_t algorithm, size_t op);
uint8_t fmOperatorTotalLevelForPatch(ChipMode mode, const PatchConfig& patch, size_t op, float velocity = 1.0f);
uint8_t oplWaveformForPatch(const PatchConfig& patch);
uint8_t oplConnectionForPatch(const PatchConfig& patch);
uint8_t oplModulatorMultipleForPatch(const PatchConfig& patch);
uint8_t oplModulatorTotalLevelForPatch(const PatchConfig& patch);
uint8_t oplCarrierTotalLevelForPatch(const PatchConfig& patch, float velocity = 1.0f);
uint8_t oplRhythmModeForPatch(const PatchConfig& patch);
bool opl18ChannelLayerForPatch(const PatchConfig& patch);
bool oplFourOperatorPairForPatch(const PatchConfig& patch);
uint8_t oplFourOperatorEnableRegisterForPatch(const PatchConfig& patch);
bool opllCustomPatchEnabledForPatch(const PatchConfig& patch);
uint8_t opllOperatorMultipleForPatch(const PatchConfig& patch, size_t op);
uint8_t opllModulatorTotalLevelForPatch(const PatchConfig& patch);
uint8_t opllFeedbackForPatch(const PatchConfig& patch);
uint8_t opllOperatorAttackRateForPatch(const PatchConfig& patch, size_t op);
uint8_t opllOperatorDecayRateForPatch(const PatchConfig& patch, size_t op);
uint8_t opllOperatorSustainLevelForPatch(const PatchConfig& patch, size_t op);
uint8_t opllOperatorReleaseRateForPatch(const PatchConfig& patch, size_t op);
std::array<uint8_t, 8> opllCustomPatchBytesForPatch(const PatchConfig& patch);
uint8_t ym2413InstrumentForPatch(const PatchConfig& patch);
uint8_t ym2413RhythmModeForPatch(const PatchConfig& patch);
uint8_t ym2413VolumeNibbleForPatch(const PatchConfig& patch, size_t channel, float velocity = 1.0f);
uint8_t vrc7InstrumentForPatch(const PatchConfig& patch);
uint8_t vrc7VolumeNibbleForPatch(const PatchConfig& patch, size_t channel, float velocity = 1.0f);
uint8_t huc6280ControlForPatch(const PatchConfig& patch, size_t channel);
uint8_t wavetableWaveShapeForChannel(ChipMode mode, const PatchConfig& patch, size_t channel);
uint8_t huc6280WaveShapeForChannel(const PatchConfig& patch, size_t channel);
bool huc6280ChannelUsesNoiseForPatch(const PatchConfig& patch, size_t channel);
uint8_t huc6280LfoModeForPatch(const PatchConfig& patch);
uint8_t sccVolumeForPatch(const PatchConfig& patch, size_t channel);
bool sccChannelKeyOnForPatch(const PatchConfig& patch, size_t channel);
uint8_t namcoWsgVolumeForPatch(const PatchConfig& patch, size_t channel);
bool namcoWsgChannelEnabledForPatch(const PatchConfig& patch, size_t channel);
uint8_t wavetableRamSampleForPatch(ChipMode mode, const PatchConfig& patch, size_t channel, size_t sampleIndex);
uint8_t sampleTemplateForPatch(ChipMode mode, const PatchConfig& patch);
int8_t generatedSampleValueForPatch(ChipMode mode, const PatchConfig& patch, size_t channel, size_t sampleIndex);
uint8_t spc700SamplePlaybackModeForPatch(const PatchConfig& patch);
bool spc700SampleLoopsForPatch(const PatchConfig& patch);
uint8_t spc700VoiceVolumeForPatch(const PatchConfig& patch, size_t voice, float velocity = 1.0f);
uint8_t spc700EnvelopeShapeForPatch(const PatchConfig& patch);
uint8_t spc700AdsrForPatch(const PatchConfig& patch);
uint8_t spc700GainForPatch(const PatchConfig& patch);
bool spc700VoiceEnabledForPatch(const PatchConfig& patch, size_t voice);
uint8_t paulaChannelVolumeForPatch(const PatchConfig& patch, size_t channel, float velocity = 1.0f);
bool paulaLoopForPatch(const PatchConfig& patch);
uint8_t paulaControlForPatch(const PatchConfig& patch, size_t channel, float velocity = 1.0f);
uint8_t paulaOutputFilterModeForPatch(const PatchConfig& patch);

} // namespace chipper
