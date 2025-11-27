# ZurOS
ZurOS is a very simple OS that doesn't really do to much so far. ZurOS is a solo project, I started working on it at 25.11.2025.

# How to use ZurOS?
**Note:** this section won't include tutorial on how to install or run ZurOS (to run it with qemu simply copy this repository and run run.sh, it will work on Ubuntu I dunno about other distros and OSes)
## List of commands
- ascii - print out an ascii art
- clear - clears the screen
- color 0xXY - change terminals color, for example color 0x0F sets BG color to black and FG color to white
- color -themes - shows some nice color themes (nice color codes for color command)
- exit - shutdowns the computer (it works in qemu I dunno what will happen on a real computer)
- help - writes a list of all available commands
- kprint "X", 0xYZ - allows to use kernel's kprint function, example kprint command: kprint "Hello, World!\n", 0x0F
- kprint -help - prints out more detailed description of kprint
- test - writes hello world in colors with ids 0x00-0x0F
- Z - tells a very unfunny polish joke in polish
## Keybinds, conveniences and inconveniences
**List of custom keybinds:**
- Tab - print 'Z'
  
**List of conveniences:**
- Commands history (with up and down arrows)
  
**List of inconveniences:**
- No test scrolling (this means that you often have to use clear)
