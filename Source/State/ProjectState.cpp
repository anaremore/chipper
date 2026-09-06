#include "ProjectState.h"
#include "SampleStateMetadata.h"
#include <algorithm>
namespace chipper::state
{
using namespace assets;
std::unique_ptr<juce::XmlElement> serializeProjectState(const ProjectStateSnapshot& snapshot, bool embedProjectAssets)
{
    auto xml = snapshot.parameters.createXml();
    if (! xml) return {};
    const auto& dmcSampleBank = snapshot.dmcSampleBank;
    const auto& spc700BrrSampleBank = snapshot.spc700BrrSampleBank;
    const auto& paulaSampleBank = snapshot.paulaSampleBank;
    const auto& opnaAdpcmARegions = snapshot.opnaAdpcmARegions;
    const auto& opnbAdpcmARegions = snapshot.opnbAdpcmARegions;
    const auto& spc700BrrSample = snapshot.spc700BrrSample;
    const auto& opn2DacSample = snapshot.opn2DacSample;
    const auto& opnaRhythmRom = snapshot.opnaRhythmRom;
    const auto& opnaAdpcmBSample = snapshot.opnaAdpcmBSample;
    const auto& opnbAdpcmASample = snapshot.opnbAdpcmASample;
    const auto& opnbAdpcmBSample = snapshot.opnbAdpcmBSample;
    xml->setAttribute(chipper::state::schemaVersionAttribute, chipper::state::currentSchemaVersion);
    EmbeddedSampleBudget embeddedBudget;

    while (auto* existingCoreState = xml->getChildByName(coreStateTag))
        xml->removeChildElement(existingCoreState, true);
    while (auto* existingDmcBankState = xml->getChildByName(dmcBankStateTag))
        xml->removeChildElement(existingDmcBankState, true);
    while (auto* existingSpcBrrState = xml->getChildByName(spc700BrrStateTag))
        xml->removeChildElement(existingSpcBrrState, true);
    while (auto* existingSpcBrrBankState = xml->getChildByName(spc700BrrBankStateTag))
        xml->removeChildElement(existingSpcBrrBankState, true);
    while (auto* existingPaulaSampleBankState = xml->getChildByName(paulaSampleBankStateTag))
        xml->removeChildElement(existingPaulaSampleBankState, true);
    while (auto* existingOpn2DacSampleState = xml->getChildByName(opn2DacSampleStateTag))
        xml->removeChildElement(existingOpn2DacSampleState, true);
    while (auto* existingOpnaRhythmRomState = xml->getChildByName(opnaRhythmRomStateTag))
        xml->removeChildElement(existingOpnaRhythmRomState, true);
    while (auto* existingOpnaAdpcmBState = xml->getChildByName(opnaAdpcmBSampleStateTag))
        xml->removeChildElement(existingOpnaAdpcmBState, true);
    while (auto* existingOpnbAdpcmAState = xml->getChildByName(opnbAdpcmASampleStateTag))
        xml->removeChildElement(existingOpnbAdpcmAState, true);
    while (auto* existingOpnbAdpcmBState = xml->getChildByName(opnbAdpcmBSampleStateTag))
        xml->removeChildElement(existingOpnbAdpcmBState, true);
    while (auto* existingWavetableState = xml->getChildByName(chipper::state::wavetableStateTag))
        xml->removeChildElement(existingWavetableState, true);
    while (auto* existingMotionState = xml->getChildByName(chipper::state::motionStateTag))
        xml->removeChildElement(existingMotionState, true);

    if (! snapshot.coreMode.empty())
    {
        auto* coreState = new juce::XmlElement(coreStateTag);
        coreState->setAttribute("mode", snapshot.coreMode);
        coreState->setAttribute("implementedAccuracy", snapshot.accuracy);

        const auto& writes = snapshot.registers;
        coreState->setAttribute("count", static_cast<int>(writes.size()));
        for (const auto& write : writes)
        {
            auto* reg = new juce::XmlElement(registerTag);
            reg->setAttribute("address", static_cast<int>(write.address));
            reg->setAttribute("value", static_cast<int>(write.value));
            coreState->addChildElement(reg);
        }

        xml->addChildElement(coreState);
    }

    if (auto customWaveState = chipper::state::createWavetableStateXml(snapshot.wavetables))
        xml->addChildElement(customWaveState.release());

    if (auto customMotionState = chipper::state::createMotionStateXml(snapshot.motion))
        xml->addChildElement(customMotionState.release());

    {
        if (! dmcSampleBank.empty())
        {
            auto* dmcBankState = new juce::XmlElement(dmcBankStateTag);
            dmcBankState->setAttribute("count", static_cast<int>(dmcSampleBank.size()));
            auto playableSlotCount = size_t { 0u };
            for (const auto& slot : dmcSampleBank)
            {
                auto* sample = new juce::XmlElement(dmcSampleStateTag);
                sample->setAttribute("path", slot.path);
                sample->setAttribute("included", slot.included ? 1 : 0);
                addSampleReferenceMetadata(*sample, slot);
                if (embedProjectAssets
                    && slot.included
                    && playableSlotCount < chipper::state::maxEmbeddedSampleSlotsPerBank)
                    addEmbeddedSamplePayload(*sample, slot, chipper::state::maxEmbeddedBankSlotBytes, embeddedBudget);
                if (slot.included)
                    ++playableSlotCount;
                dmcBankState->addChildElement(sample);
            }

            xml->addChildElement(dmcBankState);
        }
    }

    {
        if (! spc700BrrSampleBank.empty())
        {
            auto* spcBrrBankState = new juce::XmlElement(spc700BrrBankStateTag);
            spcBrrBankState->setAttribute("count", static_cast<int>(spc700BrrSampleBank.size()));
            auto playableSlotCount = size_t { 0u };
            for (const auto& slot : spc700BrrSampleBank)
            {
                auto* sample = new juce::XmlElement(spc700BrrSampleStateTag);
                sample->setAttribute("path", slot.path);
                sample->setAttribute("included", slot.included ? 1 : 0);
                addSampleReferenceMetadata(*sample, slot);
                if (embedProjectAssets
                    && slot.included
                    && playableSlotCount < chipper::state::maxEmbeddedSampleSlotsPerBank)
                    addEmbeddedSamplePayload(*sample, slot, chipper::state::maxEmbeddedBankSlotBytes, embeddedBudget);
                if (slot.included)
                    ++playableSlotCount;
                spcBrrBankState->addChildElement(sample);
            }

            xml->addChildElement(spcBrrBankState);
        }
        else if (! spc700BrrSample.bytes.empty() || spc700BrrSample.path.isNotEmpty())
        {
            auto* spcBrrState = new juce::XmlElement(spc700BrrStateTag);
            spcBrrState->setAttribute("path", spc700BrrSample.path);
            addSampleReferenceMetadata(*spcBrrState, spc700BrrSample);
            if (embedProjectAssets)
                addEmbeddedSamplePayload(*spcBrrState, spc700BrrSample, chipper::state::maxEmbeddedBankSlotBytes, embeddedBudget);
            xml->addChildElement(spcBrrState);
        }
    }

    {
        if (! paulaSampleBank.empty())
        {
            auto* paulaBankState = new juce::XmlElement(paulaSampleBankStateTag);
            paulaBankState->setAttribute("count", static_cast<int>(paulaSampleBank.size()));
            auto playableSlotCount = size_t { 0u };
            for (const auto& slot : paulaSampleBank)
            {
                auto* sample = new juce::XmlElement(paulaSampleStateTag);
                sample->setAttribute("path", slot.path);
                sample->setAttribute("included", slot.included ? 1 : 0);
                addSampleReferenceMetadata(*sample, slot);
                if (embedProjectAssets
                    && slot.included
                    && playableSlotCount < chipper::state::maxEmbeddedSampleSlotsPerBank)
                    addEmbeddedSamplePayload(*sample, slot, chipper::state::maxEmbeddedBankSlotBytes, embeddedBudget);
                if (slot.included)
                    ++playableSlotCount;
                paulaBankState->addChildElement(sample);
            }

            xml->addChildElement(paulaBankState);
        }
    }

    {
        if (! opn2DacSample.bytes.empty() || opn2DacSample.path.isNotEmpty())
        {
            auto* dacSampleState = new juce::XmlElement(opn2DacSampleStateTag);
            dacSampleState->setAttribute("path", opn2DacSample.path);
            addSampleReferenceMetadata(*dacSampleState, opn2DacSample);
            addOpn2DacPlaybackMetadata(*dacSampleState, opn2DacSample);
            if (embedProjectAssets)
                addEmbeddedSamplePayload(*dacSampleState, opn2DacSample, maxEmbeddedOpn2Bytes, embeddedBudget);
            xml->addChildElement(dacSampleState);
        }
    }

    {
        const auto hasEditableRegions = std::any_of(opnaAdpcmARegions.begin(),
                                                   opnaAdpcmARegions.end(),
                                                   [](const auto& region)
                                                   {
                                                       return ! region.bytes.empty() || region.path.isNotEmpty();
                                                   });
        if (hasEditableRegions)
        {
            auto* rhythmRomState = new juce::XmlElement(opnaRhythmRomStateTag);
            rhythmRomState->setAttribute("bankMode", adpcmARegionsBankMode);
            rhythmRomState->setAttribute("count", static_cast<int>(chipper::yamahaAdpcm::regionCountA));
            for (size_t index = 0; index < opnaAdpcmARegions.size(); ++index)
            {
                const auto& region = opnaAdpcmARegions[index];
                if (region.bytes.empty() && region.path.isEmpty())
                    continue;

                auto* regionState = new juce::XmlElement(adpcmARegionStateTag);
                regionState->setAttribute("index", static_cast<int>(index));
                regionState->setAttribute("path", region.path);
                addSampleReferenceMetadata(*regionState, region);
                if (embedProjectAssets)
                    addEmbeddedSamplePayload(*regionState, region, maxEmbeddedOpnaRhythmBytes, embeddedBudget);
                rhythmRomState->addChildElement(regionState);
            }
            xml->addChildElement(rhythmRomState);
        }
        else if (! opnaRhythmRom.bytes.empty() || opnaRhythmRom.path.isNotEmpty())
        {
            auto* rhythmRomState = new juce::XmlElement(opnaRhythmRomStateTag);
            rhythmRomState->setAttribute("path", opnaRhythmRom.path);
            addSampleReferenceMetadata(*rhythmRomState, opnaRhythmRom);
            if (embedProjectAssets)
                addEmbeddedSamplePayload(*rhythmRomState, opnaRhythmRom, maxEmbeddedOpnaRhythmBytes, embeddedBudget);
            xml->addChildElement(rhythmRomState);
        }
    }
    {
        if (! opnaAdpcmBSample.bytes.empty() || opnaAdpcmBSample.path.isNotEmpty())
        {
            auto* adpcmBState = new juce::XmlElement(opnaAdpcmBSampleStateTag);
            adpcmBState->setAttribute("path", opnaAdpcmBSample.path);
            addSampleReferenceMetadata(*adpcmBState, opnaAdpcmBSample);
            if (embedProjectAssets)
                addEmbeddedSamplePayload(*adpcmBState, opnaAdpcmBSample, maxEmbeddedOpnaAdpcmBBytes, embeddedBudget);
            xml->addChildElement(adpcmBState);
        }
    }
    {
        const auto hasEditableRegions = std::any_of(opnbAdpcmARegions.begin(),
                                                   opnbAdpcmARegions.end(),
                                                   [](const auto& region)
                                                   {
                                                       return ! region.bytes.empty() || region.path.isNotEmpty();
                                                   });
        if (hasEditableRegions)
        {
            auto* adpcmAState = new juce::XmlElement(opnbAdpcmASampleStateTag);
            adpcmAState->setAttribute("bankMode", adpcmARegionsBankMode);
            adpcmAState->setAttribute("count", static_cast<int>(chipper::yamahaAdpcm::regionCountA));
            for (size_t index = 0; index < opnbAdpcmARegions.size(); ++index)
            {
                const auto& region = opnbAdpcmARegions[index];
                if (region.bytes.empty() && region.path.isEmpty())
                    continue;

                auto* regionState = new juce::XmlElement(adpcmARegionStateTag);
                regionState->setAttribute("index", static_cast<int>(index));
                regionState->setAttribute("path", region.path);
                addSampleReferenceMetadata(*regionState, region);
                if (embedProjectAssets)
                    addEmbeddedSamplePayload(*regionState, region, maxEmbeddedOpnbAdpcmABytes, embeddedBudget);
                adpcmAState->addChildElement(regionState);
            }
            xml->addChildElement(adpcmAState);
        }
        else if (! opnbAdpcmASample.bytes.empty() || opnbAdpcmASample.path.isNotEmpty())
        {
            auto* adpcmAState = new juce::XmlElement(opnbAdpcmASampleStateTag);
            adpcmAState->setAttribute("path", opnbAdpcmASample.path);
            addSampleReferenceMetadata(*adpcmAState, opnbAdpcmASample);
            if (embedProjectAssets)
                addEmbeddedSamplePayload(*adpcmAState, opnbAdpcmASample, maxEmbeddedOpnbAdpcmABytes, embeddedBudget);
            xml->addChildElement(adpcmAState);
        }
    }
    {
        if (! opnbAdpcmBSample.bytes.empty() || opnbAdpcmBSample.path.isNotEmpty())
        {
            auto* adpcmBState = new juce::XmlElement(opnbAdpcmBSampleStateTag);
            adpcmBState->setAttribute("path", opnbAdpcmBSample.path);
            addSampleReferenceMetadata(*adpcmBState, opnbAdpcmBSample);
            if (embedProjectAssets)
                addEmbeddedSamplePayload(*adpcmBState, opnbAdpcmBSample, maxEmbeddedOpnbAdpcmBBytes, embeddedBudget);
            xml->addChildElement(adpcmBState);
        }
    }

    return xml;
}

}
