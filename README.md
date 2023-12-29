# Space Invaders clone in C++

## Introduction
This project is a clone of Space Invaders built in C++ using OpenGL for graphics rendering, and irrKlang for audio control. It is intended to be my CS50 final project. 

## Game description
The entire game is written in `main.cpp`, with around 1000 lines of code. I later realised very far in the game development process that this was probably a bad idea, because it became quite difficult to organise the code properly. I chose to write it in C++ because I enjoyed using C throughout the CS50 course, but wanted some extra functionality that C could not offer, such as classes, and a greater number of libraries available to download. I also used a small amount of the GLSL language, which is used to write fragment and vertex shaders for use in OpenGL. It is syntactically quite similar to C and allowed me to render sprites using a vertex array object.

Rendering using OpenGL was one of the most conceptually difficult parts of the game. To render sprites, I used OpenGL's graphics pipeline. This involved: creating a vertex shader (responsible for providing vertex coordinates - pixel colours between the vertices are then interpolated), creating a fragment shader (responsible for doing interpolation to find colour of a pixel, using a buffer which stores our pixel data), and then creating a VAO (vertex array object) to link the vertex data and the shader programs.

## Running instructions
### Linux
For Linux users, navigate to the /bin/Release folder. Then simply double click on the executable file space_invaders_cs50.
### Windows
I use a Linux system, so there is no Windows executable file. If you like, you can open the project in Code::Blocks (the IDE used to develop the game), and build it yourself.

## Controls
- Move left: LEFT arrow
- Move right: RIGHT arrow
- Shoot: Z
- Quit game: ESC
- Start game: ENTER

## Features
- Rendering in OpenGL using GLSL language and GLFW wrapper
    - Sprite rendering using buffer
    - Text rendering
    - Scaling available to change size of text/sprites
- Title screen
- Timer system for delays and game events
- Keyboard control
- Enemy and player shooting system
- Scoring and HP system
- 3 enemy types with different healths and bullet damages
- 5 levels (game continues indefinitely after level 5)
- Audio control system using irrKlang library
    - Title screen music
    - Start game SFX
    - In-game music
    - Enemy and player damage SFX
    - Player shoot SFX
    - Enemy death SFX
    - Player death SFX
- HP regeneration between levels
- Game over screen

## Credits
See CREDITS.md for a list of all credits.
  




