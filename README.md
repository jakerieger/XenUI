<p align="middle">
    <img src="Banner.png"/>
</p>

**Xen UI** is a C++ UI framework for Windows. It uses Direct2D under the hood to provide GPU accelerated
rendering. Xen follows a very similar design ethos to Flutter without the requirement of a second
programming language.

## Example

Here's a snippet of the `XenApp::BuildUI()` method used to create the UI of the application, pulled
from the [HelloXen](Demos/HelloXen/main.cpp) demo:

```cpp
Element* HelloXen::BuildUI() {
    const auto btnRect = Rect::FromCenter(Window->GetWindowCenter().Translate(0.f, 20.f), 200, 48);
    auto btnCallback   = [&]() {
        ::MessageBoxA(Window->GetHandle(), "Hello!", "HelloXen", MB_OK | MB_ICONINFORMATION);
    };
    auto windowCallback = [&] { Quit(); };

    return new Box(
      Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset()),
      Context.AppTheme.FrameBackground,
      0,
      new Text("(Click the window background to quit)",
               Context.FontFamily,
               Window->GetWindowCenter(),
               Rect::FromPoints({0, 0}, Window->GetDimensions().AsOffset().Translate(0.f, -90.f)),
               0,
               new Text("Hello Xen!",
                        Context.FontFamily,
                        Window->GetWindowCenter(),
                        Rect::FromPoints({0, 0},
                                         Window->GetDimensions().AsOffset().Translate(0.f, -140.f)),
                        0,
                        new Box(btnRect,
                                Context.AppTheme.Tertiary,
                                1,
                                new Text("Say Hello",
                                         Context.FontFamily,
                                         Window->GetWindowCenter(),
                                         btnRect,
                                         0,
                                         nullptr,
                                         600,
                                         16.f,
                                         Context.AppTheme.White),
                                btnCallback),
                        300,
                        24.f,
                        Context.AppTheme.TextHighlight),
               300,
               14.f,
               Context.AppTheme.TextHighlight),
      windowCallback);
}
```

This produces the following application window:

<p align="middle">
    <img src="Docs/screenshot_hello.png"/>
</p>

## Status

Not even remotely close to usable. Check back in a year.