//-------------------------------------------------------------------------------------------------------
// VST Plug-Ins SDK
// Version 2.4		$Date: 2006/11/13 09:08:27 $
//
// Category     : VST 2.x SDK Samples
// Filename     : vstxlennnnnnnnnnnnnnn.cpp
// Created by   : Steinberg Media Technologies
// Description  : Example VstXSynth
//
// A simple 2 oscillators test 'synth',
// Each oscillator has waveform, frequency, and volume
//
// *very* basic monophonic 'synth' example. you should not attempt to use this
// example 'algorithm' to start a serious virtual instrument; it is intended to demonstrate
// how VstEvents ('MIDI') are handled, but not how a virtual analog synth works.
// there are numerous much better examples on the web which show how to deal with
// bandlimited waveforms etc.
//
// © 2006, Steinberg Media Technologies, All Rights Reserved
//-------------------------------------------------------------------------------------------------------

#include "vstxsynth.h"

enum
{
	kNumFrequencies = 128,	// 128 midi notes
	kWaveSize = 4096		// samples (must be power of 2 here)
};

const double midiScaler = (1. / 127.);
static float sawtooth[kWaveSize];
static float pulse[kWaveSize];
static float freqtab[kNumFrequencies];

//-----------------------------------------------------------------------------------------
// VstXSynth
//-----------------------------------------------------------------------------------------
void VstXSynth::setSampleRate (float sampleRate)
{
	AudioEffectX::setSampleRate (sampleRate);
	fScaler = (float)((double)kWaveSize / (double)sampleRate);
}

//-----------------------------------------------------------------------------------------
void VstXSynth::setBlockSize (VstInt32 blockSize)
{
	AudioEffectX::setBlockSize (blockSize);
	// you may need to have to do something here...
}

//-----------------------------------------------------------------------------------------
void VstXSynth::initProcess ()
{
	msgArrayLen = 11;
	int j;
	
	lightSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (-1 == lightSocket) /* if socket failed to initialize, exit */
    {
		printf("Error Creating Socket");
		exit(EXIT_FAILURE);
    }
	
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("192.168.3.2");
	sa.sin_port = htons(9909);
		
	for (j = 0; j < msgArrayLen; j++) {
		msgArray[j] = 0;
	}
	
	// Other redundant stuff
	
	fPhase1 = fPhase2 = 0.f;
	fScaler = (float)((double)kWaveSize / 44100.);	// we don't know the sample rate yet
	noteIsOn = false;
	currentDelta = currentNote = currentDelta = 0;
	VstInt32 i;

	// make waveforms
	VstInt32 wh = kWaveSize / 4;	// 1:3 pulse
	for (i = 0; i < kWaveSize; i++)
	{
		sawtooth[i] = (float)(-1. + (2. * ((double)i / (double)kWaveSize)));
		pulse[i] = (i < wh) ? -1.f : 1.f;
	}

	// make frequency (Hz) table
	double k = 1.059463094359;	// 12th root of 2
	double a = 6.875;	// a
	a *= k;	// b
	a *= k;	// bb
	a *= k;	// c, frequency of midi note 0
	for (i = 0; i < kNumFrequencies; i++)	// 128 midi notes
	{
		freqtab[i] = (float)a;
		a *= k;
	}
}

//-----------------------------------------------------------------------------------------
void VstXSynth::processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames)
{
	float* out1 = outputs[0];
	float* out2 = outputs[1];

	if (noteIsOn)
	{
		float baseFreq = freqtab[currentNote & 0x7f] * fScaler;
		float freq1 = baseFreq + fFreq1;	// not really linear...
		float freq2 = baseFreq + fFreq2;
		float* wave1 = (fWaveform1 < .5) ? sawtooth : pulse;
		float* wave2 = (fWaveform2 < .5) ? sawtooth : pulse;
		float wsf = (float)kWaveSize;
		float vol = (float)(fVolume * (double)currentVelocity * midiScaler);
		VstInt32 mask = kWaveSize - 1;
		
		if (currentDelta > 0)
		{
			if (currentDelta >= sampleFrames)	// future
			{
				currentDelta -= sampleFrames;
				return;
			}
			memset (out1, 0, currentDelta * sizeof (float));
			memset (out2, 0, currentDelta * sizeof (float));
			out1 += currentDelta;
			out2 += currentDelta;
			sampleFrames -= currentDelta;
			currentDelta = 0;
		}

		// loop
		while (--sampleFrames >= 0)
		{
			// this is all very raw, there is no means of interpolation,
			// and we will certainly get aliasing due to non-bandlimited
			// waveforms. don't use this for serious projects...
			(*out1++) = wave1[(VstInt32)fPhase1 & mask] * fVolume1 * vol;
			(*out2++) = wave2[(VstInt32)fPhase2 & mask] * fVolume2 * vol;
			fPhase1 += freq1;
			fPhase2 += freq2;
		}
	}						
	else
	{
		memset (out1, 0, sampleFrames * sizeof (float));
		memset (out2, 0, sampleFrames * sizeof (float));
	}
}

//-----------------------------------------------------------------------------------------
VstInt32 VstXSynth::processEvents (VstEvents* ev)
{
	for (VstInt32 i = 0; i < ev->numEvents; i++)
	{
		if ((ev->events[i])->type != kVstMidiType)
			continue;

		VstMidiEvent* event = (VstMidiEvent*)ev->events[i];
		char* midiData = event->midiData;
		VstInt32 status = midiData[0] & 0xf0;	// ignoring channel
		if (status == 0x90 || status == 0x80)	// we only look at notes
		{
			VstInt32 note = midiData[1] & 0x7f;
			VstInt32 velocity = midiData[2] & 0x7f;
			if (status == 0x80)
				velocity = 0;	// note off by velocity 0
			if (!velocity && (note == currentNote))
				noteOff (note);
			else
				noteOn (note, velocity, event->deltaFrames);
		}
		else if (status == 0xb0)
		{
			//if (midiData[1] == 0x7e || midiData[1] == 0x7b)	// all notes off
				// Nothing yet. TODO.
		}
		event++;
	}
	return 1;
}

//-----------------------------------------------------------------------------------------
void VstXSynth::noteOn (VstInt32 note, VstInt32 velocity, VstInt32 delta)
{
	uint8_t bright = velocity*2;
	setLight(note,bright);
}

//-----------------------------------------------------------------------------------------
void VstXSynth::noteOff (VstInt32 note)
{
	uint8_t bright = 0;
	setLight(note,bright);
}

void VstXSynth::setLight(VstInt32 note, uint8_t bright) {
	int r,g,b,h,s,v;
	uint8_t notes[] = {0,0,1,1,2,3,3,4,4,5,5,6,7,7,8,8,9,10,10,11,11,12,12,13}; // Half note mapping.
	
	// Some calculations to find out ranges.
	int range = note / 24;
	int octaveNote = (note % 24);
	
	uint8_t light = notes[octaveNote] + 32; // Map to whites.
		
	v=bright;
	
	switch (range) {
		case 2: // White
			s=0;
			h=0;
			break;
		case 0: // Yellow
			s=255;
			h=40;
			break;
		case 1: // Green
			s=255;
			h=90;
			break;
		case 3: // Purple
			s=255;
			h=225;
			break;
		case 4: // Red
			s=255;
			h=0;
			break;
		default:
			bright = 0; // Out of range.
			break;
	}
		
	// Colourspace conversion.
	HSVtoRGB( &r, &g, &b, h, s, v );
	
	// Set bytes to array and send it.
	msgArray[7] = light;
	msgArray[8] = r;
	msgArray[9] = g;
	msgArray[10] = b;
	
	int bytes_sent = sendto(lightSocket, msgArray, msgArrayLen, 0,(struct sockaddr*)&sa, sizeof sa);
	if (bytes_sent < 0) {
		printf("Error sending packet: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}