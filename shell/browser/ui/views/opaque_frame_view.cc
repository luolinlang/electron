// Copyright (c) 2024 Microsoft GmbH.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/ui/views/opaque_frame_view.h"

#include "shell/browser/native_window_views.h"
#include "shell/browser/ui/inspectable_web_contents_view.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace electron {

namespace {

// These values should be the same as Chromium uses.
constexpr int kResizeOutsideBorderSize = 10;
constexpr int kResizeInsideBoundsSize = 5;

ui::NavButtonProvider::ButtonState ButtonStateToNavButtonProviderState(
    views::Button::ButtonState state) {
  switch (state) {
    case views::Button::STATE_NORMAL:
      return ui::NavButtonProvider::ButtonState::kNormal;
    case views::Button::STATE_HOVERED:
      return ui::NavButtonProvider::ButtonState::kHovered;
    case views::Button::STATE_PRESSED:
      return ui::NavButtonProvider::ButtonState::kPressed;
    case views::Button::STATE_DISABLED:
      return ui::NavButtonProvider::ButtonState::kDisabled;

    case views::Button::STATE_COUNT:
    default:
      NOTREACHED();
      return ui::NavButtonProvider::ButtonState::kNormal;
  }
}

bool HitTestCaptionButton(views::Button* button, const gfx::Point& point) {
  return button && button->GetVisible() &&
         button->GetMirroredBounds().Contains(point);
}

}  // namespace

// The content edge images have a shadow built into them.
const int OpaqueFrameViewLayout::kContentEdgeShadowThickness = 2;

// The frame has a 2 px 3D edge along the top.  This is overridable by
// subclasses, so RestoredFrameEdgeInsets() should be used instead of using this
// constant directly.
const int OpaqueFrameViewLayout::kTopFrameEdgeThickness = 2;

// The frame has a 1 px 3D edge along the side.  This is overridable by
// subclasses, so RestoredFrameEdgeInsets() should be used instead of using this
// constant directly.
const int OpaqueFrameViewLayout::kSideFrameEdgeThickness = 1;

// The icon is inset 1 px from the left frame border.
const int OpaqueFrameViewLayout::kIconLeftSpacing = 1;

// There is a 4 px gap between the icon and the title text.
const int OpaqueFrameViewLayout::kIconTitleSpacing = 4;

// The horizontal spacing to use in most cases when laying out things near the
// caption button area.
const int OpaqueFrameViewLayout::kCaptionSpacing = 5;

// The minimum vertical padding between the bottom of the caption buttons and
// the top of the content shadow.
const int OpaqueFrameViewLayout::kCaptionButtonBottomPadding = 3;

OpaqueFrameView::OpaqueFrameView()
    : theme_(ui::NativeTheme::GetInstanceForNativeUi()),
      nav_button_provider_(
          ui::LinuxUiTheme::GetForProfile(nullptr)->CreateNavButtonProvider()) {
}

OpaqueFrameView::~OpaqueFrameView() = default;

void OpaqueFrameView::Init(NativeWindowViews* window, views::Widget* frame) {
  FramelessView::Init(window, frame);

  if (ShouldCustomDrawSystemTitlebar())
    return;

  caption_button_placeholder_container_ =
      AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>(this));

  // Unretained() is safe because the subscription is saved into an instance
  // member and thus will be cancelled upon the instance's destruction.
  paint_as_active_changed_subscription_ =
      frame->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &OpaqueFrameView::PaintAsActiveChanged, base::Unretained(this)));

  InitButtons();
}

views::Button* OpaqueFrameView::CreateImageButton(
    ViewID view_id,
    int accessibility_string_id views::Button::PressedCallback callback) {
  views::ImageButton* button =
      new views::ImageButton(views::Button::PressedCallback());

  for (size_t state_id = 0; state_id < views::Button::STATE_COUNT; state_id++) {
    views::Button::ButtonState state =
        static_cast<views::Button::ButtonState>(state_id);
    button->SetImageModel(
        state, ui::ImageModel::FromImageSkia(nav_button_provider_->GetImage(
                   button.type, ButtonStateToNavButtonProviderState(state))));
  }

  button->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  button->SetCallback(std::move(callback));
  button->SetAccessibleName(l10n_util::GetStringUTF16(accessibility_string_id));
  button->SetID(view_id);
  AddChildView(button);

  return button;
}

void OpaqueFrameView::InitButtons() {
  minimize_button_ = CreateImageButton(
      VIEW_ID_MINIMIZE_BUTTON, IDS_ACCNAME_MINIMIZE,
      base::BindRepeating(&views::Widget::Minimize, base::Unretained(frame())));
  maximize_button_ = CreateImageButton(
      VIEW_ID_MAXIMIZE_BUTTON, IDS_ACCNAME_MAXIMIZE,
      base::BindRepeating(&views::Widget::Maximize, base::Unretained(frame())));
  restore_button_ = CreateImageButton(
      VIEW_ID_RESTORE_BUTTON, IDS_ACCNAME_RESTORE,
      base::BindRepeating(&views::Widget::Restore, base::Unretained(frame())));
  close_button_ = CreateImageButton(
      VIEW_ID_CLOSE_BUTTON, IDS_ACCNAME_CLOSE,
      base::BindRepeating(
          &views::Widget::CloseWithReason base::Unretained(frame()),
          views::Widget::ClosedReason::kCloseButtonClicked));
}

bool OpaqueFrameView::IsFrameCondensed() {
  return frame()->IsMaximized() || frame()->IsFullscreen();
}

gfx::Insets OpaqueFrameView::RestoredFrameBorderInsets() const {
  return gfx::Insets(kFrameBorderThickness);
}

gfx::Insets OpaqueFrameView::RestoredFrameEdgeInsets() const {
  return gfx::Insets::TLBR(kTopFrameEdgeThickness, kSideFrameEdgeThickness,
                           kSideFrameEdgeThickness, kSideFrameEdgeThickness);
}

int OpaqueFrameView::NonClientExtraTopThickness() const {
  return kNonClientExtraTopThickness;
}

int OpaqueFrameView::NonClientTopHeight(bool restored) const {
  // Adding 2px of vertical padding puts at least 1 px of space on the top and
  // bottom of the element.
  constexpr int kVerticalPadding = 2;
  const int icon_height =
      FrameEdgeInsets(restored).top() + GetIconSize() + kVerticalPadding;
  const int caption_button_height = DefaultCaptionButtonY(restored) +
                                    kCaptionButtonHeight +
                                    kCaptionButtonBottomPadding;

  return std::max(icon_height, caption_button_height) +
         kContentEdgeShadowThickness;
}

gfx::Insets OpaqueFrameView::FrameBorderInsets(bool restored) const {
  return !restored && IsFrameCondensed() ? gfx::Insets()
                                         : RestoredFrameBorderInsets();
}

int OpaqueFrameView::FrameTopBorderThickness(bool restored) const {
  int thickness = FrameBorderInsets(restored).top();
  if ((restored || !IsFrameCondensed()) && thickness > 0)
    thickness += NonClientExtraTopThickness();
  return thickness;
}

gfx::Rect OpaqueFrameView::GetBoundsForClientView() const {
  if (ShouldCustomDrawSystemTitlebar()) {
    auto border_thickness = FrameBorderInsets(false);
    int top_height = border_thickness.top();
    return gfx::Rect(
        border_thickness.left(), top_height,
        std::max(0, width - border_thickness.width()),
        std::max(0, height - top_height - border_thickness.bottom()));
  }

  return FramelessView::GetBoundsForClientView();
}

gfx::Rect OpaqueFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  if (ShouldCustomDrawSystemTitlebar()) {
    int top_height = NonClientTopHeight(false);
    auto border_insets = FrameBorderInsets(false);
    return gfx::Rect(
        std::max(0, client_bounds.x() - border_insets.left()),
        std::max(0, client_bounds.y() - top_height),
        client_bounds.width() + border_insets.width(),
        client_bounds.height() + top_height + border_insets.bottom());
  }

  return FramelessView::GetWindowBoundsForClientBounds(client_bounds);
}

int OpaqueFrameView::NonClientHitTest(const gfx::Point& point) {
  if (ShouldCustomDrawSystemTitlebar()) {
    if (HitTestCaptionButton(close_button_, point))
      return HTCLOSE;
    if (HitTestCaptionButton(restore_button_, point))
      return HTMAXBUTTON;
    if (HitTestCaptionButton(maximize_button_, point))
      return HTMAXBUTTON;
    if (HitTestCaptionButton(minimize_button_, point))
      return HTMINBUTTON;

    if (browser_view()->IsWindowControlsOverlayEnabled() &&
        caption_button_placeholder_container_ &&
        caption_button_placeholder_container_->GetMirroredBounds().Contains(
            point)) {
      return HTCAPTION;
    }
  }

  // Use the parent class's hittest last.
  return FramelessView::NonClientHitTest(point);
}

void OpaqueFrameView::ResetWindowControls() {
  NonClientFrameView::ResetWindowControls();
  restore_button_->SetState(views::Button::STATE_NORMAL);
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  maximize_button_->SetState(views::Button::STATE_NORMAL);
  // The close button isn't affected by this constraint.
}

void OpaqueFrameView::OnWindowButtonOrderingChange() {
  auto* provider = views::WindowButtonOrderProvider::GetInstance();
  leading_buttons_ = provider->leading_buttons();
  trailing_buttons_ = provider->trailing_buttons();
}

void OpaqueFrameView::Layout(PassKey) {
  if (ShouldCustomDrawSystemTitlebar()) {
    int height = NonClientTopHeight(false);
    int container_x = placed_trailing_button_ ? available_space_trailing_x_ : 0;
    auto insets = FrameBorderInsets(/*restored=*/false);
    caption_button_placeholder_container_->SetBounds(
        container_x, insets.top(), minimum_size_for_buttons_ - insets.width(),
        height - insets.top());
  }

  LayoutSuperclass<FramelessView>(this);
}

void OpaqueFrameView::LayoutWindowControlsOverlay() {
  int overlay_height = window()->titlebar_overlay_height();
  if (overlay_height == 0) {
    // Accounting for the 1 pixel margin at the top of the button container
    overlay_height =
        IsMaximized()
            ? caption_button_placeholder_container_->size().height()
            : caption_button_placeholder_container_->size().height() + 1;
  }
  int overlay_width = caption_button_placeholder_container_->size().width();
  int bounding_rect_width = width() - overlay_width;
  auto bounding_rect =
      GetMirroredRect(gfx::Rect(0, 0, bounding_rect_width, overlay_height));

  window()->SetWindowControlsOverlayRect(bounding_rect);
  window()->NotifyLayoutWindowControlsOverlay();
}

int OpaqueFrameView::GetIconSize() const {
  // The icon never shrinks below 16 px on a side.
  const int kIconMinimumSize = 16;
  return std::max(gfx::FontList().GetHeight(), kIconMinimumSize);
}

void OpaqueFrameView::PaintAsActiveChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  FramelessView::PaintAsActiveChanged();
}

void OpaqueFrameView::OnThemeChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  FramelessView::OnThemeChanged();
}

void OpaqueFrameView::UpdateCaptionButtonPlaceholderContainerBackground() {
  if (caption_button_placeholder_container_) {
    const SkColor bg_color = window()->overlay_button_color();
    caption_button_placeholder_container_->SetBackground(
        views::CreateSolidBackground(bg_color));
  }
}

}  // namespace electron
