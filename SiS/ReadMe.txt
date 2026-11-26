Copyright (C) 1998-2006 Silicon Integrated Systems Corporation. All right reserved. 
This driver is licensed to you for the sole purpose as to use with the product(s) of Silicon Integrated Systems Corp. ("SiS").
Any reproduction or redistribution of this deriver is expressly prohibited. 


Overview
--------
SiS VGA software package consists of VGA drivers, utilities and Installation/Uninstallation programs for Windows 98/ME/2000/XP. 
The installation/uninstallation programs are capable of installing/uninstalling VGA drivers and utilities respectively. 
ur utilities consist of utility manager, driver mode setting utility, gamma correction utility, video setting utility, 
TV output setting utility, center screen for TV, product and file information utility, utility tray, and hotkey define utility. 


Objective
---------
The objective of this document is to introduce all utilities in our VGA package applications for Windows 98/ME/2K/XP briefly and 
give users a common view upon our VGA utilities. 


Supported Chipset
--------------------
1. 661FX/M661FX/M661MX
2. 760/760GX/M760/M760GX
3. 741/741GX/M741
4. 661GX/M661GX
5. 662/M662/M662MX
6. 761GX/761GL/M761GX
7. 771
8. 671
9. 672/M672

Utility Introduction
--------------------
(1) Utility manager: 
    It acts as an entrance for users to enter all SiS utilities. Users can move the mouse upon an animation utility button shown in 
    utility manager and press it to invoke the specified utility dialog. 

(2) Driver mode setting: 
    It provides users to change display state, including driver mode, output device and resolution. 

(3) Gamma correction: 
    It provides users to adjust the screen color TINT, the tint range from blue to red, according the preference of the users. 

(4) Video setting: 
    It provides users to adjust the hue, saturation, contrast, and brightness of video display. 

(5) TV output setting: 
    It provides users to adjust the TV screen position and TV filter. 

(6) Center screen for TV:
    It provides users to adjust display boundary on TV system. 

(7) Product and file information: 
    It is aimed to list out the product information and the related files information included in the VGA package. 

(8) Hotkey define: 
    It provides friendly ways for users to define hotkeys associated with specified functions of "OSD" or "3D Stereo". 


Installation
--------------------
(1) Generic install
    Go to the software package directory, double click "setup.exe" icon.

(2) Silence install
    Need under the DOS prompt, change directory to the software package path.
    Type "setup -s"

(3) INF install
    for example: XP-SP2
    Go to "Control Panel -> Display", select "Setting -> Advanced -> Adapter -> Properties", select "Driver -> Update",
    and point to INF's location.


SiSTray operation
--------------------
(1) Left click twice, System Property page will pop out. 
(2) Right click once, SiSTray list will show up. 
    SiSTray
     |-System Property(S)
     |-Display Property(D)---Display Mode Setting (I)
     |                     |-Desktop Gamma Correction (G) 
     |                     |-Video Setting (V)
     |                     |-Product and File Information (F)
     |-Application(A)---Uninstall Display Driver (U)
     |-About(B)...
     |-Exit Tray(E)
