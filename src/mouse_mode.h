/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mouse_mode.h Mouse mode handling. */

#ifndef MOUSE_MODE_H
#define MOUSE_MODE_H

#include "geometry.h"

/**
 * Available cursor types.
 * @ingroup viewport_group
 */
enum CursorType {
	CUR_TYPE_NORTH,    ///< Show a N corner highlight.
	CUR_TYPE_EAST,     ///< Show a E corner highlight.
	CUR_TYPE_SOUTH,    ///< Show a S corner highlight.
	CUR_TYPE_WEST,     ///< Show a W corner highlight.
	CUR_TYPE_TILE,     ///< Show a tile highlight.
	CUR_TYPE_ARROW_NE, ///< Show a build arrow in the NE direction.
	CUR_TYPE_ARROW_SE, ///< Show a build arrow in the SE direction.
	CUR_TYPE_ARROW_SW, ///< Show a build arrow in the SW direction.
	CUR_TYPE_ARROW_NW, ///< Show a build arrow in the NW direction.
	CUR_TYPE_EDGE_NE,  ///< Show a NE edge sprite highlight.
	CUR_TYPE_EDGE_SE,  ///< Show a SE edge sprite highlight.
	CUR_TYPE_EDGE_SW,  ///< Show a SW edge sprite highlight.
	CUR_TYPE_EDGE_NW,  ///< Show a NW edge sprite highlight.

	CUR_TYPE_INVALID = 0xFF, ///< Invalid/unused cursor.
};

/** Base class for displaying and handling mouse modes. */
class MouseModeSelector {
public:
	MouseModeSelector();
	virtual ~MouseModeSelector();

	virtual void MarkDirty();
	virtual CursorType GetCursor(const XYZPoint16 &voxel_pos) = 0;

	/**
	 * Rough estimate whether the selector wants to render something in the voxel stack at the given coordinate.
	 * @param x X position of the stack.
	 * @param y Y position of the stack.
	 * @return Whether the selector wants to contribute to the graphics in the given stack.
	 */
	inline bool IsInsideArea(int x, int y) const {
		return this->area.IsPointInside(x, y);
	}

	Rectangle16 area; ///< Position and size of the selected area (over-approximation of voxel stacks).
};

/** Data of a tile. */
struct TileData {
	int8 height;  ///< Height of the cursor (equal to ground height, except at steep slopes). Negative value means 'unknown'.
	bool enabled; ///< Whether the tile should have a cursor displayed.
};

/** Mouse mode displaying a tile cursor of some size at the ground. */
class CursorMouseMode : public MouseModeSelector {
public:
	CursorMouseMode();
	~CursorMouseMode();

	virtual void MarkDirty() override;
	virtual CursorType GetCursor(const XYZPoint16 &voxel_pos) override;

	void SetSize(int xsize, int ysize);
	void SetPosition(int xbase, int ybase);
	void InitTileData();

	/**
	 * Get the tile data at the given relative position.
	 * @param dx X position relative to the base position.
	 * @param dy Y position relative to the base position.
	 * @return Reference to the tile data.
	 */
	inline TileData &GetTileData(int dx, int dy)
	{
		return this->ground_height[dx * this->area.height + dy];
	}

	std::vector<TileData> ground_height; ///< Height of the ground within the area, negative means 'unknown'.
	CursorType cur_cursor;               ///< Cursor to return at the #GetCursor call.
};

#endif

