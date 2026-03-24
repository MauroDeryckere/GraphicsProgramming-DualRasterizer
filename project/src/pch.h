#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES
#define _CRT_DECLARE_GLOBAL_VARIABLES_DIRECTLY

#include <corecrt_math.h>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <memory>

// SDL Headers
#pragma warning(push)
#pragma warning(disable : 26819) // disable the fallthrough between switch labels warning

#include "SDL.h"
#include "SDL_syswm.h"
#include "SDL_surface.h"
#include "SDL_image.h"

#pragma warning(pop)

// DirectX Headers
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d3dx11effect.h>

#pragma warning(push)
#pragma warning(disable : 26819) // disable the fallthrough between switch labels warning

#include <d3dxGlobal.h>

#pragma warning(pop)

// Framework Headers
#include "Timer.h"
#include "Math.h"

// Console colors
inline constexpr char const* RESET  = "\033[0m";
inline constexpr char const* RED    = "\033[31m";
inline constexpr char const* GREEN  = "\033[32m";
inline constexpr char const* YELLOW = "\033[33m";
