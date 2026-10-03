# Documentation media

`images/actual-tray.png` is the screenshot supplied by the author for this release. It is the only real desktop capture in these docs. No new desktop capture was made.

All other images use synthetic sessions and the application's own icon and panel drawing functions. `tools/render-demo.c` includes the application source to call `makeIcon`, `makeMotionIcon`, `badgeIcon`, and the shared `drawPanel` renderer. It renders into off-screen Windows bitmap surfaces and never creates a visible window or tray icon, reads account/session files, or calls the running Codex app. `tools/make-doc-images.py` packages those bitmaps as PNGs and looping GIFs.

The panel renderer is shared with the real application. Titles and durations are sample data. Each example highlights the first hovered row using the actual gray hover fill and the Windows hand cursor used for clickable titles. The error example deliberately shows the existing blue unread dots: ordinary error rows do not yet have a dedicated error marker. Usage-limit interruptions have their own red `i` marker. The current interface labels remain Korean.

Enlarged GIFs use nearest-neighbor scaling to preserve the native pixel-art shapes. They loop indefinitely with 300ms between frames. The full state gallery has four frames; the working cat has two distinct appearances, and the usage-limit motion has three. Idle, completed, stopped, and detection-uncertain examples remain still.

The unread badge uses the actual default red (`#e5484d`) without the temporary completion pulse. The earlier demo incorrectly enabled the bright-pulse variant on every nonzero frame, which made the badge look muted. The renderer now keeps the badge pixels identical across frames. `working-unread.gif` provides a standalone enlarged version. Checks verify that the encoded gallery GIF retains a red badge and that the hovered and unhovered rows have their actual background colors.

## Regenerate on Windows

Build the application first. Then run the following with a 64-bit Windows TinyCC executable and a Python installation containing Pillow:

```powershell
& 'C:\tools\tcc\tcc.exe' -o build\render-demo.exe tools\render-demo.c compiler\kernel32-extra.def compiler\shell32.def -luser32 -lgdi32 -lkernel32
& .\build\render-demo.exe
& 'C:\Program Files\Python313\python.exe' .\tools\make-doc-images.py
```

The native renderer writes temporary BMPs under `build`; Python writes the final documentation images under `docs/images`. The packaging script includes the docs, but excludes the renderer executable and temporary bitmaps.

The Python script checks image dimensions, GIF frame counts, infinite looping, 300ms frame durations, and differences in actual pixels. These file checks are not a visual review of the user's desktop.
