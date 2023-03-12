DescentVR
==========
This is a source port of the original DOS game “Descent” for Oculus Quest 2. It is based on Descent port for Android from https://github.com/devint1/Descent-Mobile.

The easiest way to install this on your Quest is using SideQuest, download SideQuest here:
https://sidequestvr.com/

Important Note
---------------
This is just an engine port, the resource files (descent.hog and descent.pig) you should provide yourself copying from Descent copy you own. 
You can purchase it from https://store.steampowered.com/app/273570/Descent/.

Copying descent.hog and descent.pig to your Oculus Quest
----------------------------------------------------
Copy descent.hog and descent.pig from the installed Descent game folder on your PC to the /sdcard/DescentVR folder on your Oculus Quest when it is connected to the PC.

Controls:
---------
* Dominant-Hand trigger - Fire primary weapon
* Off-Hand trigger - Fire secondary weapon
* X Button - Next primary weapon
* Y Button - Next secondary weapon
* Left Thumbstick up/down - move forward/backward
* Right Thumbstick - strafe up/down/left/right
* Right Thumbstick click - Next level
* Left Thumbstick click - Previous level

Things to note / FAQs:
----------------------
* Player is "ephemeral" - you can go through walls
* Player is invulnerable
* All weapons are available from start

Known Issues:
-------------
* No cross hair
* Music doesn't play
* End level sequence is not triggered but can still switch the levels 
* Many more - please submit most annoying

Contributing
------------
You need the following:

* Android Developer Studio
* Android SDK API level 24
* Latest Android Native Development Kit
* Oculus Mobile SDK 1.24.0
* The DescentVR folder cloned from GitHub should be below VrSamples in the extracted SDK
* Create a local.properties file in the root of the extracted Oculus Mobile SDK that contains the ndk.dir and sdk.dir properties for where your SDK/NDK are located (see Gradle documentation regarding this)
* To build debug you will need a android.debug.keystore file placed in the following folder: oculus_sdk_dir/VrSamples/DescentVR/Projects/Android