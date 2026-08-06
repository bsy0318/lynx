// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
package com.lynx.explorer.routing

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(manifest = Config.NONE, sdk = [28])
class RouteCoordinatorTest {
  class BackActivity : ComponentActivity()

  @Test fun sparklingFailureNeverFallsBackToLynx() {
    var lynxCalls = 0; var sparklingCalls = 0
    val engine = RouteCoordinatorEngine(LaunchDescriptorParser(),
      { lynxCalls++; RouteResult.accepted() },
      { sparklingCalls++; RouteResult.failure("sparkling_launch_failed", "failed") })
    val result = engine.open("hybrid://lynxview_page?bundle=main.lynx.bundle", RequestedRuntime.AUTOMATIC, RouteSource.NATIVE_MODULE)
    assertFalse(result.accepted)
    assertEquals("sparkling_launch_failed", result.code)
    assertEquals(0, lynxCalls)
    assertEquals(1, sparklingCalls)
  }

  @Test fun automaticRawRouteUsesOnlyLynx() {
    var lynxCalls = 0; var sparklingCalls = 0
    val engine = RouteCoordinatorEngine(LaunchDescriptorParser(),
      { lynxCalls++; RouteResult.accepted() },
      { sparklingCalls++; RouteResult.accepted() })
    assertTrue(engine.open("https://example.com/a.bundle", RequestedRuntime.AUTOMATIC, RouteSource.SCANNER).accepted)
    assertEquals(1, lynxCalls); assertEquals(0, sparklingCalls)
  }

  @Test fun navigateBackUsesComponentActivityDispatcher() {
    val activity = BackActivity()
    var dispatched = false
    activity.onBackPressedDispatcher.addCallback(object : OnBackPressedCallback(true) {
      override fun handleOnBackPressed() { dispatched = true }
    })
    assertTrue(RouteCoordinator.navigateBack(activity).accepted)
    assertTrue(dispatched)
  }
}
