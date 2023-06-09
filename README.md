DescentVR
==========
This is a source port of the original DOS game “Descent” for Oculus Quest 2. It is based on Descent port for Android from https://github.com/devint1/Descent-Mobile.

The easiest way to install this on your Quest is using SideQuest, download SideQuest here:
https://sidequestvr.com/

Gameplay videos
---------------
[![Demo1](https://img.youtube.com/vi/j4HTaqGv4dE/0.jpg)](https://youtu.be/j4HTaqGv4dE)

[![Demo2](https://img.youtube.com/vi/6hk1WnwpyaI/0.jpg)](https://youtu.be/6hk1WnwpyaI)


Copying game resource files
---------------
This project is far from being ready, but you can already relive the nostalgia by roaming around the levels you may have forgotten.

This is just an engine port, so you will need to provide the resource files (descent.hog and descent.pig) yourself by copying them from the Descent copy you own.
You can purchase it from https://store.steampowered.com/app/273570/Descent/.

Here is how to copy the files on to the Quest:

[![How-to](https://img.youtube.com/vi/b4w__I0EF2k/0.jpg)](https://youtu.be/b4w__I0EF2k)


Installation
------------
1. Download .apk file from [Releases](https://github.com/chvjak/DescentVR/releases) section
2. Use SideQuest to install the .apk to your Quest
3. Use SideQuest to copy *descent.hog* and *descent.pig* from Descent game folder on your PC to *DescentVR* folder on your Quest. You can do it once the Quest is connected to your PC via a cable.
4. Find DescentVR in Unknown Sources section of your Oculus Apps
5. Grant requested permissions on the first launch
6. Enjoy!

Controls:
---------
* Dominant-Hand trigger - Fire primary weapon
* Off-Hand trigger - Fire secondary weapon
* X Button - Next primary weapon
* A Button - Next secondary weapon
* Left Thumbstick up/down - move forward/backward
* Right Thumbstick - strafe up/down/left/right
* Right Thumbstick click - Next level
* Left Thumbstick click - Previous level

Things to note / FAQs:
----------------------
* Player is invulnerable
* All weapons are available from start

Known Issues:
-------------
* On some on levels above 10 - you may find yourself off place. Just fly back to start through space
* Some lighting is off
* Hostages are not rendered correctly
* Many more - please submit the most annoying

Contributing
------------
You need the following:

* Clone DescentVR sources
* Place GLM (https://github.com/g-truc/glm) in the same folder as DescentVR sources 
* Unzip Fluid Synth binaries for Android (https://github.com/FluidSynth/FluidSynth) in the same folder as DescentVR sources
* Android Developer Studio
* Android SDK
* Android NDK 22
* Oculus Mobile SDK
* The DescentVR folder cloned from GitHub should be below VrSamples in the extracted SDK
* Create a local.properties file in the root of the extracted Oculus Mobile SDK that contains the ndk.dir and sdk.dir properties for where your SDK/NDK are located (see Gradle documentation regarding this)
