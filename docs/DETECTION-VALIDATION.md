# Session discovery validation

Version 0.2.1 fixes active sessions being missed after the monitor has run for a while. Previously, excluded child and guardian agents occupied entries in the 256-entry chat array. Multiple rollouts for the same desktop session also occupied separate entries. Once the array filled, newly resumed sessions were skipped and stale stopped states remained; the capacity flag could keep the cat gray.

Discovery now validates metadata before allocating a desktop-session slot. Excluded and superseded files use a bounded 8 KiB fingerprint cache instead of chat entries. A newer rollout for the same session replaces the older one in its existing slot, preserving the title and position in an open panel. Rollout creation timestamps include milliseconds, so two files created within the same second remain distinguishable. Incomplete metadata is retried.

On October 4, 2026 (Asia/Seoul), all eight native checks passed. The detection check includes:

- 300 excluded child-agent files consume zero desktop-session slots.
- All 256 slots remain available for desktop sessions.
- A resumed session replaces stopped history even when all slots are occupied.
- Title and panel-row index remain stable during replacement.
- An older rollout cannot override the current one after cache eviction.
- Completing the resumed turn produces the completed state rather than capacity uncertainty.
- Incomplete metadata is retried; genuine exhaustion of 256 distinct sessions remains distinguishable.

The fixed executable was also applied to the author's running installation without changing preferences. Before the fix, two desktop sessions were missing and their old stopped rollouts remained selected. After the fix, a fresh diagnostic reported 46 distinct desktop sessions, no capacity overflow, and 245 cached excluded or superseded files. The still-running session was active; the other session, which had completed during diagnosis, was correctly marked finished. Tray registration succeeded and the detection failure code was zero. The process used approximately 2.63 MiB of private memory at that observation.

These are event-log, process, and native-renderer checks. No live screen capture or visual acceptance check was performed. The tests use synthetic fixtures; private session logs and account data are not included in the repository.
