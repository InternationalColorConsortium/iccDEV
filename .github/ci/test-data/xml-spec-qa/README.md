# ICC.2 XML specification QA fixtures

These compact XML profiles are native iccDEV QA inputs. They were written for
this suite rather than copied from the larger `Testing/` or ICC ICS corpora.
Each positive profile uses a distinct, small channel geometry so header and tag
relationships remain reviewable by inspection.

| Fixture | Contract |
|---------|----------|
| `spectral-reflectance-5.xml` | Valid `rs0005` NamedColor profile, 410-690 nm in five steps |
| `bispectral-reflectance-2x3.xml` | Valid `bs0006` NamedColor profile with a two-row by three-column Donaldson matrix |
| `mcs-3-channel-mid.xml` | Valid `mc0003` MultiplexIdentification profile with three channel names and an identity AToM0 calculator |
| `spectral-steps-overflow.xml` | Spectral step count exceeds the unsigned 16-bit header field |
| `bispectral-steps-overflow.xml` | Bi-spectral step count exceeds the unsigned 16-bit header field |
| `spectral-nonfinite-start.xml` | Spectral start wavelength is not finite |
| `bispectral-nonfinite-end.xml` | Bi-spectral end wavelength is not finite |
| `bispectral-channel-mismatch.xml` | `bs0007` conflicts with the declared two-by-three ranges |
| `nonbispectral-with-bi-range.xml` | Normal `rs0002` spectral PCS illegally carries a bi-spectral range |
| `mcs-channel-mismatch.xml` | `mc0002` conflicts with six multiplex channel names |

The positive profiles correspond to ICC.2:2023 clauses 7.2.21 through 7.2.24
and Tables 21 and 22. The negative profiles isolate parser-width and
cross-field validation behavior without duplicating large spectral tables.

## ICS corpus review

The ICC ICS repository corpus was reviewed on 2026-09-22 as a candidate source.
Its 51 XML files contain no bi-spectral or MCS profiles. Its spectral
sources overlap profiles already present under `Testing/`, including exact
copies of the hybrid spectral inputs. The only materially larger spectral
candidate is the approximately 17.8 MB hybrid CMYK profile, which is not a
focused header fixture. Keep the normative ICS package sources in that
repository and use this directory for small, independent parser contracts.

Generated ICC profiles, round-trip XML, JSON reports, and logs belong in
`ICCDEV_TEST_OUTDIR`; do not add them here.
