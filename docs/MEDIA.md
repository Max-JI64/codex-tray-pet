# Documentation media

`images/actual-tray.png` is the screenshot supplied by the author for this release. It is the only real desktop capture in these docs. No new desktop capture was made.

All other images use synthetic sessions and the application's own icon and panel drawing functions. `tools/render-demo.c` includes the application source to call `makeIcon`, `makeMotionIcon`, `badgeIcon`, `coloredBadgeIcon`, and the shared `drawPanel` renderer. It renders into off-screen Windows bitmap surfaces and never creates a visible window or tray icon, reads account/session files, or calls the running Codex app. `tools/make-doc-images.py` packages those bitmaps as PNGs and looping GIFs.

The panel renderer is shared with the real application. Titles and durations are sample data. `session-examples.png` and the individual state panels show unhovered rows without a cursor. Only `session-hover.png` highlights the first row using the actual gray hover fill and the Windows hand cursor used for clickable titles. The error example deliberately shows the existing blue unread dots: ordinary error rows do not yet have a dedicated error marker. Usage-limit interruptions have their own red `i` marker. The examples use the English default. Users can choose Korean, Japanese, Simplified Chinese, or Spanish in the application settings.

Enlarged GIFs use nearest-neighbor scaling to preserve the native pixel-art shapes. They loop indefinitely with 300ms between frames. The full state gallery has four frames; the working cat has two distinct appearances, and the usage-limit motion has three. Idle, completed, stopped, and detection-uncertain examples remain still.

The unread badge uses the actual default red (`#e5484d`) without the temporary completion pulse. The renderer keeps the opaque badge interior identical across frames. `working-unread.gif` provides a standalone enlarged version. `badge-colors.png` compares all six native presets on the same working cat: charcoal (`#2f3038`), red (`#e5484d`), blue (`#2979ff`), green (`#168b5b`), purple (`#8054d9`), and white (`#ffffff`). The individual `badge-color-0.png` through `badge-color-5.png` files show each preset separately. Numbers, placement, outlines, and text colors come from the application's badge renderer. Checks verify the exact opaque fill colors, the red badge in the encoded gallery GIF, the hovered and unhovered row backgrounds, and the cursor in the standalone hover example.

## Regenerate on Windows

Build the application first. Then run the following with a 64-bit Windows TinyCC executable and a Python installation containing Pillow:

```powershell
& 'C:\tools\tcc\tcc.exe' -o build\render-demo.exe tools\render-demo.c compiler\kernel32-extra.def compiler\shell32.def -luser32 -lgdi32 -lkernel32
& .\build\render-demo.exe
& 'C:\Program Files\Python313\python.exe' .\tools\make-doc-images.py
```

The native renderer writes temporary BMPs under `build`; Python writes the final documentation images under `docs/images`. The packaging script includes the docs, but excludes the renderer executable and temporary bitmaps.

The Python script checks image dimensions, GIF frame counts, infinite looping, 300ms frame durations, and differences in actual pixels. These file checks are not a visual review of the user's desktop.
