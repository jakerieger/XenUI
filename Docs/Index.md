# XenUI guide

XenUI is a declarative GUI framework for Windows: you describe the interface as a tree of widget structs, keep state
in signals, and the framework rebuilds, lays out and repaints only what changed.

Start with **Getting started**, then read **Widgets** and **Signals**. Together they explain almost everything
you'll write day to day. The other pages can be read in any order.

| Page | What it covers |
|---|---|
| [Getting started](GettingStarted.md) | Requirements, building, adding XenUI to a project, a first window |
| [Widgets](Widgets.md) | Widget kinds, stateful components, keys, providing values, custom render objects |
| [Signals](Signals.md) | `Signal`, `Computed`, `Effect`, tracking, `Peek`, `Batch` and `Untracked` |
| [Layout](Layout.md) | Constraints, rows and columns, flex, stacks, sizing, scrolling and custom painting |
| [Input](Input.md) | Pointer events, gestures, focus, keyboard shortcuts, text input, clipboard, drag and drop |
| [Controls](Controls.md) | Buttons, toggles, sliders, knobs, text fields, selects, menus, tooltips, lists, split views |
| [Theming](Theming.md) | The `Theme` struct, custom themes, text styles, switching themes at run time |
| [Animation](Animation.md) | `Animated<T>`, easing, timers and frame callbacks |
| [Windows](Windows.md) | `App` and `Window`, custom title bars, popups, multiple windows, embedding native windows |
| [Live rendering](LiveRendering.md) | `LiveView`, `LiveCanvas`, `LiveTexture` and feeding them from other threads |
| [Accessibility](Accessibility.md) | How widgets reach screen readers, and describing your own widgets |
| [Architecture](Architecture.md) | How a frame is built, laid out, painted and composited |

The API reference is generated from the headers with Doxygen. The headers themselves are documented and are the
final word on every type: start from `Source/XenUI/XenUI.hpp`, which includes everything.

## Conventions used in XenUI

- Everything lives in the `XenUI` namespace. The examples in this guide assume `using namespace XenUI;`.
- Sizes and positions are in **device-independent pixels** (DIPs): 1 DIP is one physical pixel at 100% scaling.
- All UI work happens on the thread that created the `App`. Only the live rendering helpers are meant to be used
  from other threads.
- Widgets are aggregates. Fields must appear in declaration order inside `{ ... }`, and any field you leave out
  keeps its default.
