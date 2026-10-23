# Controls

The built-in controls are styled by the nearest `Theme` (see [Theming](Theming.md)), handle the keyboard when
focused, animate their hover and toggle states, and describe themselves to screen readers. `Showcase/main.cpp` shows
each of them in use.

## Buttons

```cpp
Button {.Label = "Save", .OnPressed = [this] { Save(); }, .Variant = ButtonVariant::Primary}
Button {.Label = "■", .OnPressed = [this] { Stop(); }, .Width = 36.0f, .AccessibleLabel = "Stop"}
```

| Field | Meaning |
|---|---|
| `Label`, `OnPressed` | Text and action. Enter and Space press a focused button |
| `Variant` | `Primary` (accent fill), `Secondary` (default, bordered) or `Ghost` (no fill until hovered) |
| `Enabled` | A disabled button is dimmed, not focusable and ignores the pointer |
| `Width` | Fixed width; otherwise the button fits its label, or fills the width a column stretches it to |
| `AccessibleLabel` | What screen readers say when `Label` is a symbol |

## Toggles

```cpp
Checkbox {.Value = Model->Loop(), .OnChanged = [Model](bool On) { Model->Loop = On; }, .Label = "Loop"}
Switch {.Value = Model->Snap(), .OnChanged = [Model](bool On) { Model->Snap = On; }, .Label = "Snap to grid"}
```

Both are controlled: they show `Value` and report changes through `OnChanged`. Clicking the label toggles them too.

## Radio buttons

```cpp
RadioGroup {
  .Options       = {"Mono", "Stereo", "Surround"},
  .SelectedIndex = Model->Channels(),
  .OnChanged     = [Model](int Index) { Model->Channels = Index; },
  .Label         = "Channels",
}
```

A `RadioGroup` shows one option per entry, stacked vertically or in a row (`Direction = Axis::Horizontal`). Click an
option, or use the arrow keys once one has focus: the selection and focus move together, wrapping at the ends.
`SelectedIndex = -1` means nothing is chosen yet. `RadioButton` is available on its own for custom arrangements.

## Sliders and knobs

```cpp
Slider {.Bind = &Model->Volume, .Label = "Volume"}
Slider {.Value = Zoom, .Min = 1.0f, .Max = 64.0f, .OnChanged = [this](float Value) { SetZoom(Value); }}
Knob {.Min = -1.0f, .Max = 1.0f, .Default = 0.0f, .Bind = &Track.Pan, .Diameter = 32.0f, .Bipolar = true, .Label = "Pan"}
```

- Bind a `Signal<float>` with `Bind` for the fastest path: dragging repaints the control without rebuilding anything.
  Use `Value` and `OnChanged` to intercept changes instead.
- Sliders follow the pointer. Click or drag to set; the wheel and arrow keys nudge; Shift gives fine steps.
  `Direction = Axis::Vertical` makes a fader.
- Knobs are dragged vertically with the cursor hidden and no screen-edge limit. Shift gives fine control,
  double-click resets to `Default`, and the wheel and arrow keys nudge. `Bipolar` draws the arc from the middle, for
  pan-like parameters.
- Always set `Label` on sliders and knobs that have no text of their own, so screen readers can name them.

## Number fields

```cpp
NumberField {.Bind = &Model->Tempo, .Min = 20.0f, .Max = 300.0f, .Step = 1.0f, .Decimals = 1, .Suffix = " BPM", .Label = "Tempo"}
```

An editable number with step buttons:

- **Typing.** Type a value and press Enter, or move focus away, to apply it. Values are clamped to `[Min, Max]`;
  invalid text reverts to the current value, and so does Escape. Typing the `Suffix` is optional.
- **Stepping.** Up and Down, the mouse wheel while focused, or the step buttons change the value by `Step`, and
  Shift steps ten times as far.
- **Display.** `Decimals` sets the digits shown after the decimal point.

Like sliders, it takes `Bind` or `Value` and `OnChanged`. It defaults to 120 DIPs wide; pass `Width = std::nullopt`
to fill the available width.

## Progress bars

```cpp
ProgressBar {.Value = Done, .Max = Total, .Label = "Export"}
ProgressBar {.Label = "Scanning"} // indeterminate
```

A determinate bar animates to each new `Value` between `Min` and `Max`. Without a value the bar is indeterminate: a
segment slides along the track, or holds still when Windows animations are off. Bars fill the available width, so
inside a `Row` wrap them in `Expanded` or a `SizedBox`. `Height` and `Fill` adjust the look.

## Tabs

```cpp
TabView {
  .Tabs =
    {
      Tab {.Label = "General", .Content = GeneralSettings {}},
      Tab {.Label = "Audio", .Content = AudioSettings {}},
    },
  .Selected = &Model->SettingsTab,
}
```

- **Building.** Only the selected tab's content is built, and each tab's content keeps its own state identity.
- **Switching.** Click a tab, use Left and Right while a tab has focus, or press Ctrl+Tab and Ctrl+Shift+Tab
  anywhere inside.
- **Selection.** Pass a `Signal<int>*` as `Selected` to keep or control the selection; otherwise it starts at
  `InitialIndex`. `OnChanged` reports every change.
- **Sizing.** In a column with bounded height the content fills the space under the tabs, and elsewhere it takes
  its natural size.

## Text fields

```cpp
TextField {.Bind = &Model->Name, .Placeholder = "Your name"}
TextField {
  .OnSubmit    = [this](const std::string& Text) { Search(Text); },
  .Placeholder = "Search",
  .MaxLength   = 64,
  .Leading     = Text {.Content = "🔍"},
}
```

| Field | Meaning |
|---|---|
| `Controller` | An external `TextEditingController` for reading and changing text and selection from code |
| `Bind` | Two-way binding to a `Signal<std::string>` |
| `InitialText`, `OnChanged` | Starting text and a callback after every edit |
| `OnSubmit` | Called on Enter. Without it, Enter passes to the field's ancestors |
| `OnFocusChanged` | Called when the field gains or loses focus, for example to commit an inline rename |
| `OnKey` | Sees keys before editing does; return `true` to consume one |
| `Placeholder`, `TextFont`, `Width`, `Leading` | Appearance |
| `FocusTarget`, `AutoFocus`, `SelectAllOnFocus` | Focus behavior |
| `Enabled`, `MaxLength` | Editing limits (`MaxLength` in characters; 0 for none) |
| `Label` | Screen reader name; the placeholder is used when empty |

Text fields support mouse and keyboard selection, Ctrl word movement, Ctrl+A/C/X/V, Ctrl/Shift+Insert, undo and redo,
and IME composition for languages such as Japanese and Chinese.

### Multi-line text

```cpp
TextArea {.Bind = &Model->Notes, .Placeholder = "Session notes...", .Lines = 6}
```

`TextArea` is a multi-line editor whose height fits `Lines` lines of text:

- **Wrapping and scrolling.** Text wraps to the width and scrolls vertically to keep the caret in view, and also
  with the mouse wheel.
- **Enter and Ctrl+Enter.** Enter inserts a line break and Ctrl+Enter calls `OnSubmit`.
- **Moving the caret.** Up, Down, Home, End, Page Up and Page Down follow the wrapped lines, and Ctrl+Home and
  Ctrl+End jump to the start and end of the text.

It takes the same `Controller`, `Bind` and `OnChanged` options as `TextField`. Setting `Lines` above 1 on a
`TextField` does the same.

## Select

A drop-down choice whose list opens in its own popup window, so it can extend past the window edge:

```cpp
Select {
  .Options       = {"Draft", "Balanced", "High"},
  .SelectedIndex = Model->Quality(),
  .OnChanged     = [Model](int Index) { Model->Quality = Index; },
  .Width         = 160.0f,
  .Label         = "Quality",
}
```

Enter, Space or Down opens it when focused; the list is navigated with the arrow keys.

## Menus

Show a menu from code with `ShowMenu`, or attach one to a widget's right-click with `ContextMenu`:

```cpp
ContextMenu {
  .Items =
    {
      MenuItem {.Label = "Rename", .ShortcutText = "F2", .OnSelect = [this] { Rename(); }},
      MenuItem {.Label = "Mute", .OnSelect = [this] { ToggleMute(); }, .Checked = Muted},
      MenuItem::Divider(),
      MenuItem {.Label = "Delete", .ShortcutText = "Del", .OnSelect = [this] { Delete(); }, .Enabled = CanDelete},
    },
  .Child = TrackHeader,
}
```

`ShowMenu(Overlay, Anchor, Placement, Items, OnClosed, Style)` opens one next to any rectangle, such as a button.
It returns an `OverlayEntry` that keeps the menu open until reset. Get the overlay with
`OverlayController::Of(Context)` and the style with `PopupStyle::Capture(Context)` while building. Menus close on
Escape, on a click elsewhere, or when an item is chosen.

## Tooltips

```cpp
Tooltip {.Message = "Loop the region in the ruler · L", .Child = LoopButton}
```

The message appears after the pointer rests on the child for `Delay` seconds (0.6 by default). It is also given to
screen readers as the child's description.

## Lists

`ListView` builds only the visible rows, so it stays fast with tens of thousands of items. Every row has the same
height:

```cpp
ListView {
  .ItemCount   = static_cast<int>(Files.size()),
  .ItemExtent  = 24.0f,
  .ItemBuilder = [this](int Index) { return FileRow {.File = &Files[Index]}; },
}
```

Give rows `Keyed` identities when they carry state. For a list that must scroll to the selection, pass a
`ScrollController` and call `EnsureVisible(Index * Extent, (Index + 1) * Extent)`.

## Tables

```cpp
Table {
  .Columns =
    {
      TableColumn {.Title = "Name", .Width = 220.0f},
      TableColumn {.Title = "BPM", .Width = 80.0f, .Alignment = TextAlignment::Trailing},
      TableColumn {.Title = "Key"},
    },
  .RowCount    = static_cast<int>(Rows.size()),
  .CellText    = [this](int Row, int Column) { return CellOf(Rows[Row], Column); },
  .OnActivated = [this](int Row) { Open(Rows[Row]); },
  .OnSort      = [this](const TableSort& Sort) { SortRows(Sort); },
}
```

Rows of data in resizable columns:

- **Rows.** Rows are virtualized, so tables with thousands of rows stay fast, and every row is `RowHeight` tall.
  `CellText` supplies each cell's text, or `CellBuilder` supplies any widget instead.
- **Columns.** Each `TableColumn` has a `Title`, a starting `Width`, a `MinWidth` and an `Alignment`. The last
  column fills the remaining width. Drag a header divider to resize the column on its left.
- **Sorting.** Clicking a sortable header sets the sort arrow and calls `OnSort` with the column and direction.
  The table does not reorder data itself: sort your rows, then rebuild, and have `CellText` read them in the new
  order.
- **Selection.** Click to select and double-click to activate. With the table focused, Up, Down, Page Up,
  Page Down, Home and End move the selection, and Enter activates. Pass `Selected` (a `Signal<int>*`) or `Sort`
  (a `Signal<TableSort>*`) to keep that state outside the table.
- **Sizing.** Give the table a bounded height, for example with `Expanded` in a column or a `SizedBox`.

## Split views

```cpp
SplitView {.Direction = Axis::Horizontal, .Ratio = &Model->BrowserSplit, .MinFirst = 200.0f, .First = Browser, .Second = Editor}
```

Two panes separated by a draggable divider. Pass a `Signal<float>*` as `Ratio` to keep or control the split;
otherwise it starts at `InitialRatio` and is kept internally. Dragging relays out only the split view.

## Popups and dialogs

The overlay above every window shows popups and dialogs. `PopupRequest` opens content anchored to a rectangle:

```cpp
_Picker = OverlayController::Of(Context)->ShowPopup({
  .Anchor      = GetGlobalBounds(GetElement()),
  .Placement   = PopupPlacement::Below,
  .Kind        = PopupKind::Menu,
  .Builder     = [Style] { return Style.Wrap(PickerPanel {}); },
  .OnDismissed = [this] { _Picker.Reset(); },
});
```

- `Kind = Menu` takes the keyboard and closes when the user clicks elsewhere, scrolls or switches windows.
  `Kind = Tooltip` is passive.
- Popups are separate native windows that can extend past the window edge. In a host without native popups (such as
  tests) they fall back to the overlay, with the same behavior.
- The popup stays open while you hold the returned `OverlayEntry`. Call `Reset()` on it to close the popup.

`OverlayController::Show(Builder)` adds a full-window layer instead, which `ShowDialog` below builds on.

## Dialogs

`ShowDialog` shows a modal dialog centered over the window, above a barrier that dims and blocks everything behind
it:

```cpp
_Dialog = ShowDialog(*OverlayController::Of(Context), PopupStyle::Capture(Context), Dialog {
  .Title   = "Delete track?",
  .Content = Text {.Content = "The track and its clips will be removed."},
  .Actions =
    {
      DialogAction {.Label = "Cancel", .OnPressed = [this] { _Dialog.Reset(); }},
      DialogAction {.Label = "Delete", .OnPressed = [this] { DeleteTrack(); _Dialog.Reset(); }, .Variant = ButtonVariant::Primary},
    },
  .OnDismiss = [this] { _Dialog.Reset(); },
});
```

- **Closing.** The dialog stays open while you hold the returned `OverlayEntry`. Reset it from the actions and from
  `OnDismiss` to close the dialog.
- **Focus.** The dialog takes focus when it opens and keeps Tab and Shift+Tab inside itself. Focus returns to
  where it was when the dialog closes.
- **Keys.** Escape calls `OnDismiss`. Enter presses the first `Primary` action, unless a focused control inside
  uses Enter itself.
- **Clicking outside.** A click on the dimmed area also calls `OnDismiss`. Pass `DismissOnBarrier = false` to
  `ShowDialog` for dialogs that must be answered.
- **Content.** `Content` can be any widget, including text fields and other controls. `Width` sets the panel width
  (420 DIPs by default).

Capture the overlay and style while building, as above, and call `ShowDialog` from an event handler.

## Menu bars

```cpp
MenuBar {
  .Menus =
    {
      Menu {.Label = "File", .Items = {MenuItem {.Label = "Open...", .ShortcutText = "Ctrl+O", .OnSelect = [this] { Open(); }}}},
      Menu {.Label = "Edit", .Items = {MenuItem {.Label = "Undo", .ShortcutText = "Ctrl+Z", .OnSelect = [this] { Undo(); }}}},
    },
}
```

A row of menus that open as popup menus. Click a menu, or press Enter, Space or Down on a focused one. While a menu
is open, hovering another menu or pressing Left and Right switches to it, and Escape closes it. A menu bar fits well
inside a custom title bar (see [Windows](Windows.md#custom-title-bars)). `ShortcutText` is only shown; bind the keys
themselves with `Window::SetShortcuts`.

## Tree views

```cpp
TreeView {
  .ChildrenOf  = [this](const std::string& Parent) { return Library.ChildrenOf(Parent); },
  .Selected    = &Model->SelectedPath,
  .OnActivated = [this](const std::string& Id) { Open(Id); },
  .Label       = "Library",
}
```

An expandable hierarchy:

- **Data.** `ChildrenOf` returns the `TreeItem`s (`Id`, `Label`, `HasChildren`) under an item, or the top-level
  items for an empty id. It is only called for expanded items, and rows are virtualized, so very large trees stay
  fast. Signals read inside `ChildrenOf` are tracked, so the tree updates when its data changes.
- **Mouse.** Click a row to select it, and double-click to activate it, which also toggles a folder. The arrow
  toggles expansion.
- **Keyboard.** Up and Down move the selection, and Home and End jump to the ends. Right expands an item, or moves
  to its first child; Left collapses it, or moves to its parent. Enter activates.
- **State.** Pass `Selected` (a `Signal<std::string>*`) and `Expanded` (a `Signal<std::set<std::string>>*`) to keep
  or control the selection and the expanded items. Otherwise the tree keeps them itself.
- **Appearance.** `RowContent` replaces the text of each row, for icons or extra columns. `RowHeight` and `Indent`
  set the spacing.
- **Sizing.** A tree view scrolls its rows, so give it a bounded height, for example with `Expanded` in a column
  or a `SizedBox`.

## Window controls

For windows with a custom title bar, `WindowDragArea` marks where the window can be dragged, and `WindowControls`
draws the minimize, maximize and close buttons. Both are covered in [Windows](Windows.md#custom-title-bars).
