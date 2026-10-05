# Tracking cleanup

Version 0.2.2 stops detailed tracking after completion, a terminal error, or a usage-limit interruption. It clears the turn and question identifiers. An unchanged terminal log is checked using file size and modification time instead of being reopened and parsed on every scan.

Unread results retain their title and blue dot until Codex reports them as read. An ordinary error retains its alert until acknowledgement or a new task. A usage-limit interruption retains its resume marker; acknowledging the flashing stops the motion without removing that marker. Codex read acknowledgement or a new task clears the usage marker.

Once a terminal entry has no pending result or alert, its chat slot is removed. The bounded cache retains only a file fingerprint, size, modification time, committed read position, session fingerprint, and small result metadata. A change resumes parsing from the committed position, so a new task appended to the same file is detected without replaying old turns. A late unread-state update can also restore an unread result. The cache is bounded to 1,024 entries (64 KiB); it contains no copied conversation text or images.

Chat storage is committed on demand and unused pages are decommitted after cleanup. Recent directory discovery remains available, and a read-only incremental query of the local thread index also discovers resumed older sessions. Original Codex logs and database records are never deleted or changed.

The eight native checks passed on the author's Windows PC. Targeted fixtures verify terminal-detail cleanup, unchanged-log skipping, slot release after reading, late unread updates, same-file restart without history replay, ordinary-error acknowledgement, quota marker retention, quota acknowledgement, truncated-log recovery, and decommit of unused chat storage. Existing capacity, question, icon, localization, and stream checks also passed. No screen capture was used.
