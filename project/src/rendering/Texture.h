#pragma once

#include "pch.h"
#include "ColorRGB.h"
#include "Vector2.h"
#include <filesystem>

namespace mau
{
	class Texture final
	{
	public:
		Texture(std::filesystem::path const& path, ID3D11Device* pDevice)
		{
			assert(std::filesystem::exists(path));

			m_pSurface = IMG_Load(path.string().c_str());
			if (!m_pSurface)
				throw std::runtime_error("Failed to load texture from path: " + path.string());

			m_pSurfacePixels = reinterpret_cast<uint32_t*>(m_pSurface->pixels);

			// Cache surface properties for fast sampling
			m_Width = static_cast<uint32_t>(m_pSurface->w);
			m_Height = static_cast<uint32_t>(m_pSurface->h);
			m_MaxX = m_Width - 1;
			m_MaxY = m_Height - 1;

			// Pre-compute channel shifts from surface format
			auto const* fmt = m_pSurface->format;
			m_RShift = fmt->Rshift;
			m_GShift = fmt->Gshift;
			m_BShift = fmt->Bshift;
			m_AShift = fmt->Ashift;

			assert(pDevice);


			DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = m_pSurface->w;
			desc.Height = m_pSurface->h;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = format;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA initData{};
			initData.pSysMem = m_pSurface->pixels;
			initData.SysMemPitch = static_cast<UINT>(m_pSurface->pitch);
			initData.SysMemSlicePitch = static_cast<UINT>(m_pSurface->h * m_pSurface->pitch);
			
			HRESULT hr = pDevice->CreateTexture2D(&desc, &initData, &m_pResource);
			
			if (FAILED(hr))
				throw std::runtime_error("Failed to create texture from path: " + path.string());
		
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			hr = pDevice->CreateShaderResourceView(m_pResource, &srvDesc, &m_pShaderResourceView);
			if (FAILED(hr))
				throw std::runtime_error("Failed to create shader resource view from path: " + path.string());
		}
		~Texture()
		{
			if (m_pSurface)
			{
				SDL_FreeSurface(m_pSurface);
			}
			SAFE_RELEASE(m_pShaderResourceView)
			SAFE_RELEASE(m_pResource)
		}

		ID3D11Texture2D* GetResource() const
		{
			return m_pResource;
		}
		ID3D11ShaderResourceView* GetShaderResourceView() const
		{
			return m_pShaderResourceView;
		}

		Texture(const Texture&) = delete;
		Texture(Texture&&) noexcept = delete;
		Texture& operator=(const Texture&) = delete;
		Texture& operator=(Texture&&) noexcept = delete;

		ColorRGB Sample(const Vector2& uv) const
		{
			uint32_t const x{ std::min(static_cast<uint32_t>(std::clamp(uv.x, 0.f, 1.f) * m_Width), m_MaxX) };
			uint32_t const y{ std::min(static_cast<uint32_t>(std::clamp(uv.y, 0.f, 1.f) * m_Height), m_MaxY) };

			uint32_t const pixel{ m_pSurfacePixels[y * m_Width + x] };
			return {
				static_cast<float>((pixel >> m_RShift) & 0xFF) * m_InvFF,
				static_cast<float>((pixel >> m_GShift) & 0xFF) * m_InvFF,
				static_cast<float>((pixel >> m_BShift) & 0xFF) * m_InvFF
			};
		}

		[[nodiscard]] float SampleAlpha(const Vector2& uv) const
		{
			uint32_t const x{ std::min(static_cast<uint32_t>(std::clamp(uv.x, 0.f, 1.f) * m_Width), m_MaxX) };
			uint32_t const y{ std::min(static_cast<uint32_t>(std::clamp(uv.y, 0.f, 1.f) * m_Height), m_MaxY) };

			uint32_t const pixel{ m_pSurfacePixels[y * m_Width + x] };
			return static_cast<float>((pixel >> m_AShift) & 0xFF) * m_InvFF;
		}


	private:
		SDL_Surface* m_pSurface{ nullptr };
		uint32_t* m_pSurfacePixels{ nullptr };

		// Cached for fast sampling
		uint32_t m_Width{};
		uint32_t m_Height{};
		uint32_t m_MaxX{};
		uint32_t m_MaxY{};
		uint8_t m_RShift{};
		uint8_t m_GShift{};
		uint8_t m_BShift{};
		uint8_t m_AShift{};
		static constexpr float m_InvFF{ 1.f / 255.f };

		ID3D11Texture2D* m_pResource{};
		ID3D11ShaderResourceView* m_pShaderResourceView{};
	};
}

