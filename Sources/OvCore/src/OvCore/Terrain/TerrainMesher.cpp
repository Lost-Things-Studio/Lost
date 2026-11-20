/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <limits>

#include <OvCore/Terrain/Heightmap.h>
#include <OvCore/Terrain/TerrainMesher.h>

namespace
{
	using OvCore::Terrain::IsHole;
	using OvCore::Terrain::TerrainLayout;

	float HeightAt(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_x, uint32_t p_z)
	{
		return p_heights[static_cast<size_t>(p_z) * p_layout.sampleCount + p_x];
	}

	// The samples a level keeps between two samples of a side: every step, and always the last one
	std::vector<uint32_t> KeptSamples(uint32_t p_first, uint32_t p_last, uint32_t p_step)
	{
		std::vector<uint32_t> samples;
		for (uint32_t s = p_first; s < p_last; s += p_step)
		{
			samples.push_back(s);
		}
		samples.push_back(p_last);
		return samples;
	}

	// The largest gap between the full detail of a line of samples and the segments a level keeps of it
	float LineError(const std::vector<float>& p_heights, const TerrainLayout& p_layout, bool p_alongX, uint32_t p_fixed, uint32_t p_first, uint32_t p_last, uint32_t p_step)
	{
		const auto at = [&](uint32_t p_s) { return p_alongX ? HeightAt(p_heights, p_layout, p_s, p_fixed) : HeightAt(p_heights, p_layout, p_fixed, p_s); };
		const auto kept = KeptSamples(p_first, p_last, p_step);

		float error = 0.0f;
		for (size_t k = 0; k + 1 < kept.size(); ++k)
		{
			const uint32_t a = kept[k];
			const uint32_t b = kept[k + 1];
			if (IsHole(at(a)) || IsHole(at(b)))
			{
				continue;
			}

			for (uint32_t s = a + 1; s < b; ++s)
			{
				if (IsHole(at(s)))
				{
					continue;
				}

				const float t = static_cast<float>(s - a) / static_cast<float>(b - a);
				const float line = at(a) + t * (at(b) - at(a));
				error = std::max(error, std::abs(at(s) - line));
			}
		}
		return error;
	}

	// The full-detail normal at a sample, from its neighbours (one-sided at the field's edges and beside a hole)
	OvMaths::FVector3 NormalAt(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_x, uint32_t p_z)
	{
		if (IsHole(HeightAt(p_heights, p_layout, p_x, p_z)))
		{
			return { 0.0f, 1.0f, 0.0f };
		}

		const uint32_t last = p_layout.sampleCount - 1;
		const auto solid = [&](uint32_t p_sx, uint32_t p_sz) { return !IsHole(HeightAt(p_heights, p_layout, p_sx, p_sz)); };
		const uint32_t xa = p_x > 0 && solid(p_x - 1, p_z) ? p_x - 1 : p_x;
		const uint32_t xb = p_x < last && solid(p_x + 1, p_z) ? p_x + 1 : p_x;
		const uint32_t za = p_z > 0 && solid(p_x, p_z - 1) ? p_z - 1 : p_z;
		const uint32_t zb = p_z < last && solid(p_x, p_z + 1) ? p_z + 1 : p_z;

		const auto slope = [&](float p_rise, uint32_t p_samples) { return p_samples > 0 ? p_rise / (static_cast<float>(p_samples) * p_layout.spacing) : 0.0f; };
		const float slopeX = slope(HeightAt(p_heights, p_layout, xb, p_z) - HeightAt(p_heights, p_layout, xa, p_z), xb - xa);
		const float slopeZ = slope(HeightAt(p_heights, p_layout, p_x, zb) - HeightAt(p_heights, p_layout, p_x, za), zb - za);

		return OvMaths::FVector3::Normalize({ -slopeX, 1.0f, -slopeZ });
	}

	// A hole's vertex, which no triangle uses, stands at p_holeHeight so that the mesh's bounds hold
	OvRendering::Geometry::Vertex MakeVertex(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_x, uint32_t p_z, float p_drop, float p_holeHeight)
	{
		const auto normal = NormalAt(p_heights, p_layout, p_x, p_z);
		const float count = static_cast<float>(p_layout.sampleCount);
		const float height = HeightAt(p_heights, p_layout, p_x, p_z);

		OvRendering::Geometry::Vertex vertex{};
		vertex.position[0] = static_cast<float>(p_x) * p_layout.spacing;
		vertex.position[1] = (IsHole(height) ? p_holeHeight : height) - p_drop;
		vertex.position[2] = static_cast<float>(p_z) * p_layout.spacing;
		vertex.texCoords[0] = (static_cast<float>(p_x) + 0.5f) / count;
		vertex.texCoords[1] = 1.0f - (static_cast<float>(p_z) + 0.5f) / count;
		vertex.normals[0] = normal.x;
		vertex.normals[1] = normal.y;
		vertex.normals[2] = normal.z;
		vertex.tangent[0] = 1.0f;
		vertex.bitangent[2] = 1.0f;
		return vertex;
	}
}

OvCore::Terrain::TerrainLayout OvCore::Terrain::TerrainLayout::Make(uint32_t p_sampleCount, float p_spacing, uint32_t p_chunkCells, uint32_t p_lodCount)
{
	TerrainLayout layout;
	layout.sampleCount = p_sampleCount;
	layout.spacing = p_spacing > 0.0f ? p_spacing : 1.0f;
	layout.chunkCells = std::clamp(p_chunkCells, 1u, 256u);
	layout.chunksPerSide = p_sampleCount >= 2 ? (p_sampleCount - 2) / layout.chunkCells + 1 : 0;

	// A level halves the samples of a side: a chunk of 4 cells allows 3 levels, of 1 cell only one
	uint32_t allowed = 1;
	while (allowed < kMaxLodCount && (1u << allowed) <= layout.chunkCells)
	{
		++allowed;
	}
	layout.lodCount = std::clamp(p_lodCount, 1u, allowed);
	return layout;
}

uint32_t OvCore::Terrain::TerrainLayout::GetChunkCount() const
{
	return chunksPerSide * chunksPerSide;
}

void OvCore::Terrain::TerrainLayout::GetChunkSamples(uint32_t p_chunk, uint32_t& p_x0, uint32_t& p_x1, uint32_t& p_z0, uint32_t& p_z1) const
{
	const uint32_t chunkX = p_chunk % chunksPerSide;
	const uint32_t chunkZ = p_chunk / chunksPerSide;
	const uint32_t last = sampleCount - 1;

	p_x0 = chunkX * chunkCells;
	p_z0 = chunkZ * chunkCells;
	p_x1 = std::min(p_x0 + chunkCells, last);
	p_z1 = std::min(p_z0 + chunkCells, last);
}

float OvCore::Terrain::ComputeSkirtDepth(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_chunk)
{
	uint32_t x0, x1, z0, z1;
	p_layout.GetChunkSamples(p_chunk, x0, x1, z0, z1);

	const uint32_t step = 1u << (p_layout.lodCount - 1);
	const float error = std::max({
		LineError(p_heights, p_layout, true, z0, x0, x1, step),
		LineError(p_heights, p_layout, true, z1, x0, x1, step),
		LineError(p_heights, p_layout, false, x0, z0, z1, step),
		LineError(p_heights, p_layout, false, x1, z0, z1, step)
	});

	return 2.0f * error + 0.1f * p_layout.spacing;
}

OvCore::Terrain::TerrainBox OvCore::Terrain::ComputeChunkBox(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_chunk, float p_skirtDepth)
{
	uint32_t x0, x1, z0, z1;
	p_layout.GetChunkSamples(p_chunk, x0, x1, z0, z1);

	float low = std::numeric_limits<float>::max();
	float high = std::numeric_limits<float>::lowest();

	for (uint32_t z = z0; z <= z1; ++z)
	{
		for (uint32_t x = x0; x <= x1; ++x)
		{
			const float height = HeightAt(p_heights, p_layout, x, z);
			if (!IsHole(height))
			{
				low = std::min(low, height);
				high = std::max(high, height);
			}
		}
	}

	// A chunk all holes draws nothing: a flat box at the actor's height
	if (low > high)
	{
		low = high = 0.0f;
	}

	return TerrainBox{
		{ static_cast<float>(x0) * p_layout.spacing, low - p_skirtDepth, static_cast<float>(z0) * p_layout.spacing },
		{ static_cast<float>(x1) * p_layout.spacing, high, static_cast<float>(z1) * p_layout.spacing }
	};
}

void OvCore::Terrain::BuildChunk(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_chunk, uint32_t p_lod, float p_skirtDepth, ChunkGeometry& p_geometry)
{
	p_geometry.vertices.clear();
	p_geometry.indices.clear();
	p_geometry.surfaceTriangles = 0;

	uint32_t x0, x1, z0, z1;
	p_layout.GetChunkSamples(p_chunk, x0, x1, z0, z1);

	const uint32_t step = 1u << std::min(p_lod, p_layout.lodCount - 1);
	const auto columns = KeptSamples(x0, x1, step);
	const auto rows = KeptSamples(z0, z1, step);
	const auto columnCount = static_cast<uint32_t>(columns.size());
	const auto rowCount = static_cast<uint32_t>(rows.size());

	p_geometry.vertices.reserve(static_cast<size_t>(columnCount) * rowCount + 4 * (columnCount + rowCount));

	// The chunk's holes, if any, and the lowest height that is not one
	bool hasHoles = false;
	float lowest = std::numeric_limits<float>::max();
	for (uint32_t z = z0; z <= z1; ++z)
	{
		for (uint32_t x = x0; x <= x1; ++x)
		{
			const float height = HeightAt(p_heights, p_layout, x, z);
			hasHoles = hasHoles || IsHole(height);
			lowest = IsHole(height) ? lowest : std::min(lowest, height);
		}
	}
	const float holeHeight = lowest < std::numeric_limits<float>::max() ? lowest : 0.0f;

	for (const uint32_t z : rows)
	{
		for (const uint32_t x : columns)
		{
			p_geometry.vertices.push_back(MakeVertex(p_heights, p_layout, x, z, 0.0f, holeHeight));
		}
	}

	// Whether a triangle of the level covers a hole of the full detail: one of the samples on or inside it. At full
	// detail, its three corners, which is what the physics leaves out
	const auto coversHole = [&](uint32_t p_xa, uint32_t p_xb, uint32_t p_za, uint32_t p_zb, bool p_belowDiagonal)
	{
		for (uint32_t z = p_za; z <= p_zb; ++z)
		{
			for (uint32_t x = p_xa; x <= p_xb; ++x)
			{
				// Below the (xa, za)-(xb, zb) diagonal: the side of (xb, za)
				const uint64_t across = static_cast<uint64_t>(x - p_xa) * (p_zb - p_za);
				const uint64_t along = static_cast<uint64_t>(z - p_za) * (p_xb - p_xa);
				const bool inside = p_belowDiagonal ? across >= along : across <= along;
				if (inside && IsHole(HeightAt(p_heights, p_layout, x, z)))
				{
					return true;
				}
			}
		}
		return false;
	};

	// Each cell split along its (x, z)-(x + 1, z + 1) diagonal, as the physics does, wound to face up
	const auto index = [&](uint32_t p_column, uint32_t p_row) { return p_row * columnCount + p_column; };
	for (uint32_t row = 0; row + 1 < rowCount; ++row)
	{
		for (uint32_t column = 0; column + 1 < columnCount; ++column)
		{
			const uint32_t i00 = index(column, row);
			const uint32_t i10 = index(column + 1, row);
			const uint32_t i01 = index(column, row + 1);
			const uint32_t i11 = index(column + 1, row + 1);

			const uint32_t xa = columns[column], xb = columns[column + 1];
			const uint32_t za = rows[row], zb = rows[row + 1];

			if (!hasHoles || !coversHole(xa, xb, za, zb, true))
			{
				p_geometry.indices.insert(p_geometry.indices.end(), { i00, i11, i10 });
			}
			if (!hasHoles || !coversHole(xa, xb, za, zb, false))
			{
				p_geometry.indices.insert(p_geometry.indices.end(), { i00, i01, i11 });
			}
		}
	}
	p_geometry.surfaceTriangles = static_cast<uint32_t>(p_geometry.indices.size() / 3);

	// Whether the full-detail samples of an edge between two of the surface's vertices, both included, hold a hole
	const auto spanHasHole = [&](uint32_t p_a, uint32_t p_b)
	{
		if (!hasHoles)
		{
			return false;
		}

		const uint32_t xa = columns[p_a % columnCount], xb = columns[p_b % columnCount];
		const uint32_t za = rows[p_a / columnCount], zb = rows[p_b / columnCount];

		for (uint32_t z = std::min(za, zb); z <= std::max(za, zb); ++z)
		{
			for (uint32_t x = std::min(xa, xb); x <= std::max(xa, xb); ++x)
			{
				if (IsHole(HeightAt(p_heights, p_layout, x, z)))
				{
					return true;
				}
			}
		}
		return false;
	};

	// A skirt under each edge: the edge's vertices copied lower, joined by quads seen from both sides, none along a
	// span with a hole, whose triangles are not drawn
	const auto skirt = [&](const std::vector<uint32_t>& p_edge)
	{
		const auto firstLow = static_cast<uint32_t>(p_geometry.vertices.size());
		for (const uint32_t top : p_edge)
		{
			auto low = p_geometry.vertices[top];
			low.position[1] -= p_skirtDepth;
			p_geometry.vertices.push_back(low);
		}

		for (size_t k = 0; k + 1 < p_edge.size(); ++k)
		{
			const uint32_t a = p_edge[k];
			const uint32_t b = p_edge[k + 1];
			if (spanHasHole(a, b))
			{
				continue;
			}

			const auto la = firstLow + static_cast<uint32_t>(k);
			const uint32_t lb = la + 1;
			p_geometry.indices.insert(p_geometry.indices.end(), { a, la, b, b, la, lb, a, b, la, b, lb, la });
		}
	};

	std::vector<uint32_t> edge;
	for (uint32_t column = 0; column < columnCount; ++column) edge.push_back(index(column, 0));
	skirt(edge);
	edge.clear();
	for (uint32_t column = 0; column < columnCount; ++column) edge.push_back(index(column, rowCount - 1));
	skirt(edge);
	edge.clear();
	for (uint32_t row = 0; row < rowCount; ++row) edge.push_back(index(0, row));
	skirt(edge);
	edge.clear();
	for (uint32_t row = 0; row < rowCount; ++row) edge.push_back(index(columnCount - 1, row));
	skirt(edge);
}

uint32_t OvCore::Terrain::SelectLod(float p_distance, float p_firstDistance, uint32_t p_lodCount, uint32_t p_previous, float p_margin)
{
	if (p_lodCount <= 1 || p_firstDistance <= 0.0f)
	{
		return 0;
	}

	// Level n ends at p_firstDistance * 2^n
	const auto boundary = [&](uint32_t p_level) { return p_firstDistance * static_cast<float>(1u << p_level); };

	uint32_t level = std::min(p_previous, p_lodCount - 1);
	while (level + 1 < p_lodCount && p_distance > boundary(level) * (1.0f + p_margin))
	{
		++level;
	}
	while (level > 0 && p_distance < boundary(level - 1) * (1.0f - p_margin))
	{
		--level;
	}
	return level;
}

float OvCore::Terrain::DistanceToBox(const OvMaths::FVector3& p_point, const OvMaths::FVector3& p_min, const OvMaths::FVector3& p_max)
{
	const float dx = std::max({ p_min.x - p_point.x, 0.0f, p_point.x - p_max.x });
	const float dy = std::max({ p_min.y - p_point.y, 0.0f, p_point.y - p_max.y });
	const float dz = std::max({ p_min.z - p_point.z, 0.0f, p_point.z - p_max.z });
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}
