#include "pch.h"
#include "Renderer.h"
#include "Mesh.h"
#include "Utils.h"
#include "BRDF.h"
#include "Effect.h"
#include <execution>
#include <atomic>
#include <Profiler/ProfilerMacros.h>

namespace mau {

	Renderer::Renderer(SDL_Window* pWindow) :
		m_pWindow(pWindow)
	{
		//Initialize
		SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

		//Create Buffers (software rasterizer)
		m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
		m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
		m_pBackBufferPixels = static_cast<uint32_t*>(m_pBackBuffer->pixels);

		m_DepthBuffer.resize(m_Width * m_Height, FLT_MAX);

		//Initialize DirectX pipeline
		if (SUCCEEDED(InitializeDirectX()))
		{
			m_IsDirectXInitialized = true;
			std::cout << GREEN << "DirectX is initialized and ready!" << RESET << std::endl;
		}
		else
		{
			m_IsSoftwareRasterizerMode = true;
			std::cout << RED << "DirectX initialization failed!" << RESET << std::endl;
		}

		//Intialize textures
		m_pVehicleDiffuseTexture = std::make_unique<Texture>(L"resources/vehicle_diffuse.png", m_pDevice);
		m_pVehicleGlossinessTexture = std::make_unique<Texture>(L"resources/vehicle_gloss.png", m_pDevice);
		m_pVehicleNormalTexture = std::make_unique<Texture>(L"resources/vehicle_normal.png", m_pDevice);
		m_pVehicleSpecularTexture = std::make_unique<Texture>(L"resources/vehicle_specular.png", m_pDevice);

		m_pFireDiffuseTexture = std::make_unique<Texture>(L"resources/fireFX_diffuse.png", m_pDevice);


		//Initialize effects
		m_pVehicleEffect = std::make_shared<PixelShadingEffect>(m_pDevice, L"resources/PosCol3D.fx");
		m_pVehicleEffect->SetDiffuseTexture(m_pVehicleDiffuseTexture.get());
		m_pVehicleEffect->SetGlossinessTexture(m_pVehicleGlossinessTexture.get());
		m_pVehicleEffect->SetNormalTexture(m_pVehicleNormalTexture.get());
		m_pVehicleEffect->SetSpecularTexture(m_pVehicleSpecularTexture.get());

		m_pFireEffect = std::make_shared<BaseEffect>(m_pDevice, L"resources/PartialCoverage3D.fx");
		m_pFireEffect->SetDiffuseTexture(m_pFireDiffuseTexture.get());

		//Initialize models
		m_Meshes.emplace_back(std::make_unique<Mesh>(m_pDevice, "resources/vehicle.obj", m_pVehicleEffect));
		m_Meshes.emplace_back(std::make_unique<Mesh>(m_pDevice, "resources/fireFX.obj", m_pFireEffect));
		m_Meshes[0]->Translate({ 0.f, 0.f, 50.f });
		m_Meshes[1]->Translate({ 0.f, 0.f, 50.f }); // Firemesh alwasy at idx 1 for this demo since we need to check for it to disable.

		//Camera setup
		m_Camera.Initialize(45.f, { 0.f, 0.f, 0.f }, static_cast<float>(m_Width) / static_cast<float>(m_Height));
	}

	Renderer::~Renderer()
	{

		// Direct X safe release macro (call release if exists)
		SAFE_RELEASE(m_pRenderTargetView)
		SAFE_RELEASE(m_pRenderTargetBuffer)

		SAFE_RELEASE(m_pDepthStencilView)
		SAFE_RELEASE(m_pDepthStencilBuffer)

		SAFE_RELEASE(m_pSwapChain)

		if (m_pDeviceContext)
		{
			m_pDeviceContext->ClearState();
			m_pDeviceContext->Flush();
			m_pDeviceContext->Release();
		}

		SAFE_RELEASE(m_pDevice)
	}

	void Renderer::ChangeSamplerState() noexcept
	{
		if (m_IsSoftwareRasterizerMode)
		{
			std::cout << RED << "Not in hardware rasterizer, can not cycle sampler state setting\n" << RESET;
			return;
		}

		if (!m_IsDirectXInitialized)
		{
			std::cout << RED << "DirectX not Initialized\n" << RESET;
			return;
		}

		//Calculate new idx
		uint8_t newId = static_cast<uint8_t>(m_SamplerState);
		++newId %= static_cast<uint8_t>(SamplerState::COUNT);
		m_SamplerState = static_cast<SamplerState>(newId);

		switch (m_SamplerState)
		{
		case SamplerState::Point: 
			std::cout << "Sampler state -> " << GREEN << "PointSampler \n";
			std::cout << RESET;
			break;
		case SamplerState::Linear:
			std::cout << "Sampler state -> " << GREEN << "LinearSampler \n";
			std::cout << RESET;
			break;
		case SamplerState::Anisotropic:
			std::cout << "Sampler state -> " << GREEN << "AnisotropicSampler \n";
			std::cout << RESET;
			break;
		default: break;
		}

		for (auto& m : m_Meshes)
		{
			m->SetSamplingMode(newId);
		}
	}

	void Renderer::Update(const Timer* pTimer)
	{
		m_Camera.Update(pTimer);

		for (auto& m : m_Meshes)
		{
			if (m_IsRotationMode)
			{
				m->RotateY(TO_RADIANS*(45.f * pTimer->GetElapsed()));
			}
			m->UpdateEffectMatrices(m_Camera.viewMatrix * m_Camera.projectionMatrix);
			m->UpdateCameraPos(m_Camera.origin);
		}
	}

	void Renderer::Render() const
	{
		if (m_IsSoftwareRasterizerMode)
		{
			RenderSoftware();
			return;
		}
		RenderDirectXHardware();
	}

	void Renderer::RenderDirectXHardware() const
	{
		PROFILER_FUNCTION();
		if (!m_IsDirectXInitialized)
		{
			std::cout << RED << "DirectX not Initialized\n" << RESET;
			return;
		}

		// Clear RTV & DSV
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, m_DisplayUniformClearColor ? UNIFORM_COLOR : HARDWARE_COLOR);
		m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// Set pipeline + invoke drawcalls
		for (auto& m : m_Meshes)
		{
			if (!m_DisplayFireMesh && m_Meshes.size() > 1 && m == m_Meshes[1]) // Hard coded to array idx 1 currently since it's just a demo
			{
				continue;
			}
			m->Render(m_pDeviceContext);
		}

		// present backbuffer (swap)
		m_pSwapChain->Present(0, 0);
	}

	void Renderer::RenderSoftware() const
	{
		PROFILER_FUNCTION();
		//Lock BackBuffer
		SDL_LockSurface(m_pBackBuffer);

		std::fill(m_DepthBuffer.begin(), m_DepthBuffer.end(), FLT_MAX);
		std::fill_n(m_pBackBufferPixels, m_Width * m_Height, 0);

		//clear the background
		if (m_DisplayUniformClearColor)
		{
			SDL_FillRect(m_pBackBuffer, nullptr, SDL_MapRGB(m_pBackBuffer->format, static_cast<uint8_t>(UNIFORM_COLOR[0] * 255), static_cast<uint8_t>(UNIFORM_COLOR[1] * 255), static_cast<uint8_t>(UNIFORM_COLOR[2] * 255)));
		}
		else
		{
			SDL_FillRect(m_pBackBuffer, nullptr, SDL_MapRGB(m_pBackBuffer->format, static_cast<uint8_t>(SOFTWARE_COLOR[0] * 255), static_cast<uint8_t>(SOFTWARE_COLOR[1] * 255), static_cast<uint8_t>(SOFTWARE_COLOR[2] * 255)));
		}

		for (auto & m : m_Meshes)
		{
			//Hard coded to fire mesh since we don't support this in software currently.
			if (m == m_Meshes[1])
			{
				continue;
			}

			// Transform vertices to clip space (perspective divide deferred to after clipping)
			VertexTransformationFunction(m.get(), m_ClipSpaceVertices);

			PROFILER_SCOPE("Rasterization");
			switch (m->GetPrimitiveTopology())
			{
			case PrimitiveTopology::TriangleList:
				std::for_each(
					std::execution::par_unseq, // Note: depth buffer access is not atomic - potential race on overlapping triangles
					m->GetIndices().begin(), m->GetIndices().end(),
					[this, &m](uint32_t v)
					{
						if (v % 3 == 0)
						{
							RenderTriangle(m.get(), m_ClipSpaceVertices, v, false);
						}
					});
				break;
			case PrimitiveTopology::TriangleStrip:
				std::for_each(
					std::execution::par_unseq,
					m->GetIndices().begin(), m->GetIndices().end() - 2,
					[this, &m](uint32_t v)
					{
						RenderTriangle(m.get(), m_ClipSpaceVertices, v, v % 2);
					});
				break;
			default:
				break;
			}
		}

		//@END
		//Update SDL Surface
		SDL_UnlockSurface(m_pBackBuffer);
		SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
		SDL_UpdateWindowSurface(m_pWindow);
	}

	void Renderer::VertexTransformationFunction(Mesh const* mesh, std::vector<Vertex_Out>& clipSpaceVertices) const
	{
		PROFILER_FUNCTION();
		// Model -> world -> view -> clip space
		auto const m{ mesh->GetWorldMatrix() * m_Camera.viewMatrix * m_Camera.projectionMatrix };
		auto const& worldMatrix{ mesh->GetWorldMatrix() };

		auto const vertCount{ mesh->GetVertices().size() };
		clipSpaceVertices.resize(vertCount);
		m_NdcVertices.resize(vertCount);
		m_ScreenVertices.resize(vertCount);

		float const halfWidth{ static_cast<float>(m_Width) * 0.5f };
		float const halfHeight{ static_cast<float>(m_Height) * 0.5f };

		// Transform vertices to clip space + NDC + screen space in parallel
		auto const* vertData = mesh->GetVertices().data();
		std::transform(
			std::execution::par,
			mesh->GetVertices().begin(), mesh->GetVertices().end(),
			clipSpaceVertices.begin(),
			[&](auto const& v) {
				Vertex_Out vOut{};
				vOut.texcoord = v.texcoord;
				vOut.position = m.TransformPoint(v.position.ToPoint4());
				vOut.worldPosition = worldMatrix.TransformPoint(v.position);
				vOut.normal = worldMatrix.TransformVector(v.normal);
				vOut.tangent = worldMatrix.TransformVector(v.tangent);

				// Pre-compute NDC + screen space for the common (non-clipped) path
				auto const idx{ &v - vertData };
				m_NdcVertices[idx] = vOut;
				float const invW{ 1.f / vOut.position.w };
				m_NdcVertices[idx].position.x = vOut.position.x * invW;
				m_NdcVertices[idx].position.y = vOut.position.y * invW;
				m_NdcVertices[idx].position.z = vOut.position.z * invW;

				m_ScreenVertices[idx] = {
					(m_NdcVertices[idx].position.x + 1.f) * halfWidth,
					(1.f - m_NdcVertices[idx].position.y) * halfHeight
				};

				return vOut;
			});
	}

	void Renderer::RenderTriangle(Mesh const* m, std::vector<Vertex_Out> const& clipSpaceVertices, uint32_t startVertex, bool swapVertex) const
	{
		size_t const idx1{ m->GetIndices()[startVertex + (2 * swapVertex)] };
		size_t const idx2{ m->GetIndices()[startVertex + 1] };
		size_t const idx3{ m->GetIndices()[startVertex + (!swapVertex * 2)] };

		// Degenerate triangle
		if (idx1 == idx2 || idx2 == idx3 || idx3 == idx1)
			return;

		auto const& cv0 = clipSpaceVertices[idx1];
		auto const& cv1 = clipSpaceVertices[idx2];
		auto const& cv2 = clipSpaceVertices[idx3];

		// 1. Trivial reject - all vertices on wrong side of same frustum plane
		if (Utils::IsTriangleOutsideFrustum(cv0.position, cv1.position, cv2.position))
			return;

		// 2. Trivial accept - all vertices inside all frustum planes, skip clipping
		if (Utils::IsTriangleInsideFrustum(cv0.position, cv1.position, cv2.position))
		{
			// Fast path - use pre-computed NDC + screen coords, zero extra work
			RasterizeTriangle(m,
				m_ScreenVertices[idx1], m_ScreenVertices[idx2], m_ScreenVertices[idx3],
				m_NdcVertices[idx1], m_NdcVertices[idx2], m_NdcVertices[idx3]);
			return;
		}

		// 3. Near/far clip (mandatory - prevents behind-camera artifacts)
		Utils::ClipPolygon polygon;
		polygon.Add(cv0); polygon.Add(cv1); polygon.Add(cv2);
		Utils::ClipTriangleNearFar(polygon);

		if (polygon.count < 3)
			return;

		// 4. Guard-band check - if vertices stay within guard band, skip x/y clipping
		if (!Utils::IsInsideGuardBand(polygon))
		{
			polygon.Clear();
			polygon.Add(cv0); polygon.Add(cv1); polygon.Add(cv2);
			Utils::ClipTriangleFull(polygon);

			if (polygon.count < 3)
				return;
		}

		// 5 & 6. Perspective divide + fan-triangulate + rasterize (only for clipped verts)
		float const halfWidth{ static_cast<float>(m_Width) * 0.5f };
		float const halfHeight{ static_cast<float>(m_Height) * 0.5f };

		Vector2 screenVerts[Utils::MAX_CLIP_VERTS];
		Vertex_Out ndcVerts[Utils::MAX_CLIP_VERTS];

		for (uint8_t i{ 0 }; i < polygon.count; ++i)
		{
			ndcVerts[i] = polygon.verts[i];
			float const invW{ 1.f / polygon.verts[i].position.w };
			ndcVerts[i].position.x = polygon.verts[i].position.x * invW;
			ndcVerts[i].position.y = polygon.verts[i].position.y * invW;
			ndcVerts[i].position.z = polygon.verts[i].position.z * invW;
			screenVerts[i] = {
				(ndcVerts[i].position.x + 1.f) * halfWidth,
				(1.f - ndcVerts[i].position.y) * halfHeight
			};
		}

		for (uint8_t i{ 1 }; i + 1 < polygon.count; ++i)
		{
			RasterizeTriangle(m,
				screenVerts[0], screenVerts[i], screenVerts[i + 1],
				ndcVerts[0], ndcVerts[i], ndcVerts[i + 1]);
		}
	}

	void Renderer::RasterizeTriangle(Mesh const* m,
		Vector2 const& vert0, Vector2 const& vert1, Vector2 const& vert2,
		Vertex_Out const& v0, Vertex_Out const& v1, Vertex_Out const& v2) const
	{
		float const totalTriangleArea{ (vert1.x - vert0.x) * (vert2.y - vert0.y) - (vert1.y - vert0.y) * (vert2.x - vert0.x) };

		// Face culling
		switch (m_CurrCullMode)
		{
		case CullMode::Back:
			if (totalTriangleArea < 0.f) return;
			break;
		case CullMode::Front:
			if (totalTriangleArea > 0.f) return;
			break;
		case CullMode::None:
			break;
		default:
			break;
		}

		float const invTotalTriangleArea{ 1.f / totalTriangleArea };

		// Bounding box with small margin to prevent black lines
		Vector2 topLeft{ Vector2::Min(vert0, Vector2::Min(vert1, vert2)) - Vector2{1.f, 1.f} };
		Vector2 bottomRight{ Vector2::Max(vert0, Vector2::Max(vert1, vert2)) + Vector2{1.f, 1.f} };

		// Clamp to screen bounds
		topLeft.x = std::clamp(topLeft.x, 0.f, static_cast<float>(m_Width));
		topLeft.y = std::clamp(topLeft.y, 0.f, static_cast<float>(m_Height));
		bottomRight.x = std::clamp(bottomRight.x, 0.f, static_cast<float>(m_Width));
		bottomRight.y = std::clamp(bottomRight.y, 0.f, static_cast<float>(m_Height));

		// Cache per-triangle data
		float const depth0{ v0.position.z };
		float const depth1{ v1.position.z };
		float const depth2{ v2.position.z };
		float const invDepth0{ 1.f / depth0 };
		float const invDepth1{ 1.f / depth1 };
		float const invDepth2{ 1.f / depth2 };

		float const invW0{ 1.f / v0.position.w };
		float const invW1{ 1.f / v1.position.w };
		float const invW2{ 1.f / v2.position.w };

		auto const& uv0 = v0.texcoord;
		auto const& uv1 = v1.texcoord;
		auto const& uv2 = v2.texcoord;

		auto const& n0 = v0.normal;
		auto const& n1 = v1.normal;
		auto const& n2 = v2.normal;

		auto const& t0 = v0.tangent;
		auto const& t1 = v1.tangent;
		auto const& t2 = v2.tangent;

		Vector2 const edge12{ vert1 - vert2 };
		Vector2 const edge20{ vert2 - vert0 };
		Vector2 const edge01{ vert0 - vert1 };

		int const startX{ static_cast<int>(topLeft.x) };
		int const endX{ static_cast<int>(bottomRight.x) };
		int const startY{ static_cast<int>(topLeft.y) };
		int const endY{ static_cast<int>(bottomRight.y) };

		for (int px{ startX }; px < endX; ++px)
		{
			for (int py{ startY }; py < endY; ++py)
			{
				if (m_ShowBoundingBoxes)
				{
					m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format, 255, 255, 255);
					continue;
				}

				Vector2 const pixel{ static_cast<float>(px) + .5f, static_cast<float>(py) + .5f };

				float const weight0{ Vector2::Cross((pixel - vert1), edge12) * invTotalTriangleArea };
				if (weight0 < 0.f) continue;
				float const weight1{ Vector2::Cross((pixel - vert2), edge20) * invTotalTriangleArea };
				if (weight1 < 0.f) continue;
				float const weight2{ Vector2::Cross((pixel - vert0), edge01) * invTotalTriangleArea };
				if (weight2 < 0.f) continue;

				float const interpolatedDepth{ 1.f / (weight0 * invDepth0 + weight1 * invDepth1 + weight2 * invDepth2) };

				int const pixelIdx{ px + py * m_Width };
				if (interpolatedDepth < 0.f || interpolatedDepth > 1.f || m_DepthBuffer.data()[pixelIdx] < interpolatedDepth)
					continue;

				m_DepthBuffer.data()[pixelIdx] = interpolatedDepth;

				ColorRGB finalColor;

				if (m_ShowDepthBuffer)
				{
					float const remap{ Utils::DepthRemap(interpolatedDepth, .985f, 1.f) };
					finalColor = ColorRGB{ (1.f - remap) * 5, (1.f - remap) * 5, 1.f };
				}
				else
				{
					float const interpolatedW{ 1.f / (weight0 * invW0 + weight1 * invW1 + weight2 * invW2) };

					Vertex_Out pixelToShade{};
					pixelToShade.position = { static_cast<float>(px), static_cast<float>(py), interpolatedDepth, interpolatedDepth };

					// Perspective-correct view direction from interpolated world positions
					Vector3 const worldPos{ interpolatedW * (
						weight0 * v0.worldPosition * invW0 +
						weight1 * v1.worldPosition * invW1 +
						weight2 * v2.worldPosition * invW2) };
					Vector3 const viewDir{ (worldPos - m_Camera.origin).Normalized() };

					pixelToShade.texcoord = interpolatedW * (weight0 * uv0 * invW0 + weight1 * uv1 * invW1 + weight2 * uv2 * invW2);
					pixelToShade.normal = Vector3{ interpolatedW * (weight0 * n0 * invW0 + weight1 * n1 * invW1 + weight2 * n2 * invW2) }.Normalized();
					pixelToShade.tangent = Vector3{ interpolatedW * (weight0 * t0 * invW0 + weight1 * t1 * invW1 + weight2 * t2 * invW2) }.Normalized();

					finalColor = PixelShading(m, pixelToShade, viewDir);
				}

				finalColor.MaxToOne();
				m_pBackBufferPixels[pixelIdx] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}

	ColorRGB Renderer::PixelShading(const Mesh* m, const Vertex_Out& v, const Vector3& viewDir) const
	{
		//Global light & other defines
		Vector3 static constexpr LIGHT_DIRECTION{ Vector3{.577f, -.577f, .577f} };
		ColorRGB static constexpr AMBIENT_COLOR{ 0.025f, 0.025f, 0.025f };
		float static constexpr SHININESS{ 25.0f };
		float static constexpr KD{ 7.f };

		// Early out: skip all shading if surface faces away from light
		if (m_CurrShadingMode != ShadingMode::ObservedArea && Vector3::Dot(v.normal, -LIGHT_DIRECTION) <= 0.f)
		{
			return AMBIENT_COLOR;
		}

		// Normal mapping (manual 3x3 TBN transform - avoids 4x4 Matrix overhead)
		Vector3 sampledNormal{ v.normal };
		if (m_UseNormalMapping)
		{
			Vector3 const biNormal = Vector3::Cross(v.normal, v.tangent);
			ColorRGB const normalColor = m_pVehicleNormalTexture->Sample(v.texcoord);
			float const nx = normalColor.r * 2.f - 1.f;
			float const ny = normalColor.g * 2.f - 1.f;
			float const nz = normalColor.b * 2.f - 1.f;

			sampledNormal = {
				nx * v.tangent.x + ny * biNormal.x + nz * v.normal.x,
				nx * v.tangent.y + ny * biNormal.y + nz * v.normal.y,
				nx * v.tangent.z + ny * biNormal.z + nz * v.normal.z
			};
			sampledNormal.Normalize();
		}

		float const observedArea{ std::clamp(Utils::CalculateObservedArea(sampledNormal, LIGHT_DIRECTION), 0.f, 1.f) };
		ColorRGB result{ };
		switch (m_CurrShadingMode)
		{
		case ShadingMode::ObservedArea:
		{
			result = ColorRGB{ observedArea, observedArea, observedArea };
			break;
		}
		case ShadingMode::Diffuse:
		{
			result = BRDF::Lambert(KD, m_pVehicleDiffuseTexture->Sample(v.texcoord)) * observedArea;
			break;
		}
		case ShadingMode::Specular:
		{
			result = observedArea * m_pVehicleSpecularTexture->Sample(v.texcoord).r * BRDF::Phong(1.f, SHININESS * m_pVehicleGlossinessTexture->Sample(v.texcoord).r, LIGHT_DIRECTION, viewDir, sampledNormal);
			break;
		}
		case ShadingMode::Combined:
		{
			auto const lambert{ BRDF::Lambert(KD, m_pVehicleDiffuseTexture->Sample(v.texcoord)) };
			ColorRGB const phong = m_pVehicleSpecularTexture->Sample(v.texcoord).r * BRDF::Phong(1.f, SHININESS * m_pVehicleGlossinessTexture->Sample(v.texcoord).r, LIGHT_DIRECTION, viewDir, sampledNormal);
			result = observedArea * lambert + phong;
			break;
		}
		default:
			break;
		}
		result += AMBIENT_COLOR;
		return result;
	}

	HRESULT Renderer::InitializeDirectX()
	{
		HRESULT result{};

		//Create device and device context
		D3D_FEATURE_LEVEL constexpr featureLevel{ D3D_FEATURE_LEVEL_11_1 };
		uint32_t createDeviceFlags{ 0 };

		#if defined(DEBUG) ||defined(_DEBUG)
			createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		#endif

		// Create DXGI Factory
		IDXGIFactory1* pFactory{ };
		result = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&pFactory));
		if (FAILED(result))
		{
			return result;
		}

		IDXGIAdapter* pAdapter{ nullptr };
		IDXGIAdapter* selectedAdapter{ nullptr };

		//Look for the GPU with most memory
		UINT i{ 0 };
		size_t maxDedicatedVideoMemory{ 0 };

		std::cout << GREEN << "Selecting adapter\n" << RESET;

		while (pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND) 
		{
			DXGI_ADAPTER_DESC desc;
			pAdapter->GetDesc(&desc);

			std::wcout << L"Adapter " << i << L": " << desc.Description << "\n";
			std::wcout << L"VendorId: " << desc.VendorId << "\n";
			std::wcout << L"DedicatedVideoMemory: " << desc.DedicatedVideoMemory << L" bytes \n";

			// Skip integrated graphics
			if (desc.DedicatedVideoMemory > maxDedicatedVideoMemory) 
			{
				maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
				selectedAdapter = pAdapter;
			}
			else 
			{
				pAdapter->Release();
			}

			++i;
		}

		if (selectedAdapter) 
		{
			DXGI_ADAPTER_DESC selectedDesc;
			selectedAdapter->GetDesc(&selectedDesc);
			std::wcout << GREEN << L"\nSelected Adapter: " << selectedDesc.Description <<"\n\n" << RESET;
		}
		else 
		{
			std::cout << RED << "\nNo suitable adapter found!\n\n" << RESET;
			return false;
		}

		// https://learn.microsoft.com/en-us/windows/win32/seccrypto/common-hresult-values
		result = D3D11CreateDevice(
			selectedAdapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			nullptr,
			createDeviceFlags,
			&featureLevel,
			1,
			D3D11_SDK_VERSION,
			&m_pDevice,
			nullptr,
			&m_pDeviceContext
		);

		if (FAILED(result))
		{
			return result;
		}

		// Create swap chain | "https://learn.microsoft.com/en-us/windows/win32/direct3d9/what-is-a-swap-chain-"
		// https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_swap_chain_desc
		// Swap chain description
		DXGI_SWAP_CHAIN_DESC swapChainDesc{};
		swapChainDesc.BufferDesc.Width = m_Width;
		swapChainDesc.BufferDesc.Height = m_Height;
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 1;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 60;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 1;
		swapChainDesc.Windowed = true;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swapChainDesc.Flags = 0;

		// Get Hanle from the SDL backbuffer
		SDL_SysWMinfo wmInfo{};
		SDL_GetVersion(&wmInfo.version);
		SDL_GetWindowWMInfo(m_pWindow, &wmInfo);
		swapChainDesc.OutputWindow = wmInfo.info.win.window;

		// Create swapchain
		result = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
		if (FAILED(result))
		{
			return result;
		}

		// Create Depth Buffer - DepthStencil & DepthStencilView
		D3D11_TEXTURE2D_DESC depthStencilDesc{};
		depthStencilDesc.Width = m_Width;
		depthStencilDesc.Height = m_Height;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.CPUAccessFlags = 0;
		depthStencilDesc.MiscFlags = 0;

		// View
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
		depthStencilViewDesc.Format = depthStencilDesc.Format;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		result = m_pDevice->CreateTexture2D(&depthStencilDesc, nullptr, &m_pDepthStencilBuffer);
		if (FAILED(result))
		{
			return result;
		}

		result = m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, &depthStencilViewDesc, &m_pDepthStencilView);
		if (FAILED(result))
		{
			return result;
		}

		// Create Render Target View
		//Resource
		result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Resource), reinterpret_cast<void**>(&m_pRenderTargetBuffer));
		if (FAILED(result))
		{
			return result;
		}

		//View
		result = m_pDevice->CreateRenderTargetView(m_pRenderTargetBuffer, nullptr, &m_pRenderTargetView);
		if (FAILED(result))
		{
			return result;
		}

		//Bind the views to the pipeline
		m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

		//Set viewport
		D3D11_VIEWPORT vp{};
		vp.Width = static_cast<float>(m_Width);
		vp.Height = static_cast<float>(m_Height);
		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		m_pDeviceContext->RSSetViewports(1, &vp);

		return result;
	}
}
