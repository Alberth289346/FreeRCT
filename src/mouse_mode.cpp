/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mouse_mode.cpp Mouse mode handling implementation. */

#include "stdafx.h"
#include "mouse_mode.h"
#include "math_func.h"
#include "map.h"
#include "viewport.h"

MouseModeSelector::MouseModeSelector()
{
}

MouseModeSelector::~MouseModeSelector()
{
}

/** Mark all voxels changed by the selector as dirty, so they get redrawn. */
void MouseModeSelector::MarkDirty()
{
}

/**
 * \fn CursorMouseMode MouseModeSelector::GetCursor(const XYZPoint16 &voxel_pos) const
 * Retrieve the cursor to display at the given voxel.
 * @param voxel_pos %Voxel to decorate with a cursor.
 * @return The cursor to use, or #CUR_TYPE_INVALID if no cursor should be displayed.
 */

CursorMouseMode::CursorMouseMode() : MouseModeSelector()
{
}

CursorMouseMode::~CursorMouseMode()
{
}

/**
 * Set the size of the cursor area.
 * @param xsize Horizontal size of the area.
 * @param ysize Vertical size of the area.
 */
void CursorMouseMode::SetSize(int xsize, int ysize)
{
	xsize = Clamp(xsize, 0, 128); // Arbitrary upper limit.
	ysize = Clamp(ysize, 0, 128);
	this->area.width = xsize;
	this->area.height = ysize;
	this->InitTileData();
}

/** Initialize the tile data of the cursor area. */
void CursorMouseMode::InitTileData()
{
	if (this->area.width == 0 || this->area.height == 0) return;

	/* Setup the cursor area for the current position and size. */
	int size = this->area.width * this->area.height;
	this->ground_height.resize(size);
	for (int x = 0; x < this->area.width; x++) {
		int xpos = this->area.base.x + x;
		for (int y = 0; y < this->area.height; y++) {
			int ypos = this->area.base.y + y;
			TileData &td = this->GetTileData(x, y);
			td.enabled = xpos >= 0 && xpos < _world.GetXSize() && ypos >= 0 && ypos < _world.GetYSize() &&
					_world.GetTileOwner(xpos, ypos) == OWN_PARK;
			td.height = -1;
		}
	}
}

/**
 * Set the position of the cursor area.
 * @param xbase X coordinate of the base position.
 * @param ybase Y coordinate of the base position.
 */
void CursorMouseMode::SetPosition(int xbase, int ybase)
{
	this->area.base.x = xbase;
	this->area.base.y = ybase;
	this->InitTileData();
}

/**
 * Get the top height of ground at the given world voxel stack.
 * @param [inout] td Til data height cache.
 * @param xpos X position of the queried voxel stack.
 * @param ypos Y position of the queried voxel stack.
 * @return Top voxel height containing ground.
 */
static inline int GetTopGroundHeight(TileData &td, int xpos, int ypos)
{
	if (td.height >= 0) return td.height;
	td.height = _world.GetTopGroundHeight(xpos, ypos);
	return td.height;
}

CursorType CursorMouseMode::GetCursor(const XYZPoint16 &voxel_pos)
{
	int x = voxel_pos.x - this->area.base.x;
	if (x < 0 || x >= this->area.width) return CUR_TYPE_INVALID;

	int y = voxel_pos.y - this->area.base.y;
	if (y < 0 || y >= this->area.height) return CUR_TYPE_INVALID;

	TileData &td = this->GetTileData(x, y);
	if (td.enabled && GetTopGroundHeight(td, voxel_pos.x, voxel_pos.y) == voxel_pos.z) return CUR_TYPE_TILE;
	return CUR_TYPE_INVALID;
}

void CursorMouseMode::MarkDirty()
{
	for (int x = 0; x < this->area.width; x++) {
		int xpos = this->area.base.x + x;
		for (int y = 0; y < this->area.height; y++) {
			int ypos = this->area.base.y + y;
			TileData &td = this->GetTileData(x, y);
			if (!td.enabled) continue;

			MarkVoxelDirty(XYZPoint16(xpos, ypos, GetTopGroundHeight(td, xpos, ypos)), 0);
		}
	}
}

