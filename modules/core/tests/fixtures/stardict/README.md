# Generated StarDict test fixture

The StarDict reader tests generate their `.ifo`, `.idx`, and `.dict` files at
runtime. This keeps binary blobs out of the repository while making every byte
of the fixture deterministic and reviewable.

The fixture is original test data licensed under GPL-3.0-or-later with the
surrounding project. It contains short synthetic English definitions and one
UTF-8 headword. It is not derived from a third-party dictionary.

The initial fixture covers uncompressed StarDict 2.4.2 files with 32-bit index
offsets and `sametypesequence=m`. Compressed data, synonyms, resources, and
other article type sequences require separate compatibility fixtures before
they are enabled.
