# AutoCadCreated

AOI teaching tool. The user draws one sample ROI on an image.
The program finds similar parts, estimates the array pitch, fills missing parts,
and creates inspection rectangles ("CAD objects").

Two vision back ends share one code base: **OpenCV** and **HALCON**.

Status: steps S00–S07 are done (plan, UX spec, Qt UI shell with the image canvas, the core:
ports, use case, job runner, fake back end, dependency check, the presenters:
interaction state machine, undo and redo, object tree, parameter panel; the lattice estimator;
zero-copy image sources for Qt and OpenCV, 12-bit images, JSON recipe and sample library storage;
the OpenCV back end: coarse-to-fine template search and the sample library matcher, an override for the image bit depth).
The OpenCV search needs a `*-opencv` build (`build\release-opencv\aoi.exe --backend opencv`); the default `fake` back end draws a synthetic grid. HALCON comes in S08. See [docs/current-status.md](docs/current-status.md) and [docs/00-master-plan.md](docs/00-master-plan.md).

Build and test on Windows 11 (PowerShell, MSVC 2022, Qt 6.12.0; OpenCV 4.9.0 is optional, see the `release-opencv` preset):

```powershell
. .\scripts\dev\env.ps1                 # enter the dev environment (MSVC, Qt, CMake, Ninja)
cmake --preset release
cmake --build build/release
powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1
build\release\aoi.exe --demo 5000        # synthetic board with 5000 boxes
node tools/check-deps.mjs                # layer dependency check (also runs in CTest)
```

Paths, versions and known pitfalls of this machine are in [AI_CONTEXT.md](AI_CONTEXT.md).
