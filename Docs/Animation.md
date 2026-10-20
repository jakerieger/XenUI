# Animation

## Animated values

`Animated<T>` is a value that moves smoothly to new targets. Reading it is tracked like a signal, so whatever reads it
updates on every frame of the animation and stops costing anything when the animation ends.

```cpp
struct Expander {
    Node Content;

    class State : public ComponentState<Expander> {
    protected:
        Node Build(BuildContext& Context) override {
            (void)Context;
            return MouseRegion {
              .OnEnter = [this](const PointerEvent&) { _Height.AnimateTo(160.0f, 0.25, Easing::EaseOut); },
              .OnExit  = [this](const PointerEvent&) { _Height.AnimateTo(40.0f, 0.2); },
              .Child   = SizedBox {.Height = _Height(), .Child = GetWidget().Content},
            };
        }

    private:
        Animated<float> _Height {40.0f};
    };
};
```

| Member | Meaning |
|---|---|
| `AnimateTo(Target, Seconds, Curve)` | Animates from the current value. Asking for the target already in progress does not restart it |
| `Set(Value)` | Jumps to `Value` and stops any animation |
| `Get()`, `operator()`, `Peek()` | Read the current value, tracked or not |
| `GetTarget()`, `IsAnimating()` | Where it is heading, and whether it is moving |

`T` can be `float`, `double`, `Vec2`, `Size`, `Rect`, `Color`, or any type with an `Interpolate(From, To, Amount)`
overload in the `XenUI` namespace. Colors interpolate channel by channel, including alpha.

Easing curves are `Linear`, `EaseIn`, `EaseOut` (the default), `EaseInOut` and `EaseOutBack`, which overshoots
slightly before settling.

### Where to read it

- **Read it in `Build`**, as above, and the component rebuilds each frame. Fine for small components.
- **Read it inside a `CustomPaint` callback** and only that widget repaints, with no rebuild. Prefer this for large
  areas or many simultaneous animations.

### Respecting the system setting

When **Animation effects** is turned off in Windows settings, animations jump straight to their targets. The
built-in controls rely on this, so they follow the setting with no extra work. To force animations on or off, for
example in tests, call `AnimationTicker::SetAnimationsEnabled(true)` or `false`, and pass `std::nullopt` to follow
the system again.

## Timers

```cpp
Timer _Autosave = Timer::Every(30.0, [this] { Save(); });
Timer _Hint     = Timer::After(0.6, [this] { ShowHint(); });
```

Timers run on the UI thread. A `Timer` is an owning handle: the callback is cancelled when the handle is destroyed,
reassigned or `Cancel()`ed, so keeping it as a member of your state ties it to the widget's lifetime.

## Frame callbacks

For animation driven by your own clock, such as a game loop or a playhead following audio,
`Window::SetFrameCallback` runs at the start of every frame:

```cpp
MainWindow.SetFrameCallback([&](const FrameInfo& Frame) {
    Model.Playhead = Engine.GetPosition();
    if (Engine.IsPlaying()) { MainWindow.RequestFrame(); }
});
```

`FrameInfo` carries `Time` (seconds since the app started), `DeltaTime` and `Index`. Call `RequestFrame()` from the
callback to keep receiving frames. When no frames are requested the window renders nothing and uses no CPU.

`LiveView {.Continuous = true}` redraws every frame on its own and is usually simpler for meters and scopes (see
[Live rendering](LiveRendering.md)).
