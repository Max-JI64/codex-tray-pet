# Installation validation

On October 3, 2026 (Asia/Seoul), the author installed the public GitHub v0.1.1 release on the development Windows PC using the documented launchers. This was an actual installation, not a mocked installer test. No screen capture or visual acceptance check was performed.

## Package tested

- Source: [v0.1.1 Windows x64 ZIP](https://github.com/Max-JI64/codex-tray-pet/releases/download/v0.1.1/codex-tray-pet-v0.1.1-windows-x64.zip)
- SHA-256: `ae5401ad685de02438e0942f2f4110e034c194d2dd61d68191243b113274e0be`
- Downloaded without GitHub authentication and checked against the public `SHA256SUMS.txt` and release asset digest.
- Extracted to a permanent directory, replacing the earlier development installation's startup registration.
- Extracted helper scripts were explicitly marked with Internet ZoneId=3 to reproduce the restriction normally associated with a browser download. The download itself used PowerShell; this was not a browser-extraction UI test.

## Checks that passed

| Check | Observed result |
|---|---|
| Old installation removal | Its scheduled task was unregistered and its process stopped. Old files were retained as a backup. |
| `Uninstall.cmd` | Removed the installed task and stopped the running pet. |
| Downloaded `Install.cmd` | Exited successfully with Internet-marked helper scripts under the existing RemoteSigned policy. |
| Startup registration | Task pointed to the extracted executable and working directory, with a user logon trigger, Interactive logon, and Limited run level. |
| Runtime | One pet process remained alive; task state was Running. |
| Codex detection | The new process detected the currently running Codex desktop process and reported detection failure 0. |
| Tray registration | The app reported successful notification-area icon registration. This does not constitute a visual review. |
| Repeat installation | Running `Install.cmd` again kept the same process and did not create a duplicate. |
| Execution policy | All persistent policy settings were identical before and after installation. |
| Windows-only documentation | The downloaded README contained the Windows x64 support notice at the top. |

The v0.1.0 launcher failed before installation when Internet-marked unsigned scripts were subject to RemoteSigned. Version 0.1.1 fixes that by passing `-ExecutionPolicy Bypass` to the install/uninstall PowerShell process only. It does not change machine-wide or user policy settings. Organization-enforced restrictions remain applicable.

## Not verified

Installation on another PC, a real Windows logoff/reboot, the Explorer ZIP extraction workflow, and a real Codex Exit/relaunch were not tested during this validation. Existing user settings were not migrated to the fresh installation. The executable remains unsigned. Internal Codex file changes can affect task detection.
