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
                           ? "Checked WAV/AIFF files feed the 32-slot Paula-style sample bank and note map. Folder contents stay local."
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
                                                   paulaMode ? "No WAV/AIFF files loaded" : (spc700Mode ? "No SPC700 samples loaded" : "No .dmc files loaded"), {}, 0, false, false },
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
    return label == "Macro" ? juce::String("Follow") : juce::String(label);
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
    if (sourceIndex == 1 && patch.playMode != chipper::PlayMode::chipPoly)
        return std::min(primary + 1, 3);

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

ChipWaveformPreviewShape wavetablePreviewShape(const chipper::PatchConfig& patch)
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

template <typename ButtonArray>
void layoutSegmentedButtons(ButtonArray& buttons, juce::Rectangle<int> bounds, size_t visibleCount)
{
    const auto count = std::max<size_t>(1u, std::min(visibleCount, buttons.size()));
    const auto gap = 4;
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

    const auto graph = bounds.reduced(5.0f, 4.0f);
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

    auto graph = bounds.reduced(7.0f, 5.0f);
    if (graph.getWidth() < 80.0f || graph.getHeight() < 34.0f)
        return;

    const auto opRadius = std::clamp(std::min(graph.getWidth(), graph.getHeight()) * 0.075f, 4.0f, 8.0f);
    const auto carrierColour = juce::Colour(0xfff0c94d);
    const auto modColour = juce::Colour(0xff56c7d8);
    const auto mutedColour = juce::Colour(0xff72818a);
    const auto lineColour = juce::Colour(0xff56c7d8).withAlpha(follow ? 0.55f : 0.78f);

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
    }

    g.setColour(carrierColour.withAlpha(follow ? 0.70f : 0.95f));
    g.fillRoundedRectangle(outX - 7.0f, yMid - 9.0f, 14.0f, 18.0f, 2.0f);
    g.setColour(juce::Colour(0xff101414));
    g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
    g.drawText("OUT", juce::Rectangle<float> { outX - 15.0f, yMid - 8.0f, 30.0f, 16.0f }, juce::Justification::centred);

    g.setColour(follow ? juce::Colour(0xffaebbc4) : juce::Colour(0xffdbe8e5));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(juce::String(follow ? "Follow -> " : "") + "Algorithm " + juce::String(algorithm),
               graph.removeFromTop(13.0f),
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
    g.drawText(juce::String(follow ? "Follow -> " : "") + "OPL W" + juce::String(waveform),
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

ChipperAudioProcessorEditor::ChipperAudioProcessorEditor(ChipperAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      audioProcessor(processor)
{
    setResizable(true, true);
    setResizeLimits(1180, 920, 1600, 1280);
    setSize(1180, 1040);

    auto& state = audioProcessor.getValueTreeState();

    titleLabel.setText("Chipper", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff7d85a));
    titleLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    coreReadinessLabel.setJustificationType(juce::Justification::centred);
    coreReadinessLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    coreReadinessLabel.setMinimumHorizontalScale(0.70f);
    coreReadinessLabel.setColour(juce::Label::textColourId, juce::Colour(0xff101414));
    coreReadinessLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff56c7d8));
    addAndMakeVisible(coreReadinessLabel);

    chipModeBox.addItemList(chipper::parameters::chipModeChoices(), 1);
    accuracyBox.addItemList(chipper::parameters::accuracyChoices(), 1);
    presetBox.setTextWhenNothingSelected("Browse Chip Presets");
    macroBox.addItemList(chipper::parameters::macroChoices(), 1);
    playModeBox.addItemList(chipper::parameters::playModeChoices(), 1);
    chipModeBox.setTooltip(withMidiCc("Selects the chip model or planned chip family.", chipper::parameters::id::chipMode));
    accuracyBox.setTooltip(withMidiCc("Selects the requested accuracy tier for the active core.", chipper::parameters::id::accuracy));
    presetBox.setTooltip("Browse factory presets for the selected chip mode. Choosing one applies the sound immediately.");
    macroBox.setTooltip(withMidiCc("Applies a chip-specific musical register template.", chipper::parameters::id::macro));
    playModeBox.setTooltip(withMidiCc("Chooses how incoming notes use the chip channels inside one patch.", chipper::parameters::id::playMode));

    const std::array<const char*, 5> headerNames { "Preset", "Chip Mode", "Accuracy", "Template", "Play Mode" };
    for (size_t i = 0; i < headerControlLabels.size(); ++i)
    {
        headerControlLabels[i].setText(headerNames[i], juce::dontSendNotification);
        headerControlLabels[i].setJustificationType(juce::Justification::centredLeft);
        headerControlLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        headerControlLabels[i].setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(headerControlLabels[i]);
    }

    addAndMakeVisible(presetBox);
    addAndMakeVisible(chipModeBox);
    addAndMakeVisible(accuracyBox);
    addAndMakeVisible(macroBox);
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

    dmcLoopButton.setButtonText("Loop");
    dmcLoopButton.setTooltip(withMidiCcForRole("RP2A03 $4010 DMC loop bit. Off plays one-shot; Loop repeats the selected DMC sample from the first byte.", chipper::ChipParameterRole::nesDmcLoop));
    addAndMakeVisible(dmcLoopButton);
    dmcLoopAttachment = std::make_unique<ButtonAttachment>(state, chipper::parameters::id::nesDmcLoop, dmcLoopButton);

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
    envelopeDecaySlider.setTooltip(withMidiCc("Maps musical decay to the active chip's hardware envelope period. Zero preserves the macro's original envelope/level behavior.", chipper::parameters::id::envelopeDecay));
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
            return choice <= 0 ? juce::String("Follow") : juce::String(choice - 1);
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

    const std::array<const char*, pulse2DutyCount> pulse2Labels { "Follow", "12.5%", "25%", "50%", "75%" };
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

    const std::array<const char*, dmgWaveLevelCount> dmgWaveLevelLabels { "Follow", "Mute", "100%", "50%", "25%" };
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

    const std::array<const char*, dmgStereoRouteCount> dmgStereoRouteLabels { "Follow", "Both", "Left", "Right", "Split" };
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

    const std::array<const char*, snNoiseModeCount> snNoiseLabels { "Follow", "P-Lo", "P-Hi", "W-Lo", "W-T3" };
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
        moduleNumberLabels[i].setText(juce::String(static_cast<int>(i + 1)), juce::dontSendNotification);
        moduleNumberLabels[i].setFont(juce::FontOptions(13.0f, juce::Font::bold));
        moduleNumberLabels[i].setJustificationType(juce::Justification::centred);
        moduleNumberLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xff101414));
        moduleNumberLabels[i].setColour(juce::Label::backgroundColourId, juce::Colour(0xfff0c94d));
        addAndMakeVisible(moduleNumberLabels[i]);

        moduleTitleLabels[i].setFont(juce::FontOptions(15.0f, juce::Font::bold));
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

    globalStripLabel.setText("Performance", juce::dontSendNotification);
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

void ChipperAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101414));
    g.setColour(juce::Colour(0xff314047));
    g.drawHorizontalLine(84, 16.0f, static_cast<float>(getWidth() - 16));
    g.drawHorizontalLine(getHeight() - 62, 16.0f, static_cast<float>(getWidth() - 16));

    for (const auto& bounds : moduleBounds)
    {
        if (bounds.isEmpty())
            continue;

        const auto panel = bounds.toFloat();
        g.setColour(juce::Colour(0xff182125));
        g.fillRoundedRectangle(panel, 6.0f);
        g.setColour(juce::Colour(0xff34474c));
        g.drawRoundedRectangle(panel.reduced(0.5f), 6.0f, 1.0f);
    }

    if (usesSourceChannelSurface(displayedMode))
    {
        for (const auto& bounds : sourceChannelBounds)
        {
            if (bounds.isEmpty())
                continue;

            const auto card = bounds.toFloat();
            g.setColour(juce::Colour(0xff202c33));
            g.fillRoundedRectangle(card, 4.0f);
            g.setColour(juce::Colour(0xff56c7d8).withAlpha(0.70f));
            g.drawRoundedRectangle(card.reduced(0.5f), 4.0f, 1.0f);
        }
    }

    if (! globalStripBounds.isEmpty())
    {
        const auto strip = globalStripBounds.toFloat();
        g.setColour(juce::Colour(0xff171c20));
        g.fillRoundedRectangle(strip, 6.0f);
        g.setColour(juce::Colour(0xff344047));
        g.drawRoundedRectangle(strip.reduced(0.5f), 6.0f, 1.0f);
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

    titleLabel.setBounds(top.removeFromLeft(122));
    top.removeFromLeft(8);
    placeHeaderCombo(0, presetBox, top.removeFromLeft(210));
    top.removeFromLeft(8);
    placeHeaderCombo(1, chipModeBox, top.removeFromLeft(210));
    top.removeFromLeft(8);
    placeHeaderCombo(2, accuracyBox, top.removeFromLeft(112));
    top.removeFromLeft(8);
    placeHeaderCombo(3, macroBox, top.removeFromLeft(136));
    top.removeFromLeft(8);
    placeHeaderCombo(4, playModeBox, top.removeFromLeft(136));
    top.removeFromLeft(8);
    coreReadinessLabel.setBounds(top.removeFromTop(44).reduced(0, 12));

    area.removeFromTop(10);
    chipSummaryLabel.setBounds(area.removeFromTop(34));
    area.removeFromTop(10);

    constexpr auto footerReserve = 52;
    const auto sidLayout = displayedMode == chipper::ChipMode::sid;
    const auto spc700Layout = displayedMode == chipper::ChipMode::spc700;
    const auto performanceStripHeight = sidLayout ? 260 : (spc700Layout ? 280 : 300);
    const auto maxModulesHeight = sidLayout ? 620 : (spc700Layout ? 540 : 492);
    const auto modulesHeight = std::clamp(area.getHeight() - footerReserve - 12 - performanceStripHeight, 410, maxModulesHeight);
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
    else if (spc700Layout)
    {
        const auto topRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.24)), 108, 136);
        const auto sourceRowHeight = std::clamp(static_cast<int>(std::round(static_cast<double>(modules.getHeight()) * 0.38)), 172, 220);
        const auto bottomRowHeight = std::max(128, modules.getHeight() - topRowHeight - sourceRowHeight - (gap * 2));
        const auto leftX = modules.getX();
        const auto rightX = modules.getX() + columnWidth + gap;
        const auto bottomColumnWidth = (modules.getWidth() - (gap * 2)) / 3;
        const auto topY = modules.getY();
        const auto sourceY = topY + topRowHeight + gap;
        const auto bottomY = sourceY + sourceRowHeight + gap;

        moduleBounds[0] = { leftX, topY, columnWidth, topRowHeight };
        moduleBounds[1] = { modules.getX(), sourceY, modules.getWidth(), sourceRowHeight };
        moduleBounds[2] = { rightX, topY, columnWidth, topRowHeight };
        moduleBounds[3] = { modules.getX(), bottomY, bottomColumnWidth, bottomRowHeight };
        moduleBounds[4] = { modules.getX() + bottomColumnWidth + gap, bottomY, bottomColumnWidth, bottomRowHeight };
        moduleBounds[5] = { modules.getX() + ((bottomColumnWidth + gap) * 2), bottomY, bottomColumnWidth, bottomRowHeight };
    }
    else
    {
        for (size_t i = 0; i < moduleBounds.size(); ++i)
        {
            const auto row = static_cast<int>(i / 2);
            const auto column = static_cast<int>(i % 2);
            const auto x = modules.getX() + (column * (columnWidth + gap));
            const auto y = modules.getY() + (row * (rowHeight + gap));
            moduleBounds[i] = { x, y, columnWidth, rowHeight };
        }
    }

    for (size_t i = 0; i < moduleBounds.size(); ++i)
    {
        if (moduleBounds[i].isEmpty())
        {
            moduleNumberLabels[i].setBounds({});
            moduleTitleLabels[i].setBounds({});
            moduleSummaryLabels[i].setBounds({});
            for (auto& itemLabel : moduleItemLabels[i])
                itemLabel.setBounds({});
            continue;
        }

        auto panel = moduleBounds[i].reduced(12, 9);
        auto titleRow = panel.removeFromTop(20);
        moduleNumberLabels[i].setBounds(titleRow.removeFromLeft(22));
        titleRow.removeFromLeft(6);
        moduleTitleLabels[i].setBounds(titleRow);
        moduleSummaryLabels[i].setBounds(panel.removeFromTop(30));
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
    const auto sourceColumns = useSpc700VoiceGrid ? 4 : static_cast<int>(visibleSourceCards);
    const auto sourceRows = useSpc700VoiceGrid ? static_cast<int>((visibleSourceCards + 3u) / 4u) : 1;
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

        const auto sourceColumn = useSpc700VoiceGrid ? static_cast<int>(i % 4u) : static_cast<int>(i);
        const auto sourceRow = useSpc700VoiceGrid ? static_cast<int>(i / 4u) : 0;
        sourceChannelBounds[i] = {
            sourcePanel.getX() + (sourceColumn * (sourceCardWidth + sourceGap)),
            sourcePanel.getY() + (sourceRow * (sourceCardHeight + sourceGap)),
            sourceCardWidth,
            sourceCardHeight
        };
        const auto isSidSourceCard = displayedMode == chipper::ChipMode::sid;
        auto sourceCard = sourceChannelBounds[i].reduced(useSpc700VoiceGrid ? 5 : 8, isSidSourceCard ? 2 : 4);
        const auto buttonHeight = useSpc700VoiceGrid ? 15 : (isSidSourceCard ? 17 : 18);
        sourceChannelButtons[i].setBounds(sourceCard.removeFromTop(std::min(buttonHeight, sourceCard.getHeight())));
        sourceCard.removeFromTop(2);
        const auto previewHeight = std::clamp(sourceCard.getHeight() / (useSpc700VoiceGrid ? 3 : 4),
                                              useSpc700VoiceGrid ? 13 : (isSidSourceCard ? 22 : 20),
                                              useSpc700VoiceGrid ? 20 : (isSidSourceCard ? 32 : 28));
        sourcePreviewScopes[i].setBounds(sourceCard.removeFromTop(std::min(previewHeight, sourceCard.getHeight())));
        sourceCard.removeFromTop(1);

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

        auto levelRow = sourceCard.removeFromTop(std::min(useSpc700VoiceGrid ? 10 : 12, sourceCard.getHeight()));
        sourceLevelLabels[i].setBounds(levelRow.removeFromLeft(std::min(useSpc700VoiceGrid ? 34 : 48, levelRow.getWidth())));
        sourceLevelValueLabels[i].setBounds(levelRow);
        sourceCard.removeFromTop(1);
        sourceLevelSliders[i].setBounds(sourceCard.removeFromTop(std::min(useSpc700VoiceGrid ? 10 : (isSidSourceCard ? 11 : 14), sourceCard.getHeight())).reduced(0, 1));
    }

    auto tonePanel = moduleBounds[2].reduced(12, 9);
    tonePanel.removeFromTop(20);
    if (displayedMode == chipper::ChipMode::sid)
        tonePanel.removeFromTop(4);
    else
    {
        tonePanel.removeFromTop(30);
        tonePanel.removeFromTop(4);
    }
    auto primaryTonePanel = tonePanel;
    auto secondaryTonePanel = tonePanel;
    if (displayedMode == chipper::ChipMode::nes
        || displayedMode == chipper::ChipMode::dmg
        || displayedMode == chipper::ChipMode::ym2149)
    {
        primaryTonePanel = tonePanel.removeFromTop(58);
        tonePanel.removeFromTop(6);
        secondaryTonePanel = tonePanel;
    }

    if (displayedMode == chipper::ChipMode::sid)
    {
        const auto placeFilterSlider = [](juce::Slider& slider,
                                          juce::Label& label,
                                          juce::Label& valueLabel,
                                          juce::Rectangle<int> bounds)
        {
            auto header = bounds.removeFromTop(std::min(18, bounds.getHeight()));
            label.setBounds(header.removeFromLeft(std::min(92, header.getWidth())));
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
    else
        placeWaveShapeSegment(primaryTonePanel);
    if (displayedMode != chipper::ChipMode::sid)
    {
        placeDmgWaveLevelSegment(secondaryTonePanel);
        if (displayedMode != chipper::ChipMode::spc700)
            placeYmEnvelopeShapeSegment(displayedMode == chipper::ChipMode::ym2149 ? secondaryTonePanel : primaryTonePanel);
    }

    auto motionPanel = moduleBounds[4].reduced(12, 9);
    motionPanel.removeFromTop(20);
    motionPanel.removeFromTop(30);
    motionPanel.removeFromTop(4);
    placeSnNoiseModeSegment(displayedMode == chipper::ChipMode::sid
                                ? motionPanel
                                : (displayedMode == chipper::ChipMode::nes ? secondaryTonePanel : primaryTonePanel));

    auto envelopePanel = moduleBounds[3].reduced(12, 9);
    envelopePanel.removeFromTop(20);
    if (displayedMode == chipper::ChipMode::sid)
        envelopePanel.removeFromTop(4);
    else
    {
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
        auto shapeArea = envelopeDecayPanel.removeFromTop(std::min(58, envelopeDecayPanel.getHeight()));
        placeYmEnvelopeShapeSegment(shapeArea);
        envelopeDecayPanel.removeFromTop(6);
        placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, envelopeDecayPanel);
        ymEnvelopePreview.setBounds({});
    }
    else
    {
        placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, envelopeDecayPanel);
        ymEnvelopePreview.setBounds({});
    }

    auto outputPanel = moduleBounds[5].reduced(12, 9);
    outputPanel.removeFromTop(20);
    outputPanel.removeFromTop(30);
    outputPanel.removeFromTop(4);
    outputPanel.removeFromBottom(6);
    if (displayedMode != chipper::ChipMode::sid)
        placeLabeledSliderWithReadout(stereoSpreadSlider, stereoSpreadLabel, stereoSpreadValueLabel, outputPanel);

    auto profilePanel = moduleBounds[0].reduced(12, 9);
    profilePanel.removeFromTop(20);
    profilePanel.removeFromTop(30);
    profilePanel.removeFromTop(4);
    placeDmgStereoRouteSegment(displayedMode == chipper::ChipMode::sid ? profilePanel : outputPanel);

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

    placeGroupedSlider(nativeSliders[0], nativeGroupLabels[0], nativeLabels[0], controlValueLabels[0], controlCells[0]);
    placePulseDutySegment(controlCells[0]);
    placeGroupedSlider(nativeSliders[1], nativeGroupLabels[1], nativeLabels[1], controlValueLabels[1], controlCells[1]);
    if (displayedMode != chipper::ChipMode::sid)
        placeGroupedSlider(nativeSliders[2], nativeGroupLabels[2], nativeLabels[2], controlValueLabels[2], controlCells[2]);

    const auto sustainCell = displayedMode == chipper::ChipMode::sid ? controlCells[2] : controlCells[3];
    placeGroupedSlider(nativeSliders[3], nativeGroupLabels[3], nativeLabels[3], controlValueLabels[3], sustainCell);
    placeToneNoiseMixSegment(sustainCell);

    const auto utilityCell = displayedMode == chipper::ChipMode::sid ? controlCells[3] : controlCells[4];
    if (displayedMode == chipper::ChipMode::nes)
    {
        clockSlider.setBounds({});
        clockLabel.setBounds({});
        auto dmcCell = utilityCell;
        auto directCell = dmcCell.removeFromTop(std::max(44, dmcCell.getHeight() / 2));
        auto rateCell = directCell.removeFromRight(std::max(116, directCell.getWidth() / 3));
        directCell.removeFromRight(8);
        placeLabeledSliderWithReadout(dmcDirectSlider, dmcDirectLabel, controlValueLabels[4], directCell);
        dmcRateLabel.setBounds(rateCell.removeFromTop(16));
        dmcRateBox.setBounds(rateCell.removeFromTop(24).reduced(0, 1));
        dmcCell.removeFromTop(4);
        auto sampleHeader = dmcCell.removeFromTop(20);
        dmcSampleLabel.setText("DMC", juce::dontSendNotification);
        dmcSampleLabel.setBounds(sampleHeader.removeFromLeft(42));
        dmcPlaybackModeBox.setBounds(sampleHeader.removeFromLeft(104).reduced(0, 1));
        sampleHeader.removeFromLeft(4);
        const auto buttonWidth = std::max(42, (sampleHeader.getWidth() - 12) / 3);
        dmcSampleFileButton.setBounds(sampleHeader.removeFromLeft(buttonWidth).reduced(0, 1));
        sampleHeader.removeFromLeft(4);
        dmcSampleFolderButton.setBounds(sampleHeader.removeFromLeft(buttonWidth).reduced(0, 1));
        sampleHeader.removeFromLeft(4);
        dmcSampleBankButton.setBounds(sampleHeader.removeFromLeft(buttonWidth).reduced(0, 1));
        dmcCell.removeFromTop(2);
        auto sampleRow = dmcCell.removeFromTop(22);
        auto loopCell = sampleRow.removeFromRight(62);
        sampleRow.removeFromRight(6);
        auto rootCell = sampleRow.removeFromRight(78);
        sampleRow.removeFromRight(6);
        dmcSampleSlotBox.setBounds(sampleRow.reduced(0, 1));
        dmcMapRootBox.setBounds(rootCell.reduced(0, 1));
        dmcLoopButton.setBounds(loopCell.reduced(0, 1));
        dmcCell.removeFromTop(2);
        dmcSampleStatusLabel.setBounds(dmcCell.reduced(0, 1));
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

        auto sampleCell = utilityCell;
        auto sampleHeader = sampleCell.removeFromTop(22);
        dmcSampleLabel.setText(displayedMode == chipper::ChipMode::paula ? "Paula Sample" : "SPC700 Sample", juce::dontSendNotification);
        dmcSampleLabel.setBounds(sampleHeader.removeFromLeft(displayedMode == chipper::ChipMode::paula ? 104 : 92));
        const auto brrButtonWidth = std::max(46, (sampleHeader.getWidth() - 8) / 3);
        dmcSampleFileButton.setBounds(sampleHeader.removeFromLeft(brrButtonWidth).reduced(0, 1));
        sampleHeader.removeFromLeft(4);
        dmcSampleFolderButton.setBounds(sampleHeader.removeFromLeft(brrButtonWidth).reduced(0, 1));
        sampleHeader.removeFromLeft(4);
        dmcSampleBankButton.setBounds(sampleHeader.removeFromLeft(brrButtonWidth).reduced(0, 1));
        sampleCell.removeFromTop(4);
        auto playbackRow = sampleCell.removeFromTop(24);
        dmcPlaybackModeBox.setBounds(playbackRow.reduced(0, 1));
        sampleCell.removeFromTop(4);
        auto sampleRow = sampleCell.removeFromTop(22);
        auto rootCell = sampleRow.removeFromRight(86);
        sampleRow.removeFromRight(6);
        dmcSampleSlotBox.setBounds(sampleRow.reduced(0, 1));
        dmcMapRootBox.setBounds(rootCell.reduced(0, 1));
        sampleCell.removeFromTop(3);
        dmcSampleStatusLabel.setBounds(sampleCell.reduced(0, 1));
    }
    else
    {
        dmcDirectSlider.setBounds({});
        dmcDirectLabel.setBounds({});
        dmcRateLabel.setBounds({});
        dmcRateBox.setBounds({});
        dmcSampleLabel.setBounds({});
        dmcSampleStatusLabel.setBounds({});
        dmcSampleFileButton.setBounds({});
        dmcSampleFolderButton.setBounds({});
        dmcSampleBankButton.setBounds({});
        dmcSampleSlotBox.setBounds({});
        dmcPlaybackModeBox.setBounds({});
        dmcMapRootBox.setBounds({});
        dmcLoopButton.setBounds({});
        placeLabeledSliderWithReadout(clockSlider, clockLabel, controlValueLabels[4], utilityCell);
    }

    auto outputCell = displayedMode == chipper::ChipMode::sid ? controlCells[4].getUnion(controlCells[5]) : controlCells[5];
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
    label.setBounds(bounds.removeFromTop(20));
    slider.setBounds(bounds.removeFromTop(30).reduced(0, 2));
    valueLabel.setBounds(bounds);
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

        const auto minimumControlHeight = 46;
        const auto previewReserve = voiceColumn.getHeight() >= 86 ? 42 : (voiceColumn.getHeight() >= 70 ? 32 : 0);
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
            sidEnvelopePreviews[voice].setBounds(voiceColumn.removeFromTop(std::min(40, voiceColumn.getHeight())).reduced(0, 1));
        else
            sidEnvelopePreviews[voice].setBounds({});
    }
}

void ChipperAudioProcessorEditor::placePulseDutySegment(juce::Rectangle<int> bounds)
{
    if (usesPulse2DutySegment(displayedMode))
    {
        nativeGroupLabels[0].setBounds(bounds.removeFromTop(13));

        auto pulse1Header = bounds.removeFromTop(14);
        nativeLabels[0].setBounds(pulse1Header.removeFromLeft(118));
        controlValueLabels[0].setJustificationType(juce::Justification::centredRight);
        controlValueLabels[0].setBounds(pulse1Header);

        pulseDutySegmentBounds = bounds.removeFromTop(22).reduced(0, 1);
        layoutSegmentedButtons(pulseDutyButtons, pulseDutySegmentBounds, pulseDutyButtons.size());

        bounds.removeFromTop(3);
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
    auto header = bounds.removeFromTop(14);
    pulse2DutyLabel.setBounds(header.removeFromLeft(118));
    pulse2DutyValueLabel.setBounds(header);

    pulse2DutySegmentBounds = bounds.removeFromTop(22).reduced(0, 1);
    layoutSegmentedButtons(pulse2DutyButtons, pulse2DutySegmentBounds, pulse2DutyButtons.size());
}

void ChipperAudioProcessorEditor::placeWaveShapeSegment(juce::Rectangle<int> bounds)
{
    waveShapeLabel.setBounds(bounds.removeFromTop(18));
    waveShapeSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
    layoutSegmentedButtons(waveShapeButtons, waveShapeSegmentBounds, waveShapeButtons.size());

    waveShapeValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeFmAlgorithmControl(juce::Rectangle<int> bounds)
{
    auto header = bounds.removeFromTop(std::min(18, bounds.getHeight()));
    waveShapeLabel.setBounds(header.removeFromLeft(std::min(92, header.getWidth())));
    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setBounds(header);
    bounds.removeFromTop(3);

    auto controlRow = bounds.removeFromTop(std::min(28, bounds.getHeight()));
    fmAlgorithmBox.setBounds(controlRow.removeFromLeft(std::max(132, controlRow.getWidth() / 3)).reduced(0, 1));
    bounds.removeFromTop(5);
    fmAlgorithmPreview.setBounds(bounds.reduced(0, 1));
    waveShapeSegmentBounds = {};
    for (auto& button : waveShapeButtons)
        button.setBounds({});
}

void ChipperAudioProcessorEditor::placeOplWaveformControl(juce::Rectangle<int> bounds)
{
    auto header = bounds.removeFromTop(std::min(18, bounds.getHeight()));
    waveShapeLabel.setBounds(header.removeFromLeft(std::min(92, header.getWidth())));
    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setBounds(header);
    bounds.removeFromTop(3);

    auto controlRow = bounds.removeFromTop(std::min(28, bounds.getHeight()));
    oplWaveformBox.setBounds(controlRow.removeFromLeft(std::max(132, controlRow.getWidth() / 3)).reduced(0, 1));
    bounds.removeFromTop(5);
    oplWaveformPreview.setBounds(bounds.reduced(0, 1));
    waveShapeSegmentBounds = {};
    for (auto& button : waveShapeButtons)
        button.setBounds({});
}

void ChipperAudioProcessorEditor::placeOpllInstrumentControl(juce::Rectangle<int> bounds)
{
    auto header = bounds.removeFromTop(std::min(18, bounds.getHeight()));
    waveShapeLabel.setBounds(header.removeFromLeft(std::min(96, header.getWidth())));
    waveShapeValueLabel.setJustificationType(juce::Justification::centredRight);
    waveShapeValueLabel.setBounds(header);
    bounds.removeFromTop(3);

    opllInstrumentBox.setBounds(bounds.removeFromTop(std::min(28, bounds.getHeight())).reduced(0, 1));
    waveShapeSegmentBounds = {};
    for (auto& button : waveShapeButtons)
        button.setBounds({});
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

void ChipperAudioProcessorEditor::placeDmgWaveLevelSegment(juce::Rectangle<int> bounds)
{
    dmgWaveLevelLabel.setBounds(bounds.removeFromTop(18));
    dmgWaveLevelSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
    layoutSegmentedButtons(dmgWaveLevelButtons, dmgWaveLevelSegmentBounds, dmgWaveLevelButtons.size());

    dmgWaveLevelValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeDmgStereoRouteSegment(juce::Rectangle<int> bounds)
{
    dmgStereoRouteLabel.setBounds(bounds.removeFromTop(18));
    if (displayedMode == chipper::ChipMode::spc700 || displayedMode == chipper::ChipMode::paula)
    {
        dmgStereoRouteBox.setBounds(bounds.removeFromTop(std::min(28, bounds.getHeight())).reduced(0, 1));
        dmgStereoRouteSegmentBounds = {};
        for (auto& button : dmgStereoRouteButtons)
            button.setBounds({});
    }
    else
    {
        dmgStereoRouteSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
        layoutSegmentedButtons(dmgStereoRouteButtons, dmgStereoRouteSegmentBounds, dmgStereoRouteButtons.size());
        dmgStereoRouteBox.setBounds({});
    }

    dmgStereoRouteValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeYmEnvelopeShapeSegment(juce::Rectangle<int> bounds)
{
    const auto* spec = chipper::parameterSpecFor(displayedMode, chipper::ChipParameterRole::ymEnvelopeShape);
    const auto visibleCount = spec != nullptr
                                  ? std::min(ymEnvelopeShapeButtons.size(), spec->choices.size())
                                  : ymEnvelopeShapeButtons.size();

    if (spec != nullptr && spec->surface == chipper::ControlSurface::menu)
    {
        auto header = bounds.removeFromTop(std::min(16, bounds.getHeight()));
        ymEnvelopeShapeLabel.setBounds(header.removeFromLeft(std::min(96, header.getWidth())));
        ymEnvelopeShapeValueLabel.setJustificationType(juce::Justification::centredRight);
        ymEnvelopeShapeValueLabel.setBounds(header);
        bounds.removeFromTop(3);
        sidFilterModeBox.setBounds(bounds.removeFromTop(std::min(26, bounds.getHeight())).reduced(0, 1));
        ymEnvelopeShapeSegmentBounds = {};
        for (auto& button : ymEnvelopeShapeButtons)
            button.setBounds({});
        return;
    }

    ymEnvelopeShapeLabel.setBounds(bounds.removeFromTop(18));
    ymEnvelopeShapeSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
    layoutSegmentedButtons(ymEnvelopeShapeButtons, ymEnvelopeShapeSegmentBounds, visibleCount);

    ymEnvelopeShapeValueLabel.setJustificationType(juce::Justification::centredLeft);
    ymEnvelopeShapeValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeSidFilterRoutingControl(juce::Rectangle<int> bounds)
{
    auto header = bounds.removeFromTop(std::min(17, bounds.getHeight()));
    sidFilterRoutingLabel.setBounds(header.removeFromLeft(std::min(112, header.getWidth())));
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
    snNoiseModeLabel.setBounds(bounds.removeFromTop(18));
    if (displayedMode == chipper::ChipMode::sn76489)
    {
        snNoiseModeBox.setBounds(bounds.removeFromTop(std::min(28, bounds.getHeight())).reduced(0, 1));
        snNoiseModeSegmentBounds = {};
        for (auto& button : snNoiseModeButtons)
            button.setBounds({});
    }
    else
    {
        snNoiseModeSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
        layoutSegmentedButtons(snNoiseModeButtons, snNoiseModeSegmentBounds, snNoiseModeButtons.size());
        snNoiseModeBox.setBounds({});
    }

    snNoiseModeValueLabel.setBounds(bounds);
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

    presetBox.clear(juce::dontSendNotification);

    std::optional<chipper::ChipMode> currentSection;
    for (size_t i = 0; i < displayedPresets.size(); ++i)
    {
        const auto& preset = *displayedPresets[i];
        if (! currentSection.has_value() || *currentSection != preset.chip)
        {
            currentSection = preset.chip;
            presetBox.addSectionHeading(chipper::toString(preset.chip));
        }

        presetBox.addItem(juce::String(preset.category) + " / " + juce::String(preset.name),
                          static_cast<int>(i + 1u));
    }

    if (displayedPresets.empty())
    {
        headerControlLabels[0].setText("Preset", juce::dontSendNotification);
        presetBox.setText("No factory presets", juce::dontSendNotification);
        presetBox.setTooltip("No factory presets are active for this chip mode yet.");
    }
    else
    {
        const auto presetCount = static_cast<int>(displayedPresets.size());
        const auto& firstPreset = *displayedPresets.front();
        headerControlLabels[0].setText(juce::String("Preset (") + juce::String(presetCount) + ")",
                                       juce::dontSendNotification);
        presetBox.setSelectedId(0, juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected(juce::String(presetCount) + " presets - "
                                             + juce::String(firstPreset.name));
        presetBox.setTooltip("Browse " + juce::String(presetCount) + " factory presets for "
                             + juce::String(chipper::toString(mode))
                             + ". Choosing one applies an audible chip-specific sound immediately.");
    }
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
    ymChannelMixLabel.setTooltip("Per-channel AY mixer overrides. Follow uses the global Tone/Noise Mix control.");
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
    const auto presetCount = static_cast<int>(displayedPresets.size());
    if (displayedPresets.empty())
    {
        headerControlLabels[0].setText("Preset", juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected("No factory presets");
        presetBox.setTooltip("No factory presets are active for this chip mode yet.");
    }
    else
    {
        const auto& firstPreset = *displayedPresets.front();
        headerControlLabels[0].setText(juce::String("Preset (") + juce::String(presetCount) + ")",
                                       juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected(juce::String(presetCount) + " presets - "
                                             + juce::String(firstPreset.name));
        presetBox.setTooltip("Browse " + juce::String(presetCount) + " factory presets for "
                             + juce::String(chipper::toString(mode))
                             + ". Choosing one applies an audible chip-specific sound immediately.");
    }

    updateLiveControlReadouts();
}

void ChipperAudioProcessorEditor::applySelectedPreset()
{
    if (suppressPresetApply)
        return;

    const auto selected = presetBox.getSelectedId() - 1;
    if (selected < 0 || static_cast<size_t>(selected) >= displayedPresets.size())
        return;

    applyFactoryPreset(*displayedPresets[static_cast<size_t>(selected)]);
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
            && samplePlaybackMode == 2);
}

bool ChipperAudioProcessorEditor::usesPulseDutySegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::macroControl1,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesPulse2DutySegment(chipper::ChipMode mode) const
{
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
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::dmgWaveLevel,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesDmgStereoRouteSegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::dmgStereoRoute,
                                            chipper::ControlSurface::segmentedChoice)
        || chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::dmgStereoRoute,
                                            chipper::ControlSurface::menu);
}

bool ChipperAudioProcessorEditor::usesYmEnvelopeShapeSegment(chipper::ChipMode mode) const
{
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
        return label + " -> " + pulseDutyReadout(mode, patch.control1) + " | " + dmgWaveLevelReadout(patch) + " | " + dmgStereoRouteReadout(patch) + laneText;

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

    if (mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2413)
        return label + " -> " + fmChipReadout(mode, patch) + " | " + waveShapeReadout(mode, patch.waveShape) + laneText;

    return label + ": " + juce::String(templ.help) + laneText;
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
    const auto detail = juce::String("bits ") + dutyBits[static_cast<size_t>(resolved)] + ", " + dutyLabels[static_cast<size_t>(resolved)];

    return choice == 0 ? juce::String("Follow -> ") + detail : detail;
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
                return "Follow waveform: resolves to SID CTRL bits";
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
                return "Follow POKEY distortion from template";
        }
    }

    if (mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula)
    {
        switch (std::clamp(choice, 0, 4))
        {
            case 1: return "Triangle sample template";
            case 2: return "Saw sample template";
            case 3: return "Pulse sample template";
            case 4: return "Stepped/noise sample template";
            case 0:
            default:
                return "Follow generated sample template";
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
                return "Follow FM template/operator registers";
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
    return patch.dmgStereoRoute == 0 ? juce::String("Follow -> ") + resolved : resolved;
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
    return patch.ymEnvelopeShape == 0 ? juce::String("Follow -> ") + resolved : resolved;
}

juce::String ChipperAudioProcessorEditor::sampleChipReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    const auto chipLabel = mode == chipper::ChipMode::paula ? juce::String("8-bit hard-pan period sample") : juce::String("lo-fi sample voice");
    const auto decay = static_cast<int>(std::round(std::clamp(patch.envelopeDecay, 0.0f, 1.0f) * 15.0f));
    const auto volume = static_cast<int>(std::round(std::clamp(patch.control4, 0.0f, 1.0f) * 15.0f));
    auto text = chipLabel
        + " | template " + juce::String(static_cast<int>(chipper::sampleTemplateForPatch(mode, patch)))
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

    const auto prefix = patch.ymEnvelopeShape == 0 ? juce::String("Follow -> ") : juce::String();
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

    return patch.snNoiseMode == 0 ? juce::String("Follow -> ") + resolved : resolved;
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

    return patch.snNoiseMode == 0 ? juce::String("Follow -> ") + resolved : resolved;
}

static juce::String paulaHardwarePanLabel(size_t index)
{
    return (index == 0u || index == 3u) ? juce::String("L") : juce::String("R");
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
    const auto templateId = static_cast<int>(chipper::sampleTemplateForPatch(mode, patch));
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
            const auto modeText = info.playbackMode == 0 ? juce::String("Manual") : juce::String("Map");
            const auto slotText = info.bankCount > 1
                ? juce::String("S ") + modeText + " " + juce::String(info.selectedSlot + 1) + "/" + juce::String(info.bankCount)
                : juce::String("Sample");
            return "V" + number + " | " + slotText;
        }

        return "V" + number
            + " | T" + juce::String(templateId)
            + " " + juce::String(sample0) + "/" + juce::String(sample32);
    }

    if (mode == chipper::ChipMode::paula)
        return "Ch " + number
            + " " + paulaHardwarePanLabel(index)
            + " | T" + juce::String(templateId)
            + " " + juce::String(sample0) + "/" + juce::String(sample32);

    return {};
}

juce::String ChipperAudioProcessorEditor::sampleSourceRegisterReadout(chipper::ChipMode mode,
                                                                      const chipper::PatchConfig& patch,
                                                                      size_t index) const
{
    const auto channel = static_cast<int>(index + 1u);
    const auto templateId = static_cast<int>(chipper::sampleTemplateForPatch(mode, patch));
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
            const auto modeText = info.playbackMode == 0 ? juce::String("manual") : juce::String("mapped");
            readout += "sample slot " + juce::String(info.selectedSlot + 1) + "/" + juce::String(info.bankCount)
                + " " + info.sampleName
                + " | " + modeText;
            if (info.playbackMode != 0)
            {
                readout += " "
                    + chipper::parameters::midiNoteChoices()[info.mapRootNote]
                    + "-"
                    + chipper::parameters::midiNoteChoices()[info.mapHighNote];
            }
        }
        else
        {
            readout += "tmpl " + juce::String(templateId)
                + " | sample[0/32] " + juce::String(sample0) + "/" + juce::String(sample32);
        }
        return readout;
    }

    if (mode == chipper::ChipMode::paula)
    {
        const auto volume = chipper::paulaChannelVolumeForPatch(patch, index);
        const auto control = chipper::paulaControlForPatch(patch, index);
        const auto base = static_cast<uint8_t>(std::min(index, size_t { 3u }) * 0x10u);
        return "Paula ch " + juce::String(channel)
            + " " + paulaHardwarePanLabel(index)
            + " | regs $" + byteHex(base) + "-$" + byteHex(static_cast<uint8_t>(base + 4u))
            + " | vol " + juce::String(static_cast<int>(volume)) + "/64"
            + " | ctrl $" + byteHex(control)
            + " | " + ((control & 0x02u) != 0 ? juce::String("loop") : juce::String("one-shot"))
            + " | tmpl " + juce::String(templateId)
            + " | sample[0/32] " + juce::String(sample0) + "/" + juce::String(sample32);
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
    switch (std::clamp(patch.waveShape, 0, 4))
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
        return "HuC6280 Ch " + juce::String(channel)
            + " | $00 select " + juce::String(static_cast<int>(index))
            + " | $04 ctrl $" + byteHex(control)
            + " vol " + juce::String(static_cast<int>(control & 0x1fu)) + "/31"
            + " | RAM[0/31] " + juce::String(static_cast<int>(wave0)) + "/" + juce::String(static_cast<int>(wave31));
    }

    if (mode == chipper::ChipMode::namcoWsg)
    {
        const auto base = static_cast<uint8_t>(0x80u + std::min(index, size_t { 7u }) * 4u);
        const auto volume = chipper::namcoWsgVolumeForPatch(patch, index);
        const auto enabled = chipper::namcoWsgChannelEnabledForPatch(patch, index);
        return "Namco lane " + juce::String(channel)
            + " | regs $" + byteHex(base) + "-$" + byteHex(static_cast<uint8_t>(base + 3u))
            + " | vol " + juce::String(static_cast<int>(volume)) + "/15"
            + " | " + (enabled ? juce::String("enabled") : juce::String("muted"))
            + " | RAM[0/31] $" + byteHex(wave0) + "/$" + byteHex(wave31);
    }

    if (mode == chipper::ChipMode::scc)
    {
        const auto volume = chipper::sccVolumeForPatch(patch, index);
        const auto keyBit = chipper::sccChannelKeyOnForPatch(patch, index);
        return "SCC Ch " + juce::String(channel)
            + " | freq regs $" + byteHex(static_cast<uint8_t>(0xa0u + std::min(index, size_t { 4u }) * 2u))
            + "/$" + byteHex(static_cast<uint8_t>(0xa1u + std::min(index, size_t { 4u }) * 2u))
            + " | $" + byteHex(static_cast<uint8_t>(0xaau + std::min(index, size_t { 4u }))) + " vol " + juce::String(static_cast<int>(volume)) + "/15"
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

juce::String ChipperAudioProcessorEditor::ym2612DacModeReadout(const chipper::PatchConfig& patch) const
{
    const auto mode = chipper::ym2612DacModeForPatch(patch);
    const auto resolved = mode == 2u
                              ? juce::String("DAC Drum: $2B=0x80, stream $2A on Ch 6")
                              : juce::String("FM Ch6: channel 6 remains four-operator FM");
    return patch.snNoiseMode == 0 ? juce::String("Follow -> ") + resolved : resolved;
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

    return patch.dmgWaveLevel == 0 ? juce::String("Follow -> ") + resolvedText : resolvedText;
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
    return patch.dmgStereoRoute == 0 ? juce::String("Follow -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::spc700SamplePlaybackReadout(const chipper::PatchConfig& patch) const
{
    const auto mode = chipper::spc700SamplePlaybackModeForPatch(patch);
    const auto resolvedText = mode == 1u
                                  ? juce::String("Loop, sample RAM wraps while held")
                                  : juce::String("One-shot, voice stops at sample end");
    return patch.dmgStereoRoute == 0 ? juce::String("Follow -> ") + resolvedText : resolvedText;
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

    return patch.dmgStereoRoute == 0 ? juce::String("Follow -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::sidModelReadout(const chipper::PatchConfig& patch) const
{
    const auto model = chipper::sidModelNumberForPatch(patch);
    const auto detail = model == 8580 ? "cleaner/brighter filter profile" : "warmer/rougher filter profile";
    const auto text = juce::String("SID ") + juce::String(model) + ", " + detail;
    return std::clamp(patch.dmgStereoRoute, 0, 2) == 0 ? juce::String("Auto -> ") + text : text;
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
                return "Follow template, Drum/Hit use native OPL2 rhythm";
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
                return "Follow template, Drum/Hit use native OPLL rhythm";
        }
    }

    if (displayedMode == chipper::ChipMode::ym2612)
    {
        const auto clamped = std::clamp(choice, 0, 4);
        switch (clamped)
        {
            case 1: return "OPN2 AR/DR/SR/SL+RR set for plucked keys";
            case 2: return "OPN2 AR/DR/SR/SL+RR set for sustained leads";
            case 3: return "OPN2 AR/DR/SR/SL+RR set for soft pads";
            case 4: return "OPN2 AR/DR/SR/SL+RR set for percussive hits";
            case 0:
            default:
                return "Follow template, writes OPN2 operator envelope registers";
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
                return "Follow template, writes POKEY AUDCTL filter bits";
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
                return "Follow template, writes clean-room SPC700 ADSR/gain contour";
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
    return patch.ymEnvelopeShape == 0 ? juce::String("Follow -> ") + text : text;
}

juce::String ChipperAudioProcessorEditor::sidFilterRoutingReadout(const chipper::PatchConfig& patch) const
{
    const auto bits = chipper::sidFilterRoutingBitsForPatch(patch);
    const auto registerText = juce::String("$D417 low 0x")
        + juce::String::toHexString(static_cast<int>(bits & 0x07u)).toUpperCase();
    const auto text = registerText + ", " + sidFilterRoutingName(bits);
    return patch.sidFilterRouting == 0 ? juce::String("Follow -> ") + text : text;
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
    return patch.snNoiseMode == 0 ? juce::String("Follow -> ") + text : text;
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
    return std::clamp(patch.snNoiseMode, 0, 2) == 0 ? juce::String("Follow -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::dmgNoiseModeReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseRegister = chipper::dmgNoiseRegisterForPatch(patch);
    const auto registerText = juce::String("NR43=0x") + juce::String::toHexString(static_cast<int>(noiseRegister)).paddedLeft('0', 2).toUpperCase();
    const auto widthText = (noiseRegister & 0x08u) != 0 ? "7-bit LFSR" : "15-bit LFSR";
    const auto shiftText = juce::String("shift ") + juce::String(static_cast<int>((noiseRegister >> 4u) & 0x0fu));
    const auto divisorText = juce::String("divisor code ") + juce::String(static_cast<int>(noiseRegister & 0x07u));
    const auto resolvedText = registerText + ", " + widthText + ", " + shiftText + ", " + divisorText;
    return std::clamp(patch.snNoiseMode, 0, 2) == 0 ? juce::String("Follow -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::snNoiseModeReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseControl = chipper::sn76489NoiseControlForPatch(patch);
    const auto registerText = juce::String("Reg E=0x") + juce::String::toHexString(static_cast<int>(noiseControl)).toUpperCase();
    const auto resolvedText = registerText + ", " + snNoiseRegisterLabel(noiseControl);
    return patch.snNoiseMode == 0 ? juce::String("Follow -> ") + resolvedText : resolvedText;
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
    static constexpr std::array<const char*, 5> choiceLabels { "Follow", "Tone", "Noise", "Both", "Off" };
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
    setSidVoiceWaveControlsVisible(shouldBeVisible && mode == chipper::ChipMode::sid);
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

    const auto active = shouldBeVisible && usesWaveShapeSegment(mode);
    const auto fmAlgorithmActive = active && (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151);
    const auto oplWaveformActive = active && mode == chipper::ChipMode::opl3;
    const auto opllInstrumentActive = active && mode == chipper::ChipMode::ym2413;
    waveShapeLabel.setVisible(active);
    waveShapeValueLabel.setVisible(active);
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
    dmgWaveLevelLabel.setVisible(active);
    dmgWaveLevelValueLabel.setVisible(active);
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
    const auto usesMenu = active && spec != nullptr && spec->surface == chipper::ControlSurface::menu;
    dmgStereoRouteLabel.setVisible(active);
    dmgStereoRouteValueLabel.setVisible(active);
    dmgStereoRouteBox.setVisible(usesMenu);
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
    const auto menuActive = active && chipper::chipHasParameterSurface(mode,
                                                                       chipper::ChipParameterRole::snNoiseMode,
                                                                       chipper::ControlSurface::menu);
    snNoiseModeLabel.setVisible(active);
    snNoiseModeValueLabel.setVisible(active);
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
        "Ext In  | planned",
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
        "Ext In  | planned",
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
            sourceLevelLabels[i].setTooltip(withMidiCcForRole(juce::String(levelSpec->label) + ": " + juce::String(levelSpec->help), sourceLevelRole(i)));
            sourceLevelSliders[i].setTooltip(withMidiCcForRole(juce::String(levelSpec->label) + ": " + juce::String(levelSpec->help), sourceLevelRole(i)));
        }
        else
        {
            sourceLevelLabels[i].setTooltip(withMidiCcForRole(juce::String("Trim level for ") + (*labels)[i], sourceLevelRole(i)));
            sourceLevelSliders[i].setTooltip(withMidiCcForRole(juce::String("Trim level for ") + (*labels)[i], sourceLevelRole(i)));
        }

        sourceLevelValueLabels[i].setTooltip(sourceLevelSliders[i].getTooltip());
        sourceLevelValueLabels[i].setText(sourceLevelReadout(i), juce::dontSendNotification);
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
            tooltip = juce::String(index == 0 ? "DMG pulse 1 duty: " : "DMG pulse 2 duty: ") + pulseDutyReadout(mode, patch.control1);
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
            + ": generated lo-fi sample template preview."
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
        shape = wavetablePreviewShape(patch);
        tooltip = juce::String("Paula sample channel ") + juce::String(static_cast<int>(index + 1u))
            + ": 8-bit tracker sample template with period playback."
            + "\nSample Shape: " + waveShapeReadout(mode, patch.waveShape)
            + "\n" + sampleSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::huc6280)
    {
        shape = wavetablePreviewShape(patch);
        tooltip = juce::String("HuC6280 wavetable channel ") + juce::String(static_cast<int>(index + 1u))
            + ": 32-sample wave RAM preview."
            + "\nWave Shape: " + waveShapeReadout(mode, patch.waveShape)
            + "\n" + wavetableSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::namcoWsg)
    {
        shape = wavetablePreviewShape(patch);
        tooltip = juce::String("Namco WSG wavetable lane ") + juce::String(static_cast<int>(index + 1u))
            + ": 4-bit wave RAM preview."
            + "\nWave Shape: " + waveShapeReadout(mode, patch.waveShape)
            + "\n" + wavetableSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::scc)
    {
        shape = wavetablePreviewShape(patch);
        tooltip = juce::String("Konami SCC wavetable channel ") + juce::String(static_cast<int>(index + 1u))
            + ": 32-byte wave RAM preview."
            + "\nWave Shape: " + waveShapeReadout(mode, patch.waveShape)
            + "\n" + wavetableSourceRegisterReadout(mode, patch, index);
    }
    else if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2413)
    {
        switch (std::clamp(patch.waveShape, 0, 4))
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
        const auto nibbleText = clamped == 0 ? juce::String("Follow -> ") + juce::String(resolvedNibble)
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
                                                           + ". Follow uses the macro/control-resolved register nibble.",
                                                       sidAdsrRole(i)));
        sidAdsrSliders[i].setValue(clamped, juce::dontSendNotification);
    }

    if (shouldBeVisible)
        envelopeDecayValueLabel.setTooltip(withMidiCcForRole(combinedTooltip, chipper::ChipParameterRole::envelopeDecay));
}

void ChipperAudioProcessorEditor::updateStereoSpreadReadout(chipper::ChipMode mode)
{
    stereoSpreadValueLabel.setText(stereoSpreadReadout(mode, parameterValue(chipper::parameters::id::stereoSpread)), juce::dontSendNotification);
}

juce::String ChipperAudioProcessorEditor::envelopeDecayReadout(chipper::ChipMode mode, float value) const
{
    if (value <= 0.01f)
        return "Off: macro envelope/level unchanged";

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
    return juce::String("NES envelope decay, period ") + juce::String(period);
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
    const auto* spec = chipper::parameterSpecFor(chipper::ChipMode::nes, chipper::ChipParameterRole::pulse2Duty);
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

    pulse2DutyLabel.setVisible(shouldBeVisible);
    pulse2DutyValueLabel.setVisible(shouldBeVisible);
    pulse2DutyValueLabel.setText(pulse2DutyReadout(patch), juce::dontSendNotification);
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

    waveShapeValueLabel.setVisible(shouldBeVisible);
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
        ? juce::String("Follow -> Alg ") + juce::String(resolvedAlgorithm)
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
        ? juce::String("Follow -> W") + juce::String(resolvedWaveform)
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
        ? juce::String("Follow -> I") + juce::String(resolvedInstrument)
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

    dmgWaveLevelValueLabel.setVisible(shouldBeVisible);
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

    dmgStereoRouteBox.setVisible(shouldBeVisible && menuActive);
    if (menuActive)
    {
        dmgStereoRouteBox.setSelectedId(static_cast<int>(selected) + 1, juce::dontSendNotification);
        if (spec != nullptr)
            dmgStereoRouteBox.setTooltip(withMidiCcForRole(juce::String(spec->help) + "\n" + spc700SamplePlaybackReadout(patch), spec->role));
    }

    dmgStereoRouteValueLabel.setVisible(shouldBeVisible);
    dmgStereoRouteValueLabel.setText(mode == chipper::ChipMode::sid
                                         ? sidModelReadout(patch)
                                         : (mode == chipper::ChipMode::spc700
                                               ? spc700SamplePlaybackReadout(patch)
                                               : (mode == chipper::ChipMode::ym2612
                                                      ? ym2612PanReadout(patch)
                                                      : dmgStereoRouteReadout(patch))),
                                     juce::dontSendNotification);
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

    snNoiseModeValueLabel.setVisible(shouldBeVisible);
    snNoiseModeValueLabel.setText(noiseModeReadout(mode, patch), juce::dontSendNotification);
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

    const juce::ScopedValueSetter<bool> suppress(suppressManualChoiceCallbacks, true);
    if (sampleCount != displayedDmcSampleCount || revision != displayedDmcSampleRevision)
    {
        dmcSampleSlotBox.clear(juce::dontSendNotification);
        for (int i = 0; i < sampleCount; ++i)
            dmcSampleSlotBox.addItem(juce::String(i + 1).paddedLeft('0', 2) + "  " + names[i], i + 1);

        displayedDmcSampleCount = sampleCount;
        displayedDmcSampleRevision = revision;
    }

    if (sampleCount > 0)
        dmcSampleSlotBox.setSelectedId(std::clamp(selectedSlot, 0, sampleCount - 1) + 1, juce::dontSendNotification);
    else
        dmcSampleSlotBox.setSelectedId(0, juce::dontSendNotification);

    dmcSampleSlotBox.setEnabled(sampleCount > 0);
    const auto playbackInfo = audioProcessor.nesDmcSamplePlaybackInfo();
    dmcSampleStatusLabel.setText(playbackInfo.statusLine, juce::dontSendNotification);
    auto sampleTooltip = playbackInfo.statusLine
        + "\nDMC bit clock: " + juce::String(playbackInfo.bitRateHz / 1000.0, 2) + " kHz from $4010 rate index "
        + juce::String(playbackInfo.rateIndex) + ".";
    if (playbackInfo.byteCount > 0)
    {
        sampleTooltip += "\nSample payload: " + juce::String(playbackInfo.byteCount) + " bytes / "
            + juce::String(playbackInfo.bitCount) + " DPCM bits, approx "
            + juce::String(playbackInfo.durationMs, playbackInfo.durationMs < 10.0 ? 1 : 0) + " ms at this rate.";
    }
    sampleTooltip += playbackInfo.loopEnabled
        ? "\nLoop is on: $4010 bit 6 restarts playback from byte 0 when the sample ends."
        : "\nLoop is off: playback stops when the selected DMC sample ends.";
    sampleTooltip += "\nManual Slot plays the selected dropdown slot. Note Map maps checked slots upward from the DMC Map Root; Sample Map Only maps notes and suppresses pulse, triangle, and noise.";
    if (playbackInfo.playbackMode != 0 && playbackInfo.activeSlotCount > 0)
        sampleTooltip += "\nCurrent map span: "
            + chipper::parameters::midiNoteChoices()[playbackInfo.mapRootNote]
            + " to "
            + chipper::parameters::midiNoteChoices()[playbackInfo.mapHighNote]
            + ".";
    sampleTooltip += "\nFolder loads create a local checklist; checked entries become up to 32 CC117-addressable slots. WAV-to-DMC conversion is planned.";
    dmcSampleStatusLabel.setTooltip(withMidiCcForRole(sampleTooltip, chipper::ChipParameterRole::nesDmcSampleSlot));
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
    const auto bankModeLabel = playbackMode == 2 ? juce::String("Drum Map") : (playbackMode == 1 ? juce::String("Key Map") : juce::String("Manual Slot"));
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
    if (sampleCount != displayedDmcSampleCount || revision != displayedDmcSampleRevision)
    {
        dmcSampleSlotBox.clear(juce::dontSendNotification);
        for (int i = 0; i < sampleCount; ++i)
            dmcSampleSlotBox.addItem(juce::String(i + 1).paddedLeft('0', 2) + "  " + names[i], i + 1);

        displayedDmcSampleCount = sampleCount;
        displayedDmcSampleRevision = revision;
    }

    if (sampleCount > 0)
        dmcSampleSlotBox.setSelectedId(std::clamp(selectedSlot, 0, sampleCount - 1) + 1, juce::dontSendNotification);
    else
        dmcSampleSlotBox.setSelectedId(0, juce::dontSendNotification);

    dmcSampleSlotBox.setEnabled(sampleCount > 0);
    dmcSampleSlotBox.setTextWhenNothingSelected("No SPC700 samples");
    dmcSampleSlotBox.setTooltip(withMidiCcForRole("Selects the manual SPC700 sample from the loaded bank. MIDI CC117 selects the same slot; Manual playback uses it for every note.", chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcPlaybackModeBox.setTooltip(withMidiCcForRole("SPC700 Sample Playback. Manual Slot plays the selected dropdown slot; Key Map maps loaded folder slots upward from the Sample Map Root. Drum Map uses the same bank mapping path for one-shot/percussion templates.", chipper::ChipParameterRole::nesDmcPlaybackMode));
    dmcMapRootBox.setTooltip(withMidiCcForRole("SPC700 Sample Map Root. Loaded folder slots map upward from this MIDI note when playback is a map mode.", chipper::ChipParameterRole::nesDmcMapRoot));

    const auto info = audioProcessor.spc700BrrSampleInfo();
    auto visibleStatus = info.statusLine;
    if (info.loaded && info.bankCount > 1 && playbackMode != 0)
        visibleStatus += " | Map " + chipper::parameters::midiNoteChoices()[info.mapRootNote]
            + "-" + chipper::parameters::midiNoteChoices()[info.mapHighNote];
    else if (info.loaded && info.bankCount > 1)
        visibleStatus += " | Manual";
    if (! info.loaded)
        visibleStatus += " | Generated " + waveShapeReadout(chipper::ChipMode::spc700, playbackPatch.waveShape);
    visibleStatus += " | Bank " + bankModeLabel + " | Voice " + lifetimeLabel;
    if (info.loaded)
    {
        visibleStatus += " | ARAM "
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
        tooltip += juce::String("\nNo user sample is loaded, so the internal generated sample template remains playable.")
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
            + " upward and Follow Template resolves to one-shot sample playback.";
    else
        tooltip += "\nSample Playback is a map mode: notes browse the loaded bank from "
            + chipper::parameters::midiNoteChoices()[mapRoot] + " upward.";
    tooltip += "\nBank Mode: " + bankModeLabel + ".";
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
}

void ChipperAudioProcessorEditor::updatePaulaSampleControls()
{
    updateSamplePlaybackModeChoices(chipper::ChipMode::paula);
    dmcSampleLabel.setText("Paula Sample", juce::dontSendNotification);
    const auto mapRoot = std::clamp(static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcMapRoot))), 0, 127);
    const auto playbackMode = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcPlaybackMode)));
    dmcSampleLabel.setTooltip(withMidiCcForRole("Load one user-provided WAV/AIFF sample or a folder. CC117 selects the manual Paula sample slot; Sample Playback chooses whether MIDI notes browse the loaded bank.", chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcSampleFileButton.setButtonText("File");
    dmcSampleFileButton.setTooltip("Load one user-provided WAV/AIFF into the Paula-style 8-bit sample voice model.");
    dmcSampleFolderButton.setButtonText("Folder");
    dmcSampleFolderButton.setTooltip("Load a folder of user-provided WAV/AIFF files. Checked entries become up to 32 playable slots and note-mapped sample choices.");
    dmcSampleBankButton.setButtonText("Bank");
    dmcSampleBankButton.setTooltip("Open the Paula sample checklist. Checked files become the playable 32-slot dropdown and note map.");

    const auto names = audioProcessor.paulaSampleNames();
    const auto sampleCount = names.size();
    const auto revision = audioProcessor.paulaSampleRevision();
    const auto selectedSlot = static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcSampleSlot)));

    const juce::ScopedValueSetter<bool> suppress(suppressManualChoiceCallbacks, true);
    if (sampleCount != displayedDmcSampleCount || revision != displayedDmcSampleRevision)
    {
        dmcSampleSlotBox.clear(juce::dontSendNotification);
        for (int i = 0; i < sampleCount; ++i)
            dmcSampleSlotBox.addItem(juce::String(i + 1).paddedLeft('0', 2) + "  " + names[i], i + 1);

        displayedDmcSampleCount = sampleCount;
        displayedDmcSampleRevision = revision;
    }

    if (sampleCount > 0)
        dmcSampleSlotBox.setSelectedId(std::clamp(selectedSlot, 0, sampleCount - 1) + 1, juce::dontSendNotification);
    else
        dmcSampleSlotBox.setSelectedId(0, juce::dontSendNotification);

    dmcSampleSlotBox.setEnabled(sampleCount > 0);
    dmcSampleSlotBox.setTextWhenNothingSelected("No Paula samples");
    dmcSampleSlotBox.setTooltip(withMidiCcForRole("Selects the manual Paula sample from the loaded bank. MIDI CC117 selects the same slot; Manual Slot uses it for every note.", chipper::ChipParameterRole::nesDmcSampleSlot));
    dmcPlaybackModeBox.setTooltip(withMidiCcForRole("Paula Sample Playback. Manual Slot plays the selected dropdown slot; Key Map maps loaded folder slots upward from the Sample Map Root for held/melodic samples. Tracker Map uses the same bank mapping path and resolves Follow Template playback to one-shot tracker kits.", chipper::ChipParameterRole::nesDmcPlaybackMode));
    dmcMapRootBox.setTooltip(withMidiCcForRole("Paula Sample Map Root. Loaded folder slots map upward from this MIDI note when Sample Playback is a map mode.", chipper::ChipParameterRole::nesDmcMapRoot));

    const auto info = audioProcessor.paulaSampleInfo();
    auto visibleStatus = info.statusLine;
    if (info.loaded && info.bankCount > 1 && playbackMode != 0)
        visibleStatus += " | Map " + chipper::parameters::midiNoteChoices()[info.mapRootNote]
            + "-" + chipper::parameters::midiNoteChoices()[info.mapHighNote];
    else if (info.loaded && info.bankCount > 1)
        visibleStatus += " | Manual";
    dmcSampleStatusLabel.setText(visibleStatus, juce::dontSendNotification);

    auto tooltip = info.statusLine
        + "\nWAV/AIFF files are imported into Paula-style 8-bit sample memory. Folder loads keep up to 32 user-provided files addressable by the dropdown and CC117.";
    if (playbackMode == 0)
        tooltip += "\nSample Playback is Manual Slot: each MIDI note uses the selected dropdown sample.";
    else if (playbackMode == 2)
        tooltip += "\nSample Playback is Tracker Map: notes browse the loaded bank from "
            + chipper::parameters::midiNoteChoices()[mapRoot]
            + " upward and Follow Template resolves to one-shot tracker playback.";
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
    tooltip += "\nIFF/8SVX/MOD import and exact Paula DMA timing remain planned.";
    dmcSampleStatusLabel.setTooltip(withMidiCcForRole(tooltip, chipper::ChipParameterRole::nesDmcSampleSlot));
}

void ChipperAudioProcessorEditor::updateSamplePlaybackModeChoices(chipper::ChipMode mode)
{
    const auto choices = mode == chipper::ChipMode::spc700
        ? juce::StringArray { "Manual Slot", "Key Map", "Drum Map" }
        : mode == chipper::ChipMode::paula
        ? juce::StringArray { "Manual Slot", "Key Map", "Tracker Map" }
        : chipper::parameters::nesDmcPlaybackModeChoices();

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

    if (choicesAlreadyMatch)
        return;

    const auto selectedChoice = std::clamp(static_cast<int>(std::round(parameterValue(chipper::parameters::id::nesDmcPlaybackMode))), 0, choices.size() - 1);
    const juce::ScopedValueSetter<bool> suppress(suppressManualChoiceCallbacks, true);
    dmcPlaybackModeBox.clear(juce::dontSendNotification);
    for (int i = 0; i < choices.size(); ++i)
        dmcPlaybackModeBox.addItem(choices[i], i + 1);
    dmcPlaybackModeBox.setTextWhenNothingSelected(mode == chipper::ChipMode::spc700 ? "Sample Playback" : (mode == chipper::ChipMode::paula ? "Sample Playback" : "DMC Playback"));
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
                                                           "*.wav;*.aif;*.aiff");
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
                                                           "*.wav;*.aif;*.aiff");
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

    const auto chipChangedAfterInitialLoad = descriptorTextInitialized && mode != displayedMode;
    descriptorTextInitialized = true;
    displayedMode = mode;
    if (chipChangedAfterInitialLoad)
    {
        displayedDmcSampleCount = -1;
        displayedDmcSampleRevision = std::numeric_limits<uint64_t>::max();
    }
    const auto& descriptor = chipper::descriptorFor(mode);
    const auto hasLiveCore = descriptor.implemented;
    const auto supportsChipPoly = chipper::supportsPlayMode(mode, chipper::PlayMode::chipPoly);

    updateMacroChoices(mode);
    updatePresetChoices(mode);
    chipSummaryLabel.setText(descriptor.summary, juce::dontSendNotification);
    auto verificationBadge = juce::String(descriptor.verification.badge);
    coreReadinessLabel.setText(verificationBadge.toUpperCase(), juce::dontSendNotification);

    auto verificationTooltip = juce::String(descriptor.verification.summary)
        + "\nEvidence: " + juce::String(descriptor.verification.evidence);
    if (! descriptor.verification.verifiedBehaviors.empty())
    {
        verificationTooltip += "\nVerified:";
        for (const auto& behavior : descriptor.verification.verifiedBehaviors)
            verificationTooltip += "\n- " + juce::String(behavior);
    }
    if (! descriptor.verification.knownGaps.empty())
    {
        verificationTooltip += "\nKnown gaps:";
        for (const auto& gap : descriptor.verification.knownGaps)
            verificationTooltip += "\n- " + juce::String(gap);
    }
    verificationTooltip += descriptor.verification.cycleAccurate
        ? "\nCycle accuracy: claimed."
        : "\nCycle accuracy: not claimed.";
    verificationTooltip += descriptor.verification.hardwareValidated
        ? "\nHardware validation: complete for the documented scope."
        : "\nHardware validation: not complete.";
    coreReadinessLabel.setTooltip(verificationTooltip);
    coreReadinessLabel.setColour(juce::Label::textColourId, hasLiveCore ? juce::Colour(0xff101414) : juce::Colour(0xffd9e1e8));
    coreReadinessLabel.setColour(juce::Label::backgroundColourId, hasLiveCore ? juce::Colour(0xff56c7d8) : juce::Colour(0xff344047));
    globalStripLabel.setText(hasLiveCore ? "Performance" : "Roadmap", juce::dontSendNotification);
    macroSummaryLabel.setVisible(true);
    macroSummaryLabel.setEnabled(true);
    macroSummaryLabel.setAlpha(hasLiveCore ? 1.0f : 0.85f);
    accuracyBox.setEnabled(hasLiveCore);
    presetBox.setEnabled(hasLiveCore && ! displayedPresets.empty());
    macroBox.setEnabled(true);
    macroBox.setTooltip(hasLiveCore
                            ? "Applies a chip-specific musical template to native controls."
                            : "Browses chip-specific roadmap templates. Audio remains disabled until this chip has an audited or clean-room core.");
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
    dmcSampleLabel.setVisible(showSampleFileControls);
    dmcSampleStatusLabel.setVisible(showSampleFileControls);
    dmcSampleFileButton.setVisible(showSampleFileControls);
    dmcSampleFolderButton.setVisible(showSampleFileControls);
    dmcSampleBankButton.setVisible(showSampleBankControls);
    dmcSampleSlotBox.setVisible(showSampleBankControls);
    dmcPlaybackModeBox.setVisible(showSampleBankControls);
    dmcMapRootBox.setVisible(showSampleBankControls);
    dmcLoopButton.setVisible(showNesDmcSampleControls);
    dmcSampleLabel.setEnabled(showSampleFileControls);
    dmcSampleStatusLabel.setEnabled(showSampleFileControls);
    dmcSampleFileButton.setEnabled(showSampleFileControls);
    dmcSampleFolderButton.setEnabled(showSampleFileControls);
    dmcSampleBankButton.setEnabled(showSampleBankControls);
    dmcSampleSlotBox.setEnabled(showSampleBankControls);
    dmcPlaybackModeBox.setEnabled(showSampleBankControls);
    dmcMapRootBox.setEnabled(showSampleBankControls);
    dmcLoopButton.setEnabled(showNesDmcSampleControls);
    dmcSampleLabel.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    dmcSampleStatusLabel.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    dmcSampleFileButton.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    dmcSampleFolderButton.setAlpha(showSampleFileControls ? 1.0f : 0.55f);
    dmcSampleBankButton.setAlpha(showSampleBankControls ? 1.0f : 0.55f);
    dmcPlaybackModeBox.setAlpha(showSampleBankControls ? 1.0f : 0.55f);
    dmcMapRootBox.setAlpha(showSampleBankControls ? 1.0f : 0.55f);
    dmcLoopButton.setAlpha(showNesDmcSampleControls ? 1.0f : 0.55f);
    clockLabel.setVisible(mode != chipper::ChipMode::nes && mode != chipper::ChipMode::spc700 && mode != chipper::ChipMode::paula);
    clockSlider.setVisible(mode != chipper::ChipMode::nes && mode != chipper::ChipMode::spc700 && mode != chipper::ChipMode::paula);

    for (size_t i = 0; i < moduleTitleLabels.size(); ++i)
    {
        if (i >= descriptor.modules.size())
            continue;

        const auto& module = descriptor.modules[i];
        moduleTitleLabels[i].setText(module.title, juce::dontSendNotification);
        auto summary = juce::String(module.summary);
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
    setPulse2DutySegmentVisible(mode, usesPulse2DutySegment(mode) && hasLiveCore);
    setWaveShapeSegmentVisible(mode, usesWaveShapeSegment(mode) && hasLiveCore);
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
    moduleSummaryLabels[1].setVisible(!(hasLiveCore && mode == chipper::ChipMode::sid && usesSourceChannelSurface(mode)));
    moduleSummaryLabels[3].setVisible(!(hasLiveCore && mode == chipper::ChipMode::sid && usesEnvelopeDecayControl(mode)));
    const auto hasCustomProfileSurface = hasLiveCore && mode == chipper::ChipMode::sid && usesDmgStereoRouteSegment(mode);
    for (auto& itemLabel : moduleItemLabels[0])
        itemLabel.setVisible(! hasCustomProfileSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomToneSurface = hasLiveCore && (usesWaveShapeSegment(mode)
        || usesPulse2DutySegment(mode)
        || usesDmgWaveLevelSegment(mode)
        || usesYmChannelMixControls(mode)
        || (mode != chipper::ChipMode::sid && usesSnNoiseModeSegment(mode))
        || (mode != chipper::ChipMode::spc700 && usesYmEnvelopeShapeSegment(mode))
        || hasSidFilterRoutingControl);
    moduleSummaryLabels[2].setVisible(! hasCustomToneSurface);
    for (auto& itemLabel : moduleItemLabels[2])
        itemLabel.setVisible(! hasCustomToneSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomEnvelopeSurface = hasLiveCore
        && (usesEnvelopeDecayControl(mode) || (mode == chipper::ChipMode::spc700 && usesYmEnvelopeShapeSegment(mode)));
    for (auto& itemLabel : moduleItemLabels[3])
        itemLabel.setVisible(! hasCustomEnvelopeSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomMotionSurface = hasLiveCore && mode == chipper::ChipMode::sid && usesSnNoiseModeSegment(mode);
    for (auto& itemLabel : moduleItemLabels[4])
        itemLabel.setVisible(! hasCustomMotionSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomOutputSurface = hasLiveCore
        && mode != chipper::ChipMode::sid
        && (usesStereoSpreadControl(mode) || usesDmgStereoRouteSegment(mode));
    for (auto& itemLabel : moduleItemLabels[5])
        itemLabel.setVisible(! hasCustomOutputSurface && ! itemLabel.getText().isEmpty());
    resized();
    updatePulseDutyButtons(parameterValue(chipper::parameters::id::macroControl1), usesPulseDutySegment(mode) && hasLiveCore);
    updateLiveControlReadouts();

    if (chipChangedAfterInitialLoad && ! applyingFactoryPreset && hasLiveCore && ! displayedPresets.empty())
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
    auto performanceText = macroText;
    auto performanceTooltip = macroText;
    if (! displayedPresets.empty())
    {
        const auto presetCount = static_cast<int>(displayedPresets.size());
        performanceText = juce::String(presetCount) + " "
            + juce::String(chipper::toString(mode))
            + " presets | " + macroText;
        performanceTooltip = "Use the Preset menu to audition "
            + juce::String(presetCount)
            + " factory presets for "
            + juce::String(chipper::toString(mode))
            + ".\n"
            + macroText;
    }
    const auto selectedPreset = presetBox.getSelectedId() - 1;
    if (selectedPreset >= 0 && static_cast<size_t>(selectedPreset) < displayedPresets.size())
    {
        const auto& preset = *displayedPresets[static_cast<size_t>(selectedPreset)];
        performanceText = juce::String("Preset: ") + juce::String(preset.name) + " | " + macroText;
        performanceTooltip = juce::String(preset.name) + ": " + juce::String(preset.note) + "\n" + macroText;
    }

    macroSummaryLabel.setText(performanceText, juce::dontSendNotification);
    macroSummaryLabel.setTooltip(performanceTooltip);
    macroBox.setTooltip(withMidiCc(macroText, chipper::parameters::id::macro));

    const auto hasPulseDutySegment = usesPulseDutySegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasPulse2DutySegment = usesPulse2DutySegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasWaveShapeSegment = usesWaveShapeSegment(mode) && chipper::descriptorFor(mode).implemented;
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
    updateStereoSpreadReadout(mode);

    if (mode == chipper::ChipMode::nes)
    {
        controlValueLabels[0].setText(pulseDutyReadout(mode, patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(nesSweepReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(nesNoiseReadout(patch.control3), juce::dontSendNotification);
        controlValueLabels[3].setText(nesFocusReadout(patch.control4), juce::dontSendNotification);

        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::dmg)
    {
        controlValueLabels[0].setText(pulseDutyReadout(mode, patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(dmgSweepReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(dmgNoiseReadout(patch.control3), juce::dontSendNotification);
        controlValueLabels[3].setText(dmgEnvelopeReadout(patch.control4), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::ym2149)
    {
        controlValueLabels[0].setText(ymSpreadReadout(patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(ymMotionReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(ymNoiseReadout(patch.control3), juce::dontSendNotification);
        controlValueLabels[3].setText(ymToneNoiseReadout(patch.control4) + " | " + ymChannelMixReadout(patch), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::sn76489)
    {
        controlValueLabels[0].setText(snStackReadout(patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(snMotionReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(snNoiseModeReadout(patch), juce::dontSendNotification);
        controlValueLabels[3].setText(snLevelReadout(patch.control4), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::sid)
    {
        controlValueLabels[0].setText(sidPulseWidthReadout(patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(sidDetuneReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(sidCutoffReadout(patch.control3), juce::dontSendNotification);
        controlValueLabels[3].setText(sidSustainReadout(patch), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::pokey)
    {
        controlValueLabels[0].setText(waveShapeReadout(mode, patch.waveShape), juce::dontSendNotification);
        controlValueLabels[1].setText(pokeyRegisterReadout(patch), juce::dontSendNotification);
        controlValueLabels[2].setText("Poly/timer bias " + juce::String(patch.control3, 2), juce::dontSendNotification);
        controlValueLabels[3].setText("AUDV volume " + juce::String(static_cast<int>(std::round(patch.control4 * 15.0f))) + "/15", juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::spc700)
    {
        controlValueLabels[0].setText(waveShapeReadout(mode, patch.waveShape), juce::dontSendNotification);
        controlValueLabels[1].setText(spc700PitchMotionReadout(patch), juce::dontSendNotification);
        controlValueLabels[2].setText(spc700EchoReadout(patch), juce::dontSendNotification);
        controlValueLabels[3].setText(sampleChipReadout(mode, patch), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::paula)
    {
        controlValueLabels[0].setText(waveShapeReadout(mode, patch.waveShape), juce::dontSendNotification);
        controlValueLabels[1].setText("Pitch/rate motion " + juce::String(patch.control2, 2), juce::dontSendNotification);
        controlValueLabels[2].setText("Sample color " + juce::String(patch.control3, 2), juce::dontSendNotification);
        controlValueLabels[3].setText(sampleChipReadout(mode, patch), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
    {
        controlValueLabels[0].setText("Channel spread " + juce::String(static_cast<int>(std::round(patch.control1 * 12.0f))) + " st", juce::dontSendNotification);
        controlValueLabels[1].setText("Pitch motion " + juce::String(patch.control2, 2), juce::dontSendNotification);
        controlValueLabels[2].setText(waveShapeReadout(mode, patch.waveShape), juce::dontSendNotification);
        controlValueLabels[3].setText(wavetableChipReadout(mode, patch), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2413)
    {
        controlValueLabels[0].setText(fmChipReadout(mode, patch), juce::dontSendNotification);
        controlValueLabels[1].setText("Feedback/motion " + juce::String(patch.control2, 2), juce::dontSendNotification);
        controlValueLabels[2].setText(mode == chipper::ChipMode::ym2612 ? ym2612DacModeReadout(patch) : waveShapeReadout(mode, patch.waveShape), juce::dontSendNotification);
        controlValueLabels[3].setText("FM output level " + juce::String(static_cast<int>(std::round(patch.control4 * 15.0f))) + "/15", juce::dontSendNotification);
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
        controlValueLabels[4].setTooltip("Loads WAV/AIFF into the Paula-style 8-bit sample voice path.");
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
