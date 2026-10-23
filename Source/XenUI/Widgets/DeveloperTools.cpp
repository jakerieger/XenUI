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

#include "XenUI/Widgets/DeveloperTools.hpp"
#include "XenUI/Platform/Window.hpp"
#include "XenUI/Render/Semantics.hpp"
#include "XenUI/Tree/Element.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Scroll.hpp"
#include "XenUI/Widgets/Theme.hpp"
#include "XenUI/Widgets/TreeView.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <typeinfo>

namespace XenUI {
    namespace {
        constexpr double kStatsInterval     = 0.25;
        constexpr double kInspectorInterval = 0.5;
        constexpr uint32_t kInitialDepth    = 14;
        constexpr float kBudgetMs           = 1000.0f / 60.0f;
        constexpr size_t kGraphFrames       = 120;
        const Font kMonoFont {.Family = "Consolas", .Size = 11.0f};

        const char* RoleName(SemanticsRole Role) {
            switch (Role) {
                case SemanticsRole::Excluded:
                    return "Excluded";
                case SemanticsRole::Group:
                    return "Group";
                case SemanticsRole::Pane:
                    return "Pane";
                case SemanticsRole::Text:
                    return "Text";
                case SemanticsRole::Image:
                    return "Image";
                case SemanticsRole::Button:
                    return "Button";
                case SemanticsRole::CheckBox:
                    return "CheckBox";
                case SemanticsRole::Switch:
                    return "Switch";
                case SemanticsRole::Slider:
                    return "Slider";
                case SemanticsRole::Edit:
                    return "Edit";
                case SemanticsRole::ComboBox:
                    return "ComboBox";
                case SemanticsRole::List:
                    return "List";
                case SemanticsRole::ListItem:
                    return "ListItem";
                case SemanticsRole::Menu:
                    return "Menu";
                case SemanticsRole::MenuItem:
                    return "MenuItem";
                case SemanticsRole::ToolBar:
                    return "ToolBar";
                case SemanticsRole::RadioButton:
                    return "RadioButton";
                case SemanticsRole::ProgressBar:
                    return "ProgressBar";
                case SemanticsRole::TabList:
                    return "TabList";
                case SemanticsRole::TabItem:
                    return "TabItem";
                case SemanticsRole::Spinner:
                    return "Spinner";
                case SemanticsRole::Dialog:
                    return "Dialog";
                case SemanticsRole::MenuBar:
                    return "MenuBar";
                case SemanticsRole::Tree:
                    return "Tree";
                case SemanticsRole::TreeItem:
                    return "TreeItem";
                case SemanticsRole::Table:
                    return "Table";
                case SemanticsRole::TableRow:
                    return "TableRow";
                case SemanticsRole::ColumnHeader:
                    return "ColumnHeader";
            }
            return "Other";
        }

        std::string FormatExtent(float Value) {
            return std::isfinite(Value) ? std::format("{:g}", Value) : std::string("∞");
        }

        void ReplaceAll(std::string& Text, std::string_view From, std::string_view To) {
            for (size_t At = Text.find(From); At != std::string::npos; At = Text.find(From, At)) {
                Text.replace(At, From.size(), To);
                At += To.size();
            }
        }

        /// @brief The inspector window's content: the element tree beside the selected element's details.
        struct InspectorView {
            InspectorModel* Model = nullptr;

            Node Build(BuildContext& Context) const {
                const Theme Style       = Theme::Of(Context);
                InspectorModel* Shared  = Model;
                const uint64_t Revision = Shared->Inspector.GetRevision();
                (void)Revision;
                const auto& Nodes  = Shared->Inspector.GetNodes();
                const bool Picking = Shared->Picking();

                const auto Muted = [Style](std::string Content) {
                    return Text {.Content  = std::move(Content),
                                 .Font     = Style.SmallFont,
                                 .Color    = Style.TextMuted,
                                 .Wrapping = TextWrapping::NoWrap};
                };

                const Node Toolbar = Container {
                  .Padding    = EdgeInsets::Symmetric(10.0f, 6.0f),
                  .Background = Style.Chrome,
                  .Child =
                    Row {
                      .Spacing = 8.0f,
                      .Children =
                        {
                          Button {.Label     = Picking ? "Click an element..." : "Pick element",
                                  .OnPressed = [Shared] { Shared->Picking = !Shared->Picking.Peek(); },
                                  .Variant   = Picking ? ButtonVariant::Primary : ButtonVariant::Secondary},
                          Button {.Label = "Refresh", .OnPressed = [Shared] { Shared->Inspector.Refresh(); }},
                          Spacer {},
                          Muted(std::format("{} elements · Esc cancels picking", Nodes.size())),
                        },
                    },
                };

                const Node Tree = TreeView {
                  .ChildrenOf =
                    [Shared](const std::string& Parent) {
                        (void)Shared->Inspector.GetRevision();
                        const auto& All = Shared->Inspector.GetNodes();
                        std::vector<TreeItem> Items;
                        if (Parent.empty()) {
                            if (!All.empty()) { Items.push_back({All[0].Id, All[0].Name, !All[0].Children.empty()}); }
                            return Items;
                        }
                        const int Index = Shared->Inspector.IndexOf(Parent);
                        if (Index < 0) { return Items; }
                        for (const int Child : All[static_cast<size_t>(Index)].Children) {
                            const InspectorNode& Entry = All[static_cast<size_t>(Child)];
                            Items.push_back({Entry.Id, Entry.Name, !Entry.Children.empty()});
                        }
                        return Items;
                    },
                  .Selected   = &Shared->Selected,
                  .Expanded   = &Shared->Expanded,
                  .RowContent = [Shared, Muted, Style](const TreeItem& Item) -> Node {
                      const InspectorNode* Entry = Shared->Inspector.Find(Item.Id);
                      std::string Detail;
                      if (Entry != nullptr) {
                          if (!Entry->Key.empty()) { Detail += "key " + Entry->Key + "  "; }
                          if (Entry->Kind == "Render" && Entry->Bounds) {
                              Detail += std::format("{:g} × {:g}", Entry->Bounds->Width, Entry->Bounds->Height);
                          }
                      }
                      return Row {
                        .Spacing = 8.0f,
                        .Children =
                          {
                            Text {.Content  = Item.Label,
                                  .Font     = Style.BodyFont,
                                  .Color    = Entry != nullptr && Entry->Kind == "Render" ? Style.Text : Style.Accent,
                                  .Wrapping = TextWrapping::NoWrap},
                            Muted(Detail),
                          },
                      };
                  },
                  .RowHeight = 22.0f,
                  .Indent    = 8.0f,
                  .Label     = "Element tree",
                };

                NodeList Details;
                const InspectorNode* Selected = Shared->Inspector.Find(Shared->Selected());
                if (Selected == nullptr) {
                    Details.push_back(Muted("Select an element in the tree, or pick one in the window."));
                } else {
                    const auto Field = [&](const char* Label, std::string Value) {
                        Details.push_back(Row {
                          .Spacing        = 12.0f,
                          .CrossAlignment = CrossAxisAlignment::Start,
                          .Children =
                            {
                              SizedBox {.Width = 110.0f, .Child = Muted(Label)},
                              Expanded {.Child =
                                          Text {.Content = std::move(Value), .Font = kMonoFont, .Color = Style.Text}},
                            },
                        });
                    };
                    Details.push_back(Text {.Content = Selected->Name,
                                            .Font    = XenUI::Font {.Size = 16.0f, .Weight = FontWeight::SemiBold},
                                            .Color   = Style.Text});
                    Field("Kind", Selected->Kind);
                    Field("Depth", std::to_string(Selected->Depth));
                    Field("Children", std::to_string(Selected->Children.size()));
                    if (!Selected->Key.empty()) { Field("Key", Selected->Key); }
                    if (!Selected->RenderName.empty()) { Field("Render object", Selected->RenderName); }
                    if (Selected->Bounds) {
                        const Rect& Box = *Selected->Bounds;
                        Field("Position", std::format("{:g}, {:g}", Box.X, Box.Y));
                        Field("Size", std::format("{:g} × {:g}", Box.Width, Box.Height));
                    }
                    if (Selected->Constraints) {
                        const BoxConstraints& Limits = *Selected->Constraints;
                        Field("Width limits",
                              std::format("{} – {}", FormatExtent(Limits.MinWidth), FormatExtent(Limits.MaxWidth)));
                        Field("Height limits",
                              std::format("{} – {}", FormatExtent(Limits.MinHeight), FormatExtent(Limits.MaxHeight)));
                    }
                    if (!Selected->RenderName.empty()) {
                        Field("Repaint boundary", Selected->RepaintBoundary ? "yes" : "no");
                        Field("Dirty",
                              Selected->NeedsLayout ? (Selected->NeedsPaint ? "layout, paint" : "layout")
                                                    : (Selected->NeedsPaint ? "paint" : "no"));
                    }
                    if (!Selected->Semantics.empty()) { Field("Accessibility", Selected->Semantics); }
                    Field("Path", Selected->Id);
                }

                return Column {
                  .CrossAlignment = CrossAxisAlignment::Stretch,
                  .Children =
                    {
                      Toolbar,
                      Expanded {
                        .Child =
                          SplitView {
                            .InitialRatio = 0.62f,
                            .MinFirst     = 220.0f,
                            .MinSecond    = 220.0f,
                            .First        = Padding {.Insets = EdgeInsets::All(6.0f), .Child = Tree},
                            .Second = ScrollView {.Child = Padding {.Insets = EdgeInsets::All(14.0f),
                                                                    .Child  = Column {.Spacing = 8.0f,
                                                                                      .CrossAlignment =
                                                                                        CrossAxisAlignment::Stretch,
                                                                                      .MainSize = MainAxisSize::Min,
                                                                                      .Children = std::move(Details)}}},
                          }},
                    },
                };
            }
        };
    }  // namespace

    std::string WidgetInspector::CleanTypeName(std::string_view Raw) {
        std::string Name(Raw);
        for (const std::string_view Noise :
             {"struct ", "class ", "enum ", "XenUI::", "`anonymous namespace'::", "(anonymous namespace)::"}) {
            ReplaceAll(Name, Noise, "");
        }
        ReplaceAll(Name, " __ptr64", "");
        ReplaceAll(Name, " *", "*");
        ReplaceAll(Name, ",", ", ");
        return Name;
    }

    void WidgetInspector::Refresh() {
        _Nodes.clear();
        _ByBox.clear();
        Element* Root = _Target->GetTree().GetRootElement();

        const std::function<void(Element&, int, std::string)> Visit = [&](Element& Source, int Parent, std::string Id) {
            InspectorNode Entry;
            Entry.Id     = std::move(Id);
            Entry.Parent = Parent;
            Entry.Depth  = Source.GetDepth();
            if (const WidgetType* Type = Source.GetWidget().GetType(); Type != nullptr && Type->Info != nullptr) {
                Entry.Name = CleanTypeName(Type->Info->name());
            }
            if (Source.GetWidget().GetKey().IsValid()) {
                Entry.Key = std::format("{:016x}", Source.GetWidget().GetKey().GetHash());
            }

            const bool OwnsRender = dynamic_cast<RenderElementBase*>(&Source) != nullptr;
            Entry.Kind            = OwnsRender ? "Render" : "Component";
            auto* Box             = dynamic_cast<RenderBox*>(Source.FindRenderObject());
            if (Box != nullptr) {
                Entry.Bounds = Rect::FromOriginSize(Box->GetGlobalOffset(), Box->GetSize());
                if (OwnsRender) {
                    Entry.RenderName      = CleanTypeName(typeid(*Box).name());
                    Entry.Constraints     = Box->GetConstraints();
                    Entry.RepaintBoundary = Box->IsRepaintBoundary();
                    Entry.NeedsLayout     = Box->NeedsLayout();
                    Entry.NeedsPaint      = Box->NeedsPaint();
                    SemanticsProperties Described;
                    if (Box->DescribeSemantics(Described)) {
                        Entry.Semantics = RoleName(Described.Role);
                        if (!Described.Label.empty()) { Entry.Semantics += " \"" + Described.Label + "\""; }
                    }
                }
            }

            const int Index = static_cast<int>(_Nodes.size());
            _Nodes.push_back(std::move(Entry));
            if (Parent >= 0) { _Nodes[static_cast<size_t>(Parent)].Children.push_back(Index); }
            if (OwnsRender && Box != nullptr) { _ByBox[Box] = Index; }

            const std::string Prefix = _Nodes[static_cast<size_t>(Index)].Id;
            int Child                = 0;
            Source.VisitChildren([&](Element& Next) { Visit(Next, Index, Prefix + "/" + std::to_string(Child++)); });
        };
        if (Root != nullptr) { Visit(*Root, -1, "0"); }
        _Revision = _Revision.Peek() + 1;
    }

    const InspectorNode* WidgetInspector::Find(std::string_view Id) const {
        const int Index = IndexOf(Id);
        return Index >= 0 ? &_Nodes[static_cast<size_t>(Index)] : nullptr;
    }

    int WidgetInspector::IndexOf(std::string_view Id) const {
        const auto Found = std::ranges::find_if(_Nodes, [Id](const InspectorNode& Entry) { return Entry.Id == Id; });
        return Found != _Nodes.end() ? static_cast<int>(Found - _Nodes.begin()) : -1;
    }

    std::string WidgetInspector::HitTest(const Vec2& Position) const {
        RenderBox* Content = _Target->GetRootBox();
        if (Content == nullptr) { return {}; }
        HitTestResult Result(Position);
        Content->HitTest(Result, Position - Content->GetGlobalOffset());
        for (const HitTestResult::Entry& Hit : Result.GetEntries()) {
            if (const auto Found = _ByBox.find(Hit.Target); Found != _ByBox.end()) {
                return _Nodes[static_cast<size_t>(Found->second)].Id;
            }
        }
        for (auto Index = static_cast<int>(_Nodes.size()) - 1; Index >= 0; --Index) {
            const InspectorNode& Entry = _Nodes[static_cast<size_t>(Index)];
            if (Entry.Kind == "Render" && Entry.Bounds && Entry.Bounds->Contains(Position)) { return Entry.Id; }
        }
        return {};
    }

    void InspectorModel::Reveal(const std::string& Id) {
        Expanded.Modify([&Id](std::set<std::string>& Open) {
            for (size_t Slash = Id.find('/'); Slash != std::string::npos; Slash = Id.find('/', Slash + 1)) {
                Open.insert(Id.substr(0, Slash));
            }
        });
    }

    void FrameStatsPanel::State::Init() {
        _Refresh = Timer::Every(kStatsInterval, [this] { _Tick = _Tick.Peek() + 1; });
    }

    Node FrameStatsPanel::State::Build(BuildContext& Context) {
        const Theme& Style = Theme::Of(Context);
        (void)_Tick();
        FrameStatistics* Stats = GetWidget().Source;
        if (Stats == nullptr) { return SizedBox {}; }

        const double Now                       = TimerQueue::Get().Now();
        const std::vector<FrameSample> Samples = Stats->GetRecent(kGraphFrames);
        const size_t Fps                       = Stats->CountFramesSince(Now - 1.0);

        FrameSample Average;
        float Worst  = 0.0f;
        size_t Count = 0;
        for (const FrameSample& Sample : Samples) {
            if (Sample.Time < Now - 1.0) { continue; }
            Average.CallbackMs += Sample.CallbackMs;
            Average.BuildMs += Sample.BuildMs;
            Average.LayoutMs += Sample.LayoutMs;
            Average.PaintMs += Sample.PaintMs;
            Average.DrawMs += Sample.DrawMs;
            Average.PresentMs += Sample.PresentMs;
            Worst = std::max(Worst, Sample.GetWorkMs());
            ++Count;
        }
        if (Count > 0) {
            const float Scale = 1.0f / static_cast<float>(Count);
            Average.CallbackMs *= Scale;
            Average.BuildMs *= Scale;
            Average.LayoutMs *= Scale;
            Average.PaintMs *= Scale;
            Average.DrawMs *= Scale;
            Average.PresentMs *= Scale;
        }
        const FrameSample Last = Samples.empty() ? FrameSample {} : Samples.back();

        const auto Line = [&Style](std::string Content, std::optional<Color> Tint = std::nullopt) {
            return Text {.Content  = std::move(Content),
                         .Font     = kMonoFont,
                         .Color    = Tint.value_or(Style.Text),
                         .Wrapping = TextWrapping::NoWrap};
        };
        const Color Good    = Style.Success;
        const Color Warning = Style.Warning;
        const Color Bad     = Style.Danger;
        const Color Grid    = Style.TextMuted.WithAlpha(0.5f);

        return Container {
          .Width        = 300.0f,
          .Padding      = EdgeInsets::All(10.0f),
          .Background   = Style.SurfaceRaised.WithAlpha(0.94f),
          .CornerRadius = 6.0f,
          .BorderColor  = Style.Border,
          .Child =
            Column {
              .Spacing        = 3.0f,
              .CrossAlignment = CrossAxisAlignment::Stretch,
              .MainSize       = MainAxisSize::Min,
              .Children =
                {
                  Line(Count == 0
                         ? std::string("idle: no frames in the last second")
                         : std::format("{} fps   work {:.2f} ms avg, {:.2f} max", Fps, Average.GetWorkMs(), Worst),
                       Count == 0 ? Style.TextMuted
                                  : (Worst <= kBudgetMs * 0.5f ? Good : (Worst <= kBudgetMs ? Warning : Bad))),
                  Line(std::format("build {:5.2f}  layout {:5.2f}  paint {:5.2f}",
                                   Average.BuildMs,
                                   Average.LayoutMs,
                                   Average.PaintMs),
                       Style.TextMuted),
                  Line(std::format("draw  {:5.2f}  present {:4.2f}  callback {:4.2f}",
                                   Average.DrawMs,
                                   Average.PresentMs,
                                   Average.CallbackMs),
                       Style.TextMuted),
                  Line(std::format("last: {} rebuilt, {} laid out, {} painted",
                                   Last.Rebuilt,
                                   Last.LaidOut,
                                   Last.Repainted),
                       Style.TextMuted),
                  Line(std::format("last: {:.0f}% of the window redrawn", Last.Damage * 100.0f), Style.TextMuted),
                  SizedBox {.Height = 4.0f},
                  CustomPaint {
                    .OnPaint =
                      [Samples, Good, Warning, Bad, Grid](Painter& Canvas, const Size& Extent) {
                          const float Scale = Extent.Height / (kBudgetMs * 2.0f);
                          const float Width = Extent.Width / static_cast<float>(kGraphFrames);
                          const float Start = Extent.Width - Width * static_cast<float>(Samples.size());
                          for (size_t i = 0; i < Samples.size(); ++i) {
                              const float Work   = Samples[i].GetWorkMs();
                              const float Height = std::clamp(Work * Scale, 1.0f, Extent.Height);
                              const Color Tint = Work <= kBudgetMs * 0.5f ? Good : (Work <= kBudgetMs ? Warning : Bad);
                              Canvas.FillRect({Start + Width * static_cast<float>(i),
                                               Extent.Height - Height,
                                               std::max(Width - 1.0f, 1.0f),
                                               Height},
                                              Tint);
                          }
                          const float Budget = Extent.Height - kBudgetMs * Scale;
                          Canvas.DrawLine({0.0f, Budget}, {Extent.Width, Budget}, Grid);
                      },
                    .Height = 48.0f,
                  },
                  Line("bars: work per frame; line: 16.7 ms (60 Hz)", Style.TextMuted),
                },
            },
        };
    }

    DeveloperTools::State::State()  = default;
    DeveloperTools::State::~State() = default;

    Signal<bool>& DeveloperTools::State::GetInspectorSwitch() {
        Signal<bool>* External = GetWidget().ShowInspector;
        return External != nullptr ? *External : _InspectorOn;
    }

    Signal<bool>& DeveloperTools::State::GetStatsSwitch() {
        Signal<bool>* External = GetWidget().ShowFrameStats;
        return External != nullptr ? *External : _StatsOn;
    }

    void DeveloperTools::State::Init() {}

    void DeveloperTools::State::Dispose() {
        _Watch.reset();
        _Follow.reset();
        _SyncLater.Cancel();
        _Refresh.Cancel();
        if (_Host != nullptr && _Interceptor != 0) { _Host->GetInput().RemoveKeyInterceptor(_Interceptor); }
        _StatsEntry.Reset();
        _HighlightEntry.Reset();
        _PickEntry.Reset();
        CloseInspector();
    }

    Node DeveloperTools::State::Build(BuildContext& Context) {
        const DeveloperHandle* Handle = Context.Find<DeveloperHandle>();
        _Overlay                      = OverlayController::Of(Context);
        _Style                        = PopupStyle::Capture(Context);
        if (_Model == nullptr && Handle != nullptr && Handle->Host != nullptr) {
            _Host        = Handle->Host;
            _Model       = std::make_unique<InspectorModel>(*_Host);
            _Interceptor = _Host->GetInput().AddKeyInterceptor([this](const KeyEvent& Event) {
                if (!Event.Pressed || Event.Repeat) { return false; }
                if (Event.Code == KeyCode::Escape && _Model->Picking.Peek()) {
                    _Model->Picking = false;
                    return true;
                }
                if (!GetWidget().Shortcuts || Event.Code != KeyCode::F12) { return false; }
                if (Event.Modifiers == KeyModifiers::None) {
                    GetInspectorSwitch() = !GetInspectorSwitch().Peek();
                    return true;
                }
                if (Event.Modifiers == KeyModifiers::Shift) {
                    GetStatsSwitch() = !GetStatsSwitch().Peek();
                    return true;
                }
                return false;
            });
            _Watch.emplace([this] {
                (void)GetInspectorSwitch().Get();
                (void)GetStatsSwitch().Get();
                (void)_Model->Picking.Get();
                _SyncLater = Timer::After(0.0, [this] { Sync(); });
            });
            _Follow.emplace([this] { UpdateHighlight(); });
        }
        return GetWidget().Child;
    }

    void DeveloperTools::State::UpdateHighlight() {
        const std::string& Id = _Model->Selected.Get();
        (void)_Model->Inspector.GetRevision();
        if (_Model->Picking.Peek()) { return; }
        const InspectorNode* Entry = _Model->Inspector.Find(Id);
        _Model->Highlight          = Entry != nullptr ? Entry->Bounds : std::nullopt;
        _Model->HighlightLabel     = Entry != nullptr ? Entry->Name : std::string();
    }

    void DeveloperTools::State::Sync() {
        if (_Overlay == nullptr || _Model == nullptr) { return; }
        const bool WantInspector = GetInspectorSwitch().Peek();
        const bool WantStats     = GetStatsSwitch().Peek();
        const bool Picking       = WantInspector && _Model->Picking.Peek();

        if (WantInspector && _Window == nullptr) { OpenInspector(); }
        if (!WantInspector && _Window != nullptr) { CloseInspector(); }

        if (WantStats && !_StatsEntry.IsShown()) {
            _StatsEntry = _Overlay->Show([Style = _Style, Stats = &_Host->GetFrameStatistics()] {
                return Style.Wrap(
                  IgnorePointer {.Child = Align {.Position = Alignment::TopRight(),
                                                 .Child    = Padding {.Insets = {.Top = 44.0f, .Right = 12.0f},
                                                                      .Child  = FrameStatsPanel {Stats}}}});
            });
        } else if (!WantStats && _StatsEntry.IsShown()) {
            _StatsEntry.Reset();
        }

        if (WantInspector && !_HighlightEntry.IsShown()) {
            _HighlightEntry =
              _Overlay
                ->Show(
                  [Style = _Style, Model = _Model.get()] {
                      const std::optional<Rect> Area = Model->Highlight.Get();
                      const std::string Label        = Model->HighlightLabel.Get();
                      if (!Area) { return Node(IgnorePointer {.Child = SizedBox {}}); }
                      const Color Accent = Style.Palette.Accent;
                      return Node(
                        IgnorePointer {.Child =
                                         Stack {
                                           .Fit = StackFit::Expand,
                                           .Children =
                                             {
                                               Positioned {.Left   = Area->X,
                                                           .Top    = Area->Y,
                                                           .Width  = std::max(Area->Width, 1.0f),
                                                           .Height = std::max(Area->Height, 1.0f),
                                                           .Child = DecoratedBox {.Background = Accent.WithAlpha(0.12f),
                                                                                  .BorderColor = Accent,
                                                                                  .BorderWidth = 1.5f}},
                                               Positioned {.Left  = Area->X,
                                                           .Top   = std::max(0.0f, Area->Y - 20.0f),
                                                           .Child = Container {.Padding =
                                                                                 EdgeInsets::Symmetric(6.0f, 2.0f),
                                                                               .Background   = Accent,
                                                                               .CornerRadius = 3.0f,
                                                                               .Child = Text {.Content = std::format("{"
                                                                                                                     "}"
                                                                                                                     " "
                                                                                                                     " "
                                                                                                                     "{"
                                                                                                                     ":"
                                                                                                                     "g"
                                                                                                                     "}"
                                                                                                                     " "
                                                                                                                     "×"
                                                                                                                     " "
                                                                                                                     "{"
                                                                                                                     ":"
                                                                                                                     "g"
                                                                                                                     "}",
                                                                                                                     Label,
                                                                                                                     Area
                                                                                                                       ->Width,
                                                                                                                     Area
                                                                                                                       ->Height),
                                                                                              .Font    = kMonoFont,
                                                                                              .Color =
                                                                                                Style
                                                                                                  .Palette.AccentText,
                                                                                              .Wrapping =
                                                                                                TextWrapping::NoWrap}}},
                                             },
                                         }});
                  });
        } else if (!WantInspector && _HighlightEntry.IsShown()) {
            _HighlightEntry.Reset();
        }

        if (Picking && !_PickEntry.IsShown()) {
            _Model->Inspector.Refresh();
            _PickEntry = _Overlay->Show([this] {
                return Node(MouseRegion {
                  .Cursor = CursorShape::Crosshair,
                  .Child  = Listener {
                    .OnDown =
                      [this](const PointerEvent& Event) {
                          const std::string Id = _Model->Inspector.HitTest(Event.Position);
                          _Model->Picking      = false;
                          if (!Id.empty()) {
                              _Model->Reveal(Id);
                              _Model->Selected = Id;
                          }
                          UpdateHighlight();
                          if (_Window != nullptr) { _Window->Activate(); }
                      },
                    .OnMove =
                      [this](const PointerEvent& Event) {
                          const InspectorNode* Entry =
                            _Model->Inspector.Find(_Model->Inspector.HitTest(Event.Position));
                          _Model->Highlight      = Entry != nullptr ? Entry->Bounds : std::nullopt;
                          _Model->HighlightLabel = Entry != nullptr ? Entry->Name : std::string();
                      },
                    .Behavior = HitTestBehavior::Opaque,
                    .Child    = SizedBox {},
                  },
                });
            });
        } else if (!Picking && _PickEntry.IsShown()) {
            _PickEntry.Reset();
            UpdateHighlight();
        }
    }

    void DeveloperTools::State::OpenInspector() {
        _Model->Inspector.Refresh();
        if (_Model->Expanded.Peek().empty()) {
            std::set<std::string> Open;
            for (const InspectorNode& Entry : _Model->Inspector.GetNodes()) {
                if (Entry.Depth < kInitialDepth && !Entry.Children.empty()) { Open.insert(Entry.Id); }
            }
            _Model->Expanded = std::move(Open);
        }

        _Window = std::make_unique<Window>(WindowDesc {
          .Title        = "XenUI Inspector",
          .ClientSize   = {980.0f, 680.0f},
          .Background   = _Style.Palette.Background,
          .DarkTitleBar = _Style.Palette.IsDark,
        });
        _Window->SetContent(
          _Style.Wrap(Provide<TextStyle> {.Value = {.Font = _Style.Palette.BodyFont, .Color = _Style.Palette.Text},
                                          .Child = InspectorView {_Model.get()}}));
        _Window->SetOnClosed([this] { _SyncLater = Timer::After(0.0, [this] { GetInspectorSwitch() = false; }); });
        _Window->Show();
        _Refresh = Timer::Every(kInspectorInterval, [this] {
            if (!_Model->Picking.Peek()) { _Model->Inspector.Refresh(); }
        });
    }

    void DeveloperTools::State::CloseInspector() {
        _Refresh.Cancel();
        if (_Model != nullptr) { _Model->Picking = false; }
        if (_Window != nullptr) {
            _Window->SetOnClosed({});
            _Window.reset();
        }
    }
}  // namespace XenUI
