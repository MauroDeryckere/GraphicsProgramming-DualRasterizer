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
			float const u{ std::clamp(uv.x, 0.f, 1.f) };
			float const v{ std::clamp(uv.y, 0.f, 1.f) };
			uint32_t const x{ std::min(static_cast<uint32_t>(u * m_pSurface->w), static_cast<uint32_t>(m_pSurface->w - 1)) };
			uint32_t const y{ std::min(static_cast<uint32_t>(v * m_pSurface->h), static_cast<uint32_t>(m_pSurface->h - 1)) };

			uint8_t r{};
			uint8_t g{};
			uint8_t b{};
			SDL_GetRGB(m_pSurfacePixels[(y * m_pSurface->w) + x], m_pSurface->format, &r, &g, &b);
			static constexpr float normalizedFactor{ 1 / 255.f };
			return { r * normalizedFactor, g * normalizedFactor, b * normalizedFactor };
		 }
		

	private:
		SDL_Surface* m_pSurface{ nullptr };
		uint32_t* m_pSurfacePixels{ nullptr };

		ID3D11Texture2D* m_pResource{};
		ID3D11ShaderResourceView* m_pShaderResourceView{};
	};
}

