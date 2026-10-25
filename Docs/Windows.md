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

| Field            | Default    | Meaning                                                                          |
|------------------|------------|----------------------------------------------------------------------------------|
| `Title`          | `"XenUI"`  | Title bar and taskbar text                                                       |
| `ClientSize`     | 1280 × 720 | Initial client area in DIPs; the window opens centered on its monitor            |
| `Background`     | dark gray  | Shown behind the content and while resizing                                      |
| `Resizable`      | `true`     | Whether the user can resize or maximize it                                       |
| `DarkTitleBar`   | `true`     | Whether the system title bar uses dark mode                                      |
| `CustomTitleBar` | `false`    | Lets the content draw the title bar (see below)                                  |
| `Icon`           | `0`        | Resource ID of the window's icon; 0 uses the executable's first icon (see below) |

After creation, `Window` can `Show`, `Maximize`, `Activate`, `Close`, `Destroy`, `SetTitle`, `SetClientSize`,
`SetBackground`, `SetDarkTitleBar`, `SetIcon` and `SetIconFile`, and report `GetClientSize`, `GetDpiScale` and
`GetNativeHandle` (the `HWND`).

### Icons

A window shows its icon in the title bar, the taskbar and Alt+Tab. By default it uses the executable's first icon
resource, the one Explorer shows for the file, so an application that has an icon in its `.rc` file needs nothing
else. To pick another icon, pass its resource ID as `WindowDesc::Icon` or call `SetIcon`; to load one from an `.ico`
file, call `SetIconFile`. Icons are reloaded at the right size when the window moves to a monitor with different
scaling.

### Closing

`Close` asks the window to close, exactly as the close button, Alt+F4 and the taskbar do. Each of them first calls the
callback set with `SetOnCloseRequested`; return `false` from it to keep the window open. Once the window is gone,
`SetOnClosed`'s callback runs. `Destroy` closes the window without asking, for when your callback has already
decided:

```cpp
MainWindow.SetOnCloseRequested([&] {
    if (!Document.IsModified()) { return true; }
    // Ask first. ConfirmDiscard shows a dialog and calls its callback if the user chose "Discard".
    ConfirmDiscard([&MainWindow] { MainWindow.Destroy(); });
    return false;
});
```

### DPI

Windows are per-monitor DPI aware. Everything in XenUI is measured in device-independent pixels, and text and
graphics are rendered at the monitor's real resolution. Moving a window to a monitor with different scaling
re-lays it out at the new scale.

### Rendering only when needed

A window renders a frame only when something changed: a rebuild, a repaint, an animation or a `LiveView`. While
nothing changes, the run loop sleeps. `Window::SetShowDamageRegions(true)` briefly tints every repainted area, which
helps find widgets that repaint more than they should. [Developer tools](DeveloperTools.md) adds a frame stats overlay
and a widget inspector.

## Custom title bars

With `CustomTitleBar = true` the client area covers the whole window inside its resize borders, and your content
draws the title bar. `TitleBar` is a complete one:

```cpp
Column {
  .CrossAlignment = CrossAxisAlignment::Stretch,
  .Children =
    {
      TitleBar {
        .Title   = "Editor",
        .Menus   = {Menu {.Label = "File", .Items = {...}}, Menu {.Label = "Edit", .Items = {...}}},
        .Actions = {Select {.Options = {"System", "Dark", "Light"}, .Width = 96.0f, .Label = "Theme"}},
      },
      Expanded {.Child = EditorBody {}},
    },
}
```

- **Menus** go on the left, as a `MenuBar`. With menus, the title is centered on the bar; without them, it sits on
  the left.
- **Actions** go on the right, before the minimize, maximize and close buttons. Use them for a theme selector, a
  search field or an account button.
- The whole bar drags the window, and the title dims while the window is inactive, like a system title bar.
- `Height` (34 DIPs), `MenuWidth` (the width of the open menus) and `Background` (the theme's chrome color) adjust
  it.

### Building your own

For a different layout, build the title bar from the same parts `TitleBar` uses:

```cpp
struct MyTitleBar {
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

## Work from other threads

Widgets, signals and windows belong to the UI thread. To update them from a worker thread, such as when a background
job makes progress, hand the update to `App::Post`:

```cpp
Job.OnProgress = [this](int Done) {
    // On the worker thread: copy what the UI needs, then post.
    App::Post([this, Done] { _Done = Done; });
};
```

Posted work runs on the UI thread soon after the call, in the order it was posted, and also while a modal loop such as
a file dialog is open. `App::Post` is static and safe to call from any thread; it returns `false`, dropping the work,
when no `App` exists.

Posted work must not outlive what it captures. Stopping a job in `Dispose` isn't enough on its own, because updates it
posted before stopping may still be queued. A component can guard them with a token that it drops in `Dispose`:

```cpp
std::shared_ptr<bool> _Alive = std::make_shared<bool>(true);

void PostToUi(std::function<void()> Work) {
    App::Post([Alive = std::weak_ptr(_Alive), Work = std::move(Work)] {
        if (Alive.lock()) { Work(); }
    });
}

void Dispose() override {
    _Alive.reset();
    _Job.Stop();
}
```

For data that changes faster than the screen, such as meters or a playhead, use the lock-free queues in
[Live rendering](LiveRendering.md) instead.

## File dialogs

`FileDialog` opens the system's open, save and folder dialogs. Each call blocks until the user closes the dialog and
returns nothing if they cancelled:

```cpp
const std::optional<std::filesystem::path> Folder = FileDialog::PickFolder({.Title = "Export to"});

const std::optional<std::filesystem::path> Sample = FileDialog::OpenFile({
  .Filters = {{"Audio files", "*.wav;*.flac;*.mp3"}, {"All files", "*.*"}},
});

const std::optional<std::filesystem::path> Project = FileDialog::SaveFile({
  .FileName         = "Untitled",
  .Filters          = {{"Xen project", "*.xen"}},
  .DefaultExtension = "xen",
});
```

`OpenFiles` lets the user choose several files. `FileDialogOptions` also sets the `Title` and the `InitialFolder`.
The dialog belongs to `Owner`, or to the active window when `Owner` is null, which is the right window when the dialog
is opened from a button. That window is disabled while the dialog is open, but every window keeps painting.

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
