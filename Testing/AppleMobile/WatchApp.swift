// Copyright (c) 2026 International Color Consortium.
// SPDX-License-Identifier: BSD-3-Clause

import SwiftUI

@main
struct IccDevWatchApp: App {
  @State private var summary = "iccDEV core device tests running..."
  @State private var hasRun = false

  var body: some Scene {
    WindowGroup {
      ScrollView {
        Text(summary)
          .font(.system(.caption2, design: .monospaced))
          .frame(maxWidth: .infinity, alignment: .leading)
          .padding()
      }
      .task {
        guard !hasRun else { return }
        hasRun = true
        summary = IccDevRunCoreSmoke()
      }
    }
  }
}
