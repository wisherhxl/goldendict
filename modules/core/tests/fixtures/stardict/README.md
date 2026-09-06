# Generated StarDict test fixture

The StarDict reader tests generate their `.ifo`, `.idx`, and `.dict` files at
runtime. This keeps binary blobs out of the repository while making every byte
of the fixture deterministic and reviewable.

The fixture is original test data licensed under GPL-3.0-or-later with the
surrounding project. It contains short synthetic English definitions and one
UTF-8 headword. It is not derived from a third-party dictionary.

The fixture covers plain and gzip/dictzip-compatible index, dictionary, and
synonym companions with the pinned name, case, and precedence variants for
StarDict files with retained 32-bit index offsets. Index variants exercise
declared-versus-parsed counts, advisory sizes, truncated tails, invalid
ranges, HTML-escaped headwords, synonym ownership/filtering, metadata text,
the classic Qt named/numeric entity behavior, and filename/book-name language
inference including unknown-code fallback. It also covers plain
`sametypesequence=m` articles, raw HTML `sametypesequence=h` articles, an
internal dictionary link, and a resource under the adjacent `res` directory.
Resource ZIP archives and other article type sequences require separate
compatibility fixtures before they are enabled.

The generated application index is not dictionary source data. Tests create it
in a temporary index location, verify reuse and safe stale/corrupt rebuilds,
verify the `stardict-records-v3` to `stardict-records-v4` transition and
StarDict full-text semantics invalidation, and discard it with the temporary
directory.

The standalone generator also writes the exact
`.goldendict-disposable-acceptance-v1` marker. This authorizes the bounded
changed-source and unavailable-companion acceptance adapter to mutate only a
generated copy outside the repository. The marker never authorizes mutation of
operator-provided dictionaries.

For the manual Qt WebEngine check, generate a persistent copy outside the
source tree:

```sh
python3 modules/core/tests/fixtures/stardict/generate_fixture.py \
  /tmp/goldendict-stardict-fixture
```

Choose `/tmp/goldendict-stardict-fixture` in the application and look up
`example`. Regenerating the same directory produces byte-identical dictionary
source and resource files.
