#pragma once
#include "Effect.h"
#include "Matrix.h"
#include "Vertex_In.h"
#include "Texture.h"


namespace mau
{
	enum class PrimitiveTopology : uint8_t
	{
		TriangleList,
		TriangleStrip
	};

	class Mesh final
	{
	public:
		Mesh(ID3D11Device* pDevice, std::string const& path, std::shared_ptr<BaseEffect> pEffect);
		~Mesh()
		{
			SAFE_RELEASE(m_pInputLayout)
			SAFE_RELEASE(m_pVertexBuffer)
			SAFE_RELEASE(m_pIndexBuffer)
		}
		void Render(ID3D11DeviceContext* pDeviceContext) const
		{
			//Set primitive topology
			switch (m_PrimitiveTopology)
			{
			case PrimitiveTopology::TriangleList:
				pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				break;
			case PrimitiveTopology::TriangleStrip:
				pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				break;
			}

			//Set input layout
			pDeviceContext->IASetInputLayout(m_pInputLayout);

			//Set vertex buffer
			UINT constexpr stride{ sizeof(Vertex_In) };
			UINT constexpr offset{ 0 };
			pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

			//Set index buffer
			pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			//Draw
			D3DX11_TECHNIQUE_DESC techDesc{};
			m_pEffect->GetTechnique()->GetDesc(&techDesc);
			for (UINT p = 0; p < techDesc.Passes; ++p)
			{
				m_pEffect->GetTechnique()->GetPassByIndex(p)->Apply(0, pDeviceContext);
				pDeviceContext->DrawIndexed(static_cast<uint32_t>(m_Indices.size()), 0, 0);
			}
		}

		// Position
		void UpdateCameraPos(Vector3 const& cameraPos)
		{
			m_pEffect->SetCameraPosition(cameraPos);
		}
		void UpdateEffectMatrices(Matrix const& viewProjectionMatrix)
		{
			m_pEffect->SetWorldViewProjectionMatrix(m_WorldMatrix * viewProjectionMatrix);
			m_pEffect->SetWorldMatrix(m_WorldMatrix);
		}

		// Movement
		void Translate(Vector3 const& t) noexcept
		{
			m_WorldMatrix = Matrix::CreateTranslation(t.x, t.y, t.z) * m_WorldMatrix;
		}
		void RotateY(float r) noexcept
		{
			m_WorldMatrix =  Matrix::CreateRotationY(r) * m_WorldMatrix;
		}

		//Some functions required for software rasterizer
		[[nodiscard]] PrimitiveTopology GetPrimitiveTopology() const noexcept
		{
			return m_PrimitiveTopology;
		}

		[[nodiscard]] std::vector<uint32_t> const& GetIndices() const noexcept
		{
			return m_Indices;
		}

		[[nodiscard]] std::vector<Vertex_Out>& GetVertices_Out_Ref() noexcept
		{
			return m_Vertices_Out;
		}
		[[nodiscard]] std::vector<Vertex_Out> const& GetVertices_Out() const noexcept
		{
			return m_Vertices_Out;
		}

		[[nodiscard]] std::vector<Vertex_In> const& GetVertices() const noexcept
		{
			return m_Vertices;
		}

		[[nodiscard]] Matrix const& GetWorldMatrix() const noexcept
		{
			return m_WorldMatrix;
		}

		void SetSamplingMode(uint8_t mode) noexcept
		{
			assert(m_pEffect);
			m_pEffect->SetSamplingMode(mode);

		}

		void SetCullingMode(ID3D11Device* pDevice, uint8_t mode) noexcept
		{
			assert(m_pEffect);
			m_pEffect->SetCullingMode(pDevice, mode);
		}

		Mesh(const Mesh&) = delete;
		Mesh(Mesh&&) noexcept = delete;
		Mesh& operator=(const Mesh&) = delete;
		Mesh& operator=(Mesh&&) noexcept = delete;

	private:
		Matrix m_WorldMatrix{};

		//These don't have to be stored for the hardware rasterizer but are necessare for software.
		std::vector<Vertex_In> m_Vertices{};
		std::vector<Vertex_Out> m_Vertices_Out{};
		std::vector<uint32_t> m_Indices{};
		PrimitiveTopology m_PrimitiveTopology{ PrimitiveTopology::TriangleList };

		// would be shared with resource manager
		std::shared_ptr<BaseEffect> m_pEffect{};

		ID3D11InputLayout* m_pInputLayout{};
		ID3D11Buffer* m_pVertexBuffer{};
		ID3D11Buffer* m_pIndexBuffer{};
	};
}