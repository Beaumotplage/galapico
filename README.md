# Galapico - Pi Pico 2 Arcade Emulator

https://www.youtube.com/watch?v=WvlSHfgt45Q

This is a port of the excellent Galagino ESP32 emulator to the Pi Pico 2, with 15kHz outputs to a CRT TV/Monitor. 
See here for the Galagino emulator: https://github.com/harbaum/galagino

This port is NOT by the original author, so please do not bother them with issues.
The 'back-end' tile/sprite/video for this code differs a lot from Galagino, hence I've probably broken a few things.
You will need to follow the same steps to find the ROMs, process them and then compile a .uf2 file to drop onto a Pi Pico2
You will also need to install Vs Code and the Pi Pico plugin, then import the downloaded project and compile it (release, no debug).


Issues (many)
Does it compile for anyone other than me? Probably need a better readme/instructions.

Considerable code tidy-up needed. I threw it on GitHub prematurely, but needed to focus on other things in life.
Digdug is running 1.5MHz CPUs to avoid overloading the Pico.
Like galagino, the Z80 looks to be underclocked on some stuff - at least in 1942. 
    Setting INST_PER_FRAME to 500000 cures this but will totally overload the Pico2.
     I've resorted to individual INST_PER_FRAME for each game (config.h). 
Frogger - leftmost tiles for scrolling items need sorting. Horrible audio squeal.
Iffy sound generally. Donkey Kong needs to change sample rate, amongst other issues. 
I might change the title menu to a text based one for various internal reasons.
Donkey kong is off the side by 1 pixel (thought I'd sorted that ages ago).


Instructions (WIP)
Download the project from GIT
Follow all Galagino instructions to obtain ROMs and run the python scripts to auto-generate source code from the ROMs.
Rename z80.c to z80.cpp

Install Vscode.
Install the Raspberry Pi Pico Extension for VScode (find it using the Extensions in VScode).
Go to the Pico extention and Import Project.
Point it to the folder for the project.
Set the sdk to 2.0.0 (I've not tried newer)
Press Import and it should combine the GIT download with a compilable framework.
Go to the Cmake settings (far left triangle panel). 
In Configure, set the compiler to 'pico' in the Configure. If it's not there, something's gone wrong in the import.
In Configure, set the build to Release, unless you really want to hook up a debugger to the Pico to step the code when developing.
Build the project (View/command pallet/ build)

