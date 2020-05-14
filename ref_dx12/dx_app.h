#pragma once

#ifndef WIN32
#error "DirectX renderer can run only on Windows"
#endif // !WIN32


#include <windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>

#include "d3dx12.h"
#include "dx_texture.h"
#include "dx_common.h"
#include "dx_objects.h"
#include "dx_buffer.h"
#include "dx_shaderdefinitions.h"
#include "dx_glmodel.h"
#include "dx_camera.h"

extern "C"
{
	#include "../client/ref.h"
};

namespace FArg 
{
	struct UpdateUploadHeapBuff
	{
		ComPtr<ID3D12Resource> buffer;
		int offset = -1;
		const void* data = 0;
		int byteSize = -1;
		int alignment = -1;
	};
};


//#TODO 
// 1) currently I do : Update - Draw, Update - Draw. It should be Update Update , Draw Draw (especially text)
// 2) One huge vertex buffer for both streaming and persistent, minimize transitions if step 1 is fulfilled
//    and buffer allocation
// 3) Make your wrappers as exclusive owners of some resource, and operate with smart pointers instead to avoid mess
//    during resource management.(This requires rewrite some stuff like Textures or buffers)
// 4) For Movies and UI we don't need stream drawing, but just one quad object  and the width and height would be
//	  scaling of this quad along y or x axis
class Renderer
{
private:
	Renderer();

	constexpr static DXGI_FORMAT QBACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr static DXGI_FORMAT QDEPTH_STENCIL_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;
	constexpr static int		 QSWAP_CHAIN_BUFFER_COUNT = 2;
	constexpr static bool		 QMSAA_ENABLED = false;
	constexpr static int		 QMSAA_SAMPLE_COUNT = 4;
	constexpr static int		 QTRANSPARENT_TABLE_VAL = 255;
	constexpr static int		 QCBV_SRV_DESCRIPTORS_NUM = 256;
	constexpr static int		 QCONST_BUFFER_ALIGNMENT = 256;
	constexpr static int		 QCONST_BUFFER_SIZE = 256 * 1024 * 1024;
	constexpr static int		 QSTREAMING_VERTEX_BUFFER_SIZE = 256 * 2048;

	constexpr static char		 QRAW_TEXTURE_NAME[] = "__DX_MOVIE_TEXTURE__";
	constexpr static char		 QFONT_TEXTURE_NAME[] = "conchars";

	constexpr static bool		 QDEBUG_LAYER_ENABLED = false;

public:

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	~Renderer() = default;

	static Renderer& Inst();

	const refimport_t& GetRefImport() const { return m_refImport; };
	void SetRefImport(refimport_t RefImport) { m_refImport = RefImport; };

	// Shader resource view management
	void FreeSrvSlot(int slotIndex);
	int AllocSrvSlot();

	// Buffers management
	void DeleteConstantBuffMemory(int offset);
	void DeleteResources(ComPtr<ID3D12Resource> resourceToDelete);
	
	void UpdateStreamingConstantBuffer(XMFLOAT4 position, XMFLOAT4 scale, int offset);
	void UpdateGraphicalObjectConstantBuffer(const GraphicalObject& obj);

	Texture* FindOrCreateTexture(std::string_view textureName);

	/* API functions */
	void BeginFrame();
	void EndFrame();
	void Init(WNDPROC WindowProc, HINSTANCE hInstance);
	void Draw_Pic(int x, int y, const char* name);
	void Draw_RawPic(int x, int y, int quadWidth, int quadHeight, int textureWidth, int textureHeight, const std::byte* data);
	void Draw_Char(int x, int y, int num);
	void GetDrawTextureSize(int* x, int* y, const char* name) const;
	void SetPalette(const unsigned char* palette);
	void RegisterWorldModel(const char* model);
	void RenderFrame(const refdef_t& frameUpdateData);
	Texture* RegisterDrawPic(const char* name);

private:

	/* Initialize win32 specific stuff */
	void InitWin32(WNDPROC WindowProc, HINSTANCE hInstance);
	/* Initialize DirectX stuff */
	void InitDX();

	void EnableDebugLayer();

	void InitUtils();

	void InitScissorRect();

	void InitViewport();

	void CreateDepthStencilBufferAndView();

	void CreateRenderTargetViews();

	void CreateDescriptorsHeaps();

	void CreateSwapChain();

	void CheckMSAAQualitySupport();

	void CreateCmdAllocatorAndCmdList();

	void CreateCommandQueue();

	void CreateFences();
	void InitDescriptorSizes();
	void CreateDevice();
	void CreateDxgiFactory();

	void CreateRootSignature();
	void CreatePipelineState();
	void CreateInputLayout();
	void LoadShaders();

	void CreateTextureSampler();

	int GetMSAASampleCount() const;
	int GetMSAAQuality() const;

	ComPtr<ID3DBlob> LoadCompiledShader(const std::string& filename) const;
	ComPtr<ID3D12RootSignature> SerializeAndCreateRootSigFromRootDesc(const CD3DX12_ROOT_SIGNATURE_DESC& rootSigDesc) const;

	void ExecuteCommandLists();
	void FlushCommandQueue();

	ID3D12Resource* GetCurrentBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView();
	
	void PresentAndSwapBuffers();

	/* Texture */
	Texture* CreateTextureFromFile(const char* name);
	void CreateGpuTexture(const unsigned int* raw, int width, int height, int bpp, Texture& outTex);
	Texture* CreateTextureFromData(const std::byte* data, int width, int height, int bpp, const char* name);
	void ResampleTexture(const unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight);
	void GetDrawTextureFullname(const char* name, char* dest, int destSize) const;
	void UpdateTexture(Texture& tex, const std::byte* data);

	/* Buffer */
	ComPtr<ID3D12Resource> CreateDefaultHeapBuffer(const void* data, UINT64 byteSize);
	ComPtr<ID3D12Resource> CreateUploadHeapBuffer(UINT64 byteSize) const;
	void UpdateUploadHeapBuff(FArg::UpdateUploadHeapBuff& args) const;

	/* Shutdown and clean up Win32 specific stuff */
	void ShutdownWin32();

	/* Factory functionality */
	void CreatePictureObject(const char* pictureName);
	void CreateGraphicalObjectFromGLSurface(const msurface_t& surf);
	void DecomposeGLModelNode(const model_t& model, const mnode_t& node);

	/* Rendering */
	void Draw(const GraphicalObject& object);
	void DrawIndiced(const GraphicalObject& object);
	void DrawStreaming(const std::byte* vertices, int verticesSizeInBytes, int verticesStride, const char* texName, const XMFLOAT4& pos);

	/* Utils */
	void GetDrawAreaSize(int* Width, int* Height);
	void Load8To24Table();
	void ImageBpp8To32(const std::byte* data, int width, int height, unsigned int* out) const;
	void FindImageScaledSizes(int width, int height, int& scaledWidth, int& scaledHeight) const;
	bool IsVisible(const GraphicalObject& obj) const;

	HWND		m_hWindows = nullptr;

	refimport_t m_refImport;

	ComPtr<ID3D12Device>   m_device;
	ComPtr<IDXGIFactory4>  m_dxgiFactory;

	ComPtr<IDXGISwapChain> m_swapChain;
	ComPtr<ID3D12Fence>	   m_fence;
	ComPtr<ID3D12Resource> m_swapChainBuffer[QSWAP_CHAIN_BUFFER_COUNT];
	ComPtr<ID3D12Resource> m_depthStencilBuffer;

	ComPtr<ID3D12CommandQueue>		  m_commandQueue;
	ComPtr<ID3D12CommandAllocator>	  m_commandListAlloc;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	ComPtr<ID3D12DescriptorHeap>	  m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap>	  m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap>	  m_cbvSrvHeap;
	ComPtr<ID3D12DescriptorHeap>	  m_samplerHeap;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	ComPtr<ID3D12PipelineState>		  m_pipelineState;
	ComPtr<ID3D12RootSignature>		  m_rootSingature;

	ComPtr<ID3DBlob> m_psShader;
	ComPtr<ID3DBlob> m_vsShader;

	AllocBuffer m_constantBuffer;
	AllocBuffer m_streamingVertexBuffer;

	D3D12_VIEWPORT m_viewport;
	tagRECT		   m_scissorRect;

	INT	m_currentBackBuffer = 0;

	/* Render target descriptor size */
	UINT								   m_rtvDescriptorSize = 0;
	/* Depth/Stencil descriptor size */
	UINT								   m_dsvDescriptorSize = 0;
	/* Constant buffer / shader resource descriptor size */
	UINT								   m_cbvSrbDescriptorSize = 0;
	/* Sampler descriptor size */
	UINT								   m_samplerDescriptorSize = 0;

	UINT m_MSQualityLevels = 0;

	UINT64 m_currentFenceValue = 0;

	std::unordered_map<std::string, Texture> m_textures;
	// When we upload something on GPU we need to make sure that its ComPtr is alive until 
	// we finish execution of command list that references this. So I will just put this stuff here
	// and clear this vector at the end of every frame.
	std::vector<ComPtr<ID3D12Resource>> m_uploadResources;
	// If we want to delete resource we can't do this right away cause there is a high chance that this 
	// resource will still be in use, so we just put it here and delete it later 
	std::vector<ComPtr<ID3D12Resource>> m_resourcesToDelete;

	std::array<unsigned int, 256> m_8To24Table;
	std::array<unsigned int, 256> m_rawPalette;

	// Bookkeeping for which descriptors are taken and which aren't. This is very simple,
	// true means slot is taken.
	std::array<bool, QCBV_SRV_DESCRIPTORS_NUM> m_cbvSrvRegistry;

	// Should I separate UI from game object? Damn, is this NWN speaks in me
	std::vector<GraphicalObject> m_graphicalObjects;

	std::vector<int> m_streamingConstOffsets;
	
	XMFLOAT4X4 m_uiProjectionMat;
	XMFLOAT4X4 m_uiViewMat;
	// DirectX and OpenGL have different directions for Y axis,
	// this matrix is required to fix this. Also apparently original Quake 2
	// had origin in a middle of a screen, while we have it in upper left corner,
	// so we need to center content to the screen center
	XMFLOAT4X4 m_yInverseAndCenterMatrix;

	Camera m_camera;
};