#include "PluginStateSchema.h"

#include "Engine/ControlRegistry.h"
#include "Parameters.h"

namespace chipper::state
{
namespace
{
juce::Result migrateSchema1To2(juce::XmlElement& xml)
{
    // Schema 1 used the same APVTS and asset tags but had no explicit version
    // marker. The explicit marker is the complete schema-2 migration.
    xml.setAttribute(schemaVersionAttribute, 2);
    return juce::Result::ok();
}

juce::Result migrateSchema2To3(juce::XmlElement& xml)
{
    // Schema 3 adds optional embedded custom Wave RAM. Existing schema-2
    // states have no custom lanes and therefore need only the version marker.
    xml.setAttribute(schemaVersionAttribute, 3);
    return juce::Result::ok();
}

juce::Result migrateSchema3To4(juce::XmlElement& xml)
{
    // Schema 4 adds optional per-chip tracker-motion patterns. Existing
    // schema-3 states use the disabled neutral pattern for every chip.
    xml.setAttribute(schemaVersionAttribute, 4);
    return juce::Result::ok();
}

void ensureChoiceParameterDefault(juce::XmlElement& xml, const char* parameterId)
{
    for (auto* child : xml.getChildIterator())
    {
        if (child != nullptr && child->getStringAttribute("id") == parameterId)
            return;
    }

    auto* parameter = new juce::XmlElement("PARAM");
    parameter->setAttribute("id", parameterId);
    parameter->setAttribute("value", 0.0);
    xml.addChildElement(parameter);
}

double choiceParameterValue(const juce::XmlElement& xml, const char* parameterId)
{
    for (const auto* child : xml.getChildIterator())
    {
        if (child != nullptr && child->getStringAttribute("id") == parameterId)
            return child->getDoubleAttribute("value", 0.0);
    }

    return 0.0;
}

void setChoiceParameterValue(juce::XmlElement& xml, const char* parameterId, double value)
{
    for (auto* child : xml.getChildIterator())
    {
        if (child != nullptr && child->getStringAttribute("id") == parameterId)
        {
            child->setAttribute("value", value);
            return;
        }
    }

    auto* parameter = new juce::XmlElement("PARAM");
    parameter->setAttribute("id", parameterId);
    parameter->setAttribute("value", value);
    xml.addChildElement(parameter);

}

juce::Result migrateSchema4To5(juce::XmlElement& xml)
{
    // Schema 5 adds direct YM2151 LFO waveform/PMS/AMS choices. Explicitly
    // backfill Preset so loading an older patch cannot retain stale overrides.
    ensureChoiceParameterDefault(xml, parameter_ids::opmLfoWaveform);
    ensureChoiceParameterDefault(xml, parameter_ids::opmLfoPms);
    ensureChoiceParameterDefault(xml, parameter_ids::opmLfoAms);
    xml.setAttribute(schemaVersionAttribute, 5);
    return juce::Result::ok();
}

juce::Result migrateSchema5To6(juce::XmlElement& xml)
{
    // Schema 6 adds direct per-operator YM2151 DT1/DT2 choices. Backfill
    // Preset so older state cannot inherit stale detune values from the target.
    for (const auto* parameterId : {
             parameter_ids::opmOperator1Dt1,
             parameter_ids::opmOperator2Dt1,
             parameter_ids::opmOperator3Dt1,
             parameter_ids::opmOperator4Dt1,
             parameter_ids::opmOperator1Dt2,
             parameter_ids::opmOperator2Dt2,
             parameter_ids::opmOperator3Dt2,
             parameter_ids::opmOperator4Dt2
         })
    {
        ensureChoiceParameterDefault(xml, parameterId);
    }
    xml.setAttribute(schemaVersionAttribute, 6);
    return juce::Result::ok();
}


juce::Result migrateSchema6To7(juce::XmlElement& xml)
{
    // Schema 7 reuses the stable YM2151 DT1/DT2 automation slots for native
    // OPL3 AM/VIB/KSR and KSL fields. APVTS stores choice values normalized,
    // so recover the saved chip choice before deciding whether these values
    // were genuine YM2151 detune overrides or previously ignored latent data.
    const auto chipChoices = parameters::chipModeChoices();
    const auto maxChipChoice = std::max(0, chipChoices.size() - 1);
    const auto savedChipValue = choiceParameterValue(xml, parameter_ids::chipMode);
    const auto savedChipChoice = savedChipValue > 1.0
        ? static_cast<int>(std::round(savedChipValue))
        : static_cast<int>(std::round(std::clamp(savedChipValue, 0.0, 1.0) * static_cast<double>(maxChipChoice)));

    if (parameters::chipModeFromChoice(savedChipChoice) != ChipMode::ym2151)
    {
        for (const auto* parameterId : {
                 parameter_ids::opmOperator1Dt1,
                 parameter_ids::opmOperator2Dt1,
                 parameter_ids::opmOperator3Dt1,
                 parameter_ids::opmOperator4Dt1,
                 parameter_ids::opmOperator1Dt2,
                 parameter_ids::opmOperator2Dt2,
                 parameter_ids::opmOperator3Dt2,
                 parameter_ids::opmOperator4Dt2
             })
        {
            setChoiceParameterValue(xml, parameterId, 0.0);
        }
    }

    xml.setAttribute(schemaVersionAttribute, 7);
    return juce::Result::ok();
}
}

juce::Result validateAndMigrate(juce::XmlElement& xml, const juce::Identifier& expectedRootType)
{
    if (! xml.hasTagName(expectedRootType))
        return juce::Result::fail("This file does not contain Chipper plugin state.");

    auto schemaVersion = 1;
    if (xml.hasAttribute(schemaVersionAttribute))
    {
        const auto versionText = xml.getStringAttribute(schemaVersionAttribute).trim();
        if (versionText.isEmpty() || ! versionText.containsOnly("0123456789") || versionText.length() > 9)
            return juce::Result::fail("This Chipper state has an invalid schema version.");
        schemaVersion = versionText.getIntValue();
    }

    if (schemaVersion < 1)
        return juce::Result::fail("This Chipper state has an invalid schema version.");
    if (schemaVersion > currentSchemaVersion)
        return juce::Result::fail("This Chipper state was created by a newer plugin version (state schema "
                                  + juce::String(schemaVersion) + ", supported through "
                                  + juce::String(currentSchemaVersion) + ").");

    while (schemaVersion < currentSchemaVersion)
    {
        juce::Result migration = juce::Result::fail("No migration is available for this Chipper state.");
        if (schemaVersion == 1)
            migration = migrateSchema1To2(xml);
        else if (schemaVersion == 2)
            migration = migrateSchema2To3(xml);
        else if (schemaVersion == 3)
            migration = migrateSchema3To4(xml);
        else if (schemaVersion == 4)
            migration = migrateSchema4To5(xml);
        else if (schemaVersion == 5)
            migration = migrateSchema5To6(xml);
        else if (schemaVersion == 6)
            migration = migrateSchema6To7(xml);
        if (migration.failed())
            return migration;
        ++schemaVersion;
    }

    xml.setAttribute(schemaVersionAttribute, schemaVersion);
    return juce::Result::ok();
}

juce::File resolvePortableAssetPath(const juce::XmlElement& assetState, const juce::File& presetDirectory)
{
    const auto relativePath = assetState.getStringAttribute("relativePath").trim();
    if (relativePath.isNotEmpty()
        && ! juce::File::isAbsolutePath(relativePath)
        && presetDirectory.isDirectory())
    {
        const auto relativeFile = presetDirectory.getChildFile(relativePath);
        if (relativeFile.isAChildOf(presetDirectory) && relativeFile.existsAsFile())
            return relativeFile;
    }

    const auto originalPath = assetState.getStringAttribute("path").trim();
    if (originalPath.isEmpty())
        return {};

    const juce::File originalFile(originalPath);
    if (originalFile.existsAsFile())
        return originalFile;

    if (presetDirectory.isDirectory())
    {
        const auto siblingFile = presetDirectory.getChildFile(originalFile.getFileName());
        if (siblingFile.existsAsFile())
            return siblingFile;

        const auto samplesFile = presetDirectory.getChildFile("Samples").getChildFile(originalFile.getFileName());
        if (samplesFile.existsAsFile())
            return samplesFile;

        const auto lowercaseSamplesFile = presetDirectory.getChildFile("samples").getChildFile(originalFile.getFileName());
        if (lowercaseSamplesFile.existsAsFile())
            return lowercaseSamplesFile;
    }

    return originalFile;
}

namespace
{
bool isAssetReferenceTag(const juce::String& tagName)
{
    return tagName == "DMC_SAMPLE"
        || tagName == "BRR_SAMPLE"
        || tagName == "PAULA_SAMPLE"
        || tagName == "CHIPPER_SPC700_BRR"
        || tagName == "CHIPPER_OPNA_RHYTHM_ROM"
        || tagName == "CHIPPER_OPNA_ADPCM_B_SAMPLE"
        || tagName == "CHIPPER_OPNB_ADPCM_A_SAMPLE"
        || tagName == "CHIPPER_OPNB_ADPCM_B_SAMPLE";
}

juce::String portableRelativePath(const juce::File& sampleFile, const juce::File& presetDirectory)
{
    if (presetDirectory == juce::File {} || sampleFile == juce::File {})
        return {};
    if (sampleFile.getParentDirectory() == presetDirectory || sampleFile.isAChildOf(presetDirectory))
        return sampleFile.getRelativePathFrom(presetDirectory);
    if (presetDirectory.getChildFile(sampleFile.getFileName()).existsAsFile())
        return sampleFile.getFileName();

    for (const auto& directoryName : { juce::String("Samples"), juce::String("samples") })
    {
        const auto candidate = presetDirectory.getChildFile(directoryName).getChildFile(sampleFile.getFileName());
        if (candidate.existsAsFile())
            return directoryName + "/" + sampleFile.getFileName();
    }
    return {};
}
}

void annotatePortableAssetReferences(juce::XmlElement& xml, const juce::File& presetDirectory)
{
    if (isAssetReferenceTag(xml.getTagName()) && xml.hasAttribute("path"))
    {
        const juce::File sampleFile(xml.getStringAttribute("path"));
        if (sampleFile != juce::File {})
        {
            xml.setAttribute("fileName", sampleFile.getFileName());
            const auto relativePath = portableRelativePath(sampleFile, presetDirectory);
            if (relativePath.isNotEmpty())
                xml.setAttribute("relativePath", relativePath);
        }
    }

    for (auto* child : xml.getChildIterator())
        if (child != nullptr)
            annotatePortableAssetReferences(*child, presetDirectory);
}
}
