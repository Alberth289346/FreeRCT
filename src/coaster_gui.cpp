/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file coaster_gui.cpp Roller coaster windows. */

#include "stdafx.h"
#include "math_func.h"
#include "memory.h"
#include "window.h"
#include "sprite_store.h"
#include "ride_type.h"
#include "coaster.h"
#include "coaster_build.h"
#include "map.h"
#include "gui_sprites.h"

//XXX CoasterBuildMode _coaster_builder; ///< Coaster build mouse mode handler.

/** Widget numbers of the roller coaster instance window. */
enum CoasterInstanceWidgets {
	CIW_TITLEBAR, ///< Titlebar widget.
};

/** Widget parts of the #CoasterInstanceWindow. */
static const WidgetPart _coaster_instance_gui_parts[] = {
	Intermediate(0, 1),
		Intermediate(1, 0),
			Widget(WT_TITLEBAR, CIW_TITLEBAR, COL_RANGE_DARK_RED), SetData(STR_ARG1, GUI_TITLEBAR_TIP),
			Widget(WT_CLOSEBOX, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED),
		EndContainer(),

		Widget(WT_PANEL, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED),
			Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetMinimalSize(100, 100),
	EndContainer(),
};


/** Window to display and setup a roller coaster. */
class CoasterInstanceWindow : public GuiWindow {
public:
	CoasterInstanceWindow(CoasterInstance *ci);
	~CoasterInstanceWindow();

	void SetWidgetStringParameters(WidgetNumber wid_num) const override;

private:
	CoasterInstance *ci; ///< Roller coaster instance to display and control.
};

/**
 * Constructor of the roller coaster instance window.
 * @param ci Roller coaster instance to display and control.
 */
CoasterInstanceWindow::CoasterInstanceWindow(CoasterInstance *ci) : GuiWindow(WC_COASTER_MANAGER, ci->GetIndex())
{
	this->ci = ci;
	this->SetupWidgetTree(_coaster_instance_gui_parts, lengthof(_coaster_instance_gui_parts));
}

CoasterInstanceWindow::~CoasterInstanceWindow()
{
	if (!GetWindowByType(WC_COASTER_BUILD, this->wnumber) && !this->ci->IsAccessible()) {
		_rides_manager.DeleteInstance(this->ci->GetIndex());
	}
}

void CoasterInstanceWindow::SetWidgetStringParameters(WidgetNumber wid_num) const
{
	switch (wid_num) {
		case CIW_TITLEBAR:
			_str_params.SetUint8(1, (uint8 *)this->ci->name);
			break;
	}
}

/**
 * Open a roller coaster management window for the given roller coaster ride.
 * @param coaster Coaster instance to display.
 */
void ShowCoasterManagementGui(RideInstance *coaster)
{
	if (coaster->GetKind() != RTK_COASTER) return;
	CoasterInstance *ci = static_cast<CoasterInstance *>(coaster);
	assert(ci != nullptr);

	RideInstanceState ris = ci->DecideRideState();
	if (ris == RIS_TESTING || ris == RIS_CLOSED || ris == RIS_OPEN) {
		if (HighlightWindowByType(WC_COASTER_MANAGER, coaster->GetIndex())) return;

		new CoasterInstanceWindow(ci);
		return;
	}
	ShowCoasterBuildGui(ci);
}

/** Mouse selector for building/selecting new track pieces. */
class TrackPieceMouseMode : public CursorMouseMode {
public:
	TrackPieceMouseMode();
	~TrackPieceMouseMode();

	void SetTrackPiece(const XYZPoint16 &pos, ConstTrackPiecePtr &piece);

	ConstTrackPiecePtr piece; ///< Piece to display, or \c nullptr if no piece to display.
	XYZPoint16 piece_pos;     ///< Position of the track piece (may be different from the base of the cursor area).
};

TrackPieceMouseMode::TrackPieceMouseMode() : CursorMouseMode()
{
	this->piece = nullptr;
}

TrackPieceMouseMode::~TrackPieceMouseMode() { }

/**
 * Setup the mouse selector for displaying a track piece.
 * @param pos Base position of the new track piece.
 * @param piece Track piece to display.
 */
void TrackPieceMouseMode::SetTrackPiece(const XYZPoint16 &pos, ConstTrackPiecePtr &piece)
{
	if (this->piece != nullptr) this->MarkDirty(); // Mark current area.

	this->piece = piece;
	if (this->piece != nullptr) {
		this->piece_pos = pos;

		this->area = this->piece->GetArea();
		this->area.base.x += pos.x; // Set new cursor area, origin may be different from piece_pos due to negative extent of a piece.
		this->area.base.y += pos.y;

		const int dx = pos.x - this->area.base.x;
		const int dy = pos.y - this->area.base.y;
		this->InitTileData();
		for (const TrackVoxel *tv : this->piece->track_voxels) {
			int xpos = dx + tv->dxyz.x; // (pos.x + tv->dxyz.x) - this->area.base.x
			int ypos = dy + tv->dxyz.y; // (pos.y + tv->dxyz.y) - this->area.base.y
			assert(xpos >= 0 && xpos < this->area.width);
			assert(ypos >= 0 && ypos < this->area.height);

			TileData &td = GetTileData(xpos, ypos);
			td.cursor_enabled = true;
			td.AddVoxel(pos.z + tv->dxyz.z);
		}

		this->MarkDirty();
	}
}

/** Widgets of the coaster construction window. */
enum CoasterConstructionWidgets {
	CCW_TITLEBAR,            ///< Titlebar widget.
	CCW_BEND_WIDE_LEFT,      ///< Button for selecting wide left turn. Same order as #TrackBend.
	CCW_BEND_NORMAL_LEFT,    ///< Button for selecting normal left turn.
	CCW_BEND_TIGHT_LEFT,     ///< Button for selecting tight left turn.
	CCW_BEND_NONE,           ///< Button for selecting straight ahead (no turn).
	CCW_BEND_TIGHT_RIGHT,    ///< Button for selecting tight right turn.
	CCW_BEND_NORMAL_RIGHT,   ///< Button for selecting normal right turn.
	CCW_BEND_WIDE_RIGHT,     ///< Button for selecting wide right turn.
	CCW_BANK_NONE,           ///< Button for selecting no banking. Same order as #TrackPieceBanking.
	CCW_BANK_LEFT,           ///< Button for selecting banking to the left.
	CCW_BANK_RIGHT,          ///< Button for selecting banking to the right.
	CCW_NO_PLATFORM,         ///< Button for selecting tracks without platform.
	CCW_PLATFORM,            ///< Button for selecting tracks with platform.
	CCW_NOT_POWERED,         ///< Button for selecting unpowered tracks.
	CCW_POWERED,             ///< Button for selecting powered tracks.
	CCW_SLOPE_DOWN,          ///< Button for selecting gentle down slope. Same order as #TrackSlope.
	CCW_SLOPE_FLAT,          ///< Button for selecting level slope.
	CCW_SLOPE_UP,            ///< Button for selecting gentle up slope.
	CCW_SLOPE_STEEP_DOWN,    ///< Button for selecting steep down slope.
	CCW_SLOPE_STEEP_UP,      ///< Button for selecting steep up slope.
	CCW_SLOPE_VERTICAL_DOWN, ///< Button for selecting vertically down slope.
	CCW_SLOPE_VERTICAL_UP,   ///< Button for selecting vertically up slope.
	CCW_DISPLAY_PIECE,       ///< Display space for a track piece.
	CCW_REMOVE,              ///< Remove track piece button.
	CCW_BACKWARD,            ///< Move backward.
	CCW_FORWARD,             ///< Move forward.
	CCW_ROT_NEG,             ///< Rotate in negative direction.
	CCW_ROT_POS,             ///< Rotate in positive direction.
};

/** Widget parts of the #CoasterBuildWindow. */
static const WidgetPart _coaster_construction_gui_parts[] = {
	Intermediate(0, 1),
		Intermediate(1, 0),
			Widget(WT_TITLEBAR, CCW_TITLEBAR, COL_RANGE_DARK_RED), SetData(STR_ARG1, GUI_TITLEBAR_TIP),
			Widget(WT_CLOSEBOX, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED),
		EndContainer(),

		Widget(WT_PANEL, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED),
			Intermediate(5, 1),
				Intermediate(1, 9), // Bend type.
					Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetFill(1, 0),
					Widget(WT_IMAGE_BUTTON, CCW_BEND_WIDE_LEFT,    COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BEND_START + TBN_LEFT_WIDE, GUI_COASTER_BUILD_LEFT_BEND_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_BEND_NORMAL_LEFT,  COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BEND_START + TBN_LEFT_NORMAL, GUI_COASTER_BUILD_LEFT_BEND_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_BEND_TIGHT_LEFT,   COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BEND_START + TBN_LEFT_TIGHT, GUI_COASTER_BUILD_LEFT_BEND_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_BEND_NONE,         COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BEND_START + TBN_STRAIGHT, GUI_COASTER_BUILD_NO_BEND_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_BEND_TIGHT_RIGHT,  COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BEND_START + TBN_RIGHT_TIGHT, GUI_COASTER_BUILD_RIGHT_BEND_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_BEND_NORMAL_RIGHT, COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BEND_START + TBN_RIGHT_NORMAL, GUI_COASTER_BUILD_RIGHT_BEND_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_BEND_WIDE_RIGHT,   COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BEND_START + TBN_RIGHT_WIDE, GUI_COASTER_BUILD_RIGHT_BEND_TOOLTIP),
					Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetFill(1, 0),
				Intermediate(1, 11), // Banking, platforms, powered.
					Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetFill(1, 0),
					Widget(WT_IMAGE_BUTTON, CCW_BANK_LEFT,  COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BANK_START + TPB_LEFT, GUI_COASTER_BUILD_BANK_LEFT_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_BANK_NONE,  COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_BANK_START + TPB_NONE, GUI_COASTER_BUILD_BANK_NONE_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_BANK_RIGHT, COL_RANGE_DARK_RED), SetPadding(0, 0, 3, 0),
							SetData(SPR_GUI_BANK_START + TPB_RIGHT, GUI_COASTER_BUILD_BANK_RIGHT_TOOLTIP),
					Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetFill(1, 0),
					Widget(WT_IMAGE_BUTTON, CCW_PLATFORM, COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_HAS_PLATFORM, GUI_COASTER_BUILD_BANK_RIGHT_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_NO_PLATFORM, COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_NO_PLATFORM, GUI_COASTER_BUILD_BANK_RIGHT_TOOLTIP),
					Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetFill(1, 0),
					Widget(WT_IMAGE_BUTTON, CCW_POWERED, COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_HAS_POWER, GUI_COASTER_BUILD_BANK_RIGHT_TOOLTIP),
					Widget(WT_IMAGE_BUTTON, CCW_NOT_POWERED, COL_RANGE_DARK_RED), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_NO_POWER, GUI_COASTER_BUILD_BANK_RIGHT_TOOLTIP),
					Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetFill(1, 0),
				Intermediate(1, 9), // Slopes.
					Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetFill(1, 0),
					Widget(WT_IMAGE_BUTTON, CCW_SLOPE_VERTICAL_DOWN, COL_RANGE_GREY), SetPadding(0, 0, 0, 5),
							SetData(SPR_GUI_SLOPES_START + TSL_STRAIGHT_DOWN, GUI_PATH_GUI_SLOPE_DOWN_TIP),
					Widget(WT_IMAGE_BUTTON, CCW_SLOPE_STEEP_DOWN, COL_RANGE_GREY), SetPadding(0, 0, 0, 5),
							SetData(SPR_GUI_SLOPES_START + TSL_STEEP_DOWN, GUI_PATH_GUI_SLOPE_DOWN_TIP),
					Widget(WT_IMAGE_BUTTON, CCW_SLOPE_DOWN, COL_RANGE_GREY), SetPadding(0, 0, 0, 5),
							SetData(SPR_GUI_SLOPES_START + TSL_DOWN, GUI_PATH_GUI_SLOPE_DOWN_TIP),
					Widget(WT_IMAGE_BUTTON, CCW_SLOPE_FLAT, COL_RANGE_GREY), SetPadding(0, 0, 0, 5),
							SetData(SPR_GUI_SLOPES_START + TSL_FLAT, GUI_PATH_GUI_SLOPE_FLAT_TIP),
					Widget(WT_IMAGE_BUTTON, CCW_SLOPE_UP, COL_RANGE_GREY), SetPadding(0, 0, 0, 5),
							SetData(SPR_GUI_SLOPES_START + TSL_UP, GUI_PATH_GUI_SLOPE_UP_TIP),
					Widget(WT_IMAGE_BUTTON, CCW_SLOPE_STEEP_UP, COL_RANGE_GREY), SetPadding(0, 0, 0, 5),
							SetData(SPR_GUI_SLOPES_START + TSL_STEEP_UP, GUI_PATH_GUI_SLOPE_UP_TIP),
					Widget(WT_IMAGE_BUTTON, CCW_SLOPE_VERTICAL_UP, COL_RANGE_GREY), SetPadding(0, 5, 0, 5),
							SetData(SPR_GUI_SLOPES_START + TSL_STRAIGHT_UP, GUI_PATH_GUI_SLOPE_UP_TIP),
					Widget(WT_EMPTY, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetFill(1, 0),
				Widget(WT_PANEL, INVALID_WIDGET_INDEX, COL_RANGE_DARK_RED), SetPadding(5, 2, 5, 2),
					Widget(WT_TEXT_PUSHBUTTON, CCW_DISPLAY_PIECE, COL_RANGE_DARK_RED),
							SetData(STR_NULL, GUI_COASTER_BUILD_BUY_TOOLTIP), SetFill(1, 1), SetMinimalSize(200, 200),
				Intermediate(1, 5), // delete, prev/next, rotate
					Widget(WT_TEXT_PUSHBUTTON, CCW_REMOVE, COL_RANGE_DARK_RED),  SetPadding(0, 3, 3, 0),
							SetData(GUI_PATH_GUI_REMOVE, GUI_PATH_GUI_BULLDOZER_TIP),
					Widget(WT_TEXT_PUSHBUTTON, CCW_BACKWARD, COL_RANGE_DARK_RED),  SetPadding(0, 3, 3, 0),
							SetData(GUI_PATH_GUI_BACKWARD, GUI_PATH_GUI_BACKWARD_TIP),
					Widget(WT_TEXT_PUSHBUTTON, CCW_FORWARD, COL_RANGE_DARK_RED),  SetPadding(0, 3, 3, 0),
							SetData(GUI_PATH_GUI_FORWARD, GUI_PATH_GUI_FORWARD_TIP),
					Widget(WT_IMAGE_PUSHBUTTON, CCW_ROT_POS, COL_RANGE_DARK_GREEN), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_ROT3D_POS, GUI_RIDE_SELECT_ROT_POS_TOOLTIP),
					Widget(WT_IMAGE_PUSHBUTTON, CCW_ROT_NEG, COL_RANGE_DARK_GREEN), SetPadding(0, 3, 3, 0),
							SetData(SPR_GUI_ROT3D_NEG, GUI_RIDE_SELECT_ROT_NEG_TOOLTIP),
	EndContainer(),
};

/** Three-valued boolean. */
enum BoolSelect {
	BSL_FALSE, ///< Selected boolean is \c false.
	BSL_TRUE,  ///< Selected boolean is \c true.
	BSL_NONE,  ///< Boolean is not selectable.
};

/**
 * Window to build or edit a roller coaster.
 *
 * The build window can be in the following state
 * - #cur_piece is \c nullptr: An initial piece is being placed, The mouse mode defines where, #build_direction defines in which direction.
 * - #cur_piece is not \c nullptr, and #cur_after: A piece is added after #cur_piece.
 * - #cur_piece is not \c nullptr, and not #cur_after: A piece is added before #cur_piece.
 * In the latter two cases, #cur_sel points at the piece being replaced, if it exists.
 */
class CoasterBuildWindow : public GuiWindow {
public:
	CoasterBuildWindow(CoasterInstance *ci);
	~CoasterBuildWindow();

	void SetWidgetStringParameters(WidgetNumber wid_num) const override;
	void OnChange(ChangeCode code, uint32 parameter) override;
	void OnClick(WidgetNumber widget, const Point16 &pos) override;

	void SelectorMouseMoveEvent(Viewport *vp, const Point16 &pos) override;
	void SelectorMouseButtonEvent(uint8 state) override;

private:
	CoasterInstance *ci; ///< Roller coaster instance to build or edit.

	PositionedTrackPiece *cur_piece;     ///< Current track piece, if available (else \c nullptr).
	bool cur_after;                      ///< Position relative to #cur_piece, \c false means before, \c true means after.
	const PositionedTrackPiece *cur_sel; ///< Selected track piece of #cur_piece and #cur_after, or \c nullptr if no piece selected.

	ConstTrackPiecePtr sel_piece; ///< Currently selected piece (and not yet build), if any.
	TileEdge build_direction;     ///< If #cur_piece is \c nullptr, the direction of building.
	TrackSlope sel_slope;         ///< Selected track slope at the UI, or #TSL_INVALID.
	TrackBend sel_bend;           ///< Selected bend at the UI, or #TPB_INVALID.
	TrackPieceBanking sel_bank;   ///< Selected bank at the UI, or #TBN_INVALID.
	BoolSelect sel_platform;      ///< Whether the track piece should have a platform, or #BSL_NONE.
	BoolSelect sel_power;         ///< Whether the selected piece should have power, or #BSL_NONE.

	void SetupSelection();
	int SetButtons(int start_widget, int count, uint avail, int cur_sel, int invalid_val);
	int BuildTrackPiece();

	TrackPieceMouseMode piece_selector; ///< Selector for displaying new track pieces.
};

/**
 * Constructor of the roller coaster build window. The provided instance may be completely empty.
 * @param ci Coaster instance to build or modify.
 */
CoasterBuildWindow::CoasterBuildWindow(CoasterInstance *ci) : GuiWindow(WC_COASTER_BUILD, ci->GetIndex())
{
	this->ci = ci;
	this->SetupWidgetTree(_coaster_construction_gui_parts, lengthof(_coaster_construction_gui_parts));
// XXX	_coaster_builder.OpenWindow(this->ci->GetIndex());
// XXX	_mouse_modes.SetMouseMode(MM_COASTER_BUILD);

	int first = this->ci->GetFirstPlacedTrackPiece();
	if (first >= 0) {
		this->cur_piece = &this->ci->pieces[first];
		first = this->ci->FindSuccessorPiece(*this->cur_piece);
		this->cur_sel = (first >= 0) ? &this->ci->pieces[first] : nullptr;
	} else {
		this->cur_piece = nullptr;
		this->cur_sel = nullptr;
	}
	this->cur_after = true;
	this->build_direction = EDGE_NE;
	this->sel_slope = TSL_INVALID;
	this->sel_bend = TBN_INVALID;
	this->sel_bank = TPB_INVALID;
	this->sel_platform = BSL_NONE;
	this->sel_power = BSL_NONE;

	this->SetSelector(&this->piece_selector);
	this->SetupSelection();
}

CoasterBuildWindow::~CoasterBuildWindow()
{
// XXX	_coaster_builder.CloseWindow(this->ci->GetIndex());

	if (!GetWindowByType(WC_COASTER_MANAGER, this->wnumber) && !this->ci->IsAccessible()) {
		_rides_manager.DeleteInstance(this->ci->GetIndex());
	}
}

void CoasterBuildWindow::SetWidgetStringParameters(WidgetNumber wid_num) const
{
	switch (wid_num) {
		case CCW_TITLEBAR:
			_str_params.SetUint8(1, (uint8 *)this->ci->name);
			break;
	}
}

void CoasterBuildWindow::OnClick(WidgetNumber widget, const Point16 &pos)
{
	switch (widget) {
		case CCW_BANK_NONE:
		case CCW_BANK_LEFT:
		case CCW_BANK_RIGHT:
			this->sel_bank = (TrackPieceBanking)(widget - CCW_BANK_NONE);
			break;

		case CCW_PLATFORM:
			this->sel_platform = this->IsWidgetPressed(widget) ? BSL_NONE : BSL_TRUE;
			break;

		case CCW_NO_PLATFORM:
			this->sel_platform = this->IsWidgetPressed(widget) ? BSL_NONE : BSL_FALSE;
			break;

		case CCW_POWERED:
			this->sel_power = this->IsWidgetPressed(widget) ? BSL_NONE : BSL_TRUE;
			break;

		case CCW_NOT_POWERED:
			this->sel_power = this->IsWidgetPressed(widget) ? BSL_NONE : BSL_FALSE;
			break;

		case CCW_SLOPE_DOWN:
		case CCW_SLOPE_FLAT:
		case CCW_SLOPE_UP:
		case CCW_SLOPE_STEEP_DOWN:
		case CCW_SLOPE_STEEP_UP:
		case CCW_SLOPE_VERTICAL_DOWN:
		case CCW_SLOPE_VERTICAL_UP:
			this->sel_slope = (TrackSlope)(widget - CCW_SLOPE_DOWN);
			break;

		case CCW_DISPLAY_PIECE: {
			int index = this->BuildTrackPiece();
			if (index >= 0) {
				/* Piece was added, change the setup for the next piece. */
				this->cur_piece = &this->ci->pieces[index];
				int succ = this->ci->FindSuccessorPiece(*this->cur_piece);
				this->cur_sel = (succ >= 0) ? &this->ci->pieces[succ] : nullptr;
				this->cur_after = true;
			}
			break;
		}
		case CCW_REMOVE: {
			int pred_index = this->ci->FindPredecessorPiece(*this->cur_piece);
			_additions.Clear();
			this->ci->RemovePositionedPiece(*this->cur_piece);
			_additions.Commit();
			this->cur_piece = pred_index == -1 ? nullptr : &this->ci->pieces[pred_index];
			break;
		}

		case CCW_BEND_WIDE_LEFT:
		case CCW_BEND_NORMAL_LEFT:
		case CCW_BEND_TIGHT_LEFT:
		case CCW_BEND_NONE:
		case CCW_BEND_TIGHT_RIGHT:
		case CCW_BEND_NORMAL_RIGHT:
		case CCW_BEND_WIDE_RIGHT:
			this->sel_bend = (TrackBend)(widget - CCW_BEND_WIDE_LEFT);
			break;

		case CCW_ROT_NEG:
			if (this->cur_piece == nullptr) {
				this->build_direction = (TileEdge)((this->build_direction + 1) % 4);
			}
			break;

		case CCW_ROT_POS:
			if (this->cur_piece == nullptr) {
				this->build_direction = (TileEdge)((this->build_direction + 3) % 4);
			}
			break;
	}
	this->SetupSelection();
}

/**
 * Set buttons according to availability of track pieces.
 * @param start_widget First widget of the buttons.
 * @param count Number of buttons.
 * @param avail Bitset of available track pieces for the buttons.
 * @param cur_sel Currently selected button.
 * @param invalid_val Invalid value for the selection.
 * @return New value for the current selection.
 */
int CoasterBuildWindow::SetButtons(int start_widget, int count, uint avail, int cur_sel, int invalid_val)
{
	int num_bits = CountBits(avail);
	for (int i = 0; i < count; i++) {
		if ((avail & (1 << i)) == 0) {
			this->SetWidgetShaded(start_widget + i, true);
			if (cur_sel == i) cur_sel = invalid_val;
		} else {
			this->SetWidgetShaded(start_widget + i, false);
			if (num_bits == 1) cur_sel = i;
			this->SetWidgetPressed(start_widget + i, cur_sel == i);
		}
	}
	return cur_sel;
}

/**
 * Find out whether the provided track piece has a platform.
 * @param piece Track piece to examine.
 * @return #BSL_TRUE if the track piece has a platform, else #BSL_FALSE.
 */
static BoolSelect GetPlatform(ConstTrackPiecePtr piece)
{
	return piece->HasPlatform() ? BSL_TRUE : BSL_FALSE;
}

/**
 * Find out whether the provided track piece has is powered.
 * @param piece Track piece to examine.
 * @return #BSL_TRUE if the track piece is powered, else #BSL_FALSE.
 */
static BoolSelect GetPower(ConstTrackPiecePtr piece)
{
	return piece->HasPower() ? BSL_TRUE : BSL_FALSE;
}

/** Set up the window so the user can make a selection. */
void CoasterBuildWindow::SetupSelection()
{
	uint directions = 0; // Build directions of initial pieces.
	uint avail_bank = 0;
	uint avail_slope = 0;
	uint avail_bend = 0;
	uint avail_platform = 0;
	uint avail_power = 0;
	this->sel_piece = nullptr;

	if (this->cur_piece == nullptr || this->cur_sel == nullptr) {
		/* Only consider track pieces when there is no current positioned track piece. */

		const CoasterType *ct = this->ci->GetCoasterType();

		bool selectable[1024]; // Arbitrary limit on the number of non-placed track pieces.
		uint count = ct->pieces.size();
		if (count > lengthof(selectable)) count = lengthof(selectable);
		/* Round 1: Select on connection or initial placement. */
		for (uint i = 0; i < count; i++) {
			ConstTrackPiecePtr piece = ct->pieces[i];
			bool avail = true;
			if (this->cur_piece != nullptr) {
				/* Connect after or before 'cur_piece'. */
				if (this->cur_after) {
					if (piece->entry_connect != this->cur_piece->piece->exit_connect) avail = false;
				} else {
					if (piece->exit_connect != this->cur_piece->piece->entry_connect) avail = false;
				}
			} else {
				/* Initial placement. */
				if (!piece->IsStartingPiece()) {
					avail = false;
				} else {
					directions |= 1 << piece->GetStartDirection();
					if (piece->GetStartDirection() != this->build_direction) avail = false;
				}
			}
			selectable[i] = avail;
		}

		/* Round 2: Setup banking. */
		for (uint i = 0; i < count; i++) {
			if (!selectable[i]) continue;
			ConstTrackPiecePtr piece = ct->pieces[i];
			avail_bank |= 1 << piece->GetBanking();
		}
		if (this->sel_bank != TPB_INVALID && (avail_bank & (1 << this->sel_bank)) == 0) this->sel_bank = TPB_INVALID;

		/* Round 3: Setup slopes from pieces with the correct bank. */
		for (uint i = 0; i < count; i++) {
			if (!selectable[i]) continue;
			ConstTrackPiecePtr piece = ct->pieces[i];
			if (this->sel_bank != TPB_INVALID && piece->GetBanking() != this->sel_bank) {
				selectable[i] = false;
			} else {
				avail_slope |= 1 << piece->GetSlope();
			}
		}
		if (this->sel_slope != TSL_INVALID && (avail_slope & (1 << this->sel_slope)) == 0) this->sel_slope = TSL_INVALID;

		/* Round 4: Setup bends from pieces with the correct slope. */
		for (uint i = 0; i < count; i++) {
			if (!selectable[i]) continue;
			ConstTrackPiecePtr piece = ct->pieces[i];
			if (this->sel_slope != TSL_INVALID && piece->GetSlope() != this->sel_slope) {
				selectable[i] = false;
			} else {
				avail_bend |= 1 << piece->GetBend();
			}
		}
		if (this->sel_bend != TBN_INVALID && (avail_bend & (1 << this->sel_bend)) == 0) this->sel_bend = TBN_INVALID;

		/* Round 5: Setup platform from pieces with the correct bend. */
		for (uint i = 0; i < count; i++) {
			if (!selectable[i]) continue;
			ConstTrackPiecePtr piece = ct->pieces[i];
			if (this->sel_bend != TBN_INVALID && piece->GetBend() != this->sel_bend) {
				selectable[i] = false;
			} else {
				avail_platform |= 1 << GetPlatform(piece);
			}
		}
		if (this->sel_platform != BSL_NONE && (avail_platform & (1 << this->sel_platform)) == 0) this->sel_platform = BSL_NONE;

		/* Round 6: Setup power from pieces with the correct platform. */
		for (uint i = 0; i < count; i++) {
			if (!selectable[i]) continue;
			ConstTrackPiecePtr piece = ct->pieces[i];
			if (this->sel_platform != BSL_NONE && GetPlatform(piece) != this->sel_platform) {
				selectable[i] = false;
			} else {
				avail_power |= 1 << GetPower(piece);
			}
		}
		if (this->sel_power != BSL_NONE && (avail_power & (1 << this->sel_power)) == 0) this->sel_power = BSL_NONE;

		/* Round 7: Select a piece from the pieces with the correct power. */
		for (uint i = 0; i < count; i++) {
			if (!selectable[i]) continue;
			ConstTrackPiecePtr piece = ct->pieces[i];
			if (this->sel_power != BSL_NONE && GetPower(piece) != this->sel_power) continue;
			this->sel_piece = piece;
			break;
		}
	}

	/* Set shading of rotate buttons. */
	bool enabled = (this->cur_piece == nullptr && CountBits(directions) > 1);
	this->SetWidgetShaded(CCW_ROT_NEG,  !enabled);
	this->SetWidgetShaded(CCW_ROT_POS,  !enabled);
	enabled = (this->cur_piece != nullptr && this->cur_sel != nullptr);
	this->SetWidgetShaded(CCW_BACKWARD, !enabled);
	this->SetWidgetShaded(CCW_FORWARD,  !enabled);
	enabled = (this->cur_piece != nullptr && this->cur_sel == nullptr);
	this->SetWidgetShaded(CCW_DISPLAY_PIECE, !enabled);
	this->SetWidgetShaded(CCW_REMOVE, !enabled);

	this->sel_bank = static_cast<TrackPieceBanking>(this->SetButtons(CCW_BANK_NONE, TPB_COUNT, avail_bank, this->sel_bank, TPB_INVALID));
	this->sel_slope = static_cast<TrackSlope>(this->SetButtons(CCW_SLOPE_DOWN, TSL_COUNT_VERTICAL, avail_slope, this->sel_slope, TSL_INVALID));
	this->sel_bend = static_cast<TrackBend>(this->SetButtons(CCW_BEND_WIDE_LEFT, TBN_COUNT, avail_bend, this->sel_bend, TBN_INVALID));
	this->sel_platform = static_cast<BoolSelect>(this->SetButtons(CCW_NO_PLATFORM, 2, avail_platform, this->sel_platform, BSL_NONE));
	this->sel_power = static_cast<BoolSelect>(this->SetButtons(CCW_NOT_POWERED, 2, avail_power, this->sel_power, BSL_NONE));

	if (this->sel_piece == nullptr) {
		this->piece_selector.SetSize(0, 0); // Nothing to display.
		this->piece_selector.piece = nullptr;
		return;
	}

	if (this->cur_piece == nullptr) { // Display start-piece, moving it around.
		this->piece_selector.SetTrackPiece(XYZPoint16(0, 0, 0), this->sel_piece);
// XXX			_coaster_builder.SelectPosition(this->wnumber, this->sel_piece, this->build_direction);
		return;
	}

	if (this->cur_after) { // Disply next coaster piece.
		this->piece_selector.SetTrackPiece(this->cur_piece->GetEndXYZ(), this->sel_piece);
//				TileEdge dir = (TileEdge)(this->cur_piece->piece->exit_connect & 3); /// \todo Define this in the data format
// XXX				_coaster_builder.DisplayPiece(this->wnumber, this->sel_piece, this->cur_piece->GetEndXYZ(), dir);
		return;
	}

	// Display previous coaster piece.
	// XXX Fix me
	this->piece_selector.SetSize(0, 0); // XXX Fix me
	this->piece_selector.piece = nullptr;
}

void CoasterBuildWindow::SelectorMouseMoveEvent(Viewport *vp, const Point16 &pos)
{
	if (this->selector == nullptr || this->piece_selector.piece == nullptr) return; // No active selector.
	if (this->sel_piece == nullptr || this->cur_piece != nullptr) return; // No piece, or fixed position.

	FinderData fdata(CS_GROUND, FW_TILE);
	if (vp->ComputeCursorPosition(&fdata) != CS_GROUND) return;
	XYZPoint16 &piece_base = this->piece_selector.piece_pos;
	int dx = fdata.voxel_pos.x - piece_base.x;
	int dy = fdata.voxel_pos.y - piece_base.y;
	if (dx == 0 && dy == 0) return;

	this->piece_selector.MarkDirty();

	this->piece_selector.SetPosition(this->piece_selector.area.base.x + dx, this->piece_selector.area.base.y + dy);
	piece_base.x += dx; // Also update base position of the piece.
	piece_base.y += dy;

	this->piece_selector.MarkDirty();
}

void CoasterBuildWindow::SelectorMouseButtonEvent(uint8 state)
{
	printf("click!\n");
}

/**
 * Create the currently selected track piece in the world.
 * @return Position of the new piece in the coaster instance, or \c -1.
 */
int CoasterBuildWindow::BuildTrackPiece()
{
	if (this->sel_piece == nullptr) return -1;
	return -1;
	PositionedTrackPiece ptp(XYZPoint16(0, 0, 0), /*_coaster_builder.track_pos,*/ this->sel_piece);
	if (!ptp.CanBePlaced()) return -1;

	/* Add the piece to the coaster instance. */
	int ptp_index = this->ci->AddPositionedPiece(ptp);
	if (ptp_index >= 0) {
		/* Add the piece to the world. */
		_additions.Clear();
		this->ci->PlaceTrackPieceInAdditions(ptp);
		_additions.Commit();
	}
	return ptp_index;
}

void CoasterBuildWindow::OnChange(ChangeCode code, uint32 parameter)
{
	if (code != CHG_PIECE_POSITIONED || parameter != 0) return; // XXX ???

	int index = this->BuildTrackPiece();
	if (index >= 0) {
		/* Piece was added, change the setup for the next piece. */
		this->cur_piece = &this->ci->pieces[index];
		int succ = this->ci->FindSuccessorPiece(*this->cur_piece);
		this->cur_sel = (succ >= 0) ? &this->ci->pieces[succ] : nullptr;
		this->cur_after = true;
	}
	this->SetupSelection();
}

/**
 * Open a roller coaster build/edit window for the given roller coaster.
 * @param coaster Coaster instance to modify.
 */
void ShowCoasterBuildGui(CoasterInstance *coaster)
{
	if (coaster->GetKind() != RTK_COASTER) return;
	if (HighlightWindowByType(WC_COASTER_BUILD, coaster->GetIndex())) return;

	new CoasterBuildWindow(coaster);
}

// XXX /** State in the coaster build mouse mode. */
// XXX class CoasterBuildState {
// XXX public:
// XXX 	/**
// XXX 	 * Constructor of a build mouse mode state.
// XXX 	 * @param mode Mouse mode reference.
// XXX 	 */
// XXX 	CoasterBuildState(CoasterBuildMode &mode) : mode(&mode)
// XXX 	{
// XXX 	}
// XXX 
// XXX 	/**
// XXX 	 * Notification to the mouse mode that a coaster construction window has been opened.
// XXX 	 * @param instance Ride number of the window.
// XXX 	 */
// XXX 	virtual void OpenWindow(uint16 instance) const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Notification to the mouse mode that a coaster construction window has been closed.
// XXX 	 * @param instance Ride number of the window.
// XXX 	 */
// XXX 	virtual void CloseWindow(uint16 instance) const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Query from the viewport whether the mouse mode may be activated.
// XXX 	 * @return The mouse mode may be activated.
// XXX 	 * @see MouseMode::MayActivateMode
// XXX 	 */
// XXX 	virtual bool MayActivateMode() const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Notification from the viewport that the mouse mode has been activated.
// XXX 	 * @param pos Current mouse position.
// XXX 	 * @see MouseMode::ActivateMode
// XXX 	 */
// XXX 	virtual void ActivateMode(const Point16 &pos) const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Notification from the viewport that the mouse has moved.
// XXX 	 * @param vp The viewport.
// XXX 	 * @param old_pos Previous position of the mouse.
// XXX 	 * @param pos Current mouse position.
// XXX 	 * @see MouseMode::OnMouseMoveEvent
// XXX 	 */
// XXX 	virtual void OnMouseMoveEvent(Viewport *vp, const Point16 &old_pos, const Point16 &pos) const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Notification from the viewport that a mouse button has changed value.
// XXX 	 * @param vp The viewport.
// XXX 	 * @param state Old and new state of the mouse buttons.
// XXX 	 * @see MouseMode::OnMouseButtonEvent
// XXX 	 */
// XXX 	virtual void OnMouseButtonEvent(Viewport *vp, uint8 state) const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Notification from the viewport that the mouse mode has been de-activated.
// XXX 	 * @see MouseMode::LeaveMode
// XXX 	 */
// XXX 	virtual void LeaveMode() const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Query from the viewport whether the mouse mode wants to have cursors displayed.
// XXX 	 * @return Cursors should be enabled.
// XXX 	 * @see MouseMode::EnableCursors
// XXX 	 */
// XXX 	virtual bool EnableCursors() const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Notification from the construction window to display no track piece.
// XXX 	 * @param instance Ride number of the window.
// XXX 	 */
// XXX 	virtual void ShowNoPiece(uint16 instance) const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Notification from the construction window to display a track piece attached to the mouse cursor.
// XXX 	 * @param instance Ride number of the window.
// XXX 	 * @param piece Track piece to display.
// XXX 	 * @param direction Direction of building (to use with a cursor).
// XXX 	 */
// XXX 	virtual void SelectPosition(uint16 instance, ConstTrackPiecePtr piece, TileEdge direction) const = 0;
// XXX 
// XXX 	/**
// XXX 	 * Notification from the construction window to display a track piece at a given position.
// XXX 	 * @param instance Ride number of the window.
// XXX 	 * @param piece Track piece to display.
// XXX 	 * @param vox Position of the piece.
// XXX 	 * @param direction Direction of building (to use with a cursor).
// XXX 	 */
// XXX 	virtual void DisplayPiece(uint16 instance, ConstTrackPiecePtr piece, const XYZPoint16 &vox, TileEdge direction) const = 0;
// XXX 
// XXX 	CoasterBuildMode * const mode; ///< Coaster build mouse mode.
// XXX };
// XXX 
// XXX /** State when no coaster construction window is opened. */
// XXX class CoasterBuildStateOff : public CoasterBuildState {
// XXX public:
// XXX 	/**
// XXX 	 * Constructor of the #CoasterBuildStateOff state.
// XXX 	 * @param mode Builder mode.
// XXX 	 */
// XXX 	CoasterBuildStateOff(CoasterBuildMode &mode) : CoasterBuildState(mode)
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OpenWindow(uint16 instance) const override
// XXX 	{
// XXX 		this->mode->instance = instance;
// XXX 		this->mode->SetNoPiece();
// XXX 		this->mode->DisableDisplay();
// XXX 		this->mode->SetState(BS_STARTING);
// XXX 	}
// XXX 
// XXX 	void CloseWindow(uint16 instance) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	bool MayActivateMode() const override
// XXX 	{
// XXX 		return false;
// XXX 	}
// XXX 
// XXX 	void ActivateMode(const Point16 &pos) const override
// XXX 	{
// XXX 		NOT_REACHED(); // Should never happen.
// XXX 	}
// XXX 
// XXX 	void OnMouseMoveEvent(Viewport *vp, const Point16 &old_pos, const Point16 &pos) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OnMouseButtonEvent(Viewport *vp, uint8 state) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void LeaveMode() const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	bool EnableCursors() const override
// XXX 	{
// XXX 		return false;
// XXX 	}
// XXX 
// XXX 	void ShowNoPiece(uint16 instance) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void SelectPosition(uint16 instance, ConstTrackPiecePtr piece, TileEdge direction) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void DisplayPiece(uint16 instance, ConstTrackPiecePtr piece, const XYZPoint16 &vox, TileEdge direction) const override
// XXX 	{
// XXX 	}
// XXX };
// XXX 
// XXX /** State with opened window, but no active mouse mode yet. */
// XXX class CoasterBuildStateStarting : public CoasterBuildState {
// XXX public:
// XXX 	/**
// XXX 	 * Constructor of the #CoasterBuildStateStarting state.
// XXX 	 * @param mode Builder mode.
// XXX 	 */
// XXX 	CoasterBuildStateStarting(CoasterBuildMode &mode) : CoasterBuildState(mode)
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OpenWindow(uint16 instance) const override
// XXX 	{
// XXX 		this->mode->instance = instance; // Nothing has happened yet, so switching can still be done.
// XXX 	}
// XXX 
// XXX 	void CloseWindow(uint16 instance) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 
// XXX 		this->mode->instance = INVALID_RIDE_INSTANCE;
// XXX 		this->mode->SetState(BS_OFF);
// XXX 	}
// XXX 
// XXX 	bool MayActivateMode() const override
// XXX 	{
// XXX 		return true;
// XXX 	}
// XXX 
// XXX 	void ActivateMode(const Point16 &pos) const override
// XXX 	{
// XXX 		this->mode->EnableDisplay();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		BuilderState new_state;
// XXX 		if (this->mode->cur_piece == nullptr) {
// XXX 			new_state = BS_ON;
// XXX 		} else if (this->mode->use_mousepos) {
// XXX 			new_state = BS_MOUSE;
// XXX 		} else {
// XXX 			new_state = BS_FIXED;
// XXX 		}
// XXX 		this->mode->SetState(new_state);
// XXX 	}
// XXX 
// XXX 	void OnMouseMoveEvent(Viewport *vp, const Point16 &old_pos, const Point16 &pos) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OnMouseButtonEvent(Viewport *vp, uint8 state) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void LeaveMode() const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	bool EnableCursors() const override
// XXX 	{
// XXX 		return false;
// XXX 	}
// XXX 
// XXX 	void ShowNoPiece(uint16 instance) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetNoPiece();
// XXX 	}
// XXX 
// XXX 	void SelectPosition(uint16 instance, ConstTrackPiecePtr piece, TileEdge direction) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetSelectPosition(piece, direction);
// XXX 	}
// XXX 
// XXX 	void DisplayPiece(uint16 instance, ConstTrackPiecePtr piece, const XYZPoint16 &vox, TileEdge direction) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetFixedPiece(piece, vox, direction);
// XXX 	}
// XXX };
// XXX 
// XXX /** State for display a not-available track piece, or suppressing display of an available track piece. */
// XXX class CoasterBuildStateOn : public CoasterBuildState {
// XXX public:
// XXX 	/**
// XXX 	 * Constructor of the #CoasterBuildStateOn state.
// XXX 	 * @param mode Builder mode.
// XXX 	 */
// XXX 	CoasterBuildStateOn(CoasterBuildMode &mode) : CoasterBuildState(mode)
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OpenWindow(uint16 instance) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void CloseWindow(uint16 instance) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->DisableDisplay();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_STARTING);
// XXX 		_mouse_modes.SetViewportMousemode(); // Select another mouse mode.
// XXX 	}
// XXX 
// XXX 	bool MayActivateMode() const override
// XXX 	{
// XXX 		return true; // But is already enabled, doesn't matter.
// XXX 	}
// XXX 
// XXX 	void ActivateMode(const Point16 &pos) const override
// XXX 	{
// XXX 		this->mode->SetMousePosition(pos);
// XXX 	}
// XXX 
// XXX 	void OnMouseMoveEvent(Viewport *vp, const Point16 &old_pos, const Point16 &pos) const override
// XXX 	{
// XXX 		this->mode->SetMousePosition(pos); // Nothing is displayed.
// XXX 	}
// XXX 
// XXX 	void OnMouseButtonEvent(Viewport *vp, uint8 state) const override
// XXX 	{
// XXX 		/* Nothing is displayed. */
// XXX 	}
// XXX 
// XXX 	void LeaveMode() const override
// XXX 	{
// XXX 		this->mode->DisableDisplay();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_STARTING);
// XXX 	}
// XXX 
// XXX 	bool EnableCursors() const override
// XXX 	{
// XXX 		return false;
// XXX 	}
// XXX 
// XXX 	void ShowNoPiece(uint16 instance) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetNoPiece();
// XXX 	}
// XXX 
// XXX 	void SelectPosition(uint16 instance, ConstTrackPiecePtr piece, TileEdge direction) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetSelectPosition(piece, direction);
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_MOUSE);
// XXX 	}
// XXX 
// XXX 	void DisplayPiece(uint16 instance, ConstTrackPiecePtr piece, const XYZPoint16 &vox, TileEdge direction) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetFixedPiece(piece, vox, direction);
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_FIXED);
// XXX 	}
// XXX };
// XXX 
// XXX /** State to display a track piece at the position of the mouse (at the ground). */
// XXX class CoasterBuildStateMouse : public CoasterBuildState {
// XXX public:
// XXX 	/**
// XXX 	 * Constructor of the #CoasterBuildStateMouse state.
// XXX 	 * @param mode Builder mode.
// XXX 	 */
// XXX 	CoasterBuildStateMouse(CoasterBuildMode &mode) : CoasterBuildState(mode)
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OpenWindow(uint16 instance) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void CloseWindow(uint16 instance) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->DisableDisplay();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_DOWN);
// XXX 		_mouse_modes.SetViewportMousemode(); // Select another mouse mode.
// XXX 	}
// XXX 
// XXX 	bool MayActivateMode() const override
// XXX 	{
// XXX 		return true;
// XXX 	}
// XXX 
// XXX 	void ActivateMode(const Point16 &pos) const override
// XXX 	{
// XXX 		this->mode->SetMousePosition(pos);
// XXX 		this->mode->UpdateDisplay(true);
// XXX 	}
// XXX 
// XXX 	void OnMouseMoveEvent(Viewport *vp, const Point16 &old_pos, const Point16 &pos) const override
// XXX 	{
// XXX 		this->mode->SetMousePosition(pos);
// XXX 		this->mode->UpdateDisplay(true);
// XXX 	}
// XXX 
// XXX 	void OnMouseButtonEvent(Viewport *vp, uint8 state) const override
// XXX 	{
// XXX 		PositionedTrackPiece ptp(this->mode->track_pos, this->mode->cur_piece);
// XXX 		if (ptp.CanBePlaced()) {
// XXX 			this->mode->SetNoPiece();
// XXX 			this->mode->UpdateDisplay(false);
// XXX 			NotifyChange(WC_COASTER_BUILD, this->mode->instance, CHG_PIECE_POSITIONED, 0);
// XXX 			this->mode->SetState(BS_ON);
// XXX 		}
// XXX 	}
// XXX 
// XXX 	void LeaveMode() const override
// XXX 	{
// XXX 		this->mode->DisableDisplay();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_STARTING);
// XXX 	}
// XXX 
// XXX 	bool EnableCursors() const override
// XXX 	{
// XXX 		return true;
// XXX 	}
// XXX 
// XXX 	void ShowNoPiece(uint16 instance) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetNoPiece();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_ON);
// XXX 	}
// XXX 
// XXX 	void SelectPosition(uint16 instance, ConstTrackPiecePtr piece, TileEdge direction) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetSelectPosition(piece, direction);
// XXX 		this->mode->UpdateDisplay(false);
// XXX 	}
// XXX 
// XXX 	void DisplayPiece(uint16 instance, ConstTrackPiecePtr piece, const XYZPoint16 &vox, TileEdge direction) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetFixedPiece(piece, vox, direction);
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_FIXED);
// XXX 	}
// XXX };
// XXX 
// XXX /** Display a track piece at a fixed position in the world. */
// XXX class CoasterBuildStateFixed : public CoasterBuildState {
// XXX public:
// XXX 	/**
// XXX 	 * Constructor of the #CoasterBuildStateFixed state.
// XXX 	 * @param mode Builder mode.
// XXX 	 */
// XXX 	CoasterBuildStateFixed(CoasterBuildMode &mode) : CoasterBuildState(mode)
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OpenWindow(uint16 instance) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void CloseWindow(uint16 instance) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->DisableDisplay();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_DOWN);
// XXX 		_mouse_modes.SetViewportMousemode(); // Select another mouse mode.
// XXX 	}
// XXX 
// XXX 	bool MayActivateMode() const override
// XXX 	{
// XXX 		return true; // But is already enabled, doesn't matter.
// XXX 	}
// XXX 
// XXX 	void ActivateMode(const Point16 &pos) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OnMouseMoveEvent(Viewport *vp, const Point16 &old_pos, const Point16 &pos) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OnMouseButtonEvent(Viewport *vp, uint8 state) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void LeaveMode() const override
// XXX 	{
// XXX 		this->mode->DisableDisplay();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_STARTING);
// XXX 	}
// XXX 
// XXX 	bool EnableCursors() const override
// XXX 	{
// XXX 		return true;
// XXX 	}
// XXX 
// XXX 	void ShowNoPiece(uint16 instance) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetNoPiece();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_ON);
// XXX 	}
// XXX 
// XXX 	void SelectPosition(uint16 instance, ConstTrackPiecePtr piece, TileEdge direction) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetSelectPosition(piece, direction);
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_MOUSE);
// XXX 	}
// XXX 
// XXX 	void DisplayPiece(uint16 instance, ConstTrackPiecePtr piece, const XYZPoint16 &vox, TileEdge direction) const override
// XXX 	{
// XXX 		if (this->mode->instance != instance) return;
// XXX 		this->mode->SetFixedPiece(piece, vox, direction);
// XXX 		this->mode->UpdateDisplay(false);
// XXX 	}
// XXX };
// XXX 
// XXX /** Stopping state, mouse mode wants to deactivate, but has to wait until it gets permission to do so. */
// XXX class CoasterBuildStateStopping : public CoasterBuildState {
// XXX public:
// XXX 	/**
// XXX 	 * Constructor of the #CoasterBuildStateStopping state.
// XXX 	 * @param mode Builder mode.
// XXX 	 */
// XXX 	CoasterBuildStateStopping(CoasterBuildMode &mode) : CoasterBuildState(mode)
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OpenWindow(uint16 instance) const override
// XXX 	{
// XXX 		this->mode->instance = instance;
// XXX 		this->mode->SetNoPiece();
// XXX 		this->mode->EnableDisplay();
// XXX 		this->mode->UpdateDisplay(false);
// XXX 		this->mode->SetState(BS_ON);
// XXX 	}
// XXX 
// XXX 	void CloseWindow(uint16 instance) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	bool MayActivateMode() const override
// XXX 	{
// XXX 		return false;
// XXX 	}
// XXX 
// XXX 	void ActivateMode(const Point16 &pos) const override
// XXX 	{
// XXX 		this->mode->SetMousePosition(pos);
// XXX 	}
// XXX 
// XXX 	void OnMouseMoveEvent(Viewport *vp, const Point16 &old_pos, const Point16 &pos) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void OnMouseButtonEvent(Viewport *vp, uint8 state) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void LeaveMode() const override
// XXX 	{
// XXX 		this->mode->SetState(BS_OFF);
// XXX 	}
// XXX 
// XXX 	bool EnableCursors() const override
// XXX 	{
// XXX 		return false;
// XXX 	}
// XXX 
// XXX 	void ShowNoPiece(uint16 instance) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void SelectPosition(uint16 instance, ConstTrackPiecePtr piece, TileEdge direction) const override
// XXX 	{
// XXX 	}
// XXX 
// XXX 	void DisplayPiece(uint16 instance, ConstTrackPiecePtr piece, const XYZPoint16 &vox, TileEdge direction) const override
// XXX 	{
// XXX 	}
// XXX };

// XXX static const CoasterBuildStateOff      _coaster_build_state_off(_coaster_builder);      ///< Off state of the coaster build mouse mode.
// XXX static const CoasterBuildStateStarting _coaster_build_state_starting(_coaster_builder); ///< Starting state of the coaster build mouse mode.
// XXX static const CoasterBuildStateOn       _coaster_build_state_on(_coaster_builder);       ///< On state of the coaster build mouse mode.
// XXX static const CoasterBuildStateMouse    _coaster_build_state_mouse(_coaster_builder);    ///< Select position state of the coaster build mouse mode.
// XXX static const CoasterBuildStateFixed    _coaster_build_state_fixed(_coaster_builder);    ///< Show fixed position state of the coaster build mouse mode.
// XXX static const CoasterBuildStateStopping _coaster_build_state_stopping(_coaster_builder); ///< Stopping state of the coaster build mouse mode.

/** Available states of the coaster build mouse mode. */
// XXX static const CoasterBuildState *_coaster_build_states[] = {
// XXX 	&_coaster_build_state_off,      // BS_OFF
// XXX 	&_coaster_build_state_starting, // BS_STARTING
// XXX 	&_coaster_build_state_on,       // BS_ON
// XXX 	&_coaster_build_state_mouse,    // BS_MOUSE
// XXX 	&_coaster_build_state_fixed,    // BS_FIXED
// XXX 	&_coaster_build_state_stopping, // BS_DOWN
// XXX };

// XXX CoasterBuildMode::CoasterBuildMode() : MouseMode(WC_COASTER_BUILD, MM_COASTER_BUILD)
// XXX {
// XXX 	this->instance = INVALID_RIDE_INSTANCE;
// XXX 	this->state = BS_OFF;
// XXX 	this->mouse_state = 0;
// XXX 	this->SetNoPiece();
// XXX }
// XXX 
// XXX /**
// XXX  * Notification to the mouse mode that a coaster construction window has been opened.
// XXX  * @param instance Ride number of the window.
// XXX  */
// XXX void CoasterBuildMode::OpenWindow(uint16 instance)
// XXX {
// XXX 	_coaster_build_states[this->state]->OpenWindow(instance);
// XXX }
// XXX 
// XXX /**
// XXX  * Notification to the mouse mode that a coaster construction window has been closed.
// XXX  * @param instance Ride number of the window.
// XXX  */
// XXX void CoasterBuildMode::CloseWindow(uint16 instance)
// XXX {
// XXX 	_coaster_build_states[this->state]->CloseWindow(instance);
// XXX }
// XXX 
// XXX /**
// XXX  * Notification from the construction window to display no track piece.
// XXX  * @param instance Ride number of the window.
// XXX  */
// XXX void CoasterBuildMode::ShowNoPiece(uint16 instance)
// XXX {
// XXX 	_coaster_build_states[this->state]->ShowNoPiece(instance);
// XXX }
// XXX 
// XXX /**
// XXX  * Notification from the construction window to display a track piece attached to the mouse cursor.
// XXX  * @param instance Ride number of the window.
// XXX  * @param piece Track piece to display.
// XXX  * @param direction Direction of building (to use with a cursor).
// XXX  */
// XXX void CoasterBuildMode::SelectPosition(uint16 instance, ConstTrackPiecePtr piece, TileEdge direction)
// XXX {
// XXX 	_coaster_build_states[this->state]->SelectPosition(instance, piece, direction);
// XXX }
// XXX 
// XXX /**
// XXX  * Notification from the construction window to display a track piece at a given position.
// XXX  * @param instance Ride number of the window.
// XXX  * @param piece Track piece to display.
// XXX  * @param vox Position of the piece.
// XXX  * @param direction Direction of building (to use with a cursor).
// XXX  */
// XXX void CoasterBuildMode::DisplayPiece(uint16 instance, ConstTrackPiecePtr piece, const XYZPoint16 &vox, TileEdge direction)
// XXX {
// XXX 	_coaster_build_states[this->state]->DisplayPiece(instance, piece, vox, direction);
// XXX }
// XXX 
// XXX /**
// XXX  * Query from the viewport whether the mouse mode may be activated.
// XXX  * @return The mouse mode may be activated.
// XXX  * @see MouseMode::MayActivateMode
// XXX  */
// XXX bool CoasterBuildMode::MayActivateMode()
// XXX {
// XXX 	return _coaster_build_states[this->state]->MayActivateMode();
// XXX }
// XXX 
// XXX /**
// XXX  * Notification from the viewport that the mouse mode has been activated.
// XXX  * @param pos Current mouse position.
// XXX  * @see MouseMode::ActivateMode
// XXX  */
// XXX void CoasterBuildMode::ActivateMode(const Point16 &pos)
// XXX {
// XXX 	_coaster_build_states[this->state]->ActivateMode(pos);
// XXX }
// XXX 
// XXX /**
// XXX  * Notification from the viewport that the mouse mode has been de-activated.
// XXX  * @see MouseMode::LeaveMode
// XXX  */
// XXX void CoasterBuildMode::LeaveMode()
// XXX {
// XXX 	_coaster_build_states[this->state]->LeaveMode();
// XXX }
// XXX 
// XXX /**
// XXX  * Notification from the viewport that the mouse has moved.
// XXX  * @param vp The viewport.
// XXX  * @param old_pos Previous position of the mouse.
// XXX  * @param pos Current mouse position.
// XXX  * @see MouseMode::OnMouseMoveEvent
// XXX  */
// XXX void CoasterBuildMode::OnMouseMoveEvent(Viewport *vp, const Point16 &old_pos, const Point16 &pos)
// XXX {
// XXX 	if ((this->mouse_state & MB_RIGHT) != 0) {
// XXX 		/* Drag the window if button is pressed down. */
// XXX 		Viewport *vp = GetViewport();
// XXX 		if (vp != nullptr) vp->MoveViewport(pos.x - old_pos.x, pos.y - old_pos.y);
// XXX 	}
// XXX 	_coaster_build_states[this->state]->OnMouseMoveEvent(vp, old_pos, pos);
// XXX }
// XXX 
// XXX /**
// XXX  * Notification from the viewport that a mouse button has changed value.
// XXX  * @param vp The viewport.
// XXX  * @param state Old and new state of the mouse buttons.
// XXX  * @see MouseMode::OnMouseButtonEvent
// XXX  */
// XXX void CoasterBuildMode::OnMouseButtonEvent(Viewport *vp, uint8 state)
// XXX {
// XXX 	this->mouse_state = state & MB_CURRENT;
// XXX 	_coaster_build_states[this->state]->OnMouseButtonEvent(vp, state);
// XXX }
// XXX 
// XXX /**
// XXX  * Query from the viewport whether the mouse mode wants to have cursors displayed.
// XXX  * @return Cursors should be enabled.
// XXX  * @see MouseMode::EnableCursors
// XXX  */
// XXX bool CoasterBuildMode::EnableCursors()
// XXX {
// XXX 	return _coaster_build_states[this->state]->EnableCursors();
// XXX }
// XXX 
// XXX /**
// XXX  * Update the displayed position.
// XXX  * @param mousepos_changed If set, check the mouse position to x/y/z again (and only update the display if it is different).
// XXX  */
// XXX void CoasterBuildMode::UpdateDisplay(bool mousepos_changed)
// XXX {
// XXX 	if (this->suppress_display || this->cur_piece == nullptr) {
// XXX 		DisableWorldAdditions();
// XXX 		return;
// XXX 	}
// XXX 
// XXX 	Viewport *vp = GetViewport();
// XXX 	if (use_mousepos) {
// XXX 		FinderData fdata(CS_GROUND, FW_TILE);
// XXX 		if (vp->ComputeCursorPosition(&fdata) != CS_GROUND) {
// XXX 			DisableWorldAdditions();
// XXX 			return;
// XXX 		}
// XXX 		/* Found ground, is the position the same? */
// XXX 		if (mousepos_changed && fdata.voxel_pos == this->track_pos) {
// XXX 			return;
// XXX 		}
// XXX 
// XXX 		this->track_pos = fdata.voxel_pos;
// XXX 	}
// XXX 	_additions.Clear();
// XXX 	EnableWorldAdditions();
// XXX 	PositionedTrackPiece ptp(this->track_pos, this->cur_piece);
// XXX 	CoasterInstance *ci = static_cast<CoasterInstance *>(_rides_manager.GetRideInstance(this->instance));
// XXX 	if (ptp.CanBePlaced()) ci->PlaceTrackPieceInAdditions(ptp);
// XXX 	vp->arrow_cursor.SetCursor(this->track_pos, (CursorType)(CUR_TYPE_ARROW_NE + this->direction));
// XXX }
