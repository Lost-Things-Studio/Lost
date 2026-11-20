/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>

#include <OvDebug/Logger.h>
#include <OvPhysics/Entities/PhysicalHeightField.h>

namespace
{
	constexpr uint32_t kBlockSize = 2;
	constexpr float kMaximumHeightError = 0.005f;
	constexpr float kDefaultFriction = 0.8f;

	/**
	* The bits a sample needs for kMaximumHeightError. Jolt works it out block by block and reads past the heights
	* when their count is not a multiple of the block size (129 samples, say), so it is given a copy padded with the
	* last row and column, which changes no block's range of heights
	*/
	uint32_t BitsPerSampleFor(const std::vector<float>& p_heights, uint32_t p_sampleCount, const JPH::HeightFieldShapeSettings& p_settings)
	{
		if (p_sampleCount % kBlockSize == 0)
		{
			return p_settings.CalculateBitsPerSampleForError(kMaximumHeightError);
		}

		const uint32_t paddedCount = (p_sampleCount / kBlockSize + 1) * kBlockSize;
		std::vector<float> padded(static_cast<size_t>(paddedCount) * paddedCount);

		for (uint32_t y = 0; y < paddedCount; ++y)
		{
			for (uint32_t x = 0; x < paddedCount; ++x)
			{
				const uint32_t sourceX = std::min(x, p_sampleCount - 1);
				const uint32_t sourceY = std::min(y, p_sampleCount - 1);
				padded[static_cast<size_t>(y) * paddedCount + x] = p_heights[static_cast<size_t>(sourceY) * p_sampleCount + sourceX];
			}
		}

		JPH::HeightFieldShapeSettings paddedSettings(padded.data(), p_settings.mOffset, p_settings.mScale, paddedCount);
		paddedSettings.mBlockSize = p_settings.mBlockSize;
		return paddedSettings.CalculateBitsPerSampleForError(kMaximumHeightError);
	}
}

/**
* The last shape made, kept for as long as the heights and the scale stay the same, so that recreating the body
* (when it changes settings) doesn't rebuild the field
*/
struct OvPhysics::Entities::PhysicalHeightField::ShapeCache
{
	JPH::Ref<JPH::Shape> shape;
	OvMaths::FVector3 scale;
};

OvPhysics::Entities::PhysicalHeightField::PhysicalHeightField() :
	PhysicalObject(),
	m_shapeCache(std::make_unique<ShapeCache>())
{
	Init();
	SetFriction(kDefaultFriction);
}

OvPhysics::Entities::PhysicalHeightField::PhysicalHeightField(OvMaths::FTransform& p_transform) :
	PhysicalObject(p_transform),
	m_shapeCache(std::make_unique<ShapeCache>())
{
	Init();
	SetFriction(kDefaultFriction);
}

OvPhysics::Entities::PhysicalHeightField::~PhysicalHeightField() = default;

bool OvPhysics::Entities::PhysicalHeightField::SetHeights(uint32_t p_sampleCount, float p_spacing, std::vector<float> p_heights)
{
	if (!p_heights.empty() && p_heights.size() != static_cast<size_t>(p_sampleCount) * p_sampleCount)
	{
		return false;
	}

	// A hole: no triangle touching that sample collides
	for (float& height : p_heights)
	{
		if (std::isinf(height) && height > 0.0f)
		{
			height = JPH::HeightFieldShapeConstants::cNoCollisionValue;
		}
	}

	m_sampleCount = p_sampleCount;
	m_spacing = p_spacing;
	m_heights = std::move(p_heights);
	m_shapeCache->shape = nullptr;

	RecreateBody();
	return true;
}

uint32_t OvPhysics::Entities::PhysicalHeightField::GetSampleCount() const
{
	return m_sampleCount;
}

float OvPhysics::Entities::PhysicalHeightField::GetSpacing() const
{
	return m_spacing;
}

const std::vector<float>& OvPhysics::Entities::PhysicalHeightField::GetHeights() const
{
	return m_heights;
}

bool OvPhysics::Entities::PhysicalHeightField::MustBeStatic() const
{
	return true;
}

JPH::Shape* OvPhysics::Entities::PhysicalHeightField::CreateShape(const OvMaths::FVector3& p_scale) const
{
	if (m_heights.empty())
	{
		return new JPH::EmptyShape();
	}

	if (m_shapeCache->shape && m_shapeCache->scale.x == p_scale.x && m_shapeCache->scale.y == p_scale.y && m_shapeCache->scale.z == p_scale.z)
	{
		return m_shapeCache->shape.GetPtr();
	}

	JPH::HeightFieldShapeSettings settings(
		m_heights.data(),
		JPH::Vec3::sZero(),
		JPH::Vec3(m_spacing * p_scale.x, p_scale.y, m_spacing * p_scale.z),
		m_sampleCount
	);

	settings.mBlockSize = kBlockSize;
	settings.mBitsPerSample = BitsPerSampleFor(m_heights, m_sampleCount, settings);

	const JPH::ShapeSettings::ShapeResult result = settings.Create();

	if (result.HasError())
	{
		OVLOG_ERROR(std::string("Unable to create a height field: ") + result.GetError().c_str());
		return new JPH::EmptyShape();
	}

	m_shapeCache->shape = result.Get();
	m_shapeCache->scale = p_scale;

	return m_shapeCache->shape.GetPtr();
}
