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


#include "base/plugins.h"

#include "engines/advancedDetector.h"

static const PlainGameDescriptor wageGames[] = {
	{"afm", "Another Fine Mess"},
	{"amot", "A Mess O' Trouble"},
	{"cantitoe", "Camp Cantitoe"},
	{"drakmythcastle", "Drakmyth Castle"},
	{"grailquest", "GrailQuest: Adventure in the Age of King Arthur"},
	{"raysmaze", "Ray's Maze"},
	{"scepters", "Enchanted Scepters"},
	{"twisted", "Twisted!"},
	{"wage", "WAGE"},
	{0, 0}
};

#include "wage/detection_tables.h"
#include "wage/detection.h"

class WageMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	WageMetaEngineDetection() : AdvancedMetaEngineDetection(Wage::gameDescriptions, wageGames) {
		_md5Bytes = 2 * 1024 * 1024;
		_guiOptions = GUIO3(GUIO_NOSPEECH, GUIO_NOMIDI, GAMEOPTION_TTS);
	}

	const char *getName() const override {
		return "wage";
	}

	const char *getEngineName() const override {
		return "World Adventure Game Engine";
	}

	const char *getOriginalCopyright() const override {
		return "World Builder (C) Silicon Beach Software";
	}
};

REGISTER_PLUGIN_STATIC(WAGE_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, WageMetaEngineDetection);
