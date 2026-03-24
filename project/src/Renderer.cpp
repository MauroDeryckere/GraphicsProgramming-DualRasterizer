#include "pch.h"
#include "Renderer.h"
#include "Mesh.h"
#include "Utils.h"
#include "BRDF.h"
#include "Effect.h"
#include <execution>

namespace mau {

	Renderer::Renderer(SDL_Window* pWindow) :
		m_pWindow(pWindow)
	{
		//Initialize
		SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

		//Create Buffers (software rasterizer)
		m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
		m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
		m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

		m_pDepthBufferPixels = new float[m_Width * m_Height];
		std::fill_n(m_pDepthBufferPixels, (m_Width * m_Height), FLT_MAX);

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
		m_pVehicleDiffuseTexture = std::make_unique<Texture>(L"Resources/vehicle_diffuse.png", m_pDevice);
		m_pVehicleGlossinessTexture = std::make_unique<Texture>(L"Resources/vehicle_gloss.png", m_pDevice);
		m_pVehicleNormalTexture = std::make_unique<Texture>(L"Resources/vehicle_normal.png", m_pDevice);
		m_pVehicleSpecularTexture = std::make_unique<Texture>(L"Resources/vehicle_specular.png", m_pDevice);

		m_pFireDiffuseTexture = std::make_unique<Texture>(L"Resources/fireFX_diffuse.png", m_pDevice);


		//Initialize effects
		m_pVehicleEffect = std::make_shared<PixelShadingEffect>(m_pDevice, L"Resources/PosCol3D.fx");
		m_pVehicleEffect->SetDiffuseTexture(m_pVehicleDiffuseTexture.get());
		m_pVehicleEffect->SetGlossinessTexture(m_pVehicleGlossinessTexture.get());
		m_pVehicleEffect->SetNormalTexture(m_pVehicleNormalTexture.get());
		m_pVehicleEffect->SetSpecularTexture(m_pVehicleSpecularTexture.get());

		m_pFireEffect = std::make_shared<BaseEffect>(m_pDevice, L"Resources/PartialCoverage3D.fx");
		m_pFireEffect->SetDiffuseTexture(m_pFireDiffuseTexture.get());

		//Initialize models
		m_Meshes.emplace_back(std::make_unique<Mesh>(m_pDevice, "Resources/vehicle.obj", m_pVehicleEffect));
		m_Meshes.emplace_back(std::make_unique<Mesh>(m_pDevice, "Resources/fireFX.obj", m_pFireEffect));
		m_Meshes[0]->Translate({ 0.f, 0.f, 50.f });
		m_Meshes[1]->Translate({ 0.f, 0.f, 50.f }); // Firemesh alwasy at idx 1 for this demo since we need to check for it to disable.

		//Camera setup
		m_Camera.Initialize(45.f, { 0.f, 0.f, 0.f }, static_cast<float>(m_Width) / static_cast<float>(m_Height));
	}

	Renderer::~Renderer()
	{
		delete[] m_pDepthBufferPixels;

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

	void Renderer::Update(Timer* pTimer)
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
		//Lock BackBuffer
		SDL_LockSurface(m_pBackBuffer);

		std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);
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
			//Meshes defined in world space before the transform function,
			std::vector<Vector2> vertices_screenSpace{};
			//convert each NDC coordinates to screen space / raster space
			VertexTransformationFunction(vertices_screenSpace, m.get());

			switch (m->GetPrimitiveTopology())
			{
			case PrimitiveTopology::TriangleList:
				// Use parallel execution for triangle list
				std::for_each(
					std::execution::par_unseq,  // threading execution policy - threading would be slightly better using a "custom" system but for this demo it is sufficient
					m->GetIndices().begin(), m->GetIndices().end(),
					[this, &m, &vertices_screenSpace](uint32_t v)
					{
						if (v % 3 == 0)
						{  // Only process every 3rd index
							RenderTriangle(m.get(), vertices_screenSpace, v, false);
						}
					});
				break;
			case PrimitiveTopology::TriangleStrip:
				std::for_each(
					std::execution::par_unseq,  // Parallel execution policy - threading would be slightly better using a "custom" system but for this demo it is sufficient
					m->GetIndices().begin(), m->GetIndices().end() - 2,
					[this, &m, &vertices_screenSpace](uint32_t v)
					{
						RenderTriangle(m.get(), vertices_screenSpace, v, v % 2);
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

	void Renderer::VertexTransformationFunction(std::vector<Vector2>& screenSpace, Mesh* mesh) const
	{
		//projection stage:
		//model -> world space -> world -> view space 
		auto const m{ mesh->GetWorldMatrix() * m_Camera.viewMatrix * m_Camera.projectionMatrix };

		// Prepare the output container
		screenSpace.resize(mesh->GetVertices().size());
		mesh->GetVertices_Out_Ref().resize(mesh->GetVertices().size());

		// Transform vertices in parallel
		std::transform(
			std::execution::par,  // Parallel execution policy
			mesh->GetVertices().begin(), mesh->GetVertices().end(),
			mesh->GetVertices_Out_Ref().begin(),
			[&](auto const& v) {
				Vertex_Out vOut{};
				vOut.texcoord = v.texcoord;

				vOut.position = m.TransformPoint(v.position.ToPoint4());

				vOut.normal = mesh->GetWorldMatrix().TransformVector(v.normal);
				vOut.tangent = mesh->GetWorldMatrix().TransformVector(v.tangent);

				// View -> clipping space (NDC)
				float const inverseWComponent{ 1.f / vOut.position.w };
				vOut.position.x *= inverseWComponent;
				vOut.position.y *= inverseWComponent;	
				vOut.position.z *= inverseWComponent;

				// Convert to screen space (raster space)
				float const x_screen{ (vOut.position.x + 1) * 0.5f * static_cast<float>(m_Width) }; //center of pixel
				float const y_screen{ (1 - vOut.position.y) * 0.5f * static_cast<float>(m_Height) };

				// Store screen space coordinates
				auto const idx{ &v - mesh->GetVertices().data() }; // Calculate correct idx
				screenSpace[idx] = { x_screen, y_screen };

				return vOut;
			});
	}

	void Renderer::RenderTriangle(Mesh* m, std::vector<Vector2> const& vertices, uint32_t startVertex, bool swapVertex) const
	{
		//"Clipping" Stage
		const size_t idx1{ m->GetIndices()[startVertex + (2 * swapVertex)] };
		const size_t idx2{ m->GetIndices()[startVertex + 1] };
		const size_t idx3{ m->GetIndices()[startVertex + (!swapVertex * 2)] };

		// Not a triangle when 2 vertices are equal
		if (idx1 == idx2 || idx2 == idx3 || idx3 == idx1)
		{
			return;
		}

		//Frustum Culling
		if (Utils::IsTriangleOutsideFrustum(m, static_cast<uint32_t>(idx1), static_cast<uint32_t>(idx2), static_cast<uint32_t>(idx3)))
			return; //clipping could be applied here instead of just returning.


		//Rasterization stage
		const Vector2& vert0{ vertices[idx1] };
		const Vector2& vert1{ vertices[idx2] };
		const Vector2& vert2{ vertices[idx3] };
		float const totalTriangleArea{ (vert1.x - vert0.x) * (vert2.y - vert0.y) - (vert1.y - vert0.y) * (vert2.x - vert0.x) };

		switch (m_CurrCullMode)
		{
		case CullMode::Back:
			if (totalTriangleArea < 0.f)
			{
				return; // Back-facing, cull it
			}
			break;
		case CullMode::Front:
			if (totalTriangleArea > 0.f)
			{
				return; // Front-facing, cull it
			}
			break;
		case CullMode::None: // no face culling necessary
			break;
		default:
			break;
		}

		float const invTotalTriangleArea{ 1 / totalTriangleArea };

		//Bounding boxes logic - only loop over pixels within the smallest possible bounding box
		//Small margin is required to prevent "black lines"
		Vector2 topLeft{ Vector2::Min(vert0,Vector2::Min(vert1,vert2)) - Vector2{1.f, 1.f} };
		Vector2 topRight{ Vector2::Max(vert0,Vector2::Max(vert1,vert2)) + Vector2{1.f, 1.f} };
		
		// prevent looping over something off-screen
		topLeft.x = std::clamp(topLeft.x, 0.f, static_cast<float>(m_Width));
		topLeft.y = std::clamp(topLeft.y, 0.f, static_cast<float>(m_Height));
		topRight.x = std::clamp(topRight.x, 0.f, static_cast<float>(m_Width));
		topRight.y = std::clamp(topRight.y, 0.f, static_cast<float>(m_Height));

		for (int px{ static_cast<int>(topLeft.x) }; px < static_cast<int>(topRight.x); ++px)
		{
			for (int py{ static_cast<int>(topLeft.y) }; py < static_cast<int>(topRight.y); ++py)
			{
				ColorRGB finalColor{ colors::White };

				if (m_ShowBoundingBoxes)
				{
					m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
						static_cast<uint8_t>(finalColor.r * 255),
						static_cast<uint8_t>(finalColor.g * 255),
						static_cast<uint8_t>(finalColor.b * 255));

					continue;
				}

				Vector2 const pixel{ static_cast<float>(px) + .5f, static_cast<float>(py) + .5f };

				//Calculate barycentric coordinates
				float weight0{ Vector2::Cross((pixel - vert1), (vert1 - vert2)) * invTotalTriangleArea };
				float weight1{ Vector2::Cross((pixel - vert2), (vert2 - vert0)) * invTotalTriangleArea };
				float weight2{ Vector2::Cross((pixel - vert0), (vert0 - vert1)) * invTotalTriangleArea };

				// Not in triangle
				if (weight0 < 0.f || weight1 < 0.f || weight2 < 0.f)
					continue;

				//Normalize weights
				auto const totWeight = weight0 + weight1 + weight2;
				weight0 /= totWeight;
				weight1 /= totWeight;
				weight2 /= totWeight;

				float const depth0{ m->GetVertices_Out()[idx1].position.z };
				float const depth1{ m->GetVertices_Out()[idx2].position.z };
				float const depth2{ m->GetVertices_Out()[idx3].position.z };

				// Z-buffer uses NDC z for depth testing
				float const interpolatedDepth{ 1.f / (weight0 * (1.f / depth0) + weight1 * (1.f / depth1) + weight2 * (1.f / depth2)) };

				if (interpolatedDepth < 0.f || interpolatedDepth > 1.f || m_pDepthBufferPixels[px + py * m_Width] < interpolatedDepth)
				{
					continue;
				}
				m_pDepthBufferPixels[px + py * m_Width] = interpolatedDepth;

				// Use original clip-space w for perspective-correct attribute interpolation
				float const w0{ m->GetVertices_Out()[idx1].position.w };
				float const w1{ m->GetVertices_Out()[idx2].position.w };
				float const w2{ m->GetVertices_Out()[idx3].position.w };
				float const interpolatedW{ 1.f / (weight0 / w0 + weight1 / w1 + weight2 / w2) };

				if (m_ShowDepthBuffer)
				{
					float const remap{ Utils::DepthRemap(interpolatedDepth, .985f, 1.f) };
					finalColor = ColorRGB{ (1.f - remap) * 5,   (1.f - remap) * 5, 1.f };
				}
				else // Pixel shading
				{
					Vertex_Out pixelToShade{};
					pixelToShade.position = { static_cast<float>(px), static_cast<float>(py), interpolatedDepth,interpolatedDepth };


					//Calculate viewdirection
					Vector3 const viewDir{ (m->GetWorldMatrix().TransformPoint((weight0 * m->GetVertices()[idx1].position 
																				+ weight1 * m->GetVertices()[idx2].position 
																				+ weight2 * m->GetVertices()[idx3].position)) - m_Camera.origin).Normalized() };

					pixelToShade.texcoord = interpolatedW * ((weight0 * m->GetVertices()[idx1].texcoord) / w0
																+ (weight1 * m->GetVertices()[idx2].texcoord) / w1
																+ (weight2 * m->GetVertices()[idx3].texcoord) / w2);
					pixelToShade.normal = Vector3{ interpolatedW * (weight0 * m->GetVertices_Out()[idx1].normal / w0
																  + weight1 * m->GetVertices_Out()[idx2].normal / w1
																  + weight2 * m->GetVertices_Out()[idx3].normal / w2) }.Normalized();
					pixelToShade.tangent = Vector3{ interpolatedW * (weight0 * m->GetVertices_Out()[idx1].tangent / w0
																	+ weight1 * m->GetVertices_Out()[idx2].tangent / w1
																	+ weight2 * m->GetVertices_Out()[idx3].tangent / w2) }.Normalized();


					finalColor = PixelShading(m, pixelToShade, viewDir);
				}


				//Update Color in Buffer
				finalColor.MaxToOne();
				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}

	ColorRGB Renderer::PixelShading(Mesh const* m, Vertex_Out const& v, Vector3 const& viewDir) const
	{
		//Global light & other defines
		Vector3 static constexpr LIGHT_DIRECTION{ Vector3{.577f, -.577f, .577f} };
		ColorRGB static constexpr AMBIENT_COLOR{ 0.025f, 0.025f, 0.025f };
		float static constexpr SHININESS{ 25.0f };
		float static constexpr KD{ 7.f };

		ColorRGB result{ };

		// Normal mapping
		Vector3 const biNormal = Vector3::Cross(v.normal, v.tangent);
		Matrix const tangentSpaceAxis = { v.tangent, biNormal, v.normal, Vector3::Zero };

		ColorRGB const normalColor = m_pVehicleNormalTexture->Sample(v.texcoord);
		Vector3 sampledNormal = { normalColor.r, normalColor.g, normalColor.b }; //range [0, 1]
		sampledNormal = 2.f * sampledNormal - Vector3{ 1, 1, 1 }; //[0, 1] to [-1, 1]
		sampledNormal = tangentSpaceAxis.TransformVector(sampledNormal).Normalized();


		//calculate observed area
		float const observedArea{ std::clamp(m_UseNormalMapping ? Utils::CalculateObservedArea(sampledNormal,LIGHT_DIRECTION)
																	: Utils::CalculateObservedArea(v.normal, LIGHT_DIRECTION), 0.f, 1.f) };
		switch (m_CurrShadingMode)
		{
		case ShadingMode::ObservedArea:
		{
			result = ColorRGB{ observedArea, observedArea, observedArea };
			break;
		}
		case ShadingMode::Diffuse:
		{
			if (observedArea <= 0)
			{
				return{ colors::Black };
			}
			result = BRDF::Lambert(KD, m_pVehicleDiffuseTexture->Sample(v.texcoord)) * observedArea;
			break;
		}
		case ShadingMode::Specular:
		{
			if (observedArea <= 0)
			{
				return{ colors::Black };
			}
			result = observedArea * m_pVehicleSpecularTexture->Sample(v.texcoord).r * BRDF::Phong(1.f, SHININESS * m_pVehicleGlossinessTexture->Sample(v.texcoord).r, LIGHT_DIRECTION, viewDir, m_UseNormalMapping ? sampledNormal : v.normal);
			break;
		}
		case ShadingMode::Combined:
		{
			if (observedArea <= 0)
			{
				return{ colors::Black };
			}
			auto const lambert{ BRDF::Lambert(KD, m_pVehicleDiffuseTexture->Sample(v.texcoord)) };
			ColorRGB const phong = m_pVehicleSpecularTexture->Sample(v.texcoord).r * BRDF::Phong(1.f, SHININESS * m_pVehicleGlossinessTexture->Sample(v.texcoord).r,LIGHT_DIRECTION, viewDir, m_UseNormalMapping ? sampledNormal : v.normal);


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
