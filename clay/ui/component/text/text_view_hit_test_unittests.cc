// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <memory>

#include "base/include/fml/thread.h"
#include "clay/ui/component/page_view.h"
#include "clay/ui/component/text/text_view.h"
#include "clay/ui/component/view.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace clay {
namespace {

class MountedInlineViewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    page_ = std::make_unique<PageView>(0, nullptr, thread_.GetTaskRunner());
    text_view_ = std::make_unique<TextView>(1, page_.get());
    inline_view_ = std::make_unique<View>(2, page_.get());

    page_->SetBound(0, 0, 300, 100);
    page_->AddChild(text_view_.get());
    text_view_->AddChild(inline_view_.get());
    text_view_->PushInlineViewIndex(2, 0);
  }

  void TearDown() override {
    text_view_->RemoveChild(inline_view_.get());
    page_->RemoveChild(text_view_.get());
  }

  BaseView* HitTextView(const FloatPoint& point,
                        FloatPoint* relative_position) {
    // Use the public BaseView contract while retaining virtual dispatch to the
    // private TextView override.
    BaseView* text_as_base = text_view_.get();
    return text_as_base->GetTopViewToAcceptEvent(point, relative_position);
  }

  fml::Thread thread_{"ui"};
  std::unique_ptr<PageView> page_;
  std::unique_ptr<TextView> text_view_;
  std::unique_ptr<View> inline_view_;
};

}  // namespace

TEST_F(MountedInlineViewTest, UsesMountedBoundsWithoutPlaceholderGeometry) {
  text_view_->SetBound(20, 20, 200, 40);
  inline_view_->SetBound(160, 10, 20, 20);

  FloatPoint relative_position;
  EXPECT_EQ(HitTextView(FloatPoint(190, 40), &relative_position),
            inline_view_.get());
  EXPECT_FLOAT_EQ(relative_position.x(), 10);
  EXPECT_FLOAT_EQ(relative_position.y(), 10);
}

TEST_F(MountedInlineViewTest, RespectsOverflowWhenInlineViewEscapesTextBounds) {
  text_view_->SetBound(20, 20, 100, 20);
  inline_view_->SetBound(90, 0, 20, 20);

  FloatPoint relative_position;
  text_view_->SetOverflow(CSSProperty::OVERFLOW_XY);
  EXPECT_EQ(HitTextView(FloatPoint(125, 30), &relative_position),
            inline_view_.get());
  EXPECT_FLOAT_EQ(relative_position.x(), 15);
  EXPECT_FLOAT_EQ(relative_position.y(), 10);

  text_view_->SetOverflow(CSSProperty::OVERFLOW_HIDDEN);
  EXPECT_EQ(HitTextView(FloatPoint(125, 30), &relative_position), nullptr);

  text_view_->SetOverflow(CSSProperty::OVERFLOW_Y);
  EXPECT_EQ(HitTextView(FloatPoint(125, 30), &relative_position), nullptr);

  text_view_->SetOverflow(CSSProperty::OVERFLOW_X);
  EXPECT_EQ(HitTextView(FloatPoint(125, 30), &relative_position),
            inline_view_.get());
}

TEST_F(MountedInlineViewTest, RespectsVerticalOverflowIndependently) {
  text_view_->SetBound(20, 20, 100, 20);
  inline_view_->SetBound(0, 15, 20, 20);

  FloatPoint relative_position;
  text_view_->SetOverflow(CSSProperty::OVERFLOW_XY);
  EXPECT_EQ(HitTextView(FloatPoint(30, 50), &relative_position),
            inline_view_.get());
  EXPECT_FLOAT_EQ(relative_position.x(), 10);
  EXPECT_FLOAT_EQ(relative_position.y(), 15);

  text_view_->SetOverflow(CSSProperty::OVERFLOW_HIDDEN);
  EXPECT_EQ(HitTextView(FloatPoint(30, 50), &relative_position), nullptr);

  text_view_->SetOverflow(CSSProperty::OVERFLOW_X);
  EXPECT_EQ(HitTextView(FloatPoint(30, 50), &relative_position), nullptr);

  text_view_->SetOverflow(CSSProperty::OVERFLOW_Y);
  EXPECT_EQ(HitTextView(FloatPoint(30, 50), &relative_position),
            inline_view_.get());
}

TEST_F(MountedInlineViewTest, ReturnsDeepestInlineViewDescendantCoordinates) {
  text_view_->SetBound(20, 20, 200, 40);
  inline_view_->SetBound(160, 10, 20, 20);
  auto descendant = std::make_unique<View>(3, page_.get());
  descendant->SetBound(4, 3, 8, 8);
  inline_view_->AddChild(descendant.get());

  FloatPoint relative_position;
  EXPECT_EQ(HitTextView(FloatPoint(186, 35), &relative_position),
            descendant.get());
  EXPECT_FLOAT_EQ(relative_position.x(), 2);
  EXPECT_FLOAT_EQ(relative_position.y(), 2);

  inline_view_->RemoveChild(descendant.get());
}

TEST_F(MountedInlineViewTest, RespectsIntermediateInlineViewAxisClipping) {
  text_view_->SetBound(20, 20, 200, 40);
  text_view_->SetOverflow(CSSProperty::OVERFLOW_XY);
  inline_view_->SetBound(160, 10, 20, 20);
  auto descendant = std::make_unique<View>(3, page_.get());
  descendant->SetBound(18, 3, 8, 8);
  inline_view_->AddChild(descendant.get());

  FloatPoint relative_position;
  inline_view_->SetOverflow(CSSProperty::OVERFLOW_Y);
  EXPECT_EQ(HitTextView(FloatPoint(202, 37), &relative_position),
            text_view_.get());

  inline_view_->SetOverflow(CSSProperty::OVERFLOW_X);
  EXPECT_EQ(HitTextView(FloatPoint(202, 37), &relative_position),
            descendant.get());

  descendant->SetBound(3, 18, 8, 8);
  EXPECT_EQ(HitTextView(FloatPoint(187, 52), &relative_position),
            text_view_.get());

  inline_view_->SetOverflow(CSSProperty::OVERFLOW_Y);
  EXPECT_EQ(HitTextView(FloatPoint(187, 52), &relative_position),
            descendant.get());

  inline_view_->RemoveChild(descendant.get());
}

}  // namespace clay
