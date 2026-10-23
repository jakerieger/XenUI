# Accessibility

XenUI windows implement UI Automation, the Windows accessibility API that screen readers such as Narrator and NVDA,
voice control, and UI test tools use. Built-in controls describe themselves; your own widgets need a line or two.

## What you get for free

| Widget | Announced as | Screen reader actions |
|---|---|---|
| `Button` | button | press |
| `Checkbox` | check box | toggle |
| `RadioGroup`, `RadioButton` | group of radio buttons | select |
| `Switch` | toggle switch | toggle |
| `Slider`, `Knob` | slider | set value, step with arrows |
| `TextField` | edit | read and set text |
| `NumberField` | spinner | read and set value, step |
| `ProgressBar` | progress bar | read value |
| `TabView` | tab list with tab items | select |
| `TreeView` | tree with tree items | select, expand and collapse, activate |
| `Table` | table with column headers and data items | sort by a column, select and activate rows |
| `TextArea` | edit | read and set text |
| `MenuBar` | menu bar with menu items | expand and collapse |
| `Dialog` | dialog | none; its controls are operated as usual |
| `Select` | combo box | read value, expand and collapse |
| Menus, menu items | menu, menu item | invoke, read checked state and shortcut |
| `WindowControls` | buttons | minimize, maximize or restore, close |
| `Text` | text | read |

Tooltips become the description of the control they wrap. Focus changes, value changes, toggles, expansion and
structure changes are reported to Windows as they happen.

Nothing runs until an accessibility client connects to a window, so applications pay no cost when no screen reader is
in use.

## Names

A control is announced by its name, so give unlabeled controls one:

```cpp
Knob {.Bind = &Track.Pan, .Bipolar = true, .Label = "Pan"}
Slider {.Bind = &Model->Volume, .Label = "Master volume"}
TextField {.Bind = &Query, .Placeholder = "Search", .Label = "Search samples"}
Select {.Options = Modes, .SelectedIndex = Mode, .OnChanged = SetMode, .Label = "Snap"}
Button {.Label = "■", .OnPressed = Stop, .AccessibleLabel = "Stop"}
```

Buttons, checkboxes and switches are named after their visible text unless told otherwise. A text field falls back to
its placeholder.

## Describing your own widgets

Wrap a custom widget in `Semantics` to tell assistive technology what it is and what can be done with it. Each
action is offered only when its callback is set:

```cpp
Semantics {
  .Role     = SemanticsRole::Button,
  .Label    = "Mute",
  .Toggled  = Track.Muted(),
  .OnToggle = [this] { ToggleMute(); },
  .Child    = MuteChip,
}

Semantics {
  .Role       = SemanticsRole::Slider,
  .Label      = "Cutoff",
  .Range      = SemanticsRange {.Value = Cutoff.Peek(), .Min = 20.0f, .Max = 20000.0f, .Step = 10.0f},
  .ReadRange  = [this] { return Cutoff.Peek(); },
  .OnSetRange = [this](float Value) { Cutoff = Value; },
  .Child      = CutoffDisplay,
}
```

| Field | Meaning |
|---|---|
| `Role` | What it is: `Button`, `CheckBox`, `Switch`, `RadioButton`, `Slider`, `Spinner`, `ProgressBar`, `Edit`, `ComboBox`, `List`, `ListItem`, `TabList`, `TabItem`, `Tree`, `TreeItem`, `Table`, `TableRow`, `ColumnHeader`, `Dialog`, `MenuBar`, `Menu`, `MenuItem`, `Text`, `Image`, `Group`, `Pane`, `ToolBar` |
| `Label`, `Description`, `Identifier` | Name, extra help, and a stable id for test automation |
| `Value`, `Range`, `Toggled`, `Expanded`, `Selected` | State, each optional |
| `Enabled`, `ReadOnly` | Whether it can be operated |
| `OnInvoke`, `OnToggle`, `OnSetRange`, `OnSetValue`, `OnExpand`, `OnSelect` | The actions it supports |
| `ReadRange` | Reads the current range value, for values that change without a rebuild, such as a bound signal |

Controls such as buttons, sliders, edits and list items are **leaves**. Their children are not exposed separately,
and when they have no `Label` they are named after the text inside them. Containers such as `Group`, `List` and
`Menu` expose their children.

To make a custom control reachable with the keyboard, put a `Focus` inside or around its `Semantics`. The pair is
then announced as focused when the user tabs to it.

Two more widgets help:

- `ExcludeSemantics {.Child = ...}` hides decoration, such as an arrow glyph, that would only add noise.
- `SemanticsHint {.Hint = "...", .Child = ...}` gives the first control inside a description. `Tooltip` uses it.

## Render objects

A custom render object can describe itself instead, by overriding `RenderBox::DescribeSemantics`. That is how
`Slider` and `TextField` report live values without rebuilding.

## Testing

`SemanticsTree` builds the accessibility tree of a `WidgetHost` without a window, so you can check descriptions and
run actions in unit tests:

```cpp
SemanticsTree Tree;
Tree.Rebuild(Host.GetView(), &Host.GetInput().GetFocusManager());
const SemanticsNode* Play = Tree.FindByLabel("Play");
Tree.Invoke(Play->Id);
```

`Tests/AccessibilityTests.cpp` has complete examples. To check a running application, use **Accessibility Insights
for Windows** or **Inspect.exe** from the Windows SDK, or turn on Narrator with Ctrl+Win+Enter.
