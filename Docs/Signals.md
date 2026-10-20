# Signals

State in XenUI lives in signals. A signal holds a value and remembers who read it; when the value changes, exactly
those readers update. You never tell the UI to refresh.

```cpp
Signal<float> Volume = 0.8f;

Volume = 0.5f;               // write, and notify readers if the value changed
float Now = Volume();        // read and subscribe (same as Volume.Get())
float Quiet = Volume.Peek(); // read without subscribing
```

## Who subscribes

A read subscribes the **current tracking scope**. These run in one:

- a component's `Build`, which then rebuilds when the signal changes;
- the `OnPaint` callback of a `CustomPaint` and the `OnDraw` callback of a `LiveView`, which then repaint without
  any rebuild, and the same goes for bound sliders, knobs, text fields, scroll views and split views;
- a `Computed` while computing its value;
- an `Effect` callback.

Reads anywhere else, such as in a button's `OnPressed` or on another thread, subscribe nothing. Dependencies are
recorded from scratch on every run, so a component that reads a signal only on some branches subscribes only when
it takes those branches.

![Signals and rebuilds](Images/signals.svg)

## Get versus Peek

Use `Get()` (or `operator()`) when the value should drive what you build or draw. Use `Peek()` when you only need the
current value at that moment, which is typical in event handlers:

```cpp
Button {.Label = "Next", .OnPressed = [this] { _Page = _Page.Peek() + 1; }}
```

A `Peek` in `Build` is occasionally right too: for example, to read an initial value you don't want to rebuild on.

## Writing

| Call | Effect |
|---|---|
| `Value = X` or `Value.Set(X)` | Stores `X` and notifies readers. When `T` supports `==`, writing an equal value does nothing. |
| `Value.Modify([](T& V) { ... })` | Changes the value in place and always notifies. Use it for containers. |

```cpp
Signal<std::vector<std::string>> Names;
Names.Modify([](std::vector<std::string>& List) { List.push_back("Ada"); });
```

Signals are movable but not copyable, and belong to whoever owns the state: a component's `State`, or an
application model passed down by pointer.

## Bound controls

Controls that edit a value accept a signal pointer as `Bind`. A bound control reads and writes the signal directly,
and while the value is dragged it **repaints without rebuilding** anything:

```cpp
Slider {.Bind = &Model->Volume, .Label = "Volume"}
Knob {.Min = -1.0f, .Max = 1.0f, .Bind = &Track.Pan, .Bipolar = true, .Label = "Pan"}
TextField {.Bind = &Model->Name}
```

Pass `Value` and `OnChanged` instead when you want to intercept or validate every change.

## Computed

`Computed<T>` derives a value from other signals. It is evaluated lazily, recomputed only after a dependency
changes, and can be read and subscribed like a signal:

```cpp
Signal<std::string> First = std::string("Ada");
Signal<std::string> Last  = std::string("Lovelace");
Computed<std::string> Full([&] { return First() + " " + Last(); });

Text {.Content = Full()}
```

## Effect

`Effect` runs a callback now and again whenever a signal it read changes. Use it for side effects outside the
widget tree, such as saving settings or pushing a value to an audio engine:

```cpp
Effect SaveVolume([this] { Settings.Write("volume", Model.Volume()); });
```

The effect stops when the object is destroyed, so keep it as a member of whatever owns the signals. Re-runs are
deferred until the current notification finishes, so an effect always sees a consistent state.

## Batch and Untracked

```cpp
Batch([&] {
    Model.Start = 0.0;
    Model.End   = 16.0;
}); // effects that read both run once, after the batch

Untracked([&] { Viewport = Model.Scroll.GetViewportExtent(); }); // reads here subscribe nothing
```

`Batch` defers effects until the callback returns. `Untracked` lets a tracked scope read a signal without depending
on it.

## Threads

Signals belong to the UI thread. To move data from another thread, such as audio meters, use the lock-free helpers
in [Live rendering](LiveRendering.md#threads), and write signals only on the UI thread.
