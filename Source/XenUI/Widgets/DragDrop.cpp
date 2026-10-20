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

#include "XenUI/Widgets/DragDrop.hpp"
#include "XenUI/Widgets/Basic.hpp"

namespace XenUI {
    namespace {
        constexpr Vec2 kFeedbackOffset {14.0f, 10.0f};
    }  // namespace

    Node DragSource::State::Build(BuildContext& Context) {
        _Overlay                = OverlayController::Of(Context);
        const DragSource& Props = GetWidget();

        RenderDragSource::Callbacks Callbacks {
          .CreateData = [this]() -> std::optional<DragData> {
              const DragSource& Current       = GetWidget();
              std::optional<DragData> Payload = Current.Data ? Current.Data() : std::nullopt;
              if (Payload && _Overlay != nullptr && !Current.Feedback.IsEmpty()) {
                  _Feedback = _Overlay->Show([this] {
                      const Vec2 Position = _Pointer();
                      return Node(IgnorePointer {
                        .Child = Stack {
                          .Fit      = StackFit::Expand,
                          .Children = {Positioned {
                            .Left  = Position.X + kFeedbackOffset.X,
                            .Top   = Position.Y + kFeedbackOffset.Y,
                            .Child = Opacity {.Value = _Accepted() ? 1.0f : 0.55f, .Child = GetWidget().Feedback},
                          }},
                        }});
                  });
              }
              return Payload;
          },
          .Allowed = Props.Allowed,
          .OnMove =
            [this](const Vec2& Position, DropEffect Effect) {
                Batch([&] {
                    _Pointer  = Position;
                    _Accepted = Effect != DropEffect::None;
                });
            },
          .OnExitWindow = [this] { _Feedback.Reset(); },
          .OnEnd =
            [this](DropEffect Result) {
                _Feedback.Reset();
                if (GetWidget().OnDragEnd) { GetWidget().OnDragEnd(Result); }
            },
        };
        return RawDragSource {.Callbacks = std::move(Callbacks), .Behavior = Props.Behavior, .Child = Props.Child};
    }
}  // namespace XenUI
