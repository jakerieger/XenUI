# Theming

The built-in controls read their colors and metrics from a `Theme` value provided above them. With none provided
they use `Theme::Dark()`.

## The Theme struct

`Theme` (in `Widgets/Theme.hpp`) is a plain struct of design tokens:

| Group | Tokens |
|---|---|
| Surfaces | `Background`, `Surface`, `SurfaceRaised`, `Chrome` (title bars and sidebars) |
| Control states | `Hover`, `Pressed`, `Border` |
| Text | `Text`, `TextMuted`, `TextDisabled` |
| Accent | `Accent`, `AccentText` (text drawn on accent fills, such as primary buttons) |
| Status | `Danger`, `Warning`, `Success` |
| Other | `Track` (slider and field backgrounds), `Overlay`, `ScrollbarThumb`, `ScrollbarTrack` |
| Fonts and metrics | `BodyFont`, `SmallFont`, `CornerRadius`, `ControlHeight`, `IsDark` |

`Theme::Dark()` and `Theme::Light()` are the built-in palettes. `Theme::For(Appearance::Light)` picks one, and
`Theme::FromSystem()` follows the Windows light or dark app setting.

## Making a custom theme

Start from a built-in palette and change what you need:

```cpp
Theme MakeTheme() {
    Theme Result         = Theme::Dark();
    Result.Accent        = Color::FromHex(0xFF7A45);
    Result.AccentText    = Color::FromHex(0x1A0E08);
    Result.Background    = Color::FromHex(0x16161A);
    Result.Chrome        = Color::FromHex(0x101013);
    Result.CornerRadius  = 8.0f;
    Result.ControlHeight = 30.0f;
    return Result;
}
```

A designated initializer works too, with the fields in declaration order:
`Theme {.Accent = Color::FromHex(0xFF7A45), .CornerRadius = 8.0f}`. Fields you leave out keep the dark defaults.

Do not derive your own type from `Theme`: controls look it up by exact type, so `Provide<MyTheme>` would be invisible
to them.

## Providing it

Wrap your root, or any subtree, in `Provide<Theme>`:

```cpp
const Theme Palette = MakeTheme();
MainWindow.SetBackground(Palette.Background);
MainWindow.SetContent(Provide<Theme> {
  .Value = Palette,
  .Child = Provide<TextStyle> {.Value = {.Font = Palette.BodyFont, .Color = Palette.Text}, .Child = Shell {}},
});
```

Three things to know:

- **Plain `Text` uses `TextStyle`, not the theme.** Provide a `TextStyle` built from the theme, as above, or set
  `Color` on individual `Text` widgets.
- **The window background shows behind your content** and while the window resizes. Set it with
  `WindowDesc::Background` or `Window::SetBackground`.
- **Popups match automatically.** Menus, select lists and tooltips capture the theme where they are opened.

## Using the theme in your own widgets

```cpp
Node Build(BuildContext& Context) const {
    const Theme& Style = Theme::Of(Context);
    return Container {
      .Padding      = EdgeInsets::All(12.0f),
      .Background   = Style.Surface,
      .CornerRadius = Style.CornerRadius,
      .BorderColor  = Style.Border,
      .Child        = Text {.Content = Title, .Color = Style.TextMuted},
    };
}
```

`Theme::Of` returns the nearest provided theme, or the dark default, and makes the widget rebuild when the theme
changes.

For values `Theme` does not have, such as track colors in a DAW, provide your own struct next to it and read it with
`Context.Find<T>()` (see [Widgets](Widgets.md#providing-values-to-descendants)).

## Switching at run time

Keep the choice in a signal and build the `Provide` in a component that reads it. When the signal changes, the
component rebuilds and everything below restyles:

```cpp
struct ThemedRoot {
    Signal<int>* Mode = nullptr; // 0 follows Windows, 1 dark, 2 light

    Node Build(BuildContext& Context) const {
        (void)Context;
        const int Choice = Mode->Get();
        const Theme Palette = Choice == 0 ? Theme::FromSystem() : Theme::For(Choice == 1 ? Appearance::Dark : Appearance::Light);
        return Provide<Theme> {.Value = Palette, .Child = Shell {}};
    }
};
```

`Theme::FromSystem()` is tracked, so a root using it also restyles when the user changes the Windows setting.
`Window::SetDarkTitleBar` switches the system title bar to match. `ShowcaseRoot` in `Showcase/main.cpp` does all of
this.
