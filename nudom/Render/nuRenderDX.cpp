#include "pch.h"
#include "nuRenderDX.h"
#include "../nuSysWnd.h"

#if NU_BUILD_DIRECTX

#define CHECK_HR(hresult, msg) if (FAILED(hresult)) { NUTRACE("%s failed: 0x%08x\n", msg, hresult); return false; }

nuRenderDX::nuRenderDX()
{
	memset( &D3D, 0, sizeof(D3D) );
	FBWidth = FBHeight = 0;
	AllProgs[0] = &PFill;
	AllProgs[1] = &PRect;
	static_assert(NumProgs == 2, "Add new shader here");
}

nuRenderDX::~nuRenderDX()
{
}

bool nuRenderDX::InitializeDevice( nuSysWnd& wnd )
{
	if ( !InitializeDXDevice(wnd) )
		return false;

	if ( !InitializeDXSurface(wnd) )
		return false;

	return true;
}

bool nuRenderDX::InitializeDXDevice( nuSysWnd& wnd )
{
	DXGI_SWAP_CHAIN_DESC swap;
	memset( &swap, 0, sizeof(swap) );
	swap.BufferDesc.Width = 0;
	swap.BufferDesc.Height = 0;
	swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	swap.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	swap.BufferDesc.Scaling = DXGI_MODE_SCALING_CENTERED;
	if ( nuGlobal()->EnableVSync )
	{
		// This has no effect on windowed rendering. Also, it is dumb to assume 60 hz.
		swap.BufferDesc.RefreshRate.Numerator = 1;
		swap.BufferDesc.RefreshRate.Denominator = 60;
	}
	swap.SampleDesc.Count = 1;
	swap.SampleDesc.Quality = 0;
	swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap.BufferCount = 1;
	swap.OutputWindow = wnd.SysWnd;
	swap.Windowed = true;
	swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swap.Flags = 0;

	HRESULT eCreate = D3D11CreateDeviceAndSwapChain( 
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D11_CREATE_DEVICE_SINGLETHREADED,
		NULL,
		0,
		D3D11_SDK_VERSION,
		&swap,
		&D3D.SwapChain,
		&D3D.Device,
		&D3D.FeatureLevel,
		&D3D.Context
		);

	CHECK_HR(eCreate, "D3D11CreateDeviceAndSwapChain")

	D3D11_RASTERIZER_DESC rast;
	memset( &rast, 0, sizeof(rast) );
	rast.FillMode = D3D11_FILL_SOLID;
	rast.CullMode = D3D11_CULL_NONE;
	rast.FrontCounterClockwise = TRUE; // The default DirectX value is FALSE, but we stick to the default OpenGL value which is CCW = Front
	rast.DepthBias = 0;
	rast.DepthBiasClamp = 0.0f;
	rast.SlopeScaledDepthBias = 0.0f;
	rast.DepthClipEnable = FALSE;
	rast.ScissorEnable = FALSE;
	rast.MultisampleEnable = FALSE;
	rast.AntialiasedLineEnable = FALSE;

	HRESULT eRast = D3D.Device->CreateRasterizerState( &rast, &D3D.Rasterizer );
	CHECK_HR(eRast, "CreateRasterizerState");

	D3D11_BLEND_DESC blend;
	memset( &blend, 0, sizeof(blend) );
	blend.AlphaToCoverageEnable = FALSE;
	blend.IndependentBlendEnable = FALSE;
	blend.RenderTarget[0].BlendEnable = true;
	blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	HRESULT eBlendNormal = D3D.Device->CreateBlendState( &blend, &D3D.BlendNormal );
	CHECK_HR(eBlendNormal, "CreateBlendNormal");

	return true;
}

bool nuRenderDX::InitializeDXSurface( nuSysWnd& wnd )
{
	HRESULT hr = S_OK;

	auto rect = wnd.GetRelativeClientRect();
	FBWidth = rect.Width();
	FBHeight = rect.Height();
	if ( !WindowResized() )
		return false;

	if ( !CreateShaders() )
		return false;

	if ( !SetupBuffers() )
		return false;

	return true;
}

bool nuRenderDX::WindowResized()
{
	HRESULT hr = S_OK;

	if ( D3D.RenderTargetView != NULL )
	{
		D3D.Context->OMSetRenderTargets( 0, NULL, NULL );
		D3D.RenderTargetView->Release();
		D3D.RenderTargetView = NULL;
		hr = D3D.SwapChain->ResizeBuffers( 0, FBWidth, FBHeight, DXGI_FORMAT_UNKNOWN, 0 );
		CHECK_HR(hr, "SwapChain.ResizeBuffers");
	}

	// Get a buffer and create a render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = D3D.SwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void**) &pBackBuffer );
	CHECK_HR(hr, "SwapChain.GetBuffer");

	hr = D3D.Device->CreateRenderTargetView( pBackBuffer, NULL, &D3D.RenderTargetView );
	pBackBuffer->Release();
	CHECK_HR(hr, "Device.CreateRenderTargetView");

	D3D.Context->OMSetRenderTargets( 1, &D3D.RenderTargetView, NULL );

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT) FBWidth;
	vp.Height = (FLOAT) FBHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	D3D.Context->RSSetViewports( 1, &vp );

	return true;
}

bool nuRenderDX::CreateShaders()
{
	for ( int i = 0; i < NumProgs; i++ )
	{
		if ( !CreateShader(AllProgs[i]) )
			return false;
	}
	return true;
}

bool nuRenderDX::CreateShader( nuDXProg* prog )
{
	std::string name = prog->Name();

	// Vertex shader
	ID3DBlob* vsBlob = NULL;
	if ( !CompileShader( (name + "Vertex").c_str(), prog->VertSrc(), "vs_4_0", &vsBlob ) )
		return false;

	HRESULT hr = D3D.Device->CreateVertexShader( vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &prog->Vert );
	if ( FAILED(hr) )
	{	
		NUTRACE( "CreateVertexShader failed (0x%08x) for %s", hr, (const char*) prog->Name() );
		vsBlob->Release();
		return false;
	}

	// Vertex input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, offsetof(nuVx_PTC,Pos),		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		//{ "TEXUV", 0, DXGI_FORMAT_R32G32_FLOAT,			0, offsetof(nuVx_PTC,UV),		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM,		0, offsetof(nuVx_PTC,Color),	D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = D3D.Device->CreateInputLayout( layout, arraysize(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &prog->VertLayout );
	vsBlob->Release();
	if ( FAILED(hr) )
	{
		NUTRACE( "Vertex layout for %s invalid (0x%08x)", (const char*) prog->Name(), hr );
		return false;
	}

	// Pixel shader
	ID3DBlob* psBlob = NULL;
	if ( !CompileShader( (name + "Frag").c_str(), prog->FragSrc(), "ps_4_0", &psBlob ) )
		return false;

	hr = D3D.Device->CreatePixelShader( psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &prog->Frag );
	psBlob->Release();
	if( FAILED(hr) )
	{
		NUTRACE( "CreatePixelShader failed (0x%08x) for %s", hr, (const char*) prog->Name() );
		return false;
	}

	return true;
}

bool nuRenderDX::CompileShader( const char* name, const char* source, const char* shaderTarget, ID3DBlob** blob )
{
	//D3D_SHADER_MACRO macros[1] = {
	//	{NULL,NULL}
	//};
	uint32 flags1 = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
	flags1 |= D3DCOMPILE_DEBUG;

	ID3DBlob* err = NULL;
	HRESULT hr = D3DCompile( source, strlen(source), name, NULL, NULL, "main", shaderTarget, flags1, 0, blob, &err );
	if ( FAILED(hr) )
	{
		if ( err != NULL )
		{
			NUTRACE( "Shader %s compilation failed (0x%08x): %s\n", name, hr, (const char*) err->GetBufferPointer() );
			err->Release();
		}
		else
			NUTRACE( "Shader %s compilation failed (0x%08x)\n", name, hr );
		return false;
	}

	return true;
}

bool nuRenderDX::SetupBuffers()
{
	if ( NULL == (D3D.VertBuffer = CreateBuffer( sizeof(nuVx_PTCV4) * 4, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, NULL )) )
		return false;

	uint16 quadIndices[4] = {0, 1, 3, 2};
	if ( NULL == (D3D.QuadIndexBuffer = CreateBuffer( sizeof(quadIndices), D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER, 0, quadIndices )) )
		return false;

	if ( NULL == (D3D.ShaderPerFrameConstants = CreateBuffer( sizeof(nuShaderPerFrame), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, NULL )) )
		return false;

	if ( NULL == (D3D.ShaderPerObjectConstants = CreateBuffer( sizeof(nuShaderPerObject), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, NULL )) )
		return false;

	return true;
}

void nuRenderDX::DestroyDevice( nuSysWnd& wnd )
{
	// free up all buffers etc
}

void nuRenderDX::SurfaceLost()
{
}

bool nuRenderDX::BeginRender( nuSysWnd& wnd )
{
	nuBox rect = wnd.GetRelativeClientRect();
	if ( rect.Width() != FBWidth || rect.Height() != FBHeight )
	{
		FBWidth = rect.Width();
		FBHeight = rect.Height();
		if ( !WindowResized() )
			return false;
	}

	return true;
}

void nuRenderDX::EndRender( nuSysWnd& wnd )
{
	HRESULT hr = D3D.SwapChain->Present( 0, 0 );
	if (!SUCCEEDED(hr))
		NUTRACE( "DirectX Present failed: 0x%08x", hr );

	D3D.ActiveProgram = NULL;

	// TODO: Handle
	//	DXGI_ERROR_DEVICE_REMOVED
	//	DXGI_ERROR_DRIVER_INTERNAL_ERROR
	// as described here: http://msdn.microsoft.com/en-us/library/windows/apps/dn458383.aspx "Handling device removed scenarios in Direct3D 11"
}

void nuRenderDX::PreRender()
{
	D3D.Context->RSSetState( D3D.Rasterizer );

	// Clear the back buffer 
	float clearColor[4] = {0.3f, 0.0f, 1, 0};
	D3D.Context->ClearRenderTargetView( D3D.RenderTargetView, clearColor );

	SetShaderFrameUniforms();
}

bool nuRenderDX::SetShaderFrameUniforms()
{
	nuMat4f mvproj;
	mvproj.Identity();
	Ortho( mvproj, 0, FBWidth, FBHeight, 0, 0, 1 );
	ShaderPerFrame.MVProj = mvproj.Transposed();
	ShaderPerFrame.VPort_HSize = Vec2f( FBWidth / 2.0f, FBHeight / 2.0f );

	D3D11_MAPPED_SUBRESOURCE sub;
	if ( FAILED(D3D.Context->Map( D3D.ShaderPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub )) )
		return false;
	memcpy( sub.pData, &ShaderPerFrame, sizeof(ShaderPerFrame) );
	D3D.Context->Unmap( D3D.ShaderPerFrameConstants, 0 );

	return true;
}

bool nuRenderDX::SetShaderObjectUniforms()
{
	D3D11_MAPPED_SUBRESOURCE sub;
	if ( FAILED(D3D.Context->Map( D3D.ShaderPerObjectConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub )) )
		return false;
	memcpy( sub.pData, &ShaderPerObject, sizeof(ShaderPerObject) );
	D3D.Context->Unmap( D3D.ShaderPerObjectConstants, 0 );
	D3D.Context->VSSetConstantBuffers( ConstantSlotPerObject, 1, &D3D.ShaderPerObjectConstants );
	D3D.Context->PSSetConstantBuffers( ConstantSlotPerObject, 1, &D3D.ShaderPerObjectConstants );
	return true;
}

ID3D11Buffer* nuRenderDX::CreateBuffer( size_t sizeBytes, D3D11_USAGE usage, D3D11_BIND_FLAG bind, uint cpuAccess, const void* initialContent )
{
	ID3D11Buffer* buffer = NULL;
	D3D11_BUFFER_DESC bd;
	memset( &bd, 0, sizeof(bd) );
	bd.ByteWidth = (UINT) sizeBytes;
	bd.Usage = usage;
	bd.BindFlags = bind;
	bd.CPUAccessFlags = cpuAccess;
	D3D11_SUBRESOURCE_DATA sub;
	sub.pSysMem = initialContent;
	sub.SysMemPitch = 0;
	sub.SysMemSlicePitch = 0;
	HRESULT hr = D3D.Device->CreateBuffer( &bd, initialContent != NULL ? &sub : NULL, &buffer );
	if ( !SUCCEEDED(hr) )
		NUTRACE( "CreateBuffer failed: %08x", hr );
	return buffer;
}

void nuRenderDX::PostRenderCleanup()
{
}

nuProgBase* nuRenderDX::GetShader( nuShaders shader )
{
	switch ( shader )
	{
	case nuShaderFill:		return &PFill;
	//case nuShaderFillTex:	return &PFillTex;
	case nuShaderRect:		return &PRect;
	//case nuShaderTextRGB:	return &PTextRGB;
	//case nuShaderTextWhole:	return &PTextWhole;
	default:
		NUASSERT(false);
		return NULL;
	}
}

void nuRenderDX::ActivateShader( nuShaders shader )
{
	nuDXProg* p = (nuDXProg*) GetShader( shader );
	if ( p == D3D.ActiveProgram )
		return;

	D3D.Context->VSSetShader( p->Vert, NULL, 0 );
	D3D.Context->PSSetShader( p->Frag, NULL, 0 );
	D3D.Context->IASetInputLayout( p->VertLayout );
	D3D.Context->VSSetConstantBuffers( ConstantSlotPerFrame, 1, &D3D.ShaderPerFrameConstants );
	D3D.Context->PSSetConstantBuffers( ConstantSlotPerFrame, 1, &D3D.ShaderPerFrameConstants );
	D3D.ActiveProgram = p;

	float blendFactors[4] = {0,0,0,0};
	uint sampleMask = 0xffffffff;
	D3D.Context->OMSetBlendState( D3D.BlendNormal, blendFactors, sampleMask );

}

void nuRenderDX::DrawQuad( const void* v )
{
	SetShaderObjectUniforms();

	const int nvert = 4;
	const byte* vbyte = (const byte*) v;

	// map vertex buffer with DISCARD
	D3D11_MAPPED_SUBRESOURCE map;
	memset( &map, 0, sizeof(map) );
	HRESULT hr = D3D.Context->Map( D3D.VertBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map );
	if ( FAILED(hr) )
	{
		NUTRACE_RENDER( "Vertex buffer map failed: %d", hr);
		return;
	}

	memcpy( map.pData, v, nvert * sizeof(nuVx_PTC) );
	D3D.Context->Unmap( D3D.VertBuffer, 0 );

	UINT stride = sizeof(nuVx_PTC);
	UINT offset = 0;
	D3D.Context->IASetVertexBuffers( 0, 1, &D3D.VertBuffer, &stride, &offset );
	D3D.Context->IASetIndexBuffer( D3D.QuadIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );
	//D3D.Context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	D3D.Context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
	D3D.Context->DrawIndexed( 4, 0, 0 );

	//D3D.Context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	//D3D.Context->Draw( 3, 0 );
}

void nuRenderDX::LoadTexture( nuTexture* tex, int texUnit )
{
}

void nuRenderDX::ReadBackbuffer( nuImage& image )
{
}


#endif
