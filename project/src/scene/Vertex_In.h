#pragma	once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

namespace mau
{
	struct Vertex_In
	{
		Vector3 position{};
		Vector2 texcoord{};
		Vector3 normal{};
		Vector3 tangent{};
	};

	struct Vertex_Out
	{
		Vector4 position{};
		Vector2 texcoord{};
		Vector3 normal{};
		Vector3 tangent{};
	};
}

