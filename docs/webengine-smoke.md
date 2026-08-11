# Qt WebEngine Smoke Check

The automated `goldendict_webengine_smoke` test starts the real Qt Widgets and
Qt WebEngine stack with the offscreen platform, waits for `loadFinished`, and
verifies the rendered page through `toPlainText`. This catches initialization,
profile, page, and rendering-callback failures without requiring a display.

Generate the deterministic fixture before the manual check:

```sh
python3 modules/core/tests/fixtures/stardict/generate_fixture.py \
  /tmp/goldendict-stardict-fixture
```

Before declaring the Phase 4 first-usable slice complete, run this manual Linux
check from a Release build:

1. Start `goldendict` and choose `/tmp/goldendict-stardict-fixture` with
   **Dictionary Folder...**.
2. Verify that the status reports one loaded dictionary.
3. Look up `example` and confirm `UTF-8: café` is visible.
4. Confirm the embedded fixture image loads through the `goldendict` resource
   scheme.
5. Click the internal dictionary link and confirm it starts another lookup
   without navigating the WebEngine page to a public URL.
6. Confirm an external or malformed link does not navigate inside the view.
7. Start another lookup while one is active, then close the window; confirm
   neither action leaves a hung request or process.

The GUI may parse neither dictionary data nor internal article URLs. The
desktop facade resolves typed article URLs and the headless service supplies
article/resource data. The embedded browser denies all clicked navigation;
only a resolved internal lookup becomes an application command.
