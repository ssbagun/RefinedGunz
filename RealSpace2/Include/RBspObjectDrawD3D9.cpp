#include "stdafx.h"
#include "RBspObjectDrawD3D9.h"
#include "RealSpace2.h"
#include <algorithm>
#include <numeric>

using namespace rsx;
using namespace RealSpace2;
using IndexType = u32;

template <typename T> constexpr D3DFORMAT GetD3DFormat();
template <>	constexpr D3DFORMAT GetD3DFormat<u16>() { return D3DFMT_INDEX16; }
template <> constexpr D3DFORMAT GetD3DFormat<u32>() { return D3DFMT_INDEX32; }

template <typename VertexType, typename IndexType>
static bool CreateBuffers(size_t VertexCount, size_t IndexCount, u32 FVF,
	D3DPtr<IDirect3DVertexBuffer9>& vb, D3DPtr<IDirect3DIndexBuffer9>& ib)
{
	auto hr = RGetDevice()->CreateVertexBuffer(VertexCount * sizeof(VertexType), 0,
		FVF, D3DPOOL_MANAGED,
		MakeWriteProxy(vb), nullptr);
	if (FAILED(hr))
	{
		MLog("RBspObjectDrawD3D9::Create -- Failed to create vertex buffer\n");
		return false;
	}

	hr = RGetDevice()->CreateIndexBuffer(IndexCount * sizeof(IndexType), 0,
		GetD3DFormat<IndexType>(), D3DPOOL_MANAGED,
		MakeWriteProxy(ib), nullptr);
	if (FAILED(hr))
	{
		MLog("RBspObjectDrawD3D9::Create -- Failed to create index buffer\n");
		return false;
	}

	return true;
}

void RBspObjectDrawD3D9::CreateTextures()
{
	std::unordered_map<std::string, int> TexMap;

	Textures.resize(State.Materials.size());
	for (size_t i{}; i < State.Materials.size(); ++i)
	{
		auto& Mat = State.Materials[i];

		auto LoadTexture = [&](auto& name) -> int
		{
			if (name.empty())
				return -1;

			{
				auto it = TexMap.find(name);
				if (it != TexMap.end())
					return it->second;
			}

			TextureMemory.emplace_back();
			auto hr = D3DXCreateTextureFromFile(RGetDevice(), name.c_str(),
				MakeWriteProxy(TextureMemory.back()));
			if (FAILED(hr))
			{
				DMLog("Failed to load texture %s\n", name);
				TextureMemory.pop_back();
				return -1;
			}
			auto idx = TextureMemory.size() - 1;
			TexMap.emplace(std::make_pair(name, idx));
			return idx;
		};

		auto& Tex = Textures[i];
		Tex.Diffuse = LoadTexture(Mat.tDiffuse);
		Tex.Opacity = LoadTexture(Mat.tOpacity);
		Tex.AlphaTestValue = Mat.AlphaTestValue;
	}
}

void RBspObjectDrawD3D9::CreateBatches()
{
	struct DrawPropStuff
	{
		int Mesh;
		u32 IndexBase;
		u32 Count;
	};

	struct MaterialStuff
	{
		int Data;
		std::vector<DrawPropStuff> DrawProps;
	};

	std::vector<MaterialStuff> Materials;
	Materials.resize(State.Materials.size());

	// Sort draw props per material
	for (size_t DataIndex{}; DataIndex < State.ObjectData.size(); ++DataIndex)
	{
		auto& Data = State.ObjectData[DataIndex];
		for (size_t MeshIndex{}; MeshIndex < Data.Meshes.size(); ++MeshIndex)
		{
			auto& Mesh = Data.Meshes[MeshIndex];
			for (auto& dp : Mesh.DrawProps)
			{
				auto& Mat = Materials[Data.MaterialStart + dp.material];
				Mat.Data = DataIndex;

				Mat.DrawProps.push_back({
					static_cast<int>(MeshIndex),
					dp.indexBase,
					dp.count });

			}
		}
	}

	if (!CreateBuffers<Vertex, IndexType>(
		State.TotalVertexCount, State.TotalIndexCount,
		GetFVF(), VertexBuffer, IndexBuffer))
	{
		MLog("RBspObjectDrawD3D9::Create -- Failed to create vertex and index buffers\n");
		return;
	}

	DMLog("Create vertex buffer with %d vertices, index buffer with %d indices\n",
		State.TotalVertexCount, State.TotalIndexCount);

	// Lock vertex and index buffers.
	auto Lock = [](auto& Buffer, auto*& Pointer)
	{
		if (FAILED(Buffer->Lock(0, 0, reinterpret_cast<void**>(&Pointer), 0)))
		{
			MLog("Oh no!\n");
			return false;
		}
		return true;
	};
	char* Verts{};        if (!Lock(VertexBuffer, Verts))  return;
	IndexType* Indices{}; if (!Lock(IndexBuffer, Indices)) return;

	// Insert transformed vertices for each object into the vertex buffer
	// and keep a list of the offset of each mesh's vertices.

	// Dimensions: [Data][Object][Mesh]
	// TODO: Change this terrible type
	std::vector<std::vector<std::vector<int>>> MeshVertexOffsets;
	MeshVertexOffsets.resize(State.Objects.size());
	int CumulativeVertexBase{};
	for (auto& pair : State.ObjectMap)
	{
		auto& Data = State.ObjectData[pair.first];
		auto& DataOffsets = MeshVertexOffsets[pair.first];
		auto ObjectIndexCount = pair.second.size();
		DataOffsets.resize(ObjectIndexCount);
		for (size_t ObjectListIndex{}; ObjectListIndex < ObjectIndexCount; ++ObjectListIndex)
		{
			auto ObjectIndex = pair.second[ObjectListIndex];
			auto& Object = State.Objects[ObjectIndex];

			auto& MeshOffsets = DataOffsets[ObjectListIndex];
			auto MeshCount = Data.Meshes.size();
			MeshOffsets.resize(MeshCount);
			for (size_t MeshIndex{}; MeshIndex < MeshCount; ++MeshIndex)
			{
				auto& Mesh = Data.Meshes[MeshIndex];
				MeshOffsets[MeshIndex] = CumulativeVertexBase;

				for (size_t i{}; i < Mesh.VertexCount; ++i)
				{
					auto AddVertexData = [&](auto&& val) {
						memcpy(Verts, &val, sizeof(val));
						Verts += sizeof(val);
					};
					AddVertexData(Mesh.Positions[i] * Mesh.World * Object.World);
					AddVertexData(Mesh.TexCoords[i]);
				}
				CumulativeVertexBase += Mesh.VertexCount;
			}
		}
	}

	// Insert indices for each object
	MaterialBatches.resize(State.Materials.size());
	int CumulativeIndexBase{};
	for (size_t MaterialIndex{}; MaterialIndex < Materials.size(); ++MaterialIndex)
	{
		auto& Mat = Materials[MaterialIndex];
		auto& Batch = MaterialBatches[MaterialIndex];

		Batch.TriangleCount = 0;
		Batch.VertexBase = 0;
		Batch.IndexBase = CumulativeIndexBase;

		auto& Data = State.ObjectData[Mat.Data];
		auto& ObjectIndices = State.ObjectMap[Mat.Data];
		assert(!ObjectIndices.empty());

		DMLog("Create indices for material %d\n", MaterialIndex);

		for (size_t ObjectListIndex{}; ObjectListIndex < ObjectIndices.size(); ++ObjectListIndex)
		{
			for (auto& dp : Mat.DrawProps)
			{
				for (size_t i{}; i < dp.Count; ++i)
				{
					for (size_t j{}; j < 3; ++j)
					{
						auto Index = Data.Meshes[dp.Mesh].Indices[dp.IndexBase + i * 3 + j];
						auto VertexOffset = MeshVertexOffsets[Mat.Data][ObjectListIndex][dp.Mesh];
						*Indices = VertexOffset + Index;
						++Indices;
					}
				}

				CumulativeIndexBase += dp.Count * 3;
				Batch.TriangleCount += dp.Count;
			}
		}

		DMLog("Batch: TriangleCount = %d, VertexBase = %d, IndexBase = %d\n",
			Batch.TriangleCount, Batch.VertexBase, Batch.IndexBase);
	}

	DMLog("Added %d vertices, %d indices\n", CumulativeVertexBase, CumulativeIndexBase);

	// Unlock vertex and index buffers.
	VertexBuffer->Unlock();
	IndexBuffer->Unlock();

	NumNormalMaterials = MaterialBatches.size();
}

void RBspObjectDrawD3D9::Create(rsx::LoaderState && srcState)
{
	State = std::move(srcState);

	CreateTextures();
	CreateBatches();
	
	DMLog("RBspObjectDrawD3D9 created\n");
}

static void SetWireframeStates()
{
	RGetDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	RGetDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	RGetDevice()->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	RGetDevice()->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	RGetDevice()->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	RGetDevice()->SetRenderState(D3DRS_LIGHTING, FALSE);
}

static void SetDefaultStates()
{
	bool Trilinear = RIsTrilinear();

	RGetDevice()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	RGetDevice()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	RGetDevice()->SetSamplerState(0, D3DSAMP_MIPFILTER, Trilinear ? D3DTEXF_LINEAR : D3DTEXF_NONE);
	RGetDevice()->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	RGetDevice()->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	RGetDevice()->SetSamplerState(1, D3DSAMP_MIPFILTER, Trilinear ? D3DTEXF_LINEAR : D3DTEXF_NONE);

	RGetDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	RGetDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	RGetDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	RGetDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

	RGetDevice()->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	RGetDevice()->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
}

void RBspObjectDrawD3D9::Draw()
{
	auto dev = RGetDevice();

	if (true)
		SetDefaultStates();
	else
		SetWireframeStates();

	// Enable Z-buffer testing and writing.
	RSetWBuffer(true);
	dev->SetRenderState(D3DRS_ZWRITEENABLE, true);
	dev->SetRenderState(D3DRS_LIGHTING, FALSE);
	dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	// Map triangles are clockwise, so cull counter-clockwise faces.
	dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

	dev->SetFVF(GetFVF());
	dev->SetStreamSource(0, VertexBuffer.get(), 0, GetStride());
	dev->SetIndices(IndexBuffer.get());

	// Set render states for normal materials
	dev->SetRenderState(D3DRS_ZWRITEENABLE, true);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
	dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

	for (int i{}; i < NumNormalMaterials; ++i)
	{
		auto& Mat = MaterialBatches[i];
		auto& Tex = Textures[i];

		if (Tex.Diffuse == -1)
			continue;

		dev->SetTexture(0, TextureMemory[Tex.Diffuse].get());

		auto hr = dev->DrawIndexedPrimitive(
			D3DPT_TRIANGLELIST,    // Primitive type
			Mat.VertexBase,        // BaseVertexIndex
			0,                     // MinVertexIndex
			Mat.TriangleCount * 3, // NumVertices
			Mat.IndexBase,         // Start index
			Mat.TriangleCount);    // Primitive count

		assert(SUCCEEDED(hr));
	}

	// Disable alpha testing
	dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE, false);
	dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	// Disable alpha blending
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
	dev->SetRenderState(D3DRS_ZWRITEENABLE, true);

	// Reset all textures and texture states
	dev->SetTexture(0, nullptr);
	dev->SetTexture(1, nullptr);
	dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	dev->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);
}