# 3305 Project 1: BitBoard Checkers Game

## Author
Orye Eshed

## Description
My implementation of Owl Tech ’s Bitboard Checkers Game includes creating a program in C that can play simulate a real game of checkers using bitboards. 

My game uses 64-bit unsigned integer where each bit represents one square. Using the 64-bit unsigned integer, it mimicks a game board.

The game implements the rules of checkers and gameplay using bitwise operations.

## Gameplay Instructions
- Welcome to BitBoard Checkers! Two-player (local) game.
- P1 (red) moves UP; P2 (black) moves DOWN. Single jumps supported; captures optional.
- Bit indices (0-63) are shown to the right of the board.
- Commands:
  - move:  <from> <to>    e.g., 12 21
  - save   <file>        save current game state to file
  - load   <file>        load game state from file
  - help                 show this help
  - quit                 exit the game


## Build Instructions

1. Install JetBrains CLion.
2. Open CLion.
3. A project named checkers.c
4. Implement all necessary functions for checkers game
5. Create main function in checkers.c that will run the game
6. Click the run tool on the top right and test for correct output.


## Test Results
Output was tested and is correct in all functionalities.

## Notes
- Kinging the checkers pieces proved to be difficult to implement.
- Save/Load wouldn't work until I realized that I needed save file to be in the project folder because the program didn't have access to my files.
- Had problems with capture logic. The capture logic was being counted as an illegal move for a while until I realized that I had to add a statement that would check for a capture.
