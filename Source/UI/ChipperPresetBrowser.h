#pragma once

#include <JuceHeader.h>

#include "Engine/ChipDescriptors.h"
#include "UI/ChipUiModel.h"

#include <functional>
#include <optional>
#include <vector>

struct ChipperBrowserTheme
{
    juce::Colour background;
    juce::Colour panel;
    juce::Colour card;
    juce::Colour outline;
    juce::Colour primary;
    juce::Colour accent;
    juce::Colour text;
    juce::Colour mutedText;
    juce::Colour darkText;
};

class ChipperPresetBrowser final : public juce::Component
{
public:
    enum class EntryKind
    {
        factory,
        user
    };

    struct Entry
    {
        EntryKind kind = EntryKind::factory;
        chipper::ChipMode mode = chipper::ChipMode::nes;
        juce::String key;
        juce::String factoryPresetId;
        juce::File userFile;
        juce::String name;
        juce::String category;
        juce::String role;
        juce::String engine;
        juce::StringArray tags;
        juce::String note;
        juce::String bank;
        bool favorite = false;
        bool featured = false;
        int recentRank = -1;
    };

    ChipperPresetBrowser();

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void setTheme(const ChipperBrowserTheme& themeToUse);
    void setEntries(std::vector<Entry> entriesToUse, chipper::ChipMode focusMode);
    void open(chipper::ChipMode focusMode);
    void close();

    std::function<void(Entry)> onApply;
    std::function<void(Entry, bool)> onFavoriteChanged;
    std::function<void()> onClose;

    juce::Rectangle<int> searchBoundsForTest() const { return searchBox.getBounds(); }
    juce::Rectangle<int> chipListBoundsForTest() const { return chipList.getBounds(); }
    juce::Rectangle<int> presetListBoundsForTest() const { return presetList.getBounds(); }
    juce::Rectangle<int> detailBoundsForTest() const { return detailPanelBounds; }
    int resultCountForTest() const noexcept { return static_cast<int>(filteredEntryIndices.size()); }
    void setSearchTextForTest(const juce::String& text)
    {
        searchBox.setText(text, false);
        applyFilters();
    }
    void setScopeForTest(int scopeId) { scopeBox.setSelectedId(scopeId, juce::sendNotificationSync); }
    void selectAllChipsForTest() { chipList.selectRow(0); }
    void applyFirstResultForTest()
    {
        if (! filteredEntryIndices.empty())
        {
            presetList.selectRow(0);
            applySelectedPreset();
        }
    }
    juce::Component& searchComponentForTest() noexcept { return searchBox; }

private:
    struct ChipRow
    {
        juce::String label;
        std::optional<chipper::ChipMode> mode;
        std::optional<chipper::ui::ChipBrowserGroup> group;
        bool all = false;
        bool heading = false;
    };

    class ChipListModel final : public juce::ListBoxModel
    {
    public:
        explicit ChipListModel(ChipperPresetBrowser& ownerToUse) : owner(ownerToUse) {}
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& graphics, int width, int height, bool selected) override;
        void selectedRowsChanged(int lastRowSelected) override;

    private:
        ChipperPresetBrowser& owner;
    };

    class PresetListModel final : public juce::ListBoxModel
    {
    public:
        explicit PresetListModel(ChipperPresetBrowser& ownerToUse) : owner(ownerToUse) {}
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& graphics, int width, int height, bool selected) override;
        void selectedRowsChanged(int lastRowSelected) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    private:
        ChipperPresetBrowser& owner;
    };

    void rebuildChipRows(chipper::ChipMode focusMode);
    void rebuildRoleChoices();
    void applyFilters();
    void selectChipRow(int row);
    void selectPresetRow(int row);
    void updateDetail();
    void applySelectedPreset();
    const Entry* selectedEntry() const;
    bool entryMatchesSearch(const Entry& entry) const;
    void configureLabel(juce::Label& label, float size, bool bold);

    ChipperBrowserTheme theme;
    std::vector<Entry> allEntries;
    std::vector<int> filteredEntryIndices;
    std::vector<ChipRow> chipRows;
    std::optional<chipper::ChipMode> selectedChip;
    std::optional<chipper::ui::ChipBrowserGroup> selectedGroup;
    int selectedEntryRow = -1;

    juce::Label titleLabel;
    juce::Label resultLabel;
    juce::TextEditor searchBox;
    juce::ComboBox scopeBox;
    juce::ComboBox roleBox;
    juce::TextButton closeButton;
    ChipListModel chipListModel;
    PresetListModel presetListModel;
    juce::ListBox chipList;
    juce::ListBox presetList;
    juce::Label detailTitleLabel;
    juce::Label detailBadgesLabel;
    juce::TextEditor detailText;
    juce::TextButton favoriteButton;
    juce::TextButton applyButton;
    juce::Rectangle<int> chipPanelBounds;
    juce::Rectangle<int> presetPanelBounds;
    juce::Rectangle<int> detailPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperPresetBrowser)
};
