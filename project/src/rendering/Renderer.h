#pragma once

#include "pch.h"

#include "Camera.h"
#include "Effect.h"
#include <memory>

#include "Mesh.h"

struct SDL_Window;
struct SDL_Surface;

namespace mau
{
	class Texture;
	class Mesh;

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(const Timer* pTimer);
		void Render() const;

	#pragma region RenderSettings
		// When F1 is pressed, switch between software and hardware rasterizer
		void ToggleRasterizerMode() noexcept
		{
			if (!m_IsDirectXInitialized)
			{
				std::cout << RED << "DirectX not Initialized\n" << RESET;
				return;
			}

			m_IsSoftwareRasterizerMode = !m_IsSoftwareRasterizerMode;

			if (m_IsSoftwareRasterizerMode)
			{
				std::cout << "Rasterizer -> " << GREEN << "Software\n";
				std::cout << RESET;
				return;
			}
			std::cout << "Rasterizer -> " << GREEN << "Hardware\n";
			std::cout << RESET;
		}
		// When F2 is pressed, switch between rotating or not rotating the models
		void ToggleRotationMode() noexcept
		{
			m_IsRotationMode = !m_IsRotationMode;
			if (m_IsRotationMode)
			{
				std::cout << "Rotation -> " << GREEN << "Enabled\n";
				std::cout << RESET;
				return;
			}
			std::cout << "Rotation -> " << RED << "Disabled\n";
			std::cout << RESET;
		}
		// When F3 is pressed, switch between displaying or not displaying the fire mesh (only for the hardware rasterizer currently)
		void ToggleFireMesh() noexcept
		{
			if (m_IsSoftwareRasterizerMode)
			{
				std::cout << RED << "Only available in hardware rasterizer\n" << RESET;
				return;
			}
			m_DisplayFireMesh = !m_DisplayFireMesh;
			if (m_DisplayFireMesh)
			{
				std::cout << "FireMesh -> " << GREEN << "Enabled\n";
				std::cout << RESET;
				return;
			}
			std::cout << "FireMesh -> " << RED << "Disabled\n";
			std::cout << RESET;
		}
		// When F4 is pressed, change to next sampler state (only for the hardware rasterizer currently)
		void ChangeSamplerState() noexcept;
		// When F5 is pressed change to the next shading mode (only for the software rasterizer currently)
		void ChangeShadingMode() noexcept
		{
			if (!m_IsSoftwareRasterizerMode)
			{
				std::cout << RED << "Not in software rasterizer, can not cycle shading mode setting\n" << RESET;
				return;
			}
			auto curr{ static_cast<uint8_t>(m_CurrShadingMode) };
			++curr %= static_cast<uint8_t>(ShadingMode::COUNT);

			m_CurrShadingMode = static_cast<ShadingMode>(curr);

			switch (m_CurrShadingMode)
			{
			case ShadingMode::ObservedArea:
				std::cout << "Shading mode -> " << GREEN << "ObservedArea\n";
				std::cout << RESET;
				break;
			case ShadingMode::Diffuse:
				std::cout << "Shading mode -> " << GREEN << "Diffuse\n";
				std::cout << RESET;
				break;
			case ShadingMode::Specular:
				std::cout << "Shading mode -> " << GREEN << "Specular\n";
				std::cout << RESET;
				break;
			case ShadingMode::Combined:
				std::cout << "Shading mode -> " << GREEN << "Combined\n";
				std::cout << RESET;
				break;
			default: break;
			}
		}
		//When F6 is pressed, toggle the normal map (only for the software rasterizer currently)
		void ToggleNormalMap() noexcept
		{
			if (!m_IsSoftwareRasterizerMode)
			{
				std::cout << RED << "Not in software rasterizer, can not toggle normal map buffer setting\n" << RESET;
				return;
			}

			m_UseNormalMapping = !m_UseNormalMapping;
			if (m_UseNormalMapping)
			{
				std::cout << "Normal mapping -> " << GREEN << "Enabled\n";
				std::cout << RESET;
				return;
			}
			std::cout << "Normal mapping -> " << RED << "Disabled\n";
			std::cout << RESET;
		}
		// When F7 is pressed, toggle the depth buffer visualization (only for the software rasterizer currently)
		void ToggleDisplayDepthBuffer() noexcept
		{
			if (!m_IsSoftwareRasterizerMode)
			{
				std::cout << RED << "Not in software rasterizer, can not toggle depth buffer setting\n" << RESET;
				return;
			}

			m_ShowDepthBuffer = !m_ShowDepthBuffer;
			if (m_ShowDepthBuffer)
			{
				std::cout << "Show depth buffer -> " << GREEN << "Enabled\n";
				std::cout << RESET;
				return;
			}
			std::cout << "Show depth buffer -> " << RED << "Disabled\n";
			std::cout << RESET;
		}
		// When F8 is pressed, toggle the bounding box visualization (only for the software rasterizer currently)
		void ToggleBoundingBoxes() noexcept
		{
			if (!m_IsSoftwareRasterizerMode)
			{
				std::cout << RED << "Not in software rasterizer, can not toggle bounding boxes setting\n" << RESET;
				return;
			}

			m_ShowBoundingBoxes = !m_ShowBoundingBoxes;
			if (m_ShowBoundingBoxes)
			{
				std::cout << "Show bounding boxes -> " << GREEN << "Enabled\n";
				std::cout << RESET;
				return;
			}
			std::cout << "Show bounding boxes -> " << RED << "Disabled\n";
			std::cout << RESET;
		}
		// When F9 is pressed, switch to the next cull mode
		void ChangeCullMode() noexcept
		{
			auto curr{ static_cast<uint8_t>(m_CurrCullMode) };
			++curr %= static_cast<uint8_t>(CullMode::COUNT);

			m_CurrCullMode = static_cast<CullMode>(curr);

			switch (m_CurrCullMode)
			{
			case CullMode::Back:
				std::cout << "Culling mode -> "<< GREEN << "Back\n";
				std::cout << RESET;
				break;
			case CullMode::Front:
				std::cout << "Culling mode -> " << GREEN << "Front\n";
				std::cout << RESET;
				break;
			case CullMode::None:
				std::cout << "Culling mode -> " << GREEN << "None\n";
				std::cout << RESET;
				break;
			default: break;
			}

			if (!m_IsDirectXInitialized)
			{
				std::cout << RED << "DirectX not Initialized\n" << RESET;
				return;
			}

			for(auto& m : m_Meshes)
			{
				if (m == m_Meshes[1])
				{
					continue;
				}
				m->SetCullingMode(m_pDevice, curr);
			}
		}
		// When F10 is pressed, toggle to display the uniform clear color (or not)
		void ToggleUniformClearColor() noexcept
		{
			m_DisplayUniformClearColor = !m_DisplayUniformClearColor;
			if (m_DisplayUniformClearColor)
			{
				std::cout << "Use uniform clear color ->" << GREEN << " Enabled\n";
				std::cout << RESET;
				return;
			}
			std::cout << "Use uniform clear color ->" << RED << " Disabled\n";
			std::cout << RESET;
		}
	#pragma endregion

	private:
		//Window
		SDL_Window* m_pWindow{};
		int m_Width{};
		int m_Height{};

		Camera m_Camera{};

		//Software rasterizer
		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{ nullptr };

		mutable std::vector<float> m_DepthBuffer{};
		mutable std::vector<Vertex_Out> m_ClipSpaceVertices{};
		mutable std::vector<Vertex_Out> m_NdcVertices{};     // Post perspective-divide
		mutable std::vector<Vector2> m_ScreenVertices{};      // Screen-space coordinates


		//DirectX
		bool m_IsDirectXInitialized{ false }; // Only want to render when DirectX is properly initialized
		ID3D11Device* m_pDevice{ nullptr };
		ID3D11DeviceContext* m_pDeviceContext{ nullptr };
		IDXGISwapChain* m_pSwapChain{ nullptr };
		ID3D11Texture2D* m_pDepthStencilBuffer{ nullptr };
		ID3D11DepthStencilView* m_pDepthStencilView{ nullptr };
		ID3D11Resource* m_pRenderTargetBuffer{ nullptr };
		ID3D11RenderTargetView* m_pRenderTargetView{ nullptr };

		//Effects
		// would be shared with resource manager
		std::shared_ptr<BaseEffect> m_pVehicleEffect{ nullptr };
		std::shared_ptr<BaseEffect> m_pFireEffect{ nullptr };

		//Models
		std::vector<std::unique_ptr<Mesh>> m_Meshes;

		//Textures
		// would be in resource manager
		std::unique_ptr<Texture> m_pVehicleDiffuseTexture{ nullptr };
		std::unique_ptr<Texture> m_pVehicleNormalTexture{ nullptr };
		std::unique_ptr<Texture> m_pVehicleGlossinessTexture{ nullptr };
		std::unique_ptr<Texture> m_pVehicleSpecularTexture{ nullptr };
		std::unique_ptr<Texture> m_pFireDiffuseTexture{ nullptr };

		//Settings
		bool m_IsSoftwareRasterizerMode{ true };
		bool m_IsRotationMode{ true };
		bool m_DisplayFireMesh{ true };
		SamplerState m_SamplerState{ SamplerState::Point };
		enum class ShadingMode : uint8_t
		{
			ObservedArea = 0,
			Diffuse = 1,
			Specular = 2,
			Combined = 3,
			COUNT
		};
		ShadingMode m_CurrShadingMode{ ShadingMode::Combined };
		bool m_UseNormalMapping{ true };
		bool m_ShowDepthBuffer{ false };
		bool m_ShowBoundingBoxes{ false };
		CullMode m_CurrCullMode{ CullMode::Back };
		bool m_DisplayUniformClearColor{ false };

		// Screen clear colors
		float static constexpr UNIFORM_COLOR[4] = { .1f, .1f, .1f, 1.f };
		float static constexpr HARDWARE_COLOR[4] = { .39f, .59f, .93f, 1.f };
		float static constexpr SOFTWARE_COLOR[4] = {.30f, .39f, .39f, 1.f};
		// End settings

		//Hardware
		HRESULT InitializeDirectX();
		void RenderDirectXHardware() const;

		//Software
		void RenderSoftware() const;
		void VertexTransformationFunction(Mesh const* mesh, std::vector<Vertex_Out>& clipSpaceVertices) const;
		void RenderTriangle(Mesh const* m, std::vector<Vertex_Out> const& clipSpaceVertices, uint32_t startVertex, bool swapVertex) const;
		void RasterizeTriangle(Mesh const* m,
			Vector2 const& v0Screen, Vector2 const& v1Screen, Vector2 const& v2Screen,
			Vertex_Out const& v0, Vertex_Out const& v1, Vertex_Out const& v2,
			Vector3 const& pos0World, Vector3 const& pos1World, Vector3 const& pos2World) const;

		[[nodiscard]] ColorRGB PixelShading(const Mesh* m, const Vertex_Out& v, const Vector3& viewDir) const;
	};
}
