#pragma once
#include "Engine/ChipCore.h"
namespace chipper::core_detail
{
std::unique_ptr<ChipCore> makeUnsupportedCore(ChipMode mode, AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeDmgApuCore(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeSidCore(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeNesApuCore(AccuracyMode accuracy, ChipMode mode = ChipMode::nes);
std::unique_ptr<ChipCore> makeYm2149Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makePokeyCore(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeSn76489Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeSaa1099Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makePcSpeakerCore(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeZxSpectrumBeeperCore(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeHuc6280Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeSccCore(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeNamcoWsgCore(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeSpc700Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeYm2413Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeYm2203Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeYm2608Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeYm2610Core(AccuracyMode accuracy, bool sixFmChannels = false);
std::unique_ptr<ChipCore> makeYm2612Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeOpl3Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makeYm2151Core(AccuracyMode accuracy);
std::unique_ptr<ChipCore> makePaulaCore(AccuracyMode accuracy);
}
