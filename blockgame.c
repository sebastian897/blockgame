#include "raylib.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
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
  square_probability_max = 75,
  square_probability_min = 20,
  transparency = 127
};

typedef enum gamestate { menu, playing, game_over } gamestate;

const double windowSize = 0.8;
const double squareAmount = 0.95;
int squareLength;

#define MAX_A 255
bool debug = true;
#define EMPTY (Color){30, 30, 30, 255}
Color palette[] = {EMPTY,      MAROON,    ORANGE, DARKGREEN, DARKBLUE,
                   DARKPURPLE, DARKBROWN, RED,    GOLD,      LIME,
                   BLUE,       VIOLET,    BROWN,  PINK,      YELLOW,
                   GREEN,      SKYBLUE,   PURPLE, BEIGE};

typedef bool Shape[piece_length][piece_length];

typedef struct ScreenPos {
  int x;
  int y;
} ScreenPos;

typedef struct Canvas {
  ScreenPos origin;
} Canvas;

typedef struct Grid {
  uint8_t arr[cols][rows];
  Canvas canvas;
} Grid;

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
  ScreenPos offset;
} Drag;

typedef struct Piece {
  uint8_t pal_idx;
  Shape shape;
  Drag drag;
} Piece;

typedef struct Pieces {
  Piece arr[num_pieces];
  Canvas canvas;
} Pieces;

typedef struct Size {
  int width;
  int height;
} Size;

ScreenPos CanvasToScreen(CanvasPos cpos, Canvas c) {
  return (ScreenPos){cpos.x + c.origin.x, cpos.y + c.origin.y};
}
CanvasPos ScreenToCanvas(ScreenPos cpos, Canvas c) {
  return (CanvasPos){cpos.x - c.origin.x, cpos.y - c.origin.y};
}

CanvasPos GridToCanvas(GridPos gp) {
  return (CanvasPos){gp.x * squareLength, gp.y * squareLength};
}

double sf = 0.8;
CanvasPos GridToCanvasAtHome(GridPos gp) {
  double magic = piece_length / 2.0;
  return (CanvasPos){
      (gp.x) * squareLength * sf + (1 - sf) * magic * squareLength,
      (gp.y) * squareLength * sf + (1 - sf) * magic * squareLength};
}

ScreenPos GridToScreen(GridPos gp, Canvas c) {
  return CanvasToScreen(GridToCanvas(gp), c);
}

ScreenPos GridToScreenAtHome(GridPos gp, Canvas c) {
  return CanvasToScreen(GridToCanvasAtHome(gp), c);
}

Rectangle SceenToRectangle(ScreenPos sp) {
  return (Rectangle){sp.x, sp.y, squareLength * squareAmount,
                     squareLength * squareAmount};
}

Rectangle SceenToRectangleAtHome(ScreenPos sp) {
  return (Rectangle){sp.x, sp.y, squareLength * squareAmount * sf,
                     squareLength * squareAmount * sf};
}

GridPos CanvasToGrid(CanvasPos cp) {
  if (cp.x < 0 || cp.y < 0)
    return (GridPos){-1, -1}; // ensure negatives dont get displayes
  return (GridPos){cp.x / squareLength, cp.y / squareLength};
}

CanvasPos AddCanvasPos(CanvasPos cp1, CanvasPos cp2) {
  return (CanvasPos){cp1.x + cp2.x, cp1.y + cp2.y};
}
ScreenPos AddScreenPos(ScreenPos sp1, ScreenPos sp2) {
  return (ScreenPos){sp1.x + sp2.x, sp1.y + sp2.y};
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
  return (maxTall < maxWide ? maxTall : maxWide) * windowSize;
}

bool MouseCollisionDetected(ScreenPos mouse, ScreenPos objpos, Size size) {
  if (mouse.x > objpos.x && mouse.x < objpos.x + size.width &&
      mouse.y > objpos.y && mouse.y < objpos.y + size.height)
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

bool IsSpaceOccupied(const Piece *piece, GridPos gpos, Grid *grid) {
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (!piece->shape[col][row])
        continue;
      GridPos rec_pos = AddGridPos(gpos, (GridPos){col, row});
      if (grid->arr[rec_pos.x][rec_pos.y]) {
        return false;
      }
    }
  }
  return true;
}

bool DoesPieceFit(const Piece *piece, GridPos gpos, Grid *grid) {
  Size size = GetPieceSize(piece);
  return DoesCoordFit(gpos.x, cols, size.width) &&
         DoesCoordFit(gpos.y, rows, size.height) &&
         IsSpaceOccupied(piece, gpos, grid);
}

void GetFullCols(const Grid *grid, bool full_cols[cols]) {
  for (int col = 0; col != cols; col++) {
    int colCount = 0;
    for (int row = 0; row != rows; row++) {
      if (grid->arr[col][row]) {
        colCount++;
      }
    }
    if (colCount == rows) {
      full_cols[col] = true;
    }
  }
}

void GetFullRows(const Grid *grid, bool full_rows[rows]) {
  for (int row = 0; row != rows; row++) {
    int rowCount = 0;
    for (int col = 0; col != cols; col++) {
      if (grid->arr[col][row]) {
        rowCount++;
      }
    }
    if (rowCount == cols) {
      full_rows[row] = true;
    }
  }
}

void ClearSquares(Grid *grid) {
  bool full_cols[cols] = {0};
  GetFullCols(grid, full_cols);
  bool full_rows[rows] = {0};
  GetFullRows(grid, full_rows);
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      if (full_cols[col] || full_rows[row]) {
        grid->arr[col][row] = 0;
      }
    }
  }
}

bool DropPiece(Piece *piece, GridPos gpos, Grid *grid);

bool IsPiecesEmpty(const Pieces *pieces);

bool DoPiecesFit(Grid grid, Pieces pieces, int rem_levels) {
  if (IsPiecesEmpty(&pieces) || rem_levels == 0) {
    return true;
  }
  for (int p_idx = 0; p_idx != num_pieces; p_idx++) {
    if (pieces.arr[p_idx].pal_idx == 0)
      continue;
    for (int col = 0; col != cols; col++) {
      for (int row = 0; row != rows; row++) {
        GridPos gpos = {col, row};
        if (!DropPiece(&pieces.arr[p_idx], gpos, &grid))
          continue;
        pieces.arr[p_idx].pal_idx = 0;
        ClearSquares(&grid);
        if (DoPiecesFit(grid, pieces, rem_levels - 1)) {
          return true;
        }
      }
    }
  }
  return false;
}

void CanPlacePiece(Pieces *pieces, Grid *grid, gamestate *gstate) {
  if (!DoPiecesFit(*grid, *pieces, 1))
    *gstate = game_over; // only 1 level of look ahead
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

void BuildPiece(Piece *piece, int prob) {
  bool is_empty;
  do {
    is_empty = true;
    for (int col = 0; col < piece_length; col++) {
      for (int row = 0; row < piece_length; row++) {
        int rnd = rand() % 100;
        bool b = rnd < prob;
        if (b)
          is_empty = false;
        piece->shape[col][row] = b;
      }
    }
  } while (is_empty);

  while (IsTopRowEmpty(piece->shape)) {
    RemoveTopRow(piece->shape);
  }
  while (IsLeftColEmpty(piece->shape)) {
    RemoveLeftCol(piece->shape);
  }
  piece->drag.dragging = false;
}

CanvasPos GetPieceHomePos(int piece_idx) {
  return GridToCanvas(
      (GridPos){(piece_idx / pieces_per_grid_length * piece_length),
                (piece_idx % pieces_per_grid_length) * piece_length});
}

GridPos GetShadowPos(ScreenPos drag_offset, Grid *grid) {
  ScreenPos effective_mouse_pos = {GetMouseX() +
                                       squareLength / 2, // draw shadow
                                   GetMouseY() + squareLength / 2};
  return CanvasToGrid(ScreenToCanvas(
      SubScreenPos(effective_mouse_pos, drag_offset), grid->canvas));
}

bool HasColorBeenUsed(const Pieces *pieces, int cur_idx, uint8_t col_idx) {
  for (int j = 0; j != cur_idx; j++) {
    if (pieces->arr[j].pal_idx == col_idx) {
      return true;
    }
  }
  return false;
}

void BuildPieces(Pieces *pieces, Grid *grid) {
  int attempts = 0;
  int prob = square_probability_max;
  while (true) {
    attempts++;
    if (attempts % 10 == 0 && prob > square_probability_min)
      prob--;
    for (int i = 0; i != num_pieces; i++) {
      BuildPiece(&pieces->arr[i], prob);
      uint8_t c;
      do {
        c = rand() % (ARRAY_LENGTH(palette) - 1) + 1; // dont chose empty
      } while (HasColorBeenUsed(pieces, i, c));
      pieces->arr[i].pal_idx = c;
    }
    if (DoPiecesFit(*grid, *pieces, INT_MAX))
      break;
  }
  printf("attempts = %d\n", attempts);
}

bool IsPiecesEmpty(const Pieces *pieces) {
  for (int i = 0; i != num_pieces; i++) {
    if (pieces->arr[i].pal_idx != 0)
      return false;
  }
  return true;
}

void RefillPieces(Pieces *pieces, Grid *grid) {
  if (IsPiecesEmpty(pieces))
    BuildPieces(pieces, grid);
}

void GridInit(Grid *grid) {
  grid->canvas = (Canvas){{0, 0}};
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      grid->arr[col][row] = 0; // empty index = 0
    }
  }
}

bool DropPiece(Piece *piece, GridPos gpos, Grid *grid) {
  if (!DoesPieceFit(piece, gpos, grid))
    return false;
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (!piece->shape[col][row])
        continue;
      GridPos rec_pos = AddGridPos(gpos, (GridPos){col, row});
      grid->arr[rec_pos.x][rec_pos.y] = piece->pal_idx;
    }
  }
  piece->pal_idx = 0;
  return true;
}
void RenderGrid(Grid *grid) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      Rectangle rec = SceenToRectangle(
          CanvasToScreen(GridToCanvas((GridPos){col, row}), grid->canvas));
      Color c;
      c = palette[grid->arr[col][row]];
      DrawRectangleRec(rec, c);
    }
  }
}
void DrawPiece(const Piece *piece, ScreenPos pos, int a, int scale_factor) {
  Canvas mc = {(ScreenPos){0, 0}};
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (piece->shape[col][row]) {
        Rectangle rec =
            SceenToRectangle(AddScreenPos(
                               pos, GridToScreen((GridPos){col, row}, mc)));
        Color c = palette[piece->pal_idx];
        c.a = a;
        DrawRectangleRec(rec, c);
      }
    }
  }
}

void DrawPieceAtHome(const Piece *piece, ScreenPos pos) {
  Canvas mc = {(ScreenPos){0, 0}};
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (piece->shape[col][row]) {
        Rectangle rec = SceenToRectangleAtHome(
            AddScreenPos(pos, GridToScreenAtHome((GridPos){col, row}, mc)));
        Color c = palette[piece->pal_idx];
        DrawRectangleRec(rec, c);
      }
    }
  }
}

void DrawPieces(Pieces *pieces, Grid *grid) {
  ScreenPos mousePos = {GetMouseX(), GetMouseY()};
  int drag_idx = -1;
  for (int i = 0; i != num_pieces; i++) {
    if (pieces->arr[i].pal_idx == 0)
      continue;
    if (pieces->arr[i].drag.dragging) {
      drag_idx = i; // save idx of drag pieces for later drawing
    } else {
      ScreenPos s = CanvasToScreen(GetPieceHomePos(i), pieces->canvas);
      DrawPieceAtHome(&pieces->arr[i], s);
    }
  }
  if (drag_idx != -1) {
    GridPos gpos = GetShadowPos(pieces->arr[drag_idx].drag.offset, grid);
    if (DoesPieceFit(&pieces->arr[drag_idx], gpos, grid)) {
      DrawPiece(&pieces->arr[drag_idx], GridToScreen(gpos, grid->canvas),
                transparency, 100);
    }

    ScreenPos piece_pos = SubScreenPos(
        mousePos, pieces->arr[drag_idx].drag.offset); // draw dragging piece
    DrawPiece(&pieces->arr[drag_idx], piece_pos, MAX_A, 100);
  }
}

void OnMouseClick(Pieces *pieces) {
  if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    return;
  ScreenPos mousePos = {GetMouseX(), GetMouseY()};
  for (int i = 0; i != num_pieces; i++) {
    if (pieces->arr[i].pal_idx == 0)
      continue;
    ScreenPos p_origin = CanvasToScreen(GetPieceHomePos(i), pieces->canvas);
    if (MouseCollisionDetected(
            mousePos, p_origin,
            (Size){piece_length * squareLength, piece_length * squareLength})) {
      pieces->arr[i].drag.dragging = true;
      pieces->arr[i].drag.offset = SubScreenPos(mousePos, p_origin);
      return;
    }
  }
}

void OnMouseRelease(Pieces *pieces, Grid *grid) {
  for (int i = 0; i != num_pieces; i++) {
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) &&
        pieces->arr[i].drag.dragging) {
      if (pieces->arr[i].drag.dragging) {
        GridPos gpos = GetShadowPos(pieces->arr[i].drag.offset, grid);
        DropPiece(&pieces->arr[i], gpos, grid);
        pieces->arr[i].drag.dragging = false;
        return;
      }
    }
  }
}

int main(void) {
  srand(1);
  InitWindow(800, 600, "BlockGame");
  // ToggleFullscreen();
  squareLength = CalculateSquareSize();
  int screen_width = squareLength * (cols + num_piece_rows * piece_length);
  int screen_height = squareLength * rows;
  SetWindowSize(screen_width, screen_height);
  SetTargetFPS(60);

  Grid grid;
  Pieces pieces;
  pieces.canvas = (Canvas){{GridToCanvas((GridPos){cols, 0}).x, 0}};
  bool stop = false;
  GridInit(&grid);
  BuildPieces(&pieces, &grid);
  gamestate gstate = playing;
  int fade = 0;
  while (!WindowShouldClose() && !stop) {
    switch (gstate) {
    case playing:
      OnMouseClick(&pieces);
      OnMouseRelease(&pieces, &grid);
      ClearSquares(&grid);
      RefillPieces(&pieces, &grid);
      CanPlacePiece(&pieces, &grid, &gstate);

      BeginDrawing();
      ClearBackground((Color){46, 46, 46, 255});
      RenderGrid(&grid);
      DrawPieces(&pieces, &grid);
      EndDrawing();
      break;
    case game_over:
      BeginDrawing();
      ClearBackground((Color){46, 46, 46, 255});
      RenderGrid(&grid);
      DrawPieces(&pieces, &grid);
      DrawRectangleRec((Rectangle){0, 0, screen_width, screen_height},
                       (Color){0, 0, 0, fade});
      EndDrawing();
      if (fade < MAX_A - 1)
        fade += 2;
      break;
    default:
      exit(1);
    }
  }
  CloseWindow();
  return 0;
}
