# Application languages

The default is English. The first item in the native settings menu is **Language**. Browser settings put the selector above all other sections; select a language and press **Save language**.

| Saved value | Language | Browser locale |
|---|---|---|
| 0 | English | en |
| 1 | 한국어 | ko |
| 2 | 日本語 | ja |
| 3 | 简体中文 | zh-CN |
| 4 | Español | es |

The setting is saved as `language=<value>` in the installation's `pet-settings.txt`. Missing or invalid values fall back to English. Saving icon or display preferences preserves the language. The change updates native labels and the tray tooltip immediately. Browser changes update the page after saving; when a native change is made while the browser is open, returning focus to that page refreshes its language. There is no background translation polling loop.

Menu labels, tooltip text, session-window labels, error dialogs, browser settings, notifications, image rules, and character prompts are translated. Session titles and project names remain as written by the user. File names, URLs, units, and technical motion arrays retain their standard notation. Language options always show their own names so the user can recover from an unfamiliar selection.

## Implementation

`assets/native-translations.json` provides five translations per native string. `tools/generate-localization.cjs` generates `src/language.h` as static UTF-16 tables. The application adds no translation process, network service, or translator library. Browser dictionaries and translated prompts are bundled in `assets/localization.js`, served only when browser settings are used.

`assets/browser-translations.json` supplies the browser labels and messages. Static HTML uses English before preferences load. Text nodes and accessible attributes are switched without replacing the form, so typed character descriptions and chosen files remain intact.

To regenerate and test the translations, use Node.js as a development tool:

```powershell
node tools/generate-localization.cjs
node tools/test-localization.cjs
```

The native `--language-test` checks all five languages, English fallback, saved preferences, restart loading, invalid input, and authenticated language API dispatch. The Node test checks repeated text/attribute switching and prompt specifications without controlling a browser. A separate loopback test exercises real HTTP responses:

```powershell
& 'C:\tools\tcc\tcc.exe' -o build\serve-settings-test.exe tools\serve-settings-test.c compiler\kernel32-extra.def compiler\shell32.def -luser32 -lgdi32 -lkernel32
node tools/test-settings-http.cjs
```

The HTTP fixture creates only a hidden test window, uses its own temporary directory, and never registers a tray icon or reads actual Codex sessions. Development tools are not required to run the installed pet.
