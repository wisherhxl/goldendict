# Qt 6 Article Rich-Text Measurement Worker

## Authority and readiness boundary

Status: documentation awaiting independent audit, 2026-09-10. The user approved
the independent Qt 6 helper-process
direction in workspace `evidence/article-collapse-runtime-decision-20260910.md`.
This reconciles that decision within the existing product CRD, Section 3 and
`CRD-LOOKUP-003`, preserving Sections 10-11 and open R9.8. Only Unit 1 is
Ready for development after documentation delivery; no completion claim or new baseline.

Design branch/worktree: `docs/qt6-richtext-measurement-design` /
`worktrees/docs-qt6-richtext-measurement-design`; fetched clean base and remote:
`739866e34ed5efd085bdc3e70b0fb02edb08d681`. Submodule initialized; no build/install.
The preceding [optional-text unit](article-optional-collapse-counting.md) is
integrated; see `evidence/optional-collapse-counting-20260910/target-delivery.md`.

## Exact behavior and evidence limits

Frozen `article_maker.cc:575-591,641-675` decodes complete dictionary-request
HTML with `QString::fromUtf8(pointer, byte_length)`, applies its optional DIV
loop, then measures `QTextDocumentFragment::fromHtml(...).toPlainText().length()`.
Preserve UTF-16 code units and strict `size > limit`;
do not trim, normalize, substitute the sanitized projection, count graphemes,
or reinterpret malformed UTF-8 through Core's strict decoder. Embedded NUL and
invalid numeric entities require exact differential evidence, not assumptions.
Ordinary optional SPAN text counts for both flags. The DIV loop is case-sensitive,
only removes matches after offset zero, and recognizes nested `<div ` but not
bare `<div>`; malformed closes and an initial zero-position match stop removal.

Workspace evidence (immutable inputs, outside this repository):

- `evidence/collapse-metric-investigation-20260910.md`: full source/data-flow
  analysis, initial participating-dictionary exemption and final sole-heading
  expansion. Error text/headings are outside the measured response fragment.
- `evidence/collapse-metric-oracle-20260910/README.md`: 36 synthetic rows with
  exact input bytes, post-removal UTF-16 and output UTF-16; both optional flags.
- `evidence/optional-collapse-backend-20260910/README.md`: actual frozen DSL
  backend/maker responses; canonical `final-generated/` and `final-oald8/`.
  Generated optional/nonoptional/short lengths are 266/264/8; selected real
  OALD8 length is 89. Their explicit threshold checks are bounded evidence.
- `evidence/collapse-headless-feasibility-20260910/README.md` and its
  `qgui-control/README.md`: Windows Qt 6.11.1 QCore-only PRE crashes with
  `0xC0000005`; identical PRE succeeds under QGuiApplication/offscreen with
  four units. Eight earlier inputs match frozen UTF-16. This is a narrow
  runtime-context control, not complete importer or cross-platform equality.

Defaults off/2000, current preference validation, settings format, sources,
generated indexes, sanitized HTML, structured plain text, provenance, resource
references and control identities stay unchanged by the foundation. No data
migration, precomputed index metadata, new preference or Qt 5 runtime is needed.
Rollback is an audited code delivery; no corpus or stored data is rewritten.

## Existing seams and ownership

`dictionary::Article` is an alias for installed `RuntimeDictionaryArticle`
(`runtime_dictionary_source.h`), not a freely extensible private DTO. DSL's
`Translate` currently passes its generated HTML in `data`; other formats have
their own projections. A Qt 6 Article is not automatically a frozen complete
backend request, even if several Article strings are concatenated.
`ServiceState::Lookup` in `application/dictionary_service.cc` assembles and
deduplicates individual Articles into public `LookupResponse.entries`, losing
raw bytes and dictionary-request boundaries. `DesktopFacadeImpl::ComposeLookupPage`
accepts only that response and forwards it to the private composer, which
currently counts each entry's plain-text scalars. It has no cancellation input.
The injected-service facade constructor also supports external implementations.

Replacing `Utf8CodePointCount` alone cannot restore full product collapse.
Keep the helper foundation independent of public DTOs.
Core's private article/application components retain input selection, dictionary
aggregation, thresholds, exemptions and failure presentation. The helper owns
only bounded legacy-fragment preprocessing and concrete Qt rich-text conversion.
It must not know dictionaries, configuration, indexes, GUI tabs or navigation.

Use a separate `apps/goldendict_richtext_worker` executable for the demonstrated
GUI-runtime/deployment boundary; no new shared module or plugin ABI. Its private
converter uses Qt Core/Gui; it links neither GoldenDict Core nor WebEngine/Widgets.
Later Core infrastructure uses QtCore `QProcess` behind a private standard-C++
request/result boundary. The existing `modules/external/src/external_program_source.cc`
provides shell-free launch, separate pipes and bounded wait-loop precedent;
its configurable command templates/output decoding are not this protocol.
Do not add a dependency from Core to the external-source module or generalize
that adapter into a process framework. A concrete private client suffices;
fault injection uses controlled test executables, not a factory hierarchy.

## Worker contract and untrusted-content boundary

Unit 1 uses one request and one response per process, then exits. No daemon,
pool, socket, environment-based payload, shell, command interpolation or raw
article logging. A fixed little-endian binary request contains magic `GDRM`,
u32 version 1, u32 flags (bit 0 expands optional parts; other bits rejected),
u32 byte length, then exactly that many original HTML bytes followed by EOF.
Admit 0..16 MiB; reject oversized lengths before allocation, partial frames,
trailing bytes and unknown version/flags. Read in bounded chunks.
The 24-byte reply contains `GDRR`, u32 version 1, u32 status, u32 reserved zero,
and u64 UTF-16 count. Status values 0/1/2/3 mean success/invalid request/resource
limit/conversion failure; non-success count is zero. Partial/malformed output is
never a successful metric. Production replies contain no article text.

Decode explicit bytes to QString once, then preserve the frozen removal loop's
semantics with an iterative depth scan rather than unbounded C++ recursion.
Retain its search/removal order and odd nested-tag behavior. Convert with
`QTextDocumentFragment::fromHtml(text, &denying_document).toPlainText()`.
Check the output against 16 Mi UTF-16 units before publishing success.
Exact UTF-16 contents remain observable only through the private converter's
QTest seam; a diagnostic transport returning raw content is unnecessary.

The worker constructs QGuiApplication on its own main thread, forces offscreen
before construction, creates no window, and never changes host/global settings.
Use fixed internal Qt arguments so payload/launcher arguments cannot select
plugins. Restrict plugin/library lookup to the owning resolved/install runtime;
reject unavailable offscreen startup instead of falling back to a display.
Windows launch suppresses a console window; fault-dialog suppression is child
scoped. Linux requires no DISPLAY/Wayland server; its normal product xcb policy
does not apply to this worker. macOS retains the same boundary with separate
execution/packaging acceptance. Qt remains the existing Conan-resolved version.

Qt 6.11.1 `qtextdocument.cpp:2149-2164,2273-2352` shows why an empty working
directory or a default resource callback alone is insufficient: `loadResource`
runs first and can read files/data URLs. Use a worker-private QTextDocument
subclass whose `loadResource` returns a valid empty QByteArray for every type/URL
without calling the base loader. Set an explicit empty per-document provider;
keep no parent/cache/default stylesheet/base URL, preventing default fallbacks.
No network adapter, font download, image decoding, external CSS fetch or host
resource callback is permitted. Inline styles pass unchanged to the importer.
Trusted Qt platform/system-font initialization is distinct from payload-driven
file access. Process separation is crash containment, not an OS security sandbox.

Compare denied image/background/data/file/linked/imported CSS cases against the
actual frozen converter with generated local fixtures and request counters.
Prove no external bytes affect results and no server/file resource is consumed.
If denial changes observable frozen metric behavior, record the exact case as
a compatibility conflict before product hookup; do not weaken the safety rule
or call the altered domain equivalent. The selected runtime decision does not
approve a resource-related behavioral divergence.

## Bounded client and deployment obligations (later units)

Use one invocation-local process per measurement with RAII cleanup, no shared
QProcess across threads and no host QGuiApplication/event-loop requirement.
The later private client receives a trusted absolute worker path, HTML bytes,
optional flag, cancellation callback and steady-clock deadline. Composition
must invoke it from owned asynchronous work, never block the GUI thread.
Use a 10-second total cap including start, writes, conversion and exit, shortened
by the request deadline; check cancellation at most every 50 ms. Drain separate
pipes incrementally, cap stdout at 24 bytes and stderr at 4 KiB, discard raw
stderr, kill on excess/timeout/cancel, and bound reaping to one second. No
automatic retry for the same payload; the next request may start a fresh child.
Prove cleanup on facade retirement/destruction. Before product use, bound
concurrency/queued payload memory and record startup/peak memory and platform
process-memory containment. An input bound is not a heap-amplification bound.

Resolve the helper from trusted composition/install metadata, never PATH, cwd,
dictionary paths or saved user content. Build tests pass `$<TARGET_FILE:...>`;
installed tests use declared bin paths including spaces/non-ASCII. Core-only
consumers receive deployment input through the later approved contract.
CMake owns executable/install/runtime layout and offscreen plugin deployment;
Conan owns dependency policy. Reuse Tiger app/runtime helpers where applicable,
inspect their shared/static plugin behavior, and fail explicitly if the resolved
Qt graph lacks offscreen. Preserve shared/static build support without editing
the frozen Tiger submodule or adding a second Qt dependency.

## Ordered functional units and first-unit acceptance

1. **Executable/protocol/compatibility foundation:** the private converter,
   one-shot worker and bounded process tests; no Core/product consumer. A
   working diagnostic executable is the complete independent outcome.
2. **Private client/lifetime and deployability:** trusted location, bounded
   pipes/start/cancel/deadline/reaping, app-less and QCore-only caller tests,
   crash/hang/flood controls, runtime install/package and missing-plugin cases.
   Review this unit's concrete client composition and resource budgets first.
3. **Raw-response/composition contract:** reconcile actual per-format input,
   request aggregation and retained lifetime with installed API ownership.
   Decide any public contract change explicitly before implementation.
4. **Product hookup:** worker-derived metrics and exact grouping/exemptions,
   strict thresholds/fail-open conversion behavior, async publication/cancellation,
   generated and real backend/cold-warm checks, then matched visible collapse.
   Split further by verified functional boundary if this grows too large.

Unit 1 budget: one app CMake file; `src/main.cpp`, `src/richtext_measurement.h`
and `.cpp` (target 250-350 implementation lines total); two QTest files
(`richtext_measurement_test.cpp`, `richtext_worker_protocol_test.cpp`, target
250-350 test lines); tiny generated fixture tables with provenance; short
testing/build status updates. At most one existing build-registration file if
Tiger discovery requires it. No Core/header/DTO/sanitizer/dependency changes.
Reassess scope before materially exceeding this budget, rather than bundling
the client or product integration. Use the owning worktree's documented Conan
Release setup. Proposed acceptance commands (not run results):

```powershell
./run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release
./run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(richtext_measurement_test|richtext_worker_protocol_test)$' --output-on-failure
./run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 --output-on-failure
python scripts/tests/run_with_conan_test.py
git diff --check
```

The first test compares exact UTF-16 output and count to all 36 existing frozen
synthetic rows, plus new paired whitespace/PRE/CODE/table/list/entity/malformed
UTF-8/NUL/style/resource cases. New golden values require actual frozen runs;
Qt 6 self-comparison is insufficient. An opt-in supplement reads immutable
captured backend bytes, retaining hashes/exact UTF-16 evidence outside Git.
The process test verifies the real executable's framing, every reject class,
zero/maximum bounds, finite timeout, clean exit, no windows and plugin selection.
It kills malformed/hung children and retains no raw payload logs. Windows is
the execution gate; Linux/macOS remain unexecuted. Later deployment includes
clean-PATH relocated install/package tests, missing/incorrect helper/plugin
tests and an installed
Core consumer with no GUI application; Linux also clears display variables.

## Remaining decisions and gates

The runtime architecture is approved; routine framing, one-shot lifetime and
resource denial implement its constraints and need no new product preference.
Unit 1 is Ready after this documentation unit's delivery/integration gates; see
workspace `evidence/richtext-worker-design-20260910/root-readiness.md`.
Later raw transport is a real contract choice: a narrow desktop lookup result
can retain private request context; optional public measurement metadata would
change installed DTOs/precomputation policy; an opaque retained context would
add lifetime/identity semantics. None is selected here, and raw untrusted HTML
must not be smuggled into sanitized fields or a global response-address cache.
Resolve that choice before Unit 3, independently of safe foundation delivery.
Resource metric differences and startup/memory regressions remain evidence
gates. Converter success closes no full metric/R9.8/corpus/platform/cutover gate.
Each unit needs staged-tree completion audit, commit/push and integration audit.
