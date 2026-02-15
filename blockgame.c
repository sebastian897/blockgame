#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
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
#define EMPTY (Color){30, 30, 30, 255}
Color palette[] = {EMPTY,      MAROON,    ORANGE, DARKGREEN, DARKBLUE,
                   DARKPURPLE, DARKBROWN, RED,    GOLD,      LIME,
                   BLUE,       VIOLET,    BROWN,  PINK,      YELLOW,
                   GREEN,      SKYBLUE,   PURPLE, BEIGE};

typedef bool Shape[piece_length][piece_length];

typedef uint8_t Grid[cols][rows];

typedef struct CanvasPos {
  int x;
  int y;
} CanvasPos;

typedef struct GridPos {
  int x;
  int y;
} GridPos;

typedef struct ScreenPos {
  int x;
  int y;
} ScreenPos;

typedef struct Canvas {
  ScreenPos origin;
} Canvas;

typedef struct Drag {
  bool dragging;
  ScreenPos offset;
} Drag;

typedef struct Piece {
  uint8_t pal_idx;
  Shape shape;
  Drag drag;
} Piece;

typedef struct Size {
  int width;
  int height;
} Size;

typedef Piece Pieces[num_pieces];

ScreenPos CanvasToScreen(CanvasPos cpos, Canvas c) {
  return (ScreenPos){cpos.x + c.origin.x, cpos.y + c.origin.y};
}
CanvasPos ScreenToCanvas(ScreenPos cpos, Canvas c) {
  return (CanvasPos){cpos.x - c.origin.x, cpos.y - c.origin.y};
}
CanvasPos GridToCanvas(GridPos gp) {
  return (CanvasPos){gp.x * squareLength, gp.y * squareLength};
}
Rectangle SceenToRectangle(ScreenPos sp) {
  return (Rectangle){sp.x, sp.y, squareLength * squareAmount,
                     squareLength * squareAmount};
}
// Rectangle CanvasToRectangle(CanvasPos cp) {
//   return (Rectangle){cp.x, cp.y, squareLength * squareAmount,
//                      squareLength * squareAmount};
// }
// Rectangle GridToRectangle(GridPos gp) {
//   return CanvasToRectangle(GridToCanvas(gp));
// }
GridPos CanvasToGrid(CanvasPos cp) {
  if (cp.x < 0 || cp.y < 0)
    return (GridPos){-1, -1}; // ensure negatives dont get displayes
  return (GridPos){cp.x / squareLength, cp.y / squareLength};
}

CanvasPos AddCanvasPos(CanvasPos cp1, CanvasPos cp2) {
  return (CanvasPos){cp1.x + cp2.x, cp1.y + cp2.y};
}
CanvasPos SubCanvasPos(CanvasPos cp1, CanvasPos cp2) {
  return (CanvasPos){cp1.x - cp2.x, cp1.y - cp2.y};
}
ScreenPos SubScreenPos(ScreenPos sp1, ScreenPos sp2) {
  return (ScreenPos){sp1.x - sp2.x, sp1.y - sp2.y};
}
GridPos AddGridPos(GridPos cp1, GridPos cp2) {
  return (GridPos){cp1.x + cp2.x, cp1.y + cp2.y};
}

int CalculateSquareSize(void) {
  const int screenWidth = GetMonitorWidth(GetCurrentMonitor());
  const int screenHeight = GetMonitorHeight(GetCurrentMonitor());
  int maxTall = screenHeight / (rows);
  int maxWide = screenWidth / (cols + num_piece_rows);
  return (maxTall < maxWide ? maxTall : maxWide) *windowSize;
}

bool MouseCollisionDetected(ScreenPos mouse, ScreenPos objpos, Size size) {
  if (mouse.x > objpos.x && mouse.x < objpos.x + size.width && mouse.y > objpos.y &&
      mouse.y < objpos.y + size.height)
    return true;
  return false;
}

bool DoesCoordFit(int coord, int bound, int size) {
  if (coord < 0 || coord > bound - size)
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
  return DoesCoordFit(gpos.x, cols, size.width) &&
         DoesCoordFit(gpos.y, rows, size.height);
}

void GridInit(Grid grid) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      grid[col][row] = 0; // empty index = 0
    }
  }
}

void RenderGrid(Grid grid, Canvas gc) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      Rectangle rec = SceenToRectangle(
          CanvasToScreen(GridToCanvas((GridPos){col, row}), gc));
      Color c;
      c = palette[grid[col][row]];
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
  piece->pal_idx = rand() % (ARRAY_LENGTH(palette) - 1) + 1; // dont chose empty
  piece->drag.dragging = false;
}

CanvasPos GetPieceHomePos(int piece_idx) {
  return GridToCanvas(
      (GridPos){(piece_idx / pieces_per_grid_length * piece_length),
                (piece_idx % pieces_per_grid_length) * piece_length});
}

void BuildPieces(Pieces pieces) {
  for (int i = 0; i != num_pieces; i++) {
    BuildPiece(&pieces[i]);
  }
}

void DrawShadowRectangle(const Piece *piece, int col, int row, Canvas gc) {
  ScreenPos mousePos = {GetMouseX() + squareLength / 2,
                        GetMouseY() + squareLength / 2};
  GridPos gpos = CanvasToGrid(ScreenToCanvas(SubScreenPos(mousePos, piece->drag.offset), gc));
  if (!DoesShapeFit(gpos, piece))
    return;
  if (piece->shape[col][row]) {
    Rectangle rec = SceenToRectangle(CanvasToScreen(
        GridToCanvas(AddGridPos((GridPos){col, row}, gpos)), gc));
    Color c = palette[piece->pal_idx];
    c.a = 127;
    DrawRectangleRec(rec, c);
  }
}

void DrawPiece(const Piece *piece, int i, Canvas gc, Canvas pc) {
  ScreenPos mousePos = {GetMouseX(), GetMouseY()};
  Canvas mc = {(ScreenPos){0,0}};
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (piece->shape[col][row]) {
        Rectangle rec;
        if (piece->drag.dragging) {
          DrawShadowRectangle(piece, col, row, gc);

          rec = SceenToRectangle(CanvasToScreen(
              AddCanvasPos(ScreenToCanvas(SubScreenPos(mousePos, piece->drag.offset), mc),
                           GridToCanvas((GridPos){col, row})),mc));
        } else {
          rec = SceenToRectangle(CanvasToScreen(AddCanvasPos(
              GetPieceHomePos(i), GridToCanvas((GridPos){col, row})),pc));
        }
        DrawRectangleRec(rec, palette[piece->pal_idx]);
      }
    }
  }
}

void DrawPieces(Pieces pieces, Canvas gc, Canvas pc) {
  int drag_idx = -1;
  for (int i = 0; i != num_pieces; i++) {
    if (!pieces[i].drag.dragging) {
      DrawPiece(&pieces[i], i, gc, pc);
    } else {
      drag_idx = i; // save idx of drag pieces for later drawing
    }
  }
  if (drag_idx != -1) {
    DrawPiece(&pieces[drag_idx], drag_idx, gc, pc);
  }
}

void OnMouseClick(Pieces pieces, Canvas pc) {
  ScreenPos mousePos = {GetMouseX(), GetMouseY()};
  for (int i = 0; i != num_pieces; i++) {
    ScreenPos p_origin = CanvasToScreen(GetPieceHomePos(i),pc);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        MouseCollisionDetected(
            mousePos, p_origin,
            (Size){piece_length * squareLength, piece_length * squareLength})) {
      pieces[i].drag.dragging = true;
      pieces[i].drag.offset = SubScreenPos(mousePos, p_origin);
      return;
    }
  }
}

void OnMouseRelease(Pieces pieces) {
  for (int i = 0; i != num_pieces; i++) {
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && pieces[i].drag.dragging) {
      pieces[i].drag.dragging = false;
      return;
    }
  }
}

int main(void) {
  srand(time(NULL));
  InitWindow(800, 600, "BlockGame");
  // ToggleFullscreen();
  squareLength = CalculateSquareSize();
  SetWindowSize(squareLength * (cols + num_piece_rows * piece_length),
                squareLength * rows);
  SetTargetFPS(60);
  Canvas gc = {(ScreenPos){0, 0}};
  Canvas pc = {{GridToCanvas((GridPos){cols, 0}).x, 0}};

  Grid grid;
  Pieces pieces;
  BuildPieces(pieces);
  bool stop = false;
  GridInit(grid);
  while (!WindowShouldClose() && !stop) {

    BeginDrawing();
    ClearBackground((Color){46, 46, 46, 255});
    RenderGrid(grid, gc);
    OnMouseClick(pieces, pc);
    OnMouseRelease(pieces);
    DrawPieces(pieces, gc, pc);
    EndDrawing();
  }
  CloseWindow();
  return 0;
}
