#include "ChipperPresetBrowser.h"

#include <algorithm>

namespace
{
constexpr int allScopeId = 1;
constexpr int favoriteScopeId = 2;
constexpr int recentScopeId = 3;
constexpr int userScopeId = 4;

void drawPanel(juce::Graphics& graphics, juce::Rectangle<int> bounds, const ChipperBrowserTheme& theme)
{
    graphics.setColour(theme.panel);
    graphics.fillRoundedRectangle(bounds.toFloat(), 5.0f);
    graphics.setColour(theme.outline);
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 5.0f, 1.0f);
}
}

ChipperPresetBrowser::ChipperPresetBrowser()
    : chipListModel(*this),
      presetListModel(*this),
      chipList("Chip browser", &chipListModel),
      presetList("Preset results", &presetListModel)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setComponentID("presetBrowser.surface");
    setName("Global sound browser");
    setExplicitFocusOrder(299);

    configureLabel(titleLabel, 18.0f, true);
    titleLabel.setText("SOUND BROWSER", juce::dontSendNotification);
    addAndMakeVisible(titleLabel);
    configureLabel(resultLabel, 11.0f, false);
    resultLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(resultLabel);

    searchBox.setMultiLine(false);
    searchBox.setReturnKeyStartsNewLine(false);
    searchBox.setSelectAllWhenFocused(true);
    searchBox.setTextToShowWhenEmpty("Search name, role, engine, tag, or note", juce::Colours::grey);
    searchBox.setComponentID("presetBrowser.search");
    searchBox.setExplicitFocusOrder(300);
    searchBox.onTextChange = [this] { applyFilters(); };
    addAndMakeVisible(searchBox);

    scopeBox.addItem("All sounds", allScopeId);
    scopeBox.addItem("Favorites", favoriteScopeId);
    scopeBox.addItem("Recent", recentScopeId);
    scopeBox.addItem("User presets", userScopeId);
    scopeBox.setSelectedId(allScopeId);
    scopeBox.setComponentID("presetBrowser.scope");
    scopeBox.setExplicitFocusOrder(301);
    scopeBox.onChange = [this] { applyFilters(); };
    addAndMakeVisible(scopeBox);

    roleBox.setTextWhenNothingSelected("All roles");
    roleBox.setComponentID("presetBrowser.role");
    roleBox.setExplicitFocusOrder(302);
    roleBox.onChange = [this] { applyFilters(); };
    addAndMakeVisible(roleBox);

    closeButton.setButtonText("Close");
    closeButton.setWantsKeyboardFocus(true);
    closeButton.setComponentID("presetBrowser.close");
    closeButton.setExplicitFocusOrder(308);
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible(closeButton);

    chipList.setRowHeight(26);
    chipList.setOutlineThickness(0);
    chipList.setComponentID("presetBrowser.chips");
    chipList.setWantsKeyboardFocus(true);
    chipList.setExplicitFocusOrder(303);
    addAndMakeVisible(chipList);
    presetList.setRowHeight(42);
    presetList.setOutlineThickness(0);
    presetList.setComponentID("presetBrowser.results");
    presetList.setWantsKeyboardFocus(true);
    presetList.setExplicitFocusOrder(304);
    addAndMakeVisible(presetList);

    configureLabel(detailTitleLabel, 16.0f, true);
    addAndMakeVisible(detailTitleLabel);
    configureLabel(detailBadgesLabel, 11.0f, true);
    addAndMakeVisible(detailBadgesLabel);

    detailText.setMultiLine(true, true);
    detailText.setReadOnly(true);
    detailText.setCaretVisible(false);
    detailText.setScrollbarsShown(true);
    detailText.setPopupMenuEnabled(true);
    detailText.setBorder(juce::BorderSize<int>(8));
    detailText.setComponentID("presetBrowser.detail");
    detailText.setExplicitFocusOrder(305);
    addAndMakeVisible(detailText);

    favoriteButton.setButtonText("Favorite");
    favoriteButton.setWantsKeyboardFocus(true);
    favoriteButton.setComponentID("presetBrowser.favorite");
    favoriteButton.setExplicitFocusOrder(306);
    favoriteButton.onClick = [this]
    {
        if (selectedEntryRow >= 0 && static_cast<size_t>(selectedEntryRow) < filteredEntryIndices.size())
        {
            const auto entryIndex = filteredEntryIndices[static_cast<size_t>(selectedEntryRow)];
            if (entryIndex < 0 || static_cast<size_t>(entryIndex) >= allEntries.size())
                return;
            auto& entry = allEntries[static_cast<size_t>(entryIndex)];
            const auto shouldBeFavorite = ! entry.favorite;
            entry.favorite = shouldBeFavorite;
            const auto copy = entry;
            updateDetail();
            presetList.repaint();
            if (onFavoriteChanged)
                onFavoriteChanged(copy, shouldBeFavorite);
        }
    };
    addAndMakeVisible(favoriteButton);

    applyButton.setButtonText("Load Sound");
    applyButton.setWantsKeyboardFocus(true);
    applyButton.setComponentID("presetBrowser.apply");
    applyButton.setExplicitFocusOrder(307);
    applyButton.onClick = [this] { applySelectedPreset(); };
    addAndMakeVisible(applyButton);
}

void ChipperPresetBrowser::configureLabel(juce::Label& label, float size, bool bold)
{
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}

void ChipperPresetBrowser::paint(juce::Graphics& graphics)
{
    graphics.fillAll(theme.background);
    drawPanel(graphics, chipPanelBounds, theme);
    drawPanel(graphics, presetPanelBounds, theme);
    drawPanel(graphics, detailPanelBounds, theme);
}

void ChipperPresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(14, 12);
    auto heading = area.removeFromTop(34);
    titleLabel.setBounds(heading.removeFromLeft(220));
    closeButton.setBounds(heading.removeFromRight(70).reduced(0, 2));
    resultLabel.setBounds(heading.removeFromRight(std::min(200, heading.getWidth())));
    area.removeFromTop(8);

    auto filters = area.removeFromTop(34);
    scopeBox.setBounds(filters.removeFromLeft(150));
    filters.removeFromLeft(8);
    roleBox.setBounds(filters.removeFromLeft(160));
    filters.removeFromLeft(8);
    searchBox.setBounds(filters);
    area.removeFromTop(10);

    constexpr auto gap = 10;
    const auto chipWidth = std::clamp(area.getWidth() / 5, 190, 220);
    const auto detailWidth = std::clamp(area.getWidth() / 3, 330, 390);
    chipPanelBounds = area.removeFromLeft(chipWidth);
    area.removeFromLeft(gap);
    detailPanelBounds = area.removeFromRight(detailWidth);
    area.removeFromRight(gap);
    presetPanelBounds = area;

    chipList.setBounds(chipPanelBounds.reduced(8));
    presetList.setBounds(presetPanelBounds.reduced(8));

    auto detail = detailPanelBounds.reduced(14, 12);
    detailTitleLabel.setBounds(detail.removeFromTop(28));
    detailBadgesLabel.setBounds(detail.removeFromTop(22));
    detail.removeFromTop(6);
    auto actions = detail.removeFromBottom(34);
    applyButton.setBounds(actions.removeFromRight(118));
    actions.removeFromRight(8);
    favoriteButton.setBounds(actions.removeFromRight(96));
    detail.removeFromBottom(8);
    detailText.setBounds(detail);
}

bool ChipperPresetBrowser::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'F' || key.getKeyCode() == 'f'))
    {
        searchBox.grabKeyboardFocus();
        searchBox.selectAll();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::returnKey && presetList.hasKeyboardFocus(true))
    {
        applySelectedPreset();
        return true;
    }
    return juce::Component::keyPressed(key);
}

void ChipperPresetBrowser::setTheme(const ChipperBrowserTheme& themeToUse)
{
    theme = themeToUse;
    for (auto* label : { &titleLabel, &resultLabel, &detailTitleLabel, &detailBadgesLabel })
        label->setColour(juce::Label::textColourId, label == &resultLabel ? theme.mutedText : theme.text);
    titleLabel.setColour(juce::Label::textColourId, theme.primary);
    detailBadgesLabel.setColour(juce::Label::textColourId, theme.accent);

    for (auto* box : { &scopeBox, &roleBox })
    {
        box->setColour(juce::ComboBox::backgroundColourId, theme.card);
        box->setColour(juce::ComboBox::textColourId, theme.text);
        box->setColour(juce::ComboBox::outlineColourId, theme.outline);
        box->setColour(juce::ComboBox::arrowColourId, theme.text);
    }
    searchBox.setColour(juce::TextEditor::backgroundColourId, theme.card);
    searchBox.setColour(juce::TextEditor::textColourId, theme.text);
    searchBox.setColour(juce::TextEditor::outlineColourId, theme.outline);
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, theme.accent);
    detailText.setColour(juce::TextEditor::backgroundColourId, theme.card);
    detailText.setColour(juce::TextEditor::textColourId, theme.text);
    detailText.setColour(juce::TextEditor::outlineColourId, theme.outline);
    for (auto* button : { &closeButton, &favoriteButton, &applyButton })
    {
        button->setColour(juce::TextButton::buttonColourId, theme.card);
        button->setColour(juce::TextButton::buttonOnColourId, theme.primary);
        button->setColour(juce::TextButton::textColourOffId, theme.text);
        button->setColour(juce::TextButton::textColourOnId, theme.darkText);
    }
    applyButton.setColour(juce::TextButton::buttonColourId, theme.primary);
    applyButton.setColour(juce::TextButton::textColourOffId, theme.darkText);
    chipList.setColour(juce::ListBox::backgroundColourId, theme.panel);
    presetList.setColour(juce::ListBox::backgroundColourId, theme.panel);
    repaint();
}

void ChipperPresetBrowser::setEntries(std::vector<Entry> entriesToUse, chipper::ChipMode focusMode)
{
    allEntries = std::move(entriesToUse);
    rebuildChipRows(focusMode);
    rebuildRoleChoices();
    applyFilters();
}

void ChipperPresetBrowser::open(chipper::ChipMode focusMode)
{
    selectedChip = focusMode;
    selectedGroup.reset();
    for (size_t i = 0; i < chipRows.size(); ++i)
    {
        if (chipRows[i].mode == selectedChip)
        {
            chipList.selectRow(static_cast<int>(i));
            break;
        }
    }
    setVisible(true);
    toFront(true);
    searchBox.grabKeyboardFocus();
    applyFilters();
}

void ChipperPresetBrowser::close()
{
    setVisible(false);
    if (onClose)
        onClose();
}

void ChipperPresetBrowser::rebuildChipRows(chipper::ChipMode focusMode)
{
    chipRows.clear();
    chipRows.push_back({ "All chips", std::nullopt, std::nullopt, true, false });
    for (const auto group : chipper::ui::browserGroupOrder())
    {
        chipRows.push_back({ juce::String(chipper::ui::labelFor(group).data()), std::nullopt, group, false, true });
        for (const auto mode : chipper::ui::modesInBrowserGroup(group))
            chipRows.push_back({ juce::String(chipper::descriptorFor(mode).displayName), mode, group, false, false });
    }
    selectedChip = focusMode;
    selectedGroup.reset();
    chipList.updateContent();
}

void ChipperPresetBrowser::rebuildRoleChoices()
{
    juce::StringArray roles;
    for (const auto& entry : allEntries)
    {
        if (entry.role.isNotEmpty())
            roles.addIfNotAlreadyThere(entry.role);
    }
    roles.sort(true);
    roleBox.clear(juce::dontSendNotification);
    roleBox.addItem("All roles", 1);
    for (int i = 0; i < roles.size(); ++i)
        roleBox.addItem(roles[i], i + 2);
    roleBox.setSelectedId(1, juce::dontSendNotification);
}

void ChipperPresetBrowser::applyFilters()
{
    filteredEntryIndices.clear();
    const auto scope = scopeBox.getSelectedId();
    const auto selectedRole = roleBox.getSelectedId() > 1 ? roleBox.getText() : juce::String();
    for (size_t i = 0; i < allEntries.size(); ++i)
    {
        const auto& entry = allEntries[i];
        if (selectedChip.has_value() && entry.mode != *selectedChip)
            continue;
        if (! selectedChip.has_value() && selectedGroup.has_value()
            && chipper::ui::profileFor(entry.mode).browserGroup != *selectedGroup)
            continue;
        if (scope == favoriteScopeId && ! entry.favorite)
            continue;
        if (scope == recentScopeId && entry.recentRank < 0)
            continue;
        if (scope == userScopeId && entry.kind != EntryKind::user)
            continue;
        if (selectedRole.isNotEmpty() && entry.role != selectedRole)
            continue;
        if (! entryMatchesSearch(entry))
            continue;
        filteredEntryIndices.push_back(static_cast<int>(i));
    }

    if (scope == recentScopeId)
    {
        std::stable_sort(filteredEntryIndices.begin(), filteredEntryIndices.end(), [this](int left, int right)
        {
            return allEntries[static_cast<size_t>(left)].recentRank < allEntries[static_cast<size_t>(right)].recentRank;
        });
    }

    resultLabel.setText(juce::String(static_cast<int>(filteredEntryIndices.size())) + " sounds", juce::dontSendNotification);
    presetList.updateContent();
    selectedEntryRow = filteredEntryIndices.empty() ? -1 : 0;
    presetList.selectRow(selectedEntryRow);
    updateDetail();
}

bool ChipperPresetBrowser::entryMatchesSearch(const Entry& entry) const
{
    const auto query = searchBox.getText().trim();
    if (query.isEmpty())
        return true;

    const auto corpus = entry.name + " " + entry.category + " " + entry.role + " " + entry.engine + " "
        + entry.tags.joinIntoString(" ") + " " + entry.note + " " + entry.bank + " "
        + juce::String(chipper::descriptorFor(entry.mode).displayName);
    return corpus.containsIgnoreCase(query);
}

void ChipperPresetBrowser::selectChipRow(int row)
{
    if (row < 0 || static_cast<size_t>(row) >= chipRows.size())
        return;
    const auto& chipRow = chipRows[static_cast<size_t>(row)];
    selectedChip = chipRow.mode;
    selectedGroup = chipRow.heading ? chipRow.group : std::optional<chipper::ui::ChipBrowserGroup> {};
    if (chipRow.all)
    {
        selectedChip.reset();
        selectedGroup.reset();
    }
    applyFilters();
}

void ChipperPresetBrowser::selectPresetRow(int row)
{
    selectedEntryRow = row;
    updateDetail();
}

const ChipperPresetBrowser::Entry* ChipperPresetBrowser::selectedEntry() const
{
    if (selectedEntryRow < 0 || static_cast<size_t>(selectedEntryRow) >= filteredEntryIndices.size())
        return nullptr;
    const auto index = filteredEntryIndices[static_cast<size_t>(selectedEntryRow)];
    return index >= 0 && static_cast<size_t>(index) < allEntries.size() ? &allEntries[static_cast<size_t>(index)] : nullptr;
}

void ChipperPresetBrowser::updateDetail()
{
    const auto* entry = selectedEntry();
    const auto hasEntry = entry != nullptr;
    favoriteButton.setEnabled(hasEntry);
    applyButton.setEnabled(hasEntry);
    if (! hasEntry)
    {
        detailTitleLabel.setText("No matching sounds", juce::dontSendNotification);
        detailBadgesLabel.setText("Adjust chip, scope, role, or search", juce::dontSendNotification);
        detailText.setText("The current filters do not match a factory or user preset.", false);
        favoriteButton.setButtonText("Favorite");
        return;
    }

    const auto& descriptor = chipper::descriptorFor(entry->mode);
    detailTitleLabel.setText(entry->name, juce::dontSendNotification);
    detailBadgesLabel.setText(juce::String(descriptor.displayName) + " / " + entry->role + " / " + entry->engine,
                              juce::dontSendNotification);
    juce::String detail = entry->category;
    if (entry->bank.isNotEmpty())
        detail << "\nBank: " << entry->bank;
    if (! entry->tags.isEmpty())
        detail << "\nTags: " << entry->tags.joinIntoString(", ");
    if (entry->note.isNotEmpty())
        detail << "\n\n" << entry->note;
    detailText.setText(detail, false);
    favoriteButton.setButtonText(entry->favorite ? "Unfavorite" : "Favorite");
}

void ChipperPresetBrowser::applySelectedPreset()
{
    if (const auto* entry = selectedEntry())
    {
        const auto copy = *entry;
        if (onApply)
            onApply(copy);
    }
}

int ChipperPresetBrowser::ChipListModel::getNumRows()
{
    return static_cast<int>(owner.chipRows.size());
}

void ChipperPresetBrowser::ChipListModel::paintListBoxItem(int row, juce::Graphics& graphics, int width, int height, bool selected)
{
    if (row < 0 || static_cast<size_t>(row) >= owner.chipRows.size())
        return;
    const auto& item = owner.chipRows[static_cast<size_t>(row)];
    if (selected)
    {
        graphics.setColour(owner.theme.primary.withAlpha(0.24f));
        graphics.fillRoundedRectangle(juce::Rectangle<float>(2.0f, 1.0f, static_cast<float>(width - 4), static_cast<float>(height - 2)), 3.0f);
    }
    graphics.setColour(item.heading ? owner.theme.accent : owner.theme.text);
    graphics.setFont(juce::FontOptions(item.heading ? 10.5f : 12.0f, item.heading ? juce::Font::bold : juce::Font::plain));
    const auto left = item.heading ? 6 : (item.all ? 6 : 16);
    graphics.drawText(item.label, left, 0, width - left - 5, height, juce::Justification::centredLeft, true);
}

void ChipperPresetBrowser::ChipListModel::selectedRowsChanged(int lastRowSelected)
{
    owner.selectChipRow(lastRowSelected);
}

int ChipperPresetBrowser::PresetListModel::getNumRows()
{
    return static_cast<int>(owner.filteredEntryIndices.size());
}

void ChipperPresetBrowser::PresetListModel::paintListBoxItem(int row, juce::Graphics& graphics, int width, int height, bool selected)
{
    if (row < 0 || static_cast<size_t>(row) >= owner.filteredEntryIndices.size())
        return;
    const auto index = owner.filteredEntryIndices[static_cast<size_t>(row)];
    if (index < 0 || static_cast<size_t>(index) >= owner.allEntries.size())
        return;
    const auto& entry = owner.allEntries[static_cast<size_t>(index)];
    if (selected)
    {
        graphics.setColour(owner.theme.primary.withAlpha(0.20f));
        graphics.fillRoundedRectangle(juce::Rectangle<float>(2.0f, 2.0f, static_cast<float>(width - 4), static_cast<float>(height - 4)), 4.0f);
    }
    graphics.setColour(owner.theme.text);
    graphics.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    const auto favorite = entry.favorite ? juce::String("Fav / ") : juce::String();
    graphics.drawText(favorite + entry.name, 9, 3, width - 18, 19, juce::Justification::centredLeft, true);
    graphics.setColour(owner.theme.mutedText);
    graphics.setFont(juce::FontOptions(10.5f));
    const auto kind = entry.kind == EntryKind::user ? juce::String("User") : juce::String("Factory");
    graphics.drawText(kind + " / " + entry.category + " / " + entry.role,
                      9, 21, width - 18, height - 23, juce::Justification::centredLeft, true);
}

void ChipperPresetBrowser::PresetListModel::selectedRowsChanged(int lastRowSelected)
{
    owner.selectPresetRow(lastRowSelected);
}

void ChipperPresetBrowser::PresetListModel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    owner.selectPresetRow(row);
    owner.applySelectedPreset();
}
