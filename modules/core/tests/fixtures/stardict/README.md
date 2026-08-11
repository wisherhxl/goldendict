# Generated StarDict test fixture

The StarDict reader tests generate their `.ifo`, `.idx`, and `.dict` files at
runtime. This keeps binary blobs out of the repository while making every byte
of the fixture deterministic and reviewable.

The fixture is original test data licensed under GPL-3.0-or-later with the
surrounding project. It contains short synthetic English definitions and one
UTF-8 headword. It is not derived from a third-party dictionary.

The fixture covers uncompressed and gzip/dictzip-compatible `.dict.dz`
StarDict 2.4.2 files with 32-bit index offsets, plain `sametypesequence=m`
articles, raw HTML `sametypesequence=h` articles, an internal dictionary link,
and a resource under the adjacent `res` directory. Synonyms, resource ZIP
archives, and other article type sequences require separate compatibility
fixtures before they are enabled.

The generated application index is not dictionary source data. Tests create it
in a temporary index location, verify reuse and safe stale/corrupt rebuilds,
and discard it with the temporary directory.
