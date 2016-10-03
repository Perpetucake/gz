## Installing gz
The currently supported games are:
- The Legend of Zelda: Ocarina of Time (NTSC Version 1.0)
- The Legend of Zelda: Ocarina of Time (NTSC Version 1.2)

There are two methods of using this software, described below.

### Using gz with a Gameshark
You will need:
- An N64 with a supported game cartridge, and an Expansion Pak.
- A Gameshark with a functional parallel port (note that some 3.3s only have dummy ports).
- A Parallel to USB adapter cable with bidirectional communication support.
  The included Gameshark utility is designed to function with a Moschip MCS7705 cable,
  and has only been tested with a version 3.3 Gameshark.

Follow these steps;
- Boot the Gameshark with your game cartridge. If your game requires a special keycode to boot, you'll first
  need to boot the Gameshark with a game that works with the default keycode, select the the required keycode in
  the Key Codes menu, and then reboot with the game you wish to use with gz.
- In the Select Cheat Codes menu, select your game and make sure the (M) code is active, if one exists.
- In the Start Game submenu, enable the Code Generator option, and select Start Game With Selected Codes.
- Connect the Gameshark to your computer with your Parallel to USB adapter cable.
- Start Zadig and select the Gameshark in the device the list (probably named USB Device or something similar).
- Select libusbK in the driver list and click Replace Driver.
- Navigate to the directory that corresponds to your game and run the `upload.bat` script.
  This will instruct the Gameshark utility to upload gz to the game's memory,
  and then disconnect the Gameshark. The operation will take a little while.
- When the upload is completed, you can disconnect the USB cable and start playing.

### Using gz with an emulator / flash cart
Drag and drop the rom you wish to patch onto the `patch.bat` script. A patched rom will be
created in the same directory as the script. The patched rom can be played with an emulator or transferred to a flash cart.
For emulator usage, you will need to enable Expansion Pak emulation.
On some emulators, you may need to change CPU Core Style to Interpreter.
For use with a flash cart, your N64 will need an Expansion Pak.