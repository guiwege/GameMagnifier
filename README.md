# GameMagnifier
A magnifier controlled by XInput (A.k.a Xbox Controller Protocoll)

Description: Implements a magnifier that is controlled by XInput
This program uses Microsoft's Magnification and XInput API.

Basic commands: 
-Pressing both analogs at the same time toggles the magnifier.
-Use the right analog to move the magnifier.
-Holding both analogs up for a few seconds closes the program.
-Holding LT will hold the magnifier in place.

If you want to use the Guide Button to toggle the magnifier on and off, 
follow the steps below:
1. Put xinput1_3.dll in this apps folder.

That's it if your version of Windows is below 10.

For Windows 10, it seems that there is an update that disables the Guide Button, 
and in order to get it working again you will have to follow a few more steps 
(Credits to nullskillz from Steam forums):

2. Go to Device Manager > XBOX 360 Peripherals > Right click your XBOX Controller > 
Update Driver > Search in computer > Pick from a list > 
Now choose a different driver from the list and reboot your PC. 
For me there were only 2 drivers in the list and they had exactly the same name,  
I tried the first one and it worked.  

The program will work on Windowed and Windowed Borderless modes. Fullscreen is not supported.
