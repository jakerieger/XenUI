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

`OverlayController::Show(Builder)` adds a full-window layer instead. Combine it with `ModalBarrier`, which dims the
content below and reports presses on it, to build a dialog:

```cpp
_Dialog = Overlay->Show([this] {
    return Stack {.Fit = StackFit::Expand, .Children = {
      ModalBarrier {.Tint = Color::FromHex(0x000000, 0.4f), .OnDismiss = [this] { _Dialog.Reset(); }},
      Center {.Child = ConfirmPanel {.OnClose = [this] { _Dialog.Reset(); }}},
    }};
});
```

## Window controls

For windows with a custom title bar, `WindowDragArea` marks where the window can be dragged, and `WindowControls`
draws the minimize, maximize and close buttons. Both are covered in [Windows](Windows.md#custom-title-bars).
