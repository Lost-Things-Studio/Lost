/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>

#include <OvPhysics/Entities/PhysicalBox.h>
#include <OvPhysics/Tools/Conversion.h>

OvPhysics::Entities::PhysicalBox::PhysicalBox(const OvMaths::FVector3& p_size) : PhysicalObject(), m_size(p_size)
{
	Init();
}

OvPhysics::Entities::PhysicalBox::PhysicalBox(OvMaths::FTransform & p_transform, const OvMaths::FVector3& p_size) : PhysicalObject(p_transform), m_size(p_size)
{
	Init();
}

void OvPhysics::Entities::PhysicalBox::SetSize(const OvMaths::FVector3& p_size)
{
	if (m_size != p_size)
	{
		m_size = p_size;
		RecreateBody();
	}
}

OvMaths::FVector3 OvPhysics::Entities::PhysicalBox::GetSize() const
{
	return m_size;
}

JPH::Shape* OvPhysics::Entities::PhysicalBox::CreateShape(const OvMaths::FVector3& p_scale) const
{
	const OvMaths::FVector3 halfExtent = m_size * p_scale;

	// A flat box keeps some thickness, as spheres and capsules keep some radius: with none it has no volume, so a
	// dynamic one would get no inertia
	return new JPH::BoxShape(OvPhysics::Tools::Conversion::ToJoltVector3({
		std::max(halfExtent.x, kMinimumShapeRadius),
		std::max(halfExtent.y, kMinimumShapeRadius),
		std::max(halfExtent.z, kMinimumShapeRadius)
	}));
}
