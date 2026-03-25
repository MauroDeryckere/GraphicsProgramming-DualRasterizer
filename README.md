# Dual Rasterizer

A software (CPU) and hardware (GPU) rasterizer built with C++ and DirectX 11. You can switch between both rasterizers at runtime and change various render settings on the fly.

<!-- Add screenshots/GIFs here -->

## Table of Contents
- [About](#about)
- [Features](#features)
  - [Software Rasterizer](#software-rasterizer)
  - [Hardware Rasterizer](#hardware-rasterizer)
  - [Shared](#shared)
- [Controls](#controls)
- [Building](#building)

## About

This project started as a [DAE](https://www.digitalartsandentertainment.be/page/31/Game+Development) course assignment (Graphics Programming). The goal was to implement a software rasterizer and a DirectX 11 hardware rasterizer that can render the same scene, and allow switching between them at runtime.

## Features

### Software Rasterizer
- Full rasterization pipeline on the CPU (vertex transformation, triangle rasterization, depth testing, pixel shading)
- Lambert diffuse + Phong specular shading (BRDF)
- Normal mapping
- Multiple shading/debug modes: observed area, diffuse only, specular only, combined
- Depth buffer visualization
- Bounding box visualization

### Hardware Rasterizer
- DirectX 11 rendering pipeline
- HLSL pixel shading (same lighting model as software)
- Texture sampling modes: point, linear, anisotropic
- Fire mesh with partial coverage (alpha blending)

### Shared
- Toggle between software and hardware rasterizer (F1)
- Cull mode cycling: back, front, none
- Camera with mouse look and keyboard movement
- OBJ mesh loading with diffuse, normal, specular and glossiness textures
- Custom math library (vectors, matrices)

## Controls

**Camera**<br>
WASD / Arrows: Move<br>
LShift: Sprint<br>
RMB: Rotate<br>
LMB: Rotate horizontally, move forward/backward<br>
RMB + LMB: Move up/down<br>

**Settings**<br>
F1: Toggle software/hardware rasterizer<br>
F2: Toggle rotation<br>
F3: Toggle fire mesh (hardware only)<br>
F4: Cycle sampler state (hardware only)<br>
F5: Cycle shading mode (software only)<br>
F6: Toggle normal mapping (software only)<br>
F7: Toggle depth buffer display (software only)<br>
F8: Toggle bounding boxes (software only)<br>
F9: Cycle cull mode<br>
F10: Toggle uniform clear color<br>
F11: Toggle FPS display<br>

## Building

The project uses CMake.

```bash
cmake -S . -B build
cmake --build build
```

Requires the DirectX 11 SDK (Windows).
