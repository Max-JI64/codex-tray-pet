# Waiting for approval

Version 0.2.3 adds detection of user approval requests. Earlier versions recognized question tools from rollout logs but did not implement approval waiting. Approvals can exist in the desktop application's live state without appearing in those logs.

The existing native pet now subscribes to state updates for active local sessions through the desktop app's local named pipe. It reads the `waitingOnApproval` runtime flag and pending command, file-change, and permission approval requests. Automatic approval review alone does not count as waiting for a user.

An approval request uses the yellow waiting cat. Its session row keeps the elapsed time and adds a yellow circled `!`; a question keeps the yellow circled `?`. The tray tooltip and settings status say “Waiting for approval” in the selected language. Once the app reports the request resolved, the approval overlay disappears and normal log-based status continues. The monitor cannot approve, reject, interrupt, or start a task.

The connection reads snapshots and patches with a bounded streaming parser. Conversation text and images are skipped rather than copied into a resident JSON tree. There is no additional resident process, no screen scraping, and no external request for status. Subscriptions end when sessions stop being active or the pet disconnects. Existing unread counters, quota warnings, and terminal cleanup remain in place.

This is an internal desktop protocol, currently validated against state-stream version 11 in the Windows Codex desktop app. Unsupported stream versions are ignored; ordinary log monitoring continues if the pipe is unavailable. Future app updates can require adaptation.

## Validation

All nine native suites passed on October 13, 2026 (Asia/Seoul), including the new approval suite. Fixtures cover fragmented IPC framing, yellow-state aggregation, per-session isolation, command/file/permission requests, resolution after allow or reject, question separation, remote-host rejection, unsupported versions, revision-gap recovery, large quoted histories, and disconnection cleanup. Browser localization checks also passed for all five languages.

The updated native executable connected to the running desktop app and received real snapshots and state patches for active sessions. No user approval was granted or rejected during this check, and no live screen capture was made. The approval-dialog transition itself was checked with synthetic protocol fixtures; it was not visually accepted by the author during this validation.

The public v0.2.3 release ZIP was downloaded from GitHub, checked against its published SHA-256 digest, and installed with existing preferences preserved. The installed process connected to the desktop state stream and subscribed to three active local sessions. There was one pet process, no detection failure, and no capacity overflow. Private memory was approximately 1.90 MiB and working set approximately 11.90 MiB at that observation. The startup task points to the v0.2.3 executable. The ZIP checksum is `cd5892d93ab51e1409240a2712277c0984bde8aece4099bca795b769862ac950`.
