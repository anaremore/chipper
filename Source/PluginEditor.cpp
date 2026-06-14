#include "PluginEditor.h"

#include "ChipperBuildInfo.h"
#include "Engine/ChipDescriptors.h"
#include "Presets.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace
{

constexpr int userPresetItemIdBase = 10000;
constexpr int initPresetItemId = 9000;

constexpr std::array chipSettingsSnapshotParameterIds {
    chipper::parameters::id::accuracy,
    chipper::parameters::id::clockHz,
    chipper::parameters::id::outputDb,
    chipper::parameters::id::macro,
    chipper::parameters::id::playMode,
    chipper::parameters::id::macroControl1,
    chipper::parameters::id::macroControl2,
    chipper::parameters::id::macroControl3,
    chipper::parameters::id::macroControl4,
    chipper::parameters::id::source1Enabled,
    chipper::parameters::id::source2Enabled,
    chipper::parameters::id::source3Enabled,
    chipper::parameters::id::source4Enabled,
    chipper::parameters::id::source5Enabled,
    chipper::parameters::id::source6Enabled,
    chipper::parameters::id::source7Enabled,
    chipper::parameters::id::source8Enabled,
    chipper::parameters::id::source9Enabled,
    chipper::parameters::id::source1Level,
    chipper::parameters::id::source2Level,
    chipper::parameters::id::source3Level,
    chipper::parameters::id::source4Level,
    chipper::parameters::id::source5Level,
    chipper::parameters::id::source6Level,
    chipper::parameters::id::source7Level,
    chipper::parameters::id::source8Level,
    chipper::parameters::id::source9Level,
    chipper::parameters::id::stereoSpread,
    chipper::parameters::id::sidFilterRouting,
    chipper::parameters::id::sidVoice2PulseWidth,
    chipper::parameters::id::sidVoice3PulseWidth,
    chipper::parameters::id::envelopeDecay,
    chipper::parameters::id::sidAttack,
    chipper::parameters::id::sidDecay,
    chipper::parameters::id::sidSustain,
    chipper::parameters::id::sidRelease,
    chipper::parameters::id::sidVoice2Attack,
    chipper::parameters::id::sidVoice2Decay,
    chipper::parameters::id::sidVoice2Sustain,
    chipper::parameters::id::sidVoice2Release,
    chipper::parameters::id::sidVoice3Attack,
    chipper::parameters::id::sidVoice3Decay,
    chipper::parameters::id::sidVoice3Sustain,
    chipper::parameters::id::sidVoice3Release,
    chipper::parameters::id::waveShape,
    chipper::parameters::id::sidVoice2WaveShape,
    chipper::parameters::id::sidVoice3WaveShape,
    chipper::parameters::id::pulse2Duty,
    chipper::parameters::id::dmgWaveLevel,
    chipper::parameters::id::dmgStereoRoute,
    chipper::parameters::id::ymEnvelopeShape,
    chipper::parameters::id::ymChannelAMix,
    chipper::parameters::id::ymChannelBMix,
    chipper::parameters::id::ymChannelCMix,
    chipper::parameters::id::snNoiseMode,
    chipper::parameters::id::nesDmcDirectLevel,
    chipper::parameters::id::nesDmcSampleSlot,
    chipper::parameters::id::nesDmcRateIndex,
    chipper::parameters::id::nesDmcPlaybackMode,
    chipper::parameters::id::nesDmcMapRoot,
    chipper::parameters::id::nesDmcLoop,
    chipper::parameters::id::spc700LoopStart,
    chipper::parameters::id::spc700LoopEnd,
    chipper::parameters::id::spc700Voice1SampleSlot,
    chipper::parameters::id::spc700Voice2SampleSlot,
    chipper::parameters::id::spc700Voice3SampleSlot,
    chipper::parameters::id::spc700Voice4SampleSlot,
    chipper::parameters::id::spc700Voice5SampleSlot,
    chipper::parameters::id::spc700Voice6SampleSlot,
    chipper::parameters::id::spc700Voice7SampleSlot,
    chipper::parameters::id::spc700Voice8SampleSlot
};

constexpr const char* chipperPluginVersionString =
#ifdef JucePlugin_VersionString
    JucePlugin_VersionString;
#else
    "0.1.0";
#endif

struct ChipUiTheme
{
    juce::Colour background;
    juce::Colour panel;
    juce::Colour panelAlt;
    juce::Colour outline;
    juce::Colour sourceCard;
    juce::Colour primary;
    juce::Colour accent;
    juce::Colour text;
    juce::Colour mutedText;
    juce::Colour darkText;
    float panelRadius = 6.0f;
    int textureStep = 0;
};

ChipUiTheme chipThemeFor(chipper::ChipMode mode)
{
    const auto base = ChipUiTheme {
        juce::Colour(0xff101414),
        juce::Colour(0xff182125),
        juce::Colour(0xff171c20),
        juce::Colour(0xff34474c),
        juce::Colour(0xff202c33),
        juce::Colour(0xfff0c94d),
        juce::Colour(0xff56c7d8),
        juce::Colour(0xffd9e1e8),
        juce::Colour(0xffaebbc4),
        juce::Colour(0xff101414),
        6.0f,
        0
    };

    switch (mode)
    {
        case chipper::ChipMode::nes:
            return { juce::Colour(0xff0f1112), juce::Colour(0xff171b1d), juce::Colour(0xff151819),
                     juce::Colour(0xff4f5a60), juce::Colour(0xff202426), juce::Colour(0xffff5a2f),
                     juce::Colour(0xff4fd3dd), juce::Colour(0xffececec), juce::Colour(0xffb9c0c4),
                     juce::Colour(0xff111111), 2.0f, 16 };
        case chipper::ChipMode::dmg:
            return { juce::Colour(0xff11160f), juce::Colour(0xff1a2218), juce::Colour(0xff151d14),
                     juce::Colour(0xff51624b), juce::Colour(0xff202a1f), juce::Colour(0xff9bd45d),
                     juce::Colour(0xff74d1b4), juce::Colour(0xffe5ecd8), juce::Colour(0xffb3c3aa),
                     juce::Colour(0xff101410), 3.0f, 12 };
        case chipper::ChipMode::sid:
            return { juce::Colour(0xff15120f), juce::Colour(0xff201a14), juce::Colour(0xff1a1511),
                     juce::Colour(0xff5a4b36), juce::Colour(0xff2b241c), juce::Colour(0xfff3d34a),
                     juce::Colour(0xff7fd6ff), juce::Colour(0xfff2e6c9), juce::Colour(0xffc9b993),
                     juce::Colour(0xff15120f), 3.0f, 20 };
        case chipper::ChipMode::ym2612:
        case chipper::ChipMode::opl3:
        case chipper::ChipMode::ym2151:
        case chipper::ChipMode::ym2413:
            return { juce::Colour(0xff101315), juce::Colour(0xff171f24), juce::Colour(0xff151b20),
                     juce::Colour(0xff394b55), juce::Colour(0xff202a31), juce::Colour(0xffffce54),
                     juce::Colour(0xff54c7ff), juce::Colour(0xffe3ebef), juce::Colour(0xffaebcc4),
                     juce::Colour(0xff101316), 5.0f, 24 };
        case chipper::ChipMode::spc700:
        case chipper::ChipMode::paula:
            return { juce::Colour(0xff111319), juce::Colour(0xff1a2028), juce::Colour(0xff161b22),
                     juce::Colour(0xff3b4859), juce::Colour(0xff202834), juce::Colour(0xffffc857),
                     juce::Colour(0xff79d7ff), juce::Colour(0xffe0e8f0), juce::Colour(0xffabb9c7),
                     juce::Colour(0xff101318), 6.0f, 20 };
        case chipper::ChipMode::pokey:
            return { juce::Colour(0xff121313), juce::Colour(0xff1d1f1f), juce::Colour(0xff171a1a),
                     juce::Colour(0xff4f5552), juce::Colour(0xff252827), juce::Colour(0xffffcf45),
                     juce::Colour(0xffff6a3a), juce::Colour(0xffe7e6df), juce::Colour(0xffbeb9aa),
                     juce::Colour(0xff111111), 3.0f, 12 };
        case chipper::ChipMode::ym2149:
        case chipper::ChipMode::sn76489:
            return { juce::Colour(0xff101317), juce::Colour(0xff182027), juce::Colour(0xff151b20),
                     juce::Colour(0xff364858), juce::Colour(0xff202a32), juce::Colour(0xff5fd3ff),
                     juce::Colour(0xffffd24d), juce::Colour(0xffe1e8ec), juce::Colour(0xffabb9c0),
                     juce::Colour(0xff101318), 3.0f, 14 };
        case chipper::ChipMode::huc6280:
        case chipper::ChipMode::namcoWsg:
        case chipper::ChipMode::scc:
            return { juce::Colour(0xff101418), juce::Colour(0xff18222a), juce::Colour(0xff151c22),
                     juce::Colour(0xff374c5a), juce::Colour(0xff202c34), juce::Colour(0xffffd55a),
                     juce::Colour(0xff5ed8c9), juce::Colour(0xffe1ece9), juce::Colour(0xffadc0bd),
                     juce::Colour(0xff101414), 5.0f, 18 };
    }

    return base;
}

juce::String compactSampleName(juce::String name, int maxChars = 28)
{
    name = name.trim();
    if (name.length() <= maxChars)
        return name;

    const auto dot = name.lastIndexOfChar('.');
    const auto hasShortExtension = dot > 0 && name.length() - dot <= 8;
    const auto extension = hasShortExtension ? name.substring(dot) : juce::String {};
    const auto baseName = hasShortExtension ? name.substring(0, dot) : name;
    const auto budget = std::max(12, maxChars - extension.length() - 3);
    const auto head = std::max(6, budget / 2);
    const auto tail = std::max(4, budget - head);
    const auto tailStart = std::max(0, baseName.length() - tail);

    return baseName.substring(0, head) + "..." + baseName.substring(tailStart) + extension;
}

void drawChipThemeTexture(juce::Graphics& g, juce::Rectangle<int> bounds, const ChipUiTheme& theme)
{
    if (theme.textureStep <= 0)
        return;

    g.setColour(theme.outline.withAlpha(0.12f));
    for (int x = bounds.getX(); x < bounds.getRight(); x += theme.textureStep)
        g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));

    if (theme.panelRadius <= 2.5f)
    {
        for (int y = bounds.getY(); y < bounds.getBottom(); y += theme.textureStep)
            g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
    }
}

void drawChipIdentityAccent(juce::Graphics& g, juce::Rectangle<int> bounds, chipper::ChipMode mode, const ChipUiTheme& theme)
{
    auto header = bounds.reduced(16).removeFromTop(62);
    const auto titleArea = header.removeFromLeft(230).toFloat();
    const auto logoRail = titleArea.withTrimmedLeft(2.0f)
                                   .withTrimmedRight(26.0f)
                                   .removeFromBottom(11.0f)
                                   .withHeight(4.0f)
                                   .translated(0.0f, 1.0f);
    const auto stripeY = logoRail.getY();
    const auto stripeX = logoRail.getX();
    const auto stripeW = logoRail.getWidth();

    switch (mode)
    {
        case chipper::ChipMode::sid:
        {
            static const std::array<juce::Colour, 6> c64Rainbow {
                juce::Colour(0xffd94a34), juce::Colour(0xffe58a2f), juce::Colour(0xfff3d34a),
                juce::Colour(0xff8fe36b), juce::Colour(0xff7fd6ff), juce::Colour(0xffb16bff)
            };
            const auto segmentW = stripeW / static_cast<float>(c64Rainbow.size());
            for (size_t i = 0; i < c64Rainbow.size(); ++i)
            {
                g.setColour(c64Rainbow[i].withAlpha(0.88f));
                g.fillRect(stripeX + (segmentW * static_cast<float>(i)), stripeY, segmentW + 0.5f, 4.0f);
            }
            break;
        }
        case chipper::ChipMode::nes:
        {
            g.setColour(theme.primary.withAlpha(0.92f));
            g.fillRect(stripeX, stripeY, stripeW * 0.64f, 4.0f);
            g.setColour(theme.accent.withAlpha(0.88f));
            g.fillRect(stripeX + (stripeW * 0.68f), stripeY, stripeW * 0.26f, 4.0f);
            break;
        }
        case chipper::ChipMode::dmg:
        {
            constexpr int blocks = 12;
            const auto blockW = stripeW / static_cast<float>(blocks);
            for (int i = 0; i < blocks; ++i)
            {
                g.setColour((i % 2 == 0 ? theme.primary : theme.accent).withAlpha(0.78f));
                g.fillRect(stripeX + blockW * static_cast<float>(i),
                           stripeY,
                           std::max(5.0f, blockW - 2.0f),
                           4.0f);
            }
            break;
        }
        case chipper::ChipMode::ym2612:
        case chipper::ChipMode::opl3:
        case chipper::ChipMode::ym2151:
        case chipper::ChipMode::ym2413:
        {
            g.setColour(theme.accent.withAlpha(0.88f));
            const auto y = stripeY + 2.0f;
            constexpr int nodes = 8;
            const auto nodeGap = stripeW / static_cast<float>(nodes - 1);
            g.drawLine(stripeX, y, stripeX + stripeW, y, 1.0f);
            for (int i = 0; i < nodes; ++i)
            {
                const auto x = stripeX + nodeGap * static_cast<float>(i);
                g.fillEllipse(x - 3.0f, y - 3.0f, 6.0f, 6.0f);
            }
            break;
        }
        case chipper::ChipMode::spc700:
        case chipper::ChipMode::paula:
        {
            juce::Path wave;
            wave.startNewSubPath(stripeX, stripeY + 2.0f);
            for (int i = 0; i <= 10; ++i)
            {
                const auto x = stripeX + (stripeW * static_cast<float>(i) / 10.0f);
                const auto y = stripeY + 2.0f + std::sin(static_cast<float>(i) * 1.4f) * 3.0f;
                wave.lineTo(x, y);
            }
            g.setColour(theme.accent.withAlpha(0.88f));
            g.strokePath(wave, juce::PathStrokeType(1.5f));
            break;
        }
        default:
            g.setColour(theme.primary.withAlpha(0.72f));
            g.fillRect(stripeX, stripeY, stripeW, 3.0f);
            break;
    }
}

int chipModeChoiceIndex(chipper::ChipMode mode)
{
    switch (mode)
    {
        case chipper::ChipMode::nes: return 0;
        case chipper::ChipMode::dmg: return 1;
        case chipper::ChipMode::sid: return 2;
        case chipper::ChipMode::ym2149: return 3;
        case chipper::ChipMode::sn76489: return 4;
        case chipper::ChipMode::ym2612: return 5;
        case chipper::ChipMode::opl3: return 6;
        case chipper::ChipMode::spc700: return 7;
        case chipper::ChipMode::pokey: return 8;
        case chipper::ChipMode::paula: return 9;
        case chipper::ChipMode::huc6280: return 10;
        case chipper::ChipMode::namcoWsg: return 11;
        case chipper::ChipMode::ym2151: return 12;
        case chipper::ChipMode::ym2413: return 13;
        case chipper::ChipMode::scc: return 14;
    }

    return 0;
}

int accuracyChoiceIndex(chipper::AccuracyMode mode)
{
    switch (mode)
    {
        case chipper::AccuracyMode::inspired: return 0;
        case chipper::AccuracyMode::hybrid: return 1;
        case chipper::AccuracyMode::authentic: return 2;
    }

    return 1;
}

int macroChoiceIndex(chipper::MacroKind macro)
{
    const auto order = chipper::macroOrder();
    const auto iter = std::find(order.begin(), order.end(), macro);
    return iter == order.end() ? 0 : static_cast<int>(std::distance(order.begin(), iter));
}

int playModeChoiceIndex(chipper::PlayMode mode)
{
    return mode == chipper::PlayMode::chipPoly ? 1 : 0;
}

chipper::ChipParameterRole macroControlRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 4> roles {
        chipper::ChipParameterRole::macroControl1,
        chipper::ChipParameterRole::macroControl2,
        chipper::ChipParameterRole::macroControl3,
        chipper::ChipParameterRole::macroControl4
    };

    return roles[std::min(index, roles.size() - 1u)];
}

chipper::ChipParameterRole sourceRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 9> roles {
        chipper::ChipParameterRole::source1Enabled,
        chipper::ChipParameterRole::source2Enabled,
        chipper::ChipParameterRole::source3Enabled,
        chipper::ChipParameterRole::source4Enabled,
        chipper::ChipParameterRole::source5Enabled,
        chipper::ChipParameterRole::source6Enabled,
        chipper::ChipParameterRole::source7Enabled,
        chipper::ChipParameterRole::source8Enabled,
        chipper::ChipParameterRole::source9Enabled
    };

    return roles[std::min(index, roles.size() - 1u)];
}

chipper::ChipParameterRole sourceLevelRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 9> roles {
        chipper::ChipParameterRole::source1Level,
        chipper::ChipParameterRole::source2Level,
        chipper::ChipParameterRole::source3Level,
        chipper::ChipParameterRole::source4Level,
        chipper::ChipParameterRole::source5Level,
        chipper::ChipParameterRole::source6Level,
        chipper::ChipParameterRole::source7Level,
        chipper::ChipParameterRole::source8Level,
        chipper::ChipParameterRole::source9Level
    };

    return roles[std::min(index, roles.size() - 1u)];
}

chipper::ChipParameterRole sidVoiceWaveRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 3> roles {
        chipper::ChipParameterRole::waveShape,
        chipper::ChipParameterRole::sidVoice2WaveShape,
        chipper::ChipParameterRole::sidVoice3WaveShape
    };

    return roles[std::min(index, roles.size() - 1u)];
}

const char* sidVoiceWaveParameterId(size_t index)
{
    static constexpr std::array<const char*, 3> ids {
        chipper::parameters::id::waveShape,
        chipper::parameters::id::sidVoice2WaveShape,
        chipper::parameters::id::sidVoice3WaveShape
    };

    return ids[std::min(index, ids.size() - 1u)];
}

chipper::ChipParameterRole hucVoiceWaveRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 8> roles {
        chipper::ChipParameterRole::waveShape,
        chipper::ChipParameterRole::sidVoice2WaveShape,
        chipper::ChipParameterRole::sidVoice3WaveShape,
        chipper::ChipParameterRole::pulse2Duty,
        chipper::ChipParameterRole::dmgWaveLevel,
        chipper::ChipParameterRole::snNoiseMode,
        chipper::ChipParameterRole::ymEnvelopeShape,
        chipper::ChipParameterRole::dmgStereoRoute
    };

    return roles[std::min(index, roles.size() - 1u)];
}

const char* hucVoiceWaveParameterId(size_t index)
{
    static constexpr std::array<const char*, 8> ids {
        chipper::parameters::id::waveShape,
        chipper::parameters::id::sidVoice2WaveShape,
        chipper::parameters::id::sidVoice3WaveShape,
        chipper::parameters::id::pulse2Duty,
        chipper::parameters::id::dmgWaveLevel,
        chipper::parameters::id::snNoiseMode,
        chipper::parameters::id::ymEnvelopeShape,
        chipper::parameters::id::dmgStereoRoute
    };

    return ids[std::min(index, ids.size() - 1u)];
}

bool usesChannelLocalWaveDeck(chipper::ChipMode mode)
{
    return mode == chipper::ChipMode::huc6280
        || mode == chipper::ChipMode::namcoWsg
        || mode == chipper::ChipMode::scc
        || mode == chipper::ChipMode::spc700
        || mode == chipper::ChipMode::paula;
}

chipper::ChipParameterRole spc700VoiceSampleRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 8> roles {
        chipper::ChipParameterRole::spc700Voice1SampleSlot,
        chipper::ChipParameterRole::spc700Voice2SampleSlot,
        chipper::ChipParameterRole::spc700Voice3SampleSlot,
        chipper::ChipParameterRole::spc700Voice4SampleSlot,
        chipper::ChipParameterRole::spc700Voice5SampleSlot,
        chipper::ChipParameterRole::spc700Voice6SampleSlot,
        chipper::ChipParameterRole::spc700Voice7SampleSlot,
        chipper::ChipParameterRole::spc700Voice8SampleSlot
    };

    return roles[std::min(index, roles.size() - 1u)];
}

const char* spc700VoiceSampleParameterId(size_t index)
{
    static constexpr std::array<const char*, 8> ids {
        chipper::parameters::id::spc700Voice1SampleSlot,
        chipper::parameters::id::spc700Voice2SampleSlot,
        chipper::parameters::id::spc700Voice3SampleSlot,
        chipper::parameters::id::spc700Voice4SampleSlot,
        chipper::parameters::id::spc700Voice5SampleSlot,
        chipper::parameters::id::spc700Voice6SampleSlot,
        chipper::parameters::id::spc700Voice7SampleSlot,
        chipper::parameters::id::spc700Voice8SampleSlot
    };

    return ids[std::min(index, ids.size() - 1u)];
}

chipper::ChipParameterRole sidVoicePulseWidthRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 3> roles {
        chipper::ChipParameterRole::macroControl1,
        chipper::ChipParameterRole::sidVoice2PulseWidth,
        chipper::ChipParameterRole::sidVoice3PulseWidth
    };

    return roles[std::min(index, roles.size() - 1u)];
}

const char* sidVoicePulseWidthParameterId(size_t index)
{
    static constexpr std::array<const char*, 3> ids {
        chipper::parameters::id::macroControl1,
        chipper::parameters::id::sidVoice2PulseWidth,
        chipper::parameters::id::sidVoice3PulseWidth
    };

    return ids[std::min(index, ids.size() - 1u)];
}

chipper::ChipParameterRole sidAdsrRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 12> roles {
        chipper::ChipParameterRole::sidAttack,
        chipper::ChipParameterRole::sidDecay,
        chipper::ChipParameterRole::sidSustain,
        chipper::ChipParameterRole::sidRelease,
        chipper::ChipParameterRole::sidVoice2Attack,
        chipper::ChipParameterRole::sidVoice2Decay,
        chipper::ChipParameterRole::sidVoice2Sustain,
        chipper::ChipParameterRole::sidVoice2Release,
        chipper::ChipParameterRole::sidVoice3Attack,
        chipper::ChipParameterRole::sidVoice3Decay,
        chipper::ChipParameterRole::sidVoice3Sustain,
        chipper::ChipParameterRole::sidVoice3Release
    };

    return roles[std::min(index, roles.size() - 1u)];
}

const char* sidAdsrParameterId(size_t index)
{
    static constexpr std::array<const char*, 12> ids {
        chipper::parameters::id::sidAttack,
        chipper::parameters::id::sidDecay,
        chipper::parameters::id::sidSustain,
        chipper::parameters::id::sidRelease,
        chipper::parameters::id::sidVoice2Attack,
        chipper::parameters::id::sidVoice2Decay,
        chipper::parameters::id::sidVoice2Sustain,
        chipper::parameters::id::sidVoice2Release,
        chipper::parameters::id::sidVoice3Attack,
        chipper::parameters::id::sidVoice3Decay,
        chipper::parameters::id::sidVoice3Sustain,
        chipper::parameters::id::sidVoice3Release
    };

    return ids[std::min(index, ids.size() - 1u)];
}

class DmcSampleBankEditorComponent final : public juce::Component
{
public:
    DmcSampleBankEditorComponent(ChipperAudioProcessor& processor, bool editsSpc700Brr, bool editsPaulaSamples, std::function<void()> refreshCallback)
        : audioProcessor(processor), spc700Mode(editsSpc700Brr), paulaMode(editsPaulaSamples), onRefresh(std::move(refreshCallback))
    {
        title.setText(paulaMode ? "Paula Sample Bank" : (spc700Mode ? "SPC700 Sample Bank" : "DMC Sample Bank"), juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centredLeft);
        title.setColour(juce::Label::textColourId, juce::Colour(0xffffdb5c));
        title.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        addAndMakeVisible(title);

        helper.setText(paulaMode
                           ? "Checked WAV/AIFF/8SVX files feed the 32-slot Paula-style sample bank and note map. Folder contents stay local."
                           : spc700Mode
                           ? "Checked BRR/WAV/AIFF files feed the 32-slot playable bank and note map. Folder contents stay local."
                           : "Checked files feed the 32-slot CC117 bank. Folder contents stay local and are not copied into the repo.",
                       juce::dontSendNotification);
        helper.setJustificationType(juce::Justification::centredLeft);
        helper.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        helper.setFont(juce::FontOptions(11.0f));
        helper.setMinimumHorizontalScale(0.65f);
        addAndMakeVisible(helper);

        selectFirstButton.setButtonText("First 32");
        selectFirstButton.setTooltip(paulaMode ? "Check the first 32 loaded Paula samples and uncheck the rest."
                                               : spc700Mode ? "Check the first 32 loaded SPC700 samples and uncheck the rest."
                                                            : "Check the first 32 loaded DMC files and uncheck the rest.");
        selectFirstButton.onClick = [this]
        {
            if (paulaMode)
                audioProcessor.selectFirstPaulaSamples(32);
            else if (spc700Mode)
                audioProcessor.selectFirstSpc700BrrSamples(32);
            else
                audioProcessor.selectFirstNesDmcSamples(32);
            refreshAfterEdit();
        };
        addAndMakeVisible(selectFirstButton);

        invertButton.setButtonText("Invert");
        invertButton.setTooltip(paulaMode ? "Invert checked Paula samples. Only the first 32 checked files become active playable slots."
                                          : spc700Mode ? "Invert checked SPC700 samples. Only the first 32 checked files become active playable slots."
                                                       : "Invert checked DMC files. Only the first 32 checked files become active slots.");
        invertButton.onClick = [this]
        {
            if (paulaMode)
                audioProcessor.invertPaulaSampleSelection();
            else if (spc700Mode)
                audioProcessor.invertSpc700BrrSampleSelection();
            else
                audioProcessor.invertNesDmcSampleSelection();
            refreshAfterEdit();
        };
        addAndMakeVisible(invertButton);

        clearButton.setButtonText("Clear");
        clearButton.setTooltip(paulaMode ? "Uncheck every Paula sample in this bank."
                                         : spc700Mode ? "Uncheck every SPC700 sample in this bank."
                                                      : "Uncheck every DMC file in this bank.");
        clearButton.onClick = [this]
        {
            if (paulaMode)
                audioProcessor.clearPaulaSampleSelection();
            else if (spc700Mode)
                audioProcessor.clearSpc700BrrSampleSelection();
            else
                audioProcessor.clearNesDmcSampleSelection();
            refreshAfterEdit();
        };
        addAndMakeVisible(clearButton);

        listContent = std::make_unique<juce::Component>();
        viewport.setViewedComponent(listContent.get(), false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        rebuildRows();
        setSize(520, 420);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff10181c));
        g.setColour(juce::Colour(0xff35505a));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        title.setBounds(area.removeFromTop(22));
        helper.setBounds(area.removeFromTop(34));
        auto actionRow = area.removeFromTop(26);
        selectFirstButton.setBounds(actionRow.removeFromLeft(92).reduced(0, 1));
        actionRow.removeFromLeft(6);
        invertButton.setBounds(actionRow.removeFromLeft(76).reduced(0, 1));
        actionRow.removeFromLeft(6);
        clearButton.setBounds(actionRow.removeFromLeft(76).reduced(0, 1));
        area.removeFromTop(6);
        viewport.setBounds(area);

        constexpr int rowHeight = 26;
        auto listArea = juce::Rectangle<int>(0, 0, viewport.getWidth() - 18, static_cast<int>(rows.size()) * rowHeight);
        listContent->setBounds(listArea);
        for (size_t i = 0; i < rows.size(); ++i)
            rows[i]->setBounds(0, static_cast<int>(i) * rowHeight, listArea.getWidth(), rowHeight);
    }

private:
    class Row final : public juce::Component
    {
    public:
        Row(ChipperAudioProcessor& processor,
            bool editsSpc700Brr,
            bool editsPaulaSamples,
            int sampleIndex,
            ChipperAudioProcessor::DmcSampleEntryInfo info,
            std::function<void()> refreshCallback)
            : audioProcessor(processor), spc700Mode(editsSpc700Brr), paulaMode(editsPaulaSamples), index(sampleIndex), onRefresh(std::move(refreshCallback))
        {
            toggle.setButtonText(info.name);
            toggle.setToggleState(info.included, juce::dontSendNotification);
            toggle.setTooltip(info.path);
            toggle.onClick = [this]
            {
                if (paulaMode)
                    audioProcessor.setPaulaSampleIncluded(index, toggle.getToggleState());
                else if (spc700Mode)
                    audioProcessor.setSpc700BrrSampleIncluded(index, toggle.getToggleState());
                else
                    audioProcessor.setNesDmcSampleIncluded(index, toggle.getToggleState());
                if (onRefresh)
                    onRefresh();
            };
            addAndMakeVisible(toggle);

            detail.setText(juce::String(info.activeSlot ? "slot" : "off") + " | " + juce::String(info.byteCount) + " bytes", juce::dontSendNotification);
            detail.setJustificationType(juce::Justification::centredRight);
            detail.setColour(juce::Label::textColourId, info.activeSlot ? juce::Colour(0xff59d4e8) : juce::Colour(0xff7f8b93));
            detail.setFont(juce::FontOptions(10.0f));
            addAndMakeVisible(detail);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(4, 1);
            detail.setBounds(area.removeFromRight(92));
            toggle.setBounds(area);
        }

    private:
        ChipperAudioProcessor& audioProcessor;
        bool spc700Mode = false;
        bool paulaMode = false;
        int index = 0;
        std::function<void()> onRefresh;
        juce::ToggleButton toggle;
        juce::Label detail;
    };

    void rebuildRows()
    {
        const auto entries = paulaMode ? audioProcessor.paulaSampleEntryInfo()
                                       : spc700Mode ? audioProcessor.spc700BrrSampleEntryInfo()
                                                    : audioProcessor.nesDmcSampleEntryInfo();
        rows.clear();
        listContent->removeAllChildren();

        if (entries.empty())
        {
            auto empty = std::make_unique<Row>(audioProcessor,
                                               spc700Mode,
                                               paulaMode,
                                               0,
                                               ChipperAudioProcessor::DmcSampleEntryInfo {
                                                   paulaMode ? "No external Paula sample bank loaded" : (spc700Mode ? "Generated SPC700 shape active; load samples to replace it" : "No .dmc files loaded"), {}, 0, false, false },
                                               [this] { refreshAfterEdit(); });
            empty->setEnabled(false);
            listContent->addAndMakeVisible(*empty);
            rows.push_back(std::move(empty));
            return;
        }

        for (size_t i = 0; i < entries.size(); ++i)
        {
            auto row = std::make_unique<Row>(audioProcessor, spc700Mode, paulaMode, static_cast<int>(i), entries[i], [this] { refreshAfterEdit(); });
            listContent->addAndMakeVisible(*row);
            rows.push_back(std::move(row));
        }
    }

    void refreshAfterEdit()
    {
        if (onRefresh)
            onRefresh();

        const juce::Component::SafePointer<DmcSampleBankEditorComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis == nullptr)
                return;

            safeThis->rebuildRows();
            safeThis->resized();
        });
    }

    ChipperAudioProcessor& audioProcessor;
    bool spc700Mode = false;
    bool paulaMode = false;
    std::function<void()> onRefresh;
    juce::Label title;
    juce::Label helper;
    juce::TextButton selectFirstButton;
    juce::TextButton invertButton;
    juce::TextButton clearButton;
    juce::Viewport viewport;
    std::unique_ptr<juce::Component> listContent;
    std::vector<std::unique_ptr<Row>> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DmcSampleBankEditorComponent)
};

const char* sidAdsrFieldLabel(size_t field)
{
    static constexpr std::array<const char*, 4> labels { "Atk", "Dec", "Sus", "Rel" };
    return labels[std::min(field, labels.size() - 1u)];
}

chipper::ChipParameterRole ymChannelMixRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 3> roles {
        chipper::ChipParameterRole::ymChannelAMix,
        chipper::ChipParameterRole::ymChannelBMix,
        chipper::ChipParameterRole::ymChannelCMix
    };

    return roles[std::min(index, roles.size() - 1u)];
}

const char* ymChannelMixParameterId(size_t index)
{
    static constexpr std::array<const char*, 3> ids {
        chipper::parameters::id::ymChannelAMix,
        chipper::parameters::id::ymChannelBMix,
        chipper::parameters::id::ymChannelCMix
    };

    return ids[std::min(index, ids.size() - 1u)];
}

juce::String choiceTooltip(const chipper::ChipParameterSpec& spec, size_t choiceIndex)
{
    if (choiceIndex < spec.choices.size() && ! spec.choices[choiceIndex].help.empty())
        return juce::String(spec.choices[choiceIndex].help);

    return juce::String(spec.help);
}

juce::String visibleChoiceLabel(const std::string& label)
{
    return label == "Macro" ? juce::String("Preset") : juce::String(label);
}

juce::String withMidiCc(juce::String text, const char* parameterId)
{
    const auto controller = chipper::parameters::midiControllerForParameterId(parameterId);
    if (controller < 0)
        return text;

    const auto ccText = juce::String("MIDI CC ") + juce::String(controller);
    return text.isEmpty() ? ccText : text + "\n" + ccText;
}

juce::String withMidiCcForRole(juce::String text, chipper::ChipParameterRole role)
{
    return withMidiCc(text, chipper::parameters::parameterIdForChipParameterRole(role));
}

juce::String midiCcRangeLabel()
{
    const auto& mappings = chipper::parameters::midiCcMappings();
    auto minController = 127;
    auto maxController = 0;

    for (const auto& mapping : mappings)
    {
        minController = std::min(minController, mapping.controller);
        maxController = std::max(maxController, mapping.controller);
    }

    return juce::String("MIDI CC ") + juce::String(minController) + "-" + juce::String(maxController);
}

juce::String midiCcMapTooltip()
{
    juce::String text("Fixed MIDI CC map\n");

    for (const auto& mapping : chipper::parameters::midiCcMappings())
        text += juce::String("CC ") + juce::String(mapping.controller) + " - " + juce::String(mapping.label) + "\n";

    text += "\nAll current plugin parameters have default MIDI CC control. Choice/register controls quantize to legal chip values.";
    return text;
}

juce::File defaultUserPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Chipper Presets");
}

juce::String safePresetFileStem(juce::String text)
{
    auto safe = text.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_").trim();
    safe = safe.replace(" ", "-");
    while (safe.contains("--"))
        safe = safe.replace("--", "-");

    return safe.isNotEmpty() ? safe : juce::String("Chipper-Preset");
}

std::optional<chipper::ChipMode> chipModeFromUserPresetXml(const juce::XmlElement& root)
{
    if (root.hasAttribute("chip"))
        return chipper::parseChipMode(root.getStringAttribute("chip").toStdString());

    if (const auto* child = root.getChildByName("ChipperPreset"))
        return chipModeFromUserPresetXml(*child);

    if (root.hasTagName("PARAM")
        && root.getStringAttribute("id") == chipper::parameters::id::chipMode
        && root.hasAttribute("value"))
    {
        const auto chipChoice = static_cast<int>(std::round(root.getDoubleAttribute("value", 0.0)));
        if (chipChoice >= 0 && chipChoice < chipper::parameters::chipModeChoices().size())
            return chipper::parameters::chipModeFromChoice(chipChoice);
    }

    for (const auto* child : root.getChildIterator())
    {
        if (child != nullptr)
        {
            if (const auto childMode = chipModeFromUserPresetXml(*child))
                return childMode;
        }
    }

    return std::nullopt;
}

juce::String userPresetNameFromXml(const juce::XmlElement& root, const juce::File& file)
{
    auto name = root.getStringAttribute("name").trim();
    if (name.isEmpty())
        name = file.getFileNameWithoutExtension();

    return name.isNotEmpty() ? name : juce::String("User Preset");
}

bool isPresetSampleReferenceTag(const juce::String& tagName)
{
    return tagName == "DMC_SAMPLE"
        || tagName == "BRR_SAMPLE"
        || tagName == "PAULA_SAMPLE"
        || tagName == "CHIPPER_SPC700_BRR";
}

juce::String portableSampleRelativePath(const juce::File& sampleFile, const juce::File& presetDirectory)
{
    if (presetDirectory == juce::File {} || sampleFile == juce::File {})
        return {};

    if (sampleFile.getParentDirectory() == presetDirectory || sampleFile.isAChildOf(presetDirectory))
        return sampleFile.getRelativePathFrom(presetDirectory);

    if (presetDirectory.getChildFile(sampleFile.getFileName()).existsAsFile())
        return sampleFile.getFileName();

    const auto samplesFile = presetDirectory.getChildFile("Samples").getChildFile(sampleFile.getFileName());
    if (samplesFile.existsAsFile())
        return "Samples/" + sampleFile.getFileName();

    const auto lowercaseSamplesFile = presetDirectory.getChildFile("samples").getChildFile(sampleFile.getFileName());
    if (lowercaseSamplesFile.existsAsFile())
        return "samples/" + sampleFile.getFileName();

    return {};
}

void annotatePortablePresetSampleReferences(juce::XmlElement& xml, const juce::File& presetDirectory)
{
    if (isPresetSampleReferenceTag(xml.getTagName()) && xml.hasAttribute("path"))
    {
        const juce::File sampleFile(xml.getStringAttribute("path"));
        if (sampleFile != juce::File {})
        {
            xml.setAttribute("fileName", sampleFile.getFileName());
            const auto relativePath = portableSampleRelativePath(sampleFile, presetDirectory);
            if (relativePath.isNotEmpty())
                xml.setAttribute("relativePath", relativePath);
        }
    }

    for (auto* child : xml.getChildIterator())
    {
        if (child != nullptr)
            annotatePortablePresetSampleReferences(*child, presetDirectory);
    }
}

juce::String byteHex(uint8_t value)
{
    return juce::String::toHexString(static_cast<int>(value)).paddedLeft('0', 2).toUpperCase();
}

const char* sidWaveNameForControlBits(uint8_t bits)
{
    switch (bits)
    {
        case 0x10u: return "Tri";
        case 0x20u: return "Saw";
        case 0x30u: return "Tri+Saw";
        case 0x40u: return "Pulse";
        case 0x50u: return "Tri+Pulse";
        case 0x60u: return "Saw+Pulse";
        case 0x70u: return "Tri+Saw+Pulse";
        case 0x80u: return "Noise";
        default: return "Off";
    }
}

int sidWaveChoiceForControlBits(uint8_t bits)
{
    switch (bits)
    {
        case 0x10u: return 1;
        case 0x20u: return 2;
        case 0x40u: return 3;
        case 0x80u: return 4;
        case 0x30u: return 5;
        case 0x50u: return 6;
        case 0x60u: return 7;
        case 0x70u: return 8;
        default: return 0;
    }
}

juce::String sidFilterRoutingName(uint8_t bits)
{
    switch (bits & 0x07u)
    {
        case 0x00u: return "none";
        case 0x01u: return "V1";
        case 0x02u: return "V2";
        case 0x03u: return "V1+V2";
        case 0x04u: return "V3";
        case 0x05u: return "V1+V3";
        case 0x06u: return "V2+V3";
        case 0x07u:
        default:
            return "all voices";
    }
}

juce::String sidAdsrNibbleReadout(const chipper::PatchConfig& patch, size_t voice = 0)
{
    const auto ad = chipper::sidAttackDecayForVoice(patch, voice);
    const auto sr = chipper::sidSustainReleaseForVoice(patch, voice);
    const auto attack = static_cast<int>((ad >> 4u) & 0x0fu);
    const auto decay = static_cast<int>(ad & 0x0fu);
    const auto sustain = static_cast<int>((sr >> 4u) & 0x0fu);
    const auto release = static_cast<int>(sr & 0x0fu);

    return juce::String("V") + juce::String(static_cast<int>(voice + 1u))
        + " AD 0x" + byteHex(ad)
        + " A" + juce::String(attack)
        + "/D" + juce::String(decay)
        + " | SR 0x" + byteHex(sr)
        + " S" + juce::String(sustain)
        + "/R" + juce::String(release);
}

float pulseDutyRatioForChoice(int choice)
{
    static constexpr std::array<float, 4> duty {
        0.125f,
        0.25f,
        0.5f,
        0.75f
    };

    return duty[static_cast<size_t>(std::clamp(choice, 0, 3))];
}

float pulseDutyRatioForControl(float value)
{
    return pulseDutyRatioForChoice(static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 3.0f)));
}

int nesPulse2DutyChoiceForPatch(const chipper::PatchConfig& patch)
{
    const auto primary = std::clamp(static_cast<int>(std::round(patch.control1 * 3.0f)), 0, 3);
    const auto explicitChoice = std::clamp(patch.pulse2Duty, 0, 4);
    if (explicitChoice > 0)
        return explicitChoice - 1;

    return patch.playMode == chipper::PlayMode::chipPoly ? primary : std::min(primary + 1, 3);
}

int dmgPulseDutyChoiceForSource(const chipper::PatchConfig& patch, size_t sourceIndex)
{
    const auto primary = std::clamp(static_cast<int>(std::round(patch.control1 * 3.0f)), 0, 3);
    if (sourceIndex == 1)
    {
        const auto explicitChoice = std::clamp(patch.pulse2Duty, 0, 4);
        if (explicitChoice > 0)
            return explicitChoice - 1;

        return patch.playMode == chipper::PlayMode::chipPoly ? primary : std::min(primary + 1, 3);
    }

    return primary;
}

ChipWaveformPreviewShape dmgWavePreviewShape(const chipper::PatchConfig& patch)
{
    switch (std::clamp(patch.waveShape, 0, 4))
    {
        case 1: return ChipWaveformPreviewShape::triangle;
        case 2: return ChipWaveformPreviewShape::saw;
        case 3: return ChipWaveformPreviewShape::pulse;
        case 4: return ChipWaveformPreviewShape::stepped;
        case 0:
        default:
            return ChipWaveformPreviewShape::stepped;
    }
}

ChipWaveformPreviewShape wavetablePreviewShapeForChoice(int choice)
{
    switch (std::clamp(choice, 0, 4))
    {
        case 1: return ChipWaveformPreviewShape::triangle;
        case 2: return ChipWaveformPreviewShape::saw;
        case 3: return ChipWaveformPreviewShape::pulse;
        case 4: return ChipWaveformPreviewShape::stepped;
        case 0:
        default:
            return ChipWaveformPreviewShape::stepped;
    }
}

ChipWaveformPreviewShape wavetablePreviewShape(const chipper::PatchConfig& patch)
{
    return wavetablePreviewShapeForChoice(patch.waveShape);
}

template <typename ButtonArray>
void layoutSegmentedButtons(ButtonArray& buttons, juce::Rectangle<int> bounds, size_t visibleCount)
{
    const auto count = std::max<size_t>(1u, std::min(visibleCount, buttons.size()));
    const auto idealButtonWidth = 92;
    const auto gap = bounds.getWidth() < static_cast<int>(count) * idealButtonWidth ? 3 : 4;
    auto remainingWidth = std::max(1, bounds.getWidth() - (gap * static_cast<int>(count - 1u)));
    auto x = bounds.getX();

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        if (i >= count)
        {
            buttons[i].setBounds({});
            continue;
        }

        const auto remainingButtons = static_cast<int>(count - i);
        const auto width = std::max(1, remainingWidth / remainingButtons);
        buttons[i].setBounds({ x, bounds.getY(), width, bounds.getHeight() });
        x += width + gap;
        remainingWidth -= width;
    }
}

int labelReservation(juce::Rectangle<int> header, int preferred)
{
    if (header.getWidth() <= 0)
        return 0;

    const auto reserveForReadout = std::min(82, std::max(0, header.getWidth() / 3));
    return std::clamp(preferred, 0, std::max(0, header.getWidth() - reserveForReadout));
}

} // namespace

void ChipWaveformPreview::setShape(ChipWaveformPreviewShape newShape, float pulseWidthRatio, bool shouldBeActive)
{
    const auto clampedPulseWidth = std::clamp(pulseWidthRatio, 0.02f, 0.98f);
    if (shape == newShape && std::abs(pulseWidth - clampedPulseWidth) < 0.001f && active == shouldBeActive)
        return;

    shape = newShape;
    pulseWidth = clampedPulseWidth;
    active = shouldBeActive;
    repaint();
}

void ChipWaveformPreview::setSidWaveform(uint8_t bits, float pulseWidthRatio, bool shouldBeActive)
{
    const auto waveform = bits & 0xf0u;
    if ((waveform & 0x80u) != 0u)
        setShape(ChipWaveformPreviewShape::noise, pulseWidthRatio, shouldBeActive);
    else if (std::popcount(static_cast<unsigned>(waveform)) > 1)
        setShape(ChipWaveformPreviewShape::combined, pulseWidthRatio, shouldBeActive);
    else if ((waveform & 0x40u) != 0u)
        setShape(ChipWaveformPreviewShape::pulse, pulseWidthRatio, shouldBeActive);
    else if ((waveform & 0x20u) != 0u)
        setShape(ChipWaveformPreviewShape::saw, pulseWidthRatio, shouldBeActive);
    else if ((waveform & 0x10u) != 0u)
        setShape(ChipWaveformPreviewShape::triangle, pulseWidthRatio, shouldBeActive);
    else
        setShape(ChipWaveformPreviewShape::off, pulseWidthRatio, shouldBeActive);
}

void ChipWaveformPreview::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (bounds.isEmpty())
        return;

    g.setColour(juce::Colour(0xff10181c));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colour(0xff2a3a40));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    const auto graph = bounds.reduced(4.0f, 3.0f);
    const auto top = graph.getY();
    const auto bottom = graph.getBottom();
    const auto mid = graph.getCentreY();
    const auto left = graph.getX();
    const auto right = graph.getRight();
    const auto width = std::max(1.0f, graph.getWidth());
    const auto height = std::max(1.0f, graph.getHeight());

    g.setColour(juce::Colour(0xff243139).withAlpha(0.80f));
    g.drawHorizontalLine(static_cast<int>(std::round(mid)), left, right);

    const auto drawNoise = [&]()
    {
        juce::Path path;
        uint32_t state = 0x2a6d365bu;
        path.startNewSubPath(left, mid);
        constexpr auto pointCount = 28;
        for (int i = 0; i < pointCount; ++i)
        {
            state = (state * 1664525u) + 1013904223u;
            const auto x = left + (width * static_cast<float>(i) / static_cast<float>(pointCount - 1));
            const auto normalized = static_cast<float>((state >> 24u) & 0xffu) / 255.0f;
            const auto y = bottom - (normalized * height);
            path.lineTo(x, y);
        }

        return path;
    };

    const auto drawPulse = [&](float duty)
    {
        juce::Path path;
        const auto high = top;
        const auto low = bottom;
        const auto period = width * 0.5f;
        path.startNewSubPath(left, high);
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            const auto cycleStart = left + (static_cast<float>(cycle) * period);
            const auto transition = cycleStart + (period * duty);
            const auto cycleEnd = cycleStart + period;
            path.lineTo(transition, high);
            path.lineTo(transition, low);
            path.lineTo(cycleEnd, low);
            if (cycle == 0)
                path.lineTo(cycleEnd, high);
        }

        return path;
    };

    juce::Path path;
    switch (shape)
    {
        case ChipWaveformPreviewShape::noise:
            path = drawNoise();
            break;

        case ChipWaveformPreviewShape::toneNoise:
            path = drawPulse(0.5f);
            g.setColour((active ? juce::Colour(0xff56c7d8) : juce::Colour(0xff72818a)).withAlpha(0.46f));
            g.strokePath(path, juce::PathStrokeType(1.1f));
            path = drawNoise();
            break;

        case ChipWaveformPreviewShape::pulse:
            path = drawPulse(pulseWidth);
            break;

        case ChipWaveformPreviewShape::saw:
        {
            const auto period = width * 0.5f;
            path.startNewSubPath(left, bottom);
            for (int cycle = 0; cycle < 2; ++cycle)
            {
                const auto cycleEnd = left + (static_cast<float>(cycle + 1) * period);
                path.lineTo(cycleEnd, top);
                if (cycle == 0)
                    path.lineTo(cycleEnd, bottom);
            }
            break;
        }

        case ChipWaveformPreviewShape::triangle:
            path.startNewSubPath(left, mid);
            path.lineTo(left + width * 0.125f, top);
            path.lineTo(left + width * 0.375f, bottom);
            path.lineTo(left + width * 0.625f, top);
            path.lineTo(left + width * 0.875f, bottom);
            path.lineTo(right, mid);
            break;

        case ChipWaveformPreviewShape::stepped:
        {
            path.startNewSubPath(left, bottom);
            static constexpr std::array<float, 8> steps { 0.18f, 0.42f, 0.70f, 0.92f, 0.70f, 0.42f, 0.18f, 0.54f };
            const auto segment = width / static_cast<float>(steps.size());
            for (size_t i = 0; i < steps.size(); ++i)
            {
                const auto x = left + (segment * static_cast<float>(i));
                const auto nextX = i + 1u == steps.size() ? right : x + segment;
                const auto y = bottom - (steps[i] * height);
                path.lineTo(x, y);
                path.lineTo(nextX, y);
            }
            break;
        }

        case ChipWaveformPreviewShape::combined:
        {
            path.startNewSubPath(left, mid);
            static constexpr std::array<float, 10> points { 0.52f, 0.90f, 0.76f, 0.28f, 0.12f, 0.72f, 0.60f, 0.18f, 0.96f, 0.42f };
            for (size_t i = 0; i < points.size(); ++i)
            {
                const auto x = left + (width * static_cast<float>(i) / static_cast<float>(points.size() - 1u));
                const auto y = bottom - (points[i] * height);
                path.lineTo(x, y);
            }
            break;
        }

        case ChipWaveformPreviewShape::off:
        default:
            path.startNewSubPath(left, mid);
            path.lineTo(right, mid);
            break;
    }

    g.setColour(shape == ChipWaveformPreviewShape::off
                    ? juce::Colour(0xff72818a)
                    : (active ? juce::Colour(0xff56c7d8) : juce::Colour(0xff72818a)));
    g.strokePath(path, juce::PathStrokeType(1.4f));
}

void ChipEnvelopePreview::setSidAdsr(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release)
{
    const auto previousMode = mode;
    mode = Mode::sidAdsr;
    attack &= 0x0fu;
    decay &= 0x0fu;
    sustain &= 0x0fu;
    release &= 0x0fu;

    if (previousMode == mode
        && attackNibble == attack
        && decayNibble == decay
        && sustainNibble == sustain
        && releaseNibble == release)
        return;

    attackNibble = attack;
    decayNibble = decay;
    sustainNibble = sustain;
    releaseNibble = release;
    repaint();
}

void ChipEnvelopePreview::setYmEnvelope(uint8_t shapeCode, bool envelopeEnabled)
{
    const auto previousMode = mode;
    mode = Mode::ymEnvelope;
    shapeCode &= 0x0fu;

    if (previousMode == mode && ymShapeCode == shapeCode && ymEnabled == envelopeEnabled)
        return;

    ymShapeCode = shapeCode;
    ymEnabled = envelopeEnabled;
    repaint();
}

void ChipEnvelopePreview::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (bounds.isEmpty())
        return;

    g.setColour(juce::Colour(0xff10181c));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colour(0xff2a3a40));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    const auto graph = bounds.reduced(6.0f, 5.0f);
    const auto left = graph.getX();
    const auto right = graph.getRight();
    const auto top = graph.getY();
    const auto bottom = graph.getBottom();
    const auto width = std::max(1.0f, graph.getWidth());
    const auto height = std::max(1.0f, graph.getHeight());

    if (mode == Mode::ymEnvelope)
    {
        const auto highY = top;
        const auto lowY = bottom;
        const auto midY = graph.getCentreY();

        g.setColour(juce::Colour(0xff243139).withAlpha(0.72f));
        g.drawHorizontalLine(static_cast<int>(std::round(midY)), left, right);
        for (int i = 1; i < 4; ++i)
        {
            const auto x = left + (width * static_cast<float>(i) / 4.0f);
            g.drawVerticalLine(static_cast<int>(std::round(x)), top, bottom);
        }

        juce::Path path;
        const auto segment = width / 4.0f;

        if (! ymEnabled)
        {
            path.startNewSubPath(left, midY);
            path.lineTo(right, midY);
        }
        else
        {
            const auto cont = (ymShapeCode & 0x08u) != 0u;
            const auto attack = (ymShapeCode & 0x04u) != 0u;
            const auto alternate = (ymShapeCode & 0x02u) != 0u;
            const auto hold = (ymShapeCode & 0x01u) != 0u;
            auto rising = attack;
            path.startNewSubPath(left, rising ? lowY : highY);

            if (! cont)
            {
                path.lineTo(left + segment, rising ? highY : lowY);
                path.lineTo(right, lowY);
            }
            else if (hold)
            {
                path.lineTo(left + segment, rising ? highY : lowY);
                const auto heldHigh = attack == alternate;
                path.lineTo(right, heldHigh ? highY : lowY);
            }
            else
            {
                auto x = left;
                for (int i = 0; i < 4; ++i)
                {
                    path.lineTo(x + segment, rising ? highY : lowY);
                    x += segment;
                    if (alternate)
                        rising = ! rising;
                    else
                    {
                        path.lineTo(x, rising ? lowY : highY);
                    }
                }
            }
        }

        g.setColour(ymEnabled ? juce::Colour(0xff56c7d8).withAlpha(0.13f)
                              : juce::Colour(0xff72818a).withAlpha(0.10f));
        juce::Path fill;
        fill.startNewSubPath(left, bottom);
        fill.addPath(path);
        fill.lineTo(right, bottom);
        fill.closeSubPath();
        g.fillPath(fill);

        g.setColour(ymEnabled ? juce::Colour(0xff56c7d8) : juce::Colour(0xff72818a));
        g.strokePath(path, juce::PathStrokeType(1.5f));
        return;
    }

    const auto sustainY = bottom - ((static_cast<float>(sustainNibble) / 15.0f) * height);

    const auto stageWeight = [](double seconds)
    {
        return static_cast<float>(std::log10(1.0 + (seconds * 12.0)));
    };

    const auto attackWeight = stageWeight(chipper::sidAttackSecondsForNibble(attackNibble));
    const auto decayWeight = stageWeight(chipper::sidDecayReleaseSecondsForNibble(decayNibble));
    const auto releaseWeight = stageWeight(chipper::sidDecayReleaseSecondsForNibble(releaseNibble));
    const auto sustainWeight = 0.32f;
    const auto total = std::max(0.001f, attackWeight + decayWeight + sustainWeight + releaseWeight);

    const auto attackX = left + (width * attackWeight / total);
    const auto decayX = attackX + (width * decayWeight / total);
    const auto sustainX = decayX + (width * sustainWeight / total);

    g.setColour(juce::Colour(0xff243139).withAlpha(0.80f));
    g.drawHorizontalLine(static_cast<int>(std::round(sustainY)), left, right);
    if (height >= 24.0f)
    {
        g.setColour(juce::Colour(0xff243139).withAlpha(0.62f));
        for (const auto stageX : { attackX, decayX, sustainX })
            g.drawVerticalLine(static_cast<int>(std::round(stageX)), top, bottom);
    }

    juce::Path fill;
    fill.startNewSubPath(left, bottom);
    fill.lineTo(attackX, top);
    fill.lineTo(decayX, sustainY);
    fill.lineTo(sustainX, sustainY);
    fill.lineTo(right, bottom);
    fill.lineTo(left, bottom);
    fill.closeSubPath();
    g.setColour(juce::Colour(0xff56c7d8).withAlpha(0.11f));
    g.fillPath(fill);

    juce::Path path;
    path.startNewSubPath(left, bottom);
    path.lineTo(attackX, top);
    path.lineTo(decayX, sustainY);
    path.lineTo(sustainX, sustainY);
    path.lineTo(right, bottom);

    g.setColour(juce::Colour(0xff56c7d8));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    if (height >= 30.0f && width >= 92.0f)
    {
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xffaebbc4).withAlpha(0.88f));
        const auto labelY = bottom - 10.0f;
        const auto drawStageLabel = [&](const char* text, float x)
        {
            g.drawText(text,
                       juce::Rectangle<float>(x - 10.0f, labelY, 20.0f, 10.0f),
                       juce::Justification::centred,
                       false);
        };
        drawStageLabel("A", left + 8.0f);
        drawStageLabel("D", attackX);
        drawStageLabel("S", decayX + ((sustainX - decayX) * 0.5f));
        drawStageLabel("R", std::min(right - 8.0f, sustainX));
    }
}

void FmAlgorithmPreview::setAlgorithm(int newAlgorithm, bool shouldFollow)
{
    const auto clampedAlgorithm = std::clamp(newAlgorithm, 0, 7);
    if (algorithm == clampedAlgorithm && follow == shouldFollow)
        return;

    algorithm = clampedAlgorithm;
    follow = shouldFollow;
    repaint();
}

void FmAlgorithmPreview::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (bounds.isEmpty())
        return;

    g.setColour(juce::Colour(0xff10181c));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff2a3a40));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    auto graph = bounds.reduced(7.0f, 4.0f);
    if (graph.getWidth() < 80.0f || graph.getHeight() < 20.0f)
        return;

    const auto compact = graph.getHeight() < 38.0f;
    auto labelArea = graph.removeFromTop(std::min(compact ? 11.0f : 15.0f, graph.getHeight()));
    graph.removeFromTop(std::min(compact ? 1.0f : 3.0f, graph.getHeight()));
    if (graph.getHeight() < 8.0f)
    {
        g.setColour(follow ? juce::Colour(0xffaebbc4) : juce::Colour(0xffdbe8e5));
        g.setFont(juce::FontOptions(compact ? 8.0f : 10.0f, juce::Font::bold));
        g.drawText(juce::String(follow ? "Preset -> " : "") + "Alg " + juce::String(algorithm),
                   labelArea,
                   juce::Justification::centredLeft);
        return;
    }

    const auto opRadius = std::clamp(std::min(graph.getWidth(), graph.getHeight()) * (compact ? 0.13f : 0.095f), 4.0f, 10.0f);
    const auto carrierColour = juce::Colour(0xfff0c94d);
    const auto modColour = juce::Colour(0xff56c7d8);
    const auto mutedColour = juce::Colour(0xff72818a);
    const auto lineColour = juce::Colour(0xff56c7d8).withAlpha(follow ? 0.55f : 0.78f);
    static constexpr std::array<const char*, 8> algorithmNames {
        "serial 1>2>3>4",
        "stack + branch",
        "triple mod",
        "dual branch",
        "two FM pairs",
        "1 drives 2/3/4",
        "pair + additive",
        "all carriers"
    };

    const auto x1 = graph.getX() + graph.getWidth() * 0.15f;
    const auto x2 = graph.getX() + graph.getWidth() * 0.39f;
    const auto x3 = graph.getX() + graph.getWidth() * 0.63f;
    const auto x4 = graph.getX() + graph.getWidth() * 0.84f;
    const auto yTop = graph.getY() + graph.getHeight() * 0.22f;
    const auto yMid = graph.getCentreY();
    const auto yBottom = graph.getBottom() - graph.getHeight() * 0.22f;
    const auto outX = graph.getRight() - 2.0f;

    std::array<juce::Point<float>, 4> op {
        juce::Point<float> { x1, yMid },
        juce::Point<float> { x2, yMid },
        juce::Point<float> { x3, yMid },
        juce::Point<float> { x4, yMid }
    };

    std::array<bool, 4> carrier { false, false, false, true };
    std::vector<std::pair<int, int>> edges;
    switch (algorithm)
    {
        case 0:
            edges = { { 0, 1 }, { 1, 2 }, { 2, 3 } };
            carrier = { false, false, false, true };
            break;
        case 1:
            op[0] = { x1, yTop };
            op[1] = { x2, yTop };
            op[2] = { x2, yBottom };
            op[3] = { x4, yMid };
            edges = { { 0, 1 }, { 1, 3 }, { 2, 3 } };
            carrier = { false, false, false, true };
            break;
        case 2:
            op[0] = { x1, yTop };
            op[1] = { x2, yTop };
            op[2] = { x2, yBottom };
            op[3] = { x4, yMid };
            edges = { { 0, 3 }, { 1, 3 }, { 2, 3 } };
            carrier = { false, false, false, true };
            break;
        case 3:
            op[0] = { x1, yTop };
            op[1] = { x2, yTop };
            op[2] = { x1, yBottom };
            op[3] = { x4, yMid };
            edges = { { 0, 1 }, { 1, 3 }, { 2, 3 } };
            carrier = { false, false, false, true };
            break;
        case 4:
            op[0] = { x1, yTop };
            op[1] = { x2, yTop };
            op[2] = { x1, yBottom };
            op[3] = { x2, yBottom };
            edges = { { 0, 1 }, { 2, 3 } };
            carrier = { false, true, false, true };
            break;
        case 5:
            op[0] = { x1, yMid };
            op[1] = { x3, yTop };
            op[2] = { x3, yMid };
            op[3] = { x3, yBottom };
            edges = { { 0, 1 }, { 0, 2 }, { 0, 3 } };
            carrier = { false, true, true, true };
            break;
        case 6:
            op[0] = { x1, yTop };
            op[1] = { x2, yTop };
            op[2] = { x1, yBottom };
            op[3] = { x2, yBottom };
            edges = { { 0, 1 }, { 2, 3 } };
            carrier = { false, true, true, true };
            break;
        case 7:
        default:
            op[0] = { x2, yTop };
            op[1] = { x2, yMid - graph.getHeight() * 0.10f };
            op[2] = { x2, yMid + graph.getHeight() * 0.10f };
            op[3] = { x2, yBottom };
            edges = {};
            carrier = { true, true, true, true };
            break;
    }

    g.setColour(juce::Colour(0xff243139).withAlpha(0.72f));
    g.drawLine(outX - 10.0f, graph.getY(), outX - 10.0f, graph.getBottom(), 1.0f);

    g.setColour(lineColour);
    for (const auto& edge : edges)
        g.drawLine(op[static_cast<size_t>(edge.first)].x + opRadius,
                   op[static_cast<size_t>(edge.first)].y,
                   op[static_cast<size_t>(edge.second)].x - opRadius,
                   op[static_cast<size_t>(edge.second)].y,
                   1.4f);

    for (size_t i = 0; i < op.size(); ++i)
    {
        if (carrier[i])
        {
            g.setColour(carrierColour.withAlpha(follow ? 0.70f : 0.95f));
            g.drawLine(op[i].x + opRadius, op[i].y, outX - 10.0f, op[i].y, 1.3f);
        }

        g.setColour(carrier[i] ? carrierColour : modColour);
        g.fillEllipse(op[i].x - opRadius, op[i].y - opRadius, opRadius * 2.0f, opRadius * 2.0f);
        g.setColour(follow ? mutedColour : juce::Colour(0xff101414));
        g.setFont(juce::FontOptions(std::clamp(opRadius * 1.15f, 6.5f, 9.0f), juce::Font::bold));
        g.drawText(juce::String(static_cast<int>(i + 1u)),
                   juce::Rectangle<float> { op[i].x - opRadius, op[i].y - opRadius, opRadius * 2.0f, opRadius * 2.0f },
                   juce::Justification::centred);

        if (graph.getHeight() > 50.0f)
        {
            g.setColour(carrier[i] ? carrierColour.withAlpha(0.84f) : modColour.withAlpha(0.84f));
            g.setFont(juce::FontOptions(7.0f, juce::Font::bold));
            g.drawText(carrier[i] ? "CAR" : "MOD",
                       juce::Rectangle<float> { op[i].x - 15.0f, op[i].y + opRadius + 1.0f, 30.0f, 9.0f },
                       juce::Justification::centred);
        }
    }

    g.setColour(carrierColour.withAlpha(follow ? 0.70f : 0.95f));
    g.fillRoundedRectangle(outX - 7.0f, yMid - 9.0f, 14.0f, 18.0f, 2.0f);
    g.setColour(juce::Colour(0xff101414));
    g.setFont(juce::FontOptions(compact ? 6.5f : 8.0f, juce::Font::bold));
    g.drawText("OUT", juce::Rectangle<float> { outX - 15.0f, yMid - 8.0f, 30.0f, 16.0f }, juce::Justification::centred);

    g.setColour(follow ? juce::Colour(0xffaebbc4) : juce::Colour(0xffdbe8e5));
    g.setFont(juce::FontOptions(compact ? 8.0f : 10.0f, juce::Font::bold));
    const auto label = compact
        ? juce::String(follow ? "Preset -> " : "") + "Alg " + juce::String(algorithm)
        : juce::String(follow ? "Preset -> " : "") + "Alg " + juce::String(algorithm)
            + " | " + algorithmNames[static_cast<size_t>(std::clamp(algorithm, 0, 7))];
    g.drawText(label,
               labelArea,
               juce::Justification::centredLeft);
}

void OplWaveformPreview::setWaveform(int newWaveform, bool shouldFollow)
{
    const auto clampedWaveform = std::clamp(newWaveform, 0, 3);
    if (waveform == clampedWaveform && follow == shouldFollow)
        return;

    waveform = clampedWaveform;
    follow = shouldFollow;
    repaint();
}

void OplWaveformPreview::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (bounds.isEmpty())
        return;

    g.setColour(juce::Colour(0xff10181c));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff2a3a40));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    auto graph = bounds.reduced(7.0f, 5.0f);
    if (graph.getWidth() < 60.0f || graph.getHeight() < 30.0f)
        return;

    auto labelArea = graph.removeFromTop(13.0f);
    graph.removeFromTop(2.0f);

    const auto left = graph.getX();
    const auto right = graph.getRight();
    const auto top = graph.getY();
    const auto bottom = graph.getBottom();
    const auto mid = graph.getCentreY();
    const auto width = std::max(1.0f, graph.getWidth());
    const auto halfHeight = std::max(1.0f, graph.getHeight() * 0.44f);

    g.setColour(juce::Colour(0xff243139).withAlpha(0.72f));
    g.drawHorizontalLine(static_cast<int>(std::round(mid)), left, right);
    for (int i = 1; i < 4; ++i)
    {
        const auto x = left + width * static_cast<float>(i) / 4.0f;
        g.drawVerticalLine(static_cast<int>(std::round(x)), top, bottom);
    }

    juce::Path path;
    constexpr auto pointCount = 96;
    for (int i = 0; i < pointCount; ++i)
    {
        const auto phase = static_cast<float>(i) / static_cast<float>(pointCount - 1);
        const auto radians = phase * juce::MathConstants<float>::twoPi * 2.0f;
        float sample = std::sin(radians);

        switch (waveform)
        {
            case 1:
                sample = sample > 0.0f ? sample : 0.0f;
                break;
            case 2:
                sample = std::abs(sample);
                break;
            case 3:
                sample = sample > 0.0f ? sample : 0.0f;
                sample *= 1.0f - std::fmod(phase * 2.0f, 1.0f) * 0.55f;
                break;
            case 0:
            default:
                break;
        }

        const auto x = left + width * phase;
        const auto y = mid - (sample * halfHeight);
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    const auto activeColour = waveform == 3 ? juce::Colour(0xfff0c94d) : juce::Colour(0xff56c7d8);
    g.setColour(activeColour.withAlpha(follow ? 0.16f : 0.22f));
    juce::Path fill;
    fill.startNewSubPath(left, mid);
    fill.addPath(path);
    fill.lineTo(right, mid);
    fill.closeSubPath();
    g.fillPath(fill);

    g.setColour(activeColour.withAlpha(follow ? 0.72f : 1.0f));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    g.setColour(follow ? juce::Colour(0xffaebbc4) : juce::Colour(0xffdbe8e5));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(juce::String(follow ? "Preset -> " : "") + "OPL W" + juce::String(waveform),
               labelArea,
               juce::Justification::centredLeft);
}

void OutputScopePreview::setSamples(const ChipperAudioProcessor::OutputScopeSnapshot& newSamples)
{
    samples = newSamples;
    peak = 0.0f;
    for (const auto sample : samples)
        peak = std::max(peak, std::abs(sample));

    repaint();
}

void OutputScopePreview::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (bounds.isEmpty())
        return;

    g.setColour(juce::Colour(0xff10181c));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff2a3a40));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    const auto graph = bounds.reduced(5.0f, 4.0f);
    const auto left = graph.getX();
    const auto right = graph.getRight();
    const auto top = graph.getY();
    const auto bottom = graph.getBottom();
    const auto mid = graph.getCentreY();
    const auto width = std::max(1.0f, graph.getWidth());
    const auto halfHeight = std::max(1.0f, graph.getHeight() * 0.48f);

    g.setColour(juce::Colour(0xff243139).withAlpha(0.72f));
    g.drawHorizontalLine(static_cast<int>(std::round(mid)), left, right);
    g.setColour(juce::Colour(0xff243139).withAlpha(0.36f));
    g.drawHorizontalLine(static_cast<int>(std::round(top + (graph.getHeight() * 0.25f))), left, right);
    g.drawHorizontalLine(static_cast<int>(std::round(top + (graph.getHeight() * 0.75f))), left, right);

    g.setColour(juce::Colour(0xff243139).withAlpha(0.72f));
    for (int i = 1; i < 4; ++i)
    {
        const auto x = left + (width * static_cast<float>(i) / 4.0f);
        g.drawVerticalLine(static_cast<int>(std::round(x)), top, bottom);
    }

    juce::Path waveform;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const auto x = left + (width * static_cast<float>(i) / static_cast<float>(samples.size() - 1u));
        const auto y = juce::jlimit(top, bottom, mid - (samples[i] * halfHeight));
        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }

    const auto scopeColour = peak > 0.86f ? juce::Colour(0xfff0c94d) : juce::Colour(0xff56c7d8);
    g.setColour(scopeColour.withAlpha(0.16f + (std::min(peak, 1.0f) * 0.18f)));
    g.strokePath(waveform, juce::PathStrokeType(3.0f));
    g.setColour(scopeColour);
    g.strokePath(waveform, juce::PathStrokeType(1.25f));

    const auto peakLineWidth = graph.getWidth() * std::min(peak, 1.0f);
    g.setColour(scopeColour.withAlpha(0.45f));
    g.fillRect(juce::Rectangle<float>(left, bottom - 1.0f, peakLineWidth, 1.0f));
}

void SampleWaveformPreview::setSnapshot(const ChipperAudioProcessor::SampleWaveformSnapshot& newSnapshot)
{
    snapshot = newSnapshot;
    repaint();
}

void SampleWaveformPreview::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (bounds.isEmpty())
        return;

    g.setColour(juce::Colour(0xff10181c));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff2a3a40));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    auto graph = bounds.reduced(6.0f, 4.0f);
    const auto labelHeight = std::min(15.0f, graph.getHeight() * 0.28f);
    const auto labelArea = graph.removeFromTop(labelHeight);
    graph.removeFromTop(1.0f);

    const auto left = graph.getX();
    const auto right = graph.getRight();
    const auto top = graph.getY();
    const auto bottom = graph.getBottom();
    const auto mid = graph.getCentreY();
    const auto width = std::max(1.0f, graph.getWidth());
    const auto halfHeight = std::max(1.0f, graph.getHeight() * 0.48f);

    g.setColour(juce::Colour(0xff243139).withAlpha(0.72f));
    g.drawHorizontalLine(static_cast<int>(std::round(mid)), left, right);
    for (int i = 1; i < 4; ++i)
    {
        const auto x = left + width * static_cast<float>(i) / 4.0f;
        g.drawVerticalLine(static_cast<int>(std::round(x)), top, bottom);
    }

    if (snapshot.hasLoop && snapshot.loaded)
    {
        const auto startX = left + width * std::clamp(snapshot.loopStart, 0.0f, 1.0f);
        const auto endX = left + width * std::clamp(snapshot.loopEnd, 0.0f, 1.0f);
        const auto loopLeft = std::min(startX, endX);
        const auto loopWidth = std::max(1.0f, std::abs(endX - startX));
        g.setColour(juce::Colour(0xfff7d85a).withAlpha(0.10f));
        g.fillRect(juce::Rectangle<float>(loopLeft, top, loopWidth, graph.getHeight()));
        g.setColour(juce::Colour(0xfff7d85a).withAlpha(0.80f));
        g.drawVerticalLine(static_cast<int>(std::round(startX)), top, bottom);
        g.drawVerticalLine(static_cast<int>(std::round(endX)), top, bottom);
    }

    juce::Path waveform;
    for (size_t i = 0; i < snapshot.samples.size(); ++i)
    {
        const auto x = left + (width * static_cast<float>(i) / static_cast<float>(snapshot.samples.size() - 1u));
        const auto y = juce::jlimit(top, bottom, mid - (snapshot.samples[i] * halfHeight));
        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }

    const auto colour = snapshot.loaded ? juce::Colour(0xff56c7d8) : juce::Colour(0xff607078);
    g.setColour(colour.withAlpha(snapshot.loaded ? 0.24f : 0.14f));
    g.strokePath(waveform, juce::PathStrokeType(3.0f));
    g.setColour(colour);
    g.strokePath(waveform, juce::PathStrokeType(1.25f));

    g.setColour(snapshot.loaded ? juce::Colour(0xffd9e1e8) : juce::Colour(0xff7d8b93));
    g.setFont(juce::FontOptions(labelArea.getHeight() < 12.0f ? 8.5f : 10.0f, juce::Font::bold));
    auto labelText = snapshot.label.isNotEmpty() ? snapshot.label : juce::String("No sample");
    const auto maxLabelChars = labelArea.getWidth() < 260.0f ? 30 : 44;
    labelText = compactSampleName(labelText, maxLabelChars);
    g.drawText(labelText,
               labelArea,
               juce::Justification::centredLeft,
               true);
}

ChipperAudioProcessorEditor::ChipperAudioProcessorEditor(ChipperAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      audioProcessor(processor)
{
    setResizable(true, true);
    setResizeLimits(1180, 1320, 1800, 1700);
    setSize(1240, 1440);

    auto& state = audioProcessor.getValueTreeState();
    chipSettingsSnapshots.resize(static_cast<size_t>(chipper::parameters::chipModeChoices().size()));

    const auto blockLogo = []()
    {
        const auto lower = juce::String::charToString(static_cast<juce::juce_wchar>(0x2584));
        const auto full = juce::String::charToString(static_cast<juce::juce_wchar>(0x2588));
        const auto upper = juce::String::charToString(static_cast<juce::juce_wchar>(0x2580));
        const auto dark = juce::String::charToString(static_cast<juce::juce_wchar>(0x2593));

        juce::String text;
        text << "   " << lower << full << full << full << full << lower << "   "
             << full << full << "  " << full << full << "  "
             << full << full << "  "
             << full << full << dark << full << full << full << "   "
             << full << full << dark << full << full << full << "  "
             << dark << full << full << full << full << full << "  "
             << full << full << upper << full << full << full << "  \n";
        text << "   " << full << full << upper << " " << upper << full << "  "
             << dark << full << full << "  " << full << full << " "
             << dark << full << full << " "
             << dark << full << full << "   " << full << full << " "
             << dark << full << full << "   " << full << full << " "
             << dark << full << "   " << upper << " "
             << dark << full << full << "   " << full << full << "\n";
        text << "   " << dark << full << "    " << lower << "  "
             << full << full << upper << upper << full << full << "  "
             << full << full << "  "
             << full << full << "  " << full << full << "   "
             << full << full << "  " << full << full << "   "
             << full << full << full << "    "
             << full << full << "  " << lower << full << "\n";
        text << "   " << dark << dark << lower << " " << lower << full << full << "  "
             << dark << full << "  " << full << full << "  "
             << full << full << "  "
             << full << full << lower << full << "     "
             << full << full << lower << full << "     "
             << dark << full << "  " << lower << "  "
             << full << full << upper << upper << full << lower << "  \n";
        text << "    " << dark << full << full << full << upper << "   "
             << dark << full << "  " << full << full << "  "
             << full << full << "  "
             << full << full << "       "
             << full << full << "        "
             << full << full << full << full << "  "
             << full << full << "   " << full << full << "\n\n";
        text << "     .oO  c h i p t u n e   s y n t h   c o r e  Oo.";
        return text;
    }();
    titleLabel.setText(blockLogo, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff7d85a));
    titleLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 5.2f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    chipModeBox.addItemList(chipper::parameters::chipModeChoices(), 1);
    accuracyBox.addItemList(chipper::parameters::accuracyChoices(), 1);
    presetBox.setTextWhenNothingSelected("Browse Chip Presets");
    macroBox.addItemList(chipper::parameters::macroChoices(), 1);
    playModeBox.addItemList(chipper::parameters::playModeChoices(), 1);
    chipModeBox.setTooltip(withMidiCc("Selects the named chip engine. Each mode shows its current verification status in the footer.", chipper::parameters::id::chipMode));
    accuracyBox.setTooltip(withMidiCc("Selects requested behavior strictness. Authentic favors chip limits, Hybrid keeps labeled musical helpers, and Inspired permits looser conveniences. The footer verification badge is the implementation claim.", chipper::parameters::id::accuracy));
    presetBox.setTooltip("Browse factory and user presets for the selected chip mode. Choosing one applies the sound immediately.");
    macroBox.setTooltip(withMidiCc("Internal preset recipe. Factory/user presets set this automatically for chip-native defaults.", chipper::parameters::id::macro));
    playModeBox.setTooltip(withMidiCc("Chooses how incoming notes use the chip channels inside one patch.", chipper::parameters::id::playMode));

    const std::array<const char*, 5> headerNames { "Preset", "Chip Mode", "Strictness", "", "Play Mode" };
    for (size_t i = 0; i < headerControlLabels.size(); ++i)
    {
        headerControlLabels[i].setText(headerNames[i], juce::dontSendNotification);
        headerControlLabels[i].setJustificationType(juce::Justification::centredLeft);
        headerControlLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        headerControlLabels[i].setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(headerControlLabels[i]);
    }

    addAndMakeVisible(presetBox);
    userPresetLoadButton.setButtonText("Load");
    userPresetLoadButton.setTooltip("Import a shareable .chipperpreset file from any folder.");
    userPresetLoadButton.onClick = [this] { chooseUserPresetToLoad(); };
    addAndMakeVisible(userPresetLoadButton);
    userPresetSaveButton.setButtonText("Save");
    userPresetSaveButton.setTooltip("Save the current sound as a shareable .chipperpreset file. Loaded user presets overwrite their source file; new sounds ask for a file name.");
    userPresetSaveButton.onClick = [this]
    {
        if (currentUserPresetFile != juce::File {})
            saveUserPresetFile(currentUserPresetFile);
        else
            chooseUserPresetToSave();
    };
    addAndMakeVisible(userPresetSaveButton);
    userPresetSaveAsButton.setButtonText("Save As");
    userPresetSaveAsButton.setTooltip("Save a copy of the current sound as a new shareable .chipperpreset file.");
    userPresetSaveAsButton.onClick = [this] { chooseUserPresetToSaveAs(); };
    addAndMakeVisible(userPresetSaveAsButton);
    addAndMakeVisible(chipModeBox);
    addAndMakeVisible(accuracyBox);
    addAndMakeVisible(macroBox);
    headerControlLabels[3].setVisible(false);
    macroBox.setVisible(false);
    addAndMakeVisible(playModeBox);

    chipModeAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::chipMode, chipModeBox);
    accuracyAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::accuracy, accuracyBox);
    macroAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::macro, macroBox);
    playModeAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::playMode, playModeBox);
    macroBox.onChange = [this]()
    {
        applySelectedMacroTemplate();
    };
    presetBox.onChange = [this]()
    {
        applySelectedPreset();
    };

    addLabeledSlider(clockSlider, clockLabel, "Clock");
    clockSlider.setTextValueSuffix(" Hz");
    clockSlider.setSkewFactor(0.35);
    clockSlider.setTooltip(withMidiCc("Optional chip clock override. Zero uses the documented default for the selected mode.", chipper::parameters::id::clockHz));
    clockAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::clockHz, clockSlider);

    addLabeledSlider(dmcDirectSlider, dmcDirectLabel, "DMC Direct");
    dmcDirectSlider.setNumDecimalPlacesToDisplay(2);
    dmcDirectSlider.setTooltip(withMidiCcForRole("RP2A03 $4011 direct DAC load. Renderer and VST playback can step external .dmc bytes supplied by the user.", chipper::ChipParameterRole::nesDmcDirectLevel));
    dmcDirectLabel.setTooltip(dmcDirectSlider.getTooltip());
    dmcDirectAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::nesDmcDirectLevel, dmcDirectSlider);

    dmcRateLabel.setText("DMC Rate", juce::dontSendNotification);
    dmcRateLabel.setJustificationType(juce::Justification::centredLeft);
    dmcRateLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    dmcRateLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    dmcRateLabel.setTooltip(withMidiCcForRole("RP2A03 $4010 DMC rate index. Match this to the sample's DPCM encode rate.", chipper::ChipParameterRole::nesDmcRateIndex));
    addAndMakeVisible(dmcRateLabel);

    dmcRateBox.setTextWhenNothingSelected("Rate");
    dmcRateBox.setTooltip(dmcRateLabel.getTooltip());
    {
        const auto choices = chipper::parameters::nesDmcRateChoices();
        for (int i = 0; i < choices.size(); ++i)
            dmcRateBox.addItem(choices[i], i + 1);
    }
    addAndMakeVisible(dmcRateBox);
    dmcRateAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::nesDmcRateIndex, dmcRateBox);

    dmcSampleLabel.setText("DMC Sample", juce::dontSendNotification);
    dmcSampleLabel.setJustificationType(juce::Justification::centredLeft);
    dmcSampleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    dmcSampleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    dmcSampleLabel.setTooltip(withMidiCcForRole("Load one .dmc file or a folder of .dmc files. CC selects the active preloaded sample slot.", chipper::ChipParameterRole::nesDmcSampleSlot));
    addAndMakeVisible(dmcSampleLabel);

    for (auto* label : { &dmcPlaybackModeLabel, &dmcMapRootLabel })
    {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label->setFont(juce::FontOptions(10.5f, juce::Font::bold));
        label->setMinimumHorizontalScale(0.7f);
        label->setVisible(false);
        addAndMakeVisible(*label);
    }
    dmcPlaybackModeLabel.setText("Playback", juce::dontSendNotification);
    dmcPlaybackModeLabel.setTooltip(withMidiCcForRole("Chooses whether the loaded sample bank uses one manual slot or maps slots across MIDI notes.", chipper::ChipParameterRole::nesDmcPlaybackMode));
    dmcMapRootLabel.setText("Root", juce::dontSendNotification);
    dmcMapRootLabel.setTooltip(withMidiCcForRole("First MIDI note used by sample bank note-map modes.", chipper::ChipParameterRole::nesDmcMapRoot));

    dmcSampleStatusLabel.setJustificationType(juce::Justification::centredLeft);
    dmcSampleStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    dmcSampleStatusLabel.setFont(juce::FontOptions(10.0f));
    dmcSampleStatusLabel.setMinimumHorizontalScale(0.55f);
    dmcSampleStatusLabel.setTooltip(withMidiCcForRole("Loaded .dmc bank status. WAV-to-DMC conversion is planned, not active in this build.", chipper::ChipParameterRole::nesDmcSampleSlot));
    addAndMakeVisible(dmcSampleStatusLabel);

    dmcSampleFileButton.setButtonText("File");
    dmcSampleFileButton.setTooltip("Load one user-provided .dmc file as a one-slot bank.");
    dmcSampleFileButton.onClick = [this]
    {
        if (displayedMode == chipper::ChipMode::paula)
            choosePaulaSampleFile();
        else if (displayedMode == chipper::ChipMode::spc700)
            chooseSpc700BrrSampleFile();
        else
            chooseDmcSampleFile();
    };
    addAndMakeVisible(dmcSampleFileButton);

    dmcSampleFolderButton.setButtonText("Folder");
    dmcSampleFolderButton.setTooltip("Load a folder of user-provided .dmc files. Checked bank entries form up to 32 CC117 slots.");
    dmcSampleFolderButton.onClick = [this]
    {
        if (displayedMode == chipper::ChipMode::paula)
            choosePaulaSampleDirectory();
        else if (displayedMode == chipper::ChipMode::spc700)
            chooseSpc700BrrSampleDirectory();
        else
            chooseDmcSampleDirectory();
    };
    addAndMakeVisible(dmcSampleFolderButton);

    dmcSampleBankButton.setButtonText("Bank");
    dmcSampleBankButton.setTooltip("Open the DMC folder bank checklist. Checked files become the CC117-addressable sample slots.");
    dmcSampleBankButton.onClick = [this] { showDmcSampleBankEditor(); };
    addAndMakeVisible(dmcSampleBankButton);

    dmcPlaybackModeBox.setTooltip(withMidiCcForRole("NES-only DMC playback mode. Manual Slot uses the selected sample; Note Map maps checked bank slots upward from the DMC Map Root; Sample Map Only also mutes pulse, triangle, and noise for a DMC sample keyboard.", chipper::ChipParameterRole::nesDmcPlaybackMode));
    {
        const auto choices = chipper::parameters::nesDmcPlaybackModeChoices();
        for (int i = 0; i < choices.size(); ++i)
            dmcPlaybackModeBox.addItem(choices[i], i + 1);
    }
    dmcPlaybackModeBox.onChange = [this]()
    {
        if (suppressManualChoiceCallbacks)
            return;

        const auto selected = dmcPlaybackModeBox.getSelectedId() - 1;
        if (selected < 0)
            return;

        const auto current = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcPlaybackMode)));
        if (selected != current)
            setChoiceParameterFromUi(chipper::parameters::id::nesDmcPlaybackMode, selected);

        if (displayedMode == chipper::ChipMode::paula)
            updatePaulaSampleControls();
        else if (displayedMode == chipper::ChipMode::spc700)
            updateSpc700BrrSampleControls();
        else
            updateDmcSampleControls();
    };
    addAndMakeVisible(dmcPlaybackModeBox);
    dmcPlaybackModeAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::nesDmcPlaybackMode, dmcPlaybackModeBox);

    dmcMapRootBox.setTooltip(withMidiCcForRole("Sample-bank map root. NES DMC and SPC700/Paula sample folder slots map upward from this MIDI note when note mapping is active.", chipper::ChipParameterRole::nesDmcMapRoot));
    {
        const auto choices = chipper::parameters::midiNoteChoices();
        for (int i = 0; i < choices.size(); ++i)
            dmcMapRootBox.addItem(choices[i], i + 1);
    }
    addAndMakeVisible(dmcMapRootBox);
    dmcMapRootAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::nesDmcMapRoot, dmcMapRootBox);

    dmcLoopButton.setButtonText("Loop Sample");
    dmcLoopButton.setTooltip(withMidiCcForRole("RP2A03 $4010 DMC loop bit. Off steps the selected sample once per trigger, then holds the final DMC DAC level until retriggered. Loop repeats from byte 0 at the sample end.", chipper::ChipParameterRole::nesDmcLoop));
    addAndMakeVisible(dmcLoopButton);
    dmcLoopAttachment = std::make_unique<ButtonAttachment>(state, chipper::parameters::id::nesDmcLoop, dmcLoopButton);

    spc700LoopModeButton.setButtonText("Loop While Held");
    spc700LoopModeButton.setTooltip(withMidiCcForRole("SPC700 sample lifetime helper. Checked writes Loop While Held; unchecked writes One Shot. Preset recipes still resolve to the shown effective state until you click the toggle.", chipper::ChipParameterRole::dmgStereoRoute));
    spc700LoopModeButton.onClick = [this]()
    {
        setChoiceParameterFromUi(chipper::parameters::id::dmgStereoRoute,
                                 spc700LoopModeButton.getToggleState() ? 1 : 2);
        updateSpc700BrrSampleControls();
    };
    spc700LoopModeButton.setVisible(false);
    addAndMakeVisible(spc700LoopModeButton);

    for (auto* label : { &sampleLoopStartLabel, &sampleLoopEndLabel })
    {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label->setFont(juce::FontOptions(10.5f, juce::Font::bold));
        label->setMinimumHorizontalScale(0.7f);
        label->setVisible(false);
        addAndMakeVisible(*label);
    }
    sampleLoopStartLabel.setText("Loop Start", juce::dontSendNotification);
    sampleLoopEndLabel.setText("Loop End", juce::dontSendNotification);

    for (auto* label : { &sampleLoopStartValueLabel, &sampleLoopEndValueLabel })
    {
        label->setJustificationType(juce::Justification::centredRight);
        label->setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        label->setFont(juce::FontOptions(10.0f));
        label->setMinimumHorizontalScale(0.65f);
        label->setVisible(false);
        addAndMakeVisible(*label);
    }

    for (auto* slider : { &sampleLoopStartSlider, &sampleLoopEndSlider })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setColour(juce::Slider::trackColourId, juce::Colour(0xfff7d85a));
        slider->setColour(juce::Slider::thumbColourId, juce::Colour(0xff56c7d8));
        slider->setVisible(false);
        addAndMakeVisible(*slider);
    }
    sampleLoopStartSlider.setTooltip(withMidiCcForRole("Normalized SPC700 sample loop start. Leave at 0% to keep the BRR loop flag as the default start when present.", chipper::ChipParameterRole::spc700LoopStart));
    sampleLoopEndSlider.setTooltip(withMidiCcForRole("Normalized SPC700 sample loop end. Leave at 100% to loop to the sample end.", chipper::ChipParameterRole::spc700LoopEnd));
    sampleLoopStartLabel.setTooltip(sampleLoopStartSlider.getTooltip());
    sampleLoopEndLabel.setTooltip(sampleLoopEndSlider.getTooltip());
    sampleLoopStartAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::spc700LoopStart, sampleLoopStartSlider);
    sampleLoopEndAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::spc700LoopEnd, sampleLoopEndSlider);

    sampleWaveformPreview.setTooltip("Selected sample waveform. SPC700 mode also shades the active loop range.");
    sampleWaveformPreview.setVisible(false);
    addAndMakeVisible(sampleWaveformPreview);

    dmcSampleSlotBox.setTextWhenNothingSelected("No samples");
    dmcSampleSlotBox.setTooltip(withMidiCcForRole("Selects the active sample from the loaded .dmc bank. MIDI CC117 selects the same slot.", chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcSampleSlotBox.onChange = [this]()
    {
        if (suppressManualChoiceCallbacks)
            return;

        const auto selected = dmcSampleSlotBox.getSelectedId() - 1;
        if (selected >= 0)
        {
            setPlainParameterValueFromUi(chipper::parameters::id::nesDmcSampleSlot, static_cast<float>(selected));
            if (displayedMode == chipper::ChipMode::paula)
                updatePaulaSampleControls();
            else if (displayedMode == chipper::ChipMode::spc700)
                updateSpc700BrrSampleControls();
        }
    };
    addAndMakeVisible(dmcSampleSlotBox);

    addLabeledSlider(outputSlider, outputLabel, "Output");
    outputSlider.setTextValueSuffix(" dB");
    outputSlider.setTooltip(withMidiCc("Final plugin output level.", chipper::parameters::id::outputDb));
    outputAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::outputDb, outputSlider);

    outputScopePreview.setTooltip("Final post-trim audio scope. It follows the actual plugin output, not a symbolic oscillator preview.");
    addAndMakeVisible(outputScopePreview);

    addLabeledSlider(stereoSpreadSlider, stereoSpreadLabel, "Stereo Spread");
    stereoSpreadSlider.setNumDecimalPlacesToDisplay(2);
    stereoSpreadSlider.setTooltip(withMidiCc("Modern stereo spread for chips that expose it; zero preserves mono output.", chipper::parameters::id::stereoSpread));
    stereoSpreadAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::stereoSpread, stereoSpreadSlider);

    stereoSpreadValueLabel.setJustificationType(juce::Justification::centredLeft);
    stereoSpreadValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    stereoSpreadValueLabel.setFont(juce::FontOptions(11.0f));
    stereoSpreadValueLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(stereoSpreadValueLabel);

    addLabeledSlider(envelopeDecaySlider, envelopeDecayLabel, "Envelope Decay");
    envelopeDecaySlider.setNumDecimalPlacesToDisplay(2);
    envelopeDecaySlider.setTooltip(withMidiCc("Maps musical decay to the selected chip's native envelope, gain, or volume-register helper. Zero preserves the preset recipe's original envelope/level behavior.", chipper::parameters::id::envelopeDecay));
    envelopeDecayAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::envelopeDecay, envelopeDecaySlider);

    envelopeDecayValueLabel.setJustificationType(juce::Justification::centredLeft);
    envelopeDecayValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    envelopeDecayValueLabel.setFont(juce::FontOptions(11.0f));
    envelopeDecayValueLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(envelopeDecayValueLabel);

    for (size_t voice = 0; voice < sidEnvelopeVoiceLabels.size(); ++voice)
    {
        auto& label = sidEnvelopeVoiceLabels[voice];
        label.setText(juce::String("Voice ") + juce::String(static_cast<int>(voice + 1u)), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label.setVisible(false);
        addAndMakeVisible(label);

        auto& preview = sidEnvelopePreviews[voice];
        preview.setTooltip(juce::String("Register-resolved SID voice ") + juce::String(static_cast<int>(voice + 1u)) + " ADSR envelope preview.");
        preview.setVisible(false);
        addAndMakeVisible(preview);
    }

    ymEnvelopePreview.setTooltip("YM/AY hardware envelope shape preview. It follows register 13 when the envelope shape is enabled.");
    ymEnvelopePreview.setVisible(false);
    addAndMakeVisible(ymEnvelopePreview);

    static constexpr std::array<const char*, sidAdsrFieldCount> sidAdsrHeaders { "Atk", "Dec", "Sus", "Rel" };
    static constexpr std::array<const char*, sidAdsrFieldCount> sidAdsrHeaderTooltips {
        "SID attack nibble override columns.",
        "SID decay nibble override columns.",
        "SID sustain nibble override columns.",
        "SID release nibble override columns."
    };

    for (size_t i = 0; i < sidAdsrHeaderLabels.size(); ++i)
    {
        auto& label = sidAdsrHeaderLabels[i];
        label.setText(sidAdsrHeaders[i], juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        label.setTooltip(sidAdsrHeaderTooltips[i]);
        label.setVisible(false);
        addAndMakeVisible(label);
    }

    for (size_t i = 0; i < sidAdsrBoxes.size(); ++i)
    {
        auto& label = sidAdsrLabels[i];
        label.setText(sidAdsrFieldLabel(i % sidAdsrFieldCount), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label.setMinimumHorizontalScale(0.7f);
        label.setVisible(false);
        addAndMakeVisible(label);

        auto& valueLabel = sidAdsrValueLabels[i];
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        valueLabel.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        valueLabel.setMinimumHorizontalScale(0.55f);
        valueLabel.setVisible(false);
        addAndMakeVisible(valueLabel);

        auto& box = sidAdsrBoxes[i];
        box.addItemList(chipper::parameters::sidAdsrNibbleChoices(), 1);
        box.setJustificationType(juce::Justification::centred);
        box.setVisible(false);
        box.setTooltip(withMidiCcForRole("SID ADSR nibble override.", sidAdsrRole(i)));
        box.onChange = [this, i]()
        {
            const auto selected = sidAdsrBoxes[i].getSelectedItemIndex();
            if (selected >= 0)
            {
                setChoiceParameterFromUi(sidAdsrParameterId(i), selected);
                updateLiveControlReadouts();
            }
        };
        addAndMakeVisible(box);

        auto& slider = sidAdsrSliders[i];
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(0.0, static_cast<double>(sidAdsrChoiceCount - 1u), 1.0);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xfff7d85a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff56c7d8));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff4f7f8));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff10181c));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff53636c));
        slider.textFromValueFunction = [](double value)
        {
            const auto choice = static_cast<int>(std::round(value));
            return choice <= 0 ? juce::String("Preset") : juce::String(choice - 1);
        };
        slider.valueFromTextFunction = [](const juce::String& text)
        {
            const auto trimmed = text.trim();
            if (trimmed.equalsIgnoreCase("follow") || trimmed.equalsIgnoreCase("f"))
                return 0.0;

            return static_cast<double>(std::clamp(trimmed.getIntValue() + 1, 0, static_cast<int>(sidAdsrChoiceCount - 1u)));
        };
        slider.setTooltip(withMidiCcForRole("SID ADSR nibble override.", sidAdsrRole(i)));
        slider.onValueChange = [this, i]()
        {
            setChoiceParameterFromUi(sidAdsrParameterId(i), static_cast<int>(std::round(sidAdsrSliders[i].getValue())));
            updateLiveControlReadouts();
        };
        slider.setVisible(false);
        addAndMakeVisible(slider);
    }

    for (size_t i = 0; i < fmOperatorNameLabels.size(); ++i)
    {
        auto& nameLabel = fmOperatorNameLabels[i];
        nameLabel.setJustificationType(juce::Justification::centredLeft);
        nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        nameLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        nameLabel.setMinimumHorizontalScale(0.65f);
        nameLabel.setVisible(false);
        addAndMakeVisible(nameLabel);

        auto& valueLabel = fmOperatorValueLabels[i];
        valueLabel.setJustificationType(juce::Justification::centredLeft);
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
        valueLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff202c33));
        valueLabel.setFont(juce::FontOptions(10.0f));
        valueLabel.setMinimumHorizontalScale(0.45f);
        valueLabel.setVisible(false);
        addAndMakeVisible(valueLabel);
    }

    const std::array<const char*, 4> ids {
        chipper::parameters::id::macroControl1,
        chipper::parameters::id::macroControl2,
        chipper::parameters::id::macroControl3,
        chipper::parameters::id::macroControl4
    };

    for (size_t i = 0; i < nativeSliders.size(); ++i)
    {
        nativeGroupLabels[i].setJustificationType(juce::Justification::centredLeft);
        nativeGroupLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        nativeGroupLabels[i].setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(nativeGroupLabels[i]);

        addLabeledSlider(nativeSliders[i], nativeLabels[i], "Native Control");
        nativeSliders[i].setNumDecimalPlacesToDisplay(2);
        nativeAttachments[i] = std::make_unique<SliderAttachment>(state, ids[i], nativeSliders[i]);
    }

    for (auto& valueLabel : controlValueLabels)
    {
        valueLabel.setJustificationType(juce::Justification::centredLeft);
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        valueLabel.setFont(juce::FontOptions(11.0f));
        valueLabel.setMinimumHorizontalScale(0.75f);
        addAndMakeVisible(valueLabel);
    }

    const std::array<const char*, 4> dutyLabels { "12.5%", "25%", "50%", "75%" };
    for (size_t i = 0; i < pulseDutyButtons.size(); ++i)
    {
        auto& button = pulseDutyButtons[i];
        button.setButtonText(dutyLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("Pulse duty ") + dutyLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            const auto value = static_cast<float>(i) / static_cast<float>(pulseDutyButtons.size() - 1u);
            setParameterValueFromUi(chipper::parameters::id::macroControl1, value);
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    pulse2DutyLabel.setText("Pulse 2 Duty", juce::dontSendNotification);
    pulse2DutyLabel.setJustificationType(juce::Justification::centredLeft);
    pulse2DutyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    pulse2DutyLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    pulse2DutyLabel.setVisible(false);
    addAndMakeVisible(pulse2DutyLabel);

    pulse2DutyValueLabel.setJustificationType(juce::Justification::centredRight);
    pulse2DutyValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    pulse2DutyValueLabel.setFont(juce::FontOptions(10.0f));
    pulse2DutyValueLabel.setMinimumHorizontalScale(0.55f);
    pulse2DutyValueLabel.setVisible(false);
    addAndMakeVisible(pulse2DutyValueLabel);

    const std::array<const char*, pulse2DutyCount> pulse2Labels { "Preset", "12.5%", "25%", "50%", "75%" };
    for (size_t i = 0; i < pulse2DutyButtons.size(); ++i)
    {
        auto& button = pulse2DutyButtons[i];
        button.setButtonText(pulse2Labels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("Pulse 2 duty ") + pulse2Labels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::pulse2Duty, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    const std::array<const char*, toneNoiseMixCount> toneNoiseLabels { "Noise", "Tone", "Both" };
    for (size_t i = 0; i < toneNoiseMixButtons.size(); ++i)
    {
        auto& button = toneNoiseMixButtons[i];
        button.setButtonText(toneNoiseLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("YM2149 tone/noise mixer: ") + toneNoiseLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            const auto value = static_cast<float>(i) / static_cast<float>(toneNoiseMixButtons.size() - 1u);
            setParameterValueFromUi(chipper::parameters::id::macroControl4, value);
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    waveShapeLabel.setText("Wave Shape", juce::dontSendNotification);
    waveShapeLabel.setJustificationType(juce::Justification::centredLeft);
    waveShapeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    waveShapeLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    waveShapeLabel.setVisible(false);
    addAndMakeVisible(waveShapeLabel);

    waveShapeValueLabel.setJustificationType(juce::Justification::centredLeft);
    waveShapeValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    waveShapeValueLabel.setFont(juce::FontOptions(11.0f));
    waveShapeValueLabel.setMinimumHorizontalScale(0.75f);
    waveShapeValueLabel.setVisible(false);
    addAndMakeVisible(waveShapeValueLabel);

    const std::array<const char*, waveShapeCount> waveLabels { "RAM", "Tri", "Saw", "Pulse", "Steps" };
    for (size_t i = 0; i < waveShapeButtons.size(); ++i)
    {
        auto& button = waveShapeButtons[i];
        button.setButtonText(waveLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("DMG Wave RAM shape: ") + waveLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::waveShape, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    fmAlgorithmBox.setVisible(false);
    fmAlgorithmBox.setTooltip(withMidiCcForRole("FM algorithm register choice.", chipper::ChipParameterRole::waveShape));
    fmAlgorithmBox.onChange = [this]()
    {
        if (suppressManualChoiceCallbacks)
            return;

        const auto selected = fmAlgorithmBox.getSelectedId() - 1;
        if (selected >= 0)
        {
            setChoiceParameterFromUi(chipper::parameters::id::waveShape, selected);
            updateLiveControlReadouts();
        }
    };
    addAndMakeVisible(fmAlgorithmBox);

    fmAlgorithmPreview.setVisible(false);
    fmAlgorithmPreview.setTooltip("Four-operator FM algorithm signal flow. Cyan operators modulate, yellow operators reach output.");
    addAndMakeVisible(fmAlgorithmPreview);

    oplWaveformBox.setVisible(false);
    oplWaveformBox.setTooltip(withMidiCcForRole("OPL2 operator waveform register choice.", chipper::ChipParameterRole::waveShape));
    oplWaveformBox.onChange = [this]()
    {
        if (suppressManualChoiceCallbacks)
            return;

        const auto selected = oplWaveformBox.getSelectedId() - 1;
        if (selected >= 0)
        {
            setChoiceParameterFromUi(chipper::parameters::id::waveShape, selected);
            updateLiveControlReadouts();
        }
    };
    addAndMakeVisible(oplWaveformBox);

    oplWaveformPreview.setVisible(false);
    oplWaveformPreview.setTooltip("OPL2/YM3812 operator waveform preview. It follows the waveform register written to both operators.");
    addAndMakeVisible(oplWaveformPreview);

    opllInstrumentBox.setVisible(false);
    opllInstrumentBox.setTooltip(withMidiCcForRole("YM2413 ROM preset instrument choice.", chipper::ChipParameterRole::waveShape));
    opllInstrumentBox.onChange = [this]()
    {
        if (suppressManualChoiceCallbacks)
            return;

        const auto selected = opllInstrumentBox.getSelectedId() - 1;
        if (selected >= 0)
        {
            setChoiceParameterFromUi(chipper::parameters::id::waveShape, selected);
            updateLiveControlReadouts();
        }
    };
    addAndMakeVisible(opllInstrumentBox);

    const std::array<const char*, sidVoiceWaveCount> voiceWaveLabels { "V1", "V2", "V3" };
    for (size_t i = 0; i < sidVoiceWaveBoxes.size(); ++i)
    {
        auto& label = sidVoiceWaveLabels[i];
        label.setText(voiceWaveLabels[i], juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label.setVisible(false);
        addAndMakeVisible(label);

        auto& box = sidVoiceWaveBoxes[i];
        box.addItemList(chipper::parameters::sidVoiceWaveShapeChoices(), 1);
        box.setVisible(false);
        box.setTooltip(withMidiCcForRole("SID voice waveform register choice.", sidVoiceWaveRole(i)));
        box.onChange = [this, i]()
        {
            if (suppressManualChoiceCallbacks)
                return;

            const auto selected = sidVoiceWaveBoxes[i].getSelectedId() - 1;
            if (selected >= 0)
            {
                setChoiceParameterFromUi(sidVoiceWaveParameterId(i), selected);
                updateLiveControlReadouts();
            }
        };
        addAndMakeVisible(box);

        auto& pulseLabel = sidVoicePulseWidthLabels[i];
        pulseLabel.setText("PW", juce::dontSendNotification);
        pulseLabel.setJustificationType(juce::Justification::centredLeft);
        pulseLabel.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        pulseLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        pulseLabel.setMinimumHorizontalScale(0.70f);
        pulseLabel.setVisible(false);
        addAndMakeVisible(pulseLabel);

        auto& pulseSlider = sidVoicePulseWidthSliders[i];
        pulseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        pulseSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pulseSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xfff7d85a));
        pulseSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff56c7d8));
        pulseSlider.setTooltip(withMidiCcForRole("SID voice pulse-width register.", sidVoicePulseWidthRole(i)));
        pulseSlider.setVisible(false);
        addAndMakeVisible(pulseSlider);
        sidVoicePulseWidthAttachments[i] = std::make_unique<SliderAttachment>(state, sidVoicePulseWidthParameterId(i), pulseSlider);

        auto& pulseValue = sidVoicePulseWidthValueLabels[i];
        pulseValue.setJustificationType(juce::Justification::centredRight);
        pulseValue.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        pulseValue.setFont(juce::FontOptions(9.5f));
        pulseValue.setMinimumHorizontalScale(0.60f);
        pulseValue.setTooltip(pulseSlider.getTooltip());
        pulseValue.setVisible(false);
        addAndMakeVisible(pulseValue);

    }

    for (size_t i = 0; i < hucVoiceWaveBoxes.size(); ++i)
    {
        auto& label = hucVoiceWaveLabels[i];
        label.setText(juce::String("Ch ") + juce::String(static_cast<int>(i + 1u)), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label.setVisible(false);
        addAndMakeVisible(label);

        auto& box = hucVoiceWaveBoxes[i];
        box.addItemList({ "Preset", "Ramp", "Tri", "Pulse", "Noise" }, 1);
        box.setVisible(false);
        box.setTooltip(withMidiCcForRole("HuC6280 per-channel wave RAM shape.", hucVoiceWaveRole(i)));
        box.onChange = [this, i]()
        {
            if (suppressManualChoiceCallbacks)
                return;

            const auto selected = hucVoiceWaveBoxes[i].getSelectedId() - 1;
            if (selected >= 0)
            {
                if (displayedMode == chipper::ChipMode::spc700)
                    setPlainParameterValueFromUi(spc700VoiceSampleParameterId(i), static_cast<float>(selected));
                else
                    setChoiceParameterFromUi(hucVoiceWaveParameterId(i), selected);
                updateLiveControlReadouts();
            }
        };
        addAndMakeVisible(box);
    }

    for (size_t i = 0; i < paulaVoiceSampleBoxes.size(); ++i)
    {
        auto& label = paulaVoiceSampleLabels[i];
        label.setText("Sample Slot", juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        label.setVisible(false);
        addAndMakeVisible(label);

        auto& box = paulaVoiceSampleBoxes[i];
        box.setVisible(false);
        box.setTextWhenNothingSelected("Preset");
        box.onChange = [this, i]()
        {
            if (suppressManualChoiceCallbacks)
                return;

            const auto selected = paulaVoiceSampleBoxes[i].getSelectedId() - 1;
            if (selected >= 0)
            {
                setPlainParameterValueFromUi(spc700VoiceSampleParameterId(i), static_cast<float>(selected));
                updateLiveControlReadouts();
            }
        };
        addAndMakeVisible(box);
    }

    dmgWaveLevelLabel.setText("Wave Level", juce::dontSendNotification);
    dmgWaveLevelLabel.setJustificationType(juce::Justification::centredLeft);
    dmgWaveLevelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    dmgWaveLevelLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    dmgWaveLevelLabel.setVisible(false);
    addAndMakeVisible(dmgWaveLevelLabel);

    dmgWaveLevelValueLabel.setJustificationType(juce::Justification::centredLeft);
    dmgWaveLevelValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    dmgWaveLevelValueLabel.setFont(juce::FontOptions(11.0f));
    dmgWaveLevelValueLabel.setMinimumHorizontalScale(0.75f);
    dmgWaveLevelValueLabel.setVisible(false);
    addAndMakeVisible(dmgWaveLevelValueLabel);

    const std::array<const char*, dmgWaveLevelCount> dmgWaveLevelLabels { "Preset", "Mute", "100%", "50%", "25%" };
    for (size_t i = 0; i < dmgWaveLevelButtons.size(); ++i)
    {
        auto& button = dmgWaveLevelButtons[i];
        button.setButtonText(dmgWaveLevelLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("DMG NR32 wave level: ") + dmgWaveLevelLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::dmgWaveLevel, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    dmgStereoRouteLabel.setText("Stereo Route", juce::dontSendNotification);
    dmgStereoRouteLabel.setJustificationType(juce::Justification::centredLeft);
    dmgStereoRouteLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    dmgStereoRouteLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    dmgStereoRouteLabel.setVisible(false);
    addAndMakeVisible(dmgStereoRouteLabel);

    dmgStereoRouteValueLabel.setJustificationType(juce::Justification::centredLeft);
    dmgStereoRouteValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    dmgStereoRouteValueLabel.setFont(juce::FontOptions(11.0f));
    dmgStereoRouteValueLabel.setMinimumHorizontalScale(0.75f);
    dmgStereoRouteValueLabel.setVisible(false);
    addAndMakeVisible(dmgStereoRouteValueLabel);

    const std::array<const char*, dmgStereoRouteCount> dmgStereoRouteLabels { "Preset", "Both", "Left", "Right", "Split" };
    for (size_t i = 0; i < dmgStereoRouteButtons.size(); ++i)
    {
        auto& button = dmgStereoRouteButtons[i];
        button.setButtonText(dmgStereoRouteLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("DMG NR51 stereo route: ") + dmgStereoRouteLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::dmgStereoRoute, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    dmgStereoRouteBox.setVisible(false);
    dmgStereoRouteBox.setTooltip(withMidiCcForRole("SPC700-style sample playback mode.", chipper::ChipParameterRole::dmgStereoRoute));
    dmgStereoRouteBox.onChange = [this]()
    {
        if (suppressManualChoiceCallbacks)
            return;

        const auto selected = dmgStereoRouteBox.getSelectedId() - 1;
        if (selected >= 0)
        {
            setChoiceParameterFromUi(chipper::parameters::id::dmgStereoRoute, selected);
            updateLiveControlReadouts();
        }
    };
    addAndMakeVisible(dmgStereoRouteBox);

    ymEnvelopeShapeLabel.setText("Envelope Shape", juce::dontSendNotification);
    ymEnvelopeShapeLabel.setJustificationType(juce::Justification::centredLeft);
    ymEnvelopeShapeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    ymEnvelopeShapeLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    ymEnvelopeShapeLabel.setVisible(false);
    addAndMakeVisible(ymEnvelopeShapeLabel);

    ymEnvelopeShapeValueLabel.setJustificationType(juce::Justification::centredLeft);
    ymEnvelopeShapeValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    ymEnvelopeShapeValueLabel.setFont(juce::FontOptions(11.0f));
    ymEnvelopeShapeValueLabel.setMinimumHorizontalScale(0.75f);
    ymEnvelopeShapeValueLabel.setVisible(false);
    addAndMakeVisible(ymEnvelopeShapeValueLabel);

    for (size_t i = 0; i < ymEnvelopeShapeButtons.size(); ++i)
    {
        auto& button = ymEnvelopeShapeButtons[i];
        button.setClickingTogglesState(false);
        button.setTooltip("YM2149 envelope shape.");
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::ymEnvelopeShape, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    sidFilterModeBox.setVisible(false);
    sidFilterModeBox.setTooltip(withMidiCcForRole("SID filter mode bits.", chipper::ChipParameterRole::ymEnvelopeShape));
    sidFilterModeBox.onChange = [this]()
    {
        if (suppressManualChoiceCallbacks)
            return;

        const auto selected = sidFilterModeBox.getSelectedItemIndex();
        if (selected >= 0)
        {
            setChoiceParameterFromUi(chipper::parameters::id::ymEnvelopeShape, selected);
            updateLiveControlReadouts();
        }
    };
    addAndMakeVisible(sidFilterModeBox);

    sidFilterRoutingLabel.setText("Filter Routing", juce::dontSendNotification);
    sidFilterRoutingLabel.setJustificationType(juce::Justification::centredLeft);
    sidFilterRoutingLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    sidFilterRoutingLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    sidFilterRoutingLabel.setVisible(false);
    addAndMakeVisible(sidFilterRoutingLabel);

    sidFilterRoutingBox.addItemList(chipper::parameters::sidFilterRoutingChoices(), 1);
    sidFilterRoutingBox.setVisible(false);
    sidFilterRoutingBox.setTooltip(withMidiCcForRole("SID filter input routing.", chipper::ChipParameterRole::sidFilterRouting));
    sidFilterRoutingAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::sidFilterRouting, sidFilterRoutingBox);
    addAndMakeVisible(sidFilterRoutingBox);

    sidFilterRoutingValueLabel.setJustificationType(juce::Justification::centredLeft);
    sidFilterRoutingValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    sidFilterRoutingValueLabel.setFont(juce::FontOptions(10.5f));
    sidFilterRoutingValueLabel.setMinimumHorizontalScale(0.6f);
    sidFilterRoutingValueLabel.setVisible(false);
    addAndMakeVisible(sidFilterRoutingValueLabel);

    ymChannelMixLabel.setText("Channel Mix", juce::dontSendNotification);
    ymChannelMixLabel.setJustificationType(juce::Justification::centredLeft);
    ymChannelMixLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    ymChannelMixLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    ymChannelMixLabel.setVisible(false);
    addAndMakeVisible(ymChannelMixLabel);

    ymChannelMixValueLabel.setJustificationType(juce::Justification::centredLeft);
    ymChannelMixValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    ymChannelMixValueLabel.setFont(juce::FontOptions(11.0f));
    ymChannelMixValueLabel.setMinimumHorizontalScale(0.75f);
    ymChannelMixValueLabel.setVisible(false);
    addAndMakeVisible(ymChannelMixValueLabel);

    const std::array<const char*, ymChannelMixCount> ymChannelMixLabelText { "A", "B", "C" };
    for (size_t i = 0; i < ymChannelMixBoxes.size(); ++i)
    {
        auto& label = ymChannelMixLabels[i];
        label.setText(ymChannelMixLabelText[i], juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label.setVisible(false);
        addAndMakeVisible(label);

        auto& box = ymChannelMixBoxes[i];
        box.addItemList(chipper::parameters::ymChannelMixChoices(), 1);
        box.setVisible(false);
        box.setTooltip(withMidiCcForRole("YM/AY channel tone/noise mixer override.", ymChannelMixRole(i)));
        box.onChange = [this, i]()
        {
            if (suppressManualChoiceCallbacks)
                return;

            const auto selected = ymChannelMixBoxes[i].getSelectedItemIndex();
            if (selected >= 0)
            {
                setChoiceParameterFromUi(ymChannelMixParameterId(i), selected);
                updateLiveControlReadouts();
            }
        };
        addAndMakeVisible(box);
    }

    snNoiseModeLabel.setText("Noise Mode", juce::dontSendNotification);
    snNoiseModeLabel.setJustificationType(juce::Justification::centredLeft);
    snNoiseModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    snNoiseModeLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    snNoiseModeLabel.setVisible(false);
    addAndMakeVisible(snNoiseModeLabel);

    snNoiseModeValueLabel.setJustificationType(juce::Justification::centredLeft);
    snNoiseModeValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    snNoiseModeValueLabel.setFont(juce::FontOptions(11.0f));
    snNoiseModeValueLabel.setMinimumHorizontalScale(0.75f);
    snNoiseModeValueLabel.setVisible(false);
    addAndMakeVisible(snNoiseModeValueLabel);

    const std::array<const char*, snNoiseModeCount> snNoiseLabels { "Preset", "P-Lo", "P-Hi", "W-Lo", "W-T3" };
    for (size_t i = 0; i < snNoiseModeButtons.size(); ++i)
    {
        auto& button = snNoiseModeButtons[i];
        button.setButtonText(snNoiseLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("SN76489 noise register mode: ") + snNoiseLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::snNoiseMode, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    snNoiseModeBox.setVisible(false);
    snNoiseModeBox.setTooltip(withMidiCcForRole("SN76489 noise-control register choice.", chipper::ChipParameterRole::snNoiseMode));
    snNoiseModeBox.onChange = [this]()
    {
        if (suppressManualChoiceCallbacks)
            return;

        const auto selected = snNoiseModeBox.getSelectedId() - 1;
        if (selected >= 0)
        {
            setChoiceParameterFromUi(chipper::parameters::id::snNoiseMode, selected);
            updateLiveControlReadouts();
        }
    };
    addAndMakeVisible(snNoiseModeBox);

    statusLabel.setFont(juce::FontOptions(12.0f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1f2a34));
    statusLabel.setText("Loading chip core...", juce::dontSendNotification);
    statusLabel.setMinimumHorizontalScale(0.70f);
    addAndMakeVisible(statusLabel);

    buildLabel.setFont(juce::FontOptions(11.0f));
    buildLabel.setJustificationType(juce::Justification::centredRight);
    buildLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7f9099));
    buildLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1f2a34));
    buildLabel.setText(juce::String("Build ") + chipper::build::label, juce::dontSendNotification);
    buildLabel.setTooltip(juce::String("Built ") + chipper::build::builtAtUtc + " from " + chipper::build::gitState + " source");
    addAndMakeVisible(buildLabel);

    midiCcLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    midiCcLabel.setJustificationType(juce::Justification::centred);
    midiCcLabel.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
    midiCcLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff18252d));
    midiCcLabel.setText(midiCcRangeLabel(), juce::dontSendNotification);
    midiCcLabel.setTooltip(midiCcMapTooltip());
    addAndMakeVisible(midiCcLabel);

    chipSummaryLabel.setFont(juce::FontOptions(14.0f));
    chipSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    chipSummaryLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    chipSummaryLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(chipSummaryLabel);

    for (size_t i = 0; i < moduleTitleLabels.size(); ++i)
    {
        moduleTitleLabels[i].setFont(juce::FontOptions(16.0f, juce::Font::bold));
        moduleTitleLabels[i].setJustificationType(juce::Justification::centredLeft);
        moduleTitleLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xfff0c94d));
        addAndMakeVisible(moduleTitleLabels[i]);

        moduleSummaryLabels[i].setFont(juce::FontOptions(11.0f));
        moduleSummaryLabels[i].setJustificationType(juce::Justification::centredLeft);
        moduleSummaryLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffc8d4dc));
        moduleSummaryLabels[i].setMinimumHorizontalScale(0.70f);
        addAndMakeVisible(moduleSummaryLabels[i]);

        for (auto& itemLabel : moduleItemLabels[i])
        {
            itemLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            itemLabel.setJustificationType(juce::Justification::centredLeft);
            itemLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdbe8e5));
            itemLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff243038));
            itemLabel.setMinimumHorizontalScale(0.68f);
            addAndMakeVisible(itemLabel);
        }
    }

    const std::array<const char*, sourceChannelCount> sourceIds {
        chipper::parameters::id::source1Enabled,
        chipper::parameters::id::source2Enabled,
        chipper::parameters::id::source3Enabled,
        chipper::parameters::id::source4Enabled,
        chipper::parameters::id::source5Enabled,
        chipper::parameters::id::source6Enabled,
        chipper::parameters::id::source7Enabled,
        chipper::parameters::id::source8Enabled,
        chipper::parameters::id::source9Enabled
    };
    const std::array<const char*, sourceChannelCount> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level,
        chipper::parameters::id::source5Level,
        chipper::parameters::id::source6Level,
        chipper::parameters::id::source7Level,
        chipper::parameters::id::source8Level,
        chipper::parameters::id::source9Level
    };

    for (size_t i = 0; i < sourceChannelButtons.size(); ++i)
    {
        auto& button = sourceChannelButtons[i];
        button.setClickingTogglesState(true);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff203a30));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff93a2aa));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffe6f2f0));
        button.setVisible(false);
        addAndMakeVisible(button);
        sourceEnableAttachments[i] = std::make_unique<ButtonAttachment>(state, sourceIds[i], button);

        auto& slider = sourceLevelSliders[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xfff7d85a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff56c7d8));
        slider.setTooltip(withMidiCc("Source level trim.", sourceLevelIds[i]));
        slider.setVisible(false);
        addAndMakeVisible(slider);
        sourceLevelAttachments[i] = std::make_unique<SliderAttachment>(state, sourceLevelIds[i], slider);

        auto& levelLabel = sourceLevelLabels[i];
        levelLabel.setText("Level", juce::dontSendNotification);
        levelLabel.setJustificationType(juce::Justification::centredLeft);
        levelLabel.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        levelLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        levelLabel.setMinimumHorizontalScale(0.70f);
        levelLabel.setTooltip(withMidiCc("Source level trim.", sourceLevelIds[i]));
        levelLabel.setVisible(false);
        addAndMakeVisible(levelLabel);

        auto& valueLabel = sourceLevelValueLabels[i];
        valueLabel.setJustificationType(juce::Justification::centredRight);
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        valueLabel.setFont(juce::FontOptions(10.0f));
        valueLabel.setMinimumHorizontalScale(0.70f);
        valueLabel.setTooltip(withMidiCc("Source level trim.", sourceLevelIds[i]));
        valueLabel.setVisible(false);
        addAndMakeVisible(valueLabel);

        auto& scope = sourcePreviewScopes[i];
        scope.setTooltip("Register-resolved source preview.");
        scope.setVisible(false);
        addAndMakeVisible(scope);
    }

    globalStripLabel.setText("Performance Macros", juce::dontSendNotification);
    globalStripLabel.setJustificationType(juce::Justification::centredLeft);
    globalStripLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0c94d));
    globalStripLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(globalStripLabel);

    macroSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    macroSummaryLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    macroSummaryLabel.setFont(juce::FontOptions(12.0f));
    macroSummaryLabel.setMinimumHorizontalScale(0.60f);
    addAndMakeVisible(macroSummaryLabel);

    updateDescriptorText();
    updateLiveControlReadouts();
    outputScopePreview.setSamples(audioProcessor.outputScopeSnapshot());
    startTimerHz(12);
}

void ChipperAudioProcessorEditor::applyChipTheme()
{
    const auto theme = chipThemeFor(displayedMode);

    const auto styleSlider = [&theme](juce::Slider& slider)
    {
        slider.setColour(juce::Slider::trackColourId, theme.primary);
        slider.setColour(juce::Slider::thumbColourId, theme.accent);
        slider.setColour(juce::Slider::textBoxTextColourId, theme.text);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, theme.background);
        slider.setColour(juce::Slider::textBoxOutlineColourId, theme.outline);
    };

    const auto styleCombo = [&theme](juce::ComboBox& box)
    {
        box.setColour(juce::ComboBox::backgroundColourId, theme.sourceCard);
        box.setColour(juce::ComboBox::textColourId, theme.text);
        box.setColour(juce::ComboBox::outlineColourId, theme.outline);
        box.setColour(juce::ComboBox::arrowColourId, theme.text);
    };

    const auto styleButton = [&theme](juce::TextButton& button)
    {
        button.setColour(juce::TextButton::buttonColourId, theme.sourceCard);
        button.setColour(juce::TextButton::buttonOnColourId, theme.primary);
        button.setColour(juce::TextButton::textColourOffId, theme.text);
        button.setColour(juce::TextButton::textColourOnId, theme.darkText);
    };

    titleLabel.setColour(juce::Label::textColourId, theme.primary);
    chipSummaryLabel.setColour(juce::Label::textColourId, theme.text);
    globalStripLabel.setColour(juce::Label::textColourId, theme.primary);
    macroSummaryLabel.setColour(juce::Label::textColourId, theme.mutedText);
    midiCcLabel.setColour(juce::Label::textColourId, theme.accent);
    midiCcLabel.setColour(juce::Label::backgroundColourId, theme.panel);
    statusLabel.setColour(juce::Label::backgroundColourId, theme.panelAlt);
    buildLabel.setColour(juce::Label::backgroundColourId, theme.panelAlt);

    for (auto& label : headerControlLabels)
        label.setColour(juce::Label::textColourId, theme.mutedText);

    for (auto& label : moduleTitleLabels)
        label.setColour(juce::Label::textColourId, theme.primary);

    for (auto& label : moduleSummaryLabels)
        label.setColour(juce::Label::textColourId, theme.text.withAlpha(0.88f));

    for (auto& moduleItems : moduleItemLabels)
    {
        for (auto& label : moduleItems)
        {
            label.setColour(juce::Label::textColourId, theme.text);
            label.setColour(juce::Label::backgroundColourId, theme.sourceCard);
        }
    }

    for (auto* label : std::array<juce::Label*, 23> {
             &clockLabel, &dmcDirectLabel, &dmcRateLabel, &dmcSampleLabel, &sampleLoopStartLabel,
             &sampleLoopEndLabel, &outputLabel, &stereoSpreadLabel, &envelopeDecayLabel,
             &waveShapeLabel, &pulse2DutyLabel, &dmgWaveLevelLabel, &dmgStereoRouteLabel,
             &ymEnvelopeShapeLabel, &sidFilterRoutingLabel, &ymChannelMixLabel, &snNoiseModeLabel,
             &nativeGroupLabels[0], &nativeGroupLabels[1], &nativeGroupLabels[2], &nativeGroupLabels[3],
             &sidFilterRoutingValueLabel, &ymChannelMixValueLabel })
    {
        label->setColour(juce::Label::textColourId, theme.accent);
    }

    for (auto* label : std::array<juce::Label*, 11> {
             &stereoSpreadValueLabel, &envelopeDecayValueLabel, &waveShapeValueLabel, &pulse2DutyValueLabel,
             &dmgWaveLevelValueLabel, &dmgStereoRouteValueLabel, &ymEnvelopeShapeValueLabel,
             &snNoiseModeValueLabel, &dmcSampleStatusLabel, &sampleLoopStartValueLabel, &sampleLoopEndValueLabel })
    {
        label->setColour(juce::Label::textColourId, theme.mutedText);
    }

    for (auto& label : sidEnvelopeVoiceLabels)
        label.setColour(juce::Label::textColourId, theme.accent);
    for (auto& label : sidAdsrHeaderLabels)
        label.setColour(juce::Label::textColourId, theme.accent);
    for (auto& label : sidAdsrLabels)
        label.setColour(juce::Label::textColourId, theme.accent);
    for (auto& label : sidAdsrValueLabels)
        label.setColour(juce::Label::textColourId, theme.mutedText);
    for (auto& label : fmOperatorNameLabels)
        label.setColour(juce::Label::textColourId, theme.accent);
    for (auto& label : fmOperatorValueLabels)
        label.setColour(juce::Label::textColourId, theme.mutedText);
    for (auto& label : sourceLevelLabels)
        label.setColour(juce::Label::textColourId, theme.accent);
    for (auto& label : sourceLevelValueLabels)
        label.setColour(juce::Label::textColourId, theme.mutedText);

    for (auto* slider : std::array<juce::Slider*, 11> {
             &clockSlider, &dmcDirectSlider, &sampleLoopStartSlider, &sampleLoopEndSlider, &outputSlider,
             &stereoSpreadSlider, &envelopeDecaySlider, &nativeSliders[0], &nativeSliders[1],
             &nativeSliders[2], &nativeSliders[3] })
    {
        styleSlider(*slider);
    }

    for (auto& slider : sourceLevelSliders)
        styleSlider(slider);
    for (auto& slider : sidVoicePulseWidthSliders)
        styleSlider(slider);
    for (auto& slider : sidAdsrSliders)
        styleSlider(slider);

    for (auto* box : std::array<juce::ComboBox*, 13> {
             &presetBox, &chipModeBox, &accuracyBox, &macroBox, &playModeBox, &dmcRateBox,
             &dmcSampleSlotBox, &dmcPlaybackModeBox, &dmcMapRootBox, &dmgStereoRouteBox,
             &sidFilterModeBox, &sidFilterRoutingBox, &snNoiseModeBox })
    {
        styleCombo(*box);
    }

    for (auto& box : sidVoiceWaveBoxes)
        styleCombo(box);
    for (auto& box : hucVoiceWaveBoxes)
        styleCombo(box);
    for (auto& box : paulaVoiceSampleBoxes)
        styleCombo(box);
    for (auto& box : sidAdsrBoxes)
        styleCombo(box);
    for (auto& box : ymChannelMixBoxes)
        styleCombo(box);
    styleCombo(fmAlgorithmBox);
    styleCombo(oplWaveformBox);
    styleCombo(opllInstrumentBox);

    for (auto* button : std::array<juce::TextButton*, 5> {
             &userPresetLoadButton, &userPresetSaveButton, &userPresetSaveAsButton,
             &dmcSampleFileButton, &dmcSampleFolderButton })
    {
        styleButton(*button);
    }
    styleButton(dmcSampleBankButton);

    for (auto& button : sourceChannelButtons)
        styleButton(button);
    for (auto& button : pulseDutyButtons)
        styleButton(button);
    for (auto& button : pulse2DutyButtons)
        styleButton(button);
    for (auto& button : waveShapeButtons)
        styleButton(button);
    for (auto& button : dmgWaveLevelButtons)
        styleButton(button);
    for (auto& button : dmgStereoRouteButtons)
        styleButton(button);
    for (auto& button : ymEnvelopeShapeButtons)
        styleButton(button);
    for (auto& button : snNoiseModeButtons)
        styleButton(button);
    for (auto& button : toneNoiseMixButtons)
        styleButton(button);

    dmcLoopButton.setColour(juce::ToggleButton::textColourId, theme.text);
    spc700LoopModeButton.setColour(juce::ToggleButton::textColourId, theme.text);
    repaint();
}

void ChipperAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto theme = chipThemeFor(displayedMode);

    g.fillAll(theme.background);
    drawChipThemeTexture(g, getLocalBounds(), theme);
    drawChipIdentityAccent(g, getLocalBounds(), displayedMode, theme);
    g.setColour(theme.outline);
    g.drawHorizontalLine(84, 16.0f, static_cast<float>(getWidth() - 16));
    g.drawHorizontalLine(getHeight() - 62, 16.0f, static_cast<float>(getWidth() - 16));

    for (const auto& bounds : moduleBounds)
    {
        if (bounds.isEmpty())
            continue;

        const auto panel = bounds.toFloat();
        g.setColour(theme.panel);
        g.fillRoundedRectangle(panel, theme.panelRadius);
        g.setColour(theme.outline);
        g.drawRoundedRectangle(panel.reduced(0.5f), theme.panelRadius, 1.0f);
    }

    if (usesSourceChannelSurface(displayedMode))
    {
        for (const auto& bounds : sourceChannelBounds)
        {
            if (bounds.isEmpty())
                continue;

            const auto card = bounds.toFloat();
            g.setColour(theme.sourceCard);
            g.fillRoundedRectangle(card, std::max(2.0f, theme.panelRadius - 2.0f));
            g.setColour(theme.accent.withAlpha(0.70f));
            g.drawRoundedRectangle(card.reduced(0.5f), std::max(2.0f, theme.panelRadius - 2.0f), 1.0f);
        }
    }

    if (! globalStripBounds.isEmpty())
    {
        const auto strip = globalStripBounds.toFloat();
        g.setColour(theme.panelAlt);
        g.fillRoundedRectangle(strip, theme.panelRadius);
        g.setColour(theme.outline);
        g.drawRoundedRectangle(strip.reduced(0.5f), theme.panelRadius, 1.0f);
    }
}

void ChipperAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto top = area.removeFromTop(62);
    const auto placeHeaderCombo = [this](size_t index, juce::ComboBox& comboBox, juce::Rectangle<int> bounds)
    {
        headerControlLabels[index].setBounds(bounds.removeFromTop(16));
        comboBox.setBounds(bounds.reduced(0, 4));
    };

    titleLabel.setBounds(top.removeFromLeft(230));
    top.removeFromLeft(8);

    constexpr auto headerGap = 8;
    constexpr auto compactGap = 4;
    constexpr auto loadButtonWidth = 46;
    constexpr auto saveButtonWidth = 60;
    constexpr auto saveAsButtonWidth = 76;
    constexpr auto chipModeWidth = 190;
    constexpr auto accuracyWidth = 112;
    constexpr auto playModeWidth = 128;
    constexpr auto presetMinWidth = 148;
    constexpr auto presetMaxWidth = 330;

    const auto fixedHeaderWidth = compactGap + loadButtonWidth
        + compactGap + saveButtonWidth
        + compactGap + saveAsButtonWidth
        + headerGap + chipModeWidth
        + headerGap + accuracyWidth
        + headerGap + playModeWidth;
    const auto presetWidth = std::clamp(top.getWidth() - fixedHeaderWidth, presetMinWidth, presetMaxWidth);

    placeHeaderCombo(0, presetBox, top.removeFromLeft(presetWidth));
    top.removeFromLeft(4);
    userPresetLoadButton.setBounds(top.removeFromLeft(loadButtonWidth).withTrimmedTop(20).reduced(0, 4));
    top.removeFromLeft(4);
    userPresetSaveButton.setBounds(top.removeFromLeft(saveButtonWidth).withTrimmedTop(20).reduced(0, 4));
    top.removeFromLeft(4);
    userPresetSaveAsButton.setBounds(top.removeFromLeft(saveAsButtonWidth).withTrimmedTop(20).reduced(0, 4));
    top.removeFromLeft(8);
    placeHeaderCombo(1, chipModeBox, top.removeFromLeft(chipModeWidth));
    top.removeFromLeft(8);
    placeHeaderCombo(2, accuracyBox, top.removeFromLeft(accuracyWidth));
    top.removeFromLeft(8);
    placeHeaderCombo(4, playModeBox, top.removeFromLeft(playModeWidth));
    area.removeFromTop(10);
    chipSummaryLabel.setBounds(area.removeFromTop(34));
    area.removeFromTop(10);

    constexpr auto footerReserve = 52;
    const auto nesLayout = displayedMode == chipper::ChipMode::nes;
    const auto sidLayout = displayedMode == chipper::ChipMode::sid;
    const auto dmgLayout = displayedMode == chipper::ChipMode::dmg;
    const auto spc700Layout = displayedMode == chipper::ChipMode::spc700;
    const auto paulaLayout = displayedMode == chipper::ChipMode::paula;
    const auto sampleLayout = spc700Layout || paulaLayout;
    const auto wavetableLayout = displayedMode == chipper::ChipMode::huc6280
        || displayedMode == chipper::ChipMode::namcoWsg
        || displayedMode == chipper::ChipMode::scc;
    const auto showMotionModule = sidLayout;
    const auto performanceStripHeight = sidLayout ? 260 : (nesLayout ? 318 : (sampleLayout ? 220 : (wavetableLayout ? 220 : 300)));
    const auto maxModulesHeight = sidLayout ? 620 : (nesLayout ? 650 : (spc700Layout ? 1220 : (paulaLayout ? 1240 : (wavetableLayout ? 640 : 492))));
    const auto availableModulesHeight = std::max(0, area.getHeight() - footerReserve - 12 - performanceStripHeight);
    const auto modulesHeight = std::clamp(availableModulesHeight, std::min(410, availableModulesHeight), std::min(maxModulesHeight, availableModulesHeight));
    auto modules = area.removeFromTop(modulesHeight);
    const auto gap = 10;
    const auto columnWidth = (modules.getWidth() - gap) / 2;
    const auto rowHeight = (modules.getHeight() - (gap * 2)) / 3;

    if (sidLayout)
    {
        const auto topRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.27)), 126, 166);
        const auto middleRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.26)), 118, 156);
        const auto envelopeHeight = std::max(160, modules.getHeight() - topRowHeight - middleRowHeight - (gap * 2));
        const auto leftX = modules.getX();
        const auto rightX = modules.getX() + columnWidth + gap;
        const auto topY = modules.getY();
        const auto middleY = topY + topRowHeight + gap;
        const auto envelopeY = middleY + middleRowHeight + gap;

        moduleBounds[0] = { leftX, topY, columnWidth, topRowHeight };
        moduleBounds[1] = { rightX, topY, columnWidth, topRowHeight };
        moduleBounds[2] = { leftX, middleY, columnWidth, middleRowHeight };
        moduleBounds[3] = { modules.getX(), envelopeY, modules.getWidth(), envelopeHeight };
        moduleBounds[4] = { rightX, middleY, columnWidth, middleRowHeight };
        moduleBounds[5] = {};
    }
    else if (nesLayout)
    {
        const auto topRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.36)), 220, 258);
        const auto sampleRowHeight = std::max(260, modules.getHeight() - topRowHeight - gap);
        const auto topY = modules.getY();
        const auto bottomY = topY + topRowHeight + gap;

        moduleBounds[0] = {};
        moduleBounds[1] = { modules.getX(), topY, modules.getWidth(), topRowHeight };
        moduleBounds[2] = {};
        moduleBounds[3] = {};
        moduleBounds[4] = {};
        moduleBounds[5] = { modules.getX(), bottomY, modules.getWidth(), sampleRowHeight };
    }
    else if (spc700Layout)
    {
        const auto topRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.20)), 196, 226);
        const auto minimumSampleRowHeight = 430;
        const auto maximumSourceRowHeight = std::max(0, modules.getHeight() - topRowHeight - minimumSampleRowHeight - (gap * 2));
        const auto desiredSourceHeight = static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.45));
        const auto sourceRowHeight = std::clamp(desiredSourceHeight, std::min(440, maximumSourceRowHeight), std::min(540, maximumSourceRowHeight));
        const auto sampleRowHeight = std::max(0, modules.getHeight() - topRowHeight - sourceRowHeight - (gap * 2));
        const auto topColumnWidth = (modules.getWidth() - gap) / 2;
        const auto topY = modules.getY();
        const auto sourceY = topY + topRowHeight + gap;
        const auto bottomY = sourceY + sourceRowHeight + gap;

        moduleBounds[0] = {};
        moduleBounds[1] = { modules.getX(), sourceY, modules.getWidth(), sourceRowHeight };
        moduleBounds[2] = { modules.getX(), topY, topColumnWidth, topRowHeight };
        moduleBounds[3] = { modules.getX() + topColumnWidth + gap, topY, topColumnWidth, topRowHeight };
        moduleBounds[4] = {};
        moduleBounds[5] = { modules.getX(), bottomY, modules.getWidth(), sampleRowHeight };
    }
    else if (dmgLayout)
    {
        const auto topRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.50)), 232, 270);
        const auto bottomRowHeight = std::max(170, modules.getHeight() - topRowHeight - gap);
        const auto topY = modules.getY();
        const auto bottomY = topY + topRowHeight + gap;

        moduleBounds[0] = {};
        moduleBounds[1] = { modules.getX(), topY, modules.getWidth(), topRowHeight };
        moduleBounds[2] = {};
        moduleBounds[3] = { modules.getX(), bottomY, columnWidth, bottomRowHeight };
        moduleBounds[4] = {};
        moduleBounds[5] = { modules.getX() + columnWidth + gap, bottomY, columnWidth, bottomRowHeight };
    }
    else if (wavetableLayout)
    {
        const auto availableHeight = modules.getHeight();
        const auto reservedGap = gap * 2;
        const auto minimumEnvelopeHeight = 112;
        const auto minimumOutputHeight = displayedMode == chipper::ChipMode::huc6280 ? 104 : 90;
        const auto sourceMaximumHeight = std::min(390, std::max(160, availableHeight - minimumEnvelopeHeight - minimumOutputHeight - reservedGap));
        const auto sourceMinimumHeight = std::min(320, sourceMaximumHeight);
        const auto desiredSourceHeight = static_cast<int>(std::round(static_cast<double>(availableHeight) * 0.56));
        const auto sourceRowHeight = std::clamp(desiredSourceHeight, sourceMinimumHeight, sourceMaximumHeight);
        const auto remainingHeight = std::max(0, availableHeight - sourceRowHeight - reservedGap);
        const auto envelopeMinimumHeight = std::min(minimumEnvelopeHeight, remainingHeight);
        const auto envelopeMaximumHeight = std::min(148, remainingHeight);
        const auto desiredEnvelopeHeight = std::max(remainingHeight / 2, minimumEnvelopeHeight);
        const auto envelopeRowHeight = std::clamp(desiredEnvelopeHeight, envelopeMinimumHeight, envelopeMaximumHeight);
        const auto outputRowHeight = std::max(0, remainingHeight - envelopeRowHeight);
        const auto topY = modules.getY();
        const auto envelopeY = topY + sourceRowHeight + gap;
        const auto outputY = envelopeY + envelopeRowHeight + gap;

        moduleBounds[0] = {};
        moduleBounds[1] = { modules.getX(), topY, modules.getWidth(), sourceRowHeight };
        moduleBounds[2] = {};
        moduleBounds[3] = { modules.getX(), envelopeY, modules.getWidth(), envelopeRowHeight };
        moduleBounds[4] = {};
        moduleBounds[5] = { modules.getX(), outputY, modules.getWidth(), outputRowHeight };
    }
    else if (paulaLayout)
    {
        const auto middleRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.07)), 70, 90);
        const auto sampleMinimumHeight = std::min(500, std::max(410, modules.getHeight() / 3));
        const auto sourceMaximumHeight = std::max(0, modules.getHeight() - middleRowHeight - sampleMinimumHeight - (gap * 2));
        const auto sourceMinimumHeight = std::min(420, sourceMaximumHeight);
        const auto desiredSourceHeight = static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.40));
        const auto sourceRowHeight = std::clamp(desiredSourceHeight, sourceMinimumHeight, sourceMaximumHeight);
        const auto sampleRowHeight = std::max(0, modules.getHeight() - middleRowHeight - sourceRowHeight - (gap * 2));
        const auto topY = modules.getY();
        const auto middleY = topY + sourceRowHeight + gap;
        const auto bottomY = middleY + middleRowHeight + gap;

        moduleBounds[0] = {};
        moduleBounds[1] = { modules.getX(), topY, modules.getWidth(), sourceRowHeight };
        moduleBounds[2] = {};
        moduleBounds[3] = { modules.getX(), middleY, modules.getWidth(), middleRowHeight };
        moduleBounds[4] = {};
        moduleBounds[5] = { modules.getX(), bottomY, modules.getWidth(), sampleRowHeight };
    }
    else if (sampleLayout)
    {
        const auto topRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.28)), 136, 174);
        const auto middleRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.30)), 148, 188);
        const auto bottomRowHeight = std::max(210, modules.getHeight() - topRowHeight - middleRowHeight - (gap * 2));
        const auto topY = modules.getY();
        const auto middleY = topY + topRowHeight + gap;
        const auto bottomY = middleY + middleRowHeight + gap;

        moduleBounds[0] = {};
        moduleBounds[1] = { modules.getX(), topY, modules.getWidth(), topRowHeight };
        moduleBounds[2] = { modules.getX(), middleY, columnWidth, middleRowHeight };
        moduleBounds[3] = { modules.getX() + columnWidth + gap, middleY, columnWidth, middleRowHeight };
        moduleBounds[4] = {};
        moduleBounds[5] = { modules.getX(), bottomY, modules.getWidth(), bottomRowHeight };
    }
    else
    {
        moduleBounds[0] = {};
        moduleBounds[1] = { modules.getX(), modules.getY(), modules.getWidth(), rowHeight };
        moduleBounds[2] = { modules.getX(), modules.getY() + rowHeight + gap, columnWidth, rowHeight };
        moduleBounds[3] = { modules.getX() + columnWidth + gap, modules.getY() + rowHeight + gap, columnWidth, rowHeight };
        moduleBounds[4] = {};
        moduleBounds[5] = {
            modules.getX(),
            modules.getY() + (2 * (rowHeight + gap)),
            modules.getWidth(),
            rowHeight
        };
    }

    for (size_t i = 0; i < moduleBounds.size(); ++i)
    {
        if (moduleBounds[i].isEmpty())
        {
            moduleTitleLabels[i].setBounds({});
            moduleSummaryLabels[i].setBounds({});
            for (auto& itemLabel : moduleItemLabels[i])
                itemLabel.setBounds({});
            continue;
        }

        auto panel = moduleBounds[i].reduced(12, 9);
        auto titleRow = panel.removeFromTop(22);
        moduleTitleLabels[i].setBounds(titleRow);
        moduleSummaryLabels[i].setBounds(panel.removeFromTop(28));
        panel.removeFromTop(4);

        const auto itemGap = 6;
        const auto itemWidth = (panel.getWidth() - itemGap) / 2;
        const auto itemHeight = 20;
        for (size_t item = 0; item < moduleItemLabels[i].size(); ++item)
        {
            const auto itemRow = static_cast<int>(item / 2);
            const auto itemColumn = static_cast<int>(item % 2);
            moduleItemLabels[i][item].setBounds({
                panel.getX() + (itemColumn * (itemWidth + itemGap)),
                panel.getY() + (itemRow * (itemHeight + itemGap)),
                itemWidth,
                itemHeight
            });
        }
    }

    if (dmgLayout)
    {
        nativeGroupLabels[0].setBounds({});
        nativeLabels[0].setBounds({});
        controlValueLabels[0].setBounds({});
    }

    auto sourcePanel = moduleBounds[1].reduced(12, 9);
    sourcePanel.removeFromTop(20);
    const auto sourceSurfaceActive = chipper::descriptorFor(displayedMode).implemented && usesSourceChannelSurface(displayedMode);
    if (! sourceSurfaceActive || moduleSummaryLabels[1].isVisible())
    {
        sourcePanel.removeFromTop(30);
        sourcePanel.removeFromTop(4);
    }
    else
        sourcePanel.removeFromTop(4);

    const auto sourceGap = 6;
    const auto visibleSourceCards = chipper::visibleSourceCountForMode(displayedMode);
    const auto useSpc700VoiceGrid = displayedMode == chipper::ChipMode::spc700 && visibleSourceCards > 4u;
    const auto usePaulaVoiceGrid = displayedMode == chipper::ChipMode::paula && visibleSourceCards > 2u;
    const auto useWavetableVoiceGrid = (displayedMode == chipper::ChipMode::huc6280
        || displayedMode == chipper::ChipMode::namcoWsg
        || displayedMode == chipper::ChipMode::scc) && visibleSourceCards > 4u;
    constexpr auto wavetableColumns = 4;
    const auto paulaColumns = 2;
    const auto sourceColumns = useSpc700VoiceGrid ? 4 : (usePaulaVoiceGrid ? paulaColumns : (useWavetableVoiceGrid ? wavetableColumns : static_cast<int>(visibleSourceCards)));
    const auto sourceRows = (useSpc700VoiceGrid || usePaulaVoiceGrid || useWavetableVoiceGrid)
        ? static_cast<int>((visibleSourceCards + static_cast<size_t>(sourceColumns) - 1u) / static_cast<size_t>(sourceColumns))
        : 1;
    const auto sourceCardWidth = sourceColumns > 0
        ? (sourcePanel.getWidth() - (sourceGap * (sourceColumns - 1))) / sourceColumns
        : sourcePanel.getWidth();
    const auto sourceCardHeight = sourceRows > 0
        ? (sourcePanel.getHeight() - (sourceGap * (sourceRows - 1))) / sourceRows
        : sourcePanel.getHeight();
    for (size_t i = 0; i < sourceChannelBounds.size(); ++i)
    {
        if (i >= visibleSourceCards)
        {
            sourceChannelBounds[i] = {};
            sourceChannelButtons[i].setBounds({});
            sourcePreviewScopes[i].setBounds({});
            sourceLevelSliders[i].setBounds({});
            sourceLevelLabels[i].setBounds({});
            sourceLevelValueLabels[i].setBounds({});
            if (i < hucVoiceWaveBoxes.size())
            {
                hucVoiceWaveLabels[i].setBounds({});
                hucVoiceWaveBoxes[i].setBounds({});
            }
            if (i < paulaVoiceSampleBoxes.size())
            {
                paulaVoiceSampleLabels[i].setBounds({});
                paulaVoiceSampleBoxes[i].setBounds({});
            }
            if (displayedMode == chipper::ChipMode::sid && i < sidVoiceWaveCount)
            {
                sidVoiceWaveLabels[i].setBounds({});
                sidVoiceWaveBoxes[i].setBounds({});
                sidVoicePulseWidthLabels[i].setBounds({});
                sidVoicePulseWidthSliders[i].setBounds({});
                sidVoicePulseWidthValueLabels[i].setBounds({});
            }
            continue;
        }

        const auto sourceColumn = (useSpc700VoiceGrid || usePaulaVoiceGrid || useWavetableVoiceGrid) ? static_cast<int>(i % static_cast<size_t>(sourceColumns)) : static_cast<int>(i);
        const auto sourceRow = (useSpc700VoiceGrid || usePaulaVoiceGrid || useWavetableVoiceGrid) ? static_cast<int>(i / static_cast<size_t>(sourceColumns)) : 0;
        sourceChannelBounds[i] = {
            sourcePanel.getX() + (sourceColumn * (sourceCardWidth + sourceGap)),
            sourcePanel.getY() + (sourceRow * (sourceCardHeight + sourceGap)),
            sourceCardWidth,
            sourceCardHeight
        };
        const auto isSidSourceCard = displayedMode == chipper::ChipMode::sid;
        const auto isNesSourceCard = displayedMode == chipper::ChipMode::nes;
        const auto isDmgSourceCard = displayedMode == chipper::ChipMode::dmg;
        const auto isPaulaSourceCard = displayedMode == chipper::ChipMode::paula;
        const auto isSpc700SourceCard = displayedMode == chipper::ChipMode::spc700;
        const auto isWavetableSourceCard = useWavetableVoiceGrid;
        const auto isDenseSampleCard = isWavetableSourceCard || isPaulaSourceCard || isSpc700SourceCard;
        auto sourceCard = sourceChannelBounds[i].reduced(useSpc700VoiceGrid ? 5 : (isDenseSampleCard ? 5 : 8),
                                                         isSidSourceCard ? 2 : (isDenseSampleCard ? 3 : 4));
        constexpr auto standardInlineControlHeight = 28;
        constexpr auto embeddedLabelHeight = 14;
        constexpr auto embeddedControlRowHeight = embeddedLabelHeight + standardInlineControlHeight;
        const auto buttonHeight = useSpc700VoiceGrid ? 20 : (isPaulaSourceCard ? 20 : (isSidSourceCard ? 17 : (isDenseSampleCard ? 20 : 18)));
        sourceChannelButtons[i].setBounds(sourceCard.removeFromTop(std::min(buttonHeight, sourceCard.getHeight())));
        sourceCard.removeFromTop(isDenseSampleCard ? 3 : 2);
        const auto previewHeight = isWavetableSourceCard
            ? std::clamp(sourceCard.getHeight() / 6, 18, 26)
            : std::clamp(sourceCard.getHeight() / (useSpc700VoiceGrid ? 3 : 4),
                         useSpc700VoiceGrid ? 18 : (isSidSourceCard ? 22 : ((isNesSourceCard || isDmgSourceCard || isPaulaSourceCard) ? 24 : 20)),
                         useSpc700VoiceGrid ? 28 : (isSidSourceCard ? 32 : (isPaulaSourceCard ? 30 : ((isNesSourceCard || isDmgSourceCard) ? 34 : 28))));
        sourcePreviewScopes[i].setBounds(sourceCard.removeFromTop(std::min(previewHeight, sourceCard.getHeight())));
        sourceCard.removeFromTop(isNesSourceCard || isDmgSourceCard || isDenseSampleCard ? 3 : 1);

        const auto placeEmbeddedLevelInArea = [this, i](juce::Rectangle<int> levelArea, int labelWidth = 52)
        {
            auto levelRow = levelArea.removeFromTop(std::min(14, levelArea.getHeight()));
            sourceLevelLabels[i].setBounds(levelRow.removeFromLeft(std::min(labelWidth, levelRow.getWidth())));
            sourceLevelValueLabels[i].setBounds(levelRow);
            levelArea.removeFromTop(std::min(5, levelArea.getHeight()));
            const auto sliderHeight = std::clamp(levelArea.getHeight(), 18, 26);
            sourceLevelSliders[i].setBounds(levelArea.removeFromTop(std::min(sliderHeight, levelArea.getHeight())).reduced(0, 2));
        };

        if (isSidSourceCard && i < sidVoiceWaveCount)
        {
            auto waveRow = sourceCard.removeFromTop(std::min(20, sourceCard.getHeight()));
            sidVoiceWaveLabels[i].setBounds(waveRow.removeFromLeft(38));
            sidVoiceWaveBoxes[i].setBounds(waveRow);
            sourceCard.removeFromTop(1);

            auto pulseWidthRow = sourceCard.removeFromTop(std::min(16, sourceCard.getHeight()));
            sidVoicePulseWidthLabels[i].setBounds(pulseWidthRow.removeFromLeft(22));
            sidVoicePulseWidthValueLabels[i].setBounds(pulseWidthRow.removeFromRight(42));
            sidVoicePulseWidthSliders[i].setBounds(pulseWidthRow.reduced(0, 2));
            sourceCard.removeFromTop(1);
        }

        if (isNesSourceCard || isDmgSourceCard)
        {
            constexpr auto compactSegmentHeight = 24;
            constexpr auto compactLabelHeight = 18;
            const auto placeCompactSegment = [](auto& buttons, juce::Rectangle<int> segmentBounds, size_t count)
            {
                layoutSegmentedButtons(buttons, segmentBounds.reduced(0, 1), count);
            };
            const auto placeCompactLabel = [](juce::Label& label, juce::Label* valueLabel, juce::Rectangle<int> labelBounds)
            {
                auto labelTextBounds = labelBounds;
                if (valueLabel != nullptr && labelBounds.getWidth() > 160)
                {
                    valueLabel->setJustificationType(juce::Justification::centredRight);
                    valueLabel->setMinimumHorizontalScale(0.65f);
                    valueLabel->setBounds(labelTextBounds.removeFromRight(std::min(138, labelTextBounds.getWidth() / 2)));
                    labelTextBounds.removeFromRight(std::min(8, labelTextBounds.getWidth()));
                }
                else if (valueLabel != nullptr)
                {
                    valueLabel->setBounds({});
                }

                label.setJustificationType(juce::Justification::centredLeft);
                label.setMinimumHorizontalScale(0.7f);
                label.setBounds(labelTextBounds);
            };

            if (i == 0)
            {
                placeCompactLabel(nativeLabels[0], &controlValueLabels[0], sourceCard.removeFromTop(std::min(compactLabelHeight, sourceCard.getHeight())));
                pulseDutySegmentBounds = sourceCard.removeFromTop(std::min(compactSegmentHeight, sourceCard.getHeight()));
                placeCompactSegment(pulseDutyButtons, pulseDutySegmentBounds, pulseDutyButtons.size());
                sourceCard.removeFromTop(3);
            }
            else if (i == 1)
            {
                placeCompactLabel(pulse2DutyLabel, &pulse2DutyValueLabel, sourceCard.removeFromTop(std::min(compactLabelHeight, sourceCard.getHeight())));
                pulse2DutySegmentBounds = sourceCard.removeFromTop(std::min(compactSegmentHeight, sourceCard.getHeight()));
                placeCompactSegment(pulse2DutyButtons, pulse2DutySegmentBounds, pulse2DutyButtons.size());
                sourceCard.removeFromTop(3);
            }
            else if (isDmgSourceCard && i == 2)
            {
                placeCompactLabel(waveShapeLabel, &waveShapeValueLabel, sourceCard.removeFromTop(std::min(compactLabelHeight, sourceCard.getHeight())));
                waveShapeSegmentBounds = sourceCard.removeFromTop(std::min(compactSegmentHeight, sourceCard.getHeight()));
                placeCompactSegment(waveShapeButtons, waveShapeSegmentBounds, waveShapeButtons.size());
                sourceCard.removeFromTop(3);
                placeCompactLabel(dmgWaveLevelLabel, &dmgWaveLevelValueLabel, sourceCard.removeFromTop(std::min(compactLabelHeight, sourceCard.getHeight())));
                dmgWaveLevelSegmentBounds = sourceCard.removeFromTop(std::min(compactSegmentHeight, sourceCard.getHeight()));
                placeCompactSegment(dmgWaveLevelButtons, dmgWaveLevelSegmentBounds, dmgWaveLevelButtons.size());
                sourceCard.removeFromTop(3);
            }
            else if (i == 3)
            {
                placeCompactLabel(snNoiseModeLabel, &snNoiseModeValueLabel, sourceCard.removeFromTop(std::min(compactLabelHeight, sourceCard.getHeight())));
                snNoiseModeSegmentBounds = sourceCard.removeFromTop(std::min(compactSegmentHeight, sourceCard.getHeight()));
                placeCompactSegment(snNoiseModeButtons, snNoiseModeSegmentBounds, snNoiseModeButtons.size());
                sourceCard.removeFromTop(3);
            }
        }
        else if (isWavetableSourceCard && i < hucVoiceWaveBoxes.size())
        {
            auto waveRow = sourceCard.removeFromTop(std::min(embeddedControlRowHeight, sourceCard.getHeight()));
            hucVoiceWaveLabels[i].setBounds(waveRow.removeFromTop(std::min(embeddedLabelHeight, waveRow.getHeight())));
            hucVoiceWaveBoxes[i].setBounds(waveRow.removeFromTop(std::min(standardInlineControlHeight, waveRow.getHeight())).reduced(0, 1));
            sourceCard.removeFromTop(std::min(4, sourceCard.getHeight()));

            auto levelArea = sourceCard.removeFromTop(std::min(54, sourceCard.getHeight()));
            placeEmbeddedLevelInArea(levelArea);
        }
        else if (isPaulaSourceCard && i < hucVoiceWaveBoxes.size())
        {
            if (i < paulaVoiceSampleBoxes.size())
            {
                const auto placeLabeledCombo = [&](juce::Label& label, juce::ComboBox& box, juce::Rectangle<int> area)
                {
                    label.setBounds(area.removeFromTop(std::min(embeddedLabelHeight, area.getHeight())));
                    box.setBounds(area.removeFromTop(std::min(standardInlineControlHeight, area.getHeight())).reduced(0, 1));
                };

                auto selectorRow = sourceCard.removeFromTop(std::min(embeddedControlRowHeight, sourceCard.getHeight()));
                if (selectorRow.getWidth() >= 320)
                {
                    constexpr auto selectorGap = 8;
                    const auto halfWidth = (selectorRow.getWidth() - selectorGap) / 2;
                    auto shapeArea = selectorRow.removeFromLeft(halfWidth);
                    selectorRow.removeFromLeft(std::min(selectorGap, selectorRow.getWidth()));
                    placeLabeledCombo(hucVoiceWaveLabels[i], hucVoiceWaveBoxes[i], shapeArea);
                    placeLabeledCombo(paulaVoiceSampleLabels[i], paulaVoiceSampleBoxes[i], selectorRow);
                }
                else
                {
                    placeLabeledCombo(hucVoiceWaveLabels[i], hucVoiceWaveBoxes[i], selectorRow);
                    auto sampleRow = sourceCard.removeFromTop(std::min(embeddedControlRowHeight, sourceCard.getHeight()));
                    placeLabeledCombo(paulaVoiceSampleLabels[i], paulaVoiceSampleBoxes[i], sampleRow);
                }
                sourceCard.removeFromTop(std::min(3, sourceCard.getHeight()));

                auto levelArea = sourceCard.removeFromTop(std::min(54, sourceCard.getHeight()));
                placeEmbeddedLevelInArea(levelArea);
            }
        }
        else if (isSpc700SourceCard && i < hucVoiceWaveBoxes.size())
        {
            const auto sampleRowHeight = std::min(embeddedControlRowHeight, sourceCard.getHeight());
            auto sampleRow = sourceCard.removeFromTop(sampleRowHeight);
            hucVoiceWaveLabels[i].setBounds(sampleRow.removeFromTop(std::min(embeddedLabelHeight, sampleRow.getHeight())));
            hucVoiceWaveBoxes[i].setBounds(sampleRow.removeFromTop(std::min(standardInlineControlHeight, sampleRow.getHeight())).reduced(0, 1));
            sourceCard.removeFromTop(std::min(4, sourceCard.getHeight()));

            auto levelArea = sourceCard.removeFromTop(std::min(54, sourceCard.getHeight()));
            placeEmbeddedLevelInArea(levelArea, 42);
        }

        if (! isWavetableSourceCard && ! isPaulaSourceCard && ! isSpc700SourceCard)
        {
            auto levelRow = sourceCard.removeFromTop(std::min(useSpc700VoiceGrid ? 12 : (isDenseSampleCard ? 12 : 12), sourceCard.getHeight()));
            sourceLevelLabels[i].setBounds(levelRow.removeFromLeft(std::min(useSpc700VoiceGrid ? 36 : (isDenseSampleCard ? 38 : 48), levelRow.getWidth())));
            sourceLevelValueLabels[i].setBounds(levelRow);
            sourceCard.removeFromTop(1);
            const auto sliderHeight = useSpc700VoiceGrid ? 12 : (isSidSourceCard ? 11 : (isDenseSampleCard ? 14 : 14));
            sourceLevelSliders[i].setBounds(sourceCard.removeFromTop(std::min(sliderHeight, sourceCard.getHeight())).reduced(0, 1));
        }
    }

    auto tonePanel = moduleBounds[2].reduced(12, 9);
    tonePanel.removeFromTop(20);
    if (moduleSummaryLabels[2].isVisible())
        tonePanel.removeFromTop(30);
    tonePanel.removeFromTop(moduleSummaryLabels[2].isVisible() ? 4 : 8);
    auto primaryTonePanel = tonePanel;
    auto secondaryTonePanel = tonePanel;
    auto tertiaryTonePanel = tonePanel;
    const auto usesFmToneStack = displayedMode == chipper::ChipMode::ym2612
        || displayedMode == chipper::ChipMode::opl3
        || displayedMode == chipper::ChipMode::ym2151;
    const auto usesFmEnvelopeShapePanel = displayedMode == chipper::ChipMode::ym2612
        || displayedMode == chipper::ChipMode::ym2151;
    const auto usesFmOperatorRegisterSurface = displayedMode == chipper::ChipMode::ym2612
        || displayedMode == chipper::ChipMode::opl3
        || displayedMode == chipper::ChipMode::ym2151;
    const auto usesSampleToneStack = (displayedMode == chipper::ChipMode::spc700
        || displayedMode == chipper::ChipMode::paula)
        && usesSnNoiseModeSegment(displayedMode);
    if (displayedMode == chipper::ChipMode::dmg)
    {
        const auto availableHeight = tonePanel.getHeight();
        const auto rowGap = std::min(7, std::max(4, availableHeight / 18));
        const auto dmgToneRowHeight = std::max(44, (availableHeight - (rowGap * 2)) / 3);
        primaryTonePanel = tonePanel.removeFromTop(std::min(dmgToneRowHeight, tonePanel.getHeight()));
        tonePanel.removeFromTop(std::min(rowGap, tonePanel.getHeight()));
        secondaryTonePanel = tonePanel.removeFromTop(std::min(dmgToneRowHeight, tonePanel.getHeight()));
        tonePanel.removeFromTop(std::min(rowGap, tonePanel.getHeight()));
        tertiaryTonePanel = tonePanel;
    }
    else if (usesFmToneStack)
    {
        const auto isFourOperatorFm = displayedMode == chipper::ChipMode::ym2612
            || displayedMode == chipper::ChipMode::ym2151;
        if (isFourOperatorFm)
        {
            const auto availableHeight = tonePanel.getHeight();
            const auto routeHeight = std::clamp(availableHeight / 3, 38, 50);
            const auto gapHeight = std::min(6, std::max(0, availableHeight - routeHeight));
            const auto primaryHeight = std::max(54, availableHeight - routeHeight - gapHeight);
            primaryTonePanel = tonePanel.removeFromTop(std::min(primaryHeight, tonePanel.getHeight()));
            tonePanel.removeFromTop(std::min(gapHeight, tonePanel.getHeight()));
            secondaryTonePanel = {};
            tertiaryTonePanel = tonePanel;
        }
        else
        {
            const auto availableHeight = tonePanel.getHeight();
            const auto gapHeight = std::min(8, std::max(0, availableHeight - 82));
            const auto primaryHeight = std::clamp(availableHeight / 2, 52, 64);
            primaryTonePanel = tonePanel.removeFromTop(std::min(primaryHeight, tonePanel.getHeight()));
            tonePanel.removeFromTop(std::min(gapHeight, tonePanel.getHeight()));
            secondaryTonePanel = tonePanel;
            tertiaryTonePanel = {};
        }
    }
    else if (usesSampleToneStack)
    {
        primaryTonePanel = tonePanel.removeFromTop(std::min(58, tonePanel.getHeight()));
        tonePanel.removeFromTop(std::min(6, tonePanel.getHeight()));
        secondaryTonePanel = tonePanel;
    }
    else if (displayedMode == chipper::ChipMode::nes
        || displayedMode == chipper::ChipMode::dmg
        || displayedMode == chipper::ChipMode::ym2149
        || displayedMode == chipper::ChipMode::pokey)
    {
        if (displayedMode == chipper::ChipMode::nes)
        {
            const auto availableHeight = tonePanel.getHeight();
            const auto gapHeight = std::min(8, std::max(0, availableHeight - 1));
            const auto desiredPulseHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(availableHeight) * 0.52)), 92, 112);
            const auto minNoiseHeight = 76;
            const auto pulseHeight = std::clamp(desiredPulseHeight,
                                                0,
                                                std::max(0, availableHeight - gapHeight - minNoiseHeight));
            primaryTonePanel = tonePanel.removeFromTop(std::min(pulseHeight, tonePanel.getHeight()));
            tonePanel.removeFromTop(std::min(gapHeight, tonePanel.getHeight()));
            secondaryTonePanel = tonePanel;
        }
        else if (displayedMode == chipper::ChipMode::dmg)
        {
            const auto availableHeight = tonePanel.getHeight();
            const auto gapHeight = std::min(5, std::max(2, availableHeight / 28));
            const auto dmgToneRowHeight = std::max(0, (availableHeight - (gapHeight * 2)) / 3);
            primaryTonePanel = tonePanel.removeFromTop(std::min(dmgToneRowHeight, tonePanel.getHeight()));
            tonePanel.removeFromTop(std::min(gapHeight, tonePanel.getHeight()));
            secondaryTonePanel = tonePanel.removeFromTop(std::min(dmgToneRowHeight, tonePanel.getHeight()));
            tonePanel.removeFromTop(std::min(gapHeight, tonePanel.getHeight()));
            tertiaryTonePanel = tonePanel;
        }
        else
        {
            primaryTonePanel = tonePanel.removeFromTop(std::min(58, tonePanel.getHeight()));
            tonePanel.removeFromTop(std::min(6, tonePanel.getHeight()));
            secondaryTonePanel = tonePanel;
        }
    }

    if (displayedMode == chipper::ChipMode::sid)
    {
        const auto placeFilterSlider = [](juce::Slider& slider,
                                          juce::Label& label,
                                          juce::Label& valueLabel,
                                          juce::Rectangle<int> bounds)
        {
            auto header = bounds.removeFromTop(std::min(18, bounds.getHeight()));
            label.setBounds(header.removeFromLeft(labelReservation(header, 118)));
            valueLabel.setJustificationType(juce::Justification::centredRight);
            valueLabel.setBounds(header);
            bounds.removeFromTop(2);
            slider.setBounds(bounds.removeFromTop(std::min(22, bounds.getHeight())).reduced(0, 1));
        };

        const auto columnGap = 12;
        const auto rowGap = 8;
        auto topRow = tonePanel.removeFromTop(std::min(42, tonePanel.getHeight()));
        tonePanel.removeFromTop(rowGap);
        auto bottomRow = tonePanel;

        auto cutoffPanel = topRow.removeFromLeft((topRow.getWidth() - columnGap) / 2);
        topRow.removeFromLeft(columnGap);
        placeFilterSlider(nativeSliders[2], nativeLabels[2], controlValueLabels[2], cutoffPanel);
        placeFilterSlider(stereoSpreadSlider, stereoSpreadLabel, stereoSpreadValueLabel, topRow);

        auto modePanel = bottomRow.removeFromLeft((bottomRow.getWidth() - columnGap) / 2);
        bottomRow.removeFromLeft(columnGap);
        placeYmEnvelopeShapeSegment(modePanel);
        placeSidFilterRoutingControl(bottomRow);
    }
    else if (displayedMode == chipper::ChipMode::ym2149)
        placeYmChannelMixControls(primaryTonePanel);
    else if (displayedMode == chipper::ChipMode::ym2612 || displayedMode == chipper::ChipMode::ym2151)
        placeFmAlgorithmControl(primaryTonePanel);
    else if (displayedMode == chipper::ChipMode::opl3)
        placeOplWaveformControl(primaryTonePanel);
    else if (displayedMode == chipper::ChipMode::ym2413)
        placeOpllInstrumentControl(primaryTonePanel);
    else if (displayedMode == chipper::ChipMode::nes)
    {
        // NES register controls are owned by the individual source cards.
    }
    else if (displayedMode == chipper::ChipMode::dmg)
    {
        waveShapeLabel.setBounds({});
        waveShapeValueLabel.setBounds({});
        pulse2DutyLabel.setBounds({});
        pulse2DutyValueLabel.setBounds({});
        dmgWaveLevelLabel.setBounds({});
        dmgWaveLevelValueLabel.setBounds({});
    }
    else if (displayedMode == chipper::ChipMode::huc6280 || displayedMode == chipper::ChipMode::namcoWsg || displayedMode == chipper::ChipMode::scc || displayedMode == chipper::ChipMode::paula)
    {
        waveShapeLabel.setBounds({});
        waveShapeValueLabel.setBounds({});
        for (auto& button : waveShapeButtons)
            button.setBounds({});
    }
    else
        placeWaveShapeSegment(primaryTonePanel);
    if (displayedMode != chipper::ChipMode::sid && displayedMode != chipper::ChipMode::dmg)
    {
        if (usesDmgWaveLevelSegment(displayedMode))
            placeDmgWaveLevelSegment(displayedMode == chipper::ChipMode::dmg ? tertiaryTonePanel : secondaryTonePanel);
        if (displayedMode != chipper::ChipMode::spc700
            && displayedMode != chipper::ChipMode::ym2413
            && ! usesFmEnvelopeShapePanel
            && usesYmEnvelopeShapeSegment(displayedMode))
            placeYmEnvelopeShapeSegment(displayedMode == chipper::ChipMode::ym2149
                                            || displayedMode == chipper::ChipMode::pokey
                                            || usesFmToneStack
                                            ? secondaryTonePanel
                                            : primaryTonePanel);
    }

    auto motionPanel = moduleBounds[4].reduced(12, 9);
    motionPanel.removeFromTop(20);
    motionPanel.removeFromTop(30);
    motionPanel.removeFromTop(4);
    if (usesSnNoiseModeSegment(displayedMode)
        && displayedMode != chipper::ChipMode::nes
        && displayedMode != chipper::ChipMode::dmg
        && displayedMode != chipper::ChipMode::paula)
    {
        auto noiseModePanel = primaryTonePanel;
        if (displayedMode == chipper::ChipMode::sid)
            noiseModePanel = motionPanel;
        else if (displayedMode == chipper::ChipMode::nes || usesSampleToneStack)
            noiseModePanel = secondaryTonePanel;
        else if (usesFmToneStack)
            noiseModePanel = tertiaryTonePanel;

        placeSnNoiseModeSegment(noiseModePanel);
    }

    auto envelopePanel = moduleBounds[3].reduced(12, 9);
    envelopePanel.removeFromTop(20);
    if (displayedMode == chipper::ChipMode::sid)
        envelopePanel.removeFromTop(4);
    else
    {
        if (moduleSummaryLabels[3].isVisible())
            envelopePanel.removeFromTop(30);
        envelopePanel.removeFromTop(4);
    }

    auto envelopeDecayPanel = envelopePanel;
    envelopeDecayPanel.removeFromBottom(6);
    if (displayedMode == chipper::ChipMode::sid)
    {
        ymEnvelopePreview.setBounds({});
        placeSidAdsrControls(envelopeDecayPanel);
    }
    else if (displayedMode == chipper::ChipMode::ym2149)
    {
        auto speedArea = envelopeDecayPanel.removeFromTop(std::min(56, envelopeDecayPanel.getHeight()));
        placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, speedArea);
        envelopeDecayPanel.removeFromTop(6);
        ymEnvelopePreview.setBounds(envelopeDecayPanel.reduced(0, 1));
    }
    else if (displayedMode == chipper::ChipMode::spc700)
    {
        auto shapeArea = envelopeDecayPanel.removeFromTop(std::min(44, envelopeDecayPanel.getHeight()));
        placeYmEnvelopeShapeSegment(shapeArea);
        envelopeDecayPanel.removeFromTop(std::min(4, envelopeDecayPanel.getHeight()));
        placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, envelopeDecayPanel);
        ymEnvelopePreview.setBounds({});
    }
    else if (displayedMode == chipper::ChipMode::paula)
    {
        const auto filterHeight = std::clamp(envelopeDecayPanel.getHeight() / 2, 50, 64);
        const auto gapHeight = std::min(6, std::max(0, envelopeDecayPanel.getHeight() - filterHeight));
        const auto decayHeight = std::max(44, envelopeDecayPanel.getHeight() - filterHeight - gapHeight);
        auto decayArea = envelopeDecayPanel.removeFromTop(std::min(decayHeight, envelopeDecayPanel.getHeight()));
        envelopeDecayPanel.removeFromTop(std::min(gapHeight, envelopeDecayPanel.getHeight()));

        placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, decayArea);
        placeSnNoiseModeSegment(envelopeDecayPanel);
        ymEnvelopePreview.setBounds({});
    }
    else if (usesFmEnvelopeShapePanel)
    {
        auto shapeArea = envelopeDecayPanel.removeFromTop(std::min(46, envelopeDecayPanel.getHeight()));
        placeYmEnvelopeShapeSegment(shapeArea);
        envelopeDecayPanel.removeFromTop(std::min(6, envelopeDecayPanel.getHeight()));
        placeFmOperatorRegisterSurface(displayedMode, envelopeDecayPanel);
        ymEnvelopePreview.setBounds({});
    }
    else if (usesFmOperatorRegisterSurface)
    {
        placeFmOperatorRegisterSurface(displayedMode, envelopeDecayPanel);
        ymEnvelopePreview.setBounds({});
    }
    else if (wavetableLayout)
    {
        placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, envelopeDecayPanel);
        ymEnvelopePreview.setBounds({});
    }
    else
    {
        placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, envelopeDecayPanel);
        ymEnvelopePreview.setBounds({});
    }

    auto outputPanel = sampleLayout ? juce::Rectangle<int> {} : moduleBounds[5].reduced(12, 9);
    if (! sampleLayout)
    {
        outputPanel.removeFromTop(20);
        if (moduleSummaryLabels[5].isVisible())
            outputPanel.removeFromTop(30);
        outputPanel.removeFromTop(4);
        outputPanel.removeFromBottom(6);
    }
    auto outputTonePanel = outputPanel;
    auto outputRoutePanel = outputPanel;
    const auto splitOutputCard = displayedMode != chipper::ChipMode::sid
        && usesStereoSpreadControl(displayedMode)
        && usesDmgStereoRouteSegment(displayedMode);
    if (splitOutputCard)
    {
        const auto availableHeight = outputPanel.getHeight();
        const auto routeHeight = std::clamp((availableHeight / 2) + 12, 58, 72);
        const auto gapHeight = std::min(6, std::max(0, availableHeight - routeHeight));
        const auto toneHeight = std::max(38, availableHeight - routeHeight - gapHeight);
        outputTonePanel = outputPanel.removeFromTop(std::min(toneHeight, outputPanel.getHeight()));
        outputPanel.removeFromTop(std::min(gapHeight, outputPanel.getHeight()));
        outputRoutePanel = outputPanel;
    }
    if (displayedMode != chipper::ChipMode::sid && ! sampleLayout)
        placeLabeledSliderWithReadout(stereoSpreadSlider, stereoSpreadLabel, stereoSpreadValueLabel, outputTonePanel);
    else if (sampleLayout)
    {
        stereoSpreadSlider.setBounds({});
        stereoSpreadLabel.setBounds({});
        stereoSpreadValueLabel.setBounds({});
    }

    auto profilePanel = moduleBounds[0].reduced(12, 9);
    profilePanel.removeFromTop(20);
    profilePanel.removeFromTop(30);
    profilePanel.removeFromTop(4);
    if (sampleLayout)
    {
        dmgStereoRouteLabel.setBounds({});
        dmgStereoRouteValueLabel.setBounds({});
        dmgStereoRouteBox.setBounds({});
        for (auto& button : dmgStereoRouteButtons)
            button.setBounds({});
    }
    else
    {
        placeDmgStereoRouteSegment(displayedMode == chipper::ChipMode::sid ? profilePanel : outputRoutePanel);
    }

    area.removeFromTop(12);
    globalStripBounds = area.removeFromTop(performanceStripHeight);
    auto strip = globalStripBounds.reduced(12, 8);
    auto stripHeader = strip.removeFromTop(20);
    globalStripLabel.setBounds(stripHeader.removeFromLeft(116));
    stripHeader.removeFromLeft(10);
    macroSummaryLabel.setBounds(stripHeader);
    strip.removeFromTop(4);

    const auto controlGap = 10;
    const auto controlColumnWidth = (strip.getWidth() - (controlGap * 2)) / 3;
    const auto controlRowHeight = (strip.getHeight() - controlGap) / 2;

    std::array<juce::Rectangle<int>, 6> controlCells;
    for (size_t i = 0; i < controlCells.size(); ++i)
    {
        const auto row = static_cast<int>(i / 3);
        const auto column = static_cast<int>(i % 3);
        controlCells[i] = {
            strip.getX() + (column * (controlColumnWidth + controlGap)),
            strip.getY() + (row * (controlRowHeight + controlGap)),
            controlColumnWidth,
            controlRowHeight
        };
    }

    if (displayedMode == chipper::ChipMode::nes)
    {
        constexpr int minNesMacroRowHeight = 64;
        constexpr int maxNesMacroRowHeight = 76;
        constexpr int minNesDecayRowHeight = 48;
        constexpr int maxNesDecayRowHeight = 58;

        auto nesRow = strip;
        const auto macroRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(nesRow.getHeight()) * 0.28)),
                                               minNesMacroRowHeight,
                                               maxNesMacroRowHeight);
        auto macroRow = nesRow.removeFromTop(std::min(macroRowHeight, nesRow.getHeight()));
        nesRow.removeFromTop(std::min(controlGap, nesRow.getHeight()));

        const auto nesMacroColumnWidth = (macroRow.getWidth() - (controlGap * 2)) / 3;
        for (size_t i = 0; i < 3u; ++i)
        {
            controlCells[i] = {
                macroRow.getX() + (static_cast<int>(i) * (nesMacroColumnWidth + controlGap)),
                macroRow.getY(),
                nesMacroColumnWidth,
                macroRow.getHeight()
            };
        }

        controlCells[3] = {};
        const auto decayRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(nesRow.getHeight()) * 0.30)),
                                               minNesDecayRowHeight,
                                               maxNesDecayRowHeight);
        controlCells[4] = nesRow.removeFromTop(std::min(decayRowHeight, nesRow.getHeight()));
        nesRow.removeFromTop(std::min(controlGap, nesRow.getHeight()));
        controlCells[5] = nesRow;
    }

    if (displayedMode == chipper::ChipMode::nes)
    {
        nativeGroupLabels[0].setBounds({});
        nativeLabels[0].setBounds({});
        nativeSliders[0].setBounds({});
        controlValueLabels[0].setBounds({});
        placeGroupedSlider(nativeSliders[1], nativeGroupLabels[1], nativeLabels[1], controlValueLabels[1], controlCells[0]);
        placeGroupedSlider(nativeSliders[2], nativeGroupLabels[2], nativeLabels[2], controlValueLabels[2], controlCells[1]);
        placeGroupedSlider(nativeSliders[3], nativeGroupLabels[3], nativeLabels[3], controlValueLabels[3], controlCells[2]);
        placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, controlCells[4]);
    }
    else if (displayedMode == chipper::ChipMode::dmg)
    {
        nativeGroupLabels[0].setBounds({});
        nativeSliders[0].setBounds({});
        controlValueLabels[0].setBounds({});
        placeGroupedSlider(nativeSliders[1], nativeGroupLabels[1], nativeLabels[1], controlValueLabels[1], controlCells[0]);
        placeGroupedSlider(nativeSliders[2], nativeGroupLabels[2], nativeLabels[2], controlValueLabels[2], controlCells[1]);
        placeGroupedSlider(nativeSliders[3], nativeGroupLabels[3], nativeLabels[3], controlValueLabels[3], controlCells[2]);
    }
    else if (sampleLayout)
    {
        placeGroupedSlider(nativeSliders[0], nativeGroupLabels[0], nativeLabels[0], controlValueLabels[0], controlCells[0]);
        placeGroupedSlider(nativeSliders[2], nativeGroupLabels[2], nativeLabels[2], controlValueLabels[2], controlCells[2]);
        placeGroupedSlider(nativeSliders[3], nativeGroupLabels[3], nativeLabels[3], controlValueLabels[3], controlCells[3]);
    }
    else
    {
        placeGroupedSlider(nativeSliders[0], nativeGroupLabels[0], nativeLabels[0], controlValueLabels[0], controlCells[0]);
        placePulseDutySegment(controlCells[0]);
        placeGroupedSlider(nativeSliders[1], nativeGroupLabels[1], nativeLabels[1], controlValueLabels[1], controlCells[1]);
        if (displayedMode != chipper::ChipMode::sid)
            placeGroupedSlider(nativeSliders[2], nativeGroupLabels[2], nativeLabels[2], controlValueLabels[2], controlCells[2]);

        const auto sustainCell = displayedMode == chipper::ChipMode::sid ? controlCells[2] : controlCells[3];
        placeGroupedSlider(nativeSliders[3], nativeGroupLabels[3], nativeLabels[3], controlValueLabels[3], sustainCell);
        placeToneNoiseMixSegment(sustainCell);
    }

    auto nesDmcModuleCell = moduleBounds[5].reduced(12, 9);
    if (displayedMode == chipper::ChipMode::nes)
    {
        nesDmcModuleCell.removeFromTop(20);
        if (moduleSummaryLabels[5].isVisible())
            nesDmcModuleCell.removeFromTop(30);
        nesDmcModuleCell.removeFromTop(4);
        nesDmcModuleCell.removeFromBottom(6);
    }
    auto sampleModuleCell = moduleBounds[5].reduced(12, 9);
    if (sampleLayout)
    {
        sampleModuleCell.removeFromTop(20);
        if (moduleSummaryLabels[5].isVisible())
            sampleModuleCell.removeFromTop(30);
        sampleModuleCell.removeFromTop(4);
        sampleModuleCell.removeFromBottom(6);
    }

    const auto utilityCell = displayedMode == chipper::ChipMode::sid
        ? controlCells[3]
        : (displayedMode == chipper::ChipMode::nes
               ? nesDmcModuleCell
               : (sampleLayout ? sampleModuleCell : controlCells[4]));
    if (displayedMode == chipper::ChipMode::nes)
    {
        clockSlider.setBounds({});
        clockLabel.setBounds({});
        auto dmcCell = utilityCell;
        constexpr int standardDmcControlHeight = 28;
        const auto useTwoColumnDmc = dmcCell.getWidth() >= 760 && dmcCell.getHeight() >= 180;
        auto dmcControlColumn = dmcCell;
        auto dmcWaveformColumn = dmcCell;
        if (useTwoColumnDmc)
        {
            const auto columnGap = 12;
            const auto controlWidth = std::clamp(static_cast<int>(std::round(static_cast<double>(dmcCell.getWidth()) * 0.42)), 420, 560);
            dmcControlColumn = dmcCell.removeFromLeft(std::min(controlWidth, dmcCell.getWidth()));
            dmcCell.removeFromLeft(std::min(columnGap, dmcCell.getWidth()));
            dmcWaveformColumn = dmcCell;
        }

        auto directCell = dmcControlColumn.removeFromTop(std::min(44, dmcControlColumn.getHeight()));
        auto rateCell = directCell.removeFromRight(std::max(148, directCell.getWidth() / 3));
        directCell.removeFromRight(8);
        auto directHeader = directCell.removeFromTop(16);
        dmcDirectLabel.setBounds(directHeader.removeFromLeft(86));
        controlValueLabels[4].setJustificationType(juce::Justification::centredRight);
        controlValueLabels[4].setBounds(directHeader);
        dmcDirectSlider.setBounds(directCell.reduced(0, 2));
        dmcRateLabel.setBounds(rateCell.removeFromTop(16));
        dmcRateBox.setBounds(rateCell.removeFromTop(std::min(standardDmcControlHeight, rateCell.getHeight())).reduced(0, 1));
        dmcControlColumn.removeFromTop(8);
        auto sampleHeader = dmcControlColumn.removeFromTop(std::min(standardDmcControlHeight, dmcControlColumn.getHeight()));
        dmcSampleLabel.setText("DMC", juce::dontSendNotification);
        dmcSampleLabel.setBounds(sampleHeader.removeFromLeft(42));
        dmcPlaybackModeLabel.setBounds({});
        dmcMapRootLabel.setBounds({});
        dmcPlaybackModeBox.setBounds(sampleHeader.removeFromLeft(104).reduced(0, 1));
        sampleHeader.removeFromLeft(4);
        const auto buttonWidth = std::max(42, (sampleHeader.getWidth() - 12) / 3);
        dmcSampleFileButton.setBounds(sampleHeader.removeFromLeft(buttonWidth).reduced(0, 1));
        sampleHeader.removeFromLeft(4);
        dmcSampleFolderButton.setBounds(sampleHeader.removeFromLeft(buttonWidth).reduced(0, 1));
        sampleHeader.removeFromLeft(4);
        dmcSampleBankButton.setBounds(sampleHeader.removeFromLeft(buttonWidth).reduced(0, 1));
        dmcControlColumn.removeFromTop(6);
        auto sampleRow = dmcControlColumn.removeFromTop(std::min(standardDmcControlHeight, dmcControlColumn.getHeight()));
        auto loopCell = sampleRow.removeFromRight(108);
        sampleRow.removeFromRight(6);
        auto rootCell = sampleRow.removeFromRight(78);
        sampleRow.removeFromRight(6);
        dmcSampleSlotBox.setBounds(sampleRow.reduced(0, 1));
        dmcMapRootBox.setBounds(rootCell.reduced(0, 1));
        dmcLoopButton.setBounds(loopCell.reduced(0, 1));
        spc700LoopModeButton.setBounds({});
        dmcControlColumn.removeFromTop(6);
        dmcSampleStatusLabel.setBounds(dmcControlColumn.removeFromTop(std::min(18, dmcControlColumn.getHeight())).reduced(0, 1));
        if (! useTwoColumnDmc)
            dmcControlColumn.removeFromTop(6);
        sampleWaveformPreview.setBounds((useTwoColumnDmc ? dmcWaveformColumn : dmcControlColumn).reduced(0, 2));
        sampleLoopStartLabel.setBounds({});
        sampleLoopEndLabel.setBounds({});
        sampleLoopStartValueLabel.setBounds({});
        sampleLoopEndValueLabel.setBounds({});
        sampleLoopStartSlider.setBounds({});
        sampleLoopEndSlider.setBounds({});
    }
    else if (displayedMode == chipper::ChipMode::spc700 || displayedMode == chipper::ChipMode::paula)
    {
        clockSlider.setBounds({});
        clockLabel.setBounds({});
        dmcDirectSlider.setBounds({});
        dmcDirectLabel.setBounds({});
        dmcRateLabel.setBounds({});
        dmcRateBox.setBounds({});
        dmcLoopButton.setBounds({});
        spc700LoopModeButton.setBounds({});

        auto sampleCell = utilityCell;
        const auto minimumTwoColumnSampleBankHeight = displayedMode == chipper::ChipMode::spc700 ? 360 : 300;
        const auto twoColumnSampleBank = sampleCell.getWidth() >= 760
            && sampleCell.getHeight() >= minimumTwoColumnSampleBankHeight;
        auto controlColumn = sampleCell;
        auto waveformColumn = sampleCell;
        if (twoColumnSampleBank)
        {
            const auto columnGap = 12;
            const auto controlWidth = std::clamp(static_cast<int>(std::round(static_cast<double>(sampleCell.getWidth()) * 0.40)), 390, 500);
            controlColumn = sampleCell.removeFromLeft(std::min(controlWidth, sampleCell.getWidth()));
            sampleCell.removeFromLeft(std::min(columnGap, sampleCell.getWidth()));
            waveformColumn = sampleCell;
        }

        auto pitchPanel = controlColumn.removeFromTop(std::min(twoColumnSampleBank ? 72 : 84, controlColumn.getHeight()));
        placeGroupedSlider(nativeSliders[1], nativeGroupLabels[1], nativeLabels[1], controlValueLabels[1], pitchPanel);
        controlColumn.removeFromTop(std::min(twoColumnSampleBank ? 6 : 8, controlColumn.getHeight()));

        auto sampleHeader = controlColumn.removeFromTop(std::min(18, controlColumn.getHeight()));
        dmcSampleLabel.setText(displayedMode == chipper::ChipMode::paula ? "Paula Sample" : "SPC700 Sample", juce::dontSendNotification);
        dmcSampleLabel.setBounds(sampleHeader);
        controlColumn.removeFromTop(3);

        constexpr int standardSampleControlHeight = 30;
        auto sampleActionRow = controlColumn.removeFromTop(std::min(standardSampleControlHeight, controlColumn.getHeight()));
        const auto brrButtonWidth = std::max(54, (sampleActionRow.getWidth() - 8) / 3);
        dmcSampleFileButton.setBounds(sampleActionRow.removeFromLeft(brrButtonWidth).reduced(0, 1));
        sampleActionRow.removeFromLeft(4);
        dmcSampleFolderButton.setBounds(sampleActionRow.removeFromLeft(brrButtonWidth).reduced(0, 1));
        sampleActionRow.removeFromLeft(4);
        dmcSampleBankButton.setBounds(sampleActionRow.removeFromLeft(brrButtonWidth).reduced(0, 1));
        controlColumn.removeFromTop(std::min(twoColumnSampleBank ? 5 : 6, controlColumn.getHeight()));

        auto playbackRow = controlColumn.removeFromTop(std::min(standardSampleControlHeight, controlColumn.getHeight()));
        dmcPlaybackModeLabel.setText("Playback", juce::dontSendNotification);
        dmcPlaybackModeLabel.setBounds(playbackRow.removeFromLeft(twoColumnSampleBank ? 70 : 78));
        playbackRow.removeFromLeft(6);
        dmcPlaybackModeBox.setBounds(playbackRow.reduced(0, 1));
        controlColumn.removeFromTop(std::min(twoColumnSampleBank ? 4 : 5, controlColumn.getHeight()));
        auto sampleRow = controlColumn.removeFromTop(std::min(standardSampleControlHeight, controlColumn.getHeight()));
        auto rootCell = sampleRow.removeFromRight(std::min(twoColumnSampleBank ? 116 : 108, sampleRow.getWidth()));
        sampleRow.removeFromRight(std::min(8, sampleRow.getWidth()));
        dmcMapRootLabel.setBounds(rootCell.removeFromLeft(38));
        rootCell.removeFromLeft(std::min(3, rootCell.getWidth()));
        dmcSampleSlotBox.setBounds(sampleRow.reduced(0, 1));
        dmcMapRootBox.setBounds(rootCell.reduced(0, 1));

        if (displayedMode == chipper::ChipMode::spc700)
        {
            controlColumn.removeFromTop(std::min(twoColumnSampleBank ? 3 : 4, controlColumn.getHeight()));
            auto playbackLifeRow = controlColumn.removeFromTop(std::min(standardSampleControlHeight, controlColumn.getHeight()));
            dmgStereoRouteLabel.setBounds({});
            spc700LoopModeButton.setBounds(playbackLifeRow.removeFromLeft(std::min(170, playbackLifeRow.getWidth())).reduced(0, 1));
            dmgStereoRouteBox.setBounds({});
            for (auto& button : dmgStereoRouteButtons)
                button.setBounds({});
            controlColumn.removeFromTop(std::min(twoColumnSampleBank ? 4 : 5, controlColumn.getHeight()));
        }
        else
        {
            dmgStereoRouteLabel.setBounds({});
            dmgStereoRouteBox.setBounds({});
            spc700LoopModeButton.setBounds({});
        }

        auto loopColumn = twoColumnSampleBank ? waveformColumn : controlColumn;
        if (twoColumnSampleBank)
        {
            controlColumn.removeFromTop(std::min(6, controlColumn.getHeight()));
            const auto statusHeight = displayedMode == chipper::ChipMode::paula ? 26 : 40;
            dmcSampleStatusLabel.setBounds(controlColumn.removeFromTop(std::min(statusHeight, controlColumn.getHeight())).reduced(0, 1));
        }
        else
        {
            controlColumn.removeFromTop(4);
        }

        if (displayedMode == chipper::ChipMode::spc700)
        {
            auto placeLoopRow = [](juce::Label& label,
                                   juce::Label& valueLabel,
                                   juce::Slider& slider,
                                   juce::Rectangle<int> row)
            {
                label.setBounds(row.removeFromLeft(std::min(76, row.getWidth())));
                valueLabel.setBounds(row.removeFromRight(std::min(54, row.getWidth())));
                slider.setBounds(row.reduced(0, 7));
            };

            auto startLoop = loopColumn.removeFromTop(std::min(25, loopColumn.getHeight()));
            placeLoopRow(sampleLoopStartLabel, sampleLoopStartValueLabel, sampleLoopStartSlider, startLoop);
            loopColumn.removeFromTop(3);
            auto endLoop = loopColumn.removeFromTop(std::min(25, loopColumn.getHeight()));
            placeLoopRow(sampleLoopEndLabel, sampleLoopEndValueLabel, sampleLoopEndSlider, endLoop);
            loopColumn.removeFromTop(6);
        }
        else
        {
            sampleLoopStartLabel.setBounds({});
            sampleLoopEndLabel.setBounds({});
            sampleLoopStartValueLabel.setBounds({});
            sampleLoopEndValueLabel.setBounds({});
            sampleLoopStartSlider.setBounds({});
            sampleLoopEndSlider.setBounds({});
        }

        if (! twoColumnSampleBank)
        {
            dmcSampleStatusLabel.setBounds(controlColumn.removeFromTop(std::min(16, controlColumn.getHeight())).reduced(0, 1));
            controlColumn.removeFromTop(2);
        }

        auto waveformBounds = loopColumn.reduced(0, 1);
        if (displayedMode == chipper::ChipMode::paula && twoColumnSampleBank)
        {
            const auto previewHeight = std::min(waveformBounds.getHeight(), 220);
            sampleWaveformPreview.setBounds(waveformBounds.removeFromTop(previewHeight));
        }
        else
        {
            sampleWaveformPreview.setBounds(waveformBounds);
        }
    }
    else
    {
        dmcDirectSlider.setBounds({});
        dmcDirectLabel.setBounds({});
        dmcRateLabel.setBounds({});
        dmcRateBox.setBounds({});
        dmcSampleLabel.setBounds({});
        dmcSampleStatusLabel.setBounds({});
        dmcPlaybackModeLabel.setBounds({});
        dmcMapRootLabel.setBounds({});
        dmcSampleFileButton.setBounds({});
        dmcSampleFolderButton.setBounds({});
        dmcSampleBankButton.setBounds({});
        dmcSampleSlotBox.setBounds({});
        dmcPlaybackModeBox.setBounds({});
        dmcMapRootBox.setBounds({});
        dmcLoopButton.setBounds({});
        spc700LoopModeButton.setBounds({});
        sampleWaveformPreview.setBounds({});
        sampleLoopStartLabel.setBounds({});
        sampleLoopEndLabel.setBounds({});
        sampleLoopStartValueLabel.setBounds({});
        sampleLoopEndValueLabel.setBounds({});
        sampleLoopStartSlider.setBounds({});
        sampleLoopEndSlider.setBounds({});
        if (wavetableLayout)
        {
            clockSlider.setBounds({});
            clockLabel.setBounds({});
            controlValueLabels[4].setBounds({});
        }
        else
            placeLabeledSliderWithReadout(clockSlider, clockLabel, controlValueLabels[4], utilityCell);
    }

    auto outputCell = displayedMode == chipper::ChipMode::sid
        ? controlCells[4].getUnion(controlCells[5])
        : controlCells[5];
    if (displayedMode == chipper::ChipMode::nes)
        outputCell.removeFromTop(std::min(6, outputCell.getHeight()));
    auto outputHeader = outputCell.removeFromTop(18);
    outputLabel.setBounds(outputHeader.removeFromLeft(96));
    controlValueLabels[5].setJustificationType(juce::Justification::centredRight);
    controlValueLabels[5].setBounds(outputHeader);
    outputCell.removeFromTop(3);
    outputSlider.setBounds(outputCell.removeFromTop(20).reduced(0, 1));
    outputCell.removeFromTop(4);
    outputScopePreview.setBounds(outputCell.reduced(0, 1));

    auto footer = getLocalBounds().reduced(16).removeFromBottom(44);
    buildLabel.setBounds(footer.removeFromRight(190));
    footer.removeFromRight(8);
    midiCcLabel.setBounds(footer.removeFromRight(136));
    footer.removeFromRight(8);
    statusLabel.setBounds(footer);
}

void ChipperAudioProcessorEditor::timerCallback()
{
    updateDescriptorText();
    updateLiveControlReadouts();
    statusLabel.setText(audioProcessor.currentCoreStatus(), juce::dontSendNotification);
    statusLabel.setTooltip(audioProcessor.currentCoreStatusDetail());
    outputScopePreview.setSamples(audioProcessor.outputScopeSnapshot());
    updateSampleWaveformPreview(displayedMode);
}

void ChipperAudioProcessorEditor::addLabeledSlider(juce::Slider& slider, juce::Label& label, const juce::String& fallbackText)
{
    label.setText(fallbackText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    label.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 74, 24);
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xfff7d85a));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff56c7d8));
    addAndMakeVisible(slider);
}

void ChipperAudioProcessorEditor::placeLabeledSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromTop(18));
    slider.setBounds(bounds.reduced(0, 2));
}

void ChipperAudioProcessorEditor::placeGroupedSlider(juce::Slider& slider,
                                                     juce::Label& groupLabel,
                                                     juce::Label& label,
                                                     juce::Label& valueLabel,
                                                     juce::Rectangle<int> bounds)
{
    valueLabel.setJustificationType(juce::Justification::centredLeft);
    groupLabel.setBounds(bounds.removeFromTop(13));
    label.setBounds(bounds.removeFromTop(17));
    slider.setBounds(bounds.removeFromTop(26).reduced(0, 1));
    valueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeLabeledSliderWithReadout(juce::Slider& slider,
                                                                juce::Label& label,
                                                                juce::Label& valueLabel,
                                                                juce::Rectangle<int> bounds)
{
    valueLabel.setJustificationType(juce::Justification::centredLeft);

    if (bounds.getHeight() <= 42)
    {
        auto header = bounds.removeFromTop(std::min(18, bounds.getHeight()));
        label.setBounds(header.removeFromLeft(std::min(180, header.getWidth())));
        valueLabel.setBounds(header);
        bounds.removeFromTop(std::min(2, bounds.getHeight()));
        slider.setBounds(bounds.reduced(0, 1));
        return;
    }

    auto header = bounds.removeFromTop(std::min(22, bounds.getHeight()));
    label.setBounds(header.removeFromLeft(std::min(220, header.getWidth())));
    if (header.getWidth() >= 76)
        valueLabel.setBounds(header.removeFromRight(std::min(112, header.getWidth())));
    else
        valueLabel.setBounds({});

    bounds.removeFromTop(std::min(3, bounds.getHeight()));
    slider.setBounds(bounds.removeFromTop(std::min(30, bounds.getHeight())).reduced(0, 2));

    if (! bounds.isEmpty())
    {
        if (valueLabel.getBounds().isEmpty())
            valueLabel.setBounds(bounds.removeFromTop(std::min(18, bounds.getHeight())));
        else
            valueLabel.setTooltip(valueLabel.getText());
    }
}

void ChipperAudioProcessorEditor::placeSidAdsrControls(juce::Rectangle<int> bounds)
{
    auto speedRow = bounds.removeFromTop(std::min(20, bounds.getHeight()));
    envelopeDecayLabel.setBounds(speedRow.removeFromLeft(86));
    envelopeDecayValueLabel.setBounds(speedRow.removeFromRight(132));
    envelopeDecaySlider.setBounds(speedRow.reduced(0, 1));
    bounds.removeFromTop(4);

    for (auto& label : sidAdsrHeaderLabels)
        label.setBounds({});

    constexpr auto voiceGap = 10;
    const auto voiceWidth = (bounds.getWidth() - (voiceGap * static_cast<int>(sidAdsrVoiceCount - 1u))) / static_cast<int>(sidAdsrVoiceCount);
    const auto voiceHeight = bounds.getHeight();
    for (size_t voice = 0; voice < sidAdsrVoiceCount; ++voice)
    {
        auto voiceColumn = bounds.removeFromLeft(voiceWidth).reduced(0, 1);
        if (voice + 1u < sidAdsrVoiceCount)
            bounds.removeFromLeft(voiceGap);

        sidEnvelopeVoiceLabels[voice].setBounds(voiceColumn.removeFromTop(std::min(15, voiceColumn.getHeight())));
        voiceColumn.removeFromTop(1);

        const auto availableVoiceHeight = voiceColumn.getHeight();
        const auto minimumControlHeight = availableVoiceHeight >= 140 ? 72 : 46;
        const auto previewReserve = availableVoiceHeight >= 140 ? 62
            : (availableVoiceHeight >= 110 ? 52
                   : (availableVoiceHeight >= 86 ? 42
                          : (availableVoiceHeight >= 70 ? 32 : 0)));
        auto sliderRow = voiceColumn.removeFromTop(std::max(minimumControlHeight, voiceColumn.getHeight() - previewReserve - 4));
        sliderRow = sliderRow.withHeight(std::min(sliderRow.getHeight(), voiceHeight));

        constexpr auto fieldGap = 4;
        const auto fieldWidth = (sliderRow.getWidth() - (fieldGap * static_cast<int>(sidAdsrFieldCount - 1u))) / static_cast<int>(sidAdsrFieldCount);
        for (size_t field = 0; field < sidAdsrFieldCount; ++field)
        {
            const auto index = (voice * sidAdsrFieldCount) + field;
            auto cell = sliderRow.removeFromLeft(fieldWidth);
            if (field + 1u < sidAdsrFieldCount)
                sliderRow.removeFromLeft(fieldGap);

            sidAdsrLabels[index].setBounds(cell.removeFromTop(std::min(12, cell.getHeight())));
            sidAdsrValueLabels[index].setBounds(cell.removeFromBottom(std::min(13, cell.getHeight())));
            sidAdsrSliders[index].setBounds(cell.reduced(0, 1));
            sidAdsrBoxes[index].setBounds({});
        }

        voiceColumn.removeFromTop(4);
        if (voiceColumn.getHeight() >= 24)
            sidEnvelopePreviews[voice].setBounds(voiceColumn.removeFromTop(std::min(64, voiceColumn.getHeight())).reduced(0, 1));
        else
            sidEnvelopePreviews[voice].setBounds({});
    }
}

void ChipperAudioProcessorEditor::placeFmOperatorRegisterSurface(chipper::ChipMode mode, juce::Rectangle<int> bounds)
{
    const auto rowCount = mode == chipper::ChipMode::opl3 ? 3u : fmOperatorReadoutRows;
    const auto rowGap = 4;
    const auto availableRows = static_cast<int>(rowCount);
    const auto rowHeight = availableRows > 0
        ? std::clamp((bounds.getHeight() - (rowGap * (availableRows - 1))) / availableRows, 13, 20)
        : 0;

    for (size_t i = 0; i < fmOperatorNameLabels.size(); ++i)
    {
        if (i >= rowCount || bounds.getHeight() < 8)
        {
            fmOperatorNameLabels[i].setBounds({});
            fmOperatorValueLabels[i].setBounds({});
            continue;
        }

        auto row = bounds.removeFromTop(std::min(rowHeight, bounds.getHeight()));
        const auto nameWidth = mode == chipper::ChipMode::opl3 ? 42 : 36;
        fmOperatorNameLabels[i].setBounds(row.removeFromLeft(std::min(nameWidth, row.getWidth())));
        row.removeFromLeft(std::min(6, row.getWidth()));
        fmOperatorValueLabels[i].setBounds(row.reduced(3, 0));
        bounds.removeFromTop(std::min(rowGap, bounds.getHeight()));
    }
}

void ChipperAudioProcessorEditor::placePulseDutySegment(juce::Rectangle<int> bounds)
{
    if (usesPulse2DutySegment(displayedMode))
    {
        const auto compact = bounds.getHeight() < 84;
        const auto groupHeight = compact ? 11 : 13;
        const auto headerHeight = compact ? 13 : 14;
        const auto buttonHeight = compact ? 20 : 22;
        const auto gapHeight = compact ? 2 : 3;

        nativeGroupLabels[0].setBounds(bounds.removeFromTop(std::min(groupHeight, bounds.getHeight())));

        auto pulse1Header = bounds.removeFromTop(std::min(headerHeight, bounds.getHeight()));
        nativeLabels[0].setBounds(pulse1Header.removeFromLeft(118));
        controlValueLabels[0].setJustificationType(juce::Justification::centredRight);
        controlValueLabels[0].setBounds(pulse1Header);

        pulseDutySegmentBounds = bounds.removeFromTop(std::min(buttonHeight, bounds.getHeight())).reduced(0, 1);
        layoutSegmentedButtons(pulseDutyButtons, pulseDutySegmentBounds, pulseDutyButtons.size());

        bounds.removeFromTop(std::min(gapHeight, bounds.getHeight()));
        placePulse2DutySegment(bounds);
        return;
    }

    controlValueLabels[0].setJustificationType(juce::Justification::centredLeft);
    bounds.removeFromTop(30);
    pulseDutySegmentBounds = bounds.removeFromTop(26).reduced(0, 1);
    layoutSegmentedButtons(pulseDutyButtons, pulseDutySegmentBounds, pulseDutyButtons.size());
}

void ChipperAudioProcessorEditor::placePulse2DutySegment(juce::Rectangle<int> bounds)
{
    const auto compact = bounds.getHeight() < 38;
    auto header = bounds.removeFromTop(std::min(compact ? 13 : 14, bounds.getHeight()));
    pulse2DutyLabel.setBounds(header.removeFromLeft(118));
    pulse2DutyValueLabel.setBounds(header);

    pulse2DutySegmentBounds = bounds.removeFromTop(std::min(compact ? 20 : 22, bounds.getHeight())).reduced(0, 1);
    layoutSegmentedButtons(pulse2DutyButtons, pulse2DutySegmentBounds, pulse2DutyButtons.size());
}

void ChipperAudioProcessorEditor::placeWaveShapeSegment(juce::Rectangle<int> bounds)
{
    const auto tight = bounds.getHeight() < 40;
    const auto compact = bounds.getHeight() < 64;
    waveShapeLabel.setBounds(bounds.removeFromTop(std::min(tight ? 12 : (compact ? 15 : 18), bounds.getHeight())));
    bounds.removeFromTop(std::min(tight ? 1 : (compact ? 1 : 0), bounds.getHeight()));
    waveShapeSegmentBounds = bounds.removeFromTop(std::min(tight ? 20 : (compact ? 24 : 28), bounds.getHeight())).reduced(0, 1);
    layoutSegmentedButtons(waveShapeButtons, waveShapeSegmentBounds, waveShapeButtons.size());

    const auto canShowReadout = ! compact
        && ! tight
        && bounds.getHeight() >= 18
        && displayedMode != chipper::ChipMode::paula;
    waveShapeValueLabel.setBounds(canShowReadout ? bounds : juce::Rectangle<int> {});
}

void ChipperAudioProcessorEditor::placeFmAlgorithmControl(juce::Rectangle<int> bounds)
{
    const auto compact = bounds.getHeight() < 56;
    auto header = bounds.removeFromTop(std::min(compact ? 15 : 18, bounds.getHeight()));
    waveShapeLabel.setBounds(header.removeFromLeft(labelReservation(header, 128)));
    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setBounds(header);
    bounds.removeFromTop(std::min(compact ? 2 : 3, bounds.getHeight()));

    if (compact)
    {
        fmAlgorithmBox.setBounds(bounds.removeFromTop(std::min(24, bounds.getHeight())).reduced(0, 1));
        fmAlgorithmPreview.setBounds({});
        waveShapeSegmentBounds = {};
        for (auto& button : waveShapeButtons)
            button.setBounds({});
        return;
    }

    const auto selectorWidth = std::clamp(bounds.getWidth() / 3, 142, 190);
    auto selectorArea = bounds.removeFromLeft(std::min(selectorWidth, bounds.getWidth()));
    fmAlgorithmBox.setBounds(selectorArea.removeFromTop(std::min(28, selectorArea.getHeight())).reduced(0, 1));
    if (bounds.getWidth() > 12)
        bounds.removeFromLeft(8);
    fmAlgorithmPreview.setBounds(bounds.reduced(0, 1));
    waveShapeSegmentBounds = {};
    for (auto& button : waveShapeButtons)
        button.setBounds({});
}

void ChipperAudioProcessorEditor::placeOplWaveformControl(juce::Rectangle<int> bounds)
{
    const auto compact = bounds.getHeight() < 56;
    auto header = bounds.removeFromTop(std::min(compact ? 15 : 18, bounds.getHeight()));
    waveShapeLabel.setBounds(header.removeFromLeft(labelReservation(header, 128)));
    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setBounds(header);
    bounds.removeFromTop(std::min(compact ? 2 : 3, bounds.getHeight()));

    if (compact)
    {
        oplWaveformBox.setBounds(bounds.removeFromTop(std::min(24, bounds.getHeight())).reduced(0, 1));
        oplWaveformPreview.setBounds({});
        waveShapeSegmentBounds = {};
        for (auto& button : waveShapeButtons)
            button.setBounds({});
        return;
    }

    const auto selectorWidth = std::clamp(bounds.getWidth() / 3, 142, 190);
    auto selectorArea = bounds.removeFromLeft(std::min(selectorWidth, bounds.getWidth()));
    oplWaveformBox.setBounds(selectorArea.removeFromTop(std::min(28, selectorArea.getHeight())).reduced(0, 1));
    if (bounds.getWidth() > 12)
        bounds.removeFromLeft(8);
    oplWaveformPreview.setBounds(bounds.reduced(0, 1));
    waveShapeSegmentBounds = {};
    for (auto& button : waveShapeButtons)
        button.setBounds({});
}

void ChipperAudioProcessorEditor::placeOpllInstrumentControl(juce::Rectangle<int> bounds)
{
    const auto* rhythmSpec = chipper::parameterSpecFor(displayedMode, chipper::ChipParameterRole::ymEnvelopeShape);
    const auto rhythmCount = rhythmSpec != nullptr
                                 ? std::min(ymEnvelopeShapeButtons.size(), rhythmSpec->choices.size())
                                 : ymEnvelopeShapeButtons.size();
    const auto compact = bounds.getHeight() < 88;
    const auto useColumns = bounds.getWidth() >= 430 && ! compact;
    const auto rowGap = compact ? 5 : 8;
    const auto controlHeight = compact ? 28 : 32;
    const auto labelHeight = compact ? 15 : 17;
    const auto readoutHeight = compact ? 14 : 18;

    if (compact)
    {
        const auto availableHeight = bounds.getHeight();
        const auto readoutFits = availableHeight >= 74;
        const auto compactLabelHeight = readoutFits ? 14 : 12;
        const auto compactControlHeight = readoutFits ? 26 : 24;
        const auto compactGap = readoutFits ? 4 : 3;

        waveShapeLabel.setBounds(bounds.removeFromTop(std::min(compactLabelHeight, bounds.getHeight())));
        opllInstrumentBox.setBounds(bounds.removeFromTop(std::min(compactControlHeight, bounds.getHeight())).reduced(0, 1));
        waveShapeValueLabel.setBounds(readoutFits
            ? bounds.removeFromTop(std::min(14, bounds.getHeight()))
            : juce::Rectangle<int> {});

        bounds.removeFromTop(std::min(compactGap, bounds.getHeight()));
        ymEnvelopeShapeLabel.setBounds(bounds.removeFromTop(std::min(compactLabelHeight, bounds.getHeight())));
        ymEnvelopeShapeSegmentBounds = bounds.removeFromTop(std::min(compactControlHeight, bounds.getHeight())).reduced(0, 1);
        layoutSegmentedButtons(ymEnvelopeShapeButtons, ymEnvelopeShapeSegmentBounds, rhythmCount);
        ymEnvelopeShapeValueLabel.setBounds(readoutFits ? bounds : juce::Rectangle<int> {});

        waveShapeSegmentBounds = {};
        for (auto& button : waveShapeButtons)
            button.setBounds({});
        sidFilterModeBox.setBounds({});
        return;
    }

    auto header = bounds.removeFromTop(std::min(labelHeight, bounds.getHeight()));
    waveShapeLabel.setBounds(header.removeFromLeft(std::min(132, header.getWidth())));
    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setBounds(header);
    bounds.removeFromTop(std::min(rowGap, bounds.getHeight()));

    auto instrumentPanel = bounds;
    auto rhythmPanel = bounds;
    if (useColumns)
    {
        const auto columnGap = 12;
        instrumentPanel = bounds.removeFromLeft(std::max(180, (bounds.getWidth() - columnGap) / 2));
        bounds.removeFromLeft(std::min(columnGap, bounds.getWidth()));
        rhythmPanel = bounds;
    }
    else
    {
        const auto availableRows = bounds.getHeight();
        const auto instrumentHeight = std::min(controlHeight + readoutHeight + 2, std::max(controlHeight, availableRows / 2));
        instrumentPanel = bounds.removeFromTop(std::min(instrumentHeight, bounds.getHeight()));
        bounds.removeFromTop(std::min(rowGap, bounds.getHeight()));
        rhythmPanel = bounds;
    }

    opllInstrumentBox.setBounds(instrumentPanel.removeFromTop(std::min(controlHeight, instrumentPanel.getHeight())).reduced(0, 1));
    waveShapeValueLabel.setBounds(instrumentPanel.removeFromTop(std::min(readoutHeight, instrumentPanel.getHeight())));

    auto rhythmHeader = rhythmPanel.removeFromTop(std::min(labelHeight, rhythmPanel.getHeight()));
    ymEnvelopeShapeLabel.setBounds(rhythmHeader.removeFromLeft(std::min(132, rhythmHeader.getWidth())));
    ymEnvelopeShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    ymEnvelopeShapeValueLabel.setBounds(rhythmHeader);
    rhythmPanel.removeFromTop(std::min(compact ? 3 : 5, rhythmPanel.getHeight()));
    ymEnvelopeShapeSegmentBounds = rhythmPanel.removeFromTop(std::min(controlHeight, rhythmPanel.getHeight())).reduced(0, 1);
    layoutSegmentedButtons(ymEnvelopeShapeButtons, ymEnvelopeShapeSegmentBounds, rhythmCount);

    if (! useColumns)
    {
        ymEnvelopeShapeValueLabel.setJustificationType(juce::Justification::centredLeft);
        ymEnvelopeShapeValueLabel.setBounds(rhythmPanel);
    }

    waveShapeSegmentBounds = {};
    for (auto& button : waveShapeButtons)
        button.setBounds({});
    sidFilterModeBox.setBounds({});
}

void ChipperAudioProcessorEditor::placeSidVoiceWaveControls(juce::Rectangle<int> bounds)
{
    waveShapeLabel.setBounds(bounds.removeFromTop(16));
    if (displayedMode == chipper::ChipMode::sid)
    {
        waveShapeValueLabel.setBounds(bounds);
        return;
    }

    auto row = bounds.removeFromTop(28).reduced(0, 2);
    const auto gap = 6;
    const auto width = (row.getWidth() - (gap * static_cast<int>(sidVoiceWaveBoxes.size() - 1u))) / static_cast<int>(sidVoiceWaveBoxes.size());

    for (size_t i = 0; i < sidVoiceWaveBoxes.size(); ++i)
    {
        auto cell = row.removeFromLeft(width);
        if (i + 1u < sidVoiceWaveBoxes.size())
            row.removeFromLeft(gap);

        sidVoiceWaveLabels[i].setBounds(cell.removeFromLeft(22));
        sidVoiceWaveBoxes[i].setBounds(cell);
    }

    waveShapeValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeHucVoiceWaveControls(juce::Rectangle<int> bounds)
{
    waveShapeLabel.setBounds(bounds.removeFromTop(std::min(18, bounds.getHeight())));
    bounds.removeFromTop(std::min(4, bounds.getHeight()));

    const auto visibleCount = displayedMode == chipper::ChipMode::namcoWsg ? size_t { 8u } : (displayedMode == chipper::ChipMode::scc ? size_t { 5u } : size_t { 6u });
    const auto columns = visibleCount > 6u ? size_t { 4u } : size_t { 3u };
    const auto topCount = std::min(columns, visibleCount);
    const auto bottomCount = visibleCount - topCount;
    const auto rowGap = std::min(8, std::max(4, bounds.getHeight() / 14));
    const auto rowHeight = std::clamp((bounds.getHeight() - rowGap) / 2, 26, 36);
    auto topRow = bounds.removeFromTop(std::min(rowHeight, bounds.getHeight())).reduced(0, 1);
    bounds.removeFromTop(std::min(rowGap, bounds.getHeight()));
    auto bottomRow = bounds.removeFromTop(std::min(rowHeight, bounds.getHeight())).reduced(0, 1);
    const auto gap = 8;

    const auto layoutRow = [this, gap](juce::Rectangle<int> row, size_t first, size_t count)
    {
        if (row.isEmpty())
            return;

        const auto cellWidth = (row.getWidth() - gap * static_cast<int>(count - 1u)) / static_cast<int>(count);
        for (size_t local = 0; local < count; ++local)
        {
            const auto i = first + local;
            auto cell = row.removeFromLeft(cellWidth);
            if (local + 1u < count)
                row.removeFromLeft(gap);

            hucVoiceWaveLabels[i].setBounds(cell.removeFromTop(std::min(12, cell.getHeight())));
            cell.removeFromTop(std::min(2, cell.getHeight()));
            hucVoiceWaveBoxes[i].setBounds(cell.reduced(0, 1));
        }
    };

    layoutRow(topRow, 0, topCount);
    if (bottomCount > 0u)
        layoutRow(bottomRow, topCount, bottomCount);
    waveShapeValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeDmgWaveLevelSegment(juce::Rectangle<int> bounds)
{
    const auto tight = bounds.getHeight() < 40;
    const auto compact = bounds.getHeight() < 64;
    dmgWaveLevelLabel.setBounds(bounds.removeFromTop(std::min(tight ? 12 : (compact ? 15 : 18), bounds.getHeight())));
    bounds.removeFromTop(std::min(tight ? 1 : (compact ? 1 : 0), bounds.getHeight()));
    dmgWaveLevelSegmentBounds = bounds.removeFromTop(std::min(tight ? 20 : (compact ? 24 : 28), bounds.getHeight())).reduced(0, 1);
    layoutSegmentedButtons(dmgWaveLevelButtons, dmgWaveLevelSegmentBounds, dmgWaveLevelButtons.size());

    const auto canShowReadout = ! compact && ! tight && bounds.getHeight() >= 18;
    dmgWaveLevelValueLabel.setBounds(canShowReadout ? bounds : juce::Rectangle<int> {});
}

void ChipperAudioProcessorEditor::placeDmgStereoRouteSegment(juce::Rectangle<int> bounds)
{
    const auto compact = bounds.getHeight() < 52
        || displayedMode == chipper::ChipMode::ym2612
        || displayedMode == chipper::ChipMode::ym2151;
    dmgStereoRouteLabel.setBounds(bounds.removeFromTop(std::min(compact ? 15 : 18, bounds.getHeight())));
    if (displayedMode == chipper::ChipMode::spc700 || displayedMode == chipper::ChipMode::paula)
    {
        dmgStereoRouteBox.setBounds(bounds.removeFromTop(std::min(28, bounds.getHeight())).reduced(0, 1));
        dmgStereoRouteSegmentBounds = {};
        for (auto& button : dmgStereoRouteButtons)
            button.setBounds({});
    }
    else
    {
        bounds.removeFromTop(std::min(compact ? 2 : 0, bounds.getHeight()));
        dmgStereoRouteSegmentBounds = bounds.removeFromTop(std::min(compact ? 24 : 28, bounds.getHeight())).reduced(0, 1);
        layoutSegmentedButtons(dmgStereoRouteButtons, dmgStereoRouteSegmentBounds, dmgStereoRouteButtons.size());
        dmgStereoRouteBox.setBounds({});
    }

    dmgStereoRouteValueLabel.setBounds(compact ? juce::Rectangle<int> {} : bounds);
}

void ChipperAudioProcessorEditor::placeYmEnvelopeShapeSegment(juce::Rectangle<int> bounds)
{
    const auto* spec = chipper::parameterSpecFor(displayedMode, chipper::ChipParameterRole::ymEnvelopeShape);
    const auto visibleCount = spec != nullptr
                                  ? std::min(ymEnvelopeShapeButtons.size(), spec->choices.size())
                                  : ymEnvelopeShapeButtons.size();
    const auto compact = bounds.getHeight() < 52
        || displayedMode == chipper::ChipMode::ym2612
        || displayedMode == chipper::ChipMode::opl3
        || displayedMode == chipper::ChipMode::ym2151
        || displayedMode == chipper::ChipMode::ym2413;

    if (spec != nullptr && spec->surface == chipper::ControlSurface::menu)
    {
        auto header = bounds.removeFromTop(std::min(16, bounds.getHeight()));
        ymEnvelopeShapeLabel.setBounds(header.removeFromLeft(labelReservation(header, 132)));
        ymEnvelopeShapeValueLabel.setJustificationType(juce::Justification::centredRight);
        ymEnvelopeShapeValueLabel.setBounds(header);
        bounds.removeFromTop(3);
        sidFilterModeBox.setBounds(bounds.removeFromTop(std::min(26, bounds.getHeight())).reduced(0, 1));
        ymEnvelopeShapeSegmentBounds = {};
        for (auto& button : ymEnvelopeShapeButtons)
            button.setBounds({});
        return;
    }

    ymEnvelopeShapeLabel.setBounds(bounds.removeFromTop(std::min(compact ? 15 : 18, bounds.getHeight())));
    bounds.removeFromTop(std::min(compact ? 2 : 0, bounds.getHeight()));
    ymEnvelopeShapeSegmentBounds = bounds.removeFromTop(std::min(compact ? 24 : 28, bounds.getHeight())).reduced(0, 1);
    layoutSegmentedButtons(ymEnvelopeShapeButtons, ymEnvelopeShapeSegmentBounds, visibleCount);

    ymEnvelopeShapeValueLabel.setJustificationType(juce::Justification::centredLeft);
    ymEnvelopeShapeValueLabel.setBounds(compact ? juce::Rectangle<int> {} : bounds);
}

void ChipperAudioProcessorEditor::placeSidFilterRoutingControl(juce::Rectangle<int> bounds)
{
    auto header = bounds.removeFromTop(std::min(17, bounds.getHeight()));
    sidFilterRoutingLabel.setBounds(header.removeFromLeft(labelReservation(header, 136)));
    sidFilterRoutingValueLabel.setJustificationType(juce::Justification::centredRight);
    sidFilterRoutingValueLabel.setBounds(header);
    bounds.removeFromTop(3);
    sidFilterRoutingBox.setBounds(bounds.removeFromTop(std::min(28, bounds.getHeight())).reduced(0, 1));
}

void ChipperAudioProcessorEditor::placeYmChannelMixControls(juce::Rectangle<int> bounds)
{
    ymChannelMixLabel.setBounds(bounds.removeFromTop(16));
    auto row = bounds.removeFromTop(28).reduced(0, 2);
    const auto gap = 6;
    const auto width = (row.getWidth() - (gap * static_cast<int>(ymChannelMixBoxes.size() - 1u))) / static_cast<int>(ymChannelMixBoxes.size());

    for (size_t i = 0; i < ymChannelMixBoxes.size(); ++i)
    {
        auto cell = row.removeFromLeft(width);
        if (i + 1u < ymChannelMixBoxes.size())
            row.removeFromLeft(gap);

        ymChannelMixLabels[i].setBounds(cell.removeFromLeft(22));
        ymChannelMixBoxes[i].setBounds(cell);
    }

    ymChannelMixValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeSnNoiseModeSegment(juce::Rectangle<int> bounds)
{
    const auto* spec = chipper::parameterSpecFor(displayedMode, chipper::ChipParameterRole::snNoiseMode);
    const auto choiceCount = spec == nullptr || spec->choices.empty()
                                 ? snNoiseModeButtons.size()
                                 : std::min(snNoiseModeButtons.size(), spec->choices.size());
    const auto tight = bounds.getHeight() < 40;
    const auto compact = bounds.getHeight() < 52
        || displayedMode == chipper::ChipMode::ym2612
        || displayedMode == chipper::ChipMode::opl3
        || displayedMode == chipper::ChipMode::ym2151
        || displayedMode == chipper::ChipMode::ym2413;
    const auto useRegisterSegment = (displayedMode == chipper::ChipMode::nes
                                     || displayedMode == chipper::ChipMode::dmg)
                                    && bounds.getHeight() >= 44;
    snNoiseModeLabel.setBounds(bounds.removeFromTop(std::min(tight ? 12 : (useRegisterSegment ? 15 : (compact ? 15 : 18)), bounds.getHeight())));
    if (displayedMode == chipper::ChipMode::sn76489)
    {
        snNoiseModeBox.setBounds(bounds.removeFromTop(std::min(28, bounds.getHeight())).reduced(0, 1));
        snNoiseModeSegmentBounds = {};
        for (auto& button : snNoiseModeButtons)
            button.setBounds({});
    }
    else
    {
        bounds.removeFromTop(std::min(tight ? 1 : (useRegisterSegment ? 2 : (compact ? 2 : 0)), bounds.getHeight()));
        const auto preferredButtonHeight = useRegisterSegment
            ? std::min(38, std::max(30, bounds.getHeight()))
            : (tight ? 20 : (compact ? 24 : 28));
        snNoiseModeSegmentBounds = bounds.removeFromTop(std::min(preferredButtonHeight, bounds.getHeight())).reduced(0, 1);
        layoutSegmentedButtons(snNoiseModeButtons, snNoiseModeSegmentBounds, choiceCount);
        snNoiseModeBox.setBounds({});
    }

    const auto hideValueReadout = compact
        || tight
        || useRegisterSegment
        || displayedMode == chipper::ChipMode::paula;
    snNoiseModeValueLabel.setBounds(hideValueReadout ? juce::Rectangle<int> {} : bounds);
}

void ChipperAudioProcessorEditor::placeToneNoiseMixSegment(juce::Rectangle<int> bounds)
{
    bounds.removeFromTop(30);
    toneNoiseMixSegmentBounds = bounds.removeFromTop(26).reduced(0, 1);
    layoutSegmentedButtons(toneNoiseMixButtons, toneNoiseMixSegmentBounds, toneNoiseMixButtons.size());
}

float ChipperAudioProcessorEditor::parameterValue(const char* parameterId) const
{
    if (const auto* value = audioProcessor.getValueTreeState().getRawParameterValue(parameterId))
        return value->load();

    return 0.0f;
}

void ChipperAudioProcessorEditor::setParameterValueFromUi(const char* parameterId, float plainValue)
{
    if (auto* parameter = audioProcessor.getValueTreeState().getParameter(parameterId))
    {
        const auto normalized = parameter->convertTo0to1(std::clamp(plainValue, 0.0f, 1.0f));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

void ChipperAudioProcessorEditor::setPlainParameterValueFromUi(const char* parameterId, float plainValue)
{
    if (auto* parameter = audioProcessor.getValueTreeState().getParameter(parameterId))
    {
        const auto normalized = parameter->convertTo0to1(plainValue);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

void ChipperAudioProcessorEditor::setChoiceParameterFromUi(const char* parameterId, int choiceIndex)
{
    if (auto* parameter = audioProcessor.getValueTreeState().getParameter(parameterId))
    {
        const auto normalized = parameter->convertTo0to1(static_cast<float>(choiceIndex));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

void ChipperAudioProcessorEditor::updateMacroChoices(chipper::ChipMode mode)
{
    const auto order = chipper::macroOrder();
    const auto selected = std::clamp(static_cast<int>(std::round(parameterValue(chipper::parameters::id::macro))),
                                     0,
                                     static_cast<int>(order.size() - 1u));

    const juce::ScopedValueSetter<bool> suppress(suppressMacroTemplateApply, true);
    macroBox.clear(juce::dontSendNotification);
    for (size_t i = 0; i < order.size(); ++i)
    {
        const auto& templ = chipper::macroTemplateFor(mode, order[i]);
        const auto label = templ.label.empty() ? juce::String(chipper::toString(order[i])) : juce::String(templ.label);
        macroBox.addItem(label, static_cast<int>(i + 1u));
    }

    macroBox.setSelectedItemIndex(selected, juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updatePresetChoices(chipper::ChipMode mode)
{
    const juce::ScopedValueSetter<bool> suppress(suppressPresetApply, true);
    displayedPresets = chipper::presetBrowserCatalog(mode);
    reloadUserPresetFiles(mode);

    presetBox.clear(juce::dontSendNotification);
    presetBox.addSectionHeading("Start");
    presetBox.addItem("Init Patch", initPresetItemId);

    juce::String currentFactoryCategory;
    for (size_t i = 0; i < displayedPresets.size(); ++i)
    {
        const auto& preset = *displayedPresets[i];
        const auto category = juce::String(preset.category).isNotEmpty()
            ? juce::String(preset.category)
            : juce::String("Factory Presets");
        if (category != currentFactoryCategory)
        {
            const auto categoryCount = std::count_if(displayedPresets.begin(),
                                                     displayedPresets.end(),
                                                     [&category](const chipper::PresetInfo* candidate)
                                                     {
                                                         if (candidate == nullptr)
                                                             return false;

                                                         const auto candidateCategory = juce::String(candidate->category).isNotEmpty()
                                                             ? juce::String(candidate->category)
                                                             : juce::String("Factory Presets");
                                                         return candidateCategory == category;
                                                     });
            presetBox.addSectionHeading(category + " (" + juce::String(static_cast<int>(categoryCount)) + ")");
            currentFactoryCategory = category;
        }

        presetBox.addItem(juce::String(preset.name), static_cast<int>(i + 1u));
    }

    if (! displayedUserPresets.empty())
    {
        presetBox.addSectionHeading("User Presets (" + juce::String(static_cast<int>(displayedUserPresets.size())) + ")");
        for (size_t i = 0; i < displayedUserPresets.size(); ++i)
            presetBox.addItem(displayedUserPresets[i].name, userPresetItemIdBase + static_cast<int>(i));
    }

    const auto presetCount = static_cast<int>(displayedPresets.size() + displayedUserPresets.size());
    if (presetCount == 0)
    {
        headerControlLabels[0].setText("Preset", juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected("Init Patch");
        presetBox.setTooltip("Use Init Patch for a neutral chip-local starting point. No factory or user presets are active for this chip mode yet.");
    }
    else
    {
        headerControlLabels[0].setText(juce::String("Preset (") + juce::String(presetCount) + ")",
                                       juce::dontSendNotification);
        presetBox.setSelectedId(0, juce::dontSendNotification);
        const auto firstName = ! displayedPresets.empty()
            ? juce::String(displayedPresets.front()->name)
            : displayedUserPresets.front().name;
        presetBox.setTextWhenNothingSelected(juce::String(presetCount) + " presets - " + firstName);
        presetBox.setTooltip("Browse " + juce::String(presetCount) + " factory/user presets for "
                             + juce::String(chipper::toString(mode))
                             + ". Factory sounds are grouped by musical category; choosing one applies an audible chip-specific sound immediately.");
    }
}

void ChipperAudioProcessorEditor::reloadUserPresetFiles(chipper::ChipMode mode)
{
    displayedUserPresets.clear();

    const auto addPresetFileIfMatchesMode = [this, mode](const juce::File& file)
    {
        if (file == juce::File {} || ! file.existsAsFile())
            return;

        const std::unique_ptr<juce::XmlElement> root(juce::XmlDocument::parse(file));
        if (root == nullptr)
            return;

        const auto presetMode = chipModeFromUserPresetXml(*root);
        if (! presetMode.has_value() || *presetMode != mode)
            return;

        const auto alreadyListed = std::any_of(displayedUserPresets.begin(),
                                               displayedUserPresets.end(),
                                               [&file](const UserPresetFile& preset)
                                               {
                                                   return preset.file == file;
                                               });
        if (! alreadyListed)
            displayedUserPresets.push_back({ file, userPresetNameFromXml(*root, file) });
    };

    const auto directory = defaultUserPresetDirectory();
    if (directory.isDirectory())
    {
        juce::Array<juce::File> files;
        directory.findChildFiles(files, juce::File::findFiles, false, "*.chipperpreset;*.xml");
        files.sort();

        for (const auto& file : files)
            addPresetFileIfMatchesMode(file);
    }

    addPresetFileIfMatchesMode(currentUserPresetFile);

    std::sort(displayedUserPresets.begin(),
              displayedUserPresets.end(),
              [](const UserPresetFile& left, const UserPresetFile& right)
              {
                  return left.name.compareIgnoreCase(right.name) < 0;
              });
}

void ChipperAudioProcessorEditor::updateSegmentedControlSpecs(chipper::ChipMode mode)
{
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);

    const auto applyChoices = [](auto& buttons, const chipper::ChipParameterSpec* spec)
    {
        if (spec == nullptr)
            return;

        for (size_t i = 0; i < buttons.size(); ++i)
        {
            if (i >= spec->choices.size())
                continue;

            buttons[i].setButtonText(visibleChoiceLabel(spec->choices[i].label));
            buttons[i].setTooltip(withMidiCcForRole(choiceTooltip(*spec, i), spec->role));
        }
    };

    applyChoices(pulseDutyButtons, chipper::parameterSpecFor(mode, chipper::ChipParameterRole::macroControl1));
    applyChoices(pulse2DutyButtons, chipper::parameterSpecFor(mode, chipper::ChipParameterRole::pulse2Duty));
    applyChoices(toneNoiseMixButtons, chipper::parameterSpecFor(mode, chipper::ChipParameterRole::macroControl4));

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::pulse2Duty))
    {
        pulse2DutyLabel.setText(spec->label, juce::dontSendNotification);
        pulse2DutyLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        pulse2DutyValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::waveShape))
    {
        waveShapeLabel.setText(mode == chipper::ChipMode::sid ? "Voice Waves" : spec->label, juce::dontSendNotification);
        waveShapeLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        waveShapeValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(waveShapeButtons, spec);
        if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151)
        {
            fmAlgorithmBox.clear(juce::dontSendNotification);
            for (size_t i = 0; i < spec->choices.size(); ++i)
                fmAlgorithmBox.addItem(juce::String(spec->choices[i].label), static_cast<int>(i) + 1);
        }
        if (mode == chipper::ChipMode::opl3)
        {
            oplWaveformBox.clear(juce::dontSendNotification);
            for (size_t i = 0; i < spec->choices.size(); ++i)
                oplWaveformBox.addItem(juce::String(spec->choices[i].label), static_cast<int>(i) + 1);
        }
        if (mode == chipper::ChipMode::ym2413)
        {
            opllInstrumentBox.clear(juce::dontSendNotification);
            for (size_t i = 0; i < spec->choices.size(); ++i)
                opllInstrumentBox.addItem(juce::String(spec->choices[i].label), static_cast<int>(i) + 1);
        }
        sidVoiceWaveLabels[0].setText(mode == chipper::ChipMode::sid ? "Wave" : "V1", juce::dontSendNotification);
        sidVoiceWaveLabels[0].setTooltip(withMidiCcForRole(spec->help, spec->role));
        sidVoiceWaveBoxes[0].setTooltip(withMidiCcForRole(spec->help, spec->role));
    }

    for (size_t i = 1; i < sidVoiceWaveBoxes.size(); ++i)
    {
        if (const auto* spec = chipper::parameterSpecFor(mode, sidVoiceWaveRole(i)))
        {
            sidVoiceWaveLabels[i].setText(mode == chipper::ChipMode::sid
                                              ? juce::String("Wave")
                                              : juce::String("V") + juce::String(static_cast<int>(i + 1u)),
                                          juce::dontSendNotification);
            sidVoiceWaveLabels[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
            sidVoiceWaveBoxes[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
        }
    }

    for (size_t i = 0; i < sidVoicePulseWidthSliders.size(); ++i)
    {
        if (const auto* spec = chipper::parameterSpecFor(mode, sidVoicePulseWidthRole(i)))
        {
            sidVoicePulseWidthLabels[i].setText("PW", juce::dontSendNotification);
            sidVoicePulseWidthLabels[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
            sidVoicePulseWidthSliders[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
            sidVoicePulseWidthValueLabels[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
        }
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::dmgWaveLevel))
    {
        dmgWaveLevelLabel.setText(spec->label, juce::dontSendNotification);
        dmgWaveLevelLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        dmgWaveLevelValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(dmgWaveLevelButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::dmgStereoRoute))
    {
        dmgStereoRouteLabel.setText(spec->label, juce::dontSendNotification);
        dmgStereoRouteLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        dmgStereoRouteValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(dmgStereoRouteButtons, spec);
        dmgStereoRouteBox.clear(juce::dontSendNotification);
        for (size_t i = 0; i < spec->choices.size(); ++i)
            dmgStereoRouteBox.addItem(juce::String(spec->choices[i].label), static_cast<int>(i) + 1);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::ymEnvelopeShape))
    {
        ymEnvelopeShapeLabel.setText(spec->label, juce::dontSendNotification);
        ymEnvelopeShapeLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        ymEnvelopeShapeValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        sidFilterModeBox.clear(juce::dontSendNotification);
        for (size_t i = 0; i < spec->choices.size(); ++i)
            sidFilterModeBox.addItem(visibleChoiceLabel(spec->choices[i].label), static_cast<int>(i + 1u));
        sidFilterModeBox.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(ymEnvelopeShapeButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::sidFilterRouting))
    {
        sidFilterRoutingLabel.setText(spec->label, juce::dontSendNotification);
        sidFilterRoutingLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        sidFilterRoutingBox.setTooltip(withMidiCcForRole(spec->help, spec->role));
        sidFilterRoutingValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
    }

    ymChannelMixLabel.setText("Channel Mix", juce::dontSendNotification);
    ymChannelMixLabel.setTooltip("Per-channel AY mixer overrides. Preset uses the global Tone/Noise Mix control.");
    ymChannelMixValueLabel.setTooltip("Resolved YM/AY channel mixer choices.");
    for (size_t i = 0; i < ymChannelMixBoxes.size(); ++i)
    {
        if (const auto* spec = chipper::parameterSpecFor(mode, ymChannelMixRole(i)))
        {
            ymChannelMixLabels[i].setText(spec->label, juce::dontSendNotification);
            ymChannelMixLabels[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
            ymChannelMixBoxes[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
        }
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::snNoiseMode))
    {
        snNoiseModeLabel.setText(spec->label, juce::dontSendNotification);
        snNoiseModeLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        snNoiseModeValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(snNoiseModeButtons, spec);
        if (mode == chipper::ChipMode::nes && spec->choices.size() >= 3)
        {
            static constexpr std::array<const char*, 3> nesNoiseLabels { "Auto", "Long", "Short" };
            for (size_t i = 0; i < nesNoiseLabels.size(); ++i)
            {
                snNoiseModeButtons[i].setButtonText(nesNoiseLabels[i]);
                snNoiseModeButtons[i].setTooltip(withMidiCcForRole(choiceTooltip(*spec, i), spec->role));
            }
        }
        snNoiseModeBox.clear(juce::dontSendNotification);
        for (size_t i = 0; i < spec->choices.size(); ++i)
            snNoiseModeBox.addItem(juce::String(spec->choices[i].label), static_cast<int>(i) + 1);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::envelopeDecay))
    {
        envelopeDecayLabel.setText(spec->label, juce::dontSendNotification);
        envelopeDecayLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        envelopeDecaySlider.setTooltip(withMidiCcForRole(spec->help, spec->role));
        envelopeDecayValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
    }

    for (size_t i = 0; i < sidAdsrBoxes.size(); ++i)
    {
        if (const auto* spec = chipper::parameterSpecFor(mode, sidAdsrRole(i)))
        {
            sidAdsrLabels[i].setText(sidAdsrFieldLabel(i % sidAdsrFieldCount), juce::dontSendNotification);
            sidAdsrLabels[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
            sidAdsrBoxes[i].setTooltip(withMidiCcForRole(spec->help, spec->role));
        }
    }
}

void ChipperAudioProcessorEditor::chooseUserPresetToLoad()
{
    userPresetChooser = std::make_unique<juce::FileChooser>("Load Chipper preset",
                                                            defaultUserPresetDirectory(),
                                                            "*.chipperpreset;*.xml");
    userPresetChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                   [this](const juce::FileChooser& chooser)
                                   {
                                       const auto file = chooser.getResult();
                                       if (file == juce::File {})
                                           return;

                                       loadUserPresetFile(file);
                                   });
}

void ChipperAudioProcessorEditor::chooseUserPresetToSave()
{
    juce::String presetName;
    const auto selectedId = presetBox.getSelectedId();
    const auto selectedFactoryPreset = selectedId - 1;
    const auto selectedUserPreset = selectedId - userPresetItemIdBase;

    if (selectedFactoryPreset >= 0 && static_cast<size_t>(selectedFactoryPreset) < displayedPresets.size())
        presetName = displayedPresets[static_cast<size_t>(selectedFactoryPreset)]->name;
    else if (selectedUserPreset >= 0 && static_cast<size_t>(selectedUserPreset) < displayedUserPresets.size())
        presetName = displayedUserPresets[static_cast<size_t>(selectedUserPreset)].name;
    else if (currentUserPresetFile != juce::File {})
        presetName = currentUserPresetFile.getFileNameWithoutExtension();
    else
        presetName = juce::String(chipper::toString(displayedMode)) + " User Preset";

    const auto suggestedFile = defaultUserPresetDirectory()
                                   .getChildFile(safePresetFileStem(presetName))
                                   .withFileExtension(".chipperpreset");
    userPresetChooser = std::make_unique<juce::FileChooser>("Save Chipper preset",
                                                            suggestedFile,
                                                            "*.chipperpreset");
    userPresetChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                       | juce::FileBrowserComponent::canSelectFiles
                                       | juce::FileBrowserComponent::warnAboutOverwriting,
                                   [this](const juce::FileChooser& chooser)
                                   {
                                       auto file = chooser.getResult();
                                       if (file == juce::File {})
                                           return;

                                       if (! file.hasFileExtension(".chipperpreset"))
                                           file = file.withFileExtension(".chipperpreset");

                                       saveUserPresetFile(file);
                                   });
}

void ChipperAudioProcessorEditor::chooseUserPresetToSaveAs()
{
    chooseUserPresetToSave();
}

void ChipperAudioProcessorEditor::loadUserPresetFile(const juce::File& file)
{
    const std::unique_ptr<juce::XmlElement> root(juce::XmlDocument::parse(file));
    if (root == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Preset not loaded",
                                               "That file is not a readable Chipper preset.");
        return;
    }

    const auto stateTag = audioProcessor.getValueTreeState().state.getType().toString();
    const auto* stateXml = root->hasTagName(stateTag) ? root.get() : root->getChildByName(stateTag);
    if (stateXml == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Preset not loaded",
                                               "That file does not contain Chipper plugin state.");
        return;
    }

    const auto result = audioProcessor.restoreStateXml(*stateXml, file.getParentDirectory());
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Preset not loaded",
                                               result.getErrorMessage());
        return;
    }

    descriptorTextInitialized = false;
    displayedDmcSampleCount = -1;
    displayedDmcSampleRevision = std::numeric_limits<uint64_t>::max();
    updateDescriptorText();
    currentUserPresetFile = file;
    userPresetSaveButton.setTooltip("Save changes back to " + file.getFileName()
                                    + "\nUse Load to import another .chipperpreset file.");
    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    updatePresetChoices(chipper::parameters::chipModeFromChoice(modeChoice));
    {
        const juce::ScopedValueSetter<bool> suppress(suppressPresetApply, true);
        for (size_t i = 0; i < displayedUserPresets.size(); ++i)
        {
            if (displayedUserPresets[i].file == file)
            {
                presetBox.setSelectedId(userPresetItemIdBase + static_cast<int>(i), juce::dontSendNotification);
                break;
            }
        }
    }
    updateLiveControlReadouts();
    macroSummaryLabel.setText("Loaded user preset: " + file.getFileNameWithoutExtension(), juce::dontSendNotification);
    macroSummaryLabel.setTooltip("Loaded from " + file.getFullPathName());
}

void ChipperAudioProcessorEditor::saveUserPresetFile(const juce::File& file)
{
    auto stateXml = audioProcessor.createStateXml();
    if (stateXml == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Preset not saved",
                                               "Chipper could not create plugin state for this preset.");
        return;
    }

    const auto presetDirectory = file.getParentDirectory();
    annotatePortablePresetSampleReferences(*stateXml, presetDirectory);

    auto presetName = file.getFileNameWithoutExtension();
    if (presetName.isEmpty())
        presetName = "Chipper Preset";

    juce::XmlElement presetXml("ChipperPreset");
    presetXml.setAttribute("formatVersion", 1);
    presetXml.setAttribute("plugin", "Chipper");
    presetXml.setAttribute("pluginVersion", chipperPluginVersionString);
    presetXml.setAttribute("name", presetName);
    presetXml.setAttribute("chip", chipper::toString(displayedMode));
    presetXml.addChildElement(stateXml.release());

    const auto directoryResult = file.getParentDirectory().createDirectory();
    if (directoryResult.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Preset not saved",
                                               directoryResult.getErrorMessage());
        return;
    }

    if (! file.replaceWithText(presetXml.toString(), true, false))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Preset not saved",
                                               "Chipper could not write the preset file.");
        return;
    }

    currentUserPresetFile = file;
    userPresetSaveButton.setTooltip("Save changes back to " + file.getFileName()
                                    + "\nUser presets are portable .chipperpreset files.");
    reloadUserPresetFiles(displayedMode);
    updatePresetChoices(displayedMode);
    {
        const juce::ScopedValueSetter<bool> suppress(suppressPresetApply, true);
        for (size_t i = 0; i < displayedUserPresets.size(); ++i)
        {
            if (displayedUserPresets[i].file == file)
            {
                presetBox.setSelectedId(userPresetItemIdBase + static_cast<int>(i), juce::dontSendNotification);
                break;
            }
        }
    }
    macroSummaryLabel.setText("Saved user preset: " + file.getFileNameWithoutExtension(), juce::dontSendNotification);
    macroSummaryLabel.setTooltip("Saved to " + file.getFullPathName()
                                 + "\nExternal samples are referenced, not embedded. Put shared samples beside the preset or in a Samples folder.");
}

void ChipperAudioProcessorEditor::storeChipSettingsSnapshot(chipper::ChipMode mode)
{
    const auto index = chipModeChoiceIndex(mode);
    if (index < 0 || static_cast<size_t>(index) >= chipSettingsSnapshots.size())
        return;

    auto& snapshot = chipSettingsSnapshots[static_cast<size_t>(index)];
    snapshot.parameterValues.clear();
    snapshot.parameterValues.reserve(chipSettingsSnapshotParameterIds.size());
    for (const auto* parameterId : chipSettingsSnapshotParameterIds)
        snapshot.parameterValues.push_back(parameterValue(parameterId));

    snapshot.presetSelectedId = presetBox.getSelectedId();
    snapshot.userPresetFile = currentUserPresetFile;
    snapshot.valid = true;
}

bool ChipperAudioProcessorEditor::restoreChipSettingsSnapshot(chipper::ChipMode mode)
{
    const auto index = chipModeChoiceIndex(mode);
    if (index < 0 || static_cast<size_t>(index) >= chipSettingsSnapshots.size())
        return false;

    const auto& snapshot = chipSettingsSnapshots[static_cast<size_t>(index)];
    if (! snapshot.valid || snapshot.parameterValues.size() != chipSettingsSnapshotParameterIds.size())
        return false;

    const juce::ScopedValueSetter<bool> suppressManual(suppressManualChoiceCallbacks, true);
    const juce::ScopedValueSetter<bool> suppressPreset(suppressPresetApply, true);
    for (size_t i = 0; i < chipSettingsSnapshotParameterIds.size(); ++i)
        setPlainParameterValueFromUi(chipSettingsSnapshotParameterIds[i], snapshot.parameterValues[i]);

    presetBox.setSelectedId(snapshot.presetSelectedId, juce::dontSendNotification);
    currentUserPresetFile = snapshot.userPresetFile;
    userPresetSaveButton.setTooltip(currentUserPresetFile != juce::File {}
                                        ? "Save changes back to " + currentUserPresetFile.getFileName()
                                        : "Save the current sound as a shareable .chipperpreset file. Loaded user presets overwrite their source file; new sounds ask for a file name.");
    return true;
}

void ChipperAudioProcessorEditor::applySelectedMacroTemplate()
{
    if (suppressMacroTemplateApply)
        return;

    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    if (! chipper::descriptorFor(mode).implemented)
        return;

    const auto selected = macroBox.getSelectedItemIndex();
    if (selected < 0)
        return;

    const auto order = chipper::macroOrder();
    const auto macroIndex = static_cast<size_t>(std::clamp(selected, 0, static_cast<int>(order.size() - 1u)));
    const auto& templ = chipper::macroTemplateFor(mode, order[macroIndex]);
    const std::array<const char*, 4> ids {
        chipper::parameters::id::macroControl1,
        chipper::parameters::id::macroControl2,
        chipper::parameters::id::macroControl3,
        chipper::parameters::id::macroControl4
    };
    const std::array<const char*, sourceChannelCount> sourceIds {
        chipper::parameters::id::source1Enabled,
        chipper::parameters::id::source2Enabled,
        chipper::parameters::id::source3Enabled,
        chipper::parameters::id::source4Enabled,
        chipper::parameters::id::source5Enabled,
        chipper::parameters::id::source6Enabled,
        chipper::parameters::id::source7Enabled,
        chipper::parameters::id::source8Enabled,
        chipper::parameters::id::source9Enabled
    };
    const std::array<const char*, sourceChannelCount> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level,
        chipper::parameters::id::source5Level,
        chipper::parameters::id::source6Level,
        chipper::parameters::id::source7Level,
        chipper::parameters::id::source8Level,
        chipper::parameters::id::source9Level
    };

    const juce::ScopedValueSetter<bool> suppress(suppressMacroTemplateApply, true);
    for (size_t i = 0; i < ids.size(); ++i)
        setParameterValueFromUi(ids[i], templ.controls[i]);
    for (size_t i = 0; i < sourceIds.size(); ++i)
    {
        const auto anyTemplateSourceEnabled = std::any_of(templ.sourceEnabled.begin(), templ.sourceEnabled.end(), [](bool value) { return value; });
        const auto enabled = i < templ.sourceEnabled.size()
            ? templ.sourceEnabled[i]
            : (anyTemplateSourceEnabled && chipper::nativeSourceCountForMode(mode) > i);
        setParameterValueFromUi(sourceIds[i], enabled ? 1.0f : 0.0f);
    }
    for (size_t i = 0; i < sourceLevelIds.size(); ++i)
        setParameterValueFromUi(sourceLevelIds[i], 1.0f);
    setParameterValueFromUi(chipper::parameters::id::envelopeDecay, templ.envelopeDecay);
    for (size_t i = 0; i < sidAdsrOverrideCount; ++i)
        setChoiceParameterFromUi(sidAdsrParameterId(i), 0);
    setParameterValueFromUi(chipper::parameters::id::stereoSpread, templ.stereoSpread);
    setChoiceParameterFromUi(chipper::parameters::id::waveShape, templ.waveShape);
    setChoiceParameterFromUi(chipper::parameters::id::sidVoice2WaveShape, 0);
    setChoiceParameterFromUi(chipper::parameters::id::sidVoice3WaveShape, 0);
    setChoiceParameterFromUi(chipper::parameters::id::sidFilterRouting, 0);
    setParameterValueFromUi(chipper::parameters::id::sidVoice2PulseWidth, templ.controls[0]);
    setParameterValueFromUi(chipper::parameters::id::sidVoice3PulseWidth, templ.controls[0]);
    setChoiceParameterFromUi(chipper::parameters::id::pulse2Duty, 0);
    setChoiceParameterFromUi(chipper::parameters::id::dmgWaveLevel, 0);
    setChoiceParameterFromUi(chipper::parameters::id::dmgStereoRoute, templ.dmgStereoRoute);
    setChoiceParameterFromUi(chipper::parameters::id::ymEnvelopeShape, templ.ymEnvelopeShape);
    setChoiceParameterFromUi(chipper::parameters::id::ymChannelAMix, 0);
    setChoiceParameterFromUi(chipper::parameters::id::ymChannelBMix, 0);
    setChoiceParameterFromUi(chipper::parameters::id::ymChannelCMix, 0);
    setChoiceParameterFromUi(chipper::parameters::id::snNoiseMode, templ.snNoiseMode);
    setParameterValueFromUi(chipper::parameters::id::nesDmcDirectLevel, templ.nesDmcDirectLevel);
    if (mode == chipper::ChipMode::spc700)
    {
        const auto patch = chipper::makePatchConfig(mode,
                                                    templ.macro,
                                                    templ.controls[0],
                                                    templ.controls[1],
                                                    templ.controls[2],
                                                    templ.controls[3],
                                                    chipper::parameters::playModeFromChoice(static_cast<int>(std::round(parameterValue(chipper::parameters::id::playMode)))),
                                                    { true, true, true, true, true, true, true, true, true },
                                                    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
                                                    templ.stereoSpread,
                                                    templ.envelopeDecay,
                                                    templ.waveShape,
                                                    0,
                                                    0,
                                                    templ.dmgStereoRoute,
                                                    templ.ymEnvelopeShape,
                                                    0,
                                                    0,
                                                    0,
                                                    templ.snNoiseMode);
        setChoiceParameterFromUi(chipper::parameters::id::nesDmcPlaybackMode,
                                 static_cast<int>(chipper::spc700SamplePlaybackModeForPatch(patch)));
    }
    else
    {
        setChoiceParameterFromUi(chipper::parameters::id::nesDmcPlaybackMode, 0);
    }

    const juce::ScopedValueSetter<bool> suppressPreset(suppressPresetApply, true);
    presetBox.setSelectedId(0, juce::dontSendNotification);
    const auto presetCount = static_cast<int>(displayedPresets.size() + displayedUserPresets.size());
    if (presetCount == 0)
    {
        headerControlLabels[0].setText("Preset", juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected("Init Patch");
        presetBox.setTooltip("Use Init Patch for a neutral chip-local starting point. No factory or user presets are active for this chip mode yet.");
    }
    else
    {
        const auto firstName = ! displayedPresets.empty()
            ? juce::String(displayedPresets.front()->name)
            : displayedUserPresets.front().name;
        headerControlLabels[0].setText(juce::String("Preset (") + juce::String(presetCount) + ")",
                                       juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected(juce::String(presetCount) + " presets - " + firstName);
        presetBox.setTooltip("Browse " + juce::String(presetCount) + " factory/user presets for "
                             + juce::String(chipper::toString(mode))
                             + ". Factory sounds are grouped by musical category; choosing one applies an audible chip-specific sound immediately.");
    }

    updateLiveControlReadouts();
}

void ChipperAudioProcessorEditor::applySelectedPreset()
{
    if (suppressPresetApply)
        return;

    const auto selectedId = presetBox.getSelectedId();
    if (selectedId == initPresetItemId)
    {
        applyInitPreset();
        return;
    }

    if (selectedId >= userPresetItemIdBase)
    {
        const auto userIndex = selectedId - userPresetItemIdBase;
        if (userIndex >= 0 && static_cast<size_t>(userIndex) < displayedUserPresets.size())
            loadUserPresetFile(displayedUserPresets[static_cast<size_t>(userIndex)].file);
        return;
    }

    const auto selected = selectedId - 1;
    if (selected < 0 || static_cast<size_t>(selected) >= displayedPresets.size())
        return;

    applyFactoryPreset(*displayedPresets[static_cast<size_t>(selected)]);
}

void ChipperAudioProcessorEditor::applyInitPreset()
{
    auto preset = chipper::initPresetForChip(displayedMode);
    preset.accuracy = chipper::parameters::accuracyFromChoice(
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::accuracy))));
    const auto requestedPlayMode = chipper::parameters::playModeFromChoice(
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::playMode))));
    preset.playMode = chipper::supportsPlayMode(displayedMode, requestedPlayMode)
        ? requestedPlayMode
        : chipper::PlayMode::stack;

    applyFactoryPreset(preset);
    {
        const juce::ScopedValueSetter<bool> suppressPreset(suppressPresetApply, true);
        presetBox.setSelectedId(initPresetItemId, juce::dontSendNotification);
    }
    presetBox.setTooltip("Init Patch: neutral chip-local starting point for " + juce::String(chipper::toString(displayedMode)) + ".");
    macroSummaryLabel.setText("Init Patch: neutral " + juce::String(chipper::toString(displayedMode)) + " starting point",
                              juce::dontSendNotification);
    macroSummaryLabel.setTooltip("Init Patch resets the current chip's sound controls while preserving the current Strictness and supported Play Mode.");
}

void ChipperAudioProcessorEditor::applyFactoryPreset(const chipper::PresetInfo& preset)
{
    const std::array<const char*, 4> controlIds {
        chipper::parameters::id::macroControl1,
        chipper::parameters::id::macroControl2,
        chipper::parameters::id::macroControl3,
        chipper::parameters::id::macroControl4
    };
    const std::array<const char*, sourceChannelCount> sourceIds {
        chipper::parameters::id::source1Enabled,
        chipper::parameters::id::source2Enabled,
        chipper::parameters::id::source3Enabled,
        chipper::parameters::id::source4Enabled,
        chipper::parameters::id::source5Enabled,
        chipper::parameters::id::source6Enabled,
        chipper::parameters::id::source7Enabled,
        chipper::parameters::id::source8Enabled,
        chipper::parameters::id::source9Enabled
    };
    const std::array<const char*, sourceChannelCount> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level,
        chipper::parameters::id::source5Level,
        chipper::parameters::id::source6Level,
        chipper::parameters::id::source7Level,
        chipper::parameters::id::source8Level,
        chipper::parameters::id::source9Level
    };

    const juce::ScopedValueSetter<bool> suppressMacro(suppressMacroTemplateApply, true);
    const juce::ScopedValueSetter<bool> applyingPreset(applyingFactoryPreset, true);
    currentUserPresetFile = {};
    userPresetSaveButton.setTooltip("Save the current sound as a shareable .chipperpreset file. Loaded user presets overwrite their source file; new sounds ask for a file name.");
    setChoiceParameterFromUi(chipper::parameters::id::chipMode, chipModeChoiceIndex(preset.chip));
    setChoiceParameterFromUi(chipper::parameters::id::accuracy, accuracyChoiceIndex(preset.accuracy));
    setChoiceParameterFromUi(chipper::parameters::id::macro, macroChoiceIndex(preset.macro));
    setChoiceParameterFromUi(chipper::parameters::id::playMode, playModeChoiceIndex(preset.playMode));

    for (size_t i = 0; i < controlIds.size(); ++i)
        setParameterValueFromUi(controlIds[i], preset.controls[i]);

    for (size_t i = 0; i < sourceIds.size(); ++i)
    {
        const auto anyPresetSourceEnabled = std::any_of(preset.sourceEnabled.begin(), preset.sourceEnabled.end(), [](bool value) { return value; });
        const auto enabled = i < preset.sourceEnabled.size()
            ? preset.sourceEnabled[i]
            : (anyPresetSourceEnabled && chipper::nativeSourceCountForMode(preset.chip) > i);
        setParameterValueFromUi(sourceIds[i], enabled ? 1.0f : 0.0f);
    }

    const std::array<float, sourceChannelCount> sourceLevels {
        preset.source1Level,
        preset.source2Level,
        preset.source3Level,
        preset.source4Level,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };
    for (size_t i = 0; i < sourceLevelIds.size(); ++i)
        setParameterValueFromUi(sourceLevelIds[i], sourceLevels[i]);

    setParameterValueFromUi(chipper::parameters::id::envelopeDecay, preset.envelopeDecay);
    const std::array<int, sidAdsrOverrideCount> sidAdsrChoices {
        preset.sidAttack,
        preset.sidDecay,
        preset.sidSustain,
        preset.sidRelease,
        preset.sidVoice2Attack,
        preset.sidVoice2Decay,
        preset.sidVoice2Sustain,
        preset.sidVoice2Release,
        preset.sidVoice3Attack,
        preset.sidVoice3Decay,
        preset.sidVoice3Sustain,
        preset.sidVoice3Release
    };
    for (size_t i = 0; i < sidAdsrOverrideCount; ++i)
        setChoiceParameterFromUi(sidAdsrParameterId(i), sidAdsrChoices[i]);
    setParameterValueFromUi(chipper::parameters::id::stereoSpread, preset.stereoSpread);
    setChoiceParameterFromUi(chipper::parameters::id::waveShape, preset.waveShape);
    setChoiceParameterFromUi(chipper::parameters::id::sidVoice2WaveShape, preset.sidVoice2WaveShape);
    setChoiceParameterFromUi(chipper::parameters::id::sidVoice3WaveShape, preset.sidVoice3WaveShape);
    setChoiceParameterFromUi(chipper::parameters::id::sidFilterRouting, preset.sidFilterRouting);
    setParameterValueFromUi(chipper::parameters::id::sidVoice2PulseWidth, preset.controls[0]);
    setParameterValueFromUi(chipper::parameters::id::sidVoice3PulseWidth, preset.controls[0]);
    setChoiceParameterFromUi(chipper::parameters::id::pulse2Duty, 0);
    setChoiceParameterFromUi(chipper::parameters::id::dmgWaveLevel, preset.dmgWaveLevel);
    setChoiceParameterFromUi(chipper::parameters::id::dmgStereoRoute, preset.dmgStereoRoute);
    setChoiceParameterFromUi(chipper::parameters::id::ymEnvelopeShape, preset.ymEnvelopeShape);
    setChoiceParameterFromUi(chipper::parameters::id::ymChannelAMix, 0);
    setChoiceParameterFromUi(chipper::parameters::id::ymChannelBMix, 0);
    setChoiceParameterFromUi(chipper::parameters::id::ymChannelCMix, 0);
    setChoiceParameterFromUi(chipper::parameters::id::snNoiseMode, preset.snNoiseMode);
    setParameterValueFromUi(chipper::parameters::id::nesDmcDirectLevel, preset.nesDmcDirectLevel);
    if (preset.chip == chipper::ChipMode::spc700)
        setChoiceParameterFromUi(chipper::parameters::id::nesDmcPlaybackMode,
                                 static_cast<int>(chipper::spc700SamplePlaybackModeForPatch(chipper::patchConfigForPreset(preset))));
    else
        setChoiceParameterFromUi(chipper::parameters::id::nesDmcPlaybackMode, 0);
    setParameterValueFromUi(chipper::parameters::id::spc700LoopStart, 0.0f);
    setParameterValueFromUi(chipper::parameters::id::spc700LoopEnd, 1.0f);
    for (size_t i = 0; i < 8u; ++i)
        setPlainParameterValueFromUi(spc700VoiceSampleParameterId(i), 0.0f);
    setPlainParameterValueFromUi(chipper::parameters::id::clockHz, 0.0f);
    setPlainParameterValueFromUi(chipper::parameters::id::outputDb, preset.outputDb);

    updateDescriptorText();

    {
        const juce::ScopedValueSetter<bool> suppressPreset(suppressPresetApply, true);
        for (size_t i = 0; i < displayedPresets.size(); ++i)
        {
            if (displayedPresets[i] != nullptr && displayedPresets[i]->id == preset.id)
            {
                presetBox.setSelectedId(static_cast<int>(i) + 1, juce::dontSendNotification);
                break;
            }
        }
    }

    presetBox.setTooltip(juce::String(preset.name) + ": " + juce::String(preset.note));
    updateLiveControlReadouts();
}

chipper::PatchConfig ChipperAudioProcessorEditor::currentUiPatch(chipper::ChipMode mode,
                                                                 float control1,
                                                                 float control2,
                                                                 float control3,
                                                                 float control4,
                                                                 int waveShape,
                                                                 int dmgWaveLevel,
                                                                 int dmgStereoRoute,
                                                                 int ymEnvelopeShape,
                                                                 int snNoiseMode,
                                                                 float stereoSpread) const
{
    const auto macroChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::macro)));
    const auto playModeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::playMode)));
    const auto samplePlaybackMode = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcPlaybackMode)));
    const auto resolvedDmgStereoRoute =
        mode == chipper::ChipMode::spc700 && samplePlaybackMode == 2 && dmgStereoRoute == 0
            ? 2
            : dmgStereoRoute;

    return chipper::makePatchConfig(
        mode,
        chipper::parameters::macroFromChoice(macroChoice),
        control1,
        control2,
        control3,
        control4,
        chipper::parameters::playModeFromChoice(playModeChoice),
        {
            parameterValue(chipper::parameters::id::source1Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source2Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source3Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source4Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source5Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source6Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source7Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source8Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source9Enabled) >= 0.5f
        },
        {
            parameterValue(chipper::parameters::id::source1Level),
            parameterValue(chipper::parameters::id::source2Level),
            parameterValue(chipper::parameters::id::source3Level),
            parameterValue(chipper::parameters::id::source4Level),
            parameterValue(chipper::parameters::id::source5Level),
            parameterValue(chipper::parameters::id::source6Level),
            parameterValue(chipper::parameters::id::source7Level),
            parameterValue(chipper::parameters::id::source8Level),
            parameterValue(chipper::parameters::id::source9Level)
        },
        stereoSpread,
        parameterValue(chipper::parameters::id::envelopeDecay),
        waveShape,
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::pulse2Duty))),
        dmgWaveLevel,
        resolvedDmgStereoRoute,
        ymEnvelopeShape,
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymChannelAMix))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymChannelBMix))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymChannelCMix))),
        snNoiseMode,
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice2WaveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice3WaveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidAttack))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidDecay))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidSustain))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidRelease))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice2Attack))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice2Decay))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice2Sustain))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice2Release))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice3Attack))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice3Decay))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice3Sustain))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidVoice3Release))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidFilterRouting))),
        parameterValue(chipper::parameters::id::sidVoice2PulseWidth),
        parameterValue(chipper::parameters::id::sidVoice3PulseWidth),
        parameterValue(chipper::parameters::id::nesDmcDirectLevel),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcRateIndex))),
        parameterValue(chipper::parameters::id::nesDmcLoop) >= 0.5f,
        mode == chipper::ChipMode::nes
            && samplePlaybackMode == 2,
        parameterValue(chipper::parameters::id::spc700LoopStart),
        parameterValue(chipper::parameters::id::spc700LoopEnd),
        {
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::spc700Voice1SampleSlot))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::spc700Voice2SampleSlot))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::spc700Voice3SampleSlot))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::spc700Voice4SampleSlot))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::spc700Voice5SampleSlot))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::spc700Voice6SampleSlot))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::spc700Voice7SampleSlot))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::spc700Voice8SampleSlot)))
        });
}

bool ChipperAudioProcessorEditor::usesPulseDutySegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::macroControl1,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesPulse2DutySegment(chipper::ChipMode mode) const
{
    if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
        return false;

    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::pulse2Duty,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesWaveShapeSegment(chipper::ChipMode mode) const
{
    if (chipper::chipHasParameterSurface(mode,
                                         chipper::ChipParameterRole::waveShape,
                                         chipper::ControlSurface::segmentedChoice))
        return true;

    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::waveShape,
                                            chipper::ControlSurface::menu);
}

bool ChipperAudioProcessorEditor::usesDmgWaveLevelSegment(chipper::ChipMode mode) const
{
    if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
        return false;

    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::dmgWaveLevel,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesDmgStereoRouteSegment(chipper::ChipMode mode) const
{
    if (mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
        return false;

    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::dmgStereoRoute,
                                            chipper::ControlSurface::segmentedChoice)
        || chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::dmgStereoRoute,
                                            chipper::ControlSurface::menu);
}

bool ChipperAudioProcessorEditor::usesYmEnvelopeShapeSegment(chipper::ChipMode mode) const
{
    if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
        return false;

    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::ymEnvelopeShape,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesYmChannelMixControls(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::ymChannelAMix,
                                            chipper::ControlSurface::menu);
}

bool ChipperAudioProcessorEditor::usesSnNoiseModeSegment(chipper::ChipMode mode) const
{
    if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
        return false;

    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::snNoiseMode,
                                            chipper::ControlSurface::segmentedChoice)
        || chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::snNoiseMode,
                                            chipper::ControlSurface::menu);
}

bool ChipperAudioProcessorEditor::usesToneNoiseMixSegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::macroControl4,
                                            chipper::ControlSurface::segmentedChoice);
}

juce::String ChipperAudioProcessorEditor::macroTemplateReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    const auto& templ = chipper::macroTemplateFor(mode, patch.macro);
    const auto label = templ.label.empty() ? juce::String(chipper::toString(patch.macro)) : juce::String(templ.label);
    const auto laneText = sourceLaneExposureReadout(mode);

    if (! chipper::descriptorFor(mode).implemented)
        return label + ": " + juce::String(templ.help) + " Audio core not integrated yet.";

    if (mode == chipper::ChipMode::nes)
        return label + " -> P1 " + pulseDutyReadout(mode, patch.control1) + " | P2 " + pulse2DutyReadout(patch) + " | " + nesNoiseModeReadout(patch) + " | " + nesFocusReadout(patch.control4) + laneText;

    if (mode == chipper::ChipMode::dmg)
        return label + " -> P1 " + pulseDutyReadout(mode, patch.control1) + " | P2 " + pulse2DutyReadout(patch) + " | " + dmgWaveLevelReadout(patch) + " | " + dmgStereoRouteReadout(patch) + laneText;

    if (mode == chipper::ChipMode::ym2149)
        return label + " -> " + ymSpreadReadout(patch.control1) + " | " + ymNoiseReadout(patch.control3) + " | " + ymChannelMixReadout(patch) + laneText;

    if (mode == chipper::ChipMode::sn76489)
        return label + " -> " + snStackReadout(patch.control1) + " | " + snNoiseModeReadout(patch) + " | " + snLevelReadout(patch.control4) + laneText;

    if (mode == chipper::ChipMode::sid)
        return label + " -> " + sidModelReadout(patch) + " | " + sidVoiceWaveSummary(patch) + " | " + sidFilterModeReadout(patch) + " | " + sidModModeReadout(patch) + laneText;

    if (mode == chipper::ChipMode::pokey)
        return label + " -> " + pokeyRegisterReadout(patch) + " | " + pokeyAudctlFilterReadout(patch) + " | " + waveShapeReadout(mode, patch.waveShape) + laneText;

    if (mode == chipper::ChipMode::spc700)
        return label + " -> " + sampleChipReadout(mode, patch) + " | " + spc700PitchMotionReadout(patch) + " | " + spc700NoiseReadout(patch) + laneText;

    if (mode == chipper::ChipMode::paula)
        return label + " -> " + sampleChipReadout(mode, patch) + " | " + paulaOutputFilterReadout(patch) + " | " + waveShapeReadout(mode, patch.waveShape) + laneText;

    if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
        return label + " -> " + wavetableChipReadout(mode, patch) + " | " + waveShapeReadout(mode, patch.waveShape) + laneText;

    if (mode == chipper::ChipMode::ym2612)
        return label + " -> " + fmChipReadout(mode, patch) + " | " + ym2612DacModeReadout(patch) + " | " + waveShapeReadout(mode, patch.waveShape) + laneText;

    if (mode == chipper::ChipMode::ym2151)
        return label + " -> " + fmChipReadout(mode, patch) + " | " + ym2151NoiseReadout(patch) + " | " + waveShapeReadout(mode, patch.waveShape) + laneText;

    if (mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2413)
        return label + " -> " + fmChipReadout(mode, patch) + " | " + waveShapeReadout(mode, patch.waveShape) + laneText;

    return label + ": " + juce::String(templ.help) + laneText;
}

juce::String ChipperAudioProcessorEditor::performanceMacroDestination(chipper::ChipMode mode, size_t index) const
{
    switch (mode)
    {
        case chipper::ChipMode::nes:
        {
            static constexpr std::array<const char*, 4> labels { "P1 duty bits", "Sweep/timer", "$400E noise", "Mixer focus" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::dmg:
        {
            static constexpr std::array<const char*, 4> labels { "NR11 duty", "NR10 sweep", "NR43 noise", "NRx2 envelope" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::ym2149:
        {
            static constexpr std::array<const char*, 4> labels { "A/B/C tune", "Tone period", "Reg 6 noise", "Reg 7 mixer" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::sn76489:
        {
            static constexpr std::array<const char*, 4> labels { "Tone stack", "Tone period", "Reg E noise", "Noise atten." };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::sid:
        {
            static constexpr std::array<const char*, 4> labels { "PW regs", "Voice tune", "$D415 cutoff", "$D40x SR" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::pokey:
        {
            static constexpr std::array<const char*, 4> labels { "AUDC path", "AUDF/AUDCTL", "Poly timer", "AUDV gate" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::spc700:
        {
            static constexpr std::array<const char*, 4> labels { "Sample shape", "Pitch/PMON", "Echo regs", "GAIN/ADSR" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::paula:
        {
            static constexpr std::array<const char*, 4> labels { "Sample shape", "PER motion", "Filter/color", "VOL gate" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::huc6280:
        {
            static constexpr std::array<const char*, 4> labels { "Voice spread", "Freq regs", "Wave RAM", "5-bit volume" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::namcoWsg:
        {
            static constexpr std::array<const char*, 4> labels { "Lane spread", "Freq regs", "WSG RAM", "4-bit volume" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::scc:
        {
            static constexpr std::array<const char*, 4> labels { "Ch spread", "Freq regs", "SCC RAM", "$AA volume" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::ym2612:
        {
            static constexpr std::array<const char*, 4> labels { "Algorithm", "$B0 feedback", "Ch6 DAC", "FM level" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::opl3:
        {
            static constexpr std::array<const char*, 4> labels { "OPL wave", "$C0 feedback", "Operator tone", "FM level" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::ym2151:
        {
            static constexpr std::array<const char*, 4> labels { "Algorithm", "$20 feedback", "$0F noise", "FM level" };
            return labels[std::min(index, labels.size() - 1u)];
        }

        case chipper::ChipMode::ym2413:
        {
            static constexpr std::array<const char*, 4> labels { "ROM instr.", "Pitch spread", "Motion", "Volume nibble" };
            return labels[std::min(index, labels.size() - 1u)];
        }
    }

    return {};
}

juce::String ChipperAudioProcessorEditor::performanceMacroReadout(chipper::ChipMode mode, size_t index, juce::String readout) const
{
    const auto destination = performanceMacroDestination(mode, index);
    return destination.isEmpty() ? readout : destination + ": " + readout;
}

juce::String ChipperAudioProcessorEditor::sourceLaneExposureReadout(chipper::ChipMode mode) const
{
    if (! chipper::hasInternalSourceLanes(mode))
        return {};

    const auto visible = static_cast<int>(chipper::visibleSourceCountForMode(mode));
    const auto native = static_cast<int>(chipper::nativeSourceCountForMode(mode));
    if (mode == chipper::ChipMode::huc6280)
        return " | 6/6 Chip Poly voices playable; " + juce::String(visible) + " direct trims shown";

    return " | lanes " + juce::String(visible) + "/" + juce::String(native) + " shown; extras in stack presets";
}

juce::String ChipperAudioProcessorEditor::pulseDutyReadout(chipper::ChipMode mode, float value) const
{
    static constexpr std::array<const char*, 4> nesDutyLabels { "12.5% narrow pulse", "25% hollow pulse", "50% square", "75% inverted pulse" };
    static constexpr std::array<const char*, 4> dmgDutyLabels { "12.5% thin pulse", "25% narrow pulse", "50% square", "75% wide pulse" };
    const auto index = static_cast<size_t>(std::clamp(static_cast<int>(std::round(value * 3.0f)), 0, 3));
    return mode == chipper::ChipMode::dmg ? dmgDutyLabels[index] : nesDutyLabels[index];
}

juce::String ChipperAudioProcessorEditor::pulse2DutyReadout(const chipper::PatchConfig& patch) const
{
    static constexpr std::array<const char*, 4> dutyLabels { "12.5%", "25%", "50%", "75%" };
    static constexpr std::array<const char*, 4> dutyBits { "00", "01", "10", "11" };
    const auto choice = std::clamp(patch.pulse2Duty, 0, 4);
    const auto primary = std::clamp(static_cast<int>(std::round(patch.control1 * 3.0f)), 0, 3);
    const auto resolved = choice > 0
                              ? choice - 1
                              : (patch.playMode == chipper::PlayMode::chipPoly ? primary : std::min(primary + 1, 3));
    const auto detail = juce::String(displayedMode == chipper::ChipMode::dmg ? "NR21 bits " : "bits ")
        + dutyBits[static_cast<size_t>(resolved)] + ", " + dutyLabels[static_cast<size_t>(resolved)];

    if (choice != 0)
        return detail;

    if (displayedMode == chipper::ChipMode::dmg)
    {
        const auto followMode = patch.playMode == chipper::PlayMode::chipPoly
            ? juce::String("Preset P1 -> ")
            : juce::String("Preset +1 -> ");
        return followMode + detail;
    }

    return juce::String("Preset -> ") + detail;
}

juce::String ChipperAudioProcessorEditor::compactPulse2DutyReadout(const chipper::PatchConfig& patch) const
{
    static constexpr std::array<const char*, 4> dutyLabels { "12.5%", "25%", "50%", "75%" };
    const auto choice = std::clamp(patch.pulse2Duty, 0, 4);
    const auto primary = std::clamp(static_cast<int>(std::round(patch.control1 * 3.0f)), 0, 3);
    const auto resolved = choice > 0
                              ? choice - 1
                              : (patch.playMode == chipper::PlayMode::chipPoly ? primary : std::min(primary + 1, 3));
    const auto resolvedText = juce::String(dutyLabels[static_cast<size_t>(resolved)]);

    if (choice > 0)
        return resolvedText + " independent";

    return juce::String("Follow -> ") + resolvedText;
}

juce::String ChipperAudioProcessorEditor::waveShapeReadout(chipper::ChipMode mode, int choice) const
{
    if (mode == chipper::ChipMode::sid)
    {
        switch (std::clamp(choice, 0, 4))
        {
            case 1: return "CTRL waveform bit 0x10, triangle";
            case 2: return "CTRL waveform bit 0x20, saw";
            case 3: return "CTRL waveform bit 0x40, pulse";
            case 4: return "CTRL waveform bit 0x80, noise";
            case 0:
            default:
                return "Preset waveform: resolves to SID CTRL bits";
        }
    }

    if (mode == chipper::ChipMode::pokey)
    {
        switch (std::clamp(choice, 0, 4))
        {
            case 1: return "AUDC pure tone distortion path";
            case 2: return "AUDC Poly4 buzzy counter path";
            case 3: return "AUDC Poly5 noise/tone path";
            case 4: return "AUDC Poly17 noise path";
            case 0:
            default:
                return "Preset POKEY distortion";
        }
    }

    if (mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula)
    {
        if (mode == chipper::ChipMode::paula)
        {
            switch (std::clamp(choice, 0, 4))
            {
                case 1: return "Ramp 8-bit sample";
                case 2: return "Triangle 8-bit sample";
                case 3: return "Sine 8-bit sample";
                case 4: return "Noise burst 8-bit sample";
                case 0:
                default:
                    return "Preset Paula sample shape";
            }
        }

        switch (std::clamp(choice, 0, 4))
        {
            case 1: return "Triangle sample shape";
            case 2: return "Saw sample shape";
            case 3: return "Pulse sample shape";
            case 4: return "Stepped/noise sample shape";
            case 0:
            default:
                return "Preset generated sample shape";
        }
    }

    if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2413)
    {
        switch (std::clamp(choice, 0, 4))
        {
            case 1: return "Sine/triangle-like FM operator shape";
            case 2: return "Bright FM operator shape";
            case 3: return "Complex feedback/operator shape";
            case 4: return "Stepped FM color";
            case 0:
            default:
                return "Preset FM operator registers";
        }
    }

    switch (std::clamp(choice, 0, 4))
    {
        case 1: return "Writes 32-sample triangle into Wave RAM";
        case 2: return "Writes 32-sample saw ramp into Wave RAM";
        case 3: return "Writes 50% pulse into Wave RAM";
        case 4: return "Writes stepped 4-level table into Wave RAM";
        case 0:
        default:
            return "RAM: preserve current/register-trace Wave RAM";
    }
}

int ChipperAudioProcessorEditor::pokeyCardMidiNote(const chipper::PatchConfig& patch, size_t index) const
{
    static constexpr std::array<int, sourceChannelCount> chipPolyNotes { 60, 64, 67, 72, 76, 79, 83, 86, 88 };
    const auto baseNote = patch.playMode == chipper::PlayMode::chipPoly
                              ? chipPolyNotes[std::min(index, chipPolyNotes.size() - 1u)]
                              : 60 + static_cast<int>(std::min(index, sourceChannelCount - 1u)) * 5;
    return std::clamp(baseNote, 0, 127);
}

juce::String ChipperAudioProcessorEditor::pokeySourceRegisterReadout(const chipper::PatchConfig& patch, size_t index) const
{
    const auto audc = chipper::pokeyAudcForPatch(patch);
    const auto audf = chipper::pokeyAudfForNote(chipper::parameters::defaultClockForMode(chipper::ChipMode::pokey),
                                                pokeyCardMidiNote(patch, index));
    const auto audv = static_cast<int>(audc & 0x0fu);
    const auto audctl = chipper::pokeyAudctlForPatch(patch);
    const auto pairedHigh = (index == 1u && (audctl & 0x10u) != 0) || (index == 3u && (audctl & 0x08u) != 0);
    const auto pairText = pairedHigh ? juce::String(" | high byte") : juce::String();
    return "AUDC $" + byteHex(audc) + " | AUDF $" + byteHex(audf) + " | AUDV " + juce::String(audv) + "/15" + pairText;
}

juce::String ChipperAudioProcessorEditor::pokeySourceLevelReadout(const chipper::PatchConfig& patch, size_t index) const
{
    const auto audc = chipper::pokeyAudcForPatch(patch);
    const auto audf = chipper::pokeyAudfForNote(chipper::parameters::defaultClockForMode(chipper::ChipMode::pokey),
                                                pokeyCardMidiNote(patch, index));
    const auto audv = static_cast<int>(audc & 0x0fu);
    const auto audctl = chipper::pokeyAudctlForPatch(patch);
    const auto pairedHigh = (index == 1u && (audctl & 0x10u) != 0) || (index == 3u && (audctl & 0x08u) != 0);
    const auto pairText = pairedHigh ? juce::String(" | high byte") : juce::String();
    return "AUDF $" + byteHex(audf) + " | AUDV " + juce::String(audv) + "/15" + pairText;
}

juce::String ChipperAudioProcessorEditor::pokeyRegisterReadout(const chipper::PatchConfig& patch) const
{
    const auto audc = chipper::pokeyAudcForPatch(patch);
    const auto audctl = chipper::pokeyAudctlForPatch(patch);
    const auto audfC4 = chipper::pokeyAudfForNote(chipper::parameters::defaultClockForMode(chipper::ChipMode::pokey),
                                                  pokeyCardMidiNote(patch, 0));
    return "AUDC $" + byteHex(audc) + " | AUDV " + juce::String(static_cast<int>(audc & 0x0fu)) + "/15 | Ch1 AUDF $" + byteHex(audfC4) + " | AUDCTL $" + byteHex(audctl);
}

juce::String ChipperAudioProcessorEditor::pokeyAudctlReadout(const chipper::PatchConfig& patch) const
{
    const auto audctl = chipper::pokeyAudctlForPatch(patch);
    juce::String mode;
    switch (audctl & 0x18u)
    {
        case 0x10u: mode = "Ch 1+2 linked, 16-bit pitch"; break;
        case 0x08u: mode = "Ch 3+4 linked, 16-bit pitch"; break;
        case 0x18u: mode = "Both pairs linked, two 16-bit lanes"; break;
        case 0x00u:
        default:
            mode = "Four independent 8-bit channels";
            break;
    }

    const auto resolved = "AUDCTL $" + byteHex(audctl) + ", " + mode;
    return patch.dmgStereoRoute == 0 ? juce::String("Preset -> ") + resolved : resolved;
}

juce::String ChipperAudioProcessorEditor::pokeyAudctlFilterReadout(const chipper::PatchConfig& patch) const
{
    const auto audctl = chipper::pokeyAudctlForPatch(patch);
    juce::String mode;
    switch (audctl & 0x06u)
    {
        case 0x04u: mode = "Ch1 filtered by Ch3"; break;
        case 0x02u: mode = "Ch2 filtered by Ch4"; break;
        case 0x06u: mode = "Ch1<-Ch3 + Ch2<-Ch4"; break;
        case 0x00u:
        default:
            mode = "filter off";
            break;
    }

    const auto resolved = "AUDCTL $" + byteHex(audctl) + ", " + mode;
    return patch.ymEnvelopeShape == 0 ? juce::String("Preset -> ") + resolved : resolved;
}

juce::String ChipperAudioProcessorEditor::sampleChipReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    const auto chipLabel = mode == chipper::ChipMode::paula ? juce::String("8-bit hard-pan period sample") : juce::String("lo-fi sample voice");
    const auto decay = static_cast<int>(std::round(std::clamp(patch.envelopeDecay, 0.0f, 1.0f) * 15.0f));
    const auto volume = static_cast<int>(std::round(std::clamp(patch.control4, 0.0f, 1.0f) * 15.0f));
    auto text = chipLabel
        + " | shape " + juce::String(static_cast<int>(chipper::sampleTemplateForPatch(mode, patch)))
        + " | decay " + juce::String(decay) + "/15 | volume " + juce::String(volume) + "/15";
    if (mode == chipper::ChipMode::spc700)
        text += " | " + spc700EnvelopeReadout(patch);
    return text;
}

juce::String ChipperAudioProcessorEditor::spc700PitchMotionReadout(const chipper::PatchConfig& patch) const
{
    const auto centered = static_cast<double>(patch.control2) - 0.5;
    const auto direction = std::abs(centered) <= 0.08 ? 0 : (centered > 0.0 ? 1 : -1);
    if (direction == 0)
        return "Pitch centered, PMON off";

    const auto magnitude = std::max(0.0, std::abs(centered) - 0.08) / 0.42;
    const auto macroScale = patch.macro == chipper::MacroKind::laser || patch.macro == chipper::MacroKind::powerUp || patch.macro == chipper::MacroKind::jump
        ? 2400.0
        : 900.0;
    const auto motionCents = std::clamp(magnitude, 0.0, 1.0) * macroScale;
    const auto pmonCents = std::clamp(motionCents * 0.35, 0.0, 480.0);
    const auto directionText = direction > 0 ? juce::String("rise") : juce::String("fall");
    return "Pitch " + directionText + " "
        + juce::String(static_cast<int>(std::round(motionCents))) + "c, PMON "
        + juce::String(static_cast<int>(std::round(pmonCents))) + "c";
}

juce::String ChipperAudioProcessorEditor::spc700EnvelopeReadout(const chipper::PatchConfig& patch) const
{
    const auto shape = chipper::spc700EnvelopeShapeForPatch(patch);
    juce::String shapeText;
    switch (shape)
    {
        case 1: shapeText = "Pluck"; break;
        case 3: shapeText = "Pad"; break;
        case 4: shapeText = "Perc"; break;
        case 2:
        default: shapeText = "Lead"; break;
    }

    const auto prefix = patch.ymEnvelopeShape == 0 ? juce::String("Preset -> ") : juce::String();
    return prefix + shapeText + " envelope, ADSR $" + byteHex(chipper::spc700AdsrForPatch(patch));
}

juce::String ChipperAudioProcessorEditor::spc700EchoReadout(const chipper::PatchConfig& patch) const
{
    const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
    if (color <= 0.01)
        return "Echo off";

    const auto send = std::clamp(0.08 + (color * 0.42), 0.0, 0.50);
    const auto feedback = std::clamp(0.16 + (color * 0.42), 0.0, 0.58);
    const auto delayMs = 32.0 + std::round(color * 13.0) * 16.0;
    return juce::String("Echo send ") + juce::String(static_cast<int>(std::round(send * 100.0))) + "%"
        + ", fb " + juce::String(static_cast<int>(std::round(feedback * 100.0))) + "%"
        + ", " + juce::String(static_cast<int>(std::round(delayMs))) + " ms";
}

juce::String ChipperAudioProcessorEditor::spc700NoiseReadout(const chipper::PatchConfig& patch) const
{
    const auto mode = chipper::spc700NoiseModeForPatch(patch);
    juce::String resolved;
    switch (mode)
    {
        case 2: resolved = "NON noise low, clock " + juce::String(static_cast<int>(chipper::spc700NoiseClockForPatch(patch))); break;
        case 3: resolved = "NON noise mid, clock " + juce::String(static_cast<int>(chipper::spc700NoiseClockForPatch(patch))); break;
        case 4: resolved = "NON noise high, clock " + juce::String(static_cast<int>(chipper::spc700NoiseClockForPatch(patch))); break;
        case 1:
        default:
            resolved = "NON off, sample playback";
            break;
    }

    return patch.snNoiseMode == 0 ? juce::String("Preset -> ") + resolved : resolved;
}

juce::String ChipperAudioProcessorEditor::paulaOutputFilterReadout(const chipper::PatchConfig& patch) const
{
    const auto mode = chipper::paulaOutputFilterModeForPatch(patch);
    juce::String resolved;
    switch (mode)
    {
        case 1: resolved = "Raw DAC, nearest playback"; break;
        case 2: resolved = "A500 output softening"; break;
        case 3: resolved = "LED low-pass color"; break;
        case 4: resolved = "LED + A500 low-pass"; break;
        case 0:
        default:
            resolved = "Raw DAC";
            break;
    }

    return patch.snNoiseMode == 0 ? juce::String("Preset -> ") + resolved : resolved;
}

static juce::String paulaHardwarePanLabel(size_t index)
{
    return (index == 0u || index == 3u) ? juce::String("L") : juce::String("R");
}

static juce::String sampleMappedModeLabel(chipper::ChipMode mode, int playbackMode)
{
    if (playbackMode <= 0)
        return "Manual";

    if (mode == chipper::ChipMode::spc700)
        return playbackMode == 1 ? "Note" : "Drum";

    if (mode == chipper::ChipMode::paula)
        return playbackMode == 1 ? "Key" : "Track";

    return "Map";
}

static int previewSampleSlotForVoice(const ChipperAudioProcessor::Spc700BrrSampleInfo& info, size_t voiceIndex)
{
    if (info.bankCount <= 0)
        return -1;

    if (info.playbackMode == 0)
        return std::clamp(info.selectedSlot, 0, info.bankCount - 1);

    return static_cast<int>(voiceIndex % static_cast<size_t>(info.bankCount));
}

static int resolvedSpc700SampleSlotForVoice(const chipper::PatchConfig& patch,
                                            const ChipperAudioProcessor::Spc700BrrSampleInfo& info,
                                            size_t voiceIndex)
{
    if (info.bankCount <= 0)
        return -1;

    const auto pinnedSlotChoice = voiceIndex < patch.spc700VoiceSampleSlots.size()
        ? std::clamp(patch.spc700VoiceSampleSlots[voiceIndex], 0, 32)
        : 0;
    if (pinnedSlotChoice > 0)
        return std::clamp(pinnedSlotChoice - 1, 0, info.bankCount - 1);

    return previewSampleSlotForVoice(info, voiceIndex);
}

static bool spc700VoiceUsesPinnedSample(const chipper::PatchConfig& patch, size_t voiceIndex)
{
    return voiceIndex < patch.spc700VoiceSampleSlots.size()
        && std::clamp(patch.spc700VoiceSampleSlots[voiceIndex], 0, 32) > 0;
}

static juce::String ym2149ResolvedChannelMixLabel(const chipper::PatchConfig& patch, size_t index)
{
    if (index >= 3u)
        return "Noise p" + juce::String(static_cast<int>(chipper::ym2149NoisePeriodForControl(patch.control3))).paddedLeft('0', 2);

    const auto macroMixer = chipper::ym2149MixerRegisterForControl(patch.control4);
    const auto mixer = chipper::ym2149MixerRegisterWithChannelOverrides(patch, macroMixer);
    const auto toneEnabled = (mixer & static_cast<uint8_t>(1u << index)) == 0u;
    const auto noiseEnabled = (mixer & static_cast<uint8_t>(1u << (index + 3u))) == 0u;

    if (toneEnabled && noiseEnabled)
        return "Both";
    if (toneEnabled)
        return "Tone";
    if (noiseEnabled)
        return "Noise";
    return "Off";
}

juce::String ChipperAudioProcessorEditor::sampleSourceCardLabel(chipper::ChipMode mode,
                                                                const chipper::PatchConfig& patch,
                                                                size_t index) const
{
    const auto number = juce::String(static_cast<int>(index + 1u));
    const auto templateId = static_cast<int>(mode == chipper::ChipMode::paula
        ? chipper::wavetableWaveShapeForChannel(mode, patch, index)
        : chipper::sampleTemplateForPatch(mode, patch));
    const auto sample0 = static_cast<int>(chipper::generatedSampleValueForPatch(mode, patch, index, 0));
    const auto sample32 = static_cast<int>(chipper::generatedSampleValueForPatch(mode, patch, index, 32));

    if (mode == chipper::ChipMode::spc700)
    {
        const auto noiseMode = chipper::spc700NoiseModeForPatch(patch);
        if (noiseMode > 1u)
            return "V" + number
                + " | Noise " + juce::String(static_cast<int>(chipper::spc700NoiseClockForPatch(patch)));

        const auto info = audioProcessor.spc700BrrSampleInfo();
        if (info.loaded)
        {
            const auto names = audioProcessor.spc700BrrSampleNames();
            const auto slot = resolvedSpc700SampleSlotForVoice(patch, info, index);
            const auto pinned = spc700VoiceUsesPinnedSample(patch, index);
            auto slotText = info.bankCount > 1
                ? (pinned ? juce::String("Pinned") : sampleMappedModeLabel(mode, info.playbackMode))
                    + " S" + juce::String(slot + 1).paddedLeft('0', 2)
                : juce::String("Sample");
            if (slot >= 0 && slot < names.size())
                slotText += " " + compactSampleName(names[slot], 12);
            return "V" + number + " | " + slotText;
        }

        return "V" + number
            + " | Shape " + juce::String(templateId)
            + " " + juce::String(sample0) + "/" + juce::String(sample32);
    }

    if (mode == chipper::ChipMode::paula)
    {
        const auto info = audioProcessor.paulaSampleInfo();
        if (info.loaded)
        {
            const auto names = audioProcessor.paulaSampleNames();
            const auto slot = previewSampleSlotForVoice(info, index);
            auto slotText = info.bankCount > 1
                ? sampleMappedModeLabel(mode, info.playbackMode) + " S" + juce::String(slot + 1).paddedLeft('0', 2)
                : juce::String("Sample");
            if (slot >= 0 && slot < names.size())
                slotText += " " + compactSampleName(names[slot], 12);
            return "Ch " + number + " " + paulaHardwarePanLabel(index) + " | " + slotText;
        }

        return "Ch " + number
            + " " + paulaHardwarePanLabel(index)
            + " | Shape " + juce::String(templateId)
            + " " + juce::String(sample0) + "/" + juce::String(sample32);
    }

    return {};
}

juce::String ChipperAudioProcessorEditor::sampleSourceRegisterReadout(chipper::ChipMode mode,
                                                                      const chipper::PatchConfig& patch,
                                                                      size_t index) const
{
    const auto channel = static_cast<int>(index + 1u);
    const auto templateId = static_cast<int>(mode == chipper::ChipMode::paula
        ? chipper::wavetableWaveShapeForChannel(mode, patch, index)
        : chipper::sampleTemplateForPatch(mode, patch));
    const auto sample0 = static_cast<int>(chipper::generatedSampleValueForPatch(mode, patch, index, 0));
    const auto sample32 = static_cast<int>(chipper::generatedSampleValueForPatch(mode, patch, index, 32));

    if (mode == chipper::ChipMode::spc700)
    {
        const auto volume = chipper::spc700VoiceVolumeForPatch(patch, index);
        const auto adsr = chipper::spc700AdsrForPatch(patch);
        const auto gain = chipper::spc700GainForPatch(patch);
        const auto enabled = chipper::spc700VoiceEnabledForPatch(patch, index);
        const auto info = audioProcessor.spc700BrrSampleInfo();
        const auto base = static_cast<uint8_t>(std::min(index, size_t { 7u }) * 0x10u);
        const auto noiseMode = chipper::spc700NoiseModeForPatch(patch);
        auto readout = "SPC700 voice " + juce::String(channel)
            + " | regs $" + byteHex(base) + "-$" + byteHex(static_cast<uint8_t>(base + 6u))
            + " | vol " + juce::String(static_cast<int>(volume)) + "/127"
            + " | ADSR $" + byteHex(adsr)
            + " | GAIN $" + byteHex(gain)
            + " | " + (enabled ? juce::String("key on") : juce::String("muted"))
            + " | ";
        if (noiseMode > 1u)
        {
            readout += "NON bit voice " + juce::String(channel)
                + " | noise clock " + juce::String(static_cast<int>(chipper::spc700NoiseClockForPatch(patch)));
        }
        else if (info.loaded)
        {
            if (info.bankCount <= 0 || info.selectedSlot < 0)
            {
                readout += info.statusLine;
                return readout;
            }

            const auto names = audioProcessor.spc700BrrSampleNames();
            const auto slot = resolvedSpc700SampleSlotForVoice(patch, info, index);
            const auto pinned = spc700VoiceUsesPinnedSample(patch, index);
            const auto sampleName = slot >= 0 && slot < names.size() ? names[slot] : info.sampleName;
            readout += "sample slot " + juce::String(slot + 1) + "/" + juce::String(info.bankCount)
                + " " + sampleName
                + " | " + (pinned ? juce::String("pinned voice slot") : sampleMappedModeLabel(mode, info.playbackMode).toLowerCase());
            if (! pinned && info.playbackMode != 0)
            {
                readout += " preview; triggered notes map "
                    + chipper::parameters::midiNoteChoices()[info.mapRootNote]
                    + "-"
                    + chipper::parameters::midiNoteChoices()[info.mapHighNote];
            }
        }
        else
        {
            readout += "shape " + juce::String(templateId)
                + " | sample[0/32] " + juce::String(sample0) + "/" + juce::String(sample32);
        }
        return readout;
    }

    if (mode == chipper::ChipMode::paula)
    {
        const auto volume = chipper::paulaChannelVolumeForPatch(patch, index);
        const auto control = chipper::paulaControlForPatch(patch, index);
        const auto base = static_cast<uint8_t>(std::min(index, size_t { 3u }) * 0x10u);
        auto readout = "Paula ch " + juce::String(channel)
            + " " + paulaHardwarePanLabel(index)
            + " | regs $" + byteHex(base) + "-$" + byteHex(static_cast<uint8_t>(base + 4u))
            + " | vol " + juce::String(static_cast<int>(volume)) + "/64"
            + " | ctrl $" + byteHex(control)
            + " | " + ((control & 0x02u) != 0 ? juce::String("loop") : juce::String("one-shot"));

        const auto info = audioProcessor.paulaSampleInfo();
        if (info.loaded)
        {
            if (info.bankCount <= 0 || info.selectedSlot < 0)
            {
                readout += " | " + info.statusLine;
                return readout;
            }

            const auto names = audioProcessor.paulaSampleNames();
            const auto slot = previewSampleSlotForVoice(info, index);
            const auto sampleName = slot >= 0 && slot < names.size() ? names[slot] : info.sampleName;
            readout += " | sample slot " + juce::String(slot + 1) + "/" + juce::String(info.bankCount)
                + " " + sampleName
                + " | " + sampleMappedModeLabel(mode, info.playbackMode).toLowerCase();
            if (info.playbackMode != 0)
                readout += " preview; triggered notes map "
                    + chipper::parameters::midiNoteChoices()[info.mapRootNote]
                    + "-"
                    + chipper::parameters::midiNoteChoices()[info.mapHighNote];
        }
        else
        {
            readout += " | shape " + juce::String(templateId)
                + " | sample[0/32] " + juce::String(sample0) + "/" + juce::String(sample32);
        }

        return readout;
    }

    return {};
}

juce::String ChipperAudioProcessorEditor::wavetableChipReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    const auto volume = static_cast<int>(std::round(std::clamp(patch.control4, 0.0f, 1.0f) * 15.0f));
    const auto skew = static_cast<int>(std::round(std::clamp(patch.control3, 0.0f, 1.0f) * 31.0f));
    juce::String memory = "Wave RAM";
    if (mode == chipper::ChipMode::namcoWsg)
        memory = "4-bit WSG RAM";
    else if (mode == chipper::ChipMode::scc)
        memory = "32-byte SCC RAM";
    else if (mode == chipper::ChipMode::huc6280)
        memory = "32-sample 5-bit RAM";

    return memory + " | skew " + juce::String(skew) + "/31 | volume " + juce::String(volume) + "/15";
}

juce::String ChipperAudioProcessorEditor::wavetableSourceCardLabel(chipper::ChipMode mode,
                                                                   const chipper::PatchConfig& patch,
                                                                   size_t index) const
{
    const auto number = juce::String(static_cast<int>(index + 1u));
    const auto wave0 = chipper::wavetableRamSampleForPatch(mode, patch, index, 0);
    const auto wave31 = chipper::wavetableRamSampleForPatch(mode, patch, index, 31);
    juce::String shape;
    const auto shapeChoice = mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc
                                 ? static_cast<int>(chipper::wavetableWaveShapeForChannel(mode, patch, index))
                                 : std::clamp(patch.waveShape, 0, 4);
    switch (shapeChoice)
    {
        case 1: shape = "R"; break;
        case 2: shape = "Tri"; break;
        case 3: shape = "P"; break;
        case 4: shape = "St"; break;
        case 0:
        default:
            shape = "S";
            break;
    }

    if (mode == chipper::ChipMode::huc6280)
        return "Wave " + number
            + " | " + shape
            + " " + juce::String(static_cast<int>(wave0))
            + "/" + juce::String(static_cast<int>(wave31));

    if (mode == chipper::ChipMode::namcoWsg)
        return "Lane " + number
            + " | " + shape
            + " $" + byteHex(wave0)
            + "/$" + byteHex(wave31);

    if (mode == chipper::ChipMode::scc)
        return "Ch " + number
            + " | " + shape
            + " $" + byteHex(wave0)
            + "/$" + byteHex(wave31);

    return {};
}

juce::String ChipperAudioProcessorEditor::wavetableSourceRegisterReadout(chipper::ChipMode mode,
                                                                         const chipper::PatchConfig& patch,
                                                                         size_t index) const
{
    const auto channel = static_cast<int>(index + 1u);
    const auto wave0 = chipper::wavetableRamSampleForPatch(mode, patch, index, 0);
    const auto wave31 = chipper::wavetableRamSampleForPatch(mode, patch, index, 31);

    if (mode == chipper::ChipMode::huc6280)
    {
        const auto control = chipper::huc6280ControlForPatch(patch, index);
        const auto shapeChoice = static_cast<int>(chipper::huc6280WaveShapeForChannel(patch, index));
        return "HuC6280 Ch " + juce::String(channel)
            + " | $00 select " + juce::String(static_cast<int>(index))
            + " | $04 ctrl $" + byteHex(control)
            + " vol " + juce::String(static_cast<int>(control & 0x1fu)) + "/31"
            + " | wave " + juce::String(shapeChoice)
            + " | RAM[0/31] " + juce::String(static_cast<int>(wave0)) + "/" + juce::String(static_cast<int>(wave31));
    }

    if (mode == chipper::ChipMode::namcoWsg)
    {
        const auto base = static_cast<uint8_t>(0x80u + std::min(index, size_t { 7u }) * 4u);
        const auto volume = chipper::namcoWsgVolumeForPatch(patch, index);
        const auto enabled = chipper::namcoWsgChannelEnabledForPatch(patch, index);
        const auto shapeChoice = static_cast<int>(chipper::wavetableWaveShapeForChannel(mode, patch, index));
        return "Namco lane " + juce::String(channel)
            + " | regs $" + byteHex(base) + "-$" + byteHex(static_cast<uint8_t>(base + 3u))
            + " | vol " + juce::String(static_cast<int>(volume)) + "/15"
            + " | wave " + juce::String(shapeChoice)
            + " | " + (enabled ? juce::String("enabled") : juce::String("muted"))
            + " | RAM[0/31] $" + byteHex(wave0) + "/$" + byteHex(wave31);
    }

    if (mode == chipper::ChipMode::scc)
    {
        const auto volume = chipper::sccVolumeForPatch(patch, index);
        const auto keyBit = chipper::sccChannelKeyOnForPatch(patch, index);
        const auto shapeChoice = static_cast<int>(chipper::wavetableWaveShapeForChannel(mode, patch, index));
        return "SCC Ch " + juce::String(channel)
            + " | freq regs $" + byteHex(static_cast<uint8_t>(0xa0u + std::min(index, size_t { 4u }) * 2u))
            + "/$" + byteHex(static_cast<uint8_t>(0xa1u + std::min(index, size_t { 4u }) * 2u))
            + " | $" + byteHex(static_cast<uint8_t>(0xaau + std::min(index, size_t { 4u }))) + " vol " + juce::String(static_cast<int>(volume)) + "/15"
            + " | wave " + juce::String(shapeChoice)
            + " | $AF bit " + juce::String(static_cast<int>(index)) + (keyBit ? " on" : " off")
            + " | RAM[0/31] $" + byteHex(wave0) + "/$" + byteHex(wave31);
    }

    return {};
}

juce::String ChipperAudioProcessorEditor::fmChipReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    const auto feedback = static_cast<int>(chipper::fmFeedbackForPatch(patch));
    const auto level = static_cast<int>(std::round(std::clamp(patch.control4, 0.0f, 1.0f) * 15.0f));

    if (mode == chipper::ChipMode::ym2413)
    {
        const auto instrument = static_cast<int>(chipper::ym2413InstrumentForPatch(patch));
        return "OPLL instrument " + juce::String(instrument) + " | volume " + juce::String(level) + "/15";
    }

    if (mode == chipper::ChipMode::opl3)
    {
        const auto waveform = static_cast<int>(chipper::oplWaveformForPatch(patch));
        const auto connection = chipper::oplConnectionForPatch(patch) != 0 ? juce::String("parallel") : juce::String("serial");
        const auto rhythm = chipper::oplRhythmModeForPatch(patch) == 2u ? juce::String(" | $BD rhythm") : juce::String(" | melodic");
        return "OPL wave " + juce::String(waveform)
            + " | " + connection
            + " | feedback " + juce::String(feedback) + "/7"
            + " | level " + juce::String(level) + "/15"
            + rhythm;
    }

    const auto algorithm = static_cast<int>(mode == chipper::ChipMode::ym2151
                                                ? chipper::ym2151AlgorithmForPatch(patch)
                                                : chipper::ym2612AlgorithmForPatch(patch));
    return "Algorithm " + juce::String(algorithm) + " | feedback " + juce::String(feedback) + "/7 | level " + juce::String(level) + "/15";
}

juce::String ChipperAudioProcessorEditor::fmFeedbackReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    const auto feedback = static_cast<int>(chipper::fmFeedbackForPatch(patch));

    if (mode == chipper::ChipMode::opl3)
    {
        const auto connection = static_cast<int>(chipper::oplConnectionForPatch(patch));
        const auto registerValue = static_cast<uint8_t>((static_cast<unsigned>(feedback) << 1u) | static_cast<unsigned>(connection));
        return "FB " + juce::String(feedback) + "/7 | $C0=$" + byteHex(registerValue)
            + " | " + (connection != 0 ? juce::String("parallel") : juce::String("serial"));
    }

    if (mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2612)
    {
        const auto algorithm = static_cast<int>(mode == chipper::ChipMode::ym2151
                                                    ? chipper::ym2151AlgorithmForPatch(patch)
                                                    : chipper::ym2612AlgorithmForPatch(patch));
        const auto registerValue = static_cast<uint8_t>(mode == chipper::ChipMode::ym2151
                                                            ? (0xc0u | (static_cast<unsigned>(feedback) << 3u) | static_cast<unsigned>(algorithm))
                                                            : ((static_cast<unsigned>(feedback) << 3u) | static_cast<unsigned>(algorithm)));
        return "FB " + juce::String(feedback) + "/7 | "
            + (mode == chipper::ChipMode::ym2151 ? juce::String("$20=$") : juce::String("$B0=$"))
            + byteHex(registerValue)
            + " | Alg " + juce::String(algorithm);
    }

    if (mode == chipper::ChipMode::ym2413)
        return "Pitch spread " + juce::String(static_cast<int>(std::round(std::clamp(patch.control2, 0.0f, 1.0f) * 12.0f))) + " st";

    return "Feedback " + juce::String(feedback) + "/7";
}

juce::String ChipperAudioProcessorEditor::fmOperatorRegisterReadout(chipper::ChipMode mode,
                                                                    const chipper::PatchConfig& patch,
                                                                    size_t op) const
{
    if (mode == chipper::ChipMode::opl3)
    {
        const auto wave = static_cast<int>(chipper::oplWaveformForPatch(patch));
        const auto connection = chipper::oplConnectionForPatch(patch) != 0 ? juce::String("parallel") : juce::String("serial");
        const auto feedback = static_cast<int>(chipper::fmFeedbackForPatch(patch));
        const auto melodicSustain = patch.macro != chipper::MacroKind::drum
            && patch.macro != chipper::MacroKind::hit
            && patch.macro != chipper::MacroKind::coin
            && patch.macro != chipper::MacroKind::jump;
        const auto attackDecay = 0xf4u;
        const auto sustainRelease = melodicSustain ? 0x26u : 0xa6u;

        if (op == 0u)
            return "W" + juce::String(wave) + " | " + connection + " | FB " + juce::String(feedback) + "/7";

        if (op == 1u)
            return "MUL " + juce::String(static_cast<int>(chipper::oplModulatorMultipleForPatch(patch)))
                + " | TL " + juce::String(static_cast<int>(chipper::oplModulatorTotalLevelForPatch(patch)))
                + " | AD $" + byteHex(static_cast<uint8_t>(attackDecay))
                + " | SR $" + byteHex(static_cast<uint8_t>(sustainRelease));

        return "MUL 1 | TL " + juce::String(static_cast<int>(chipper::oplCarrierTotalLevelForPatch(patch)))
            + " | AD $" + byteHex(static_cast<uint8_t>(attackDecay))
            + " | SR $" + byteHex(static_cast<uint8_t>(sustainRelease));
    }

    const auto envelope = chipper::ym2612EnvelopeRegistersForPatch(patch, op);
    const auto multiple = static_cast<int>(chipper::fmOperatorMultipleForPatch(mode, patch, op));
    const auto totalLevel = static_cast<int>(chipper::fmOperatorTotalLevelForPatch(mode, patch, op));
    return "MULT " + juce::String(multiple)
        + " | TL " + juce::String(totalLevel)
        + " | AR " + juce::String(static_cast<int>(envelope.attackRate))
        + " | D1 " + juce::String(static_cast<int>(envelope.decayRate))
        + " | D2 " + juce::String(static_cast<int>(envelope.sustainRate))
        + " | SL/RR $" + byteHex(envelope.sustainRelease);
}

juce::String ChipperAudioProcessorEditor::fmOperatorRegisterTooltip(chipper::ChipMode mode, size_t op) const
{
    if (mode == chipper::ChipMode::opl3)
    {
        if (op == 0u)
            return "OPL2 pair state written to $C0: connection and feedback, plus the $E0 waveform selector.";

        return juce::String(op == 1u ? "OPL2 modulator operator. " : "OPL2 carrier operator. ")
            + "This readout follows the current preset and shows the register-backed two-operator state; full editable OPL ADSR remains planned.";
    }

    const auto family = mode == chipper::ChipMode::ym2151 ? juce::String("YM2151/OPM") : juce::String("YM2612/OPN2");
    return family + " operator " + juce::String(static_cast<int>(op + 1u))
        + " preset-resolved registers. MULT/TL control the operator ratio and level, while AR/D1/D2/SL-RR are the native envelope registers currently written into the ymfm core.";
}

juce::String ChipperAudioProcessorEditor::ym2612DacModeReadout(const chipper::PatchConfig& patch) const
{
    const auto mode = chipper::ym2612DacModeForPatch(patch);
    const auto resolved = mode == 2u
                              ? juce::String("DAC Drum: $2B=0x80, stream $2A on Ch 6")
                              : juce::String("FM Ch6: channel 6 remains four-operator FM");
    return patch.snNoiseMode == 0 ? juce::String("Preset -> ") + resolved : resolved;
}

juce::String ChipperAudioProcessorEditor::fmSourceRegisterReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index) const
{
    const auto channel = static_cast<int>(index + 1u);
    const auto feedback = static_cast<int>(chipper::fmFeedbackForPatch(patch));

    if (mode == chipper::ChipMode::ym2413)
    {
        const auto instrument = static_cast<int>(chipper::ym2413InstrumentForPatch(patch));
        const auto volumeNibble = static_cast<int>(chipper::ym2413VolumeNibbleForPatch(patch, index));
        return "OPLL Ch " + juce::String(channel)
            + " | Reg $" + byteHex(static_cast<uint8_t>(0x30u + std::min(index, size_t { 8u })))
            + " inst " + juce::String(instrument)
            + " volume nibble " + juce::String(volumeNibble) + "/15"
            + " | $20 key/block/fnum-hi, $10 fnum-lo";
    }

    if (mode == chipper::ChipMode::opl3)
    {
        if (chipper::oplRhythmModeForPatch(patch) == 2u && index >= 6u)
        {
            static constexpr std::array<const char*, 3> rhythmLabels { "BD", "HH+SD", "TOM+CYM" };
            return "OPL Rhythm " + juce::String(rhythmLabels[std::min(index - 6u, size_t { 2u })])
                + " | $BD key bits"
                + " | level via rhythm operator TL"
                + " | ch " + juce::String(channel) + " pitch regs";
        }

        const auto waveform = static_cast<int>(chipper::oplWaveformForPatch(patch));
        const auto connection = static_cast<int>(chipper::oplConnectionForPatch(patch));
        const auto modMultiple = static_cast<int>(chipper::oplModulatorMultipleForPatch(patch));
        const auto modLevel = static_cast<int>(chipper::oplModulatorTotalLevelForPatch(patch));
        const auto carrierLevel = static_cast<int>(chipper::oplCarrierTotalLevelForPatch(patch));
        return "OPL Ch " + juce::String(channel)
            + " | Reg $" + byteHex(static_cast<uint8_t>(0xc0u + std::min(index, size_t { 8u })))
            + " FB " + juce::String(feedback)
            + " CON " + juce::String(connection)
            + " | Op waveform " + juce::String(waveform)
            + " | mod mult " + juce::String(modMultiple)
            + " TL " + juce::String(modLevel)
            + " | car TL " + juce::String(carrierLevel);
    }

    const auto algorithm = static_cast<int>(mode == chipper::ChipMode::ym2151
                                                ? chipper::ym2151AlgorithmForPatch(patch)
                                                : chipper::ym2612AlgorithmForPatch(patch));
    const auto registerValue = static_cast<uint8_t>(mode == chipper::ChipMode::ym2151
                                                        ? (0xc0u | (static_cast<unsigned>(feedback) << 3u) | static_cast<unsigned>(algorithm))
                                                        : ((static_cast<unsigned>(feedback) << 3u) | static_cast<unsigned>(algorithm)));
    if (mode == chipper::ChipMode::ym2151)
    {
        const auto op1Multiple = static_cast<int>(chipper::fmOperatorMultipleForPatch(mode, patch, 0));
        const auto op4Multiple = static_cast<int>(chipper::fmOperatorMultipleForPatch(mode, patch, 3));
        const auto modLevel = static_cast<int>(chipper::fmOperatorTotalLevelForPatch(mode, patch, 0));
        const auto carrierLevel = static_cast<int>(chipper::fmOperatorTotalLevelForPatch(mode, patch, 3));
        return "OPM Ch " + juce::String(channel)
            + " | Reg $" + byteHex(static_cast<uint8_t>(0x20u + std::min(index, size_t { 7u })))
            + " = $" + byteHex(registerValue)
            + " | Alg " + juce::String(algorithm)
            + " FB " + juce::String(feedback)
            + " | M1/M4 " + juce::String(op1Multiple) + "/" + juce::String(op4Multiple)
            + " TL " + juce::String(modLevel) + "/" + juce::String(carrierLevel);
    }

    const auto ymPort = index >= 3u ? 1 : 0;
    const auto ymSlot = static_cast<uint8_t>(std::min(index % 3u, size_t { 2u }));
    if (mode == chipper::ChipMode::ym2612 && index == 5u && chipper::ym2612DacModeForPatch(patch) == 2u)
    {
        return "OPN2 Ch 6 DAC | $2B enable bit 7 | $2A 8-bit DAC stream | Pan $"
            + byteHex(chipper::ym2612PanBitsForPatch(patch, index));
    }

    const auto op1Multiple = static_cast<int>(chipper::fmOperatorMultipleForPatch(mode, patch, 0));
    const auto op4Multiple = static_cast<int>(chipper::fmOperatorMultipleForPatch(mode, patch, 3));
    const auto modLevel = static_cast<int>(chipper::fmOperatorTotalLevelForPatch(mode, patch, 0));
    const auto carrierLevel = static_cast<int>(chipper::fmOperatorTotalLevelForPatch(mode, patch, 3));
    return "OPN2 Ch " + juce::String(channel)
        + " | Port " + juce::String(static_cast<int>(ymPort))
        + " Reg $" + byteHex(static_cast<uint8_t>(0xb0u + ymSlot))
        + " = $" + byteHex(registerValue)
        + " | Alg " + juce::String(algorithm)
        + " FB " + juce::String(feedback)
        + " | M1/M4 " + juce::String(op1Multiple) + "/" + juce::String(op4Multiple)
        + " TL " + juce::String(modLevel) + "/" + juce::String(carrierLevel);
}

juce::String ChipperAudioProcessorEditor::sourceCardNativeLabel(chipper::ChipMode mode,
                                                               const chipper::PatchConfig& patch,
                                                               size_t index,
                                                               juce::String fallback) const
{
    const auto number = juce::String(static_cast<int>(index + 1u));
    if (mode == chipper::ChipMode::pokey)
        return "Ch " + number + " | AUDC $" + byteHex(chipper::pokeyAudcForPatch(patch));

    if (mode == chipper::ChipMode::ym2149)
    {
        if (index < 3u)
            return juce::String::charToString(static_cast<juce::juce_wchar>('A' + static_cast<int>(index)))
                + " | " + ym2149ResolvedChannelMixLabel(patch, index);

        return ym2149ResolvedChannelMixLabel(patch, index);
    }

    if (mode == chipper::ChipMode::spc700)
        return sampleSourceCardLabel(mode, patch, index);

    if (mode == chipper::ChipMode::paula)
        return sampleSourceCardLabel(mode, patch, index);

    if (mode == chipper::ChipMode::huc6280)
        return wavetableSourceCardLabel(mode, patch, index);

    if (mode == chipper::ChipMode::namcoWsg)
        return wavetableSourceCardLabel(mode, patch, index);

    if (mode == chipper::ChipMode::scc)
        return wavetableSourceCardLabel(mode, patch, index);

    if (mode == chipper::ChipMode::ym2413)
        return "OPLL " + number + " | I" + juce::String(static_cast<int>(chipper::ym2413InstrumentForPatch(patch)))
            + " V" + juce::String(static_cast<int>(chipper::ym2413VolumeNibbleForPatch(patch, index)));

    if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151)
    {
        if (mode == chipper::ChipMode::opl3)
            return "OPL " + number + " | W" + juce::String(static_cast<int>(chipper::oplWaveformForPatch(patch)))
                + " TL" + juce::String(static_cast<int>(chipper::oplCarrierTotalLevelForPatch(patch)));

        const auto algorithm = static_cast<int>(mode == chipper::ChipMode::ym2151
                                                    ? chipper::ym2151AlgorithmForPatch(patch)
                                                    : chipper::ym2612AlgorithmForPatch(patch));
        auto label = (mode == chipper::ChipMode::ym2151 ? "OPM " : "OPN2 ") + number
            + " | A" + juce::String(algorithm)
            + " TL" + juce::String(static_cast<int>(chipper::fmOperatorTotalLevelForPatch(mode, patch, 3)));
        if (mode == chipper::ChipMode::ym2612)
            label += " Pan $" + byteHex(chipper::ym2612PanBitsForPatch(patch, index));
        return label;
    }

    return fallback;
}

juce::String ChipperAudioProcessorEditor::dmgWaveLevelReadout(const chipper::PatchConfig& patch) const
{
    const auto bits = chipper::dmgWaveOutputLevelBitsForPatch(patch, 1.0f, false);
    const auto registerText = juce::String("NR32 bits ") + juce::String(static_cast<int>((bits >> 5u) & 0x03u));
    juce::String resolvedText;
    switch (bits)
    {
        case 0x00u: resolvedText = registerText + ", muted"; break;
        case 0x20u: resolvedText = registerText + ", 100%"; break;
        case 0x60u: resolvedText = registerText + ", 25%"; break;
        case 0x40u:
        default:
            resolvedText = registerText + ", 50%";
            break;
    }

    return patch.dmgWaveLevel == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::dmgStereoRouteReadout(const chipper::PatchConfig& patch) const
{
    if (displayedMode == chipper::ChipMode::pokey)
        return pokeyAudctlReadout(patch);

    const auto routeRegister = chipper::dmgStereoRouteRegisterForPatch(patch);
    const auto registerText = juce::String("NR51=0x") + juce::String::toHexString(static_cast<int>(routeRegister)).paddedLeft('0', 2).toUpperCase();
    juce::String routeText;
    switch (routeRegister)
    {
        case 0xffu: routeText = "all channels both sides"; break;
        case 0xf0u: routeText = "all channels left"; break;
        case 0x0fu: routeText = "all channels right"; break;
        case 0x5au: routeText = "P1+Wave left, P2+Noise right"; break;
        default:
            routeText = "custom route bits";
            break;
    }

    const auto resolvedText = registerText + ", " + routeText;
    return patch.dmgStereoRoute == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::spc700SamplePlaybackReadout(const chipper::PatchConfig& patch) const
{
    const auto mode = chipper::spc700SamplePlaybackModeForPatch(patch);
    const auto resolvedText = mode == 1u
                                  ? juce::String("Loop, sample RAM wraps while held")
                                  : juce::String("One-shot, voice stops at sample end");
    return patch.dmgStereoRoute == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::ym2151NoiseReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseRegister = chipper::ym2151NoiseRegisterForPatch(patch);
    const auto registerText = juce::String("$0F=0x") + juce::String::toHexString(static_cast<int>(noiseRegister)).paddedLeft('0', 2).toUpperCase();
    const auto enabled = (noiseRegister & 0x80u) != 0u;
    const auto frequency = static_cast<int>(noiseRegister & 0x1fu);
    const auto resolvedText = enabled
                                  ? registerText + ", Ch8 op4 noise, freq " + juce::String(frequency) + "/31"
                                  : registerText + ", normal FM Ch8";
    return patch.snNoiseMode == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::ym2612PanReadout(const chipper::PatchConfig& patch) const
{
    const auto pan0 = chipper::ym2612PanBitsForPatch(patch, 0);
    const auto pan1 = chipper::ym2612PanBitsForPatch(patch, 1);
    juce::String resolvedText;
    if (pan0 == 0xc0u && pan1 == 0xc0u)
        resolvedText = "Both outputs, $B4 L+R";
    else if (pan0 == 0x80u && pan1 == 0x80u)
        resolvedText = "Left output, $B4 L";
    else if (pan0 == 0x40u && pan1 == 0x40u)
        resolvedText = "Right output, $B4 R";
    else
        resolvedText = "Alternating channels, $B4 L/R";

    return patch.dmgStereoRoute == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::ym2151PanReadout(const chipper::PatchConfig& patch) const
{
    const auto pan0 = chipper::ym2151PanBitsForPatch(patch, 0);
    const auto pan1 = chipper::ym2151PanBitsForPatch(patch, 1);
    juce::String resolvedText;
    if (pan0 == 0xc0u && pan1 == 0xc0u)
        resolvedText = "Both outputs, $20 L+R";
    else if (pan0 == 0x80u && pan1 == 0x80u)
        resolvedText = "Left output, $20 L";
    else if (pan0 == 0x40u && pan1 == 0x40u)
        resolvedText = "Right output, $20 R";
    else
        resolvedText = "Alternating lanes, $20 L/R";

    return patch.dmgStereoRoute == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::sidModelReadout(const chipper::PatchConfig& patch) const
{
    const auto model = chipper::sidModelNumberForPatch(patch);
    const auto detail = model == 8580 ? "cleaner/brighter filter profile" : "warmer/rougher filter profile";
    const auto text = juce::String("SID ") + juce::String(model) + ", " + detail;
    return std::clamp(patch.dmgStereoRoute, 0, 2) == 0 ? juce::String("Preset -> ") + text : text;
}

juce::String ChipperAudioProcessorEditor::ymEnvelopeShapeReadout(int choice) const
{
    if (displayedMode == chipper::ChipMode::opl3)
    {
        const auto clamped = std::clamp(choice, 0, 2);
        switch (clamped)
        {
            case 1: return "Melodic, all nine OPL2 channels use two-operator voices";
            case 2: return "Rhythm, $BD enables BD/HH/SD/TOM/CYM on channels 7-9";
            case 0:
            default:
                return "Preset recipe, Drum/Hit use native OPL2 rhythm";
        }
    }

    if (displayedMode == chipper::ChipMode::ym2413)
    {
        const auto clamped = std::clamp(choice, 0, 2);
        switch (clamped)
        {
            case 1: return "Melodic, all nine OPLL channels use preset instruments";
            case 2: return "Rhythm, $0E enables BD/HH/SD/TOM/CYM on channels 7-9";
            case 0:
            default:
                return "Preset recipe, Drum/Hit use native OPLL rhythm";
        }
    }

    if (displayedMode == chipper::ChipMode::ym2612 || displayedMode == chipper::ChipMode::ym2151)
    {
        const auto clamped = std::clamp(choice, 0, 4);
        const auto chip = displayedMode == chipper::ChipMode::ym2151 ? juce::String("OPM") : juce::String("OPN2");
        switch (clamped)
        {
            case 1: return chip + " AR/DR/SR/SL+RR set for plucked keys";
            case 2: return chip + " AR/DR/SR/SL+RR set for sustained leads";
            case 3: return chip + " AR/DR/SR/SL+RR set for soft pads";
            case 4: return chip + " AR/DR/SR/SL+RR set for percussive hits";
            case 0:
            default:
                return "Preset recipe, writes " + chip + " operator envelope registers";
        }
    }

    if (displayedMode == chipper::ChipMode::pokey)
    {
        switch (std::clamp(choice, 0, 4))
        {
            case 1: return "AUDCTL high-pass filter off";
            case 2: return "AUDCTL bit 2: channel 3 filters channel 1";
            case 3: return "AUDCTL bit 1: channel 4 filters channel 2";
            case 4: return "AUDCTL bits 1+2: both high-pass paths";
            case 0:
            default:
                return "Preset recipe, writes POKEY AUDCTL filter bits";
        }
    }

    if (displayedMode == chipper::ChipMode::spc700)
    {
        switch (std::clamp(choice, 0, 4))
        {
            case 1: return "Pluck: fast attack, short decay, low sustain";
            case 2: return "Lead: quick attack with playable sustain";
            case 3: return "Pad: slower attack, high sustain, long release";
            case 4: return "Perc: immediate transient with near-zero sustain";
            case 0:
            default:
                return "Preset recipe, writes clean-room SPC700 ADSR/gain contour";
        }
    }

    const auto clamped = std::clamp(choice, 0, 20);
    if (clamped >= 5)
    {
        const auto code = clamped - 5;
        return juce::String("Register 13 = 0x")
            + juce::String::toHexString(code).toUpperCase()
            + ", exact AY/YM envelope shape";
    }

    switch (clamped)
    {
        case 1: return "Register 13 = 0x09, fall then hold low";
        case 2: return "Register 13 = 0x0D, rise then hold high";
        case 3: return "Register 13 = 0x08, repeating saw down";
        case 4: return "Register 13 = 0x0E, repeating triangle";
        case 0:
        default:
            return "Fixed volume registers; envelope bit off";
    }
}

juce::String ChipperAudioProcessorEditor::sidFilterModeReadout(const chipper::PatchConfig& patch) const
{
    const auto modeBits = chipper::sidFilterModeBitsForPatch(patch);
    juce::String resolved;
    switch (modeBits)
    {
        case 0x10u: resolved = "LP"; break;
        case 0x20u: resolved = "BP"; break;
        case 0x40u: resolved = "HP"; break;
        case 0x30u: resolved = "LP+BP"; break;
        case 0x50u: resolved = "notch LP+HP"; break;
        case 0x60u: resolved = "BP+HP"; break;
        case 0x70u: resolved = "LP+BP+HP"; break;
        case 0x00u:
        default:
            resolved = "bypass";
            break;
    }

    const auto registerText = juce::String("$D418 bits 0x")
        + juce::String::toHexString(static_cast<int>(modeBits)).paddedLeft('0', 2).toUpperCase();
    const auto text = registerText + ", " + resolved;
    return patch.ymEnvelopeShape == 0 ? juce::String("Preset -> ") + text : text;
}

juce::String ChipperAudioProcessorEditor::sidFilterRoutingReadout(const chipper::PatchConfig& patch) const
{
    const auto bits = chipper::sidFilterRoutingBitsForPatch(patch);
    const auto registerText = juce::String("$D417 low 0x")
        + juce::String::toHexString(static_cast<int>(bits & 0x07u)).toUpperCase();
    const auto text = registerText + ", " + sidFilterRoutingName(bits);
    return patch.sidFilterRouting == 0 ? juce::String("Preset -> ") + text : text;
}

juce::String ChipperAudioProcessorEditor::sidVoiceWaveSummary(const chipper::PatchConfig& patch) const
{
    juce::String text;
    for (size_t voice = 0; voice < sidVoiceWaveCount; ++voice)
    {
        const auto bits = chipper::sidWaveformControlForVoice(patch, voice);
        if (voice > 0)
            text += " | ";

        text += juce::String("V") + juce::String(static_cast<int>(voice + 1u))
            + " 0x" + byteHex(bits)
            + " " + sidWaveNameForControlBits(bits);
    }

    return text;
}

juce::String ChipperAudioProcessorEditor::sidVoicePulseWidthReadout(const chipper::PatchConfig& patch, size_t voice) const
{
    const auto pulseWidth = chipper::sidPulseWidthForVoice(patch, voice);
    const auto percent = static_cast<int>(std::round((static_cast<float>(pulseWidth) / 4095.0f) * 100.0f));
    const auto registerBase = voice == 0 ? "$D402/$D403" : (voice == 1 ? "$D409/$D40A" : "$D410/$D411");
    return juce::String(registerBase)
        + " 0x" + juce::String::toHexString(static_cast<int>(pulseWidth)).paddedLeft('0', 3).toUpperCase()
        + ", " + juce::String(percent) + "%";
}

juce::String ChipperAudioProcessorEditor::sidModModeReadout(const chipper::PatchConfig& patch) const
{
    const auto bits = chipper::sidModulationBitsForPatch(patch, 1);
    juce::String resolved;
    switch (bits)
    {
        case 0x02u: resolved = "sync on voices 2/3"; break;
        case 0x04u: resolved = "ring on voices 2/3"; break;
        case 0x06u: resolved = "sync + ring on voices 2/3"; break;
        case 0x00u:
        default:
            resolved = "off";
            break;
    }

    const auto registerText = juce::String("CTRL bits 0x")
        + juce::String::toHexString(static_cast<int>(bits)).paddedLeft('0', 2).toUpperCase();
    const auto text = registerText + ", " + resolved;
    return patch.snNoiseMode == 0 ? juce::String("Preset -> ") + text : text;
}

juce::String ChipperAudioProcessorEditor::snNoiseRegisterLabel(uint8_t noiseControl) const
{
    const auto rate = noiseControl & 0x03u;
    const auto kind = (noiseControl & 0x04u) != 0 ? "white" : "periodic";
    const auto rateText = [rate]() -> const char*
    {
        switch (rate)
        {
            case 0: return "/512";
            case 1: return "/1024";
            case 2: return "/2048";
            case 3: return "tone 3 clock";
            default: return "/512";
        }
    }();

    return juce::String(kind) + " " + rateText;
}

juce::String ChipperAudioProcessorEditor::noiseModeReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    if (mode == chipper::ChipMode::nes)
        return nesNoiseModeReadout(patch);

    if (mode == chipper::ChipMode::dmg)
        return dmgNoiseModeReadout(patch);

    if (mode == chipper::ChipMode::sid)
        return sidModModeReadout(patch);

    if (mode == chipper::ChipMode::ym2612)
        return ym2612DacModeReadout(patch);

    if (mode == chipper::ChipMode::ym2151)
        return ym2151NoiseReadout(patch);

    if (mode == chipper::ChipMode::paula)
        return paulaOutputFilterReadout(patch);

    if (mode == chipper::ChipMode::spc700)
        return spc700NoiseReadout(patch);

    return snNoiseModeReadout(patch);
}

juce::String ChipperAudioProcessorEditor::nesNoiseModeReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseRegister = chipper::nesNoiseRegisterForPatch(patch);
    const auto registerText = juce::String("$400E=0x") + juce::String::toHexString(static_cast<int>(noiseRegister)).paddedLeft('0', 2).toUpperCase();
    const auto modeText = (noiseRegister & 0x80u) != 0 ? "short-loop" : "long LFSR";
    const auto periodText = juce::String("period ") + juce::String(static_cast<int>(noiseRegister & 0x0fu));
    const auto resolvedText = registerText + ", " + modeText + ", " + periodText;
    return std::clamp(patch.snNoiseMode, 0, 2) == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::dmgNoiseModeReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseRegister = chipper::dmgNoiseRegisterForPatch(patch);
    const auto registerText = juce::String("NR43=0x") + juce::String::toHexString(static_cast<int>(noiseRegister)).paddedLeft('0', 2).toUpperCase();
    const auto widthText = (noiseRegister & 0x08u) != 0 ? "7-bit LFSR" : "15-bit LFSR";
    const auto shiftText = juce::String("shift ") + juce::String(static_cast<int>((noiseRegister >> 4u) & 0x0fu));
    const auto divisorText = juce::String("divisor code ") + juce::String(static_cast<int>(noiseRegister & 0x07u));
    const auto resolvedText = registerText + ", " + widthText + ", " + shiftText + ", " + divisorText;
    return std::clamp(patch.snNoiseMode, 0, 2) == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::snNoiseModeReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseControl = chipper::sn76489NoiseControlForPatch(patch);
    const auto registerText = juce::String("Reg E=0x") + juce::String::toHexString(static_cast<int>(noiseControl)).toUpperCase();
    const auto resolvedText = registerText + ", " + snNoiseRegisterLabel(noiseControl);
    return patch.snNoiseMode == 0 ? juce::String("Preset -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::stereoSpreadReadout(chipper::ChipMode mode, float value) const
{
    const auto spread = std::clamp(value, 0.0f, 1.0f);
    if (mode == chipper::ChipMode::sid)
    {
        const auto resonance = chipper::sidFilterResonanceForControl(spread);
        const auto registerValue = static_cast<int>((resonance << 4u) | 0x07u);
        return juce::String("$D417=0x")
             + juce::String::toHexString(registerValue).paddedLeft('0', 2).toUpperCase()
             + ", resonance " + juce::String(static_cast<int>(resonance)) + "/15";
    }

    if (spread <= 0.01f)
        return "Mono: authentic chip output width";

    const auto percent = static_cast<int>(std::round(spread * 100.0f));
    if (mode == chipper::ChipMode::ym2149)
        return juce::String(percent) + "%: A left, B center, C right";
    if (mode == chipper::ChipMode::sn76489)
        return juce::String(percent) + "%: Tone 1 left, Tone 3 right";

    return juce::String(percent) + "% modern stereo spread";
}

juce::String ChipperAudioProcessorEditor::nesSweepReadout(float value) const
{
    if (value < 0.18f)
        return "Sweep mostly off";
    if (value < 0.45f)
        return "Small pitch gestures";
    if (value < 0.75f)
        return "Coin / jump bends";
    return "Deep laser sweep";
}

juce::String ChipperAudioProcessorEditor::nesNoiseReadout(float value) const
{
    const auto period = chipper::nesNoisePeriodForControl(value);
    if (value < 0.25f)
        return juce::String("$400E period ") + juce::String(static_cast<int>(period)) + ", low grit";
    if (value < 0.60f)
        return juce::String("$400E period ") + juce::String(static_cast<int>(period)) + ", snare noise";
    return juce::String("$400E period ") + juce::String(static_cast<int>(period)) + ", short hats";
}

juce::String ChipperAudioProcessorEditor::nesFocusReadout(float value) const
{
    if (value < 0.33f)
        return "Triangle bass focus";
    if (value > 0.66f)
        return "Pulse stack focus";
    return "Pulse + triangle stack";
}

juce::String ChipperAudioProcessorEditor::dmgSweepReadout(float value) const
{
    const auto reg = chipper::dmgSweepRegisterForControl(value);
    const auto shift = chipper::dmgSweepShiftForControl(value);
    const auto prefix = juce::String("NR10 0x") + juce::String::toHexString(static_cast<int>(reg)).toUpperCase()
        + juce::String(", shift ") + juce::String(static_cast<int>(shift));
    if (shift == 0)
        return prefix + ", mostly off";
    if (shift <= 2)
        return prefix + ", small sweep";
    if (shift <= 5)
        return prefix + ", pitch bend";
    return prefix + ", fast sweep";
}

juce::String ChipperAudioProcessorEditor::dmgNoiseReadout(float value) const
{
    const auto shift = chipper::dmgNoiseClockShiftForControl(value);
    return juce::String("NR43 clock shift ") + juce::String(static_cast<int>(shift)) + ", divisor code 2";
}

juce::String ChipperAudioProcessorEditor::dmgEnvelopeReadout(float value) const
{
    const auto level = chipper::dmgInitialEnvelopeLevelForControl(value);
    return juce::String("NRx2 initial volume ") + juce::String(static_cast<int>(level)) + "/15";
}

juce::String ChipperAudioProcessorEditor::ymSpreadReadout(float value) const
{
    const auto spread = std::clamp(static_cast<int>(std::round(value * 12.0f)), 0, 12);
    return juce::String("A/B/C spread ") + juce::String(spread) + " semitones";
}

juce::String ChipperAudioProcessorEditor::ymMotionReadout(float value) const
{
    if (value < 0.25f)
        return "Minimal pitch motion";
    if (value < 0.65f)
        return "Moderate pitch motion";
    return "Wide arcade pitch motion";
}

juce::String ChipperAudioProcessorEditor::ymNoiseReadout(float value) const
{
    const auto period = chipper::ym2149NoisePeriodForControl(value);
    return juce::String("Reg 6 period ") + juce::String(static_cast<int>(period)) + "/31";
}

juce::String ChipperAudioProcessorEditor::ymToneNoiseReadout(float value) const
{
    const auto mixer = chipper::ym2149MixerRegisterForControl(value);
    switch (mixer)
    {
        case 0x07u: return "Reg 7=0x07, noise only";
        case 0x00u: return "Reg 7=0x00, tone + noise";
        case 0x38u:
        default:
            return "Reg 7=0x38, tone only";
    }
}

juce::String ChipperAudioProcessorEditor::ymChannelMixReadout(const chipper::PatchConfig& patch) const
{
    static constexpr std::array<const char*, 5> choiceLabels { "Preset", "Tone", "Noise", "Both", "Off" };
    const auto macroMixer = chipper::ym2149MixerRegisterForControl(patch.control4);
    const auto mixer = chipper::ym2149MixerRegisterWithChannelOverrides(patch, macroMixer);
    juce::String text = "Reg 7=0x";
    text += juce::String::toHexString(static_cast<int>(mixer)).paddedLeft('0', 2).toUpperCase();
    text += " ";

    for (size_t channel = 0; channel < ymChannelMixCount; ++channel)
    {
        if (channel > 0)
            text += " | ";

        const auto choice = static_cast<size_t>(std::clamp(chipper::ym2149ChannelMixChoiceForPatch(patch, channel), 0, 4));
        text += juce::String::charToString(static_cast<juce_wchar>('A' + channel));
        text += ":";
        text += choiceLabels[choice];
    }

    return text;
}

juce::String ChipperAudioProcessorEditor::snStackReadout(float value) const
{
    const auto spread = std::clamp(static_cast<int>(std::round(value * 12.0f)), 0, 12);
    return juce::String("Tone spread ") + juce::String(spread) + " semitones";
}

juce::String ChipperAudioProcessorEditor::snMotionReadout(float value) const
{
    if (value < 0.25f)
        return "Small pitch motion";
    if (value < 0.60f)
        return "Arcade blip bends";
    return "Wide SFX pitch motion";
}

juce::String ChipperAudioProcessorEditor::snLevelReadout(float value) const
{
    const auto attenuation = chipper::sn76489NoiseAttenuationForControl(value);
    const auto attenuationText = juce::String("Attenuation reg ") + juce::String(static_cast<int>(attenuation)) + "/15";
    if (attenuation == 15u)
        return attenuationText + ", muted";
    if (attenuation == 0u)
        return attenuationText + ", full level";

    return attenuationText + juce::String(", -") + juce::String(static_cast<int>(attenuation) * 2) + " dB";
}

juce::String ChipperAudioProcessorEditor::snSourceCardLabel(const chipper::PatchConfig& patch, size_t index) const
{
    const auto noiseControl = chipper::sn76489NoiseControlForPatch(patch);
    const auto noiseClockedByTone3 = (noiseControl & 0x03u) == 0x03u;

    if (index == 0)
        return "Tone 1 | lead";
    if (index == 1)
        return "Tone 2 | support";
    if (index == 2)
        return noiseClockedByTone3 ? "Tone 3 | noise clock" : "Tone 3 | bass";

    const auto kind = (noiseControl & 0x04u) != 0 ? "white" : "periodic";
    const auto rate = noiseControl & 0x03u;
    const auto rateText = rate == 3u ? "T3" : (rate == 0u ? "/512" : (rate == 1u ? "/1024" : "/2048"));
    return juce::String("Noise | ") + kind + " " + rateText;
}

juce::String ChipperAudioProcessorEditor::snSourceCardTooltip(const chipper::PatchConfig& patch,
                                                              size_t index,
                                                              const chipper::ChipParameterSpec* spec) const
{
    const auto noiseControl = chipper::sn76489NoiseControlForPatch(patch);
    const auto base = spec != nullptr
                          ? juce::String(spec->label) + ": " + juce::String(spec->help)
                          : juce::String("SN76489 source");

    if (index < 3)
    {
        auto text = base + "\nNative square-wave tone channel.";
        if (index == 2)
        {
            text += "\nCurrent noise clock: ";
            text += (noiseControl & 0x03u) == 0x03u ? "Tone 3 period" : "internal PSG divider";
        }
        text += "\nSource Level is a modern trim after the PSG attenuation stage.";
        return text;
    }

    return base
        + "\nNoise register: Reg E=0x" + juce::String::toHexString(static_cast<int>(noiseControl)).toUpperCase()
        + ", " + snNoiseRegisterLabel(noiseControl)
        + "\nNoise attenuation: " + snLevelReadout(patch.control4);
}

juce::String ChipperAudioProcessorEditor::nesDmcDirectReadout(float value) const
{
    const auto level = chipper::nesDmcDirectLevelForControl(value);
    if (level == 0u)
        return "$4011 0/127, silent";

    return juce::String("$4011 ") + juce::String(static_cast<int>(level)) + "/127, direct grit";
}

juce::String ChipperAudioProcessorEditor::sidPulseWidthReadout(float value) const
{
    const auto pulseWidth = chipper::sidPulseWidthForControl(value);
    const auto percent = static_cast<int>(std::round((static_cast<float>(pulseWidth) / 4095.0f) * 100.0f));
    return juce::String("PW ") + juce::String(static_cast<int>(pulseWidth)) + "/4095, " + juce::String(percent) + "%";
}

juce::String ChipperAudioProcessorEditor::sidDetuneReadout(float value) const
{
    const auto semitones = std::clamp(static_cast<int>(std::round(value * 12.0f)), 0, 12);
    return juce::String("Voice spread ") + juce::String(semitones) + " semitones";
}

juce::String ChipperAudioProcessorEditor::sidCutoffReadout(float value) const
{
    const auto cutoff = std::clamp(static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 2047.0f)), 0, 2047);
    return juce::String("FC registers ") + juce::String(cutoff) + "/2047";
}

juce::String ChipperAudioProcessorEditor::sidSustainReadout(const chipper::PatchConfig& patch) const
{
    auto text = juce::String("Sustain nibbles ");
    for (size_t voice = 0; voice < sidAdsrVoiceCount; ++voice)
    {
        if (voice > 0)
            text += " | ";

        text += "V" + juce::String(static_cast<int>(voice + 1u))
            + ":" + juce::String(static_cast<int>(chipper::sidSustainNibbleForVoice(patch, voice)));
    }
    return text;
}

juce::String ChipperAudioProcessorEditor::sourceLevelReadout(size_t index) const
{
    static constexpr std::array<const char*, sourceChannelCount> sourceIds {
        chipper::parameters::id::source1Enabled,
        chipper::parameters::id::source2Enabled,
        chipper::parameters::id::source3Enabled,
        chipper::parameters::id::source4Enabled,
        chipper::parameters::id::source5Enabled,
        chipper::parameters::id::source6Enabled,
        chipper::parameters::id::source7Enabled,
        chipper::parameters::id::source8Enabled,
        chipper::parameters::id::source9Enabled
    };
    static constexpr std::array<const char*, sourceChannelCount> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level,
        chipper::parameters::id::source5Level,
        chipper::parameters::id::source6Level,
        chipper::parameters::id::source7Level,
        chipper::parameters::id::source8Level,
        chipper::parameters::id::source9Level
    };

    const auto safeIndex = std::min(index, sourceLevelIds.size() - 1u);
    const auto enabled = parameterValue(sourceIds[safeIndex]) >= 0.5f;
    const auto level = std::clamp(parameterValue(sourceLevelIds[safeIndex]), 0.0f, 1.0f);
    const auto percent = static_cast<int>(std::round(level * 100.0f));

    juce::String text;
    if (! enabled)
        text = "Off / ";

    if (level <= 0.0001f)
        return text + "0% / silent";

    const auto db = 20.0f * std::log10(level);
    return text + juce::String(percent) + "% / " + (std::abs(db) < 0.05f ? juce::String("0 dB") : juce::String(db, 1) + " dB");
}

bool ChipperAudioProcessorEditor::usesSourceChannelSurface(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::source1Enabled,
                                            chipper::ControlSurface::sourceCards);
}

bool ChipperAudioProcessorEditor::usesEnvelopeDecayControl(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::envelopeDecay,
                                            chipper::ControlSurface::slider);
}

bool ChipperAudioProcessorEditor::usesStereoSpreadControl(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::stereoSpread,
                                            chipper::ControlSurface::slider);
}

void ChipperAudioProcessorEditor::setSourceChannelSurfaceVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesSourceChannelSurface(mode);
    for (size_t i = 0; i < sourceChannelButtons.size(); ++i)
    {
        const auto hasSource = chipper::parameterSpecFor(mode, sourceRole(i)) != nullptr;
        const auto visible = active && hasSource;
        sourceChannelButtons[i].setVisible(visible);
        sourcePreviewScopes[i].setVisible(visible);
        sourceLevelSliders[i].setVisible(visible && chipper::parameterSpecFor(mode, sourceLevelRole(i)) != nullptr);
        sourceLevelLabels[i].setVisible(visible && chipper::parameterSpecFor(mode, sourceLevelRole(i)) != nullptr);
        sourceLevelValueLabels[i].setVisible(visible && chipper::parameterSpecFor(mode, sourceLevelRole(i)) != nullptr);
        if (i < paulaVoiceSampleBoxes.size())
        {
            const auto paulaSlotVisible = visible && mode == chipper::ChipMode::paula;
            paulaVoiceSampleLabels[i].setVisible(paulaSlotVisible);
            paulaVoiceSampleBoxes[i].setVisible(paulaSlotVisible);
        }
    }

    setSidVoicePulseWidthControlsVisible(active && mode == chipper::ChipMode::sid);

    for (auto& itemLabel : moduleItemLabels[1])
        itemLabel.setVisible(! active && ! itemLabel.getText().isEmpty());

    if (active)
        updateSourceChannelButtons(mode);
}

void ChipperAudioProcessorEditor::setStereoSpreadControlVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesStereoSpreadControl(mode);
    stereoSpreadLabel.setVisible(active);
    stereoSpreadSlider.setVisible(active);
    stereoSpreadValueLabel.setVisible(active);
    stereoSpreadLabel.setEnabled(active);
    stereoSpreadSlider.setEnabled(active);
    stereoSpreadValueLabel.setEnabled(active);
    stereoSpreadLabel.setAlpha(active ? 1.0f : 0.55f);
    stereoSpreadSlider.setAlpha(active ? 1.0f : 0.55f);
    stereoSpreadValueLabel.setAlpha(active ? 1.0f : 0.55f);

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::stereoSpread))
    {
        stereoSpreadLabel.setText(spec->label, juce::dontSendNotification);
        stereoSpreadLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        stereoSpreadSlider.setTooltip(withMidiCcForRole(spec->help, spec->role));
        stereoSpreadValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
    }

    if (active)
        updateStereoSpreadReadout(mode);
}

void ChipperAudioProcessorEditor::setPulse2DutySegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesPulse2DutySegment(mode);
    pulse2DutyLabel.setVisible(active);
    pulse2DutyValueLabel.setVisible(active);
    for (auto& button : pulse2DutyButtons)
        button.setVisible(active);

    if (active)
    {
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updatePulse2DutyButtons(patch, true);
    }
}

void ChipperAudioProcessorEditor::setWaveShapeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto usesPerLaneWaves = usesChannelLocalWaveDeck(mode);
    setSidVoiceWaveControlsVisible(shouldBeVisible && mode == chipper::ChipMode::sid);
    const auto perLaneWaveActive = usesPerLaneWaves
        && chipper::descriptorFor(mode).implemented
        && usesSourceChannelSurface(mode);
    setHucVoiceWaveControlsVisible(perLaneWaveActive);
    if (mode == chipper::ChipMode::sid)
    {
        for (auto& button : waveShapeButtons)
            button.setVisible(false);
        fmAlgorithmBox.setVisible(false);
        fmAlgorithmPreview.setVisible(false);
        oplWaveformBox.setVisible(false);
        oplWaveformPreview.setVisible(false);
        opllInstrumentBox.setVisible(false);
        return;
    }
    if (usesPerLaneWaves)
    {
        for (auto& button : waveShapeButtons)
            button.setVisible(false);
        fmAlgorithmBox.setVisible(false);
        fmAlgorithmPreview.setVisible(false);
        oplWaveformBox.setVisible(false);
        oplWaveformPreview.setVisible(false);
        opllInstrumentBox.setVisible(false);
        return;
    }

    const auto active = shouldBeVisible && usesWaveShapeSegment(mode);
    const auto fmAlgorithmActive = active && (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151);
    const auto oplWaveformActive = active && mode == chipper::ChipMode::opl3;
    const auto opllInstrumentActive = active && mode == chipper::ChipMode::ym2413;
    const auto embeddedInSourceCard = mode == chipper::ChipMode::dmg;
    waveShapeLabel.setVisible(active);
    waveShapeValueLabel.setVisible(active && ! embeddedInSourceCard);
    for (auto& button : waveShapeButtons)
        button.setVisible(active && ! fmAlgorithmActive && ! oplWaveformActive && ! opllInstrumentActive);
    fmAlgorithmBox.setVisible(fmAlgorithmActive);
    fmAlgorithmPreview.setVisible(fmAlgorithmActive);
    oplWaveformBox.setVisible(oplWaveformActive);
    oplWaveformPreview.setVisible(oplWaveformActive);
    opllInstrumentBox.setVisible(opllInstrumentActive);

    if (active)
    {
        const auto choice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape)));
        if (fmAlgorithmActive)
            updateFmAlgorithmControl(mode, choice, true);
        else if (oplWaveformActive)
            updateOplWaveformControl(mode, choice, true);
        else if (opllInstrumentActive)
            updateOpllInstrumentControl(mode, choice, true);
        else
            updateWaveShapeButtons(choice, true);
    }
}

void ChipperAudioProcessorEditor::setSidVoiceWaveControlsVisible(bool shouldBeVisible)
{
    waveShapeLabel.setVisible(false);
    waveShapeValueLabel.setVisible(false);
    for (auto& label : sidVoiceWaveLabels)
        label.setVisible(shouldBeVisible);
    for (auto& box : sidVoiceWaveBoxes)
        box.setVisible(shouldBeVisible);

    if (shouldBeVisible)
        updateSidVoiceWaveControls(true);
}

void ChipperAudioProcessorEditor::setHucVoiceWaveControlsVisible(bool shouldBeVisible)
{
    waveShapeLabel.setVisible(false);
    waveShapeValueLabel.setVisible(false);
    for (auto& label : hucVoiceWaveLabels)
        label.setVisible(shouldBeVisible);
    for (auto& box : hucVoiceWaveBoxes)
        box.setVisible(shouldBeVisible);

    if (shouldBeVisible)
        updateHucVoiceWaveControls(true);
}

void ChipperAudioProcessorEditor::setSidVoicePulseWidthControlsVisible(bool shouldBeVisible)
{
    for (auto& label : sidVoicePulseWidthLabels)
        label.setVisible(shouldBeVisible);
    for (auto& slider : sidVoicePulseWidthSliders)
        slider.setVisible(shouldBeVisible);
    for (auto& label : sidVoicePulseWidthValueLabels)
        label.setVisible(shouldBeVisible);

    if (shouldBeVisible)
    {
        const auto patch = currentUiPatch(
            chipper::ChipMode::sid,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updateSidVoicePulseWidthControls(patch, true);
    }
}

void ChipperAudioProcessorEditor::setDmgWaveLevelSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesDmgWaveLevelSegment(mode);
    const auto embeddedInSourceCard = mode == chipper::ChipMode::dmg;
    dmgWaveLevelLabel.setVisible(active);
    dmgWaveLevelValueLabel.setVisible(active && ! embeddedInSourceCard);
    for (auto& button : dmgWaveLevelButtons)
        button.setVisible(active);

    if (active)
    {
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updateDmgWaveLevelButtons(patch, true);
    }
}

void ChipperAudioProcessorEditor::setDmgStereoRouteSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesDmgStereoRouteSegment(mode);
    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::dmgStereoRoute);
    const auto usesSpc700LoopToggle = active && mode == chipper::ChipMode::spc700;
    const auto usesMenu = active && spec != nullptr && spec->surface == chipper::ControlSurface::menu && ! usesSpc700LoopToggle;
    dmgStereoRouteLabel.setVisible(active);
    dmgStereoRouteValueLabel.setVisible(active);
    dmgStereoRouteBox.setVisible(usesMenu);
    spc700LoopModeButton.setVisible(usesSpc700LoopToggle);
    for (auto& button : dmgStereoRouteButtons)
        button.setVisible(active && ! usesMenu);

    if (active)
    {
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updateDmgStereoRouteButtons(mode, patch, true);
    }
}

void ChipperAudioProcessorEditor::setYmEnvelopeShapeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesYmEnvelopeShapeSegment(mode);
    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::ymEnvelopeShape);
    const auto usesMenu = active && spec != nullptr && spec->surface == chipper::ControlSurface::menu;
    ymEnvelopeShapeLabel.setVisible(active);
    ymEnvelopeShapeValueLabel.setVisible(active);
    sidFilterModeBox.setVisible(usesMenu);
    for (auto& button : ymEnvelopeShapeButtons)
        button.setVisible(active && ! usesMenu);

    if (active)
    {
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updateYmEnvelopeShapeButtons(mode, patch, true);
    }
}

void ChipperAudioProcessorEditor::setSidFilterRoutingControlVisible(bool shouldBeVisible)
{
    sidFilterRoutingLabel.setVisible(shouldBeVisible);
    sidFilterRoutingBox.setVisible(shouldBeVisible);
    sidFilterRoutingValueLabel.setVisible(shouldBeVisible);

    if (shouldBeVisible)
        updateSidFilterRoutingControl(true);
}

void ChipperAudioProcessorEditor::setYmChannelMixControlsVisible(bool shouldBeVisible)
{
    ymChannelMixLabel.setVisible(shouldBeVisible);
    ymChannelMixValueLabel.setVisible(shouldBeVisible);
    for (auto& label : ymChannelMixLabels)
        label.setVisible(shouldBeVisible);
    for (auto& box : ymChannelMixBoxes)
        box.setVisible(shouldBeVisible);

    if (shouldBeVisible)
        updateYmChannelMixControls(true);
}

void ChipperAudioProcessorEditor::setSnNoiseModeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesSnNoiseModeSegment(mode);
    const auto embeddedInSourceCard = mode == chipper::ChipMode::nes || mode == chipper::ChipMode::dmg;
    const auto menuActive = active && chipper::chipHasParameterSurface(mode,
                                                                       chipper::ChipParameterRole::snNoiseMode,
                                                                       chipper::ControlSurface::menu);
    snNoiseModeLabel.setVisible(active);
    snNoiseModeValueLabel.setVisible(active && ! embeddedInSourceCard);
    for (auto& button : snNoiseModeButtons)
        button.setVisible(active && ! menuActive);
    snNoiseModeBox.setVisible(menuActive);

    if (active)
    {
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updateSnNoiseModeButtons(mode, patch, true);
    }
}

void ChipperAudioProcessorEditor::setEnvelopeDecayControlVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesEnvelopeDecayControl(mode);
    setSidAdsrControlsVisible(active && mode == chipper::ChipMode::sid);
    setFmOperatorRegisterSurfaceVisible(mode,
                                        shouldBeVisible
                                            && (mode == chipper::ChipMode::ym2612
                                                || mode == chipper::ChipMode::opl3
                                                || mode == chipper::ChipMode::ym2151));
    ymEnvelopePreview.setVisible(active && mode == chipper::ChipMode::ym2149);
    envelopeDecayLabel.setVisible(active);
    envelopeDecaySlider.setVisible(active);
    envelopeDecayValueLabel.setVisible(active);
    envelopeDecayLabel.setEnabled(active);
    envelopeDecaySlider.setEnabled(active);
    envelopeDecayValueLabel.setEnabled(active);
    envelopeDecayLabel.setAlpha(active ? 1.0f : 0.55f);
    envelopeDecaySlider.setAlpha(active ? 1.0f : 0.55f);
    envelopeDecayValueLabel.setAlpha(active ? 1.0f : 0.55f);

    if (active)
    {
        updateEnvelopeDecayReadout(mode);
        if (mode == chipper::ChipMode::ym2149)
        {
            const auto patch = currentUiPatch(
                mode,
                parameterValue(chipper::parameters::id::macroControl1),
                parameterValue(chipper::parameters::id::macroControl2),
                parameterValue(chipper::parameters::id::macroControl3),
                parameterValue(chipper::parameters::id::macroControl4),
                static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
                static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
                static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
                static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
                static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
                parameterValue(chipper::parameters::id::stereoSpread));
            updateYmEnvelopePreview(mode, patch, true);
        }
    }
}

void ChipperAudioProcessorEditor::setSidAdsrControlsVisible(bool shouldBeVisible)
{
    for (auto& label : sidEnvelopeVoiceLabels)
        label.setVisible(shouldBeVisible);
    for (auto& preview : sidEnvelopePreviews)
        preview.setVisible(shouldBeVisible);
    for (auto& label : sidAdsrHeaderLabels)
        label.setVisible(false);
    for (auto& label : sidAdsrLabels)
        label.setVisible(shouldBeVisible);
    for (auto& label : sidAdsrValueLabels)
        label.setVisible(shouldBeVisible);
    for (auto& box : sidAdsrBoxes)
        box.setVisible(false);
    for (auto& slider : sidAdsrSliders)
        slider.setVisible(shouldBeVisible);

    if (shouldBeVisible)
        updateSidAdsrControls(true);
}

void ChipperAudioProcessorEditor::setFmOperatorRegisterSurfaceVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible
        && chipper::descriptorFor(mode).implemented
        && (mode == chipper::ChipMode::ym2612
            || mode == chipper::ChipMode::opl3
            || mode == chipper::ChipMode::ym2151);

    for (auto& label : fmOperatorNameLabels)
        label.setVisible(active);
    for (auto& label : fmOperatorValueLabels)
        label.setVisible(active);

    if (! active)
        return;

    const auto patch = currentUiPatch(
        mode,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));
    updateFmOperatorRegisterSurface(mode, patch, true);
}

void ChipperAudioProcessorEditor::updateSourceChannelButtons(chipper::ChipMode mode)
{
    static const std::array<const char*, sourceChannelCount> nesBigMonoLabels {
        "Pulse 1  |  duty lead",
        "Pulse 2  |  stack / sweep",
        "Triangle | bass body",
        "Noise/DMC | snare / sample",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> nesChipPolyLabels {
        "Pulse 1  |  note 1",
        "Pulse 2  |  note 2",
        "Triangle | note 3 bass",
        "Noise/DMC | mono SFX",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> dmgBigMonoLabels {
        "Pulse 1  |  sweep voice",
        "Pulse 2  |  support pulse",
        "Wave     |  sample body",
        "Noise    |  handheld grit",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> dmgChipPolyLabels {
        "Pulse 1  |  note 1",
        "Pulse 2  |  note 2",
        "Wave     |  note 3",
        "Noise    |  SFX layer",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> ymBigMonoLabels {
        "Channel A | tone lead",
        "Channel B | chord tone",
        "Channel C | bass / stack",
        "Noise     | shared PSG",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> ymChipPolyLabels {
        "Channel A | note 1",
        "Channel B | note 2",
        "Channel C | note 3",
        "Noise     | shared layer",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> snBigMonoLabels {
        "Tone 1 | lead tone",
        "Tone 2 | chord tone",
        "Tone 3 | bass / noise clock",
        "Noise  | PSG noise",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> snChipPolyLabels {
        "Tone 1 | note 1",
        "Tone 2 | note 2",
        "Tone 3 | note 3",
        "Noise  | mono SFX layer",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> sidBigMonoLabels {
        "Voice 1 | lead",
        "Voice 2 | detune",
        "Voice 3 | stack / noise",
        "Ext In  | unavailable",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> sidChipPolyLabels {
        "Voice 1 | note 1",
        "Voice 2 | note 2",
        "Voice 3 | note 3",
        "Ext In  | unavailable",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> pokeyBigMonoLabels {
        "Ch 1 | AUDF/AUDC",
        "Ch 2 | detune/noise",
        "Ch 3 | bass timer",
        "Ch 4 | grit layer",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> pokeyChipPolyLabels {
        "Ch 1 | note 1",
        "Ch 2 | note 2",
        "Ch 3 | note 3",
        "Ch 4 | note 4",
        "Source 5 | hidden",
        "Source 6 | hidden",
        "Source 7 | hidden",
        "Source 8 | hidden",
        "Source 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> sampleBigMonoLabels {
        "Voice 1 | sample lead",
        "Voice 2 | support",
        "Voice 3 | texture",
        "Voice 4 | layer",
        "Voice 5 | rear layer",
        "Voice 6 | pitch pair",
        "Voice 7 | echo stack",
        "Voice 8 | noise / hit",
        "Voice 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> sampleChipPolyLabels {
        "Voice 1 | note 1",
        "Voice 2 | note 2",
        "Voice 3 | note 3",
        "Voice 4 | note 4",
        "Voice 5 | note 5",
        "Voice 6 | note 6",
        "Voice 7 | note 7",
        "Voice 8 | note 8",
        "Voice 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> wavetableBigMonoLabels {
        "Wave 1 | lead",
        "Wave 2 | chord tone",
        "Wave 3 | bass",
        "Wave 4 | layer",
        "Wave 5 | upper/noise",
        "Wave 6 | upper/noise",
        "Wave 7 | extra lane",
        "Wave 8 | extra lane",
        "Wave 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> wavetableChipPolyLabels {
        "Wave 1 | note 1",
        "Wave 2 | note 2",
        "Wave 3 | note 3",
        "Wave 4 | note 4",
        "Wave 5 | note 5",
        "Wave 6 | note 6",
        "Wave 7 | note 7",
        "Wave 8 | note 8",
        "Wave 9 | hidden"
    };
    static const std::array<const char*, sourceChannelCount> fmBigMonoLabels {
        "FM 1 | carrier",
        "FM 2 | stack",
        "FM 3 | color",
        "FM 4 | layer",
        "FM 5 | octave",
        "FM 6 | shimmer",
        "FM 7 | extra",
        "FM 8 | extra",
        "FM 9 | OPLL lane"
    };
    static const std::array<const char*, sourceChannelCount> fmChipPolyLabels {
        "FM 1 | note 1",
        "FM 2 | note 2",
        "FM 3 | note 3",
        "FM 4 | note 4",
        "FM 5 | note 5",
        "FM 6 | note 6",
        "FM 7 | note 7",
        "FM 8 | note 8",
        "FM 9 | note 9"
    };

    const auto playModeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::playMode)));
    const auto playMode = chipper::parameters::playModeFromChoice(playModeChoice);
    const auto* labels = &nesBigMonoLabels;

    if (mode == chipper::ChipMode::nes)
        labels = playMode == chipper::PlayMode::chipPoly ? &nesChipPolyLabels : &nesBigMonoLabels;
    else if (mode == chipper::ChipMode::dmg)
        labels = playMode == chipper::PlayMode::chipPoly ? &dmgChipPolyLabels : &dmgBigMonoLabels;
    else if (mode == chipper::ChipMode::ym2149)
        labels = playMode == chipper::PlayMode::chipPoly ? &ymChipPolyLabels : &ymBigMonoLabels;
    else if (mode == chipper::ChipMode::sn76489)
        labels = playMode == chipper::PlayMode::chipPoly ? &snChipPolyLabels : &snBigMonoLabels;
    else if (mode == chipper::ChipMode::sid)
        labels = playMode == chipper::PlayMode::chipPoly ? &sidChipPolyLabels : &sidBigMonoLabels;
    else if (mode == chipper::ChipMode::pokey)
        labels = playMode == chipper::PlayMode::chipPoly ? &pokeyChipPolyLabels : &pokeyBigMonoLabels;
    else if (mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula)
        labels = playMode == chipper::PlayMode::chipPoly ? &sampleChipPolyLabels : &sampleBigMonoLabels;
    else if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
        labels = playMode == chipper::PlayMode::chipPoly ? &wavetableChipPolyLabels : &wavetableBigMonoLabels;
    else if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2413)
        labels = playMode == chipper::PlayMode::chipPoly ? &fmChipPolyLabels : &fmBigMonoLabels;

    const auto patch = currentUiPatch(
        mode,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));

    for (size_t i = 0; i < sourceChannelButtons.size(); ++i)
    {
        const auto* spec = chipper::parameterSpecFor(mode, sourceRole(i));
        const auto* levelSpec = chipper::parameterSpecFor(mode, sourceLevelRole(i));
        const auto isSidVoice = mode == chipper::ChipMode::sid && i < sidVoiceWaveCount && spec != nullptr;
        const auto sidWaveBits = isSidVoice ? chipper::sidWaveformControlForVoice(patch, i) : uint8_t { 0u };
        juce::String buttonLabel;
        if (isSidVoice)
            buttonLabel = juce::String(spec->label) + " | " + sidWaveNameForControlBits(sidWaveBits);
        else if (mode == chipper::ChipMode::sn76489 && spec != nullptr)
            buttonLabel = snSourceCardLabel(patch, i);
        else if (spec != nullptr && labels != &nesBigMonoLabels)
            buttonLabel = sourceCardNativeLabel(mode, patch, i, juce::String((*labels)[i]));
        else
            buttonLabel = spec != nullptr ? juce::String(spec->label) : juce::String((*labels)[i]);

        sourceChannelButtons[i].setButtonText(buttonLabel);
        if (isSidVoice)
        {
            sourceChannelButtons[i].setTooltip(withMidiCcForRole(buttonLabel
                                                                     + ": " + juce::String(spec->help)
                                                                     + "\nCTRL waveform bits 0x" + byteHex(sidWaveBits)
                                                                     + "\n" + sidVoicePulseWidthReadout(patch, i)
                                                                     + "\n" + sidAdsrNibbleReadout(patch, i),
                                                                 sourceRole(i)));
        }
        else if (mode == chipper::ChipMode::sn76489 && spec != nullptr)
            sourceChannelButtons[i].setTooltip(withMidiCcForRole(snSourceCardTooltip(patch, i, spec), sourceRole(i)));
        else if (spec != nullptr && labels != &nesBigMonoLabels)
        {
            auto tooltip = buttonLabel + ": " + juce::String(spec->help);
            if (mode == chipper::ChipMode::pokey)
                tooltip += "\n" + pokeySourceRegisterReadout(patch, i);
            else if (mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula)
                tooltip += "\n" + sampleSourceRegisterReadout(mode, patch, i);
            else if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
                tooltip += "\n" + wavetableSourceRegisterReadout(mode, patch, i);
            else if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2413)
                tooltip += "\n" + fmSourceRegisterReadout(mode, patch, i);
            tooltip += "\n" + macroTemplateReadout(mode, patch);
            sourceChannelButtons[i].setTooltip(withMidiCcForRole(tooltip, sourceRole(i)));
        }
        else if (spec != nullptr)
            sourceChannelButtons[i].setTooltip(withMidiCcForRole(juce::String(spec->label) + ": " + juce::String(spec->help), sourceRole(i)));
        else
            sourceChannelButtons[i].setTooltip(withMidiCcForRole(juce::String("Enable or mute ") + (*labels)[i], sourceRole(i)));

        if (levelSpec != nullptr)
        {
            auto levelTooltip = juce::String(levelSpec->label) + ": " + juce::String(levelSpec->help);
            if (mode == chipper::ChipMode::pokey)
                levelTooltip += "\n" + pokeySourceRegisterReadout(patch, i);
            sourceLevelLabels[i].setTooltip(withMidiCcForRole(levelTooltip, sourceLevelRole(i)));
            sourceLevelSliders[i].setTooltip(withMidiCcForRole(levelTooltip, sourceLevelRole(i)));
        }
        else
        {
            sourceLevelLabels[i].setTooltip(withMidiCcForRole(juce::String("Trim level for ") + (*labels)[i], sourceLevelRole(i)));
            sourceLevelSliders[i].setTooltip(withMidiCcForRole(juce::String("Trim level for ") + (*labels)[i], sourceLevelRole(i)));
        }

        sourceLevelLabels[i].setText(mode == chipper::ChipMode::pokey ? "AUDV" : "Level", juce::dontSendNotification);
        sourceLevelValueLabels[i].setTooltip(sourceLevelSliders[i].getTooltip());
        sourceLevelValueLabels[i].setText(mode == chipper::ChipMode::pokey ? pokeySourceLevelReadout(patch, i) : sourceLevelReadout(i), juce::dontSendNotification);
        updateSourcePreviewScope(mode, patch, i, spec != nullptr && chipper::descriptorFor(mode).implemented);
    }

    updateSidVoicePulseWidthControls(patch, mode == chipper::ChipMode::sid && chipper::descriptorFor(mode).implemented);
}

void ChipperAudioProcessorEditor::updateSourcePreviewScope(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index, bool shouldBeVisible)
{
    if (index >= sourcePreviewScopes.size())
        return;

    auto& scope = sourcePreviewScopes[index];
    const auto hasSource = chipper::parameterSpecFor(mode, sourceRole(index)) != nullptr;
    const auto visible = shouldBeVisible && hasSource && usesSourceChannelSurface(mode);
    scope.setVisible(visible);
    if (! visible)
        return;

    const auto sourceIsEnabled = patch.sourceEnabled[std::min(index, patch.sourceEnabled.size() - 1u)];
    auto shape = ChipWaveformPreviewShape::off;
    auto duty = 0.5f;
    juce::String tooltip;

    if (mode == chipper::ChipMode::nes)
    {
        if (index == 0)
        {
            shape = ChipWaveformPreviewShape::pulse;
            duty = pulseDutyRatioForControl(patch.control1);
            tooltip = "RP2A03 pulse 1 duty: " + pulseDutyReadout(mode, patch.control1);
        }
        else if (index == 1)
        {
            shape = ChipWaveformPreviewShape::pulse;
            duty = pulseDutyRatioForChoice(nesPulse2DutyChoiceForPatch(patch));
            tooltip = "RP2A03 pulse 2 duty: " + pulse2DutyReadout(patch);
        }
        else if (index == 2)
        {
            shape = ChipWaveformPreviewShape::triangle;
            tooltip = "RP2A03 triangle sequencer preview.";
        }
        else
        {
            shape = ChipWaveformPreviewShape::noise;
            tooltip = "RP2A03 noise / DMC lane: " + nesNoiseModeReadout(patch)
                + "\nDMC Direct " + nesDmcDirectReadout(patch.nesDmcDirectLevel)
                + "\nExternal .dmc playback is available from the DMC sample bank.";
        }
    }
    else if (mode == chipper::ChipMode::dmg)
    {
        if (index <= 1)
        {
            shape = ChipWaveformPreviewShape::pulse;
            duty = pulseDutyRatioForChoice(dmgPulseDutyChoiceForSource(patch, index));
            tooltip = index == 0
                ? juce::String("DMG pulse 1 duty: ") + pulseDutyReadout(mode, patch.control1)
                : juce::String("DMG pulse 2 duty: ") + pulse2DutyReadout(patch);
        }
        else if (index == 2)
        {
            shape = dmgWavePreviewShape(patch);
            duty = 0.5f;
            tooltip = "DMG Wave RAM: " + waveShapeReadout(mode, patch.waveShape) + "\n" + dmgWaveLevelReadout(patch);
        }
        else
        {
            shape = ChipWaveformPreviewShape::noise;
            tooltip = "DMG noise channel: " + dmgNoiseModeReadout(patch);
        }
    }
    else if (mode == chipper::ChipMode::sid && index < sidVoiceWaveCount)
    {
        const auto bits = chipper::sidWaveformControlForVoice(patch, index);
        scope.setSidWaveform(bits, static_cast<float>(chipper::sidPulseWidthForVoice(patch, index)) / 4095.0f, sourceIsEnabled);
        tooltip = juce::String("SID voice ") + juce::String(static_cast<int>(index + 1u))
            + " waveform: " + sidWaveNameForControlBits(bits)
            + "\nCTRL waveform bits 0x" + byteHex(bits)
            + "\n" + sidVoicePulseWidthReadout(patch, index)
            + "\n" + sidAdsrNibbleReadout(patch, index)
            + "\nPreview follows the SID pulse-width register for pulse waves.";
        scope.setTooltip(withMidiCcForRole(tooltip, sourceRole(index)));
        return;
    }
    else if (mode == chipper::ChipMode::ym2149)
    {
        if (index < 3)
        {
            const auto macroMixer = chipper::ym2149MixerRegisterForControl(patch.control4);
            const auto mixer = chipper::ym2149MixerRegisterWithChannelOverrides(patch, macroMixer);
            const auto toneEnabled = (mixer & static_cast<uint8_t>(1u << index)) == 0u;
            const auto noiseEnabled = (mixer & static_cast<uint8_t>(1u << (index + 3u))) == 0u;
            if (toneEnabled && noiseEnabled)
                shape = ChipWaveformPreviewShape::toneNoise;
            else if (toneEnabled)
                shape = ChipWaveformPreviewShape::pulse;
            else if (noiseEnabled)
                shape = ChipWaveformPreviewShape::noise;
            else
                shape = ChipWaveformPreviewShape::off;

            tooltip = juce::String("YM/AY channel ") + juce::String::charToString(static_cast<juce::juce_wchar>('A' + static_cast<int>(index)))
                + " mixer: " + ymChannelMixReadout(patch);
        }
        else
        {
            shape = ChipWaveformPreviewShape::noise;
            tooltip = "YM/AY shared noise generator: " + ymNoiseReadout(patch.control3);
        }
    }
    else if (mode == chipper::ChipMode::sn76489)
    {
        if (index < 3)
        {
            shape = ChipWaveformPreviewShape::pulse;
            tooltip = juce::String("SN76489 tone ") + juce::String(static_cast<int>(index + 1u)) + " square-wave channel.";
        }
        else
        {
            shape = ChipWaveformPreviewShape::noise;
            tooltip = "SN76489 noise channel: " + snNoiseModeReadout(patch);
        }
    }
    else if (mode == chipper::ChipMode::pokey)
    {
        const auto distortionChoice = std::clamp(patch.waveShape, 0, 4);
        if (distortionChoice == 0 || distortionChoice == 1)
            shape = ChipWaveformPreviewShape::pulse;
        else if (distortionChoice == 2 || distortionChoice == 3)
            shape = ChipWaveformPreviewShape::toneNoise;
        else
            shape = ChipWaveformPreviewShape::noise;

        tooltip = juce::String("POKEY channel ") + juce::String(static_cast<int>(index + 1u))
            + ": AUDF/AUDC/AUDV-style source preview."
            + "\nDistortion: " + waveShapeReadout(mode, patch.waveShape);
    }
    else if (mode == chipper::ChipMode::spc700)
    {
        const auto noiseActive = chipper::spc700NoiseModeForPatch(patch) > 1u;
        shape = noiseActive ? ChipWaveformPreviewShape::noise : wavetablePreviewShape(patch);
        tooltip = juce::String("SPC700-style sample voice ") + juce::String(static_cast<int>(index + 1u))
            + ": generated lo-fi sample shape preview."
            + "\nSample Shape: " + waveShapeReadout(mode, patch.waveShape)
            + "\nPitch / PMON: " + spc700PitchMotionReadout(patch)
            + "\nPlayback: " + spc700SamplePlaybackReadout(patch)
            + "\nEnvelope: " + spc700EnvelopeReadout(patch)
            + "\nNoise Source: " + spc700NoiseReadout(patch)
            + "\nEcho: " + spc700EchoReadout(patch)
            + "\n" + sampleSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::paula)
    {
        const auto channelShape = static_cast<int>(chipper::wavetableWaveShapeForChannel(mode, patch, index));
        switch (std::clamp(channelShape, 0, 4))
        {
            case 1: shape = ChipWaveformPreviewShape::saw; break;
            case 2:
            case 3: shape = ChipWaveformPreviewShape::triangle; break;
            case 4: shape = ChipWaveformPreviewShape::noise; break;
            case 0:
            default: shape = ChipWaveformPreviewShape::pulse; break;
        }
        tooltip = juce::String("Paula sample channel ") + juce::String(static_cast<int>(index + 1u))
            + ": 8-bit tracker sample shape with period playback."
            + "\nSample Shape: " + waveShapeReadout(mode, channelShape)
            + "\n" + sampleSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::huc6280)
    {
        const auto channelShape = static_cast<int>(chipper::huc6280WaveShapeForChannel(patch, index));
        shape = wavetablePreviewShapeForChoice(channelShape);
        tooltip = juce::String("HuC6280 wavetable channel ") + juce::String(static_cast<int>(index + 1u))
            + ": 32-sample wave RAM preview."
            + "\nWave Shape: " + waveShapeReadout(mode, channelShape)
            + "\n" + wavetableSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::namcoWsg)
    {
        const auto laneShape = static_cast<int>(chipper::wavetableWaveShapeForChannel(mode, patch, index));
        shape = wavetablePreviewShapeForChoice(laneShape);
        tooltip = juce::String("Namco WSG wavetable lane ") + juce::String(static_cast<int>(index + 1u))
            + ": 4-bit wave RAM preview."
            + "\nWave Shape: " + waveShapeReadout(mode, laneShape)
            + "\n" + wavetableSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::scc)
    {
        const auto channelShape = static_cast<int>(chipper::wavetableWaveShapeForChannel(mode, patch, index));
        shape = wavetablePreviewShapeForChoice(channelShape);
        tooltip = juce::String("Konami SCC wavetable channel ") + juce::String(static_cast<int>(index + 1u))
            + ": 32-byte wave RAM preview."
            + "\nWave Shape: " + waveShapeReadout(mode, channelShape)
            + "\n" + wavetableSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2413)
    {
        const auto opmNoiseActive = mode == chipper::ChipMode::ym2151
            && index == 7u
            && (chipper::ym2151NoiseRegisterForPatch(patch) & 0x80u) != 0u;

        if (opmNoiseActive)
        {
            shape = ChipWaveformPreviewShape::noise;
        }
        else switch (std::clamp(patch.waveShape, 0, 4))
        {
            case 1: shape = ChipWaveformPreviewShape::triangle; break;
            case 2: shape = ChipWaveformPreviewShape::saw; break;
            case 3: shape = ChipWaveformPreviewShape::combined; break;
            case 4: shape = ChipWaveformPreviewShape::stepped; break;
            case 0:
            default:
                shape = ChipWaveformPreviewShape::saw;
                break;
        }

        tooltip = juce::String(chipper::toString(mode)) + " exposed FM lane "
            + juce::String(static_cast<int>(index + 1u))
            + ": symbolic operator-output preview. The engine uses the active FM core/register adapter."
            + "\n" + fmSourceRegisterReadout(mode, patch, index);
        if (mode == chipper::ChipMode::ym2151 && index == 7u)
            tooltip += "\nOPM Noise: " + ym2151NoiseReadout(patch);
    }

    scope.setShape(shape, duty, sourceIsEnabled && shape != ChipWaveformPreviewShape::off);
    if (tooltip.isEmpty())
        tooltip = "Register-resolved source preview.";

    scope.setTooltip(withMidiCcForRole(tooltip, sourceRole(index)));
}

void ChipperAudioProcessorEditor::updateEnvelopeDecayReadout(chipper::ChipMode mode)
{
    envelopeDecayValueLabel.setText(envelopeDecayReadout(mode, parameterValue(chipper::parameters::id::envelopeDecay)), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateSidAdsrControls(bool shouldBeVisible)
{
    const auto patch = currentUiPatch(
        chipper::ChipMode::sid,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));

    juce::String combinedTooltip;
    for (auto& label : sidAdsrHeaderLabels)
        label.setVisible(false);

    std::array<std::array<int, sidAdsrFieldCount>, sidAdsrVoiceCount> resolvedNibbles {};

    for (size_t voice = 0; voice < sidAdsrVoiceCount; ++voice)
    {
        const auto attack = chipper::sidAttackNibbleForVoice(patch, voice);
        const auto decay = chipper::sidDecayNibbleForVoice(patch, voice);
        const auto sustain = chipper::sidSustainNibbleForVoice(patch, voice);
        const auto release = chipper::sidReleaseNibbleForVoice(patch, voice);
        resolvedNibbles[voice] = { attack, decay, sustain, release };
        sidEnvelopeVoiceLabels[voice].setVisible(shouldBeVisible);
        sidEnvelopePreviews[voice].setVisible(shouldBeVisible);
        sidEnvelopePreviews[voice].setSidAdsr(attack, decay, sustain, release);

        const auto tooltip = sidAdsrNibbleReadout(patch, voice)
            + "\nAttack approx " + juce::String(chipper::sidAttackSecondsForNibble(attack), 3) + " s"
            + " | Decay approx " + juce::String(chipper::sidDecayReleaseSecondsForNibble(decay), 3) + " s"
            + " | Release approx " + juce::String(chipper::sidDecayReleaseSecondsForNibble(release), 3) + " s";
        sidEnvelopeVoiceLabels[voice].setTooltip(tooltip);
        sidEnvelopePreviews[voice].setTooltip(tooltip + "\nPreview follows Chipper's current SID ADSR approximation for this voice.");

        if (! combinedTooltip.isEmpty())
            combinedTooltip += "\n";
        combinedTooltip += tooltip;
    }

    for (size_t i = 0; i < sidAdsrBoxes.size(); ++i)
    {
        const auto selected = static_cast<int>(std::round(parameterValue(sidAdsrParameterId(i))));
        const auto clamped = std::clamp(selected, 0, static_cast<int>(sidAdsrChoiceCount - 1u));
        const auto voice = i / sidAdsrFieldCount;
        const auto field = i % sidAdsrFieldCount;
        const auto resolvedNibble = resolvedNibbles[voice][field];
        const auto nibbleText = clamped == 0 ? juce::String("Preset -> ") + juce::String(resolvedNibble)
                                             : juce::String(clamped - 1);
        sidAdsrLabels[i].setVisible(shouldBeVisible);
        sidAdsrLabels[i].setTooltip(withMidiCcForRole(juce::String("SID ") + sidAdsrFieldLabel(i % sidAdsrFieldCount)
                                                          + " override: " + nibbleText,
                                                      sidAdsrRole(i)));
        sidAdsrValueLabels[i].setVisible(shouldBeVisible);
        sidAdsrValueLabels[i].setText(clamped == 0 ? juce::String("F") + juce::String(resolvedNibble)
                                                   : juce::String(clamped - 1),
                                      juce::dontSendNotification);
        sidAdsrValueLabels[i].setTooltip(withMidiCcForRole(juce::String("SID ") + sidAdsrFieldLabel(i % sidAdsrFieldCount)
                                                               + " override: " + nibbleText,
                                                           sidAdsrRole(i)));
        sidAdsrBoxes[i].setVisible(false);
        sidAdsrBoxes[i].setSelectedItemIndex(std::clamp(selected, 0, static_cast<int>(sidAdsrChoiceCount - 1u)), juce::dontSendNotification);
        sidAdsrSliders[i].setVisible(shouldBeVisible);
        sidAdsrSliders[i].setTooltip(withMidiCcForRole(juce::String("SID ") + sidAdsrFieldLabel(i % sidAdsrFieldCount)
                                                           + " override: " + nibbleText
                                                           + ". Preset uses the macro/control-resolved register nibble.",
                                                       sidAdsrRole(i)));
        sidAdsrSliders[i].setValue(clamped, juce::dontSendNotification);
    }

    if (shouldBeVisible)
        envelopeDecayValueLabel.setTooltip(withMidiCcForRole(combinedTooltip, chipper::ChipParameterRole::envelopeDecay));
}

void ChipperAudioProcessorEditor::updateFmOperatorRegisterSurface(chipper::ChipMode mode,
                                                                  const chipper::PatchConfig& patch,
                                                                  bool shouldBeVisible)
{
    const auto active = shouldBeVisible
        && chipper::descriptorFor(mode).implemented
        && (mode == chipper::ChipMode::ym2612
            || mode == chipper::ChipMode::opl3
            || mode == chipper::ChipMode::ym2151);
    const auto visibleRows = mode == chipper::ChipMode::opl3 ? 3u : fmOperatorReadoutRows;

    static constexpr std::array<const char*, fmOperatorReadoutRows> fmNames { "OP1", "OP2", "OP3", "OP4" };
    static constexpr std::array<const char*, fmOperatorReadoutRows> oplNames { "Pair", "Mod", "Car", "" };

    for (size_t i = 0; i < fmOperatorReadoutRows; ++i)
    {
        const auto visible = active && i < visibleRows;
        fmOperatorNameLabels[i].setVisible(visible);
        fmOperatorValueLabels[i].setVisible(visible);
        if (! visible)
            continue;

        fmOperatorNameLabels[i].setText(mode == chipper::ChipMode::opl3 ? oplNames[i] : fmNames[i], juce::dontSendNotification);
        fmOperatorValueLabels[i].setText(fmOperatorRegisterReadout(mode, patch, i), juce::dontSendNotification);

        const auto tooltip = fmOperatorRegisterTooltip(mode, i) + "\n" + fmOperatorRegisterReadout(mode, patch, i);
        fmOperatorNameLabels[i].setTooltip(tooltip);
        fmOperatorValueLabels[i].setTooltip(tooltip);
    }
}

void ChipperAudioProcessorEditor::updateStereoSpreadReadout(chipper::ChipMode mode)
{
    stereoSpreadValueLabel.setText(stereoSpreadReadout(mode, parameterValue(chipper::parameters::id::stereoSpread)), juce::dontSendNotification);
}

juce::String ChipperAudioProcessorEditor::envelopeDecayReadout(chipper::ChipMode mode, float value) const
{
    if (value <= 0.01f)
    {
        if (mode == chipper::ChipMode::pokey)
            return "Off: AUDV volume unchanged";
        if (mode == chipper::ChipMode::paula)
            return "Off: Paula volume unchanged";
        if (mode == chipper::ChipMode::huc6280)
            return "Off: native 5-bit volume";
        if (mode == chipper::ChipMode::scc)
            return "Off: native 4-bit volume";
        if (mode == chipper::ChipMode::namcoWsg)
            return "Off: native lane volume";
        if (mode == chipper::ChipMode::spc700)
            return "Off: S-DSP gain unchanged";

        return "Off: macro envelope/level unchanged";
    }

    if (mode == chipper::ChipMode::dmg)
    {
        const auto period = std::clamp(static_cast<int>(std::round(7.0f - (std::clamp(value, 0.0f, 1.0f) * 6.0f))), 1, 7);
        return juce::String("DMG 64 Hz decay, period ") + juce::String(period);
    }

    if (mode == chipper::ChipMode::ym2149)
    {
        const auto period = chipper::ym2149EnvelopePeriodForControl(value);
        if (value <= 0.01f)
            return juce::String("Default register period ") + juce::String(static_cast<int>(period));

        return juce::String("Reg 11/12 period ") + juce::String(static_cast<int>(period));
    }

    if (mode == chipper::ChipMode::sid)
    {
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        return sidAdsrNibbleReadout(patch, 0)
            + " | " + sidAdsrNibbleReadout(patch, 1)
            + " | " + sidAdsrNibbleReadout(patch, 2);
    }

    const auto period = std::clamp(static_cast<int>(std::round(15.0f - (std::clamp(value, 0.0f, 1.0f) * 14.0f))), 1, 15);
    if (mode == chipper::ChipMode::pokey)
        return juce::String("POKEY AUDV volume-gate helper, step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::paula)
        return juce::String("Paula 6-bit volume gate, step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::huc6280)
        return juce::String("Gate step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::scc)
        return juce::String("Gate step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::namcoWsg)
        return juce::String("Gate step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::spc700)
        return juce::String("S-DSP ADSR/GAIN speed, step ") + juce::String(period) + "/15";

    if (mode == chipper::ChipMode::nes)
        return juce::String("APU envelope decay, period ") + juce::String(period);
    if (mode == chipper::ChipMode::sn76489)
        return juce::String("PSG attenuation gate helper, step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::ym2612)
        return juce::String("OPN2 Operator EG speed, step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::opl3)
        return juce::String("OPL Operator EG speed, step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::ym2151)
        return juce::String("OPM Operator EG speed, step ") + juce::String(period) + "/15";
    if (mode == chipper::ChipMode::ym2413)
        return juce::String("OPLL ROM envelope speed, step ") + juce::String(period) + "/15";

    return juce::String("Volume gate helper, step ") + juce::String(period) + "/15";
}

void ChipperAudioProcessorEditor::updatePulseDutyButtons(float value, bool shouldBeVisible)
{
    const auto selected = static_cast<size_t>(std::clamp(static_cast<int>(std::round(value * 3.0f)), 0, 3));
    for (size_t i = 0; i < pulseDutyButtons.size(); ++i)
    {
        pulseDutyButtons[i].setVisible(shouldBeVisible);
        pulseDutyButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }
}

void ChipperAudioProcessorEditor::updatePulse2DutyButtons(const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const auto* spec = chipper::parameterSpecFor(displayedMode, chipper::ChipParameterRole::pulse2Duty);
    const auto choiceCount = spec == nullptr || spec->choices.empty()
                                 ? pulse2DutyButtons.size()
                                 : std::min(pulse2DutyButtons.size(), spec->choices.size());
    const auto selected = static_cast<size_t>(std::clamp(patch.pulse2Duty, 0, static_cast<int>(choiceCount - 1u)));
    layoutSegmentedButtons(pulse2DutyButtons, pulse2DutySegmentBounds, choiceCount);
    for (size_t i = 0; i < pulse2DutyButtons.size(); ++i)
    {
        const auto visible = shouldBeVisible && i < choiceCount;
        pulse2DutyButtons[i].setVisible(visible);
        pulse2DutyButtons[i].setToggleState(visible && i == selected, juce::dontSendNotification);
    }

    const auto embeddedInSourceCard = displayedMode == chipper::ChipMode::nes
        || displayedMode == chipper::ChipMode::dmg;
    pulse2DutyLabel.setVisible(shouldBeVisible);
    pulse2DutyValueLabel.setVisible(shouldBeVisible);
    pulse2DutyValueLabel.setText(embeddedInSourceCard ? compactPulse2DutyReadout(patch) : pulse2DutyReadout(patch),
                                 juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateToneNoiseMixButtons(float value, bool shouldBeVisible)
{
    const auto mixer = chipper::ym2149MixerRegisterForControl(value);
    const auto selected = mixer == 0x07u ? size_t { 0u } : (mixer == 0x00u ? size_t { 2u } : size_t { 1u });
    layoutSegmentedButtons(toneNoiseMixButtons, toneNoiseMixSegmentBounds, toneNoiseMixButtons.size());
    for (size_t i = 0; i < toneNoiseMixButtons.size(); ++i)
    {
        toneNoiseMixButtons[i].setVisible(shouldBeVisible);
        toneNoiseMixButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }
}

void ChipperAudioProcessorEditor::updateWaveShapeButtons(int choice, bool shouldBeVisible)
{
    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    if (mode == chipper::ChipMode::sid)
    {
        for (auto& button : waveShapeButtons)
            button.setVisible(false);
        fmAlgorithmBox.setVisible(false);
        fmAlgorithmPreview.setVisible(false);
        oplWaveformBox.setVisible(false);
        oplWaveformPreview.setVisible(false);
        opllInstrumentBox.setVisible(false);
        updateSidVoiceWaveControls(shouldBeVisible);
        return;
    }
    if (mode == chipper::ChipMode::huc6280
        || mode == chipper::ChipMode::namcoWsg
        || mode == chipper::ChipMode::scc
        || mode == chipper::ChipMode::paula
        || mode == chipper::ChipMode::spc700)
    {
        for (auto& button : waveShapeButtons)
            button.setVisible(false);
        fmAlgorithmBox.setVisible(false);
        fmAlgorithmPreview.setVisible(false);
        oplWaveformBox.setVisible(false);
        oplWaveformPreview.setVisible(false);
        opllInstrumentBox.setVisible(false);
        updateHucVoiceWaveControls(shouldBeVisible || usesChannelLocalWaveDeck(mode));
        return;
    }
    if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151)
    {
        for (auto& button : waveShapeButtons)
            button.setVisible(false);
        updateFmAlgorithmControl(mode, choice, shouldBeVisible);
        return;
    }
    if (mode == chipper::ChipMode::opl3)
    {
        for (auto& button : waveShapeButtons)
            button.setVisible(false);
        updateOplWaveformControl(mode, choice, shouldBeVisible);
        return;
    }
    if (mode == chipper::ChipMode::ym2413)
    {
        for (auto& button : waveShapeButtons)
            button.setVisible(false);
        updateOpllInstrumentControl(mode, choice, shouldBeVisible);
        return;
    }

    fmAlgorithmBox.setVisible(false);
    fmAlgorithmPreview.setVisible(false);
    oplWaveformBox.setVisible(false);
    oplWaveformPreview.setVisible(false);
    opllInstrumentBox.setVisible(false);

    const auto selected = static_cast<size_t>(std::clamp(choice, 0, static_cast<int>(waveShapeButtons.size() - 1u)));
    for (size_t i = 0; i < waveShapeButtons.size(); ++i)
    {
        waveShapeButtons[i].setVisible(shouldBeVisible);
        waveShapeButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }

    const auto embeddedInSourceCard = mode == chipper::ChipMode::nes
        || mode == chipper::ChipMode::dmg;
    waveShapeLabel.setVisible(shouldBeVisible);
    waveShapeValueLabel.setVisible(shouldBeVisible && ! embeddedInSourceCard);
    waveShapeValueLabel.setText(waveShapeReadout(mode, static_cast<int>(selected)), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateFmAlgorithmControl(chipper::ChipMode mode, int choice, bool shouldBeVisible)
{
    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::waveShape);
    const auto isFourOperatorFm = mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151;
    const auto visible = shouldBeVisible && isFourOperatorFm && spec != nullptr;
    waveShapeLabel.setVisible(visible);
    waveShapeValueLabel.setVisible(visible);
    fmAlgorithmBox.setVisible(visible);
    fmAlgorithmPreview.setVisible(visible);
    for (auto& button : waveShapeButtons)
        button.setVisible(false);

    if (! visible)
        return;

    const auto safeChoice = std::clamp(choice, 0, static_cast<int>(spec->choices.size() - 1u));
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);
    fmAlgorithmBox.setSelectedId(safeChoice + 1, juce::dontSendNotification);

    const auto patch = currentUiPatch(
        mode,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        safeChoice,
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));

    const auto resolvedAlgorithm = static_cast<int>(mode == chipper::ChipMode::ym2151
                                                        ? chipper::ym2151AlgorithmForPatch(patch)
                                                        : chipper::ym2612AlgorithmForPatch(patch));
    const auto followsTemplate = safeChoice == 0;
    fmAlgorithmPreview.setAlgorithm(resolvedAlgorithm, followsTemplate);

    const auto valueText = followsTemplate
        ? juce::String("Preset -> Alg ") + juce::String(resolvedAlgorithm)
        : juce::String("Alg ") + juce::String(resolvedAlgorithm);
    const auto registerText = juce::String(mode == chipper::ChipMode::ym2151 ? "$20 bits alg=" : "$B0 bits alg=")
        + juce::String(resolvedAlgorithm)
        + ", fb=" + juce::String(static_cast<int>(chipper::fmFeedbackForPatch(patch)));

    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setText(valueText, juce::dontSendNotification);
    waveShapeValueLabel.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + registerText, spec->role));
    fmAlgorithmBox.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + registerText, spec->role));
    fmAlgorithmPreview.setTooltip(juce::String(mode == chipper::ChipMode::ym2151
                                                   ? "YM2151/OPM four-operator algorithm signal flow.\n"
                                                   : "YM2612/OPN2 four-operator algorithm signal flow.\n")
                                  + registerText
                                  + "\nCyan operators modulate; yellow operators reach output.");
}

void ChipperAudioProcessorEditor::updateOplWaveformControl(chipper::ChipMode mode, int choice, bool shouldBeVisible)
{
    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::waveShape);
    const auto visible = shouldBeVisible && mode == chipper::ChipMode::opl3 && spec != nullptr;
    waveShapeLabel.setVisible(visible);
    waveShapeValueLabel.setVisible(visible);
    oplWaveformBox.setVisible(visible);
    oplWaveformPreview.setVisible(visible);
    for (auto& button : waveShapeButtons)
        button.setVisible(false);

    if (! visible)
        return;

    const auto safeChoice = std::clamp(choice, 0, static_cast<int>(spec->choices.size() - 1u));
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);
    oplWaveformBox.setSelectedId(safeChoice + 1, juce::dontSendNotification);

    const auto patch = currentUiPatch(
        mode,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        safeChoice,
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));

    const auto resolvedWaveform = static_cast<int>(chipper::oplWaveformForPatch(patch));
    const auto followsTemplate = safeChoice == 0;
    oplWaveformPreview.setWaveform(resolvedWaveform, followsTemplate);

    const auto valueText = followsTemplate
        ? juce::String("Preset -> W") + juce::String(resolvedWaveform)
        : juce::String("W") + juce::String(resolvedWaveform);
    const auto registerText = juce::String("$E0 operator waveform=") + juce::String(resolvedWaveform)
        + ", FB=" + juce::String(static_cast<int>(chipper::fmFeedbackForPatch(patch)))
        + ", CNT=" + juce::String(static_cast<int>(chipper::oplConnectionForPatch(patch)));

    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setText(valueText, juce::dontSendNotification);
    waveShapeValueLabel.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + registerText, spec->role));
    oplWaveformBox.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + registerText, spec->role));
    oplWaveformPreview.setTooltip("OPL2/YM3812 operator waveform preview.\n"
                                  + registerText
                                  + "\nCurrent pass writes the selected waveform to both operators of each two-operator voice.");
}

void ChipperAudioProcessorEditor::updateOpllInstrumentControl(chipper::ChipMode mode, int choice, bool shouldBeVisible)
{
    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::waveShape);
    const auto visible = shouldBeVisible && mode == chipper::ChipMode::ym2413 && spec != nullptr;
    waveShapeLabel.setVisible(visible);
    waveShapeValueLabel.setVisible(visible);
    opllInstrumentBox.setVisible(visible);
    for (auto& button : waveShapeButtons)
        button.setVisible(false);
    fmAlgorithmBox.setVisible(false);
    fmAlgorithmPreview.setVisible(false);
    oplWaveformBox.setVisible(false);
    oplWaveformPreview.setVisible(false);

    if (! visible)
        return;

    const auto safeChoice = std::clamp(choice, 0, static_cast<int>(spec->choices.size() - 1u));
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);
    opllInstrumentBox.setSelectedId(safeChoice + 1, juce::dontSendNotification);

    const auto patch = currentUiPatch(
        mode,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        safeChoice,
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));

    const auto resolvedInstrument = static_cast<int>(chipper::ym2413InstrumentForPatch(patch));
    const auto volumeNibble = static_cast<int>(chipper::ym2413VolumeNibbleForPatch(patch, 0));
    const auto followsTemplate = safeChoice == 0;
    const auto valueText = followsTemplate
        ? juce::String("Preset -> I") + juce::String(resolvedInstrument)
        : juce::String("I") + juce::String(resolvedInstrument);
    const auto registerText = juce::String("$30 instrument=") + juce::String(resolvedInstrument)
        + ", volume nibble=" + juce::String(volumeNibble) + "/15";

    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setText(valueText, juce::dontSendNotification);
    waveShapeValueLabel.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + registerText, spec->role));
    opllInstrumentBox.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + registerText, spec->role));
}

void ChipperAudioProcessorEditor::updateSidVoiceWaveControls(bool shouldBeVisible)
{
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);

    waveShapeLabel.setVisible(false);
    waveShapeValueLabel.setVisible(false);

    const auto patch = currentUiPatch(
        chipper::ChipMode::sid,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));

    for (size_t i = 0; i < sidVoiceWaveBoxes.size(); ++i)
    {
        const auto selected = static_cast<int>(std::round(parameterValue(sidVoiceWaveParameterId(i))));
        const auto bits = chipper::sidWaveformControlForVoice(patch, i);
        const auto maxChoice = std::max(0, sidVoiceWaveBoxes[i].getNumItems() - 1);
        const auto displayChoice = selected == 0 ? 0 : selected;
        sidVoiceWaveLabels[i].setVisible(shouldBeVisible);
        sidVoiceWaveBoxes[i].setVisible(shouldBeVisible);
        sidVoiceWaveBoxes[i].setSelectedId(std::clamp(displayChoice, 0, maxChoice) + 1, juce::dontSendNotification);

        if (shouldBeVisible)
        {
            const auto tooltip = juce::String("SID voice waveform register choice.")
                + "\nResolved waveform: " + sidWaveNameForControlBits(bits)
                + "\nCTRL waveform bits 0x" + byteHex(bits);
            sidVoiceWaveLabels[i].setTooltip(withMidiCcForRole(tooltip, sidVoiceWaveRole(i)));
            sidVoiceWaveBoxes[i].setTooltip(withMidiCcForRole(tooltip, sidVoiceWaveRole(i)));
        }
    }

    if (! shouldBeVisible)
        return;

    waveShapeValueLabel.setText(sidVoiceWaveSummary(patch), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateHucVoiceWaveControls(bool shouldBeVisible)
{
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);

    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto isNamco = mode == chipper::ChipMode::namcoWsg;
    const auto isScc = mode == chipper::ChipMode::scc;
    const auto isPaula = mode == chipper::ChipMode::paula;
    const auto isSpc700 = mode == chipper::ChipMode::spc700;
    const auto visibleCount = isSpc700 ? size_t { 8u } : (isNamco ? size_t { 8u } : (isScc ? size_t { 5u } : (isPaula ? size_t { 4u } : size_t { 6u })));
    const auto laneName = isNamco ? juce::String("Lane") : (isSpc700 ? juce::String("Voice") : juce::String("Ch"));
    waveShapeLabel.setText(isSpc700 ? "Voice Samples" : (isPaula ? "Channel Samples" : (isNamco ? "Lane Waves" : "Channel Waves")), juce::dontSendNotification);
    waveShapeValueLabel.setJustificationType(juce::Justification::centredLeft);
    waveShapeLabel.setVisible(false);
    waveShapeValueLabel.setVisible(false);

    const auto patch = currentUiPatch(
        mode,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));

    juce::String summary;
    const auto spc700SampleNames = isSpc700 ? audioProcessor.spc700BrrSampleNames() : juce::StringArray {};
    const auto paulaSampleNames = isPaula ? audioProcessor.paulaSampleNames() : juce::StringArray {};
    for (size_t i = 0; i < hucVoiceWaveBoxes.size(); ++i)
    {
        auto& box = hucVoiceWaveBoxes[i];
        if (isSpc700)
        {
            box.clear(juce::dontSendNotification);
            box.addItem("Preset", 1);
            for (int slot = 0; slot < spc700SampleNames.size(); ++slot)
                box.addItem(juce::String(slot + 1).paddedLeft('0', 2) + " " + compactSampleName(spc700SampleNames[slot], 18), slot + 2);
        }
        else
        {
            const auto needsPaulaItems = isPaula && box.getItemText(3) != "Sine";
            const auto needsWavetableItems = ! isPaula && box.getItemText(3) != "Pulse";
            if (needsPaulaItems || needsWavetableItems)
            {
                box.clear(juce::dontSendNotification);
                box.addItemList(isPaula
                                    ? juce::StringArray { "Preset", "Ramp", "Tri", "Sine", "Noise" }
                                    : juce::StringArray { "Preset", "Ramp", "Tri", "Pulse", "Noise" },
                                1);
            }
        }

        const auto selected = isSpc700
            ? std::clamp(static_cast<int>(std::round(parameterValue(spc700VoiceSampleParameterId(i)))), 0, 32)
            : std::clamp(static_cast<int>(std::round(parameterValue(hucVoiceWaveParameterId(i)))), 0, 4);
        if (isSpc700 && selected > spc700SampleNames.size())
            box.addItem("Slot " + juce::String(selected).paddedLeft('0', 2) + " (not loaded)", selected + 1);

        const auto resolved = isSpc700 ? selected : static_cast<int>(chipper::wavetableWaveShapeForChannel(mode, patch, i));
        const auto maxChoice = std::max(0, box.getNumItems() - 1);
        const auto visible = shouldBeVisible && i < visibleCount;
        hucVoiceWaveLabels[i].setText(isSpc700 ? juce::String("Sample")
                                                : (isPaula ? juce::String("Shape") : laneName + " " + juce::String(static_cast<int>(i + 1u))),
                                      juce::dontSendNotification);
        hucVoiceWaveLabels[i].setVisible(visible);
        box.setVisible(visible);
        box.setSelectedId(std::clamp(selected, 0, maxChoice) + 1, juce::dontSendNotification);

        if (isSpc700)
        {
            const auto resolvedText = selected <= 0
                ? juce::String("Shared manual slot / note map")
                : (selected <= spc700SampleNames.size()
                       ? juce::String("Slot ") + juce::String(selected).paddedLeft('0', 2) + ": " + spc700SampleNames[selected - 1]
                       : juce::String("Slot ") + juce::String(selected).paddedLeft('0', 2) + " is not loaded");
            const auto tooltip = juce::String("SPC700 voice ") + juce::String(static_cast<int>(i + 1u))
                + " sample source.\nPreset uses the global manual slot or note map; slots 1-32 pin this voice to a loaded sample.\n"
                + resolvedText;
            hucVoiceWaveLabels[i].setTooltip(withMidiCcForRole(tooltip, spc700VoiceSampleRole(i)));
            box.setTooltip(withMidiCcForRole(tooltip, spc700VoiceSampleRole(i)));
        }
        else
        {
            const auto resolvedText = waveShapeReadout(mode, resolved);
            const auto tooltip = juce::String(isNamco ? "Namco WSG lane " : (isScc ? "SCC channel " : (isPaula ? "Paula channel " : "HuC6280 channel "))) + juce::String(static_cast<int>(i + 1u))
                + (isPaula ? " generated sample shape." : " wave RAM shape.")
                + "\nResolved: " + resolvedText
                + "\n" + (isPaula ? sampleSourceRegisterReadout(mode, patch, i) : wavetableSourceRegisterReadout(mode, patch, i));
            hucVoiceWaveLabels[i].setTooltip(withMidiCcForRole(tooltip, hucVoiceWaveRole(i)));
            box.setTooltip(withMidiCcForRole(tooltip, hucVoiceWaveRole(i)));
        }

        if (isPaula && i < paulaVoiceSampleBoxes.size())
        {
            auto& sampleLabel = paulaVoiceSampleLabels[i];
            auto& sampleBox = paulaVoiceSampleBoxes[i];
            sampleBox.clear(juce::dontSendNotification);
            sampleBox.addItem("Shared Bank", 1);
            for (int slot = 0; slot < paulaSampleNames.size(); ++slot)
                sampleBox.addItem(juce::String(slot + 1).paddedLeft('0', 2) + " " + compactSampleName(paulaSampleNames[slot], 18), slot + 2);

            const auto sampleSlot = std::clamp(static_cast<int>(std::round(parameterValue(spc700VoiceSampleParameterId(i)))), 0, 32);
            if (sampleSlot > paulaSampleNames.size())
                sampleBox.addItem("Slot " + juce::String(sampleSlot).paddedLeft('0', 2) + " (not loaded)", sampleSlot + 1);

            const auto sampleMaxChoice = std::max(0, sampleBox.getNumItems() - 1);
            sampleBox.setSelectedId(std::clamp(sampleSlot, 0, sampleMaxChoice) + 1, juce::dontSendNotification);
            sampleLabel.setText("Sample", juce::dontSendNotification);
            sampleLabel.setVisible(visible);
            sampleBox.setVisible(visible);

            const auto resolvedSampleText = sampleSlot <= 0
                ? juce::String("Follow the global Paula manual slot or note map")
                : (sampleSlot <= paulaSampleNames.size()
                       ? juce::String("Slot ") + juce::String(sampleSlot).paddedLeft('0', 2) + ": " + paulaSampleNames[sampleSlot - 1]
                       : juce::String("Slot ") + juce::String(sampleSlot).paddedLeft('0', 2) + " is not loaded");
            const auto sampleTooltip = juce::String("Paula channel ") + juce::String(static_cast<int>(i + 1u))
                + " external sample source.\nShared Bank uses the shared manual slot or note map; slots 1-32 pin this DAC channel to a loaded sample.\n"
                + resolvedSampleText;
            sampleLabel.setTooltip(withMidiCcForRole(sampleTooltip, spc700VoiceSampleRole(i)));
            sampleBox.setTooltip(withMidiCcForRole(sampleTooltip, spc700VoiceSampleRole(i)));
        }

        if (i < ((isSpc700 || isNamco || isPaula) ? 4u : 3u))
        {
            if (summary.isNotEmpty())
                summary += " | ";
            summary += laneName + juce::String(static_cast<int>(i + 1u)) + " "
                + box.getText();
        }
    }

    waveShapeValueLabel.setText(summary.isNotEmpty() ? summary : (isSpc700 ? juce::String("Per-voice SPC700 sample slots") : (isNamco ? juce::String("Per-lane Namco WSG wave RAM shapes") : (isScc ? juce::String("Per-channel SCC wave RAM shapes") : (isPaula ? juce::String("Per-channel Paula sample shapes") : juce::String("Per-channel HuC wave RAM shapes"))))), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateSidVoicePulseWidthControls(const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    for (size_t i = 0; i < sidVoicePulseWidthSliders.size(); ++i)
    {
        const auto visible = shouldBeVisible && i < sidVoiceWaveCount;
        sidVoicePulseWidthLabels[i].setVisible(visible);
        sidVoicePulseWidthSliders[i].setVisible(visible);
        sidVoicePulseWidthValueLabels[i].setVisible(visible);

        if (! visible)
            continue;

        const auto readout = sidVoicePulseWidthReadout(patch, i);
        const auto tooltip = juce::String("SID voice ") + juce::String(static_cast<int>(i + 1u))
            + " pulse width\n" + readout;
        const auto pulseWidth = chipper::sidPulseWidthForVoice(patch, i);
        const auto shortText = juce::String("0x")
            + juce::String::toHexString(static_cast<int>(pulseWidth)).paddedLeft('0', 3).toUpperCase();

        sidVoicePulseWidthValueLabels[i].setText(shortText, juce::dontSendNotification);
        sidVoicePulseWidthLabels[i].setTooltip(withMidiCcForRole(tooltip, sidVoicePulseWidthRole(i)));
        sidVoicePulseWidthSliders[i].setTooltip(withMidiCcForRole(tooltip, sidVoicePulseWidthRole(i)));
        sidVoicePulseWidthValueLabels[i].setTooltip(withMidiCcForRole(tooltip, sidVoicePulseWidthRole(i)));
    }
}

void ChipperAudioProcessorEditor::updateDmgWaveLevelButtons(const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const auto selected = static_cast<size_t>(std::clamp(patch.dmgWaveLevel, 0, static_cast<int>(dmgWaveLevelButtons.size() - 1u)));
    for (size_t i = 0; i < dmgWaveLevelButtons.size(); ++i)
    {
        dmgWaveLevelButtons[i].setVisible(shouldBeVisible);
        dmgWaveLevelButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }

    const auto embeddedInSourceCard = displayedMode == chipper::ChipMode::dmg;
    dmgWaveLevelLabel.setVisible(shouldBeVisible);
    dmgWaveLevelValueLabel.setVisible(shouldBeVisible && ! embeddedInSourceCard);
    dmgWaveLevelValueLabel.setText(dmgWaveLevelReadout(patch), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateDmgStereoRouteButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);

    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::dmgStereoRoute);
    const auto choiceCount = spec == nullptr || spec->choices.empty()
                                 ? dmgStereoRouteButtons.size()
                                 : std::min(dmgStereoRouteButtons.size(), spec->choices.size());
    const auto selected = static_cast<size_t>(std::clamp(patch.dmgStereoRoute, 0, static_cast<int>(choiceCount - 1u)));
    const auto menuActive = spec != nullptr && spec->surface == chipper::ControlSurface::menu;
    layoutSegmentedButtons(dmgStereoRouteButtons, dmgStereoRouteSegmentBounds, choiceCount);
    for (size_t i = 0; i < dmgStereoRouteButtons.size(); ++i)
    {
        const auto visible = shouldBeVisible && ! menuActive && i < choiceCount;
        dmgStereoRouteButtons[i].setVisible(visible);
        dmgStereoRouteButtons[i].setToggleState(visible && i == selected, juce::dontSendNotification);
    }

    const auto spc700LoopToggleActive = shouldBeVisible && mode == chipper::ChipMode::spc700;

    dmgStereoRouteBox.setVisible(shouldBeVisible && menuActive && ! spc700LoopToggleActive);
    if (menuActive)
    {
        dmgStereoRouteBox.setSelectedId(static_cast<int>(selected) + 1, juce::dontSendNotification);
        if (spec != nullptr)
            dmgStereoRouteBox.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + spc700SamplePlaybackReadout(patch), spec->role));
    }
    spc700LoopModeButton.setVisible(spc700LoopToggleActive);
    if (spc700LoopToggleActive)
    {
        const auto loops = chipper::spc700SampleLoopsForPatch(patch);
        spc700LoopModeButton.setToggleState(loops, juce::dontSendNotification);
        spc700LoopModeButton.setButtonText(loops ? "Loop While Held" : "One Shot");
        if (spec != nullptr)
            spc700LoopModeButton.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + spc700SamplePlaybackReadout(patch), spec->role));
    }

    juce::String routeReadout;
    if (mode == chipper::ChipMode::sid)
        routeReadout = sidModelReadout(patch);
    else if (mode == chipper::ChipMode::spc700)
        routeReadout = spc700SamplePlaybackReadout(patch);
    else if (mode == chipper::ChipMode::ym2612)
        routeReadout = ym2612PanReadout(patch);
    else if (mode == chipper::ChipMode::ym2151)
        routeReadout = ym2151PanReadout(patch);
    else
        routeReadout = dmgStereoRouteReadout(patch);
    const auto compactFmPan = mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151;
    dmgStereoRouteValueLabel.setVisible(shouldBeVisible && ! compactFmPan);
    dmgStereoRouteValueLabel.setText(routeReadout, juce::dontSendNotification);
    dmgStereoRouteLabel.setTooltip(withMidiCcForRole((spec != nullptr ? juce::String(spec->help) : juce::String("Chip stereo route."))
                                                       + "\n" + routeReadout,
                                                   chipper::ChipParameterRole::dmgStereoRoute));
}

void ChipperAudioProcessorEditor::updateYmEnvelopeShapeButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);

    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::ymEnvelopeShape);
    const auto visibleCount = spec != nullptr
                                  ? std::min(ymEnvelopeShapeButtons.size(), spec->choices.size())
                                  : ymEnvelopeShapeButtons.size();
    const auto safeVisibleCount = std::max<size_t>(1u, visibleCount);
    const auto selected = static_cast<size_t>(std::clamp(patch.ymEnvelopeShape, 0, static_cast<int>(safeVisibleCount - 1u)));

    if (spec != nullptr && spec->surface == chipper::ControlSurface::menu)
    {
        for (auto& button : ymEnvelopeShapeButtons)
            button.setVisible(false);

        sidFilterModeBox.setVisible(shouldBeVisible);
        sidFilterModeBox.setSelectedItemIndex(static_cast<int>(selected), juce::dontSendNotification);
        ymEnvelopeShapeValueLabel.setVisible(shouldBeVisible);
        ymEnvelopeShapeValueLabel.setText(mode == chipper::ChipMode::sid
                                              ? sidFilterModeReadout(patch)
                                              : (mode == chipper::ChipMode::pokey
                                                     ? pokeyAudctlFilterReadout(patch)
                                                     : ymEnvelopeShapeReadout(static_cast<int>(selected))),
                                          juce::dontSendNotification);
        updateYmEnvelopePreview(mode, patch, shouldBeVisible);
        return;
    }

    sidFilterModeBox.setVisible(false);

    if (! ymEnvelopeShapeSegmentBounds.isEmpty())
        layoutSegmentedButtons(ymEnvelopeShapeButtons, ymEnvelopeShapeSegmentBounds, safeVisibleCount);

    for (size_t i = 0; i < ymEnvelopeShapeButtons.size(); ++i)
    {
        ymEnvelopeShapeButtons[i].setVisible(shouldBeVisible && i < safeVisibleCount);
        ymEnvelopeShapeButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }

    ymEnvelopeShapeValueLabel.setVisible(shouldBeVisible);
    ymEnvelopeShapeValueLabel.setText(mode == chipper::ChipMode::sid
                                          ? sidFilterModeReadout(patch)
                                          : (mode == chipper::ChipMode::pokey
                                                 ? pokeyAudctlFilterReadout(patch)
                                                 : ymEnvelopeShapeReadout(static_cast<int>(selected))),
                                      juce::dontSendNotification);

    updateYmEnvelopePreview(mode, patch, shouldBeVisible);
}

void ChipperAudioProcessorEditor::updateYmEnvelopePreview(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const auto active = shouldBeVisible
        && mode == chipper::ChipMode::ym2149
        && chipper::descriptorFor(mode).implemented;
    ymEnvelopePreview.setVisible(active);
    if (! active)
        return;

    const auto envelopeEnabled = patch.ymEnvelopeShape > 0;
    const auto shapeCode = chipper::ym2149EnvelopeShapeCodeForChoice(patch.ymEnvelopeShape);
    ymEnvelopePreview.setYmEnvelope(shapeCode, envelopeEnabled);
    ymEnvelopePreview.setTooltip(withMidiCcForRole(juce::String("YM/AY envelope preview: ")
                                                       + ymEnvelopeShapeReadout(patch.ymEnvelopeShape)
                                                       + "\nRegister 13 shape code 0x" + byteHex(shapeCode)
                                                       + (envelopeEnabled ? "\nEnvelope volume bit enabled on active channels."
                                                                          : "\nFixed-volume mode; envelope bit is off."),
                                                   chipper::ChipParameterRole::ymEnvelopeShape));
}

void ChipperAudioProcessorEditor::updateSidFilterRoutingControl(bool shouldBeVisible)
{
    sidFilterRoutingLabel.setVisible(shouldBeVisible);
    sidFilterRoutingBox.setVisible(shouldBeVisible);
    sidFilterRoutingValueLabel.setVisible(shouldBeVisible);

    if (! shouldBeVisible)
        return;

    const auto selected = static_cast<int>(std::round(parameterValue(chipper::parameters::id::sidFilterRouting)));
    sidFilterRoutingBox.setSelectedItemIndex(std::clamp(selected, 0, static_cast<int>(sidFilterRoutingChoiceCount - 1u)), juce::dontSendNotification);

    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto patch = currentUiPatch(
        mode,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));
    sidFilterRoutingValueLabel.setText(sidFilterRoutingReadout(patch), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateYmChannelMixControls(bool shouldBeVisible)
{
    const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);

    for (size_t i = 0; i < ymChannelMixBoxes.size(); ++i)
    {
        const auto selected = static_cast<int>(std::round(parameterValue(ymChannelMixParameterId(i))));
        ymChannelMixLabels[i].setVisible(shouldBeVisible);
        ymChannelMixBoxes[i].setVisible(shouldBeVisible);
        ymChannelMixBoxes[i].setSelectedItemIndex(std::clamp(selected, 0, 4), juce::dontSendNotification);
    }

    ymChannelMixLabel.setVisible(shouldBeVisible);
    ymChannelMixValueLabel.setVisible(shouldBeVisible);
    if (! shouldBeVisible)
        return;

    const auto patch = currentUiPatch(
        chipper::ChipMode::ym2149,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));
    ymChannelMixValueLabel.setText(ymChannelMixReadout(patch), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateSnNoiseModeButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::snNoiseMode);
    const auto choiceCount = spec == nullptr || spec->choices.empty()
                                 ? snNoiseModeButtons.size()
                                 : std::min(snNoiseModeButtons.size(), spec->choices.size());
    const auto selected = static_cast<size_t>(std::clamp(patch.snNoiseMode, 0, static_cast<int>(choiceCount - 1u)));
    const auto menuActive = spec != nullptr && spec->surface == chipper::ControlSurface::menu;
    layoutSegmentedButtons(snNoiseModeButtons, snNoiseModeSegmentBounds, choiceCount);
    for (size_t i = 0; i < snNoiseModeButtons.size(); ++i)
    {
        const auto visible = shouldBeVisible && ! menuActive && i < choiceCount;
        snNoiseModeButtons[i].setVisible(visible);
        snNoiseModeButtons[i].setToggleState(visible && i == selected, juce::dontSendNotification);
    }

    snNoiseModeBox.setVisible(shouldBeVisible && menuActive);
    if (menuActive)
    {
        const juce::ScopedValueSetter<bool> suppressChoices(suppressManualChoiceCallbacks, true);
        snNoiseModeBox.setSelectedId(static_cast<int>(selected) + 1, juce::dontSendNotification);
        if (spec != nullptr)
            snNoiseModeBox.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + noiseModeReadout(mode, patch), spec->role));
    }

    const auto modeReadout = noiseModeReadout(mode, patch);
    const auto compactFmMode = mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151;
    const auto embeddedInSourceCard = mode == chipper::ChipMode::nes || mode == chipper::ChipMode::dmg;
    const auto registerSegmentMode = mode == chipper::ChipMode::nes || mode == chipper::ChipMode::dmg;
    snNoiseModeLabel.setVisible(shouldBeVisible);
    snNoiseModeValueLabel.setVisible(shouldBeVisible && ! embeddedInSourceCard && ! compactFmMode && ! registerSegmentMode);
    snNoiseModeValueLabel.setText(modeReadout, juce::dontSendNotification);
    snNoiseModeLabel.setTooltip(withMidiCcForRole((spec != nullptr ? juce::String(spec->help) : juce::String("Chip noise/DAC mode."))
                                                   + "\n" + modeReadout,
                                               chipper::ChipParameterRole::snNoiseMode));
}

void ChipperAudioProcessorEditor::updateDmcSampleControls()
{
    updateSamplePlaybackModeChoices(chipper::ChipMode::nes);
    dmcSampleLabel.setText("DMC", juce::dontSendNotification);
    dmcSampleLabel.setTooltip(withMidiCcForRole("Load one .dmc file or a folder of .dmc files. CC selects the active preloaded sample slot.", chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcSampleFileButton.setButtonText("File");
    dmcSampleFileButton.setTooltip("Load one user-provided .dmc file as a one-slot bank.");
    dmcSampleBankButton.setButtonText("Bank");
    dmcSampleBankButton.setTooltip("Open the DMC bank checklist. Checked files become the playable 32-slot dropdown, note map, and CC117 bank.");

    const auto names = audioProcessor.nesDmcSampleNames();
    const auto sampleCount = names.size();
    const auto revision = audioProcessor.nesDmcSampleRevision();
    const auto selectedSlot = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcSampleSlot)));
    const auto playbackMode = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcPlaybackMode)));
    const auto mapRoot = std::clamp(static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcMapRoot))), 0, 127);

    const juce::ScopedValueSetter<bool> suppress(suppressManualChoiceCallbacks, true);
    if (displayedSampleListMode != chipper::ChipMode::nes
        || sampleCount != displayedDmcSampleCount
        || revision != displayedDmcSampleRevision
        || playbackMode != displayedSamplePlaybackMode
        || mapRoot != displayedSampleMapRoot)
    {
        dmcSampleSlotBox.clear(juce::dontSendNotification);
        for (int i = 0; i < sampleCount; ++i)
        {
            auto itemLabel = juce::String(i + 1).paddedLeft('0', 2) + "  " + names[i];
            if (playbackMode != 0)
            {
                const auto note = mapRoot + i;
                const auto noteLabel = note <= 127 ? chipper::parameters::midiNoteChoices()[note] : juce::String("--");
                itemLabel = noteLabel + "  " + itemLabel;
            }
            dmcSampleSlotBox.addItem(itemLabel, i + 1);
        }

        displayedSampleListMode = chipper::ChipMode::nes;
        displayedDmcSampleCount = sampleCount;
        displayedDmcSampleRevision = revision;
        displayedSamplePlaybackMode = playbackMode;
        displayedSampleMapRoot = mapRoot;
    }

    if (sampleCount > 0)
        dmcSampleSlotBox.setSelectedId(std::clamp(selectedSlot, 0, sampleCount - 1) + 1, juce::dontSendNotification);
    else
        dmcSampleSlotBox.setSelectedId(0, juce::dontSendNotification);

    dmcSampleSlotBox.setEnabled(sampleCount > 0);
    const auto playbackInfo = audioProcessor.nesDmcSamplePlaybackInfo();
    auto visibleStatus = playbackInfo.byteCount > 0
        ? "Slot " + juce::String(playbackInfo.activeSlot + 1) + "/" + juce::String(playbackInfo.activeSlotCount)
            + ": " + compactSampleName(playbackInfo.sampleName)
        : playbackInfo.statusLine;
    if (playbackInfo.playbackMode != 0 && playbackInfo.activeSlotCount > 0)
        visibleStatus += " | Map " + chipper::parameters::midiNoteChoices()[playbackInfo.mapRootNote]
            + "-" + chipper::parameters::midiNoteChoices()[playbackInfo.mapHighNote];
    if (playbackInfo.byteCount > 0)
    {
        auto runState = juce::String(playbackInfo.loopEnabled ? "Loop" : "One-shot, DAC holds");
        if (! playbackInfo.loopEnabled)
        {
            if (playbackInfo.sampleCompleted)
                runState += " (stopped)";
            else if (playbackInfo.sampleActive)
                runState += " (playing)";
            else
                runState += " (ready)";
        }
        visibleStatus += " | Rate " + juce::String(playbackInfo.rateIndex) + " | " + runState;
    }
    dmcSampleStatusLabel.setText(visibleStatus, juce::dontSendNotification);
    dmcLoopButton.setButtonText("Loop Sample");
    auto sampleTooltip = playbackInfo.statusLine
        + "\nDMC bit clock: " + juce::String(playbackInfo.bitRateHz / 1000.0, 2) + " kHz from $4010 rate index "
        + juce::String(playbackInfo.rateIndex) + ".";
    if (playbackInfo.byteCount > 0)
    {
        sampleTooltip += "\nSample payload: " + juce::String(playbackInfo.byteCount) + " bytes / "
            + juce::String(playbackInfo.bitCount) + " DPCM bits, approx "
            + juce::String(playbackInfo.durationMs, playbackInfo.durationMs < 10.0 ? 1 : 0) + " ms at this rate.";
        if (! playbackInfo.loopEnabled)
        {
            sampleTooltip += "\nPlayback state: ";
            if (playbackInfo.sampleCompleted)
                sampleTooltip += "stopped at the sample end; the final DAC value is held.";
            else if (playbackInfo.sampleActive)
                sampleTooltip += "currently stepping DPCM bits.";
            else
                sampleTooltip += "ready for the next trigger.";
            if (playbackInfo.bitsPlayed > 0)
                sampleTooltip += " Bits stepped: " + juce::String(playbackInfo.bitsPlayed) + ".";
        }
    }
    sampleTooltip += playbackInfo.loopEnabled
        ? "\nLoop is on: $4010 bit 6 restarts playback from byte 0 when the sample ends."
        : "\nLoop is off: DPCM bit stepping stops at the sample end, and the NES DMC DAC keeps its final output level until retriggered.";
    sampleTooltip += "\nManual Slot plays the selected dropdown slot. Note Map maps checked slots upward from the DMC Map Root; Sample Map Only maps notes and suppresses pulse, triangle, and noise.";
    if (playbackInfo.playbackMode != 0 && playbackInfo.activeSlotCount > 0)
        sampleTooltip += "\nCurrent map span: "
            + chipper::parameters::midiNoteChoices()[playbackInfo.mapRootNote]
            + " to "
            + chipper::parameters::midiNoteChoices()[playbackInfo.mapHighNote]
            + ".";
    sampleTooltip += "\nFolder loads create a local checklist; checked entries become up to 32 CC117-addressable slots. WAV-to-DMC conversion is planned.";
    dmcSampleStatusLabel.setTooltip(withMidiCcForRole(sampleTooltip, chipper::ChipParameterRole::nesDmcSampleSlot));
    updateSampleWaveformPreview(chipper::ChipMode::nes);
}

void ChipperAudioProcessorEditor::updateSpc700BrrSampleControls()
{
    updateSamplePlaybackModeChoices(chipper::ChipMode::spc700);
    dmcSampleLabel.setText("SPC700 Sample", juce::dontSendNotification);
    const auto mapRoot = std::clamp(static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcMapRoot))), 0, 127);
    const auto playbackMode = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcPlaybackMode)));
    const auto playbackPatch = currentUiPatch(
        chipper::ChipMode::spc700,
        parameterValue(chipper::parameters::id::macroControl1),
        parameterValue(chipper::parameters::id::macroControl2),
        parameterValue(chipper::parameters::id::macroControl3),
        parameterValue(chipper::parameters::id::macroControl4),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
        static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
        parameterValue(chipper::parameters::id::stereoSpread));
    const auto resolvedPlaybackMode = chipper::spc700SamplePlaybackModeForPatch(playbackPatch);
    const auto bankModeLabel = playbackMode == 2 ? juce::String("Drum Map") : (playbackMode == 1 ? juce::String("Note Map") : juce::String("Manual Slot"));
    const auto lifetimeLabel = resolvedPlaybackMode == 1u ? juce::String("loop") : juce::String("one-shot");
    dmcSampleLabel.setTooltip(withMidiCcForRole("Load one user-provided SNES BRR, WAV, or AIFF sample, or a folder bank. CC117 selects the manual slot; Sample Playback chooses whether MIDI notes browse the loaded bank.", chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcSampleFileButton.setButtonText("File");
    dmcSampleFileButton.setTooltip("Load one user-provided .brr, WAV, or AIFF file into the SPC700-style sample voice model.");
    dmcSampleFolderButton.setButtonText("Folder");
    dmcSampleFolderButton.setTooltip("Load a folder of user-provided .brr, WAV, or AIFF files. The first 32 readable samples become CC117-addressable slots and note-mapped sample choices from C1 upward.");
    dmcSampleBankButton.setButtonText("Bank");
    dmcSampleBankButton.setTooltip("Open the SPC700 sample checklist. Checked files become the playable 32-slot dropdown and note map.");

    const auto names = audioProcessor.spc700BrrSampleNames();
    const auto sampleCount = names.size();
    const auto revision = audioProcessor.spc700BrrSampleRevision();
    const auto selectedSlot = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcSampleSlot)));

    const juce::ScopedValueSetter<bool> suppress(suppressManualChoiceCallbacks, true);
    if (displayedSampleListMode != chipper::ChipMode::spc700
        || sampleCount != displayedDmcSampleCount
        || revision != displayedDmcSampleRevision
        || playbackMode != displayedSamplePlaybackMode
        || mapRoot != displayedSampleMapRoot)
    {
        dmcSampleSlotBox.clear(juce::dontSendNotification);
        for (int i = 0; i < sampleCount; ++i)
        {
            auto itemLabel = juce::String(i + 1).paddedLeft('0', 2) + "  " + names[i];
            if (playbackMode != 0)
            {
                const auto note = mapRoot + i;
                const auto noteLabel = note <= 127 ? chipper::parameters::midiNoteChoices()[note] : juce::String("--");
                itemLabel = noteLabel + "  " + itemLabel;
            }
            dmcSampleSlotBox.addItem(itemLabel, i + 1);
        }

        displayedSampleListMode = chipper::ChipMode::spc700;
        displayedDmcSampleCount = sampleCount;
        displayedDmcSampleRevision = revision;
        displayedSamplePlaybackMode = playbackMode;
        displayedSampleMapRoot = mapRoot;
    }

    if (sampleCount > 0)
        dmcSampleSlotBox.setSelectedId(std::clamp(selectedSlot, 0, sampleCount - 1) + 1, juce::dontSendNotification);
    else
        dmcSampleSlotBox.setSelectedId(0, juce::dontSendNotification);

    dmcSampleSlotBox.setEnabled(sampleCount > 0);
    dmcSampleSlotBox.setTextWhenNothingSelected(sampleCount > 0 ? "Select SPC700 sample" : "Generated shape");
    dmcSampleSlotBox.setTooltip(withMidiCcForRole(
        sampleCount > 0
            ? "Selects the manual SPC700 sample from the loaded bank. MIDI CC117 selects the same slot; Manual playback uses it for every note."
            : "No external SPC700 bank is loaded. Chipper is playing the generated SPC700 sample shape selected by Sample Shape.",
        chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcPlaybackModeBox.setTooltip(withMidiCcForRole("SPC700 Sample Playback. Manual Slot plays the selected dropdown slot; Note Map maps loaded folder slots upward from the Sample Map Root. Drum Map uses the same bank mapping path for one-shot/percussion presets.", chipper::ChipParameterRole::nesDmcPlaybackMode));
    dmcMapRootBox.setTooltip(withMidiCcForRole("SPC700 Sample Map Root. Loaded folder slots map upward from this MIDI note when playback is a map mode.", chipper::ChipParameterRole::nesDmcMapRoot));

    const auto info = audioProcessor.spc700BrrSampleInfo();
    auto visibleStatus = info.loaded
        ? "Slot " + juce::String(info.selectedSlot + 1) + "/" + juce::String(info.bankCount)
            + ": " + compactSampleName(info.sampleName)
        : "Generated " + waveShapeReadout(chipper::ChipMode::spc700, playbackPatch.waveShape);
    if (info.loaded && info.bankCount > 1 && playbackMode != 0)
        visibleStatus += " | Map " + chipper::parameters::midiNoteChoices()[info.mapRootNote]
            + "-" + chipper::parameters::midiNoteChoices()[info.mapHighNote];
    else if (info.loaded && info.bankCount > 1)
        visibleStatus += " | Manual";
    visibleStatus += " | " + bankModeLabel + " | " + lifetimeLabel;
    if (info.loaded)
    {
        visibleStatus += " | "
            + juce::String(static_cast<double>(info.bankByteCount) / 1024.0, info.bankByteCount < 10240 ? 1 : 0)
            + "/64 KB";
        if (info.exceedsAramBudget)
            visibleStatus += " over";
        else if (info.nearAramBudget)
            visibleStatus += " near";
    }
    dmcSampleStatusLabel.setText(visibleStatus, juce::dontSendNotification);
    auto tooltip = info.statusLine
        + "\nBRR files are decoded into the clean-room SPC700 sample voice path. WAV/AIFF files import as 8-bit sample memory; true WAV-to-BRR conversion remains planned. Folder loads keep up to 32 user-provided files addressable by the dropdown and CC117.";
    if (! info.loaded)
        tooltip += juce::String("\nNo user sample is loaded, so the internal generated sample shape remains playable.")
            + "\nGenerated Sample Shape: " + waveShapeReadout(chipper::ChipMode::spc700, playbackPatch.waveShape);
    if (info.loaded)
    {
        tooltip += "\nActive bank payload: " + juce::String(info.bankByteCount) + " bytes / "
            + juce::String(static_cast<double>(info.bankByteCount) / 1024.0, 1)
            + " KB of the SNES 64 KB audio RAM world.";
        if (info.bankBrrBlockCount > 0)
            tooltip += " BRR blocks: " + juce::String(info.bankBrrBlockCount) + ".";
        tooltip += "\nThe real SPC700/S-DSP shares this memory with driver code, echo buffers, sequence data, and sample directory metadata, so this is a practical warning rather than an exact cartridge budget.";
        if (info.exceedsAramBudget)
            tooltip += "\nWarning: checked samples exceed 64 KB before driver/echo overhead.";
        else if (info.nearAramBudget)
            tooltip += "\nWarning: checked samples are near the 64 KB ceiling before driver/echo overhead.";
    }
    if (playbackMode == 0)
        tooltip += "\nSample Playback is Manual Slot: each MIDI note uses the selected dropdown sample.";
    else if (playbackMode == 2)
        tooltip += "\nSample Playback is Drum Map: notes browse the loaded bank from "
            + chipper::parameters::midiNoteChoices()[mapRoot]
            + " upward and Preset loop mode resolves to one-shot sample playback.";
    else
        tooltip += "\nSample Playback is a map mode: notes browse the loaded bank from "
            + chipper::parameters::midiNoteChoices()[mapRoot] + " upward.";
    tooltip += "\nSample Playback: " + bankModeLabel + ".";
    tooltip += "\nResolved playback lifetime: " + lifetimeLabel + ".";
    if (info.loaded && info.bankCount > 1 && playbackMode != 0)
        tooltip += "\nCurrent mapped key span: "
            + chipper::parameters::midiNoteChoices()[info.mapRootNote]
            + " to "
            + chipper::parameters::midiNoteChoices()[info.mapHighNote]
            + "; notes outside this span are silent.";
    if (info.loaded)
        tooltip += "\nPath: " + info.path;
    dmcSampleStatusLabel.setTooltip(withMidiCcForRole(tooltip, chipper::ChipParameterRole::nesDmcSampleSlot));
    updateSampleWaveformPreview(chipper::ChipMode::spc700);
}

void ChipperAudioProcessorEditor::updatePaulaSampleControls()
{
    updateSamplePlaybackModeChoices(chipper::ChipMode::paula);
    dmcSampleLabel.setText("Paula Sample", juce::dontSendNotification);
    const auto mapRoot = std::clamp(static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcMapRoot))), 0, 127);
    const auto playbackMode = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcPlaybackMode)));
    dmcSampleLabel.setTooltip(withMidiCcForRole("Load one user-provided WAV/AIFF/8SVX sample or a folder. CC117 selects the manual Paula sample slot; Sample Playback chooses whether MIDI notes browse the loaded bank.", chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcSampleFileButton.setButtonText("File");
    dmcSampleFileButton.setTooltip("Load one user-provided WAV/AIFF or uncompressed IFF/8SVX into the Paula-style 8-bit sample voice model.");
    dmcSampleFolderButton.setButtonText("Folder");
    dmcSampleFolderButton.setTooltip("Load a folder of user-provided WAV/AIFF/8SVX files. Checked entries become up to 32 playable slots and note-mapped sample choices.");
    dmcSampleBankButton.setButtonText("Bank");
    dmcSampleBankButton.setTooltip("Open the Paula sample checklist. Checked files become the playable 32-slot dropdown and note map.");

    const auto names = audioProcessor.paulaSampleNames();
    const auto sampleCount = names.size();
    const auto revision = audioProcessor.paulaSampleRevision();
    const auto selectedSlot = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcSampleSlot)));

    const juce::ScopedValueSetter<bool> suppress(suppressManualChoiceCallbacks, true);
    if (displayedSampleListMode != chipper::ChipMode::paula
        || sampleCount != displayedDmcSampleCount
        || revision != displayedDmcSampleRevision
        || playbackMode != displayedSamplePlaybackMode
        || mapRoot != displayedSampleMapRoot)
    {
        dmcSampleSlotBox.clear(juce::dontSendNotification);
        for (int i = 0; i < sampleCount; ++i)
        {
            auto itemLabel = juce::String(i + 1).paddedLeft('0', 2) + "  " + names[i];
            if (playbackMode != 0)
            {
                const auto note = mapRoot + i;
                const auto noteLabel = note <= 127 ? chipper::parameters::midiNoteChoices()[note] : juce::String("--");
                itemLabel = noteLabel + "  " + itemLabel;
            }
            dmcSampleSlotBox.addItem(itemLabel, i + 1);
        }

        displayedSampleListMode = chipper::ChipMode::paula;
        displayedDmcSampleCount = sampleCount;
        displayedDmcSampleRevision = revision;
        displayedSamplePlaybackMode = playbackMode;
        displayedSampleMapRoot = mapRoot;
    }

    if (sampleCount > 0)
        dmcSampleSlotBox.setSelectedId(std::clamp(selectedSlot, 0, sampleCount - 1) + 1, juce::dontSendNotification);
    else
        dmcSampleSlotBox.setSelectedId(0, juce::dontSendNotification);

    dmcSampleSlotBox.setEnabled(sampleCount > 0);
    dmcSampleSlotBox.setTextWhenNothingSelected(sampleCount > 0 ? "Select Paula sample" : "Generated shape");
    dmcSampleSlotBox.setTooltip(withMidiCcForRole(
        sampleCount > 0
            ? "Selects the manual Paula sample from the loaded bank. MIDI CC117 selects the same slot; Manual Slot uses it for every note."
            : "No external Paula bank is loaded. Chipper is playing the generated Paula sample shape selected by Sample Shape.",
        chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcPlaybackModeBox.setTooltip(withMidiCcForRole("Paula Sample Playback. Manual Slot plays the selected dropdown slot; Key Map maps loaded folder slots upward from the Sample Map Root for held/melodic samples. Tracker Map uses the same bank mapping path and resolves Preset playback to one-shot tracker kits.", chipper::ChipParameterRole::nesDmcPlaybackMode));
    dmcMapRootBox.setTooltip(withMidiCcForRole("Paula Sample Map Root. Loaded folder slots map upward from this MIDI note when Sample Playback is a map mode.", chipper::ChipParameterRole::nesDmcMapRoot));

    const auto info = audioProcessor.paulaSampleInfo();
    auto visibleStatus = info.loaded
        ? "Slot " + juce::String(info.selectedSlot + 1) + "/" + juce::String(info.bankCount)
            + ": " + compactSampleName(info.sampleName)
        : info.statusLine;
    if (info.loaded && info.bankCount > 1 && playbackMode != 0)
        visibleStatus += " | Map " + chipper::parameters::midiNoteChoices()[info.mapRootNote]
            + "-" + chipper::parameters::midiNoteChoices()[info.mapHighNote];
    else if (info.loaded && info.bankCount > 1)
        visibleStatus += " | Manual";
    dmcSampleStatusLabel.setText(visibleStatus, juce::dontSendNotification);

    auto tooltip = info.statusLine
        + "\nWAV/AIFF and uncompressed IFF/8SVX files are imported into Paula-style 8-bit sample memory. Folder loads keep up to 32 user-provided files addressable by the dropdown and CC117.";
    if (playbackMode == 0)
        tooltip += "\nSample Playback is Manual Slot: each MIDI note uses the selected dropdown sample.";
    else if (playbackMode == 2)
        tooltip += "\nSample Playback is Tracker Map: notes browse the loaded bank from "
            + chipper::parameters::midiNoteChoices()[mapRoot]
            + " upward and Preset loop mode resolves to one-shot tracker playback.";
    else
        tooltip += "\nSample Playback is a map mode: notes browse the loaded bank from "
            + chipper::parameters::midiNoteChoices()[mapRoot] + " upward.";
    if (info.loaded && info.bankCount > 1 && playbackMode != 0)
        tooltip += "\nCurrent mapped key span: "
            + chipper::parameters::midiNoteChoices()[info.mapRootNote]
            + " to "
            + chipper::parameters::midiNoteChoices()[info.mapHighNote]
            + "; notes outside this span are silent.";
    if (info.loaded)
        tooltip += "\nPath: " + info.path;
    tooltip += "\nCompressed 8SVX, MOD import, loop metadata, and exact Paula DMA timing remain planned.";
    dmcSampleStatusLabel.setTooltip(withMidiCcForRole(tooltip, chipper::ChipParameterRole::nesDmcSampleSlot));
    updateSampleWaveformPreview(chipper::ChipMode::paula);
}

void ChipperAudioProcessorEditor::updateSampleWaveformPreview(chipper::ChipMode mode)
{
    const auto showSamplePreview = mode == chipper::ChipMode::nes
        || mode == chipper::ChipMode::spc700
        || mode == chipper::ChipMode::paula;

    if (! showSamplePreview)
    {
        sampleWaveformPreview.setSnapshot({});
        sampleWaveformPreview.setVisible(false);
        return;
    }

    const auto snapshot = audioProcessor.sampleWaveformSnapshot(mode);
    sampleWaveformPreview.setSnapshot(snapshot);
    sampleWaveformPreview.setTooltip(snapshot.label
        + (snapshot.loaded ? "\nSamples displayed: " + juce::String(snapshot.sourceSampleCount) : "\nNo loaded sample data.")
        + (snapshot.hasLoop
               ? "\nLoop: " + juce::String(snapshot.loopStart * 100.0f, 1) + "% to " + juce::String(snapshot.loopEnd * 100.0f, 1) + "%"
               : juce::String()));

    if (mode == chipper::ChipMode::spc700)
    {
        sampleLoopStartValueLabel.setText(juce::String(snapshot.loopStart * 100.0f, 1) + "%", juce::dontSendNotification);
        sampleLoopEndValueLabel.setText(juce::String(snapshot.loopEnd * 100.0f, 1) + "%", juce::dontSendNotification);
    }
}

void ChipperAudioProcessorEditor::updateSamplePlaybackModeChoices(chipper::ChipMode mode)
{
    const auto choices = chipper::parameters::samplePlaybackModeChoices(mode);
    const auto selectedChoice = std::clamp(static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcPlaybackMode))), 0, choices.size() - 1);
    const auto emptyText = mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula ? juce::String("Sample Playback") : juce::String("DMC Playback");

    auto choicesAlreadyMatch = dmcPlaybackModeBox.getNumItems() == choices.size();
    if (choicesAlreadyMatch)
    {
        for (int i = 0; i < choices.size(); ++i)
        {
            if (dmcPlaybackModeBox.getItemText(i) != choices[i])
            {
                choicesAlreadyMatch = false;
                break;
            }
        }
    }

    const juce::ScopedValueSetter<bool> suppress(suppressManualChoiceCallbacks, true);
    if (choicesAlreadyMatch)
    {
        dmcPlaybackModeBox.setTextWhenNothingSelected(emptyText);
        dmcPlaybackModeBox.setSelectedId(selectedChoice + 1, juce::dontSendNotification);
        return;
    }

    dmcPlaybackModeBox.clear(juce::dontSendNotification);
    for (int i = 0; i < choices.size(); ++i)
        dmcPlaybackModeBox.addItem(choices[i], i + 1);
    dmcPlaybackModeBox.setTextWhenNothingSelected(emptyText);
    dmcPlaybackModeBox.setSelectedId(selectedChoice + 1, juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::chooseDmcSampleFile()
{
    dmcSampleChooser = std::make_unique<juce::FileChooser>("Choose a NES DMC sample",
                                                           juce::File {},
                                                           "*.dmc");
    dmcSampleChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();
                                      if (file == juce::File {})
                                          return;

                                      handleDmcSampleLoadResult(audioProcessor.loadNesDmcSampleFile(file));
                                  });
}

void ChipperAudioProcessorEditor::chooseSpc700BrrSampleFile()
{
    dmcSampleChooser = std::make_unique<juce::FileChooser>("Choose an SPC700 sample",
                                                           juce::File {},
                                                           "*.brr;*.wav;*.aif;*.aiff");
    dmcSampleChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();
                                      if (file == juce::File {})
                                          return;

                                      handleDmcSampleLoadResult(audioProcessor.loadSpc700BrrSampleFile(file));
                                  });
}

void ChipperAudioProcessorEditor::chooseSpc700BrrSampleDirectory()
{
    dmcSampleChooser = std::make_unique<juce::FileChooser>("Choose a folder of SPC700 samples",
                                                           juce::File {},
                                                           "*.brr;*.wav;*.aif;*.aiff");
    dmcSampleChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this](const juce::FileChooser& chooser)
                                  {
                                      const auto directory = chooser.getResult();
                                      if (directory == juce::File {})
                                          return;

                                      handleDmcSampleLoadResult(audioProcessor.loadSpc700BrrSampleDirectory(directory));
                                  });
}

void ChipperAudioProcessorEditor::choosePaulaSampleFile()
{
    dmcSampleChooser = std::make_unique<juce::FileChooser>("Choose an Amiga Paula sample",
                                                           juce::File {},
                                                           "*.wav;*.aif;*.aiff;*.8svx;*.iff");
    dmcSampleChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();
                                      if (file == juce::File {})
                                          return;

                                      handleDmcSampleLoadResult(audioProcessor.loadPaulaSampleFile(file));
                                  });
}

void ChipperAudioProcessorEditor::choosePaulaSampleDirectory()
{
    dmcSampleChooser = std::make_unique<juce::FileChooser>("Choose a folder of Amiga Paula samples",
                                                           juce::File {},
                                                           "*.wav;*.aif;*.aiff;*.8svx;*.iff");
    dmcSampleChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this](const juce::FileChooser& chooser)
                                  {
                                      const auto directory = chooser.getResult();
                                      if (directory == juce::File {})
                                          return;

                                      handleDmcSampleLoadResult(audioProcessor.loadPaulaSampleDirectory(directory));
                                  });
}

void ChipperAudioProcessorEditor::chooseDmcSampleDirectory()
{
    dmcSampleChooser = std::make_unique<juce::FileChooser>("Choose a folder of NES DMC samples",
                                                           juce::File {},
                                                           "*.dmc");
    dmcSampleChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this](const juce::FileChooser& chooser)
                                  {
                                      const auto directory = chooser.getResult();
                                      if (directory == juce::File {})
                                          return;

                                      handleDmcSampleLoadResult(audioProcessor.loadNesDmcSampleDirectory(directory));
                                  });
}

void ChipperAudioProcessorEditor::showDmcSampleBankEditor()
{
    const auto editsSpc700Brr = displayedMode == chipper::ChipMode::spc700;
    const auto editsPaulaSamples = displayedMode == chipper::ChipMode::paula;
    auto popup = std::make_unique<DmcSampleBankEditorComponent>(audioProcessor,
                                                                editsSpc700Brr,
                                                                editsPaulaSamples,
                                                                [this]
                                                                {
                                                                    displayedDmcSampleCount = -1;
                                                                    displayedDmcSampleRevision = std::numeric_limits<uint64_t>::max();
                                                                    if (displayedMode == chipper::ChipMode::paula)
                                                                        updatePaulaSampleControls();
                                                                    else if (displayedMode == chipper::ChipMode::spc700)
                                                                        updateSpc700BrrSampleControls();
                                                                    else
                                                                        updateDmcSampleControls();
                                                                });
    juce::CallOutBox::launchAsynchronously(std::move(popup), dmcSampleBankButton.getScreenBounds(), this);
}

void ChipperAudioProcessorEditor::handleDmcSampleLoadResult(const juce::Result& result)
{
    if (result.failed())
    {
        dmcSampleStatusLabel.setText(result.getErrorMessage(), juce::dontSendNotification);
        dmcSampleStatusLabel.setTooltip(result.getErrorMessage());
        return;
    }

    displayedDmcSampleCount = -1;
    displayedDmcSampleRevision = std::numeric_limits<uint64_t>::max();
    if (displayedMode == chipper::ChipMode::paula)
        updatePaulaSampleControls();
    else if (displayedMode == chipper::ChipMode::spc700)
        updateSpc700BrrSampleControls();
    else
        updateDmcSampleControls();
}

void ChipperAudioProcessorEditor::updateDescriptorText()
{
    const auto modeChoice = static_cast<int>(std::round(audioProcessor.getValueTreeState()
                                                            .getRawParameterValue(chipper::parameters::id::chipMode)
                                                            ->load()));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    if (descriptorTextInitialized && mode == displayedMode)
        return;

    const auto previousMode = displayedMode;
    const auto chipChangedAfterInitialLoad = descriptorTextInitialized && mode != displayedMode;
    if (chipChangedAfterInitialLoad && ! applyingFactoryPreset)
        storeChipSettingsSnapshot(previousMode);

    descriptorTextInitialized = true;
    displayedMode = mode;
    if (chipChangedAfterInitialLoad)
    {
        displayedDmcSampleCount = -1;
        displayedDmcSampleRevision = std::numeric_limits<uint64_t>::max();
    }
    applyChipTheme();
    const auto& descriptor = chipper::descriptorFor(mode);
    const auto hasLiveCore = descriptor.implemented;
    const auto supportsChipPoly = chipper::supportsPlayMode(mode, chipper::PlayMode::chipPoly);

    updateMacroChoices(mode);
    updatePresetChoices(mode);
    const auto restoredChipSettings = chipChangedAfterInitialLoad
        && ! applyingFactoryPreset
        && restoreChipSettingsSnapshot(mode);
    chipSummaryLabel.setText(descriptor.summary, juce::dontSendNotification);
    globalStripLabel.setText(hasLiveCore ? "Performance Macros" : "Roadmap", juce::dontSendNotification);
    macroSummaryLabel.setVisible(true);
    macroSummaryLabel.setEnabled(true);
    macroSummaryLabel.setAlpha(hasLiveCore ? 1.0f : 0.85f);
    accuracyBox.setEnabled(hasLiveCore);
    presetBox.setEnabled(hasLiveCore);
    headerControlLabels[3].setVisible(false);
    macroBox.setVisible(false);
    macroBox.setEnabled(false);
    macroBox.setTooltip("Internal preset recipe. Factory/user presets set this automatically for chip-native defaults.");
    playModeBox.setEnabled(hasLiveCore && supportsChipPoly);
    playModeBox.setTooltip(supportsChipPoly
                               ? "Chooses how incoming notes use the chip channels inside one patch."
                               : "Chip Poly is disabled until this chip has tested finite-channel allocation.");
    clockSlider.setEnabled(hasLiveCore);
    clockLabel.setAlpha(hasLiveCore ? 1.0f : 0.55f);
    clockSlider.setAlpha(hasLiveCore ? 1.0f : 0.55f);
    dmcDirectLabel.setVisible(hasLiveCore && mode == chipper::ChipMode::nes);
    dmcDirectSlider.setVisible(hasLiveCore && mode == chipper::ChipMode::nes);
    dmcRateLabel.setVisible(hasLiveCore && mode == chipper::ChipMode::nes);
    dmcRateBox.setVisible(hasLiveCore && mode == chipper::ChipMode::nes);
    dmcDirectLabel.setEnabled(hasLiveCore);
    dmcDirectSlider.setEnabled(hasLiveCore);
    dmcRateLabel.setEnabled(hasLiveCore);
    dmcRateBox.setEnabled(hasLiveCore);
    dmcDirectLabel.setAlpha(hasLiveCore ? 1.0f : 0.55f);
    dmcDirectSlider.setAlpha(hasLiveCore ? 1.0f : 0.55f);
    dmcRateLabel.setAlpha(hasLiveCore ? 1.0f : 0.55f);
    dmcRateBox.setAlpha(hasLiveCore ? 1.0f : 0.55f);
    const auto showNesDmcSampleControls = hasLiveCore && mode == chipper::ChipMode::nes;
    const auto showSpc700BrrControls = hasLiveCore && mode == chipper::ChipMode::spc700;
    const auto showPaulaSampleControls = hasLiveCore && mode == chipper::ChipMode::paula;
    const auto showSampleBankControls = showNesDmcSampleControls || showSpc700BrrControls || showPaulaSampleControls;
    const auto showSampleFileControls = showSampleBankControls;
    const auto showSampleBankLabels = showSpc700BrrControls || showPaulaSampleControls;
    dmcSampleLabel.setVisible(showSampleFileControls);
    dmcSampleStatusLabel.setVisible(showSampleFileControls);
    dmcPlaybackModeLabel.setVisible(showSampleBankLabels);
    dmcMapRootLabel.setVisible(showSampleBankLabels);
    dmcSampleFileButton.setVisible(showSampleFileControls);
    dmcSampleFolderButton.setVisible(showSampleFileControls);
    dmcSampleBankButton.setVisible(showSampleBankControls);
    dmcSampleSlotBox.setVisible(showSampleBankControls);
    dmcPlaybackModeBox.setVisible(showSampleBankControls);
    dmcMapRootBox.setVisible(showSampleBankControls);
    dmcLoopButton.setVisible(showNesDmcSampleControls);
    spc700LoopModeButton.setVisible(showSpc700BrrControls);
    sampleWaveformPreview.setVisible(showSampleFileControls);
    sampleLoopStartLabel.setVisible(showSpc700BrrControls);
    sampleLoopEndLabel.setVisible(showSpc700BrrControls);
    sampleLoopStartValueLabel.setVisible(showSpc700BrrControls);
    sampleLoopEndValueLabel.setVisible(showSpc700BrrControls);
    sampleLoopStartSlider.setVisible(showSpc700BrrControls);
    sampleLoopEndSlider.setVisible(showSpc700BrrControls);
    dmcSampleLabel.setEnabled(showSampleFileControls);
    dmcSampleStatusLabel.setEnabled(showSampleFileControls);
    dmcPlaybackModeLabel.setEnabled(showSampleBankLabels);
    dmcMapRootLabel.setEnabled(showSampleBankLabels);
    dmcSampleFileButton.setEnabled(showSampleFileControls);
    dmcSampleFolderButton.setEnabled(showSampleFileControls);
    dmcSampleBankButton.setEnabled(showSampleBankControls);
    dmcSampleSlotBox.setEnabled(showSampleBankControls);
    dmcPlaybackModeBox.setEnabled(showSampleBankControls);
    dmcMapRootBox.setEnabled(showSampleBankControls);
    dmcLoopButton.setEnabled(showNesDmcSampleControls);
    spc700LoopModeButton.setEnabled(showSpc700BrrControls);
    sampleWaveformPreview.setEnabled(showSampleFileControls);
    sampleLoopStartLabel.setEnabled(showSpc700BrrControls);
    sampleLoopEndLabel.setEnabled(showSpc700BrrControls);
    sampleLoopStartValueLabel.setEnabled(showSpc700BrrControls);
    sampleLoopEndValueLabel.setEnabled(showSpc700BrrControls);
    sampleLoopStartSlider.setEnabled(showSpc700BrrControls);
    sampleLoopEndSlider.setEnabled(showSpc700BrrControls);
    dmcSampleLabel.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    dmcSampleStatusLabel.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    dmcPlaybackModeLabel.setAlpha(showSampleBankLabels ? 1.0f : 0.55f);
    dmcMapRootLabel.setAlpha(showSampleBankLabels ? 1.0f : 0.55f);
    dmcSampleFileButton.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    dmcSampleFolderButton.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    dmcSampleBankButton.setAlpha(showSampleBankControls ? 1.0f : 0.55f);
    dmcPlaybackModeBox.setAlpha(showSampleBankControls ? 1.0f : 0.55f);
    dmcMapRootBox.setAlpha(showSampleBankControls ? 1.0f : 0.55f);
    dmcLoopButton.setAlpha(showNesDmcSampleControls ? 1.0f : 0.55f);
    spc700LoopModeButton.setAlpha(showSpc700BrrControls ? 1.0f : 0.55f);
    sampleWaveformPreview.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    sampleLoopStartLabel.setAlpha(showSpc700BrrControls ? 1.0f : 0.55f);
    sampleLoopEndLabel.setAlpha(showSpc700BrrControls ? 1.0f : 0.55f);
    sampleLoopStartValueLabel.setAlpha(showSpc700BrrControls ? 1.0f : 0.55f);
    sampleLoopEndValueLabel.setAlpha(showSpc700BrrControls ? 1.0f : 0.55f);
    sampleLoopStartSlider.setAlpha(showSpc700BrrControls ? 1.0f : 0.55f);
    sampleLoopEndSlider.setAlpha(showSpc700BrrControls ? 1.0f : 0.55f);
    clockLabel.setVisible(mode != chipper::ChipMode::nes && mode != chipper::ChipMode::spc700 && mode != chipper::ChipMode::paula);
    clockSlider.setVisible(mode != chipper::ChipMode::nes && mode != chipper::ChipMode::spc700 && mode != chipper::ChipMode::paula);

    for (size_t i = 0; i < moduleTitleLabels.size(); ++i)
    {
        if (i >= descriptor.modules.size())
            continue;

        const auto& module = descriptor.modules[i];
        moduleTitleLabels[i].setText(module.title, juce::dontSendNotification);
        auto summary = juce::String(module.summary);
        if (mode == chipper::ChipMode::nes && i == 5)
        {
            moduleTitleLabels[i].setText("DMC Sample", juce::dontSendNotification);
            summary = "Load a DMC file or bank, then browse manually or map slots across notes.";
        }
        else if (mode == chipper::ChipMode::spc700 && i == 5)
        {
            moduleTitleLabels[i].setText("Sample Bank", juce::dontSendNotification);
            summary = "Load BRR/WAV/AIFF samples, choose playback mapping, and edit loop range.";
        }
        else if (mode == chipper::ChipMode::paula && i == 5)
        {
            moduleTitleLabels[i].setText("Sample Bank", juce::dontSendNotification);
            summary = "Load tracker-style samples, choose playback mapping, and preview the waveform.";
        }
        if (i == 1 && hasLiveCore && usesSourceChannelSurface(mode))
        {
            const auto visibleCount = chipper::visibleSourceCountForMode(mode);
            const auto nativeCount = chipper::nativeSourceCountForMode(mode);
            if (nativeCount > visibleCount)
            {
                summary = "Showing first " + juce::String(static_cast<int>(visibleCount))
                    + " of " + juce::String(static_cast<int>(nativeCount))
                    + " native lanes; presets may use internal stack lanes.";
            }
        }
        moduleSummaryLabels[i].setText(summary, juce::dontSendNotification);
        moduleSummaryLabels[i].setVisible(true);

        for (size_t item = 0; item < moduleItemLabels[i].size(); ++item)
        {
            const auto text = item < module.items.size() ? module.items[item] : std::string {};
            moduleItemLabels[i][item].setText(text, juce::dontSendNotification);
            moduleItemLabels[i][item].setVisible(! text.empty());
        }
    }

    for (size_t i = 0; i < nativeLabels.size(); ++i)
    {
        const auto* spec = chipper::parameterSpecFor(mode, macroControlRole(i));
        const auto hasFallbackControl = i < descriptor.controls.size();
        if (spec != nullptr || hasFallbackControl)
        {
            const auto active = hasLiveCore;
            nativeGroupLabels[i].setText(spec != nullptr ? spec->group : descriptor.controls[i].group, juce::dontSendNotification);
            nativeLabels[i].setText(spec != nullptr ? spec->label : descriptor.controls[i].label, juce::dontSendNotification);
            nativeSliders[i].setTooltip(withMidiCcForRole(spec != nullptr ? spec->help : descriptor.controls[i].help, macroControlRole(i)));
            controlValueLabels[i].setTooltip(withMidiCcForRole(spec != nullptr ? spec->help : descriptor.controls[i].help, macroControlRole(i)));
            nativeGroupLabels[i].setVisible(true);
            nativeLabels[i].setVisible(true);
            nativeSliders[i].setVisible(true);
            controlValueLabels[i].setVisible(true);
            nativeGroupLabels[i].setEnabled(active);
            nativeLabels[i].setEnabled(active);
            nativeSliders[i].setEnabled(active);
            controlValueLabels[i].setEnabled(active);
            nativeGroupLabels[i].setAlpha(active ? 1.0f : 0.55f);
            nativeLabels[i].setAlpha(active ? 1.0f : 0.55f);
            nativeSliders[i].setAlpha(active ? 1.0f : 0.55f);
            controlValueLabels[i].setAlpha(active ? 1.0f : 0.55f);
        }
        else
        {
            nativeGroupLabels[i].setText({}, juce::dontSendNotification);
            nativeLabels[i].setText("Native Control", juce::dontSendNotification);
            nativeSliders[i].setTooltip({});
            controlValueLabels[i].setTooltip({});
            nativeGroupLabels[i].setVisible(false);
            nativeLabels[i].setVisible(false);
            nativeSliders[i].setVisible(false);
            controlValueLabels[i].setVisible(false);
        }
    }

    updateSegmentedControlSpecs(mode);
    controlValueLabels[4].setVisible(true);
    controlValueLabels[5].setVisible(true);
    controlValueLabels[4].setEnabled(hasLiveCore);
    controlValueLabels[5].setEnabled(hasLiveCore);
    controlValueLabels[4].setAlpha(hasLiveCore ? 1.0f : 0.55f);
    controlValueLabels[5].setAlpha(hasLiveCore ? 1.0f : 0.55f);
    setSourceChannelSurfaceVisible(mode, usesSourceChannelSurface(mode));
    const auto hasChannelLocalWaveDeck = usesChannelLocalWaveDeck(mode);
    setPulse2DutySegmentVisible(mode, usesPulse2DutySegment(mode) && hasLiveCore && ! hasChannelLocalWaveDeck);
    setWaveShapeSegmentVisible(mode, usesWaveShapeSegment(mode) && hasLiveCore && ! hasChannelLocalWaveDeck);
    setDmgWaveLevelSegmentVisible(mode, usesDmgWaveLevelSegment(mode) && hasLiveCore);
    setDmgStereoRouteSegmentVisible(mode, usesDmgStereoRouteSegment(mode) && hasLiveCore);
    setYmEnvelopeShapeSegmentVisible(mode, usesYmEnvelopeShapeSegment(mode) && hasLiveCore);
    setYmChannelMixControlsVisible(usesYmChannelMixControls(mode) && hasLiveCore);
    setSnNoiseModeSegmentVisible(mode, usesSnNoiseModeSegment(mode) && hasLiveCore);
    setEnvelopeDecayControlVisible(mode, usesEnvelopeDecayControl(mode) && hasLiveCore);
    setStereoSpreadControlVisible(mode, usesStereoSpreadControl(mode) && hasLiveCore);
    const auto hasSidFilterRoutingControl = hasLiveCore
        && mode == chipper::ChipMode::sid
        && chipper::parameterSpecFor(mode, chipper::ChipParameterRole::sidFilterRouting) != nullptr;
    setSidFilterRoutingControlVisible(hasSidFilterRoutingControl);
    const auto hasFmEnvelopeShapeSurface = mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151;
    const auto hasFmOperatorRegisterSurface = mode == chipper::ChipMode::ym2612
        || mode == chipper::ChipMode::opl3
        || mode == chipper::ChipMode::ym2151;
    moduleSummaryLabels[1].setVisible(!(hasLiveCore && usesSourceChannelSurface(mode)));
    moduleSummaryLabels[3].setVisible(!(hasLiveCore
        && (usesEnvelopeDecayControl(mode)
            || (hasFmEnvelopeShapeSurface && usesYmEnvelopeShapeSegment(mode))
            || hasFmOperatorRegisterSurface)));
    const auto hasCustomProfileSurface = hasLiveCore && mode == chipper::ChipMode::sid && usesDmgStereoRouteSegment(mode);
    const auto hasReferenceOnlyProfile = hasLiveCore && ! hasCustomProfileSurface;
    for (auto& itemLabel : moduleItemLabels[0])
        itemLabel.setVisible(! hasReferenceOnlyProfile && ! hasCustomProfileSurface && ! itemLabel.getText().isEmpty());
    const auto hasEmbeddedSourceRegisterControls = mode == chipper::ChipMode::nes
        || mode == chipper::ChipMode::dmg
        || usesChannelLocalWaveDeck(mode);
    const auto hasCustomToneSurface = hasLiveCore && mode != chipper::ChipMode::dmg && ! hasEmbeddedSourceRegisterControls && (usesWaveShapeSegment(mode)
        || usesPulse2DutySegment(mode)
        || usesDmgWaveLevelSegment(mode)
        || usesYmChannelMixControls(mode)
        || (mode != chipper::ChipMode::sid && usesSnNoiseModeSegment(mode))
        || (mode != chipper::ChipMode::spc700 && ! hasFmEnvelopeShapeSurface && usesYmEnvelopeShapeSegment(mode))
        || hasSidFilterRoutingControl);
    moduleSummaryLabels[2].setVisible(! hasCustomToneSurface);
    for (auto& itemLabel : moduleItemLabels[2])
        itemLabel.setVisible(! hasCustomToneSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomEnvelopeSurface = hasLiveCore
        && (usesEnvelopeDecayControl(mode)
            || ((mode == chipper::ChipMode::spc700 || hasFmEnvelopeShapeSurface) && usesYmEnvelopeShapeSegment(mode))
            || hasFmOperatorRegisterSurface);
    for (auto& itemLabel : moduleItemLabels[3])
        itemLabel.setVisible(! hasCustomEnvelopeSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomMotionSurface = hasLiveCore && mode == chipper::ChipMode::sid && usesSnNoiseModeSegment(mode);
    for (auto& itemLabel : moduleItemLabels[4])
        itemLabel.setVisible(! hasCustomMotionSurface && mode == chipper::ChipMode::sid && ! itemLabel.getText().isEmpty());
    const auto hasSampleBankPanel = mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula;
    const auto hasCustomOutputSurface = hasLiveCore
        && mode != chipper::ChipMode::sid
        && ! hasSampleBankPanel
        && (mode == chipper::ChipMode::nes || usesStereoSpreadControl(mode) || usesDmgStereoRouteSegment(mode));
    moduleSummaryLabels[5].setVisible(! hasSampleBankPanel && ! hasCustomOutputSurface);
    for (auto& itemLabel : moduleItemLabels[5])
        itemLabel.setVisible(! hasSampleBankPanel && ! hasCustomOutputSurface && ! itemLabel.getText().isEmpty());
    resized();
    updatePulseDutyButtons(parameterValue(chipper::parameters::id::macroControl1), usesPulseDutySegment(mode) && hasLiveCore);
    updateLiveControlReadouts();

    if (chipChangedAfterInitialLoad && ! restoredChipSettings && ! applyingFactoryPreset && hasLiveCore && ! displayedPresets.empty())
        applyFactoryPreset(*displayedPresets.front());

    repaint();
}

void ChipperAudioProcessorEditor::updateLiveControlReadouts()
{
    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto macroControl1 = parameterValue(chipper::parameters::id::macroControl1);
    const auto macroControl2 = parameterValue(chipper::parameters::id::macroControl2);
    const auto macroControl3 = parameterValue(chipper::parameters::id::macroControl3);
    const auto macroControl4 = parameterValue(chipper::parameters::id::macroControl4);
    const auto waveShape = static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape)));
    const auto dmgWaveLevel = static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel)));
    const auto dmgStereoRoute = static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute)));
    const auto ymEnvelopeShape = static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape)));
    const auto snNoiseMode = static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode)));
    const auto stereoSpread = parameterValue(chipper::parameters::id::stereoSpread);
    const auto patch = currentUiPatch(mode, macroControl1, macroControl2, macroControl3, macroControl4, waveShape, dmgWaveLevel, dmgStereoRoute, ymEnvelopeShape, snNoiseMode, stereoSpread);
    const auto macroText = macroTemplateReadout(mode, patch);
    const auto& templ = chipper::macroTemplateFor(mode, patch.macro);
    const auto templateLabel = templ.label.empty() ? juce::String(chipper::toString(patch.macro)) : juce::String(templ.label);
    auto performanceText = juce::String("Recipe: ") + templateLabel;
    auto performanceTooltip = macroText;
    if (! displayedPresets.empty() || ! displayedUserPresets.empty())
    {
        const auto presetCount = static_cast<int>(displayedPresets.size() + displayedUserPresets.size());
        performanceText = juce::String(presetCount) + " "
            + juce::String(chipper::toString(mode))
            + " presets | Recipe: " + templateLabel;
        performanceTooltip = "Use the Preset menu to audition "
            + juce::String(presetCount)
            + " factory/user presets for "
            + juce::String(chipper::toString(mode))
            + ".\n"
            + macroText;
    }
    const auto selectedId = presetBox.getSelectedId();
    const auto selectedPreset = selectedId - 1;
    if (selectedId >= userPresetItemIdBase)
    {
        const auto userIndex = selectedId - userPresetItemIdBase;
        if (userIndex >= 0 && static_cast<size_t>(userIndex) < displayedUserPresets.size())
        {
            const auto& preset = displayedUserPresets[static_cast<size_t>(userIndex)];
            performanceText = "User Preset: " + preset.name;
            performanceTooltip = "Loaded from " + preset.file.getFullPathName() + "\n" + macroText;
        }
    }
    else if (selectedPreset >= 0 && static_cast<size_t>(selectedPreset) < displayedPresets.size())
    {
        const auto& preset = *displayedPresets[static_cast<size_t>(selectedPreset)];
        performanceText = juce::String("Preset: ") + juce::String(preset.name);
        performanceTooltip = juce::String(preset.name) + ": " + juce::String(preset.note) + "\n" + macroText;
    }

    macroSummaryLabel.setText(performanceText, juce::dontSendNotification);
    macroSummaryLabel.setTooltip(performanceTooltip);
    const auto hasChannelLocalWaveDeck = usesChannelLocalWaveDeck(mode);
    const auto hasPulseDutySegment = usesPulseDutySegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasPulse2DutySegment = usesPulse2DutySegment(mode) && chipper::descriptorFor(mode).implemented && ! hasChannelLocalWaveDeck;
    const auto hasWaveShapeSegment = usesWaveShapeSegment(mode) && chipper::descriptorFor(mode).implemented && ! hasChannelLocalWaveDeck;
    const auto hasDmgWaveLevelSegment = usesDmgWaveLevelSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasDmgStereoRouteSegment = usesDmgStereoRouteSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasYmEnvelopeShapeSegment = usesYmEnvelopeShapeSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasSidFilterRoutingControl = mode == chipper::ChipMode::sid
        && chipper::parameterSpecFor(mode, chipper::ChipParameterRole::sidFilterRouting) != nullptr
        && chipper::descriptorFor(mode).implemented;
    const auto hasYmChannelMixControls = usesYmChannelMixControls(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasSnNoiseModeSegment = usesSnNoiseModeSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasToneNoiseMixSegment = usesToneNoiseMixSegment(mode) && chipper::descriptorFor(mode).implemented;
    nativeSliders[0].setVisible(! hasPulseDutySegment);
    const auto embeddedPulseDuty = (mode == chipper::ChipMode::nes || mode == chipper::ChipMode::dmg) && hasPulseDutySegment;
    nativeGroupLabels[0].setVisible(! embeddedPulseDuty);
    nativeLabels[0].setVisible(! embeddedPulseDuty || hasPulseDutySegment);
    controlValueLabels[0].setVisible(! embeddedPulseDuty || hasPulseDutySegment);
    nativeSliders[3].setVisible(! hasToneNoiseMixSegment);
    updatePulseDutyButtons(patch.control1, hasPulseDutySegment);
    updatePulse2DutyButtons(patch, hasPulse2DutySegment);
    updateToneNoiseMixButtons(patch.control4, hasToneNoiseMixSegment);
    updateWaveShapeButtons(waveShape, hasWaveShapeSegment);
    updateDmgWaveLevelButtons(patch, hasDmgWaveLevelSegment);
    updateDmgStereoRouteButtons(mode, patch, hasDmgStereoRouteSegment);
    updateYmEnvelopeShapeButtons(mode, patch, hasYmEnvelopeShapeSegment);
    updateSidFilterRoutingControl(hasSidFilterRoutingControl);
    updateYmChannelMixControls(hasYmChannelMixControls);
    updateSnNoiseModeButtons(mode, patch, hasSnNoiseModeSegment);
    updateEnvelopeDecayReadout(mode);
    updateSidAdsrControls(mode == chipper::ChipMode::sid && usesEnvelopeDecayControl(mode) && chipper::descriptorFor(mode).implemented);
    updateFmOperatorRegisterSurface(mode,
                                    patch,
                                    chipper::descriptorFor(mode).implemented
                                        && (mode == chipper::ChipMode::ym2612
                                            || mode == chipper::ChipMode::opl3
                                            || mode == chipper::ChipMode::ym2151));
    updateStereoSpreadReadout(mode);

    const auto macroReadout = [this, mode](size_t index, juce::String text)
    {
        return performanceMacroReadout(mode, index, std::move(text));
    };

    if (mode == chipper::ChipMode::nes)
    {
        controlValueLabels[0].setText(embeddedPulseDuty ? pulseDutyReadout(mode, patch.control1) : macroReadout(0, pulseDutyReadout(mode, patch.control1)),
                                      juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, nesSweepReadout(patch.control2)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, nesNoiseReadout(patch.control3)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, nesFocusReadout(patch.control4)), juce::dontSendNotification);

        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::dmg)
    {
        controlValueLabels[0].setText(embeddedPulseDuty ? pulseDutyReadout(mode, patch.control1) : macroReadout(0, pulseDutyReadout(mode, patch.control1)),
                                      juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, dmgSweepReadout(patch.control2)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, dmgNoiseReadout(patch.control3)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, dmgEnvelopeReadout(patch.control4)), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::ym2149)
    {
        controlValueLabels[0].setText(macroReadout(0, ymSpreadReadout(patch.control1)), juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, ymMotionReadout(patch.control2)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, ymNoiseReadout(patch.control3)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, ymToneNoiseReadout(patch.control4) + " | " + ymChannelMixReadout(patch)), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::sn76489)
    {
        controlValueLabels[0].setText(macroReadout(0, snStackReadout(patch.control1)), juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, snMotionReadout(patch.control2)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, snNoiseModeReadout(patch)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, snLevelReadout(patch.control4)), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::sid)
    {
        controlValueLabels[0].setText(macroReadout(0, sidPulseWidthReadout(patch.control1)), juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, sidDetuneReadout(patch.control2)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, sidCutoffReadout(patch.control3)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, sidSustainReadout(patch)), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::pokey)
    {
        controlValueLabels[0].setText(macroReadout(0, waveShapeReadout(mode, patch.waveShape)), juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, pokeyRegisterReadout(patch)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, "Poly/timer bias " + juce::String(patch.control3, 2)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, "AUDV volume " + juce::String(static_cast<int>(std::round(patch.control4 * 15.0f))) + "/15"), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::spc700)
    {
        controlValueLabels[0].setText(macroReadout(0, waveShapeReadout(mode, patch.waveShape)), juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, spc700PitchMotionReadout(patch)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, spc700EchoReadout(patch)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, sampleChipReadout(mode, patch)), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::paula)
    {
        controlValueLabels[0].setText(macroReadout(0, waveShapeReadout(mode, patch.waveShape)), juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, "Pitch/rate motion " + juce::String(patch.control2, 2)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, "Sample color " + juce::String(patch.control3, 2)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, sampleChipReadout(mode, patch)), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
    {
        controlValueLabels[0].setText(macroReadout(0, "Channel spread " + juce::String(static_cast<int>(std::round(patch.control1 * 12.0f))) + " st"), juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, "Pitch motion " + juce::String(patch.control2, 2)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2, waveShapeReadout(mode, patch.waveShape)), juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, wavetableChipReadout(mode, patch)), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2413)
    {
        controlValueLabels[0].setText(macroReadout(0, fmChipReadout(mode, patch)), juce::dontSendNotification);
        controlValueLabels[1].setText(macroReadout(1, fmFeedbackReadout(mode, patch)), juce::dontSendNotification);
        controlValueLabels[2].setText(macroReadout(2,
                                                   mode == chipper::ChipMode::ym2612
                                                       ? ym2612DacModeReadout(patch)
                                                       : (mode == chipper::ChipMode::ym2151 ? ym2151NoiseReadout(patch) : waveShapeReadout(mode, patch.waveShape))),
                                      juce::dontSendNotification);
        controlValueLabels[3].setText(macroReadout(3, "FM output level " + juce::String(static_cast<int>(std::round(patch.control4 * 15.0f))) + "/15"), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else
    {
        controlValueLabels[0].setText(juce::String(patch.control1, 2), juce::dontSendNotification);
        controlValueLabels[1].setText(juce::String(patch.control2, 2), juce::dontSendNotification);
        controlValueLabels[2].setText(juce::String(patch.control3, 2), juce::dontSendNotification);
        controlValueLabels[3].setText(juce::String(patch.control4, 2), juce::dontSendNotification);
    }

    if (mode == chipper::ChipMode::nes)
    {
        controlValueLabels[4].setText(nesDmcDirectReadout(parameterValue(chipper::parameters::id::nesDmcDirectLevel)), juce::dontSendNotification);
        controlValueLabels[4].setTooltip(withMidiCcForRole("RP2A03 $4011 DMC direct DAC load. Renderer and VST playback can step external .dmc bytes supplied by the user.", chipper::ChipParameterRole::nesDmcDirectLevel));
        updateDmcSampleControls();
    }
    else if (mode == chipper::ChipMode::spc700)
    {
        controlValueLabels[4].setText("SPC700 sample import", juce::dontSendNotification);
        controlValueLabels[4].setTooltip("Loads BRR, WAV, or AIFF samples into the SPC700-style sample voice path.");
        updateSpc700BrrSampleControls();
    }
    else if (mode == chipper::ChipMode::paula)
    {
        controlValueLabels[4].setText("Paula sample import", juce::dontSendNotification);
        controlValueLabels[4].setTooltip("Loads WAV/AIFF or uncompressed IFF/8SVX into the Paula-style 8-bit sample voice path.");
        updatePaulaSampleControls();
    }
    else
    {
        const auto clock = parameterValue(chipper::parameters::id::clockHz);
        const auto defaultClock = chipper::parameters::defaultClockForMode(mode);
        const auto clockText = clock <= 0.0f
            ? juce::String("Default ") + juce::String(defaultClock / 1000000.0, 2) + " MHz"
            : juce::String("Override ") + juce::String(static_cast<double>(clock) / 1000000.0, 2) + " MHz";
        controlValueLabels[4].setText(clockText, juce::dontSendNotification);
        controlValueLabels[4].setTooltip(withMidiCc("Optional chip clock override. Zero uses the documented default for the selected mode.", chipper::parameters::id::clockHz));
    }

    const auto outputDb = parameterValue(chipper::parameters::id::outputDb);
    controlValueLabels[5].setText(juce::String("Output ") + juce::String(outputDb, 1) + " dB", juce::dontSendNotification);
}
