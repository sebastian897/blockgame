#include "raylib.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

enum {
  rows = 9,
  cols = 9,
  piece_length = 3,
  num_pieces = 3,
  pieces_per_grid_length = rows / piece_length,
  num_piece_rows = CEIL_DIV(num_pieces, pieces_per_grid_length),
  square_probability = 35
};

const double windowSize = 0.8;
const double squareAmount = 0.95;
int squareLength;

bool debug = true;

Color palette[] = {MAROON, ORANGE, DARKGREEN, DARKBLUE, DARKPURPLE, DARKBROWN,
                   RED,    GOLD,   LIME,      BLUE,     VIOLET,     BROWN,
                   PINK,   YELLOW, GREEN,     SKYBLUE,  PURPLE,     BEIGE};

int CalculateSquareSize() {
  const int screenWidth = GetMonitorWidth(GetCurrentMonitor());
  const int screenHeight = GetMonitorHeight(GetCurrentMonitor());
  int maxTall = screenHeight / (rows + 1);
  int maxWide = screenWidth / (cols + num_piece_rows);
  return (maxTall < maxWide ? maxTall : maxWide) * windowSize;
}

typedef Color Grid[cols][rows];
typedef bool Shape[piece_length][piece_length];

typedef struct CanvasPos {
  int x;
  int y;
} CanvasPos;

typedef struct GridPos {
  int x;
  int y;
} GridPos;

typedef struct Piece {
  Color color;
  Shape shape;
  bool dragging;
  CanvasPos cpos;
} Piece;

typedef Piece Pieces[num_pieces];

CanvasPos GridToCanvas(GridPos gp) {
  return (CanvasPos){gp.x * squareLength, gp.y * squareLength};
}
Rectangle CanvasToRectangle(CanvasPos cp) {
  return (Rectangle){cp.x, cp.y, squareLength * squareAmount,
                     squareLength * squareAmount};
}
Rectangle GridToRectangle(GridPos gp) {
  return CanvasToRectangle(GridToCanvas(gp));
}
GridPos CanvasToGrid(CanvasPos cp) {
  return (GridPos){cp.x / squareLength, cp.y / squareLength};
}

CanvasPos AddCanvasPos(CanvasPos cp1, CanvasPos cp2) {
  return (CanvasPos){cp1.x + cp2.x, cp1.y + cp2.y};
}

void GridInit(Grid grid) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      grid[col][row] = (Color){30, 30, 30, 255};
    }
  }
}

void RenderGrid(Grid grid) {
  for (int col = 0; col < cols; col++) {
    for (int row = 0; row < rows; row++) {
      Rectangle rec = GridToRectangle((GridPos){col, row});
      Color c = grid[col][row];
      DrawRectangleRec(rec, c);
    }
  }
}

bool IsTopRowEmpty(Shape shape) {
  for (int col = 0; col != piece_length; col++)
    if (shape[col][0])
      return false;
  return true;
}
bool IsLeftColEmpty(Shape shape) {
  for (int row = 0; row != piece_length; row++)
    if (shape[0][row])
      return false;
  return true;
}

void RemoveTopRow(Shape shape) {
  for (int row = 0; row != piece_length - 1; row++) {
    for (int col = 0; col != piece_length; col++) {
      shape[col][row] = shape[col][row + 1];
    }
  }
  for (int col = 0; col != piece_length; col++) {
    shape[col][piece_length - 1] = false;
  }
}

void RemoveLeftCol(Shape shape) {
  for (int col = 0; col != piece_length - 1; col++) {
    for (int row = 0; row != piece_length; row++) {
      shape[col][row] = shape[col + 1][row];
    }
  }
  for (int row = 0; row != piece_length; row++) {
    shape[piece_length - 1][row] = false;
  }
}

// printf("ptr=%p col=%d row=%d rnd=%d b=%d\n", (void*)&piece->shape[col][row],
// col, row, rnd, b);
void BuildPiece(Piece *piece) {
  for (int col = 0; col < piece_length; col++) {
    for (int row = 0; row < piece_length; row++) {
      int rnd = rand() % 100;
      bool b = rnd < square_probability;
      piece->shape[col][row] = b;
    }
  }
  while (IsTopRowEmpty(piece->shape)) {
    RemoveTopRow(piece->shape);
  }
  while (IsLeftColEmpty(piece->shape)) {
    RemoveLeftCol(piece->shape);
  }
  piece->color = palette[rand() % ARRAY_LENGTH(palette)];
  piece->dragging = false;
}

void ResetPiece(Piece *piece, int i) {
  piece->cpos =
      GridToCanvas((GridPos){(cols + i / pieces_per_grid_length * piece_length),
                             (i % pieces_per_grid_length) * piece_length});
}

void BuildPieces(Pieces pieces) {
  for (int i = 0; i != num_pieces; i++) {
    BuildPiece(&pieces[i]);
    ResetPiece(&pieces[i], i);
    // if (debug) {
    //   printf("\nShape %d %p\n", i, (void *)&pieces[i]);
    //   for (int row = 0; row != PIECE_LENGTH; row++) {
    //     for (int col = 0; col != PIECE_LENGTH; col++) {
    //       printf("%s ", pieces[i].shape[col][row] ? "X" : " ");
    //     }
    //     printf("\n");
    //   }
    // }
  }
}

void DrawPieces(Pieces pieces) {
  for (int i = 0; i != num_pieces; i++) {
    // if (debug) {
    //   printf("piece# %d\n", i);
    // }
    for (int col = 0; col != piece_length; col++) {
      for (int row = 0; row != piece_length; row++) {
        if (pieces[i].shape[col][row]) {
          CanvasPos cpos =
              AddCanvasPos(pieces[i].cpos, GridToCanvas((GridPos){col, row}));
          // if (debug) {
          //   printf("canvas x,y %d,%d\n", cpos.x, cpos.y);
          // }
          Rectangle rec = CanvasToRectangle(cpos);
          Color c = pieces[i].color;
          DrawRectangleRec(rec, c);
        }
      }
    }
  }
}

int main(void) {
  srand(time(NULL));
  InitWindow(900, 690, "BlockGame");
  squareLength = CalculateSquareSize();
  SetWindowSize(squareLength * (cols + num_piece_rows * piece_length),
                squareLength * rows);
  SetTargetFPS(60);

  Grid grid;
  Pieces pieces;
  BuildPieces(pieces);
  bool stop = false;
  GridInit(grid);
  while (!WindowShouldClose() && !stop) {

    BeginDrawing();
    ClearBackground((Color){46, 46, 46, 255});
    RenderGrid(grid);
    DrawPieces(pieces);
    debug = false;
    // stop = true;
    // DrawText("Congrats! You created your first window!", 190, 200, 20,
    //          LIGHTGRAY);

    EndDrawing();
  }
  CloseWindow();
  return 0;
}