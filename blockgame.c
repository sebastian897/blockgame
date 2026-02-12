#include "raylib.h"
#include <minwindef.h>

#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))

const int rows = 9;
const int cols = 9;
const int pieceLength = 3;
const int numberOfShapes = 3;
const int piecesPerGridLength = rows / pieceLength;
const int numberOfRowsOfPieces = CEIL_DIV(numberOfShapes, piecesPerGridLength);
const double windowSize = 0.8;
int squareLength;

int CalculateSquareSize() {
  const int screenWidth = GetMonitorWidth(GetCurrentMonitor());
  const int screenHeight = GetMonitorHeight(GetCurrentMonitor());
  int maxTall = screenHeight / (rows + 1);
  int maxWide = screenWidth / (cols + numberOfRowsOfPieces);
  return (maxTall < maxWide ? maxTall : maxWide) * windowSize;
}

int main(void) {

  InitWindow(900, 690, "BlockGame");
  squareLength = CalculateSquareSize();
  SetWindowSize(squareLength * (cols + numberOfRowsOfPieces * pieceLength),
                squareLength * rows);
  SetTargetFPS(60);
  while (!WindowShouldClose()) {

    BeginDrawing();

    ClearBackground(RAYWHITE);
    // Rectangle rec = {
    //   .height =
    //   .
    // }

    // DrawRectangleRec(Rectangle, Color color)
    DrawText("Congrats! You created your first window!", 190, 200, 20,
             LIGHTGRAY);

    EndDrawing();
  }
  CloseWindow();
  return 0;
}