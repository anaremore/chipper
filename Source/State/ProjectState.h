#pragma once
#include "SampleAsset.h"
#include "MotionState.h"
#include "WavetableState.h"
namespace chipper::state
{
struct ProjectStateSnapshot
{
    juce::ValueTree parameters;
    std::string coreMode, accuracy;
    std::vector<RegisterWrite> registers;
    WavetableState wavetables;
    MotionState motion;
    std::vector<SampleAsset> dmcSampleBank;
    std::vector<SampleAsset> spc700BrrSampleBank;
    std::vector<SampleAsset> paulaSampleBank;
    std::vector<SampleAsset> opnaAdpcmARegions;
    std::vector<SampleAsset> opnbAdpcmARegions;
    SampleAsset spc700BrrSample;
    SampleAsset opn2DacSample;
    SampleAsset opnaRhythmRom;
    SampleAsset opnaAdpcmBSample;
    SampleAsset opnbAdpcmASample;
    SampleAsset opnbAdpcmBSample;
};
std::unique_ptr<juce::XmlElement> serializeProjectState(const ProjectStateSnapshot&, bool embedProjectAssets);
}
