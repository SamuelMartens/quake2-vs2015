#pragma once

#include <functional>
#include <vector>
#include <variant>

#include "dx_buffer.h"
#include "dx_drawcalls.h"
#include "dx_common.h"
#include "dx_passparameters.h"

//#TODO
// 1) Implement proper tex samplers handling. When I need more samplers
// 2) Implement root constants
// 3) Implement const buffers in descriptor table 
// 4) Implement custom render targets and resource creation
// 5) Implement include
// 6) Do proper logging for parsing and execution
// 7) Proper name generation for D3D objects

//#TODO get rid of forward decl?
struct JobContext;

//#INFO every frame should have pass collection. In this way I will not need to worry about multithreading handle inside stage itself
class Pass_UI
{
public:
	struct StageObj
	{
		std::vector<RootArg::Arg_t> rootArgs;
		const DrawCall_UI_t* originalDrawCall = nullptr;
	};

public:

	Pass_UI() = default;

	Pass_UI(const Pass_UI&) = default;
	Pass_UI& operator=(const Pass_UI&) = default;

	Pass_UI(Pass_UI&&) = default;
	Pass_UI& operator=(Pass_UI&&) = default;

	~Pass_UI() = default;

	void Execute(JobContext& context);
	void Init();
	void Finish();

	//#TODO shouldn't be public
	PassParameters passParameters;

private:

	void Start(JobContext& jobCtx);
	void UpdateDrawObjects(JobContext& jobCtx);

	void SetUpRenderState(JobContext& jobCtx);
	void Draw(JobContext& jobCtx);

private:

	unsigned int perObjectConstBuffMemorySize = 0;
	unsigned int perObjectVertexMemorySize = 0;
	unsigned int perVertexMemorySize = 0;

	BufferHandler constBuffMemory = Const::INVALID_BUFFER_HANDLER;
	BufferHandler vertexMemory = Const::INVALID_BUFFER_HANDLER;

	std::vector<StageObj> drawObjects;

	// DirectX and OpenGL have different directions for Y axis,
	// this matrix is required to fix this. Also I am using wrong projection matrix
	// and need to fix whenever I will have a chance
	//#TODO fix use right projection matrix
	XMFLOAT4X4 yInverseAndCenterMatrix;
};

using Pass_t = std::variant<Pass_UI>;