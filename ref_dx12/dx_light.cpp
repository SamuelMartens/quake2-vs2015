#include "dx_light.h"

#include <cassert>
#include <numeric>

#include "dx_app.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif


namespace
{
	float ColorNormalize(const XMFLOAT4& in, XMFLOAT4& out)
	{
		const float max = std::max(std::max(in.x, in.y), in.z);

		if (max != 0.0f)
		{
			XMStoreFloat4(&out, XMLoadFloat4(&in) / max);
		}

		return max;
	}
}

void SurfaceLight::Init(SurfaceLight& light)
{
	assert(light.surfaceIndex != Const::INVALID_INDEX && "Invalid object index for surface light init");

	const SourceStaticObject& object = Renderer::Inst().sourceStaticObjects[light.surfaceIndex];

	const int trianglesNum = object.indices.size() / 3;

	assert(trianglesNum > 0 && "Invalid triangles num in GenerateObjectTrianglesPDF");

	std::vector<float> triangleAreas(trianglesNum, 0.0f);

	for (int triangleInd = 0; triangleInd < object.indices.size() / 3; ++triangleInd)
	{
		XMVECTOR sseV0 = XMLoadFloat4(&object.vertices[triangleInd * 3 + 0]);
		XMVECTOR sseV1 = XMLoadFloat4(&object.vertices[triangleInd * 3 + 1]);
		XMVECTOR sseV2 = XMLoadFloat4(&object.vertices[triangleInd * 3 + 2]);

		triangleAreas[triangleInd] = XMVectorGetX(XMVector3Length(XMVector3Cross(sseV1 - sseV0, sseV2 - sseV0)) / 2);
	}

	light.area = std::accumulate(triangleAreas.cbegin(), triangleAreas.cend(), 0.0f);

	float currentSum = 0.0f;
	
	assert(light.trianglesPDF.empty() == true && "Light data should be empty during init");

	light.trianglesPDF.reserve(triangleAreas.size());

	std::transform(triangleAreas.cbegin(), triangleAreas.cend(), std::back_inserter(light.trianglesPDF),
		[&light, &currentSum](const float triangleArea)
	{
		currentSum += triangleArea;

		return currentSum / light.area;
	});

	assert(light.trianglesPDF.back() == 1.0f && "Something wrong with triangle PDF generation");

	light.irradiance = CalculateIrradiance(light);
}

// Taken from Q2-Pathtracing CalcReflectivityForPathtracing()
// basically averaging albedo. 
XMFLOAT4 SurfaceLight::CalculateReflectivity(const Resource& texture, const std::byte* textureData)
{
	//#DEBUG what is this? Shall I watch for this too?
	/* The reflectivity is only relevant for wall textures. */
	//if (image->type != it_wall)
		//return;

	assert(textureData != nullptr && "Invalid texture data");
	assert(texture.desc.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && "Unknown texture dimension");
	assert(texture.desc.format == DXGI_FORMAT_R8G8B8A8_UNORM && "Invalid texture format");

	XMFLOAT4 reflectivity = { 0.0f, 0.0f, 0.0f, 0.0f };

	const int numTexels = texture.desc.height * texture.desc.width;

	for (int texelInd = 0; texelInd < numTexels; ++texelInd)
	{
		reflectivity.x += std::to_integer<int>(textureData[texelInd * 4 + 0]);
		reflectivity.y += std::to_integer<int>(textureData[texelInd * 4 + 1]);
		reflectivity.z += std::to_integer<int>(textureData[texelInd * 4 + 2]);
	}

	// Find average and then normalize
	XMStoreFloat4(&reflectivity,
		XMLoadFloat4(&reflectivity) / numTexels / 255.0f);

	// scale the reflectivity up, because the textures are
	// so dim
	float scale = ColorNormalize(reflectivity, reflectivity);

	if (scale < 0.5f)
	{
		scale *= 2.0f;

		XMStoreFloat4(&reflectivity,
			XMLoadFloat4(&reflectivity) * scale);
	}

	return reflectivity;
}

XMFLOAT4 SurfaceLight::CalculateIrradiance(const SurfaceLight& light)
{
	//#DEBUG where should I use this function? Remember that some texture creation is
	// deferred
	assert(light.surfaceIndex != Const::INVALID_INDEX && "Invalid object index in light data");
	assert(light.area != 0.0f && "Invalid are in light data");

	const SourceStaticObject& object = Renderer::Inst().sourceStaticObjects[light.surfaceIndex];
	const Resource* lightTexture = ResourceManager::Inst().FindResource(object.textureKey);

	assert(lightTexture != nullptr && "Invalid texture name");

	XMFLOAT4 irradiance;

	// If reflectivity * radiance == radiant flux 
	// then
	// reflectivity * radiance / area == irradiance
	XMStoreFloat4(&irradiance, 
		XMLoadFloat4(&lightTexture->desc.reflectivity) * lightTexture->desc.radiance / light.area);

	return irradiance;
}

float SurfaceLight::GetUniformSamplePDF(const SurfaceLight& light)
{
	return 1 / light.area;
}
