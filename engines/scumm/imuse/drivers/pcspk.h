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

#ifndef SCUMM_IMUSE_PCSPK_H
#define SCUMM_IMUSE_PCSPK_H

#include "audio/softsynth/emumidi.h"
#include "audio/softsynth/pcspk.h"

namespace Scumm {

class IMuseDriver_PCSpk : public MidiDriver_Emulated {
public:
	IMuseDriver_PCSpk(Audio::Mixer *mixer);
	~IMuseDriver_PCSpk() override;

	int open() override;
	void close() override;

	void send(uint32 d) override;

	MidiChannel *allocateChannel() override;
	MidiChannel *getPercussionChannel() override { return nullptr; }

	bool isStereo() const override { return _pcSpk.isStereo(); }
	int getRate() const override { return _pcSpk.getRate(); }
protected:
	void generateSamples(int16 *buf, int len) override;
	void onTimer() override;

private:
	Audio::PCSpeakerStream _pcSpk;
	int _effectTimer;
	uint8 _randBase;

	void updateNote();
	void output(uint16 out);

	static uint8 getEffectModifier(uint16 level);
	int16 getEffectModLevel(int16 level, int8 mod);
	int16 getRandScale(int16 input);

	struct EffectEnvelope {
		uint8 state;
		int16 currentLevel;
		int16 duration;
		int16 maxLevel;
		int16 startLevel;
		uint8 loop;
		uint8 stateTargetLevels[4];
		uint8 stateModWheelLevels[4];
		uint8 modWheelSensitivity;
		uint8 modWheelState;
		uint8 modWheelLast;
		int16 stateNumSteps;
		int16 stateStepCounter;
		int16 changePerStep;
		int8 dir;
		int16 changePerStepRem;
		int16 changeCountRem;
	};

	struct EffectDefinition {
		int16 phase;
		uint8 type;
		uint8 useModWheel;
		EffectEnvelope *envelope;
	};

	struct OutputChannel {
		uint8 active;
		uint8 note;
		uint8 sustainNoteOff;
		uint8 length;
		const uint8 *instrument;
		uint8 unkA;
		uint8 unkB;
		uint8 unkC;
		int16 unkE;
		EffectEnvelope effectEnvelopeA;
		EffectDefinition effectDefA;
		EffectEnvelope effectEnvelopeB;
		EffectDefinition effectDefB;
		int16 unk60;
	};

	class MidiChannel_PcSpk: public MidiChannel {
	public:
		MidiChannel_PcSpk(IMuseDriver_PCSpk *owner, byte number);
		MidiDriver *device() override { return _owner; }
		byte getNumber() override { return _number; }
		void release() override;

		void send(uint32 b) override;
		void noteOff(byte note) override;
		void noteOn(byte note, byte velocity) override;
		void programChange(byte program) override;
		void pitchBend(int16 bend) override;
		void controlChange(byte control, byte value) override;
		void pitchBendFactor(byte value) override;
		void transpose(int8 value) override;
		void detune(int16 value) override;
		void priority(byte value) override;
		void sysEx_customInstrument(uint32 type, const byte *instr, uint32 dataSize) override;

		bool allocate();

		bool _allocated;
		OutputChannel _out;
		uint8 _instrument[23];
		uint8 _priority;
		uint8 _tl;
		uint8 _modWheel;
		int16 _pitchBend;

	private:
		IMuseDriver_PCSpk *_owner;
		const byte _number;
		//uint8 _programNr;
		uint8 _sustain;
		uint8 _pitchBendFactor;
		int16 _pitchBendTmp;
		int8 _transpose;
		int8 _detune;
	};

	void setupEffects(MidiChannel_PcSpk &chan, EffectEnvelope &env, EffectDefinition &def, byte flags, const byte *data);
	void startEffect(EffectEnvelope &env, const byte *data);
	void initNextEnvelopeState(EffectEnvelope &env);
	void updateEffectGenerator(MidiChannel_PcSpk &chan, EffectEnvelope &env, EffectDefinition &def);
	uint8 advanceEffectEnvelope(EffectEnvelope &env, EffectDefinition &def);

	MidiChannel_PcSpk *_channels[6];
	MidiChannel_PcSpk *_activeChannel;

	MidiChannel_PcSpk *_lastActiveChannel;
	uint16 _lastActiveOut;

	static const byte _outInstrumentData[1024];
	static const byte _outputTable1[];
	static const byte _outputTable2[];
	static const uint16 _effectEnvStepTable[];
	static const uint16 _frequencyTable[];
};

} // End of namespace Scumm

#endif
