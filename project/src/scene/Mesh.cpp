#include "Mesh.h"
#include "Utils.h"

mau::Mesh::Mesh(ID3D11Device* pDevice, std::string const& path, std::shared_ptr<BaseEffect> pEffect)
{
	//Initialize models
	m_Vertices = {};
	m_Indices = {};
	Utils::ParseOBJ(path, m_Vertices, m_Indices);

	m_pEffect = pEffect;
	assert(m_pEffect);

	//Create vertex layout based on vertex struct
	static constexpr uint32_t numElements{ 4 };
	D3D11_INPUT_ELEMENT_DESC layout[numElements]{};

	layout[0].SemanticName = "POSITION";
	layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layout[0].AlignedByteOffset = 0; // float3
	layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	layout[1].SemanticName = "TEXCOORD";
	layout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	layout[1].AlignedByteOffset = 12; //float2
	layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	layout[2].SemanticName = "NORMAL";
	layout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layout[2].AlignedByteOffset = 20; //float3
	layout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	layout[3].SemanticName = "TANGENT";
	layout[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layout[3].AlignedByteOffset = 32; //float3
	layout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	//Create input layout
	ID3DX11EffectTechnique* pTechnique = m_pEffect->GetTechnique();

	D3DX11_PASS_DESC passDesc{};
	pTechnique->GetPassByIndex(0)->GetDesc(&passDesc);

	HRESULT hr = pDevice->CreateInputLayout(
		layout,
		numElements,
		passDesc.pIAInputSignature,
		passDesc.IAInputSignatureSize,
		&m_pInputLayout);

	if (FAILED(hr))
		assert(false && "Failed to create input layout");

	//Create vertex buffer
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.ByteWidth = sizeof(Vertex_In) * static_cast<uint32_t>(m_Vertices.size());
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = m_Vertices.data();

	hr = pDevice->CreateBuffer(&bufferDesc, &initData, &m_pVertexBuffer);

	if (FAILED(hr))
		assert(false && "Failed to create vertex buffer");

	//Create index buffer
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.ByteWidth = sizeof(uint32_t) * static_cast<uint32_t>(m_Indices.size());
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	initData.pSysMem = m_Indices.data();

	hr = pDevice->CreateBuffer(&bufferDesc, &initData, &m_pIndexBuffer);
	if (FAILED(hr))
		assert(false && "Failed to create index buffer");
}
