/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/scummsys.h"
#include "graphics/cursorman.h"
#include "common/events.h"
#include "common/endian.h"
#include "engines/util.h"
#include "access/access.h"
#include "access/events.h"
#include "access/player.h"
#include "access/amazon/amazon_resources.h"

#define CURSOR_WIDTH 16
#define CURSOR_HEIGHT 16

namespace Access {

EventsManager::EventsManager(AccessEngine *vm) : _vm(vm) {
	_cursorId = CURSOR_NONE;
	_normalMouse = CURSOR_CROSSHAIRS;
	_frameCounter = 10;
	_priorFrameTime = 0;
	_leftButton = _rightButton = false;
	_middleButton = false;
	_wheelUp = _wheelDown = false;
	_mouseCol = _mouseRow = 0;
	_cursorExitFlag = false;
	_vbCount = 0;
	_keyCode = Common::KEYCODE_INVALID;
	_priorTimerTime = 0;
}

EventsManager::~EventsManager() {
	_invCursor.free();
}

void EventsManager::forceSetCursor(CursorType cursorId) {
	setNormalCursor(cursorId);
	setCursor(cursorId);
}

void EventsManager::setNormalCursor(CursorType cursorId) {
	_normalMouse = cursorId;
}

void EventsManager::setCursor(CursorType cursorId) {
	if (cursorId == _cursorId)
		return;
	_cursorId = cursorId;

	if (cursorId == CURSOR_INVENTORY) {
		// Set the cursor
		CursorMan.replaceCursor(_invCursor, _invCursor.w / 2, _invCursor.h / 2, 0);
	} else {
		// Get a pointer to the mouse data to use, and get the cursor hotspot
		const byte *srcP = &_vm->_res->CURSORS[cursorId][0];
		int hotspotX = (int16)READ_LE_UINT16(srcP);
		int hotspotY = (int16)READ_LE_UINT16(srcP + 2);
		srcP += 4;

		// Create a surface to build up the cursor on
		Graphics::Surface cursorSurface;
		cursorSurface.create(CURSOR_WIDTH, CURSOR_HEIGHT, Graphics::PixelFormat::createFormatCLUT8());
		byte *destP = (byte *)cursorSurface.getPixels();
		Common::fill(destP, destP + CURSOR_WIDTH * CURSOR_HEIGHT, 0);

		// Loop to build up the cursor
		for (int y = 0; y < CURSOR_HEIGHT; ++y) {
			destP = (byte *)cursorSurface.getBasePtr(0, y);
			int width = CURSOR_WIDTH;
			int skip = *srcP++;
			int plot = *srcP++;
			if (skip >= width)
				break;

			// Skip over pixels
			destP += skip;
			width -= skip;

			// Write out the pixels to plot
			while (plot > 0 && width > 0) {
				*destP++ = *srcP++;
				--plot;
				--width;
			}
		}

		// Set the cursor
		CursorMan.replaceCursor(cursorSurface, hotspotX, hotspotY, 0);

		// Free the cursor surface
		cursorSurface.free();
	}
}

void EventsManager::setCursorData(Graphics::ManagedSurface *src, const Common::Rect &r) {
	_invCursor.create(r.width(), r.height(), Graphics::PixelFormat::createFormatCLUT8());
	_invCursor.copyRectToSurface(*src, 0, 0, r);
}

void EventsManager::showCursor() {
	CursorMan.showMouse(true);
}

void EventsManager::hideCursor() {
	CursorMan.showMouse(false);
}

bool EventsManager::isCursorVisible() {
	return CursorMan.isVisible();
}

void EventsManager::pollEvents(bool skipTimers) {
	if (checkForNextFrameCounter()) {
		nextFrame();
	}

	if (checkForNextTimerUpdate() && !skipTimers)
		nextTimer();

	_vm->_sound->checkSoundQueue();

	_wheelUp = _wheelDown = false;

	Common::Event event;
	while (g_system->getEventManager()->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_QUIT:
		case Common::EVENT_RETURN_TO_LAUNCHER:
			return;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
			actionControl(event.customType, true);
			return;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_END:
			actionControl(event.customType, false);
			return;
		case Common::EVENT_KEYDOWN:
			// Check for debugger
			keyControl(event.kbd.keycode, true);
			return;
		case Common::EVENT_KEYUP:
			keyControl(event.kbd.keycode, false);
			return;
		case Common::EVENT_MOUSEMOVE:
			_mousePos = event.mouse;
			_mouseCol = _mousePos.x / 8;
			_mouseRow = _mousePos.y / 8;
			break;
		case Common::EVENT_LBUTTONDOWN:
			_leftButton = true;
			return;
		case Common::EVENT_LBUTTONUP:
			_leftButton = false;
			return;
		case Common::EVENT_RBUTTONDOWN:
			_rightButton = true;
			return;
		case Common::EVENT_RBUTTONUP:
			_rightButton = false;
			return;
		case Common::EVENT_MBUTTONDOWN:
			_middleButton = true;
			return;
		case Common::EVENT_MBUTTONUP:
			_middleButton = false;
			return;
		case Common::EVENT_WHEELUP:
			_wheelUp = true;
			return;
		case Common::EVENT_WHEELDOWN:
			_wheelDown = true;
			return;
		default:
 			break;
		}
	}
}

void EventsManager::keyControl(Common::KeyCode keycode, bool isKeyDown) {
	Player &player = *_vm->_player;

	if (!isKeyDown) {
		if (player._move != NONE) {
			_keyCode = Common::KEYCODE_INVALID;
			player._move = NONE;
		}
		return;
	}

	_keyCode = keycode;
}

void EventsManager::actionControl(Common::CustomEventType action, bool isKeyDown) {
	Player &player = *_vm->_player;

	if (!isKeyDown) {
		if (player._move != NONE) {
			_action = kActionNone;
			player._move = NONE;
		}
		return;
	}

	_action = action;

	switch (action) {
	case kActionMoveUp:
		player._move = UP;
		break;
	case kActionMoveDown:
		player._move = DOWN;
		break;
	case kActionMoveLeft:
		player._move = LEFT;
		break;
	case kActionMoveRight:
		player._move = RIGHT;
		break;
	case kActionMoveUpLeft:
		player._move = UPLEFT;
		break;
	case kActionMoveUpRight:
		player._move = UPRIGHT;
		break;
	case kActionMoveDownLeft:
		player._move = DOWNLEFT;
		break;
	case kActionMoveDownRight:
		player._move = DOWNRIGHT;
		break;
	default:
		break;
	}
}

void EventsManager::pollEventsAndWait() {
	pollEvents();
	delay();
}

bool EventsManager::checkForNextFrameCounter() {
	// Check for next game frame
	uint32 milli = g_system->getMillis();
	if ((milli - _priorFrameTime) >= GAME_FRAME_TIME) {
		--_vbCount;
		++_frameCounter;
		_priorFrameTime = milli;

		return true;
	}

	return false;
}

bool EventsManager::checkForNextTimerUpdate() {
	// Check for next timer update
	uint32 milli = g_system->getMillis();
	if ((milli - _priorTimerTime) >= GAME_TIMER_TIME) {
		_priorTimerTime = milli;

		return true;
	}

	return false;
}

void EventsManager::nextFrame() {
	_vm->_screen->update();
}

void EventsManager::nextTimer() {
	_vm->_animation->updateTimers();
	_vm->_timers.updateTimers();
}

void EventsManager::delay(int time) {
	g_system->delayMillis(time);
}

void EventsManager::zeroKeysActions() {
	_keyCode = Common::KEYCODE_INVALID;
	_action = kActionNone;
}

bool EventsManager::getAction(Common::CustomEventType &action) {
	if (_action == kActionNone) {
		return false;
	} else {
		action = _action;
		_action = kActionNone;
		return true;
	}
}

bool EventsManager::isKeyActionPending() const {
	return (_keyCode != Common::KEYCODE_INVALID || _action != kActionNone);
}

void EventsManager::debounceLeft() {
	while (_leftButton && !_vm->shouldQuit()) {
		pollEventsAndWait();
	}
}

void EventsManager::clearEvents() {
	_leftButton = _rightButton = false;
	zeroKeysActions();
}

void EventsManager::waitKeyActionMouse() {
	while (!_vm->shouldQuit() && !isKeyActionMousePressed()) {
		pollEvents(true);
		delay();
	}
}

Common::Point EventsManager::calcRawMouse() {
	Common::Point pt;
	Screen &screen = *_vm->_screen;
	pt.x = _mousePos.x - screen._windowXAdd +
		(_vm->_scrollCol * TILE_WIDTH) + _vm->_scrollX;
	pt.y = _mousePos.y - screen._screenYOff - screen._windowYAdd +
		(_vm->_scrollRow * TILE_HEIGHT) + _vm->_scrollY;

	return pt;
}

int EventsManager::checkMouseBox1(Common::Array<Common::Rect> &rects) {
	for (uint16 i = 0; i < rects.size(); ++i) {
		if (rects[i].left == -1)
			return -1;

		if ((_mousePos.x > rects[i].left) && (_mousePos.x < rects[i].right)
			&& (_mousePos.y > rects[i].top) && (_mousePos.y < rects[i].bottom))
			return i;
	}

	return -1;
}

bool EventsManager::isKeyActionMousePressed() {
	bool result = _leftButton || _rightButton || isKeyActionPending();
	debounceLeft();
	zeroKeysActions();

	return result;
}

void EventsManager::centerMousePos() {
	_mousePos = Common::Point(160, 100);
}

void EventsManager::restrictMouse() {
	// No implementation in ScummVM
}

} // End of namespace Access
