#include "dx_objects.h"

#include <limits>

#include "dx_app.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define PREVENT_SELF_MOVE_CONSTRUCT if (this == &other) { return; }
#define PREVENT_SELF_MOVE_ASSIGN if (this == &other) { return *this; }

StaticObject::StaticObject(StaticObject&& other)
{
	PREVENT_SELF_MOVE_CONSTRUCT;

	textureKey = std::move(other.textureKey);
	vertexBuffer = other.vertexBuffer;
	indexBuffer = other.indexBuffer;

	position = other.position;

	constantBufferOffset = other.constantBufferOffset;

	bbMin = std::move(other.bbMin);
	bbMax = std::move(other.bbMax);
	
	other.constantBufferOffset = BufConst::INVALID_OFFSET;
}


void StaticObject::GenerateBoundingBox(const std::vector<XMFLOAT4>& vertices)
{
	constexpr float minFloat = std::numeric_limits<float>::min();
	constexpr float maxFloat = std::numeric_limits<float>::max();

	bbMax = XMFLOAT4( minFloat, minFloat, minFloat, 1.0f );
	bbMin = XMFLOAT4( maxFloat, maxFloat, maxFloat, 1.0f );

	for (const XMFLOAT4& vertex : vertices)
	{
		bbMax.x = std::max(bbMax.x, vertex.x);
		bbMax.y = std::max(bbMax.y, vertex.y);
		bbMax.z = std::max(bbMax.z, vertex.z);

		bbMin.x = std::min(bbMin.x, vertex.x);
		bbMin.y = std::min(bbMin.y, vertex.y);
		bbMin.z = std::min(bbMin.z, vertex.z);
	}
}

XMMATRIX StaticObject::GenerateModelMat() const
{
	XMMATRIX modelMat = XMMatrixScaling(scale.x, scale.y, scale.z);
	modelMat = modelMat * XMMatrixTranslation(
		position.x,
		position.y,
		position.z
	);

	return modelMat;
}

StaticObject& StaticObject::StaticObject::operator=(StaticObject&& other)
{
	PREVENT_SELF_MOVE_ASSIGN;

	textureKey = std::move(other.textureKey);
	vertexBuffer = other.vertexBuffer;
	indexBuffer = other.indexBuffer;
	position = other.position;
	constantBufferOffset = other.constantBufferOffset;
	bbMin = std::move(other.bbMin);
	bbMax = std::move(other.bbMax);

	other.constantBufferOffset = BufConst::INVALID_OFFSET;

	return *this;
}

StaticObject::~StaticObject()
{
	if (constantBufferOffset != BufConst::INVALID_OFFSET)
	{
		Renderer::Inst().DeleteConstantBuffMemory(constantBufferOffset);
	}
}

DynamicObjectModel::DynamicObjectModel(DynamicObjectModel&& other)
{
	PREVENT_SELF_MOVE_CONSTRUCT;

	textures = std::move(other.textures);
	
	headerData = other.headerData;
	other.headerData.animFrameSizeInBytes = -1;
	other.headerData.animFrameVertsNum = -1;
	other.headerData.indicesNum = -1;

	textureCoords = other.textureCoords;
	other.textureCoords = BufConst::INVALID_BUFFER_HANDLER;

	vertices = other.vertices;
	other.vertices = BufConst::INVALID_BUFFER_HANDLER;
	
	indices = other.indices;
	other.indices = BufConst::INVALID_BUFFER_HANDLER;

	animationFrames = std::move(other.animationFrames);
}

XMMATRIX DynamicObjectModel::GenerateModelMat(const entity_t& entity)
{
	const XMFLOAT4 axisX = XMFLOAT4(1.0, 0.0, 0.0, 0.0);
	const XMFLOAT4 axisY = XMFLOAT4(0.0, 1.0, 0.0, 0.0);
	const XMFLOAT4 axisZ = XMFLOAT4(0.0, 0.0, 1.0, 0.0);

	// -entity.angles[0] is intentional. Done to avoid some Quake shenanigans
	//#DEBUG try without this minus
	const XMFLOAT4 angles = XMFLOAT4(-entity.angles[0], entity.angles[1], entity.angles[2], 0.0f);

	// Quake 2 implementation of R_RotateForEntity 
	XMMATRIX sseModelMat =
		XMMatrixRotationAxis(XMLoadFloat4(&axisX), XMConvertToRadians(-angles.z)) *
		XMMatrixRotationAxis(XMLoadFloat4(&axisY), XMConvertToRadians(-angles.x)) *
		XMMatrixRotationAxis(XMLoadFloat4(&axisZ), XMConvertToRadians(angles.y)) *

		XMMatrixTranslation(entity.origin[0], entity.origin[1], entity.origin[2]);

	return sseModelMat;
}

// move, front lerp, back lerp
std::tuple<XMFLOAT4, XMFLOAT4, XMFLOAT4> DynamicObjectModel::GenerateAnimInterpolationData(const entity_t& entity) const
{
	const DynamicObjectModel::AnimFrame& oldFrame = animationFrames[entity.oldframe];
	const DynamicObjectModel::AnimFrame& frame = animationFrames[entity.frame];

	XMVECTOR sseOldOrigin = XMLoadFloat4(&XMFLOAT4(entity.oldorigin[0], entity.oldorigin[1], entity.oldorigin[2], 1.0f));
	XMVECTOR sseOrigin = XMLoadFloat4(&XMFLOAT4(entity.origin[0], entity.origin[1], entity.origin[2], 1.0f));

	XMVECTOR sseDelta = XMVectorSubtract(sseOldOrigin, sseOrigin);

	// Generate anim transformation mat
	//#DEBUG this stuff is used in a few places, any way we can generalize this?
	const XMFLOAT4 axisX = XMFLOAT4(1.0, 0.0, 0.0, 0.0);
	const XMFLOAT4 axisY = XMFLOAT4(0.0, 1.0, 0.0, 0.0);
	const XMFLOAT4 axisZ = XMFLOAT4(0.0, 0.0, 1.0, 0.0);

	//#DEBUG switch
	const XMFLOAT4 angles = XMFLOAT4(entity.angles[0], entity.angles[1], entity.angles[2], 0.0f);

	XMMATRIX sseRotationMat =
		XMMatrixRotationAxis(XMLoadFloat4(&axisZ), XMConvertToRadians(-angles.y)) *
		XMMatrixRotationAxis(XMLoadFloat4(&axisY), XMConvertToRadians(-angles.x)) *
		XMMatrixRotationAxis(XMLoadFloat4(&axisX), XMConvertToRadians(-angles.z));

	// All we do here is transforming delta from world coordinates to model local coordinates
	XMVECTOR sseMove = XMVectorAdd(
		XMVector4Transform(sseDelta, sseRotationMat),
		XMLoadFloat4(&oldFrame.translate)
	);

	const float backLerp = entity.backlerp;

	XMFLOAT4 frontLerpVec = XMFLOAT4(1.0f - backLerp, 1.0f - backLerp, 1.0f - backLerp, 0.0f);
	XMFLOAT4 backLerpVec = XMFLOAT4(backLerp, backLerp, backLerp, 0.0f);

	//#DEBUG make it better, like use front/back lerp proper naming
	sseMove = XMVectorMultiplyAdd(
		XMLoadFloat4(&backLerpVec),
		sseMove,
		XMVectorMultiply(XMLoadFloat4(&frontLerpVec), XMLoadFloat4(&frame.translate))
	);
	//END

	XMFLOAT4 move;
	XMStoreFloat4(&move, sseMove);


	XMVECTOR sseFrontLerp = XMVectorMultiply(XMLoadFloat4(&frontLerpVec), XMLoadFloat4(&frame.scale));
	XMVECTOR sseBackLerp = XMVectorMultiply(XMLoadFloat4(&backLerpVec), XMLoadFloat4(&oldFrame.scale));

	XMStoreFloat4(&frontLerpVec, sseFrontLerp);
	XMStoreFloat4(&backLerpVec, sseBackLerp);

	return std::make_tuple(move, frontLerpVec, backLerpVec);
}

DynamicObjectModel& DynamicObjectModel::operator=(DynamicObjectModel&& other)
{
	PREVENT_SELF_MOVE_ASSIGN;

	textures = std::move(other.textures);

	headerData = other.headerData;
	other.headerData.animFrameSizeInBytes = -1;
	other.headerData.animFrameVertsNum = -1;
	other.headerData.indicesNum = -1;

	textureCoords = other.textureCoords;
	other.textureCoords = BufConst::INVALID_BUFFER_HANDLER;

	vertices = other.vertices;
	other.vertices = BufConst::INVALID_BUFFER_HANDLER;

	indices = other.indices;
	other.indices = BufConst::INVALID_BUFFER_HANDLER;

	animationFrames = std::move(other.animationFrames);

	return *this;
}

DynamicObjectModel::~DynamicObjectModel()
{
	if (indices != BufConst::INVALID_BUFFER_HANDLER)
	{
		Renderer::Inst().DeleteDefaultMemoryBufferViaHandler(indices);
	}

	if (vertices != BufConst::INVALID_BUFFER_HANDLER)
	{
		Renderer::Inst().DeleteDefaultMemoryBufferViaHandler(vertices);
	}

	if (textureCoords != BufConst::INVALID_BUFFER_HANDLER)
	{
		Renderer::Inst().DeleteDefaultMemoryBufferViaHandler(textureCoords);
	}
}

DynamicObjectConstBuffer::DynamicObjectConstBuffer(DynamicObjectConstBuffer&& other)
{
	//#DEBUG if this works change it everywhere 
	// this also eliminates PREVENT_SELF_MOVE_CONSTRUCT
	*this = std::move(other);
}

DynamicObjectConstBuffer& DynamicObjectConstBuffer::operator=(DynamicObjectConstBuffer&& other)
{
	PREVENT_SELF_MOVE_ASSIGN

	constantBufferOffset = other.constantBufferOffset;
	other.constantBufferOffset = BufConst::INVALID_OFFSET;

	isInUse = other.isInUse;
	other.isInUse = false;

	return *this;
}

DynamicObjectConstBuffer::~DynamicObjectConstBuffer()
{
	if (constantBufferOffset != BufConst::INVALID_OFFSET)
	{
		Renderer::Inst().DeleteConstantBuffMemory(constantBufferOffset);
	}
}

DynamicObject::DynamicObject(DynamicObject&& other)
{
	*this = std::move(other);
}

DynamicObject& DynamicObject::operator=(DynamicObject&& other)
{
	PREVENT_SELF_MOVE_ASSIGN

	model = other.model;
	other.model = nullptr;

	constBuffer = other.constBuffer;
	other.constBuffer = nullptr;

	return *this;
}

DynamicObject::~DynamicObject()
{
	if (constBuffer != nullptr)
	{
		constBuffer->isInUse = false;
	}
}
