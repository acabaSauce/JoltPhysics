// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include "UnitTestFramework.h"
#include "PhysicsTestContext.h"
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/PhysicsMaterialSimple.h>

TEST_SUITE("HeightFieldShapeTests")
{
	static void sRandomizeMaterials(HeightFieldShapeSettings &ioSettings, uint inMaxMaterials)
	{
		// Create materials
		for (uint i = 0; i < inMaxMaterials; ++i)
			ioSettings.mMaterials.push_back(new PhysicsMaterialSimple("Material " + ConvertToString(i), Color::sGetDistinctColor(i)));

		if (inMaxMaterials > 1)
		{
			// Make random material indices
			UnitTestRandom random;
			uniform_int_distribution<uint> index_distribution(0, inMaxMaterials - 1);
			ioSettings.mMaterialIndices.resize(Square(ioSettings.mSampleCount - 1));
			for (uint y = 0; y < ioSettings.mSampleCount - 1; ++y)
				for (uint x = 0; x < ioSettings.mSampleCount - 1; ++x)
					ioSettings.mMaterialIndices[y * (ioSettings.mSampleCount - 1) + x] = uint8(index_distribution(random));
		}
	}

	static Ref<HeightFieldShape> sValidateGetPosition(const HeightFieldShapeSettings &inSettings, float inMaxError)
	{
		// Create shape
		Ref<HeightFieldShape> shape = StaticCast<HeightFieldShape>(inSettings.Create().Get());

		// Validate it
		float max_diff = -1.0f;
		for (uint y = 0; y < inSettings.mSampleCount; ++y)
			for (uint x = 0; x < inSettings.mSampleCount; ++x)
			{
				// Perform a raycast from above the height field on this location
				RayCast ray { inSettings.mOffset + inSettings.mScale * Vec3((float)x, 100.0f, (float)y), inSettings.mScale.GetY() * Vec3(0, -200, 0) };
				RayCastResult hit;
				shape->CastRay(ray, SubShapeIDCreator(), hit);

				// Get original (unscaled) height
				float height = inSettings.mHeightSamples[y * inSettings.mSampleCount + x];
				if (height != HeightFieldShapeConstants::cNoCollisionValue)
				{
					// Check there is collision
					CHECK(!shape->IsNoCollision(x, y));

					// Calculate position
					Vec3 original_pos = inSettings.mOffset + inSettings.mScale * Vec3((float)x, height, (float)y);

					// Calculate position from the shape
					Vec3 shape_pos = shape->GetPosition(x, y);

					// Calculate delta
					float diff = (original_pos - shape_pos).Length();
					max_diff = max(max_diff, diff);

					// Materials are defined on the triangle, not on the sample points
					if (x < inSettings.mSampleCount - 1 && y < inSettings.mSampleCount - 1)
					{
						const PhysicsMaterial *m1 = PhysicsMaterial::sDefault;
						if (!inSettings.mMaterialIndices.empty())
							m1 = inSettings.mMaterials[inSettings.mMaterialIndices[y * (inSettings.mSampleCount - 1) + x]];
						else if (!inSettings.mMaterials.empty())
							m1 = inSettings.mMaterials.front();

						const PhysicsMaterial *m2 = shape->GetMaterial(x, y);
						CHECK(m1 == m2);
					}

					// Don't test borders, the ray may or may not hit
					if (x > 0 && y > 0 && x < inSettings.mSampleCount - 1 && y < inSettings.mSampleCount - 1)
					{
						// Check that the ray hit the height field
						Vec3 hit_pos = ray.GetPointOnRay(hit.mFraction);
						CHECK_APPROX_EQUAL(hit_pos, shape_pos, 1.0e-3f);
					}
				}
				else
				{
					// Should be no collision here
					CHECK(shape->IsNoCollision(x, y));

					// Ray should not have given a hit
					CHECK(hit.mFraction > 1.0f);
				}
			}

		// Check error
		CHECK(max_diff <= inMaxError);

		return shape;
	}

	TEST_CASE("TestPlane")
	{
		// Create flat plane with offset and scale
		HeightFieldShapeSettings settings;
		settings.mOffset = Vec3(3, 5, 7);
		settings.mScale = Vec3(9, 13, 17);
		settings.mSampleCount = 32;
		settings.mBitsPerSample = 1;
		settings.mBlockSize = 4;
		settings.mHeightSamples.resize(Square(settings.mSampleCount));
		for (float &h : settings.mHeightSamples)
			h = 1.0f;

		// Make some random holes
		UnitTestRandom random;
		uniform_int_distribution<uint> index_distribution(0, (uint)settings.mHeightSamples.size() - 1);
		for (int i = 0; i < 10; ++i)
			settings.mHeightSamples[index_distribution(random)] = HeightFieldShapeConstants::cNoCollisionValue;

		// We should be able to encode a flat plane in 1 bit
		CHECK(settings.CalculateBitsPerSampleForError(0.0f) == 1);

		sRandomizeMaterials(settings, 256);
		sValidateGetPosition(settings, 0.0f);
	}

	TEST_CASE("TestPlaneCloseToOrigin")
	{
		// Create flat plane very close to origin, this tests that we don't introduce a quantization error on a flat plane
		HeightFieldShapeSettings settings;
		settings.mSampleCount = 32;
		settings.mBitsPerSample = 1;
		settings.mBlockSize = 4;
		settings.mHeightSamples.resize(Square(settings.mSampleCount));
		for (float &h : settings.mHeightSamples)
			h = 1.0e-6f;

		// We should be able to encode a flat plane in 1 bit
		CHECK(settings.CalculateBitsPerSampleForError(0.0f) == 1);

		sRandomizeMaterials(settings, 50);
		sValidateGetPosition(settings, 0.0f);
	}

	TEST_CASE("TestRandomHeightField")
	{
		const float cMinHeight = -5.0f;
		const float cMaxHeight = 10.0f;

		UnitTestRandom random;
		uniform_real_distribution<float> height_distribution(cMinHeight, cMaxHeight);

		// Create height field with random samples
		HeightFieldShapeSettings settings;
		settings.mOffset = Vec3(0.3f, 0.5f, 0.7f);
		settings.mScale = Vec3(1.1f, 1.2f, 1.3f);
		settings.mSampleCount = 32;
		settings.mBitsPerSample = 8;
		settings.mBlockSize = 4;
		settings.mHeightSamples.resize(Square(settings.mSampleCount));
		for (float &h : settings.mHeightSamples)
			h = height_distribution(random);

		// Check if bits per sample is ok
		for (uint32 bits_per_sample = 1; bits_per_sample <= 8; ++bits_per_sample)
		{
			// Calculate maximum error you can get if you quantize using bits_per_sample.
			// We ignore the fact that we have range blocks that give much better compression, although
			// with random input data there shouldn't be much benefit of that.
			float max_error = 0.5f * (cMaxHeight - cMinHeight) / ((1 << bits_per_sample) - 1);
			uint32 calculated_bits_per_sample = settings.CalculateBitsPerSampleForError(max_error);
			CHECK(calculated_bits_per_sample <= bits_per_sample);
		}

		sRandomizeMaterials(settings, 1);
		sValidateGetPosition(settings, settings.mScale.GetY() * (cMaxHeight - cMinHeight) / ((1 << settings.mBitsPerSample) - 1));
	}

	TEST_CASE("TestEmptyHeightField")
	{
		// Create height field with no collision
		HeightFieldShapeSettings settings;
		settings.mSampleCount = 32;
		settings.mHeightSamples.resize(Square(settings.mSampleCount));
		for (float &h : settings.mHeightSamples)
			h = HeightFieldShapeConstants::cNoCollisionValue;

		// This should use the minimum amount of bits
		CHECK(settings.CalculateBitsPerSampleForError(0.0f) == 1);

		sRandomizeMaterials(settings, 50);
		Ref<HeightFieldShape> shape = sValidateGetPosition(settings, 0.0f);

		// Check that we allocated the minimum amount of memory
		Shape::Stats stats = shape->GetStats();
		CHECK(stats.mNumTriangles == 0);
		CHECK(stats.mSizeBytes == sizeof(HeightFieldShape));
	}

	TEST_CASE("TestGetHeights")
	{
		const float cMinHeight = -5.0f;
		const float cMaxHeight = 10.0f;
		const uint cSampleCount = 32;
		const uint cNoCollisionIndex = 10;

		UnitTestRandom random;
		uniform_real_distribution<float> height_distribution(cMinHeight, cMaxHeight);

		// Create height field with random samples
		HeightFieldShapeSettings settings;
		settings.mOffset = Vec3(0.3f, 0.5f, 0.7f);
		settings.mScale = Vec3(1.1f, 1.2f, 1.3f);
		settings.mSampleCount = cSampleCount;
		settings.mBitsPerSample = 8;
		settings.mBlockSize = 4;
		settings.mHeightSamples.resize(Square(cSampleCount));
		for (float &h : settings.mHeightSamples)
			h = height_distribution(random);

		// Add 1 sample that has no collision
		settings.mHeightSamples[cNoCollisionIndex] = HeightFieldShapeConstants::cNoCollisionValue;

		// Create shape
		ShapeRefC shape = settings.Create().Get();
		const HeightFieldShape *height_field = StaticCast<HeightFieldShape>(shape);

		{
			// Check that the GetHeights function returns the same values as the original height samples
			Array<float> sampled_heights;
			sampled_heights.resize(Square(cSampleCount));
			height_field->GetHeights(0, 0, cSampleCount, cSampleCount, sampled_heights.data(), cSampleCount);
			for (uint i = 0; i < Square(cSampleCount); ++i)
				if (i == cNoCollisionIndex)
					CHECK(sampled_heights[i] == HeightFieldShapeConstants::cNoCollisionValue);
				else
					CHECK_APPROX_EQUAL(sampled_heights[i], settings.mOffset.GetY() + settings.mScale.GetY() * settings.mHeightSamples[i], 0.05f);
		}

		{
			// With a random height field the max error is going to be limited by the amount of bits we have per sample as we will not get any benefit from a reduced range per block
			float tolerance = (cMaxHeight - cMinHeight) / ((1 << settings.mBitsPerSample) - 2);

			// Check a sub rect of the height field
			uint sx = 4, sy = 8, cx = 16, cy = 8;
			Array<float> sampled_heights;
			sampled_heights.resize(cx * cy);
			height_field->GetHeights(sx, sy, cx, cy, sampled_heights.data(), cx);
			for (uint y = 0; y < cy; ++y)
				for (uint x = 0; x < cx; ++x)
					CHECK_APPROX_EQUAL(sampled_heights[y * cx + x], settings.mOffset.GetY() + settings.mScale.GetY() * settings.mHeightSamples[(sy + y) * cSampleCount + sx + x], tolerance);
		}
	}

	TEST_CASE("TestSetHeights")
	{
		const float cMinHeight = -5.0f;
		const float cMaxHeight = 10.0f;
		const uint cSampleCount = 32;

		UnitTestRandom random;
		uniform_real_distribution<float> height_distribution(cMinHeight, cMaxHeight);

		// Create height field with random samples
		HeightFieldShapeSettings settings;
		settings.mOffset = Vec3(0.3f, 0.5f, 0.7f);
		settings.mScale = Vec3(1.1f, 1.2f, 1.3f);
		settings.mSampleCount = cSampleCount;
		settings.mBitsPerSample = 8;
		settings.mBlockSize = 4;
		settings.mHeightSamples.resize(Square(cSampleCount));
		settings.mMinHeightValue = cMinHeight;
		settings.mMaxHeightValue = cMaxHeight;
		for (float &h : settings.mHeightSamples)
			h = height_distribution(random);

		// Create shape
		Ref<Shape> shape = settings.Create().Get();
		HeightFieldShape *height_field = StaticCast<HeightFieldShape>(shape);

		// Get the original (quantized) heights
		Array<float> original_heights;
		original_heights.resize(Square(cSampleCount));
		height_field->GetHeights(0, 0, cSampleCount, cSampleCount, original_heights.data(), cSampleCount);

		// Create new data for height field
		Array<float> patched_heights;
		uint sx = 4, sy = 16, cx = 16, cy = 8;
		patched_heights.resize(cx * cy);
		for (uint y = 0; y < cy; ++y)
			for (uint x = 0; x < cx; ++x)
				patched_heights[y * cx + x] = height_distribution(random);

		// Add 1 sample that has no collision
		uint no_collision_idx = (sy + 1) * cSampleCount + sx + 2;
		patched_heights[1 * cx + 2] = HeightFieldShapeConstants::cNoCollisionValue;

		// Update the height field
		TempAllocatorMalloc temp_allocator;
		height_field->SetHeights(sx, sy, cx, cy, patched_heights.data(), cx, temp_allocator);

		// With a random height field the max error is going to be limited by the amount of bits we have per sample as we will not get any benefit from a reduced range per block
		float tolerance = (cMaxHeight - cMinHeight) / ((1 << settings.mBitsPerSample) - 2);

		// Check a sub rect of the height field
		Array<float> verify_heights;
		verify_heights.resize(cSampleCount * cSampleCount);
		height_field->GetHeights(0, 0, cSampleCount, cSampleCount, verify_heights.data(), cSampleCount);
		for (uint y = 0; y < cSampleCount; ++y)
			for (uint x = 0; x < cSampleCount; ++x)
			{
				uint idx = y * cSampleCount + x;
				if (idx == no_collision_idx)
					CHECK(verify_heights[idx] == HeightFieldShapeConstants::cNoCollisionValue);
				else if (x >= sx && x < sx + cx && y >= sy && y < sy + cy)
					CHECK_APPROX_EQUAL(verify_heights[y * cSampleCount + x], patched_heights[(y - sy) * cx + x - sx], tolerance);
				else if (x >= sx - settings.mBlockSize && x < sx + cx && y >= sy - settings.mBlockSize && y < sy + cy)
					CHECK_APPROX_EQUAL(verify_heights[idx], original_heights[idx], tolerance); // We didn't modify this but it has been quantized again
				else
					CHECK(verify_heights[idx] == original_heights[idx]); // We didn't modify this and it is outside of the affected range
			}
	}

	TEST_CASE("TestSetMaterials")
	{
		constexpr uint cSampleCount = 32;

		PhysicsMaterialRefC material_0 = new PhysicsMaterialSimple("Material 0", Color::sGetDistinctColor(0));
		PhysicsMaterialRefC material_1 = new PhysicsMaterialSimple("Material 1", Color::sGetDistinctColor(1));
		PhysicsMaterialRefC material_2 = new PhysicsMaterialSimple("Material 2", Color::sGetDistinctColor(2));
		PhysicsMaterialRefC material_3 = new PhysicsMaterialSimple("Material 3", Color::sGetDistinctColor(3));
		PhysicsMaterialRefC material_4 = new PhysicsMaterialSimple("Material 4", Color::sGetDistinctColor(4));
		PhysicsMaterialRefC material_5 = new PhysicsMaterialSimple("Material 5", Color::sGetDistinctColor(5));

		// Create height field with a single material
		HeightFieldShapeSettings settings;
		settings.mSampleCount = cSampleCount;
		settings.mBitsPerSample = 8;
		settings.mBlockSize = 4;
		settings.mHeightSamples.resize(Square(cSampleCount));
		for (float &h : settings.mHeightSamples)
			h = 0.0f;
		settings.mMaterials.push_back(material_0);
		settings.mMaterialIndices.resize(Square(cSampleCount - 1));
		for (uint8 &m : settings.mMaterialIndices)
			m = 0;

		// Store the current state
		Array<const PhysicsMaterial *> current_state;
		current_state.resize(Square(cSampleCount - 1));
		for (const PhysicsMaterial *&m : current_state)
			m = material_0;

		// Create shape
		Ref<Shape> shape = settings.Create().Get();
		HeightFieldShape *height_field = StaticCast<HeightFieldShape>(shape);

		// Check that the material is set
		auto check_materials = [height_field, &current_state]() {
			const PhysicsMaterialList &material_list = height_field->GetMaterialList();

			uint sample_count_min_1 = height_field->GetSampleCount() - 1;

			Array<uint8> material_indices;
			material_indices.resize(Square(sample_count_min_1));
			height_field->GetMaterials(0, 0, sample_count_min_1, sample_count_min_1, material_indices.data(), sample_count_min_1);

			for (uint i = 0; i < (uint)current_state.size(); ++i)
				CHECK(current_state[i] == material_list[material_indices[i]]);
		};
		check_materials();

		// Function to randomize materials
		auto update_materials = [height_field, &current_state](uint inStartX, uint inStartY, uint inSizeX, uint inSizeY, const PhysicsMaterialList *inMaterialList) {
			TempAllocatorMalloc temp_allocator;

			const PhysicsMaterialList &material_list = inMaterialList != nullptr? *inMaterialList : height_field->GetMaterialList();

			UnitTestRandom random;
			uniform_int_distribution<uint> index_distribution(0, uint(material_list.size()) - 1);

			uint sample_count_min_1 = height_field->GetSampleCount() - 1;

			Array<uint8> patched_materials;
			patched_materials.resize(inSizeX * inSizeY);
			for (uint y = 0; y < inSizeY; ++y)
				for (uint x = 0; x < inSizeX; ++x)
				{
					// Initialize the patch
					uint8 index = uint8(index_distribution(random));
					patched_materials[y * inSizeX + x] = index;

					// Update reference state
					current_state[(inStartY + y) * sample_count_min_1 + inStartX + x] = material_list[index];
				}
			CHECK(height_field->SetMaterials(inStartX, inStartY, inSizeX, inSizeY, patched_materials.data(), inSizeX, inMaterialList, temp_allocator));
		};

		{
			// Add material 1
			PhysicsMaterialList patched_materials_list;
			patched_materials_list.push_back(material_1);
			patched_materials_list.push_back(material_0);
			update_materials(4, 16, 16, 8, &patched_materials_list);
			check_materials();
		}

		{
			// Add material 2
			PhysicsMaterialList patched_materials_list;
			patched_materials_list.push_back(material_0);
			patched_materials_list.push_back(material_2);
			update_materials(8, 16, 16, 8, &patched_materials_list);
			check_materials();
		}

		{
			// Add material 3
			PhysicsMaterialList patched_materials_list;
			patched_materials_list.push_back(material_0);
			patched_materials_list.push_back(material_1);
			patched_materials_list.push_back(material_2);
			patched_materials_list.push_back(material_3);
			update_materials(8, 8, 16, 8, &patched_materials_list);
			check_materials();
		}

		{
			// Add material 4
			PhysicsMaterialList patched_materials_list;
			patched_materials_list.push_back(material_0);
			patched_materials_list.push_back(material_1);
			patched_materials_list.push_back(material_4);
			patched_materials_list.push_back(material_2);
			patched_materials_list.push_back(material_3);
			update_materials(0, 0, 30, 30, &patched_materials_list);
			check_materials();
		}

		{
			// Add material 5
			PhysicsMaterialList patched_materials_list;
			patched_materials_list.push_back(material_4);
			patched_materials_list.push_back(material_3);
			patched_materials_list.push_back(material_0);
			patched_materials_list.push_back(material_1);
			patched_materials_list.push_back(material_2);
			patched_materials_list.push_back(material_5);
			update_materials(1, 1, 30, 30, &patched_materials_list);
			check_materials();
		}

		{
			// Update materials without new material list
			update_materials(2, 5, 10, 15, nullptr);
			check_materials();
		}

		// Check materials using GetMaterial call
		for (uint y = 0; y < cSampleCount - 1; ++y)
			for (uint x = 0; x < cSampleCount - 1; ++x)
				CHECK(height_field->GetMaterial(x, y) == current_state[y * (cSampleCount - 1) + x]);
	}
}
