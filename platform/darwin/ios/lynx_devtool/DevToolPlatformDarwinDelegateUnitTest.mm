// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <LynxDevtool/DevToolPlatformDarwinDelegate.h>
#import <LynxDevtool/LynxEmulateTouchHelper.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

@interface LynxEmulateTouchHelper (PointerEventInjectionTesting)

- (BOOL)lynx_isPointerEventInjectionAvailable;
- (BOOL)lynx_dispatchPointerEvent:(LynxDevToolPointerEventType)type
                         inWindow:(UIWindow *)window
                        hitTarget:(nullable UIView *)target
                       coordinate:(CGPoint)point;
- (void)lynx_cancelCurrentPointerSequence;

@end

@interface DevToolInsertTextField : UITextField
@end

@implementation DevToolInsertTextField

- (BOOL)isFirstResponder {
  return YES;
}

@end

@interface DevToolWindowInputHelper : LynxEmulateTouchHelper

@property(nonatomic, assign) BOOL dispatchResult;
@property(nonatomic, assign) NSInteger dispatchCount;
@property(nonatomic, assign) NSInteger cancellationCount;
@property(nonatomic, weak, nullable) UIWindow *lastWindow;
@property(nonatomic, weak, nullable) UIView *lastTarget;

@end

@implementation DevToolWindowInputHelper

- (instancetype)initWithLynxView:(nullable LynxView *)view {
  self = [super initWithLynxView:view];
  if (self) {
    _dispatchResult = YES;
  }
  return self;
}

- (BOOL)lynx_isPointerEventInjectionAvailable {
  return YES;
}

- (BOOL)lynx_dispatchPointerEvent:(LynxDevToolPointerEventType)type
                         inWindow:(UIWindow *)window
                        hitTarget:(nullable UIView *)target
                       coordinate:(CGPoint)point {
  self.dispatchCount += 1;
  self.lastWindow = window;
  self.lastTarget = target;
  return self.dispatchResult;
}

- (void)lynx_cancelCurrentPointerSequence {
  self.cancellationCount += 1;
}

@end

namespace {

UIWindow *MakeWindow(CGSize size) {
  UIWindow *window = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, size.width, size.height)];
  window.hidden = NO;
  UIView *root = [[UIView alloc] initWithFrame:window.bounds];
  [window addSubview:root];
  return window;
}

UIView *WindowRootView(UIWindow *window) { return window.subviews.firstObject; }

}  // namespace

@interface DevToolPlatformDarwinDelegateUnitTest : XCTestCase
@end

@implementation DevToolPlatformDarwinDelegateUnitTest

- (void)testInsertTextUsesFirstResponderTextInput {
  UIView *lynxView = [[UIView alloc] initWithFrame:CGRectZero];
  UIView *container = [[UIView alloc] initWithFrame:CGRectZero];
  DevToolInsertTextField *textField = [[DevToolInsertTextField alloc] initWithFrame:CGRectZero];
  textField.text = @"ac";
  textField.selectedTextRange = [textField
      textRangeFromPosition:[textField positionFromPosition:textField.beginningOfDocument offset:1]
                 toPosition:[textField positionFromPosition:textField.beginningOfDocument
                                                     offset:1]];
  [container addSubview:textField];
  [lynxView addSubview:container];

  DevToolPlatformDarwinDelegate *platform =
      [[DevToolPlatformDarwinDelegate alloc] initWithLynxView:(LynxView *)lynxView];

  [platform insertText:@"b"];

  XCTAssertEqualObjects(textField.text, @"abc");
}

- (void)testPointerHelperRejectsNilWindowAndWindowBoundary {
  UIView *detachedView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 20, 20)];
  DevToolWindowInputHelper *helper =
      [[DevToolWindowInputHelper alloc] initWithLynxView:(LynxView *)detachedView];
  XCTAssertFalse([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                  pointerId:1
                                coordinateX:10
                                coordinateY:10]);

  UIWindow *window = MakeWindow(CGSizeMake(200, 100));
  UIView *lynxView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 20, 20)];
  [WindowRootView(window) addSubview:lynxView];
  [helper attachLynxView:(LynxView *)lynxView];

  XCTAssertFalse([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                  pointerId:1
                                coordinateX:-1
                                coordinateY:10]);
  XCTAssertFalse([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                  pointerId:1
                                coordinateX:200
                                coordinateY:10]);
  XCTAssertFalse([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                  pointerId:1
                                coordinateX:10
                                coordinateY:100]);
  XCTAssertEqual(helper.dispatchCount, 0);
}

- (void)testPointerHelperHitTestsFromWindowRootOutsideLynxView {
  UIWindow *window = MakeWindow(CGSizeMake(200, 200));
  UIView *root = WindowRootView(window);
  UIView *lynxView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 40, 40)];
  UIView *hostView = [[UIView alloc] initWithFrame:CGRectMake(100, 100, 40, 40)];
  [root addSubview:lynxView];
  [root addSubview:hostView];
  DevToolWindowInputHelper *helper =
      [[DevToolWindowInputHelper alloc] initWithLynxView:(LynxView *)lynxView];

  XCTAssertTrue([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                 pointerId:1
                               coordinateX:110
                               coordinateY:110]);
  XCTAssertEqual(helper.lastWindow, window);
  XCTAssertEqual(helper.lastTarget, hostView);
  XCTAssertTrue([helper injectPointerEvent:LynxDevToolPointerEventTypeUp
                                 pointerId:1
                               coordinateX:110
                               coordinateY:110]);
}

- (void)testPointerHelperCancelsOnWindowSwitchAndAttach {
  UIWindow *firstWindow = MakeWindow(CGSizeMake(100, 100));
  UIWindow *secondWindow = MakeWindow(CGSizeMake(100, 100));
  UIView *lynxView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 50)];
  [WindowRootView(firstWindow) addSubview:lynxView];
  DevToolWindowInputHelper *helper =
      [[DevToolWindowInputHelper alloc] initWithLynxView:(LynxView *)lynxView];

  XCTAssertTrue([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                 pointerId:1
                               coordinateX:10
                               coordinateY:10]);
  [WindowRootView(secondWindow) addSubview:lynxView];
  XCTAssertFalse([helper injectPointerEvent:LynxDevToolPointerEventTypeMove
                                  pointerId:1
                                coordinateX:12
                                coordinateY:12]);
  XCTAssertEqual(helper.cancellationCount, 1);

  XCTAssertTrue([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                 pointerId:2
                               coordinateX:10
                               coordinateY:10]);
  UIView *replacement = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 50)];
  [WindowRootView(secondWindow) addSubview:replacement];
  [helper attachLynxView:(LynxView *)replacement];
  XCTAssertEqual(helper.cancellationCount, 2);
}

- (void)testPointerHelperCancelsWhenReleaseDispatchFails {
  UIWindow *window = MakeWindow(CGSizeMake(100, 100));
  UIView *lynxView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 50)];
  [WindowRootView(window) addSubview:lynxView];
  DevToolWindowInputHelper *helper =
      [[DevToolWindowInputHelper alloc] initWithLynxView:(LynxView *)lynxView];

  XCTAssertTrue([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                 pointerId:1
                               coordinateX:10
                               coordinateY:10]);
  helper.dispatchResult = NO;
  XCTAssertFalse([helper injectPointerEvent:LynxDevToolPointerEventTypeUp
                                  pointerId:1
                                coordinateX:10
                                coordinateY:10]);
  XCTAssertEqual(helper.cancellationCount, 1);

  helper.dispatchResult = YES;
  XCTAssertTrue([helper injectPointerEvent:LynxDevToolPointerEventTypeDown
                                 pointerId:2
                               coordinateX:10
                               coordinateY:10]);
}

@end
