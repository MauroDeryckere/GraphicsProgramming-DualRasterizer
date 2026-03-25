#pragma once
#include <fstream>
#include "Math.h"
#include "Mesh.h"

namespace mau
{
	namespace Utils
	{

		[[nodiscard]] inline float CalculateObservedArea(const Vector3& normal, const Vector3& lightDirection) noexcept
		{
			float const observedArea{ Vector3::Dot(normal, -lightDirection) };
			if (observedArea < 0.f)
				return 0.f;

			return observedArea;
		}

		[[nodiscard]] inline bool IsPixelInTriangle(Vector2 pixel, std::vector<Vector2> const& triangle) noexcept
		{
			for (uint32_t i{ 0 }; i < triangle.size(); ++i)
			{
				auto const e{ triangle[(i + 1) % triangle.size()] - triangle[i] };
				auto const p = pixel - triangle[i];

				if (Vector2::Cross(e, p) < 0.f)
				{
					return false;
				}
			}
			return true;
		}

		// --- Frustum testing and clipping (clip space: -w <= x,y <= w, 0 <= z <= w) ---

		// Trivial reject: all 3 vertices on the wrong side of the same frustum plane
		[[nodiscard]] inline bool IsTriangleOutsideFrustum(Vector4 const& v1, Vector4 const& v2, Vector4 const& v3) noexcept
		{
			// Near: z >= 0
			if (v1.z < 0 && v2.z < 0 && v3.z < 0)
				return true;
			// Far: z <= w
			if (v1.z > v1.w && v2.z > v2.w && v3.z > v3.w)
				return true;
			// Left: x >= -w
			if (v1.x < -v1.w && v2.x < -v2.w && v3.x < -v3.w)
				return true;
			// Right: x <= w
			if (v1.x > v1.w && v2.x > v2.w && v3.x > v3.w)
				return true;
			// Bottom: y >= -w
			if (v1.y < -v1.w && v2.y < -v2.w && v3.y < -v3.w)
				return true;
			// Top: y <= w
			if (v1.y > v1.w && v2.y > v2.w && v3.y > v3.w)
				return true;

			return false;
		}

		// Trivial accept: all 3 vertices inside all 6 frustum planes
		[[nodiscard]] inline bool IsTriangleInsideFrustum(Vector4 const& v1, Vector4 const& v2, Vector4 const& v3) noexcept
		{
			auto inside = [](Vector4 const& v) {
				return v.z >= 0 && v.z <= v.w
					&& v.x >= -v.w && v.x <= v.w
					&& v.y >= -v.w && v.y <= v.w;
			};
			return inside(v1) && inside(v2) && inside(v3);
		}

		// Fixed-capacity polygon - lives entirely on the stack, no heap allocations.
		// A triangle clipped against 6 planes produces at most 9 vertices.
		uint8_t inline constexpr MAX_CLIP_VERTS{ 12 }; // some headroom

		struct ClipPolygon
		{
			Vertex_Out verts[MAX_CLIP_VERTS];
			uint8_t count{ 0 };

			void Add(Vertex_Out const& v) noexcept { assert(count < MAX_CLIP_VERTS); verts[count++] = v; }
			void Clear() noexcept { count = 0; }
		};

		// Linearly interpolate two Vertex_Out at parameter t
		[[nodiscard]] inline Vertex_Out LerpVertex(Vertex_Out const& a, Vertex_Out const& b, float t) noexcept
		{
			Vertex_Out result{};
			float const oneMinusT{ 1.f - t };
			result.position = a.position * oneMinusT + b.position * t;
			result.texcoord = a.texcoord * oneMinusT + b.texcoord * t;
			result.normal = a.normal * oneMinusT + b.normal * t;
			result.tangent = a.tangent * oneMinusT + b.tangent * t;
			return result;
		}

		// Sutherland-Hodgman clip against a single plane defined by a distance function.
		// distFunc(v) >= 0 means inside. All stack-allocated.
		template<typename DistFunc>
		inline void ClipAgainstPlane(ClipPolygon& polygon, DistFunc distFunc) noexcept
		{
			if (polygon.count < 3) return;

			ClipPolygon output;

			for (uint8_t i{ 0 }; i < polygon.count; ++i)
			{
				Vertex_Out const& current{ polygon.verts[i] };
				Vertex_Out const& next{ polygon.verts[(i + 1) % polygon.count] };

				float const dCurrent{ distFunc(current.position) };
				float const dNext{ distFunc(next.position) };

				bool const currentInside{ dCurrent >= 0.f };
				bool const nextInside{ dNext >= 0.f };

				if (currentInside)
				{
					output.Add(current);
					if (!nextInside)
					{
						float const t{ dCurrent / (dCurrent - dNext) };
						output.Add(LerpVertex(current, next, t));
					}
				}
				else if (nextInside)
				{
					float const t{ dCurrent / (dCurrent - dNext) };
					output.Add(LerpVertex(current, next, t));
				}
			}

			polygon = output;
		}

		// Clip against near (z >= 0) and far (z <= w) planes only
		inline void ClipTriangleNearFar(ClipPolygon& polygon) noexcept
		{
			ClipAgainstPlane(polygon, [](Vector4 const& p) { return p.z; });           // Near: z >= 0
			ClipAgainstPlane(polygon, [](Vector4 const& p) { return p.w - p.z; });     // Far: z <= w
		}

		// Full 6-plane Sutherland-Hodgman clip
		inline void ClipTriangleFull(ClipPolygon& polygon) noexcept
		{
			ClipAgainstPlane(polygon, [](Vector4 const& p) { return p.z; });           // Near: z >= 0
			ClipAgainstPlane(polygon, [](Vector4 const& p) { return p.w - p.z; });     // Far: z <= w
			ClipAgainstPlane(polygon, [](Vector4 const& p) { return p.x + p.w; });     // Left: x >= -w
			ClipAgainstPlane(polygon, [](Vector4 const& p) { return p.w - p.x; });     // Right: x <= w
			ClipAgainstPlane(polygon, [](Vector4 const& p) { return p.y + p.w; });     // Bottom: y >= -w
			ClipAgainstPlane(polygon, [](Vector4 const& p) { return p.w - p.y; });     // Top: y <= w
		}

		// Guard-band check: are all vertices within a generous multiple of the viewport?
		float inline constexpr GUARD_BAND_MULTIPLIER{ 2.f };

		[[nodiscard]] inline bool IsInsideGuardBand(ClipPolygon const& polygon) noexcept
		{
			for (uint8_t i{ 0 }; i < polygon.count; ++i)
			{
				auto const& v = polygon.verts[i];
				float const guardW{ GUARD_BAND_MULTIPLIER * std::abs(v.position.w) };
				if (v.position.x < -guardW || v.position.x > guardW ||
					v.position.y < -guardW || v.position.y > guardW)
				{
					return false;
				}
			}
			return true;
		}

		// Compute model-space positions for clipped vertices by recovering barycentric
		// coords from their clip-space positions relative to the original triangle.
		inline void InterpolateClipPositions(
			ClipPolygon const& polygon,
			Vertex_Out const& origCV0, Vertex_Out const& origCV1, Vertex_Out const& origCV2,
			Vector3 const& pos0, Vector3 const& pos1, Vector3 const& pos2,
			Vector3* outPositions) noexcept
		{
			// Use x,y,z of clip-space to solve for barycentrics via Cramer's rule
			// p = u * v0 + v * v1 + (1-u-v) * v2
			// p - v2 = u * (v0 - v2) + v * (v1 - v2)
			Vector3 const d0{
				origCV0.position.x - origCV2.position.x,
				origCV0.position.y - origCV2.position.y,
				origCV0.position.z - origCV2.position.z };
			Vector3 const d1{
				origCV1.position.x - origCV2.position.x,
				origCV1.position.y - origCV2.position.y,
				origCV1.position.z - origCV2.position.z };

			// Use the two components with the largest cross product for numerical stability
			float const cx{ d0.y * d1.z - d0.z * d1.y };
			float const cy{ d0.z * d1.x - d0.x * d1.z };
			float const cz{ d0.x * d1.y - d0.y * d1.x };

			float const acx{ std::abs(cx) };
			float const acy{ std::abs(cy) };
			float const acz{ std::abs(cz) };

			for (uint8_t i{ 0 }; i < polygon.count; ++i)
			{
				Vector3 const dp{
					polygon.verts[i].position.x - origCV2.position.x,
					polygon.verts[i].position.y - origCV2.position.y,
					polygon.verts[i].position.z - origCV2.position.z };

				float u, v;
				if (acx >= acy && acx >= acz)
				{
					// Use y,z components
					u = (dp.y * d1.z - dp.z * d1.y) / cx;
					v = (d0.y * dp.z - d0.z * dp.y) / cx;
				}
				else if (acy >= acz)
				{
					// Use z,x components
					u = (dp.z * d1.x - dp.x * d1.z) / cy;
					v = (d0.z * dp.x - d0.x * dp.z) / cy;
				}
				else
				{
					// Use x,y components
					u = (dp.x * d1.y - dp.y * d1.x) / cz;
					v = (d0.x * dp.y - d0.y * dp.x) / cz;
				}

				float const w{ 1.f - u - v };
				outPositions[i] = pos0 * u + pos1 * v + pos2 * w;
			}
		}

		[[nodiscard]] constexpr float DepthRemap(float v, float min, float max) noexcept
		{
			float const normalizedValue{ (v - min) / (max - min) };
			if (normalizedValue < 0.0f)
			{
				return 0.f;
			}
			return normalizedValue;
		}


		//Just parses vertices and indices
#pragma warning(push)
#pragma warning(disable : 4505) //Warning unreferenced local function
		static bool ParseOBJ(const std::string& filename, std::vector<Vertex_In>& vertices, std::vector<uint32_t>& indices, bool flipAxisAndWinding = true)
		{
			std::ifstream file(filename);
			if (!file)
				return false;

			std::vector<Vector3> positions{};
			std::vector<Vector3> normals{};
			std::vector<Vector2> UVs{};

			vertices.clear();
			indices.clear();

			std::string sCommand;
			// start a while iteration ending when the end of file is reached (ios::eof)
			while (!file.eof())
			{
				//read the first word of the string, use the >> operator (istream::operator>>) 
				file >> sCommand;
				//use conditional statements to process the different commands	
				if (sCommand == "#")
				{
					// Ignore Comment
				}
				else if (sCommand == "v")
				{
					//Vertex_In
					float x, y, z;
					file >> x >> y >> z;

					positions.emplace_back(x, y, z);
				}
				else if (sCommand == "vt")
				{
					// Vertex_In TexCoord
					float u, v;
					file >> u >> v;
					UVs.emplace_back(u, 1 - v);
				}
				else if (sCommand == "vn")
				{
					// Vertex_In Normal
					float x, y, z;
					file >> x >> y >> z;

					normals.emplace_back(x, y, z);
				}
				else if (sCommand == "f")
				{
					//if a face is read:
					//construct the 3 vertices, add them to the vertex array
					//add three indices to the index array
					//add the material index as attibute to the attribute array
					//
					// Faces or triangles
					Vertex_In vertex{};
					size_t iPosition, iTexCoord, iNormal;

					uint32_t tempIndices[3];
					for (size_t iFace = 0; iFace < 3; iFace++)
					{
						// OBJ format uses 1-based arrays
						file >> iPosition;
						vertex.position = positions[iPosition - 1];

						if ('/' == file.peek())//is next in buffer ==  '/' ?
						{
							file.ignore();//read and ignore one element ('/')

							if ('/' != file.peek())
							{
								// Optional texture coordinate
								file >> iTexCoord;
								vertex.texcoord = UVs[iTexCoord - 1];
							}

							if ('/' == file.peek())
							{
								file.ignore();

								// Optional vertex normal
								file >> iNormal;
								vertex.normal = normals[iNormal - 1];
							}
						}

						vertices.push_back(vertex);
						tempIndices[iFace] = uint32_t(vertices.size()) - 1;
						//indices.push_back(uint32_t(vertices.size()) - 1);
					}

					indices.push_back(tempIndices[0]);
					if (flipAxisAndWinding)
					{
						indices.push_back(tempIndices[2]);
						indices.push_back(tempIndices[1]);
					}
					else
					{
						indices.push_back(tempIndices[1]);
						indices.push_back(tempIndices[2]);
					}
				}
				//read till end of line and ignore all remaining chars
				file.ignore(1000, '\n');
			}

			//Cheap Tangent Calculations
			for (uint32_t i = 0; i < indices.size(); i += 3)
			{
				uint32_t index0 = indices[i];
				uint32_t index1 = indices[size_t(i) + 1];
				uint32_t index2 = indices[size_t(i) + 2];

				const Vector3& p0 = vertices[index0].position;
				const Vector3& p1 = vertices[index1].position;
				const Vector3& p2 = vertices[index2].position;
				const Vector2& uv0 = vertices[index0].texcoord;
				const Vector2& uv1 = vertices[index1].texcoord;
				const Vector2& uv2 = vertices[index2].texcoord;

				const Vector3 edge0 = p1 - p0;
				const Vector3 edge1 = p2 - p0;
				const Vector2 diffX = Vector2(uv1.x - uv0.x, uv2.x - uv0.x);
				const Vector2 diffY = Vector2(uv1.y - uv0.y, uv2.y - uv0.y);
				float r = 1.f / Vector2::Cross(diffX, diffY);

				Vector3 tangent = (edge0 * diffY.y - edge1 * diffY.x) * r;
				vertices[index0].tangent += tangent;
				vertices[index1].tangent += tangent;
				vertices[index2].tangent += tangent;
			}

			//Create the Tangents (reject)
			for (auto& v : vertices)
			{
				v.tangent = Vector3::Reject(v.tangent, v.normal).Normalized();

				if (flipAxisAndWinding)
				{
					v.position.z *= -1.f;
					v.normal.z *= -1.f;
					v.tangent.z *= -1.f;
				}

			}

			return true;
		}
#pragma warning(pop)
	}
}
