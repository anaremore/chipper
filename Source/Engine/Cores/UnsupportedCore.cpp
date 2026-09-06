#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class UnsupportedCore final : public ChipCore
{
public:
    UnsupportedCore(ChipMode selectedMode, AccuracyMode selectedAccuracy)
        : selected(selectedMode), accuracy(selectedAccuracy)
    {
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz;
    }

    void setPatch(const PatchConfig& newPatch) override { patch = newPatch; }
    void writeRegister(uint16_t, uint8_t) override {}
    void noteOn(int, float) override {}
    void noteOff(int) override {}
    StereoFrame renderSample() override { return {}; }
    std::vector<RegisterWrite> exportRegisterState() const override { return {}; }

    ChipMode mode() const override { return selected; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return toString(selected); }
    std::string implementedAccuracy() const override { return "not implemented"; }
    std::string limitations() const override
    {
        return "No permissive emulation core or clean-room register model has been integrated for this mode yet; output is silent.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"" << jsonEscape(modeName()) << "\","
             << "\"implementedAccuracy\":\"not implemented\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"playMode\":\"" << jsonEscape(toString(patch.playMode)) << "\","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    ChipMode selected;
    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 0.0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeUnsupportedCore(ChipMode mode, AccuracyMode accuracy)
{
    return std::make_unique<UnsupportedCore>(mode, accuracy);
}
}
