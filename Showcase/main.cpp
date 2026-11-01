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

#include <Windows.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace XenUI;

    const Theme* CurrentTheme = nullptr;

    /// @brief The theme the page is being built with. The root provides it and rebuilds the page when it changes.
    const Theme& Ink() {
        static const Theme kFallback = Theme::Dark();
        return CurrentTheme != nullptr ? *CurrentTheme : kFallback;
    }
    constexpr int kListItems = 100000;

    const Font kTitleFont {.Size = 22.0f, .Weight = FontWeight::Bold};
    const Font kHeadingFont {.Size = 14.0f, .Weight = FontWeight::SemiBold};
    const Font kSmallFont {.Size = 11.0f};

    constexpr std::array<std::string_view, 12> kSections = {"Controls",
                                                            "Text input",
                                                            "Lists",
                                                            "Menus & tooltips",
                                                            "Drag & drop",
                                                            "Native windows",
                                                            "Live canvas",
                                                            "Painting",
                                                            "Layout",
                                                            "Animation",
                                                            "More controls",
                                                            "System"};

    struct Swatch {
        const char* Name;
        Color Tint;
    };

    constexpr Swatch kSwatches[] = {{"Blue", Color::FromHex(0x4DA3FF)},
                                    {"Mint", Color::FromHex(0x4DFFB8)},
                                    {"Amber", Color::FromHex(0xFFC94D)},
                                    {"Coral", Color::FromHex(0xFF6A3D)}};

    const SvgIcon kPlayIcon =
      SvgIcon::FromPathData("M8 5.5v13a1 1 0 0 0 1.5.86l10-6.5a1 1 0 0 0 0-1.72l-10-6.5A1 1 0 0 0 8 5.5z");
    const SvgIcon kPauseIcon = SvgIcon::FromPathData("M6 5h4v14H6zM14 5h4v14h-4z");
    const SvgIcon kStopIcon =
      SvgIcon::FromPathData("M7 6h10a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1z");
    const SvgIcon kRecordIcon = SvgIcon::FromPathData("M12 5a7 7 0 1 0 0 14a7 7 0 1 0 0-14z");
    const SvgIcon kLoopIcon   = SvgIcon::Parse(
      R"svg(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M17 2l4 4-4 4"/><path d="M3 11V9a3 3 0 0 1 3-3h15"/><path d="M7 22l-4-4 4-4"/><path d="M21 13v2a3 3 0 0 1-3 3H3"/>
    </svg>)svg");
    const SvgIcon kSearchIcon = SvgIcon::Parse(
      R"svg(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">
      <circle cx="11" cy="11" r="7"/><line x1="16" y1="16" x2="21" y2="21"/>
    </svg>)svg");
    const SvgIcon kFolderIcon = SvgIcon::Parse(
      R"svg(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round">
      <path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>
    </svg>)svg");
    const SvgIcon kExportIcon = SvgIcon::Parse(
      R"svg(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M12 4v11M7 10l5 5 5-5M5 20h14"/>
    </svg>)svg");

    struct ShowcaseState {
        Signal<int> Section        = 0;
        Signal<float> Volume       = 0.6f;
        Signal<float> Cutoff       = 0.5f;
        Signal<float> Balance      = 0.0f;
        Signal<bool> Notifications = true;
        Signal<bool> Autosave      = false;
        Signal<int> Quality        = 1;
        Signal<std::string> Name   = std::string("XenUI");
        Signal<std::string> Status = std::string("Pick a section on the left");
        Signal<int> SelectedRow    = -1;
        Signal<float> SplitRatio   = 0.4f;
        Signal<float> NestedRatio  = 0.5f;
        Signal<int> LeftZone       = -1;
        Signal<int> RightZone      = -1;
        Signal<int> HoveredZone    = -1;
        Signal<std::vector<std::string>> DroppedFiles;
        Signal<bool> FileHover  = false;
        Signal<bool> ShowNative = true;
        /// @brief 0 follows Windows, 1 is dark and 2 is light.
        Signal<int> ThemeMode       = 0;
        Signal<int> Channels        = 1;
        Signal<float> Progress      = 0.35f;
        Signal<bool> Busy           = true;
        Signal<float> Tempo         = 120.0f;
        Signal<float> GainDb        = -6.0f;
        Signal<int> SettingsTab     = 0;
        Signal<bool> ShowInspector  = false;
        Signal<bool> ShowFrameStats = false;
        Signal<bool> ConfirmClose   = false;
        Signal<bool> Playing        = false;
        Signal<bool> Recording      = false;
        Signal<bool> Looping        = true;
        Signal<int> IconChoice      = 0;
        Signal<std::string> Picked  = std::string("Nothing picked yet");
        Window* MainWindow          = nullptr;
        Theme ActiveTheme;

        void Report(std::string Message) { Status = std::move(Message); }
    };

    double Seconds() {
        static const auto kStart = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - kStart).count();
    }

    Node Card(const std::string& Title, const std::string& Caption, const Node& Content) {
        return Container {
          .Padding      = EdgeInsets::All(16.0f),
          .Background   = Ink().Surface,
          .CornerRadius = 8.0f,
          .BorderColor  = Ink().Border,
          .Child =
            Column {
              .Spacing        = 12.0f,
              .CrossAlignment = CrossAxisAlignment::Stretch,
              .Children =
                {
                  Column {.Spacing        = 3.0f,
                          .CrossAlignment = CrossAxisAlignment::Start,
                          .MainSize       = MainAxisSize::Min,
                          .Children       = {Text {.Content = Title, .Font = kHeadingFont},
                                             Text {.Content = Caption, .Font = kSmallFont, .Color = Ink().TextMuted}}},
                  Content,
                },
            },
        };
    }

    /// @brief A label that follows a value signal.
    struct ValueLabel {
        const Signal<float>* Value = nullptr;
        std::string Format         = "{:.2f}";

        Node Build(BuildContext& Context) const {
            (void)Context;
            return Text {.Content = std::vformat(Format, std::make_format_args(Value->Get())),
                         .Font    = kSmallFont,
                         .Color   = Ink().TextMuted};
        }
    };

    struct ControlsSection {
        ShowcaseState* State = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            ShowcaseState* Model = State;
            const auto Labelled  = [](const std::string& Label, const Node& Control) -> Node {
                return Column {
                   .Spacing  = 6.0f,
                   .MainSize = MainAxisSize::Min,
                   .Children = {Control, Text {.Content = Label, .Font = kSmallFont, .Color = Ink().TextMuted}}};
            };
            return Column {
              .Spacing        = 16.0f,
              .CrossAlignment = CrossAxisAlignment::Stretch,
              .Children =
                {
                  Card("Buttons",
                       "Primary, secondary and ghost variants. Tab moves focus; Enter or Space activates.",
                       Row {.Spacing = 10.0f,
                            .Children =
                              {
                                Button {.Label     = "Primary",
                                        .OnPressed = [Model] { Model->Report("Pressed the primary button"); },
                                        .Variant   = ButtonVariant::Primary},
                                Button {.Label     = "Secondary",
                                        .OnPressed = [Model] { Model->Report("Pressed the secondary button"); }},
                                Button {.Label     = "Ghost",
                                        .OnPressed = [Model] { Model->Report("Pressed the ghost button"); },
                                        .Variant   = ButtonVariant::Ghost},
                                Button {.Label = "Disabled", .Enabled = false},
                              }}),
                  Card("Toggles",
                       "Checkboxes and switches bound to signals.",
                       Row {.Spacing = 24.0f,
                            .Children =
                              {
                                Checkbox {.Value     = Model->Notifications(),
                                          .OnChanged = [Model](bool Value) { Model->Notifications = Value; },
                                          .Label     = "Notifications"},
                                Switch {.Value     = Model->Autosave(),
                                        .OnChanged = [Model](bool Value) { Model->Autosave = Value; },
                                        .Label     = "Autosave"},
                                Text {.Content = Model->Autosave() ? "Autosave is on" : "Autosave is off",
                                      .Font    = kSmallFont,
                                      .Color   = Ink().TextMuted},
                              }}),
                  Card("Values",
                       "Sliders and knobs write their signals directly and repaint without rebuilding. Drag knobs "
                       "vertically; hold Shift for fine control; double-click to reset.",
                       Row {.Spacing = 28.0f,
                            .Children =
                              {
                                Labelled("Volume",
                                         SizedBox {.Width = 220.0f,
                                                   .Child = Slider {.Bind = &Model->Volume, .Label = "Volume"}}),
                                Labelled("Cutoff", Knob {.Bind = &Model->Cutoff, .Diameter = 44.0f, .Label = "Cutoff"}),
                                Labelled("Balance",
                                         Knob {.Min      = -1.0f,
                                               .Max      = 1.0f,
                                               .Bind     = &Model->Balance,
                                               .Diameter = 44.0f,
                                               .Bipolar  = true,
                                               .Label    = "Balance"}),
                                Column {.Spacing  = 4.0f,
                                        .MainSize = MainAxisSize::Min,
                                        .Children = {ValueLabel {&Model->Volume, "Volume {:.2f}"},
                                                     ValueLabel {&Model->Cutoff, "Cutoff {:.2f}"},
                                                     ValueLabel {&Model->Balance, "Balance {:+.2f}"}}},
                              }}),
                  Card("Select",
                       "A dropdown in its own native popup window, so it can extend past the window edge.",
                       Row {.Spacing = 12.0f,
                            .Children =
                              {
                                Select {.Options       = {"Draft", "Balanced", "High", "Lossless"},
                                        .SelectedIndex = Model->Quality(),
                                        .OnChanged     = [Model](int Index) { Model->Quality = Index; },
                                        .Width         = 160.0f,
                                        .Label         = "Quality"},
                                Text {.Content = std::format("Quality level {}", Model->Quality() + 1),
                                      .Font    = kSmallFont,
                                      .Color   = Ink().TextMuted},
                              }}),
                },
            };
        }
    };

    struct Greeting {
        ShowcaseState* State = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            const std::string& Name = State->Name();
            return Text {.Content = Name.empty()
                                      ? std::string("Type a name above")
                                      : std::format("Hello, {}! ({} characters)", Name, CountCodePoints(Name)),
                         .Color   = Ink().TextMuted};
        }
    };

    Node TextSection(ShowcaseState& State) {
        ShowcaseState* Model = &State;
        return Column {
          .Spacing        = 16.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .Children =
            {
              Card(
                "Bound text field",
                "Two-way binding to a string signal. Supports selection, Ctrl+arrows, Ctrl+A/C/X/V, undo and redo, and "
                "input-method composition.",
                Column {.Spacing        = 10.0f,
                        .CrossAlignment = CrossAxisAlignment::Stretch,
                        .MainSize       = MainAxisSize::Min,
                        .Children = {TextField {.Bind = &Model->Name, .Placeholder = "Your name", .Label = "Name"},
                                     Greeting {Model}}}),
              Card("Multi-line text",
                   "TextArea wraps and scrolls vertically. Enter inserts a line break, Ctrl+Enter submits, and Up, "
                   "Down, Home and End follow the wrapped lines.",
                   TextArea {
                     .OnSubmit =
                       [Model](const std::string& Text) {
                           Model->Report(std::format("Submitted {} characters of notes", Text.size()));
                       },
                     .Placeholder = "Session notes...",
                     .Lines       = 5,
                     .Label       = "Session notes",
                   }),
              Card("Submitting",
                   "Enter calls OnSubmit; the field also limits its length to 20 characters.",
                   TextField {
                     .OnSubmit =
                       [Model](const std::string& Text) { Model->Report(std::format("Submitted \"{}\"", Text)); },
                     .Placeholder = "Type and press Enter",
                     .MaxLength   = 20,
                   }),
              Card("Clipboard",
                   "Text and custom formats through the system clipboard.",
                   Row {.Spacing = 10.0f,
                        .Children =
                          {
                            Button {.Label = "Copy greeting",
                                    .OnPressed =
                                      [Model] {
                                          Clipboard::SetText(std::format("Hello, {}!", Model->Name.Peek()));
                                          Model->Report("Copied the greeting");
                                      }},
                            Button {.Label = "Paste into name",
                                    .OnPressed =
                                      [Model] {
                                          if (const auto Pasted = Clipboard::GetText()) { Model->Name = *Pasted; }
                                      }},
                          }}),
            },
        };
    }

    struct ListRow {
        ShowcaseState* State = nullptr;
        int Index            = 0;

        Node Build(BuildContext& Context) const {
            (void)Context;
            const bool Selected = State->SelectedRow() == Index;
            return GestureDetector {
              .OnTap =
                [Model = State, Index = Index](const TapEvent&) {
                    Model->SelectedRow = Index;
                    Model->Report(std::format("Selected row {}", Index + 1));
                },
              .Behavior = HitTestBehavior::Opaque,
              .Child =
                Container {
                  .Padding = EdgeInsets::Symmetric(12.0f, 0.0f),
                  .Background =
                    Selected ? Ink().Accent.WithAlpha(0.18f) : (Index % 2 == 0 ? Ink().Surface : Ink().SurfaceRaised),
                  .Child =
                    Row {
                      .Spacing = 12.0f,
                      .Children =
                        {
                          SizedBox {.Width = 70.0f,
                                    .Child = Text {.Content = std::format("#{}", Index + 1),
                                                   .Font    = kSmallFont,
                                                   .Color   = Ink().TextMuted}},
                          Expanded {.Child =
                                      Text {.Content = std::format("Item {} · built only while visible", Index + 1),
                                            .Font    = kSmallFont}},
                        },
                    },
                },
            };
        }
    };

    Node ListSection(ShowcaseState& State) {
        ShowcaseState* Model = &State;
        return Card(
          "Virtualized list",
          std::format("{} rows; only the visible ones exist as widgets, so scrolling costs the same at any length.",
                      kListItems),
          SizedBox {.Height = 520.0f,
                    .Child  = ClipRect {.Child = ListView {
                                          .ItemCount   = kListItems,
                                          .ItemExtent  = 26.0f,
                                          .ItemBuilder = [Model](int Index) { return Node(ListRow {Model, Index}); },
                                       }}});
    }

    Node MenuSection(ShowcaseState& State) {
        ShowcaseState* Model = &State;
        const auto Item      = [Model](std::string Label, std::string Shortcut = {}) {
            return MenuItem {.Label = Label, .ShortcutText = std::move(Shortcut), .OnSelect = [Model, Label] {
                                 Model->Report(std::format("Chose {}", Label));
                             }};
        };
        return Column {
          .Spacing        = 16.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .Children =
            {
              Card("Context menu",
                   "Right-click the area below. Menus open in native popup windows and can be driven with the arrow "
                   "keys, Enter and Escape.",
                   ContextMenu {
                     .Items = {Item("Cut", "Ctrl+X"),
                               Item("Copy", "Ctrl+C"),
                               Item("Paste", "Ctrl+V"),
                               MenuItem::Divider(),
                               Item("Select all", "Ctrl+A"),
                               {.Label = "Unavailable", .Enabled = false}},
                     .Child =
                       Container {
                         .Height         = 120.0f,
                         .Background     = Ink().Track,
                         .CornerRadius   = 6.0f,
                         .BorderColor    = Ink().Border,
                         .ChildAlignment = Alignment::Center(),
                         .Child          = Text {.Content = "Right-click here", .Color = Ink().TextMuted},
                       },
                   }),
              Card("Tooltips",
                   "Hover a button for a moment.",
                   Row {.Spacing = 10.0f,
                        .Children =
                          {
                            Tooltip {.Message = "Tooltips follow the theme and stay on screen",
                                     .Child   = Button {.Label = "Hover me"}},
                            Tooltip {.Message = "A second tooltip with a shorter delay",
                                     .Child   = Button {.Label = "Or me"},
                                     .Delay   = 0.2},
                          }}),
            },
        };
    }

    Node SwatchChip(int Index) {
        const Swatch& Entry = kSwatches[Index];
        return Container {
          .Width          = 96.0f,
          .Height         = 34.0f,
          .Background     = Entry.Tint.WithAlpha(0.22f),
          .CornerRadius   = 6.0f,
          .BorderColor    = Entry.Tint,
          .ChildAlignment = Alignment::Center(),
          .Child          = Text {.Content = Entry.Name, .Font = kSmallFont},
        };
    }

    Node DropZone(ShowcaseState* Model, Signal<int>* Value, int Zone, const char* Label) {
        const int Held     = Value->Get();
        const bool Hovered = Model->HoveredZone() == Zone;
        const Color Tint   = Held >= 0 ? kSwatches[Held].Tint : Ink().TextMuted;
        return DropTarget {
          .OnOver =
            [Model, Zone](const DropEvent& Event) {
                if (Event.Data->Get<int>() == nullptr) { return DropEffect::None; }
                Model->HoveredZone = Zone;
                return DropEffect::Copy;
            },
          .OnLeave =
            [Model, Zone] {
                if (Model->HoveredZone.Peek() == Zone) { Model->HoveredZone = -1; }
            },
          .OnDrop =
            [Model, Value, Label](const DropEvent& Event) {
                *Value = *Event.Data->Get<int>();
                Model->Report(std::format("Dropped {} on the {} zone", kSwatches[Value->Peek()].Name, Label));
            },
          .Behavior = HitTestBehavior::Opaque,
          .Child =
            Container {
              .Height         = 120.0f,
              .Background     = Tint.WithAlpha(Hovered ? 0.3f : 0.12f),
              .CornerRadius   = 8.0f,
              .BorderColor    = Hovered ? Ink().Accent : Tint,
              .BorderWidth    = Hovered ? 2.0f : 1.0f,
              .ChildAlignment = Alignment::Center(),
              .Child          = Text {.Content = Held >= 0 ? std::format("{} zone · {}", Label, kSwatches[Held].Name)
                                                           : std::format("{} zone · drop a color", Label),
                                      .Color   = Ink().TextMuted},
            },
        };
    }

    struct DropZones {
        ShowcaseState* State = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            return Row {.Spacing        = 12.0f,
                        .CrossAlignment = CrossAxisAlignment::Stretch,
                        .Children       = {Expanded {.Child = DropZone(State, &State->LeftZone, 0, "Left")},
                                           Expanded {.Child = DropZone(State, &State->RightZone, 1, "Right")}}};
        }
    };

    struct FileZone {
        ShowcaseState* State = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            ShowcaseState* Model = State;
            const auto& Files    = Model->DroppedFiles();
            const bool Hovered   = Model->FileHover();
            std::string Listing  = Files.empty() ? std::string("Drag files here from Explorer") : std::string();
            for (size_t i = 0; i < Files.size() && i < 6; ++i) {
                Listing += (i > 0 ? "\n" : "") + Files[i];
            }
            if (Files.size() > 6) { Listing += std::format("\n… and {} more", Files.size() - 6); }
            return DropTarget {
              .OnOver =
                [Model](const DropEvent& Event) {
                    if (Event.Data->Files.empty()) { return DropEffect::None; }
                    Model->FileHover = true;
                    return DropEffect::Copy;
                },
              .OnLeave = [Model] { Model->FileHover = false; },
              .OnDrop =
                [Model](const DropEvent& Event) {
                    std::vector<std::string> Names;
                    for (const auto& Path : Event.Data->Files) {
                        Names.push_back(Path.filename().string());
                    }
                    Model->Report(std::format("Received {} file{}", Names.size(), Names.size() == 1 ? "" : "s"));
                    Model->DroppedFiles = std::move(Names);
                },
              .Behavior = HitTestBehavior::Opaque,
              .Child =
                Container {
                  .Height       = 140.0f,
                  .Padding      = EdgeInsets::All(14.0f),
                  .Background   = Hovered ? Ink().Accent.WithAlpha(0.15f) : Ink().Track,
                  .CornerRadius = 8.0f,
                  .BorderColor  = Hovered ? Ink().Accent : Ink().Border,
                  .Child        = Text {.Content = Listing,
                                        .Font    = kSmallFont,
                                        .Color   = Files.empty() ? Ink().TextMuted : Ink().Text},
                },
            };
        }
    };

    Node DragSection(ShowcaseState& State) {
        ShowcaseState* Model = &State;
        NodeList Chips;
        for (int i = 0; i < static_cast<int>(std::size(kSwatches)); ++i) {
            Chips.push_back(DragSource {
              .Data = [i]() -> std::optional<DragData> {
                  return DragData {.Value = i, .Text = std::string(kSwatches[i].Name)};
              },
              .Feedback = SwatchChip(i),
              .OnDragEnd =
                [Model, i](DropEffect Result) {
                    if (Result == DropEffect::None) {
                        Model->Report(std::format("{} was not dropped anywhere", kSwatches[i].Name));
                    }
                },
              .Child = MouseRegion {.Cursor = CursorShape::Hand, .Child = SwatchChip(i)},
            });
        }
        return Column {
          .Spacing        = 16.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .Children =
            {
              Card(
                "Inside the app",
                "Drag a color onto a zone. The preview follows the pointer and dims over places that refuse the data; "
                "Escape cancels. The colors also carry text, so dropping one into Notepad or a browser types its name.",
                Column {.Spacing        = 14.0f,
                        .CrossAlignment = CrossAxisAlignment::Stretch,
                        .MainSize       = MainAxisSize::Min,
                        .Children       = {Row {.Spacing = 10.0f, .Children = std::move(Chips)}, DropZones {Model}}}),
              Card("From other applications",
                   "Drop files from Explorer. Windows registers every XenUI window as an OLE drop target, so the same "
                   "DropTarget widget receives them.",
                   FileZone {Model}),
            },
        };
    }

    /// @brief A Win32 EDIT control created inside a NativeHost container, resized with it.
    Node NativeEditor() {
        auto Edit = std::make_shared<HWND>(nullptr);
        return NativeHost {
          .OnAttach =
            [Edit](NativeHandle Container) {
                *Edit = CreateWindowExW(
                  0,
                  L"EDIT",
                  L"This is a native Win32 EDIT control living inside the XenUI layout.\r\n\r\n"
                  L"It is a child window of the main window, moved, resized and clipped by the compositor "
                  L"as the page scrolls and the window resizes. Plugin editors are hosted the same way.",
                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                  0,
                  0,
                  100,
                  100,
                  static_cast<HWND>(Container),
                  nullptr,
                  GetModuleHandleW(nullptr),
                  nullptr);
                SendMessageW(*Edit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            },
          .OnDetach = [Edit](NativeHandle) { *Edit = nullptr; },
          .OnResize =
            [Edit](int Width, int Height) {
                if (*Edit != nullptr) { MoveWindow(*Edit, 0, 0, Width, Height, TRUE); }
            },
          .Height = 180.0f,
        };
    }

    struct NativeSection {
        ShowcaseState* State = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            ShowcaseState* Model = State;
            const bool Shown     = Model->ShowNative();
            return Column {
              .Spacing        = 16.0f,
              .CrossAlignment = CrossAxisAlignment::Stretch,
              .Children =
                {
                  Card("Native child windows",
                       "NativeHost embeds a platform window in the layout. Scroll the page, resize the window or "
                       "toggle the switch: the control follows, clips to the page and is destroyed when removed.",
                       Column {.Spacing        = 12.0f,
                               .CrossAlignment = CrossAxisAlignment::Stretch,
                               .MainSize       = MainAxisSize::Min,
                               .Children =
                                 {
                                   Switch {.Value     = Shown,
                                           .OnChanged = [Model](bool Value) { Model->ShowNative = Value; },
                                           .Label     = "Show the native control"},
                                   Shown
                                     ? NativeEditor()
                                     : Node(Container {.Height         = 180.0f,
                                                       .Background     = Ink().Track,
                                                       .CornerRadius   = 6.0f,
                                                       .ChildAlignment = Alignment::Center(),
                                                       .Child = Text {.Content = "Removed", .Color = Ink().TextMuted}}),
                                 }}),
                  Card("Stacking",
                       "Native windows draw above XenUI content in the same window. Popups, menus and tooltips are "
                       "separate windows, so they still appear on top.",
                       Row {.Spacing  = 10.0f,
                            .Children = {Tooltip {.Message = "Tooltips are native popups, so they cover the control",
                                                  .Child   = Button {.Label = "Hover me"}}}}),
                  SizedBox {.Height = 600.0f},
                },
            };
        }
    };

    Node LiveSection() {
        return Card(
          "Live canvas",
          "Immediate-mode drawing with instanced Direct3D primitives, redrawn every frame for data that changes "
          "constantly, such as meters and scopes.",
          SizedBox {.Height = 420.0f,
                    .Child  = ClipRect {
                       .Child = LiveView {
                         .OnDraw =
                          [](LiveCanvas& Canvas) {
                              const double Time = Seconds();
                              const Size Extent = Canvas.GetSize();
                              Canvas.FillRect({0.0f, 0.0f, Extent.Width, Extent.Height}, Color::FromHex(0x111115));

                              constexpr int kBars  = 64;
                              const float BarWidth = Extent.Width / kBars;
                              const float BarsTop  = Extent.Height * 0.55f;
                              for (int Bar = 0; Bar < kBars; ++Bar) {
                                  const double Wave =
                                    0.5 + 0.5 * std::sin(Time * 2.3 + Bar * 0.31) * std::cos(Time * 0.7 + Bar * 0.11);
                                  const auto Height = static_cast<float>(Wave) * (Extent.Height - BarsTop - 8.0f);
                                  const Color Fill =
                                    Ink().Accent.Lerp(Color::FromHex(0xB84DFF), static_cast<float>(Bar) / kBars);
                                  Canvas.FillRect(
                                    {Bar * BarWidth + 1.0f, Extent.Height - 4.0f - Height, BarWidth - 2.0f, Height},
                                    Fill);
                              }

                              std::vector<Vec2> Points(1024);
                              for (size_t i = 0; i < Points.size(); ++i) {
                                  const double X = static_cast<double>(i) / (Points.size() - 1);
                                  const double Y =
                                    std::sin(X * 18.0 + Time * 3.0) * 0.6 + std::sin(X * 41.0 - Time * 5.0) * 0.25;
                                  Points[i] = {static_cast<float>(X) * Extent.Width,
                                               BarsTop * 0.5f - static_cast<float>(Y) * BarsTop * 0.38f};
                              }
                              Canvas.DrawLine({0.0f, BarsTop * 0.5f},
                                               {Extent.Width, BarsTop * 0.5f},
                                              1.0f,
                                              Color::FromHex(0xFFFFFF, 0.1f));
                              Canvas.DrawPolyline(Points, 2.0f, Color::FromHex(0x2ECC71));
                          },
                         .Continuous = true,
                      }}});
    }

    Node PaintSection() {
        return Card(
          "Vector painting",
          "Direct2D shapes, gradients, paths, strokes and text drawn by a CustomPaint and cached in its own layer.",
          CustomPaint {
            .OnPaint =
              [](Painter& Canvas, const Size& Extent) {
                  (void)Extent;
                  Canvas.FillRoundedRect(
                    {0.0f, 0.0f, 220.0f, 140.0f},
                    12.0f,
                    LinearGradient {.Start = {0.0f, 0.0f},
                                    .End   = {220.0f, 140.0f},
                                    .Stops = {{0.0f, Color::FromHex(0x1F3A5F)}, {1.0f, Color::FromHex(0x3B1F5F)}}});
                  Canvas.StrokeRoundedRect({0.5f, 0.5f, 219.0f, 139.0f}, 12.0f, Ink().Accent, {.Width = 1.0f});

                  Canvas.FillCircle(
                    {320.0f, 70.0f},
                    60.0f,
                    RadialGradient {.Center = {320.0f, 70.0f},
                                    .Radius = {60.0f, 60.0f},
                                    .Stops  = {{0.0f, Color::FromHex(0xFFC94D)}, {1.0f, Color::FromHex(0xE67E22)}}});

                  PathBuilder Star;
                  for (int Point = 0; Point < 10; ++Point) {
                      const double Angle  = -std::numbers::pi / 2.0 + Point * std::numbers::pi / 5.0;
                      const double Radius = Point % 2 == 0 ? 64.0 : 26.0;
                      const Vec2 Vertex {static_cast<float>(480.0 + std::cos(Angle) * Radius),
                                         static_cast<float>(72.0 + std::sin(Angle) * Radius)};
                      Point == 0 ? Star.MoveTo(Vertex) : Star.LineTo(Vertex);
                  }
                  const Path Shape = Star.Close().Build();
                  Canvas.FillPath(Shape, Color::FromHex(0x4DFFB8, 0.25f));
                  Canvas.StrokePath(Shape, Color::FromHex(0x4DFFB8), {.Width = 2.0f, .Join = LineJoin::Round});

                  for (int Line = 0; Line < 5; ++Line) {
                      const float Y = 180.0f + Line * 14.0f;
                      Canvas.DrawLine({0.0f, Y},
                                      {560.0f, Y},
                                      Ink().Text.WithAlpha(0.2f + Line * 0.15f),
                                      {.Width = 1.0f + Line, .Cap = LineCap::Round, .Dashes = {4.0f, 2.0f}});
                  }
                  const TextLayout Label("Text is shaped by DirectWrite and drawn as part of the same display list.",
                                         Font {.Size = 15.0f},
                                         {},
                                         {560.0f, kUnbounded});
                  Canvas.DrawTextLayout(Label, {0.0f, 260.0f}, Ink().Text);
              },
            .Height = 300.0f,
          });
    }

    Node LayoutSection(ShowcaseState& State) {
        const auto Pane = [](const char* Label, Color Tint) -> Node {
            return Container {.Background     = Tint.WithAlpha(0.18f),
                              .CornerRadius   = 6.0f,
                              .BorderColor    = Tint,
                              .ChildAlignment = Alignment::Center(),
                              .Child          = Text {.Content = Label, .Font = kSmallFont}};
        };
        NodeList Tags;
        for (const char* Tag : {"Kick",
                                "Snare",
                                "Closed hat",
                                "Open hat",
                                "Ride",
                                "Crash",
                                "Tom",
                                "Clap",
                                "Shaker",
                                "Percussion",
                                "Vocal chop",
                                "FX"}) {
            Tags.push_back(Container {.Padding      = EdgeInsets::Symmetric(10.0f, 4.0f),
                                      .Background   = Ink().SurfaceRaised,
                                      .CornerRadius = 12.0f,
                                      .BorderColor  = Ink().Border,
                                      .Child        = Text {.Content = Tag, .Font = kSmallFont}});
        }
        NodeList Tiles;
        for (int i = 0; i < 10; ++i) {
            Tiles.push_back(Pane(kSwatches[i % std::size(kSwatches)].Name, kSwatches[i % std::size(kSwatches)].Tint));
        }
        return Column {
          .Spacing        = 16.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .Children =
            {
              Card("Wrap",
                   "Children flow left to right and start a new run when the next one does not fit. Resize the window "
                   "to reflow.",
                   Wrap {.Spacing = 8.0f, .RunSpacing = 8.0f, .Children = std::move(Tags)}),
              Card("Grid",
                   "Equal-width columns, as many of at least 120 DIPs as fit, with a fixed row height.",
                   Grid {.MinColumnWidth = 120.0f,
                         .Spacing        = 8.0f,
                         .RunSpacing     = 8.0f,
                         .RowHeight      = 56.0f,
                         .Children       = std::move(Tiles)}),
              Card(
                "Split views",
                "Drag the dividers. Ratios live in signals, so they survive rebuilds.",
                SizedBox {.Height = 240.0f,
                          .Child =
                            SplitView {
                              .Ratio  = &State.SplitRatio,
                              .First  = Padding {.Insets = {.Right = 4.0f}, .Child = Pane("Sidebar", Ink().Accent)},
                              .Second = Padding {.Insets = {.Left = 4.0f},
                                                 .Child  = SplitView {.Direction = Axis::Vertical,
                                                                      .Ratio     = &State.NestedRatio,
                                                                      .First     = Padding {.Insets = {.Bottom = 4.0f},
                                                                                            .Child  = Pane("Editor",
                                                                                                     Color::FromHex(
                                                                                                       0x4DFFB8))},
                                                                      .Second    = Padding {.Insets = {.Top = 4.0f},
                                                                                            .Child  = Pane("Console",
                                                                                                      Color::FromHex(
                                                                                                        0xFF6A3D))}}},
                            }}),
              Card(
                "Flex and stack",
                "Rows with flexible children and a stack with positioned layers.",
                Row {
                  .Spacing        = 12.0f,
                  .CrossAlignment = CrossAxisAlignment::Stretch,
                  .Children =
                    {
                      Expanded {.Child = SizedBox {.Height = 120.0f,
                                                   .Child  = Row {.Spacing        = 6.0f,
                                                                  .CrossAlignment = CrossAxisAlignment::Stretch,
                                                                  .Children       = {Expanded {.Child = Pane("1",
                                                                                                      Ink().Accent)},
                                                                                     Expanded {.Flex  = 2,
                                                                                               .Child = Pane("2",
                                                                                                      Ink().Accent)},
                                                                                     Expanded {.Flex  = 3,
                                                                                               .Child = Pane("3",
                                                                                                      Ink().Accent)}}}}},
                      SizedBox {.Width  = 220.0f,
                                .Height = 120.0f,
                                .Child  = Stack {.Fit      = StackFit::Expand,
                                                 .Children = {Pane("", Color::FromHex(0xB84DFF)),
                                                              Positioned {.Left   = 10.0f,
                                                                          .Top    = 10.0f,
                                                                          .Width  = 90.0f,
                                                                          .Height = 50.0f,
                                                                          .Child  = Pane("Top left", Ink().Accent)},
                                                              Positioned {.Right  = 10.0f,
                                                                          .Bottom = 10.0f,
                                                                          .Width  = 100.0f,
                                                                          .Height = 50.0f,
                                                                          .Child  = Pane("Bottom right",
                                                                                       Color::FromHex(0xFFC94D))}}}},
                    }}),
            },
        };
    }

    constexpr const char* kEasingNames[] = {"Linear", "Ease in", "Ease out", "Ease in-out", "Ease out back"};
    constexpr Easing kEasings[]          = {
      Easing::Linear, Easing::EaseIn, Easing::EaseOut, Easing::EaseInOut, Easing::EaseOutBack};
    constexpr float kRaceWidth = 420.0f;

    struct AnimationSection {
        ShowcaseState* Model = nullptr;

        class State : public ComponentState<AnimationSection> {
        protected:
            Node Build(BuildContext& Context) override {
                (void)Context;
                ShowcaseState* Shared = GetWidget().Model;
                NodeList Races;
                for (size_t i = 0; i < std::size(kEasings); ++i) {
                    const float Position = _Runners[i].Get();
                    Races
                      .push_back(
                        Row {
                          .Spacing = 12.0f,
                          .Children =
                            {
                              SizedBox {
                                .Width = 100.0f,
                                .Child =
                                  Text {.Content = kEasingNames[i], .Font = kSmallFont, .Color = Ink().TextMuted}},
                              SizedBox {.Width  = kRaceWidth + 24.0f,
                                        .Height = 24.0f,
                                        .Child =
                                          Stack {
                                            .Fit = StackFit::Expand,
                                            .Children =
                                              {
                                                Align {.Position = Alignment::CenterLeft(),
                                                       .Child    = Container {.Height       = 4.0f,
                                                                              .Background   = Ink().Track,
                                                                              .CornerRadius = 2.0f}},
                                                Positioned {.Left   = Position * kRaceWidth,
                                                            .Top    = 0.0f,
                                                            .Width  = 24.0f,
                                                            .Height = 24.0f,
                                                            .Child  = Container {.Background   = Ink().Accent,
                                                                                 .CornerRadius = 12.0f}},
                                              },
                                          }},
                            },
                        });
                }

                const float Grow = _Grow.Get();
                return Column {
                  .Spacing        = 16.0f,
                  .CrossAlignment = CrossAxisAlignment::Stretch,
                  .Children =
                    {
                      Card("Easing curves",
                           "Animated<T> moves a value to a target over a duration. Values are tracked like signals, and frames are only requested while something moves.",
                           Column {.Spacing        = 10.0f,
                                   .CrossAlignment = CrossAxisAlignment::Start,
                                   .MainSize       = MainAxisSize::Min,
                                   .Children       = {Column {.Spacing = 8.0f, .MainSize = MainAxisSize::Min, .Children = std::move(Races)},
                                                      Row {.MainSize = MainAxisSize::Min, .Children = {Button {.Label = "Race", .OnPressed = [this, Shared] {
                                                                  _Forward = !_Forward;
                                                                  for (size_t i = 0; i < std::size(kEasings); ++i) { _Runners[i].AnimateTo(_Forward ? 1.0f : 0.0f, 0.9, kEasings[i]); }
                                                                  Shared->Report(AnimationTicker::AreAnimationsEnabled() ? "Racing" : "Animations are turned off in Windows, so values jump");
                                                              }, .Variant = ButtonVariant::Primary}}}}}),
                      Card("Color and size",
                           "Hover the tile. Color, size and corner radius are all animated values.",
                           MouseRegion {
                             .OnEnter = [this](const PointerEvent&) {
                                 if (!_Tint.IsAnimating()) { _Tint.Set(Ink().SurfaceRaised); }
                                 _Tint.AnimateTo(Ink().Accent, 0.25);
                                 _Hovered = true;
                                 _Grow.AnimateTo(1.0f, 0.35, Easing::EaseOutBack);
                             },
                             .OnExit = [this](const PointerEvent&) {
                                 _Hovered = false;
                                 _Tint.AnimateTo(Ink().SurfaceRaised, 0.4);
                                 _Grow.AnimateTo(0.0f, 0.3);
                             },
                             .Child = SizedBox {.Height = 150.0f,
                                                .Child  = Align {.Position = Alignment::CenterLeft(),
                                                                 .Child    = Container {
                                                                      .Width          = 160.0f + 80.0f * Grow,
                                                                      .Height         = 90.0f + 40.0f * Grow,
                                                                      .Background     = _Tint.IsAnimating() || _Hovered ? _Tint.Get() : Ink().SurfaceRaised,
                                                                      .CornerRadius   = 8.0f + 24.0f * std::clamp(Grow, 0.0f, 1.0f),
                                                                      .BorderColor    = Ink().Border,
                                                                      .ChildAlignment = Alignment::Center(),
                                                                      .Child          = Text {.Content = "Hover me", .Color = Ink().Text},
                                                                 }}},
                           }),
                      Card("Built-in controls",
                           "Buttons fade between hover and press, checkboxes fade in and switches slide. Turn off Animation effects in Windows settings and they change instantly.",
                           Row {.Spacing  = 20.0f,
                                .Children = {
                                  Button {.Label = "Hover and press"},
                                  Checkbox {.Value = Shared->Notifications(), .OnChanged = [Shared](bool Value) { Shared->Notifications = Value; }, .Label = "Check"},
                                  Switch {.Value = Shared->Autosave(), .OnChanged = [Shared](bool Value) { Shared->Autosave = Value; }, .Label = "Slide"},
                                }}),
                    },
                };
            }

        private:
            Animated<float> _Runners[std::size(kEasings)];
            Animated<float> _Grow;
            Animated<Color> _Tint {Theme::Dark().SurfaceRaised};
            bool _Forward = false;
            bool _Hovered = false;
        };
    };

    std::string DisplayPath(const std::filesystem::path& Path) {
        const std::u8string Text = Path.u8string();
        return {Text.begin(), Text.end()};
    }

    const std::vector<FileFilter> kAudioFilters = {{"Audio files", "*.wav;*.flac;*.mp3;*.ogg"}, {"All files", "*.*"}};

    void ShowPicked(ShowcaseState* Model, std::string Message) {
        Model->Report(Message);
        Model->Picked = std::move(Message);
    }

    void OpenSample(ShowcaseState* Model) {
        if (const auto File = FileDialog::OpenFile({.Title = "Open a sample", .Filters = kAudioFilters})) {
            ShowPicked(Model, "Opened " + DisplayPath(*File));
        } else {
            ShowPicked(Model, "Cancelled opening a file");
        }
    }

    void OpenSamples(ShowcaseState* Model) {
        const std::vector<std::filesystem::path> Files =
          FileDialog::OpenFiles({.Title = "Open samples", .Filters = kAudioFilters});
        if (Files.empty()) {
            ShowPicked(Model, "Cancelled opening files");
        } else {
            ShowPicked(Model,
                       std::format("Opened {} file{}, starting with {}",
                                   Files.size(),
                                   Files.size() == 1 ? "" : "s",
                                   DisplayPath(Files.front().filename())));
        }
    }

    void SaveProject(ShowcaseState* Model) {
        if (const auto File = FileDialog::SaveFile({.Title            = "Save project",
                                                    .FileName         = "Untitled",
                                                    .Filters          = {{"Xen project", "*.xen"}},
                                                    .DefaultExtension = "xen"})) {
            ShowPicked(Model, "Would save to " + DisplayPath(*File));
        } else {
            ShowPicked(Model, "Cancelled saving");
        }
    }

    void PickExportFolder(ShowcaseState* Model) {
        if (const auto Folder = FileDialog::PickFolder({.Title = "Export stems to"})) {
            ShowPicked(Model, "Picked " + DisplayPath(*Folder));
        } else {
            ShowPicked(Model, "Cancelled picking a folder");
        }
    }

    /// @brief The menus in the title bar. Items report what they would do in the status bar.
    std::vector<Menu> ShowcaseMenus(ShowcaseState& State) {
        ShowcaseState* Model = &State;
        const auto Item      = [Model](std::string Label, std::string Shortcut = {}) {
            return MenuItem {.Label = Label, .ShortcutText = std::move(Shortcut), .OnSelect = [Model, Label] {
                                 Model->Report(std::format("Menu: {}", Label));
                             }};
        };
        return {
          Menu {.Label = "File",
                // The dialogs open once the menu has closed: App::Post runs them after the current input.
                .Items = {Item("New", "Ctrl+N"),
                          MenuItem {.Label        = "Open...",
                                    .ShortcutText = "Ctrl+O",
                                    .OnSelect     = [Model] { App::Post([Model] { OpenSample(Model); }); }},
                          MenuItem {.Label        = "Save as...",
                                    .ShortcutText = "Ctrl+Shift+S",
                                    .OnSelect     = [Model] { App::Post([Model] { SaveProject(Model); }); }},
                          MenuItem::Divider(),
                          MenuItem {.Label = "Exit", .OnSelect = [Model] { Model->MainWindow->Close(); }}}},
          Menu {.Label = "Edit",
                .Items = {Item("Undo", "Ctrl+Z"),
                          Item("Redo", "Ctrl+Y"),
                          MenuItem::Divider(),
                          Item("Cut", "Ctrl+X"),
                          Item("Copy", "Ctrl+C"),
                          Item("Paste", "Ctrl+V")}},
          Menu {.Label = "View",
                .Items = {MenuItem {.Label    = "Dark theme",
                                    .OnSelect = [Model] { Model->ThemeMode = 1; },
                                    .Checked  = Model->ThemeMode() == 1},
                          MenuItem {.Label    = "Light theme",
                                    .OnSelect = [Model] { Model->ThemeMode = 2; },
                                    .Checked  = Model->ThemeMode() == 2},
                          MenuItem {.Label    = "Follow Windows",
                                    .OnSelect = [Model] { Model->ThemeMode = 0; },
                                    .Checked  = Model->ThemeMode() == 0}}},
          Menu {.Label = "Help",
                .Items = {MenuItem {.Label        = "Widget inspector",
                                    .ShortcutText = "F12",
                                    .OnSelect     = [Model] { Model->ShowInspector = !Model->ShowInspector.Peek(); },
                                    .Checked      = Model->ShowInspector()},
                          MenuItem {.Label        = "Frame stats",
                                    .ShortcutText = "Shift+F12",
                                    .OnSelect     = [Model] { Model->ShowFrameStats = !Model->ShowFrameStats.Peek(); },
                                    .Checked      = Model->ShowFrameStats()},
                          MenuItem::Divider(),
                          Item("Documentation"),
                          Item("About XenUI")}},
        };
    }

    /// @brief Opens a confirmation dialog and reports the choice.
    struct DialogDemo {
        ShowcaseState* Model = nullptr;

        class State : public ComponentState<DialogDemo> {
        protected:
            Node Build(BuildContext& Context) override {
                _Overlay = OverlayController::Of(Context);
                _Style   = PopupStyle::Capture(Context);
                return Row {.Spacing  = 12.0f,
                            .Children = {Button {.Label = "Delete track...", .OnPressed = [this] { Open(); }}}};
            }

        private:
            void Open() {
                ShowcaseState* Shared = GetWidget().Model;
                _Dialog               = ShowDialog(
                  *_Overlay,
                  _Style,
                  Dialog {
                                  .Title = "Delete track?",
                                  .Content =
                      Text {.Content = "The track and its clips will be removed. You can undo this with Ctrl+Z.",
                                          .Color   = Ink().TextMuted},
                                  .Actions =
                                    {
                        DialogAction {.Label     = "Cancel",
                                                    .OnPressed = [this, Shared] { Finish(Shared, "Kept the track"); }},
                        DialogAction {.Label     = "Delete",
                                                    .OnPressed = [this, Shared] { Finish(Shared, "Deleted the track"); },
                                                    .Variant   = ButtonVariant::Primary},
                      },
                                  .OnDismiss = [this, Shared] { Finish(Shared, "Dismissed the dialog"); },
                  });
            }

            void Finish(ShowcaseState* Shared, const char* Message) {
                Shared->Report(Message);
                _Dialog.Reset();
            }

            OverlayController* _Overlay = nullptr;
            PopupStyle _Style;
            OverlayEntry _Dialog;
        };
    };

    std::vector<TreeItem> SampleTree(const std::string& Parent) {
        if (Parent.empty()) {
            return {{"drums", "Drums", true},
                    {"bass", "Bass", true},
                    {"fx", "Effects", true},
                    {"notes", "Notes.txt", false}};
        }
        if (Parent == "drums") {
            return {{"drums/kicks", "Kicks", true},
                    {"drums/snares", "Snares", true},
                    {"drums/loop", "Break 174.wav", false}};
        }
        if (Parent == "drums/kicks" || Parent == "drums/snares") {
            std::vector<TreeItem> Files;
            const char* Kind = Parent == "drums/kicks" ? "Kick" : "Snare";
            for (int i = 1; i <= 8; ++i) {
                Files.push_back({std::format("{}/{}", Parent, i), std::format("{} {:02}.wav", Kind, i), false});
            }
            return Files;
        }
        if (Parent == "bass") { return {{"bass/sub", "Sub 808.wav", false}, {"bass/reese", "Reese.wav", false}}; }
        if (Parent == "fx") { return {{"fx/riser", "Riser.wav", false}, {"fx/impact", "Impact.wav", false}}; }
        return {};
    }

    struct SampleRow {
        std::string Name;
        int Bpm       = 0;
        float Seconds = 0.0f;
        std::string Key;
    };

    /// @brief A sortable table of generated sample files. The table reports sort requests; this component sorts.
    struct TableDemo {
        ShowcaseState* Model = nullptr;

        class State : public ComponentState<TableDemo> {
        protected:
            void Init() override {
                constexpr const char* kKinds[] = {"Kick", "Snare", "Hat", "Clap", "Bass", "Pad", "Lead", "Vox"};
                constexpr const char* kKeys[]  = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
                for (int i = 0; i < 500; ++i) {
                    _Rows.push_back({std::format("{} {:03}.wav", kKinds[i % 8], i),
                                     80 + (i * 37) % 100,
                                     0.5f + static_cast<float>((i * 53) % 400) / 20.0f,
                                     kKeys[(i * 7) % 12]});
                }
            }

            Node Build(BuildContext& Context) override {
                (void)Context;
                ShowcaseState* Shared = GetWidget().Model;
                return SizedBox {
                  .Height = 260.0f,
                  .Child  = Table {
                     .Columns =
                       {
                        TableColumn {.Title = "Name", .Width = 220.0f, .MinWidth = 80.0f},
                        TableColumn {.Title = "BPM", .Width = 80.0f, .Alignment = TextAlignment::Trailing},
                        TableColumn {.Title = "Length", .Width = 90.0f, .Alignment = TextAlignment::Trailing},
                        TableColumn {.Title = "Key"},
                      },
                     .RowCount = static_cast<int>(_Rows.size()),
                     .CellText = [this](int Row, int Column) -> std::string {
                        const SampleRow& Entry = _Rows[static_cast<size_t>(Row)];
                        switch (Column) {
                            case 0:
                                return Entry.Name;
                            case 1:
                                return std::to_string(Entry.Bpm);
                            case 2:
                                return std::format("{:.1f} s", Entry.Seconds);
                            default:
                                return Entry.Key;
                        }
                    },
                     .OnActivated =
                      [this, Shared](int Row) {
                          Shared->Report(std::format("Opened {}", _Rows[static_cast<size_t>(Row)].Name));
                      },
                     .OnSort = [this](const TableSort& Sort) { Order(Sort); },
                     .Label  = "Samples",
                  },
                };
            }

        private:
            void Order(const TableSort& Sort) {
                const auto Before = [Sort](const SampleRow& Left, const SampleRow& Right) {
                    switch (Sort.Column) {
                        case 1:
                            return Left.Bpm < Right.Bpm;
                        case 2:
                            return Left.Seconds < Right.Seconds;
                        case 3:
                            return Left.Key < Right.Key;
                        default:
                            return Left.Name < Right.Name;
                    }
                };
                std::ranges::stable_sort(_Rows, [&](const SampleRow& Left, const SampleRow& Right) {
                    return Sort.Ascending ? Before(Left, Right) : Before(Right, Left);
                });
                MarkNeedsBuild();
            }

            std::vector<SampleRow> _Rows;
        };
    };

    Node MoreControlsSection(ShowcaseState& State) {
        ShowcaseState* Model                  = &State;
        constexpr const char* kChannelNames[] = {"Mono", "Stereo", "Surround"};
        const auto Caption                    = [](std::string Content) {
            return Text {.Content = std::move(Content), .Font = kSmallFont, .Color = Ink().TextMuted};
        };
        const bool Playing = Model->Playing();
        return Column {
          .Spacing        = 16.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .Children =
            {
              Card("Icons",
                   "Single-color SVG icons. They take the text color unless given one, so they follow button states "
                   "and the theme. Icon-only buttons are named for screen readers by their AccessibleLabel.",
                   Column {
                     .Spacing        = 12.0f,
                     .CrossAlignment = CrossAxisAlignment::Stretch,
                     .MainSize       = MainAxisSize::Min,
                     .Children =
                       {
                         Row {.Spacing  = 4.0f,
                              .Children = {Button {.Icon      = Playing ? kPauseIcon : kPlayIcon,
                                                   .OnPressed = [Model] { Model->Playing = !Model->Playing.Peek(); },
                                                   .Variant   = ButtonVariant::Ghost,
                                                   .AccessibleLabel = Playing ? "Pause" : "Play"},
                                           Button {.Icon = kStopIcon,
                                                   .OnPressed =
                                                     [Model] {
                                                         Model->Playing   = false;
                                                         Model->Recording = false;
                                                     },
                                                   .Variant         = ButtonVariant::Ghost,
                                                   .AccessibleLabel = "Stop"},
                                           Button {
                                             .Icon      = kRecordIcon,
                                             .OnPressed = [Model] { Model->Recording = !Model->Recording.Peek(); },
                                             .Variant =
                                               Model->Recording() ? ButtonVariant::Primary : ButtonVariant::Ghost,
                                             .AccessibleLabel = "Record"},
                                           Button {.Icon      = kLoopIcon,
                                                   .OnPressed = [Model] { Model->Looping = !Model->Looping.Peek(); },
                                                   .Variant = Model->Looping() ? ButtonVariant::Primary : ButtonVariant::Ghost,
                                                   .AccessibleLabel = "Loop"},
                                           SizedBox {.Width = 8.0f},
                                           Caption(std::format("{}{}{}",
                                                               Playing ? "Playing" : "Stopped",
                                                               Model->Recording() ? ", recording" : "",
                                                               Model->Looping() ? ", looping" : ""))}},
                         Row {.Spacing  = 12.0f,
                              .Children = {Button {.Label     = "Open...",
                                                   .Icon      = kFolderIcon,
                                                   .OnPressed = [Model] { Model->Report("Open"); }},
                                           Button {.Label     = "Export",
                                                   .Icon      = kExportIcon,
                                                   .OnPressed = [Model] { Model->Report("Export"); },
                                                   .Variant   = ButtonVariant::Primary},
                                           Button {.Label = "Search", .Icon = kSearchIcon, .Enabled = false}}},
                         Row {.Spacing  = 16.0f,
                              .Children = {Icon {.Source = kSearchIcon},
                                           Icon {.Source = kSearchIcon, .Size = 24.0f},
                                           Icon {.Source = kSearchIcon, .Size = 32.0f},
                                           Icon {.Source = kRecordIcon,
                                                 .Size   = 24.0f,
                                                 .Color  = Ink().Danger,
                                                 .Label  = "Recording"},
                                           Icon {.Source = kLoopIcon, .Size = 32.0f, .Color = Ink().Accent},
                                           Caption("Any size and color, crisp at every DPI")}},
                       },
                   }),
              Card("Radio buttons",
                   "One choice from a few. Click an option, or move with the arrow keys once one has focus.",
                   Row {
                     .Spacing        = 32.0f,
                     .CrossAlignment = CrossAxisAlignment::Start,
                     .Children =
                       {
                         RadioGroup {
                           .Options       = {"Mono", "Stereo", "Surround"},
                           .SelectedIndex = Model->Channels(),
                           .OnChanged     = [Model](int Index) { Model->Channels = Index; },
                           .Label         = "Channels",
                         },
                         RadioGroup {
                           .Options       = {"Mono", "Stereo", "Surround"},
                           .SelectedIndex = Model->Channels(),
                           .OnChanged     = [Model](int Index) { Model->Channels = Index; },
                           .Direction     = Axis::Horizontal,
                           .Spacing       = 16.0f,
                           .Label         = "Channels, in a row",
                         },
                         Caption(std::format("Output: {}", kChannelNames[Model->Channels()])),
                       },
                   }),
              Card("Progress",
                   "Determinate bars animate to new values; indeterminate bars show that work is happening.",
                   Column {
                     .Spacing        = 12.0f,
                     .CrossAlignment = CrossAxisAlignment::Stretch,
                     .MainSize       = MainAxisSize::Min,
                     .Children =
                       {
                         ProgressBar {.Value = Model->Progress(), .Label = "Export"},
                         Row {.Spacing  = 12.0f,
                              .Children = {SizedBox {.Width = 220.0f,
                                                     .Child =
                                                       Slider {.Bind = &Model->Progress, .Label = "Export progress"}},
                                           Caption(std::format("{:.0f}%", Model->Progress() * 100.0f))}},
                         ProgressBar {.Value  = Model->Busy() ? std::nullopt : std::optional<float>(1.0f),
                                      .Height = 4.0f,
                                      .Label  = "Scanning"},
                         Switch {.Value     = Model->Busy(),
                                 .OnChanged = [Model](bool On) { Model->Busy = On; },
                                 .Label     = "Scanning the sample library"},
                       },
                   }),
              Card("Number fields",
                   "Type a value and press Enter, or use Up and Down, the wheel or the step buttons. Shift steps ten "
                   "times further.",
                   Row {
                     .Spacing = 16.0f,
                     .Children =
                       {
                         NumberField {.Bind     = &Model->Tempo,
                                      .Min      = 20.0f,
                                      .Max      = 300.0f,
                                      .Step     = 1.0f,
                                      .Decimals = 1,
                                      .Suffix   = " BPM",
                                      .Label    = "Tempo"},
                         NumberField {.Bind     = &Model->GainDb,
                                      .Min      = -60.0f,
                                      .Max      = 12.0f,
                                      .Step     = 0.5f,
                                      .Decimals = 1,
                                      .Suffix   = " dB",
                                      .Label    = "Gain"},
                         Caption(std::format("{:.1f} BPM at {:+.1f} dB", Model->Tempo(), Model->GainDb())),
                       },
                   }),
              Card("Dialogs",
                   "Modal dialogs dim the window, keep Tab inside themselves and return focus when they close. Enter "
                   "presses the primary action and Escape dismisses.",
                   DialogDemo {Model}),
              Card(
                "Tree view",
                "Children are supplied on demand and rows are virtualized. Arrow keys navigate, Right and Left expand "
                "and collapse, Enter or double-click opens.",
                SizedBox {
                  .Height = 220.0f,
                  .Child  = TreeView {.ChildrenOf = SampleTree,
                                      .OnActivated =
                                       [Model](const std::string& Id) { Model->Report(std::format("Opened {}", Id)); },
                                      .Label = "Sample library"},
                }),
              Card("Tables",
                   "Resizable columns over virtualized rows. Click a header to sort, drag a header divider to resize, "
                   "and use the arrow keys, Page Up and Page Down once a row is selected.",
                   TableDemo {Model}),
              Card(
                "Tabs",
                "Only the selected tab is built. Switch with the mouse, Left and Right on a focused tab, or Ctrl+Tab.",
                SizedBox {
                  .Height = 150.0f,
                  .Child =
                    TabView {
                      .Tabs =
                        {
                          Tab {
                            .Label   = "General",
                            .Content = Padding {.Insets = EdgeInsets::All(12.0f),
                                                .Child =
                                                  Checkbox {.Value = Model->Notifications(),
                                                            .OnChanged = [Model](bool On) { Model->Notifications = On; },
                                                            .Label = "Show notifications"}}},
                          Tab {.Label   = "Audio",
                               .Content = Padding {.Insets = EdgeInsets::All(12.0f),
                                                   .Child  = Row {.Spacing  = 12.0f,
                                                                  .Children = {Caption("Buffer size"),
                                                                               Select {.Options = {"128", "256", "512"},
                                                                                       .SelectedIndex = 1,
                                                                                       .Width         = 100.0f,
                                                                                       .Label = "Buffer size"}}}}},
                          Tab {.Label   = "About",
                               .Content = Padding {.Insets = EdgeInsets::All(12.0f),
                                                   .Child  = Caption(std::format("XenUI {}", XENUI_VERSION_STRING))}},
                        },
                      .Selected = &Model->SettingsTab,
                    },
                }),
            },
        };
    }

    /// @brief Counts through simulated work on a worker thread, which reports each step with App::Post.
    struct BackgroundDemo {
        ShowcaseState* Model = nullptr;

        class State : public ComponentState<BackgroundDemo> {
        public:
            static constexpr int kSteps = 120;

        protected:
            void Dispose() override {
                // Posted steps that are still queued find the token gone; then the worker is stopped and joined.
                _Alive.reset();
                _Worker = {};
            }

            Node Build(BuildContext& Context) override {
                (void)Context;
                const bool Running = _Running();
                return Column {
                  .Spacing        = 12.0f,
                  .CrossAlignment = CrossAxisAlignment::Stretch,
                  .MainSize       = MainAxisSize::Min,
                  .Children =
                    {
                      ProgressBar {.Value = static_cast<float>(_Done()) / kSteps, .Label = "Analysis"},
                      Row {.Spacing  = 12.0f,
                           .Children = {Button {.Label     = "Analyze library",
                                                .OnPressed = [this] { Start(); },
                                                .Variant   = ButtonVariant::Primary,
                                                .Enabled   = !Running},
                                        Button {.Label     = "Cancel",
                                                .OnPressed = [this] { _Worker.request_stop(); },
                                                .Enabled   = Running},
                                        Text {.Content = _Message(), .Font = kSmallFont, .Color = Ink().TextMuted}}},
                    },
                };
            }

        private:
            /// @brief Runs Work on the UI thread unless this component is gone by then. Safe from any thread.
            void Post(std::function<void()> Work) {
                App::Post([Alive = std::weak_ptr(_Alive), Work = std::move(Work)] {
                    if (Alive.lock()) { Work(); }
                });
            }

            void Start() {
                _Running = true;
                _Done    = 0;
                _Message = "Starting...";
                _Worker  = std::jthread([this](const std::stop_token& Stop) {
                    // Only App::Post is called here; the signals are written by the posted work, on the UI thread.
                    for (int Step = 1; Step <= kSteps; ++Step) {
                        if (Stop.stop_requested()) {
                            Post([this, Step] {
                                Finish(std::format("Cancelled after {} of {} samples", Step - 1, kSteps));
                            });
                            return;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                        Post([this, Step] {
                            _Done    = Step;
                            _Message = std::format("Analyzed sample {} of {} on a worker thread", Step, kSteps);
                        });
                    }
                    Post([this] { Finish(std::format("Analyzed all {} samples", kSteps)); });
                });
            }

            void Finish(std::string Message) {
                _Running = false;
                _Message = Message;
                GetWidget().Model->Report(std::move(Message));
            }

            Signal<bool> _Running        = false;
            Signal<int> _Done            = 0;
            Signal<std::string> _Message = std::string("Idle");
            std::shared_ptr<bool> _Alive = std::make_shared<bool>(true);
            // Declared last, so it is stopped and joined before the state above is destroyed.
            std::jthread _Worker;
        };
    };

    void ApplyIcon(ShowcaseState* Model, int Choice) {
        Window& Target = *Model->MainWindow;
        if (Choice == 2) {
            const auto File = FileDialog::OpenFile({.Title = "Choose an icon", .Filters = {{"Icons", "*.ico"}}});
            if (!File) {
                Model->Report("Kept the current icon");
                return;
            }
            try {
                Target.SetIconFile(*File);
                Model->Report("Using the icon " + DisplayPath(File->filename()));
            } catch (const Error& Failure) {
                Model->Report(Failure.what());
                return;
            }
        } else {
            Target.SetIcon(Choice + 1);
            Model->Report(Choice == 0 ? "Using the blue icon (resource 1)" : "Using the mint icon (resource 2)");
        }
        Model->IconChoice = Choice;
    }

    Node SystemSection(ShowcaseState& State) {
        ShowcaseState* Model = &State;
        const auto Caption   = [](std::string Content) {
            return Text {.Content = std::move(Content), .Font = kSmallFont, .Color = Ink().TextMuted};
        };
        return Column {
          .Spacing        = 16.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .Children =
            {
              Card(
                "File dialogs",
                "The system's open, save and folder dialogs. They block until closed, while the window keeps painting. "
                "File > Open and File > Save as use them too.",
                Column {
                  .Spacing        = 12.0f,
                  .CrossAlignment = CrossAxisAlignment::Stretch,
                  .MainSize       = MainAxisSize::Min,
                  .Children =
                    {
                      Row {.Spacing  = 12.0f,
                           .Children = {Button {.Label = "Open file...", .OnPressed = [Model] { OpenSample(Model); }},
                                        Button {.Label = "Open files...", .OnPressed = [Model] { OpenSamples(Model); }},
                                        Button {.Label = "Save as...", .OnPressed = [Model] { SaveProject(Model); }},
                                        Button {.Label     = "Pick folder...",
                                                .OnPressed = [Model] { PickExportFolder(Model); }}}},
                      Text {.Content  = Model->Picked(),
                            .Font     = kSmallFont,
                            .Color    = Ink().TextMuted,
                            .Wrapping = TextWrapping::NoWrap,
                            .Trimming = TextTrimming::Ellipsis},
                    },
                }),
              Card("Background work",
                   "A worker thread does the work and hands each update to the UI thread with App::Post. The UI stays "
                   "responsive, and leaving the page cancels the work.",
                   BackgroundDemo {Model}),
              Card("Closing",
                   "Windows can refuse to close, for example to ask about unsaved changes. This applies to the close "
                   "button, Alt+F4, the taskbar and File > Exit.",
                   Row {.Spacing  = 16.0f,
                        .Children = {Switch {.Value     = Model->ConfirmClose(),
                                             .OnChanged = [Model](bool On) { Model->ConfirmClose = On; },
                                             .Label     = "Ask before closing"},
                                     Button {.Label     = "Close window",
                                             .OnPressed = [Model] { Model->MainWindow->Close(); }}}}),
              Card("Window icon",
                   "Windows use the executable's first icon unless told otherwise. This one has two icon resources; it "
                   "can also load an .ico file. Look at the taskbar or Alt+Tab.",
                   Row {
                     .Spacing        = 32.0f,
                     .CrossAlignment = CrossAxisAlignment::Start,
                     .Children =
                       {
                         RadioGroup {
                           .Options       = {"Blue (resource 1)", "Mint (resource 2)", "From an .ico file..."},
                           .SelectedIndex = Model->IconChoice(),
                           .OnChanged = [Model](int Index) { App::Post([Model, Index] { ApplyIcon(Model, Index); }); },
                           .Label     = "Window icon",
                         },
                         Caption("Choosing a file opens the file dialog."),
                       },
                   }),
            },
        };
    }

    /// @brief Asks before the window closes while "Ask before closing" is on.
    struct CloseGuard {
        ShowcaseState* Model = nullptr;
        Node Child;

        class State : public ComponentState<CloseGuard> {
        protected:
            void Init() override {
                ShowcaseState* Shared = GetWidget().Model;
                Shared->MainWindow->SetOnCloseRequested([this, Shared] {
                    if (!Shared->ConfirmClose.Peek()) { return true; }
                    Ask(Shared);
                    return false;
                });
            }

            Node Build(BuildContext& Context) override {
                _Overlay = OverlayController::Of(Context);
                _Style   = PopupStyle::Capture(Context);
                return GetWidget().Child;
            }

        private:
            void Ask(ShowcaseState* Shared) {
                if (_Asking || _Overlay == nullptr) { return; }
                _Asking           = true;
                const auto Cancel = [this, Shared] {
                    Shared->Report("Kept the window open");
                    _Asking = false;
                    _Dialog.Reset();
                };
                _Dialog =
                  ShowDialog(*_Overlay,
                             _Style,
                             Dialog {
                               .Title   = "Close the showcase?",
                               .Content = Text {.Content = "\"Ask before closing\" is on in the System section.",
                                                .Color   = Ink().TextMuted},
                               .Actions =
                                 {
                                   DialogAction {.Label = "Cancel", .OnPressed = Cancel},
                                   DialogAction {.Label     = "Close",
                                                 .OnPressed = [Shared] { Shared->MainWindow->Destroy(); },
                                                 .Variant   = ButtonVariant::Primary},
                                 },
                               .OnDismiss = Cancel,
                             });
            }

            OverlayController* _Overlay = nullptr;
            PopupStyle _Style;
            OverlayEntry _Dialog;
            bool _Asking = false;
        };
    };

    struct SectionView {
        ShowcaseState* State = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            const int Section = State->Section();
            Node Content;
            switch (Section) {
                case 0:
                    Content = ControlsSection {State};
                    break;
                case 1:
                    Content = TextSection(*State);
                    break;
                case 2:
                    Content = ListSection(*State);
                    break;
                case 3:
                    Content = MenuSection(*State);
                    break;
                case 4:
                    Content = DragSection(*State);
                    break;
                case 5:
                    Content = NativeSection {State};
                    break;
                case 6:
                    Content = LiveSection();
                    break;
                case 7:
                    Content = PaintSection();
                    break;
                case 8:
                    Content = LayoutSection(*State);
                    break;
                case 9:
                    Content = AnimationSection {State};
                    break;
                case 10:
                    Content = MoreControlsSection(*State);
                    break;
                default:
                    Content = SystemSection(*State);
                    break;
            }
            return ScrollView {
              .Child =
                Padding {.Insets = EdgeInsets::All(24.0f),
                         .Child  = Column {.Spacing        = 18.0f,
                                           .CrossAlignment = CrossAxisAlignment::Stretch,
                                           .MainSize       = MainAxisSize::Min,
                                           .Children = {Text {.Content = kSections[Section].data(), .Font = kTitleFont},
                                                        Keyed(Section, Content)}}}};
        }
    };

    struct NavItem {
        ShowcaseState* Model = nullptr;
        int Index            = 0;

        class State : public ComponentState<NavItem> {
        protected:
            Node Build(BuildContext& Context) override {
                (void)Context;
                const NavItem& Props = GetWidget();
                const bool Selected  = Props.Model->Section() == Props.Index;
                return Semantics {
                  .Role     = SemanticsRole::ListItem,
                  .Selected = Selected,
                  .OnSelect = [Model = Props.Model, Index = Props.Index] { Model->Section = Index; },
                  .Child    = MouseRegion {
                       .OnEnter = [this](const PointerEvent&) { _Hovered = true; },
                       .OnExit  = [this](const PointerEvent&) { _Hovered = false; },
                       .Cursor  = CursorShape::Hand,
                       .Child =
                      GestureDetector {
                           .OnTap = [Model = Props.Model,
                                  Index = Props.Index](const TapEvent&) { Model->Section = Index; },
                           .Child =
                          Container {
                               .Height  = 32.0f,
                               .Padding = EdgeInsets::Symmetric(12.0f, 0.0f),
                               .Background =
                              Selected ? Ink().Accent.WithAlpha(0.18f) : (_Hovered() ? Ink().Hover : Ink().Chrome),
                               .CornerRadius = 6.0f,
                               .Child        = Align {.Position = Alignment::CenterLeft(),
                                                      .Child    = Text {.Content = kSections[Props.Index].data(),
                                                                        .Color   = Selected ? Ink().Text : Ink().TextMuted}},
                          },
                      },
                  }};
            }

        private:
            Signal<bool> _Hovered = false;
        };
    };

    struct StatusText {
        const Signal<std::string>* Value = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            return Text {.Content  = Value->Get(),
                         .Font     = kSmallFont,
                         .Color    = Ink().TextMuted,
                         .Wrapping = TextWrapping::NoWrap};
        }
    };

    Node BuildShowcase(ShowcaseState& State);

    /// @brief Picks the theme, provides it and builds the page with it. Rebuilds everything when the choice or the
    /// Windows setting changes.
    struct ShowcaseRoot {
        ShowcaseState* State = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            const int Mode = State->ThemeMode();
            State->ActiveTheme =
              Mode == 0 ? Theme::FromSystem() : Theme::For(Mode == 1 ? Appearance::Dark : Appearance::Light);
            CurrentTheme = &State->ActiveTheme;
            return Provide<Theme> {
              .Value = State->ActiveTheme,
              .Child = DeveloperTools {.Child          = CloseGuard {State, BuildShowcase(*State)},
                                       .ShowInspector  = &State->ShowInspector,
                                       .ShowFrameStats = &State->ShowFrameStats},
            };
        }
    };

    Node BuildShowcase(ShowcaseState& State) {
        NodeList Navigation {Padding {.Insets = {.Left = 12.0f, .Top = 4.0f, .Bottom = 16.0f},
                                      .Child  = Text {.Content = "XenUI", .Font = kTitleFont}}};
        for (int i = 0; i < static_cast<int>(std::size(kSections)); ++i) {
            Navigation.push_back(Keyed(i, NavItem {&State, i}));
        }

        return Provide<TextStyle> {
          .Value = {.Font = {.Size = 13.0f}, .Color = Ink().Text},
          .Child =
            Container {
              .Background = Ink().Background,
              .Child =
                Column {
                  .CrossAlignment = CrossAxisAlignment::Stretch,
                  .Children =
                    {
                      TitleBar {
                        .Title   = "XenUI Showcase",
                        .Menus   = ShowcaseMenus(State),
                        .Actions = {Text {.Content = "Theme", .Font = kSmallFont, .Color = Ink().TextMuted},
                                    Select {.Options       = {"System", "Dark", "Light"},
                                            .SelectedIndex = State.ThemeMode.Peek(),
                                            .OnChanged     = [&State](int Index) { State.ThemeMode = Index; },
                                            .Width         = 96.0f,
                                            .Label         = "Theme"}},
                      },
                      Expanded {.Child =
                                  Row {
                                    .CrossAlignment = CrossAxisAlignment::Stretch,
                                    .Children =
                                      {
                                        Container {.Width      = 210.0f,
                                                   .Padding    = EdgeInsets::All(12.0f),
                                                   .Background = Ink().Chrome,
                                                   .Child =
                                                     Semantics {.Role  = SemanticsRole::List,
                                                                .Label = "Sections",
                                                                .Child = Column {.Spacing = 4.0f,
                                                                                 .CrossAlignment = CrossAxisAlignment::Stretch,
                                                                                 .Children = std::move(Navigation)}}},
                                        Expanded {.Child = SectionView {&State}},
                                      },
                                  }},
                      Container {.Height     = 28.0f,
                                 .Padding    = EdgeInsets::Symmetric(14.0f, 0.0f),
                                 .Background = Ink().Chrome,
                                 .Child =
                                   Align {.Position = Alignment::CenterLeft(), .Child = StatusText {&State.Status}}},
                    },
                },
            },
        };
    }
}  // namespace

int main() {
    try {
        App ShowcaseApp;
        Window MainWindow({.Title = "XenUI Showcase", .ClientSize = {1180.0f, 780.0f}, .CustomTitleBar = true});
        MainWindow.SetOnClosed([&ShowcaseApp] { ShowcaseApp.Quit(); });

        ShowcaseState State;
        State.MainWindow = &MainWindow;
        MainWindow.SetContent(ShowcaseRoot {&State});
        MainWindow.Show();
        return ShowcaseApp.Run();
    } catch (const Error& Exception) {
        std::fprintf(stderr, "Fatal error: %s\n", Exception.what());
        return 1;
    }
}
