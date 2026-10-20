# Windows

## App and Window

```cpp
int main() {
    App MyApp;

    Window MainWindow({.Title = "Editor", .ClientSize = {1280.0f, 800.0f}});
    MainWindow.SetOnClosed([&MyApp] { MyApp.Quit(); });
    MainWindow.SetContent(EditorRoot {});
    MainWindow.Show();

    return MyApp.Run();
}
```

Create exactly one `App`, before any window, and keep it alive until the end. `App::Run` runs the message loop until
`Quit` is called and returns its exit code.

`WindowDesc` configures a window when it is created:

| Field | Default | Meaning |
|---|---|---|
| `Title` | `"XenUI"` | Title bar and taskbar text |
| `ClientSize` | 1280 × 720 | Initial client area in DIPs; the window opens centered on its monitor |
| `Background` | dark gray | Shown behind the content and while resizing |
| `Resizable` | `true` | Whether the user can resize or maximize it |
| `DarkTitleBar` | `true` | Whether the system title bar uses dark mode |
| `CustomTitleBar` | `false` | Lets the content draw the title bar (see below) |

After creation, `Window` can `Show`, `Maximize`, `Activate`, `Close`, `SetTitle`, `SetClientSize`, `SetBackground`
and `SetDarkTitleBar`, and report `GetClientSize`, `GetDpiScale` and `GetNativeHandle` (the `HWND`).

### DPI

Windows are per-monitor DPI aware. Everything in XenUI is measured in device-independent pixels, and text and
graphics are rendered at the monitor's real resolution. Moving a window to a monitor with different scaling
re-lays it out at the new scale.

### Rendering only when needed

A window renders a frame only when something changed: a rebuild, a repaint, an animation or a `LiveView`. While
nothing changes, the run loop sleeps. `Window::SetShowDamageRegions(true)` briefly tints every repainted area, which
helps find widgets that repaint more than they should.

## Custom title bars

With `CustomTitleBar = true` the client area covers the whole window inside its resize borders, and your content
draws the title bar:

```cpp
struct TitleBar {
    std::string Title;

    Node Build(BuildContext& Context) const {
        const Theme& Style = Theme::Of(Context);
        return Container {
          .Height     = 32.0f,
          .Background = Style.Chrome,
          .Child =
            WindowDragArea {
              .Child = Row {
                .Children = {
                  Padding {.Insets = {.Left = 12.0f}, .Child = Text {.Content = Title, .Color = Style.TextMuted}},
                  Spacer {},
                  WindowControls {},
                },
              },
            },
        };
    }
};
```

- **`WindowDragArea`** marks the area the user can drag the window by. Double-clicking it maximizes, and
  right-clicking opens the system menu. Interactive widgets placed on top of it, such as a menu button or a search
  field, still receive the pointer normally.
- **`WindowControls`** draws the minimize, maximize and close buttons. They are real caption buttons to Windows, so
  hovering the maximize button shows Snap Layouts, and screen readers can operate them.
- **`WindowChrome::Of(Context)`** gives the title bar state, such as whether the window is maximized or active. Use it
  to restyle your own title bar, as `WindowControls` does.

## Multiple windows

Create more `Window` objects. Each runs its own widget tree on the same UI thread, and they can share state through
signals:

```cpp
std::optional<Window> Inspector;

void OpenInspector(AppModel& Model) {
    if (Inspector && Inspector->IsOpen()) {
        Inspector->Activate();
        return;
    }
    Inspector.emplace(WindowDesc {.Title = "Inspector", .ClientSize = {420.0f, 600.0f}});
    Inspector->SetContent(InspectorPanel {&Model});
    Inspector->Show();
}
```

A widget in one window that reads a signal written in another updates like any other subscriber.

## Popup windows

Menus, select lists and tooltips open as separate, non-activating native windows, so they can extend past the edge
of their owner like real Windows menus. See [Controls](Controls.md#popups-and-dialogs) to open your own.

![Overlays and popups](Images/overlays.svg)

## Embedding native windows

`NativeHost` reserves space in the layout for a native child window, such as a VST3 plugin editor or a Win32
control:

```cpp
NativeHost {
  .OnAttach = [this](NativeHandle Container) { _Editor->Attach(Container); },
  .OnDetach = [this](NativeHandle Container) { (void)Container; _Editor->Detach(); },
  .OnResize = [this](int Width, int Height) { _Editor->Resize(Width, Height); },
  .Width    = 640.0f,
  .Height   = 400.0f,
}
```

- **`OnAttach`** receives the container `HWND` to create your native content in. **`OnResize`** reports its size in
  physical pixels.
- **The container follows the layout.** It moves with scrolling and clipping, and hides when its widget is not
  visible.
- **Native windows always draw above XenUI content in the same window,** because that is how Windows composes child
  windows. Popups are separate windows, so they still appear above them.
