/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2006 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

struct MidiDeviceStruct {
  AUGraph graph;
  AudioUnit synth;
  /* Note that is currently playing. */
  int note;
};
  
MidiDevice *
openMidiDevice (int errorLevel, const char *device) {
  MidiDevice *midi;
  int result;
  AUNode synthNode, outNode;
  ComponentDescription cd;
  UInt32 propVal;

  midi = mallocWrapper(sizeof(*midi));

  /* Create a graph with a software synth and a default output unit. */

  cd.componentManufacturer = kAudioUnitManufacturer_Apple;
  cd.componentFlags = 0;
  cd.componentFlagsMask = 0;

  if ((result = NewAUGraph(&midi->graph)) != noErr) {
    LogPrint(errorLevel, "Can't create audio graph component: %d", result);
    goto err;
  }

  cd.componentType = kAudioUnitType_MusicDevice;
  cd.componentSubType = kAudioUnitSubType_DLSSynth;
  if ((result = AUGraphNewNode(midi->graph, &cd, 0, NULL, &synthNode))
      != noErr) {
    LogPrint(errorLevel, "Can't create software synthersizer component: %d",
	     result);
    goto err;
  }

  cd.componentType = kAudioUnitType_Output;
  cd.componentSubType = kAudioUnitSubType_DefaultOutput;
  if ((result = AUGraphNewNode(midi->graph, &cd, 0, NULL, &outNode))
      != noErr) {
    LogPrint(errorLevel, "Can't create default output audio component: %d",
	     result);
    goto err;
  }

  if ((result = AUGraphOpen(midi->graph)) != noErr) {
    LogPrint(errorLevel, "Can't open audio graph component: %d", result);
    goto err;
  }

  if ((result = AUGraphConnectNodeInput(midi->graph, synthNode, 0, outNode, 0))
      != noErr) {
    LogPrint(errorLevel, "Can't connect synth audio component to output: %d",
	     result);
    goto err;
  }

  if ((result = AUGraphGetNodeInfo(midi->graph, synthNode, 0, 0, 0,
				   &midi->synth)) != noErr) {
    LogPrint(errorLevel, "Can't get audio component for software synth: %d",
	     result);
    goto err;
  }

  if ((result = AUGraphInitialize(midi->graph)) != noErr) {
    LogPrint(errorLevel, "Can't initialize audio graph: %d", result);
    goto err;
  }

  /* Turn off the reverb.  The value range is -120 to 40 dB. */
  propVal = false;
  if ((result = AudioUnitSetProperty(midi->synth,
				     kMusicDeviceProperty_UsesInternalReverb,
				     kAudioUnitScope_Global, 0,
				     &propVal, sizeof(propVal)))
      != noErr) {
    /* So, having reverb isn't that critical, is it? */
    LogPrint(LOG_DEBUG, "Can't turn of software synth reverb: %d",
	     result);
  }

  /* TODO: Maybe just start the graph when we are going to use it? */
  if ((result = AUGraphStart(midi->graph)) != noErr) {
    LogPrint(errorLevel, "Can't start audio graph component: %d", result);
    goto err;
  }

  return midi;

 err:
  if (midi->graph)
    DisposeAUGraph(midi->graph);
  free(midi);
  return NULL;
}

void
closeMidiDevice (MidiDevice *midi) {
  if (midi) {
    DisposeAUGraph(midi->graph);
    free(midi);
  }
}

int
flushMidiDevice (MidiDevice *midi) {
  return 1;
}

int
setMidiInstrument (MidiDevice *midi, unsigned char channel, unsigned char instrument) {
  MusicDeviceMIDIEvent(midi->synth, 0xC0 | channel, instrument, 0, 0);

  return 1;
}

int
beginMidiBlock (MidiDevice *midi) {
  return 1;
}

int
endMidiBlock (MidiDevice *midi) {
  return 1;
}

int
startMidiNote (MidiDevice *midi, unsigned char channel, unsigned char note, unsigned char volume) {
  MusicDeviceMIDIEvent(midi->synth, 0x90 | channel, note, volume, 0);
  midi->note = note;

  return 1;
}

int
stopMidiNote (MidiDevice *midi, unsigned char channel) {
  return startMidiNote(midi, channel, midi->note, 0);
}

int
insertMidiWait (MidiDevice *midi, int duration) {
  accurateDelay(duration);
  return 1;
}
