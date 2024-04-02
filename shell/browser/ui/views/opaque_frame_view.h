// Copyright 2024 Microsoft GmbH.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_UI_VIEWS_OPAQUE_FRAME_VIEW_H_
#define ELECTRON_SHELL_BROWSER_UI_VIEWS_OPAQUE_FRAME_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "shell/browser/ui/views/frameless_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/window/non_client_view.h"

class CaptionButtonPlaceholderContainer;

namespace views {
class ImageButton;
}  // namespace views

class OpaqueFrameView : public FramelessView {
  METADATA_HEADER(OpaqueFrameView, FramelessView)

 public:
  // Constants used by OpaqueBrowserFrameView as well.
  static const int kContentEdgeShadowThickness;

  // The frame border is only visible in restored mode and is hardcoded to 4 px
  // on each side regardless of the system window border size.  This is
  // overridable by subclasses, so RestoredFrameBorderInsets() should be used
  // instead of using this constant directly.
  static constexpr int kFrameBorderThickness = 4;

  // Constants public for testing only.
  static constexpr int kNonClientExtraTopThickness = 1;
  static const int kTopFrameEdgeThickness;
  static const int kSideFrameEdgeThickness;
  static const int kIconLeftSpacing;
  static const int kIconTitleSpacing;
  static const int kCaptionSpacing;
  static const int kCaptionButtonBottomPadding;

  // Constructs a non-client view for an BrowserFrame.
  OpaqueFrameView();
  OpaqueFrameView(const OpaqueFrameView&) = delete;
  OpaqueFrameView& operator=(const OpaqueFrameView&) = delete;
  ~OpaqueFrameView() override;

  void Init(NativeWindowViews* window, views::Widget* frame) override;

  void InitButtons();

  bool IsFrameCondensed();

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void ResetWindowControls() override;

 protected:
  // views::WindowButtonOrderObserver:
  void OnWindowButtonOrderingChange() override;

  // views::View:
  void Layout(PassKey) override;

 private:
  // Creates and returns an ImageButton with |this| as its listener.
  // Memory is owned by the caller.
  views::Button* CreateImageButton(ViewID view_id,
                                   int accessibility_string_id,
                                   views::Button::PressedCallback callback);

  // Returns the insets from the native window edge to the client view.
  // This does not include any client edge.  If |restored| is true, this is
  // calculated as if the window was restored, regardless of its current
  // node_data.
  gfx::Insets FrameBorderInsets(bool restored) const;

  // Returns the thickness of the border that makes up the window frame edge
  // along the top of the frame. If |restored| is true, this acts as if the
  // window is restored regardless of the actual mode.
  int FrameTopBorderThickness(bool restored) const;

  bool IsFrameCondensed();
  gfx::Insets RestoredFrameBorderInsets() const;
  gfx::Insets RestoredFrameEdgeInsets() const;
  int NonClientExtraTopThickness() const;
  int NonClientTopHeight(bool restored) const;

  void UpdateCaptionButtonPlaceholderContainerBackground();

  void LayoutWindowControlsOverlay();

  // Window controls.
  raw_ptr<views::ImageButton> minimize_button_;
  raw_ptr<views::ImageButton> maximize_button_;
  raw_ptr<views::ImageButton> restore_button_;
  raw_ptr<views::ImageButton> close_button_;

  std::vector<views::FrameButton> leading_buttons_;
  std::vector<views::FrameButton> trailing_buttons_{
      views::FrameButton::kMinimize, views::FrameButton::kMaximize,
      views::FrameButton::kClose};

  // Whether any of the window control buttons were packed on the leading or
  // trailing sides.  This state is only valid while layout is being performed.
  bool placed_leading_button_ = false;
  bool placed_trailing_button_ = false;

  // laying out titlebar elements.
  int available_space_leading_x_ = 0;
  int available_space_trailing_x_ = 0;

  // The size of the window buttons. This does not count labels or other
  // elements that should be counted in a minimal frame.
  int minimum_size_for_buttons_ = 0;

  raw_ptr<ui::NativeTheme> theme_;
  std::unique_ptr<ui::NavButtonProvider> nav_button_provider_;

  base::CallbackListSubscription paint_as_active_changed_subscription_;

  // PlaceholderContainer beneath the controls button for WCO.
  raw_ptr<CaptionButtonPlaceholderContainer>
      caption_button_placeholder_container_;
};

#endif  // ELECTRON_SHELL_BROWSER_UI_VIEWS_OPAQUE_FRAME_VIEW_H_