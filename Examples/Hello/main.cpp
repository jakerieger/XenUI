// Copyright 2026 Jake Rieger
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <XenUI/XenUI.hpp>

#include <format>
#include <string>

using namespace XenUI;

namespace {
    struct GlobalState {
        std::string Version;
        std::string Author;
    };

    /// @brief The window has a custom title bar, so the content draws it: a strip the user can drag the window by,
    /// with the minimize, maximize and close buttons on the right.
    struct TitleStrip {
        std::string Title;

        Node Build(BuildContext& Context) const {
            const Theme& Style  = Theme::Of(Context);
            const auto* Globals = Context.Find<GlobalState>();
            const auto TitleFmt = std::format("{} (v{})", Title, Globals->Version);

            return Container {
              .Height     = 32.0f,
              .Background = Style.Chrome,
              .Child =
                WindowDragArea {
                  .Child =
                    Row {
                      .Children =
                        {
                          Padding {.Insets = {.Left = 12.0f},
                                   .Child =
                                     Text {.Content = TitleFmt, .Font = Style.BodyFont, .Color = Style.TextMuted}},
                          Spacer {},
                          WindowControls {},
                        },
                    }},
            };
        }
    };

    /// @brief A text field for a name and a button that greets it. The component's state holds two signals; reading
    /// _Greeting in Build makes the component rebuild whenever the button changes it.
    struct Greeter {
        class State : public ComponentState<Greeter> {
        protected:
            Node Build(BuildContext& Context) override {
                const GlobalState* Globals = Context.Find<GlobalState>();
                const std::string Author   = Globals != nullptr ? Globals->Author : std::string();

                const auto
                  Root =
                    Column {
                      .CrossAlignment = CrossAxisAlignment::Stretch,
                      .Children =
                        {
                          TitleStrip {.Title = "Hello, XenUI"},
                          Padding {
                            .Insets = EdgeInsets::All(24.0f),
                            .Child =
                              Column {
                                .Spacing        = 12.0f,
                                .CrossAlignment = CrossAxisAlignment::Stretch,
                                .Children =
                                  {
                                    Row {
                                      .Spacing = 8.0f,
                                      .Children =
                                        {
                                          Expanded {.Child =
                                                      TextField {
                                                        .Bind        = &_Name,
                                                        .OnSubmit    = [this,
                                                                     Author](const std::string&) { Greet(Author); },
                                                        .Placeholder = "Your name",
                                                        .AutoFocus   = true,
                                                      }},
                                          Button {.Label     = "Greet",
                                                  .OnPressed = [this, Author] { Greet(Author); },
                                                  .Variant   = ButtonVariant::Primary},
                                        },
                                    },
                                    Text {.Content = _Greeting()},
                                  },
                              },
                          },
                        },
                    };

                return Root;
            }

        private:
            void Greet(const std::string& Author) {
                const auto Name = _Name.Peek();
                _Greeting       = std::format("Hello, {}", Name.empty() ? Author : Name);
            }

            Signal<std::string> _Name     = std::string();
            Signal<std::string> _Greeting = std::string();
        };
    };

    struct Hello {
        Node Build(BuildContext&) const {
            return Provide<GlobalState> {
              .Value =
                {
                  .Version = "1.0.0",
                  .Author  = "Jake Rieger",
                },
              .Child = Greeter {},
            };
        }
    };
}  // namespace

int main() {
    App HelloApp;

    const WindowDesc Desc {
      .Title          = "Hello, XenUI",
      .ClientSize     = {360.0f, 212.0f},
      .CustomTitleBar = true,  // make title bar part of client draw region
    };

    const auto OnClosed = [&HelloApp] { HelloApp.Quit(); };

    Window MainWindow(Desc);
    MainWindow.SetOnClosed(OnClosed);
    MainWindow.SetContent(Hello {});
    MainWindow.Show();

    return HelloApp.Run();
}
