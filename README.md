# AutoCadCreated

AOI teaching tool. The user draws one sample ROI on an image.
The program finds similar parts, estimates the array pitch, fills missing parts,
and creates inspection rectangles ("CAD objects").

Two vision back ends share one code base: **OpenCV** and **HALCON**.

Status: steps S00–S08 are done (plan, UX spec, Qt UI shell with the image canvas, the core:
ports, use case, job runner, fake back end, dependency check, the presenters:
interaction state machine, undo and redo, object tree, parameter panel; the lattice estimator;
zero-copy image sources for Qt and OpenCV, 12-bit images, JSON recipe and sample library storage;
the OpenCV back end: coarse-to-fine template search and the sample library matcher, an override for the image bit depth;
the HALCON back end: shape-model search, the sample library matcher, a license check with a note for the wrong dongle).
The build uses qmake only ([ADR-018](docs/adr/018-qmake-only-build.md)). OpenCV and HALCON are built in by default:
`AiInspTool.exe` opens TIFF and 16-bit images and has the `fake`, `opencv` and `halcon` back ends.
HALCON needs the HALCON 24.11 runtime and license on the computer (the headers and the import library are in `third_party/halcon`).
The default `fake` back end draws a synthetic grid.
See [docs/current-status.md](docs/current-status.md) and [docs/00-master-plan.md](docs/00-master-plan.md).

Build and test on Windows 11 (MSVC 2022, Qt 6.12.0, OpenCV 4.9.0, HALCON 24.11):

- **Qt Creator**: open `AiInspTool/AiInspTool.pro`, select the Qt 6.12.0 MSVC 2022 64-bit kit (Debug, Release or Profile),
  build, and run `AiInspTool`. The tests are in the Tests pane (Tools › Tests › Run All Tests).
- **Command line** (PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1        # qmake + jom build, then all tests (Release, OpenCV + HALCON)
powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1 -Config Debug -NoHalcon   # other configurations
powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1 -Perf  # performance measurements only
. .\scripts\dev\env.ps1                                              # enter the dev environment (MSVC, qmake, jom)
build\qmake-release\bin\AiInspTool.exe --demo 5000                    # synthetic board with 5000 boxes
node tools/check-deps.mjs                                            # layer dependency check (test.ps1 runs it too)
```

To build without a library, give qmake a variable: `AOI_WITH_OPENCV=OFF` or `AOI_WITH_HALCON=OFF`
(in Qt Creator: Projects › Build › qmake › Additional arguments). If an enabled library is missing, qmake stops with an error.

Paths, versions and known pitfalls of this machine are in [AI_CONTEXT.md](AI_CONTEXT.md).
