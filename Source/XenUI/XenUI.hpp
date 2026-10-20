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

#pragma once

#include "XenUI/Api.hpp"
#include "XenUI/App.hpp"
#include "XenUI/Core/Animation.hpp"
#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Concurrent.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Core/Timer.hpp"
#include "XenUI/Graphics/Brush.hpp"
#include "XenUI/Graphics/DisplayList.hpp"
#include "XenUI/Graphics/Image.hpp"
#include "XenUI/Graphics/Layer.hpp"
#include "XenUI/Graphics/LiveCanvas.hpp"
#include "XenUI/Graphics/Painter.hpp"
#include "XenUI/Graphics/Path.hpp"
#include "XenUI/Graphics/StrokeStyle.hpp"
#include "XenUI/Graphics/Text.hpp"
#include "XenUI/Input/DragDrop.hpp"
#include "XenUI/Input/Events.hpp"
#include "XenUI/Input/Focus.hpp"
#include "XenUI/Input/InputDispatcher.hpp"
#include "XenUI/Input/Shortcuts.hpp"
#include "XenUI/Input/TextEditing.hpp"
#include "XenUI/Input/TextEditingController.hpp"
#include "XenUI/Input/TextInput.hpp"
#include "XenUI/Layout/BoxConstraints.hpp"
#include "XenUI/Platform/Clipboard.hpp"
#include "XenUI/Platform/Window.hpp"
#include "XenUI/Reactive/Reactive.hpp"
#include "XenUI/Reactive/Signal.hpp"
#include "XenUI/Render/RenderBox.hpp"
#include "XenUI/Render/RenderControls.hpp"
#include "XenUI/Render/RenderEditable.hpp"
#include "XenUI/Render/RenderInput.hpp"
#include "XenUI/Render/RenderLayout.hpp"
#include "XenUI/Render/RenderLive.hpp"
#include "XenUI/Render/RenderNativeHost.hpp"
#include "XenUI/Render/RenderWindowChrome.hpp"
#include "XenUI/Render/RenderObject.hpp"
#include "XenUI/Render/RenderOverlay.hpp"
#include "XenUI/Render/RenderScroll.hpp"
#include "XenUI/Render/RenderVisual.hpp"
#include "XenUI/Tree/ElementTree.hpp"
#include "XenUI/Tree/Key.hpp"
#include "XenUI/Tree/Widget.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Controls.hpp"
#include "XenUI/Widgets/DragDrop.hpp"
#include "XenUI/Widgets/Input.hpp"
#include "XenUI/Widgets/Live.hpp"
#include "XenUI/Widgets/NativeHost.hpp"
#include "XenUI/Widgets/Overlay.hpp"
#include "XenUI/Widgets/Scroll.hpp"
#include "XenUI/Widgets/Semantics.hpp"
#include "XenUI/Widgets/TextField.hpp"
#include "XenUI/Widgets/Theme.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"
#include "XenUI/Widgets/WindowChrome.hpp"
