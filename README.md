# Kernel Generator

Procedural C++ generator for physically inspired bokeh kernels.

The program builds a lens model from a seed, evaluates aperture shape, field position, defocus, chromatic shift, rim energy, dust, bubbles and scratches, then writes a TGA preview image. It also includes discrete sampling data for importance sampling the generated kernel.

## Features

- Deterministic lens generation from a numeric seed.
- Procedural aperture shape with blade count, rounded blades and cat-eye deformation.
- Field-dependent aberration controls for defocus, spherical aberration, coma, astigmatism and trefoil.
- Spectral weighting and chromatic offsets across the kernel.
- Dust, bubbles, streaks and subtle grain for lens character.
- Probability tables for direct kernel sampling.
- Optional kernel bank generation across field, defocus, aperture and focal ranges.

## Examples

Each row uses one fixed optical setup. The columns change only the seed.

| Setup | Seed A | Seed B | Seed C |
| --- | --- | --- | --- |
| Balanced off-axis<br>`field=(0.58, -0.22)`<br>`defocus=0.75 aperture=0.95 focal=0.85` | ![Balanced bokeh kernel seed 1337](docs/examples/balanced_seed_1337.png) | ![Balanced bokeh kernel seed 2401](docs/examples/balanced_seed_2401.png) | ![Balanced bokeh kernel seed 4099](docs/examples/balanced_seed_4099.png) |
| Centered soft focus<br>`field=(0.00, 0.00)`<br>`defocus=0.45 aperture=1.00 focal=1.20` | ![Centered bokeh kernel seed 5189](docs/examples/center_seed_5189.png) | ![Centered bokeh kernel seed 7307](docs/examples/center_seed_7307.png) | ![Centered bokeh kernel seed 9011](docs/examples/center_seed_9011.png) |
| Edge cat-eye<br>`field=(1.00, 0.60)`<br>`defocus=1.05 aperture=0.72 focal=0.65` | ![Edge bokeh kernel seed 1123](docs/examples/edge_seed_1123.png) | ![Edge bokeh kernel seed 6827](docs/examples/edge_seed_6827.png) | ![Edge bokeh kernel seed 9721](docs/examples/edge_seed_9721.png) |
| Heavy reverse defocus<br>`field=(-0.85, 0.35)`<br>`defocus=-1.20 aperture=0.62 focal=0.55` | ![Heavy bokeh kernel seed 2027](docs/examples/heavy_seed_2027.png) | ![Heavy bokeh kernel seed 7561](docs/examples/heavy_seed_7561.png) | ![Heavy bokeh kernel seed 9901](docs/examples/heavy_seed_9901.png) |

## Requirements

- CMake 3.20 or newer.
- A C++17 compiler.

No external runtime libraries are required.

## Build

```powershell
cmake --preset release
cmake --build --preset release
```

Or configure CMake manually:

```powershell
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --config Release
```

## Run

```powershell
.\build\release\bin\kernel_generator.exe 1337 512 512 bokeh_kernel_system.tga
```

Arguments:

```text
kernel_generator [seed] [width] [height] [output.tga] [fieldX] [fieldY] [defocus] [aperture] [focal]
```

Defaults:

- `seed`: `1337`
- `width`: `512`
- `height`: `512`
- `output.tga`: `bokeh_kernel_system.tga`
- `fieldX`: `0.58`
- `fieldY`: `-0.22`
- `defocus`: `0.75`
- `aperture`: `0.95`
- `focal`: `0.85`

## Repository Layout

```text
.
├── bokeh_kernel_system.cpp
├── CMakeLists.txt
├── CMakePresets.json
├── docs/
├── .gitattributes
├── .gitignore
└── README.md
```

## Notes

The current implementation is a standalone executable. The core structures and functions are kept in one translation unit so the kernel model can be moved into a library or renderer integration later without additional dependencies.
