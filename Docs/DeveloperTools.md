# Developer tools

XenUI has two built-in tools for understanding a running application: a widget inspector and a frame stats overlay.
Both are switched on by wrapping your root widget in `DeveloperTools`:

```cpp
MainWindow.SetContent(DeveloperTools {.Child = MyRoot {}});
```

| Key | Tool |
|---|---|
| F12 | Opens or closes the widget inspector in its own window |
| Shift+F12 | Shows or hides the frame stats overlay in the window's top-right corner |

The shortcuts work wherever focus is, even inside a text field. To control the tools from your own UI instead, such
as a Help menu, pass `Signal<bool>*` switches as `ShowInspector` and `ShowFrameStats`. To disable the keys, set
`Shortcuts = false`. Leave `DeveloperTools` out of release builds if you don't want users to find them.

Place it inside your theme's `Provide<Theme>`, so the inspector window uses the same theme as your app.

## The widget inspector

The inspector shows the window's element tree beside the details of the selected element:

- **Widget.** Its type (such as `Button` or `Provide<Theme>`), its key, its depth and its number of children.
  Components (stateless and stateful widgets, `Provide`) are shown in the accent color; render widgets in the
  normal text color.
- **Render object.** For render widgets, the render object type, its window position and size, the constraints
  its parent gave it, whether it is a repaint boundary, and whether it is waiting for layout or paint.
- **Accessibility.** The role and name the element reports to screen readers, if it describes itself.
- **Highlight.** The selected element is outlined in your window, with its type and size.

Click **Pick element**, then click anything in your window, to select the element under the pointer. While picking,
the element under the pointer is outlined as you move; Escape cancels. The tree expands to the picked element and
scrolls to it.

The snapshot refreshes twice a second while the inspector is open, and **Refresh** updates it immediately.
Elements are identified by their path of child indices, so the selection survives refreshes as long as the tree
keeps its shape.

### Using the inspector in code

`WidgetInspector` is the snapshot behind the inspector window. It works on any `WidgetHost`, including in tests:

```cpp
WidgetInspector Inspector(Host);
Inspector.Refresh();
for (const InspectorNode& Node : Inspector.GetNodes()) {
    // Node.Name, Node.Kind, Node.RenderName, Node.Bounds, Node.Constraints, Node.Semantics, ...
}
const std::string Id = Inspector.HitTest({120.0f, 40.0f}); // the deepest render widget at a point
```

## The frame stats overlay

The overlay shows what the last second of rendering cost:

- **Frame rate and work time.** Frames rendered in the last second, and the average and worst time spent
  working on a frame. That's the frame's total time, excluding the wait for the display.
- **Stage times.** Average milliseconds spent building elements, laying out, painting display lists, drawing the
  damaged region, presenting and running the window's frame callback.
- **The last frame.** How many elements were rebuilt, render objects laid out and repaint boundaries painted, and
  how much of the window was redrawn.
- **A graph of recent frames.** Green is under half a 60 Hz frame, yellow is under a full frame, red is over
  budget. The line marks 16.7 ms.

An idle XenUI window renders no frames at all, so the overlay says "idle" when nothing has changed for a second.
The overlay refreshes four times a second, so it adds a few small frames of its own.

Things to look for:

- **Rebuild counts that stay high** while you hover or move a slider mean signals are read too high in the tree.
  Read them in the smallest widget that needs them, or in a `CustomPaint`.
- **Large redrawn areas** for small changes mean a missing `RepaintBoundary` around something that changes often.
  `Window::SetShowDamageRegions(true)` flashes each redrawn area, which shows exactly what is repainting.
- **Long paint or draw times** often come from complex paths or very large text layouts drawn every frame.

### Reading the numbers in code

Every `WidgetHost` keeps its recent frames in a `FrameStatistics`, and `GetLastUpdate()` returns the timings and
counts of the last update:

```cpp
const FrameStatistics& Stats = Host.GetFrameStatistics();
for (const FrameSample& Frame : Stats.GetRecent(60)) {
    // Frame.BuildMs, Frame.LayoutMs, Frame.PaintMs, Frame.DrawMs, Frame.Rebuilt, Frame.Damage, ...
}
```

Windows record complete frames, including drawing. A host driven on its own, such as in a test, records each
`Update`. `FrameStatsPanel {&Host.GetFrameStatistics()}` is the overlay's panel as a widget, if you want to show it somewhere else.
