// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Tab.h"
#include "Utils.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
}

Tab::Tab(const GUID& profile, const TermControl& control)
{
    _rootPane = std::make_shared<Pane>(profile, control, true);

    _rootPane->Closed([=]() {
        _closedHandlers();
    });

    _activePane = _rootPane;

    // Add an event handler to this pane's GotFocus event. When that pane gains
    // focus, we'll mark it as the new active pane. This pane will propogate
    // this function down as it is split, so only leaves will trigger this.
    _rootPane->SetGotFocusCallback([this](std::shared_ptr<Pane> sender) {
        _rootPane->ClearActive();
        _activePane = sender;
        _activePane->SetActive();

        // // void TerminalPage::_UpdateTabIcon(std::shared_ptr<Tab> tab)
        // {
        //     const auto lastFocusedProfileOpt = this->GetFocusedProfile();
        //     if (lastFocusedProfileOpt.has_value())
        //     {
        //         const auto lastFocusedProfile = lastFocusedProfileOpt.value();
        //         const auto* const matchingProfile = _settings->FindProfile(lastFocusedProfile);
        //         if (matchingProfile)
        //         {
        //             tab->UpdateIcon(matchingProfile->GetExpandedIconPath());
        //         }
        //         else
        //         {
        //             tab->UpdateIcon({});
        //         }
        //     }
        // }
        if (_pfnActivePaneChanged)
        {
            _pfnActivePaneChanged();
            auto newTabTitle = this->GetFocusedTitle();
            this->SetTabText(newTabTitle);
        }
    });

    _AttachEventHandersToControl(control);

    _MakeTabViewItem();
}

void Tab::_MakeTabViewItem()
{
    _tabViewItem = ::winrt::MUX::Controls::TabViewItem{};
}

UIElement Tab::GetRootElement()
{
    return _rootPane->GetRootElement();
}

// Method Description:
// - Returns nullptr if no children of this tab were the last control to be
//   focused, or the TermControl that _was_ the last control to be focused (if
//   there was one).
// - This control might not currently be focused, if the tab itself is not
//   currently focused.
// Arguments:
// - <none>
// Return Value:
// - nullptr if no children were marked `_lastFocused`, else the TermControl
//   that was last focused.
TermControl Tab::GetActiveTerminalControl() const
{
    return _activePane->GetTerminalControl();
}

winrt::MUX::Controls::TabViewItem Tab::GetTabViewItem()
{
    return _tabViewItem;
}

// Method Description:
// - Returns true if this is the currently focused tab. For any set of tabs,
//   there should only be one tab that is marked as focused, though each tab has
//   no control over the other tabs in the set.
// Arguments:
// - <none>
// Return Value:
// - true iff this tab is focused.
bool Tab::IsFocused() const noexcept
{
    return _focused;
}

// Method Description:
// - Updates our focus state. If we're gaining focus, make sure to transfer
//   focus to the last focused terminal control in our tree of controls.
// Arguments:
// - focused: our new focus state. If true, we should be focused. If false, we
//   should be unfocused.
// Return Value:
// - <none>
void Tab::SetFocused(const bool focused)
{
    _focused = focused;

    if (_focused)
    {
        _Focus();
    }
}

// Method Description:
// - Returns nullopt if no children of this tab were the last control to be
//   focused, or the GUID of the profile of the last control to be focused (if
//   there was one).
// Arguments:
// - <none>
// Return Value:
// - nullopt if no children of this tab were the last control to be
//   focused, else the GUID of the profile of the last control to be focused
std::optional<GUID> Tab::GetFocusedProfile() const noexcept
{
    return _activePane->GetFocusedProfile();
}

// Method Description:
// - Attempts to update the settings of this tab's tree of panes.
// Arguments:
// - settings: The new TerminalSettings to apply to any matching controls
// - profile: The GUID of the profile these settings should apply to.
// Return Value:
// - <none>
void Tab::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
{
    _rootPane->UpdateSettings(settings, profile);
}

// Method Description:
// - Focus the last focused control in our tree of panes.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::_Focus()
{
    _focused = true;

    auto lastFocusedControl = GetActiveTerminalControl();
    if (lastFocusedControl)
    {
        lastFocusedControl.Focus(FocusState::Programmatic);
    }
}

// Method Description:
// - Update the focus state of this tab's tree of panes. If one of the controls
//   under this tab is focused, then it will be marked as the last focused. If
//   there are no focused panes, then there will not be a last focused control
//   when this returns.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::UpdateFocus()
{
    // _rootPane->UpdateFocus();
    // Maybe root->ClearActive, active->SetActive
}

void Tab::UpdateIcon(const winrt::hstring iconPath)
{
    // Don't reload our icon if it hasn't changed.
    if (iconPath == _lastIconPath)
    {
        return;
    }

    _lastIconPath = iconPath;

    _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
        _tabViewItem.IconSource(GetColoredIcon<winrt::MUX::Controls::IconSource>(_lastIconPath));
    });
}

// Method Description:
// - Gets the title string of the last focused terminal control in our tree.
//   Returns the empty string if there is no such control.
// Arguments:
// - <none>
// Return Value:
// - the title string of the last focused terminal control in our tree.
winrt::hstring Tab::GetFocusedTitle() const
{
    // const auto lastFocusedControl = _rootPane->GetActiveTerminalControl();
    const auto lastFocusedControl = GetActiveTerminalControl();
    return lastFocusedControl ? lastFocusedControl.Title() : L"";
}

// Method Description:
// - Set the text on the TabViewItem for this tab.
// Arguments:
// - text: The new text string to use as the Header for our TabViewItem
// Return Value:
// - <none>
void Tab::SetTabText(const winrt::hstring& text)
{
    // Copy the hstring, so we don't capture a dead reference
    winrt::hstring textCopy{ text };
    _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [text = std::move(textCopy), this]() {
        _tabViewItem.Header(winrt::box_value(text));
    });
}

// Method Description:
// - Move the viewport of the terminal up or down a number of lines. Negative
//      values of `delta` will move the view up, and positive values will move
//      the viewport down.
// Arguments:
// - delta: a number of lines to move the viewport relative to the current viewport.
// Return Value:
// - <none>
void Tab::Scroll(const int delta)
{
    auto control = GetActiveTerminalControl();
    control.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [control, delta]() {
        const auto currentOffset = control.GetScrollOffset();
        control.KeyboardScrollViewport(currentOffset + delta);
    });
}

// Method Description:
// - Determines whether the focused pane has sufficient space to be split.
// Arguments:
// - splitType: The type of split we want to create.
// Return Value:
// - True if the focused pane can be split. False otherwise.
bool Tab::CanSplitPane(Pane::SplitState splitType)
{
    return _activePane->CanSplit(splitType);
}

// Method Description:
// - Split the focused pane in our tree of panes, and place the
//   given TermControl into the newly created pane.
// Arguments:
// - splitType: The type of split we want to create.
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Tab::SplitPane(Pane::SplitState splitType, const GUID& profile, TermControl& control)
{
    _activePane->Split(splitType, profile, control);

    _AttachEventHandersToControl(control);
}

// Method Description:
// - Update the size of our panes to fill the new given size. This happens when
//   the window is resized.
// Arguments:
// - newSize: the amount of space that the panes have to fill now.
// Return Value:
// - <none>
void Tab::ResizeContent(const winrt::Windows::Foundation::Size& newSize)
{
    _activePane->ResizeContent(newSize);
}

// Method Description:
// - Attempt to move a separator between panes, as to resize each child on
//   either size of the separator. See Pane::ResizePane for details.
// Arguments:
// - direction: The direction to move the separator in.
// Return Value:
// - <none>
void Tab::ResizePane(const winrt::TerminalApp::Direction& direction)
{
    _activePane->ResizePane(direction);
}

// Method Description:
// - Attempt to move focus between panes, as to focus the child on
//   the other side of the separator. See Pane::NavigateFocus for details.
// Arguments:
// - direction: The direction to move the focus in.
// Return Value:
// - <none>
void Tab::NavigateFocus(const winrt::TerminalApp::Direction& direction)
{
    _activePane->NavigateFocus(direction);
}

// Method Description:
// - Closes the currently focused pane in this tab. If it's the last pane in
//   this tab, our Closed event will be fired (at a later time) for anyone
//   registered as a handler of our close event.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::ClosePane()
{
    auto focused = _activePane->GetFocusedPane();
    focused->Close();
}

// Method Description:
// - Register any event handlers that we may need with the given TermControl.
//   This should be called on each and every TermControl that we add to the tree
//   of Panes in this tab. We'll add events too:
//   * notify us when the control's title changed, so we can update our own
//     title (if necessary)
// Arguments:
// - control: the TermControl to add events to.
// Return Value:
// - <none>
void Tab::_AttachEventHandersToControl(const TermControl& control)
{
    control.TitleChanged([this](auto newTitle) {
        // The title of the control changed, but not necessarily the title
        // of the tab. Get the title of the active pane of the tab, and set
        // the tab's text to the active panes' text.
        auto newTabTitle = GetFocusedTitle();
        SetTabText(newTabTitle);
    });
}

// Method Description:
// - Register a callback function to be called when this Tab's active pane
//   changes. The App will use this to lookup the appropriate icon for this Tab
//   (based on the newly-active pane) and also to possibly propogate the new
//   Pane's title to the window.
// Arguments:
// - pfnGotFocus: A function that should be called when this Tab's active pane changes
// Return Value:
// - <none>
void Tab::SetActivePaneChangedCallback(std::function<void()> pfnActivePaneChanged)
{
    _pfnActivePaneChanged = pfnActivePaneChanged;
}

DEFINE_EVENT(Tab, Closed, _closedHandlers, ConnectionClosedEventArgs);
