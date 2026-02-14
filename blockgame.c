#include "raylib.h"
#include <stdbool.h>
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

int CalculateSquareSize(void) {
  const int screenWidth = GetMonitorWidth(GetCurrentMonitor());
  const int screenHeight = GetMonitorHeight(GetCurrentMonitor());
  int maxTall = screenHeight / (rows + 1);
  int maxWide = screenWidth / (cols + num_piece_rows);
  return (maxTall < maxWide ? maxTall : maxWide) * windowSize;
}

typedef bool Shape[piece_length][piece_length];

typedef struct GridSquare {
  bool empty;
  Color color;
} GridSquare;

typedef GridSquare Grid[cols][rows];

typedef struct CanvasPos {
  int x;
  int y;
} CanvasPos;

typedef struct GridPos {
  int x;
  int y;
} GridPos;

typedef struct Drag {
  bool dragging;
  CanvasPos cpos;
} Drag;

typedef struct Piece {
  Color color;
  Shape shape;
  Drag drag;
} Piece;

typedef struct Size {
  int width;
  int height;
} Size;

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
CanvasPos SubCanvasPos(CanvasPos cp1, CanvasPos cp2) {
  return (CanvasPos){cp1.x - cp2.x, cp1.y - cp2.y};
}
GridPos AddGridPos(GridPos cp1, GridPos cp2) {
  return (GridPos){cp1.x + cp2.x, cp1.y + cp2.y};
}

bool MouseCollisionDetected(CanvasPos mouse, CanvasPos cp, Size size) {
  if (mouse.x > cp.x && mouse.x < cp.x + size.width && mouse.y > cp.y &&
      mouse.y < cp.y + size.height)
    return true;
  return false;
}

bool DoesCoodFit(int cood, int bound, int size) {
  if (cood < 0 || cood > bound - size)
    return false;
  return true;
}

Size GetPieceSize(const Piece *piece) {
  Size size = {0, 0};
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (piece->shape[col][row]) {
        if (col > size.width - 1) {
          size.width = col + 1;
        }
        if (row > size.height - 1) {
          size.height = row + 1;
        }
      }
    }
  }
  return size;
}

bool DoesShapeFit(GridPos gpos, const Piece *piece) {
  Size size = GetPieceSize(piece);
  return DoesCoodFit(gpos.x, cols, size.width) &&
         DoesCoodFit(gpos.y, rows, size.height);
}

void GridInit(Grid grid) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      grid[col][row].empty = true;
    }
  }
}

void RenderGrid(Grid grid) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      Rectangle rec = GridToRectangle((GridPos){col, row});
      Color c;
      if (grid[col][row].empty) {
        c = (Color){30, 30, 30, 255};
      } else {
        c = grid[col][row].color;
      }
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
  piece->drag.dragging = false;
}

CanvasPos ResetPiece(int i) {
  return GridToCanvas(
      (GridPos){(cols + i / pieces_per_grid_length * piece_length),
                (i % pieces_per_grid_length) * piece_length});
}

void BuildPieces(Pieces pieces) {
  for (int i = 0; i != num_pieces; i++) {
    BuildPiece(&pieces[i]);
  }
}

void DrawShadow(const Piece *piece) {
  CanvasPos mousePos = {GetMouseX() + squareLength / 2,
                        GetMouseY() + squareLength / 2};
  GridPos gpos = CanvasToGrid(SubCanvasPos(mousePos, piece->drag.cpos));
  if (!DoesShapeFit(gpos, piece))
    return;
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (piece->shape[col][row]) {
        CanvasPos cpos = GridToCanvas(AddGridPos((GridPos){col, row}, gpos));
        Rectangle rec = CanvasToRectangle(cpos);
        Color c = piece->color;
        c.a = 50;
        DrawRectangleRec(rec, c);
      }
    }
  }
}

void DrawPieces(Pieces pieces) {
  CanvasPos mousePos = {GetMouseX(), GetMouseY()};
  for (int i = 0; i != num_pieces; i++) {
    for (int col = 0; col != piece_length; col++) {
      for (int row = 0; row != piece_length; row++) {
        CanvasPos cpos;
        if (pieces[i].shape[col][row]) {
          if (pieces[i].drag.dragging) {
            DrawShadow(&pieces[i]);
            cpos = AddCanvasPos(SubCanvasPos(mousePos, pieces[i].drag.cpos),
                                GridToCanvas((GridPos){col, row}));
            Rectangle rec = CanvasToRectangle(cpos);
            Color c = pieces[i].color;
            DrawRectangleRec(rec, c);
          } else {
            cpos =
                AddCanvasPos(ResetPiece(i), GridToCanvas((GridPos){col, row}));
            Rectangle rec = CanvasToRectangle(cpos);
            Color c = pieces[i].color;
            DrawRectangleRec(rec, c);
          }
        }
      }
    }
  }
}

// bool DoesShapeFit(const Piece *piece, GridPos gpos, const Grid grid) {
//   GridPos newgpos =
//   for (int col = 0; col != piece_length; col++) {
//     for (int row = 0; row != piece_length; row++) {
//       if (grid[newgpos.x][newgpos.y].empty) {
//         return false;
//       }
//     }
//   }
//   return true;
// }

void OnMouseClick(Pieces pieces) {
  CanvasPos mousePos = {GetMouseX(), GetMouseY()};
  for (int i = 0; i != num_pieces; i++) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        MouseCollisionDetected(
            mousePos, ResetPiece(i),
            (Size){piece_length * squareLength, piece_length * squareLength}) &&
        !pieces[i].drag.dragging) {
      pieces[i].drag.dragging = true;
      pieces[i].drag.cpos = SubCanvasPos(mousePos, ResetPiece(i));
    }
  }
}

void OnMouseRelease(Pieces pieces) {
  for (int i = 0; i != num_pieces; i++) {
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && pieces[i].drag.dragging) {
      pieces[i].drag.dragging = false;
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
  // Drag d = {false, (CanvasPos){50, 50}};
  // Piece p = {.color = MAGENTA, {{0, 0, 1}, {1, 0, 0}, {0, 0, 1}}, .drag = d};
  // bool b = DoesShapeFit((GridPos){0, 0}, &p);
  // if (b) {
  //   // return 0;
  // }
  while (!WindowShouldClose() && !stop) {

    BeginDrawing();
    ClearBackground((Color){46, 46, 46, 255});
    RenderGrid(grid);
    OnMouseClick(pieces);
    OnMouseRelease(pieces);
    DrawPieces(pieces);
    // stop = true;
    // DrawText("Congrats! You created your first window!", 190, 200, 20,
    //          LIGHTGRAY);

    EndDrawing();
  }
  CloseWindow();
  return 0;
}
