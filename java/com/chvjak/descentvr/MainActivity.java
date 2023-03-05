// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.chvjak.descentvr;

import android.content.Context;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.util.Log;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.IOException;

/**
 * When using NativeActivity, we currently need to handle loading of dependent shared libraries
 * manually before a shared library that depends on them is loaded, since there is not currently a
 * way to specify a shared library dependency for NativeActivity via the manifest meta-data.
 *
 * <p>The simplest method for doing so is to subclass NativeActivity with an empty activity that
 * calls System.loadLibrary on the dependent libraries, which is unfortunate when the goal is to
 * write a pure native C/C++ only Android activity.
 *
 * <p>A native-code only solution is to load the dependent libraries dynamically using dlopen().
 * However, there are a few considerations, see:
 * https://groups.google.com/forum/#!msg/android-ndk/l2E2qh17Q6I/wj6s_6HSjaYJ
 *
 * <p>1. Only call dlopen() if you're sure it will succeed as the bionic dynamic linker will
 * remember if dlopen failed and will not re-try a dlopen on the same lib a second time.
 *
 * <p>2. Must remember what libraries have already been loaded to avoid infinitely looping when
 * libraries have circular dependencies.
 */
public class MainActivity extends android.app.NativeActivity {
  static {
    System.loadLibrary("vrapi");
    System.loadLibrary("descentvr");
  }

  private final static  MediaPlayer mediaPlayer = new MediaPlayer();
  @SuppressWarnings("unused")
  private void playMidi(String path, boolean looping) {
    File file = new File(path);
    Log.d("MIDI", " file: " +  file.toString());
    FileInputStream fos = null;
    FileDescriptor fd = null;

    try {
      fos = new FileInputStream(file);
      fd = fos.getFD();
      Log.d("MIDI", "FileInputStream: " + fos.toString());
      Log.d("MIDI", "FileDescriptor: " + fd.toString());
      mediaPlayer.setDataSource(fd);
      mediaPlayer.prepare();
    } catch (IOException e) {
      e.printStackTrace();
    } finally {
      if (fos != null) {
        try {
          fos.close();
        } catch (IOException e) {
          e.printStackTrace();
        }
      }
    }
    mediaPlayer.setLooping(looping);
    mediaPlayer.start();
  }


  @SuppressWarnings("unused")
  private void stopMidi() {
    mediaPlayer.stop();
    mediaPlayer.reset();
  }

  @SuppressWarnings("unused")
  private void setMidiVolume(float volume) {
    mediaPlayer.setVolume(volume, volume);
  }


  private void checkMidiPlayback() {
    AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
    if (audioManager == null) {
      Log.d("MIDI", "AudioManager not available.");
      return;
    }

    MidiManager midiManager = (MidiManager) getSystemService(Context.MIDI_SERVICE);
    if (midiManager == null) {
      Log.d("MIDI", "MidiManager not available.");
      return;
    }

    MidiDeviceInfo[] infos = midiManager.getDevices();
    if (infos.length == 0) {
      Log.d("MIDI", "No MIDI devices found.");
      return;
    }

    for (MidiDeviceInfo info : infos) {
      Log.d("MIDI", "MIDI device found: " + info.toString());
    }
  }

}
