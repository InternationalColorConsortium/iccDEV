# libpng iCCP Fixture Source

`valid-hybrid-rgb.icc.b64` is an ASCII encoding of the durable 3812-byte ICC
control used by the fault matrix. Its decoded SHA-256 is:

```text
42e12f4d6cb959f0242fa740c2547cd371ca60ec72f2ecd0926da035a2bd2f11
```

The QA program verifies that hash, then deterministically creates minimal
one-pixel PNG carriers in its temporary work directory. The generated variants
cover a valid stream, one extra decompressed byte, a bad Adler-32 checksum, a
missing Adler-32 trailer, and a bad iCCP chunk CRC. Generated PNG files are test
outputs and must not be committed.

The fixture originated from the ICC PAWG sample corpus. It is stored as Base64
so every checked-in workflow and test support file remains ASCII.
