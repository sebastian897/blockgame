#include "raylib.h"
#include <minwindef.h>

#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))

#define ROWS 9
#define COLS 9
const int pieceLength = 3;
const int numberOfShapes = 3;
const int piecesPerGridLength = ROWS / pieceLength;
const int numberOfROWSOfPieces = CEIL_DIV(numberOfShapes, piecesPerGridLength);
const double windowSize = 0.8;
const double squareAmount = 0.95;
int squareLength;

int CalculateSquareSize() {
  const int screenWidth = GetMonitorWidth(GetCurrentMonitor());
  const int screenHeight = GetMonitorHeight(GetCurrentMonitor());
  int maxTall = screenHeight / (ROWS + 1);
  int maxWide = screenWidth / (COLS + numberOfROWSOfPieces);
  return (maxTall < maxWide ? maxTall : maxWide) * windowSize;
}

typedef struct CanvasPos {
  int x;
  int y;
} CanvasPos;

typedef struct GridPos {
  int x;
  int y;
} GridPos;

CanvasPos GridToCanvas(GridPos gPos) { return (CanvasPos){gPos.x, gPos.y}; }

void GridInit(Color grid[COLS][ROWS]) {
  for (int col = 0; col != COLS; col++) {
    for (int row = 0; row != ROWS; row++) {
      grid[col][row] = (Color){30, 30, 30, 255};
    }
  }
}

int main(void) {

  InitWindow(900, 690, "BlockGame");
  squareLength = CalculateSquareSize();
  SetWindowSize(squareLength * (COLS + numberOfROWSOfPieces * pieceLength),
                squareLength * ROWS);
  SetTargetFPS(60);

  Color grid[COLS][ROWS];
  GridInit(grid);
  while (!WindowShouldClose()) {

    BeginDrawing();
    ClearBackground((Color){46,46,46,255});

    for (int col = 0; col < COLS; col++) {
      for (int row = 0; row < ROWS; row++) {
        Rectangle rec = {col * squareLength, row * squareLength,
                         squareLength * squareAmount, squareLength * squareAmount};
        Color c = grid[col][row];
        DrawRectangleRec(rec, c);
      }
    }

    DrawText("Congrats! You created your first window!", 190, 200, 20,
             LIGHTGRAY);

    EndDrawing();
  }
  CloseWindow();
  return 0;
}