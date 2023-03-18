// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.chvjak.descentvr;

import android.Manifest;
import android.content.pm.PackageManager;
import android.nfc.Tag;
import android.os.Bundle;
import android.util.Log;

import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;

import org.billthefarmer.mididriver.MidiDriver;
import org.billthefarmer.mididriver.MidiConstants;
import org.billthefarmer.mididriver.GeneralMidiConstants;
import org.billthefarmer.mididriver.ReverbConstants;

import java.io.File;
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
  static MidiDriver midiDriver;
  static {
    System.loadLibrary("vrapi");
    System.loadLibrary("descentvr");
  }
  @Override protected void onCreate( Bundle icicle ) {
    super.onCreate(icicle);

    checkPermissionsAndInitialize();

    midiDriver = MidiDriver.getInstance();
  }

  private static final int READ_EXTERNAL_STORAGE_PERMISSION_ID = 1;
  private static final int WRITE_EXTERNAL_STORAGE_PERMISSION_ID = 2;
  private void checkPermissionsAndInitialize() {
    // Boilerplate for checking runtime permissions in Android.
    if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED){
      ActivityCompat.requestPermissions(
              this,
              new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE},
              WRITE_EXTERNAL_STORAGE_PERMISSION_ID);
    }

    if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED)
    {
      ActivityCompat.requestPermissions(
              this,
              new String[] {Manifest.permission.READ_EXTERNAL_STORAGE},
              READ_EXTERNAL_STORAGE_PERMISSION_ID);
    }
  }

  /** Handles the user accepting the permission. */
  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
    int perms = 0;
    if (requestCode == READ_EXTERNAL_STORAGE_PERMISSION_ID) {
      if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
        System.exit(0);
      }
      else
        perms++;
    }

    if (requestCode == WRITE_EXTERNAL_STORAGE_PERMISSION_ID) {
      if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
        System.exit(0);
      }
      else
        perms++;
    }

    if(perms != 2)
      checkPermissionsAndInitialize();
  }

  public void PlayMidi(String midiPathName, boolean looping)
  {
    midiDriver.start();

    // Program change - harpsichord
    sendMidi(MidiConstants.PROGRAM_CHANGE,
            GeneralMidiConstants.HARPSICHORD);

    int config[] = midiDriver.config();

    midiDriver.setVolume(100);
    Log.d("MIDI:", "Starting Playing pattern");
    for (int i =0; i < 100; i++) {
      sendMidi(MidiConstants.NOTE_ON, 48, 63);
      sendMidi(MidiConstants.NOTE_ON, 52, 63);
      sendMidi(MidiConstants.NOTE_ON, 55, 63);
    }
    Log.d("MIDI:", "Started Playing");
  }

  protected void sendMidi(int m, int n)
  {
    byte msg[] = new byte[2];

    msg[0] = (byte) m;
    msg[1] = (byte) n;

    midiDriver.write(msg);
  }

  // Send a midi message, 3 bytes
  protected void sendMidi(int m, int n, int v)
  {
    byte msg[] = new byte[3];

    msg[0] = (byte) m;
    msg[1] = (byte) n;
    msg[2] = (byte) v;

    midiDriver.write(msg);
  }
}
