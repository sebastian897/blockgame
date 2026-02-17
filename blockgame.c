#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "raylib.h"

#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

int rows = 15;
int cols = 4;
int piece_length = 3;
int num_pieces = 4;
int pieces_per_grid_length;
int screen_width;
int screen_height;
bool pieces_at_bottom;

#define GRID_IDX(col, row) ((col) * rows + (row))
#define PIECE_IDX(col, row) ((col) * piece_length + (row))

enum {
  square_probability_max = 75,
  square_probability_min = 20,
  transparency = 127,
  phone_bottom_offset = 1
};

typedef enum gamestate { menu, playing, game_over } gamestate;

const float windowSize = 0.8;
const float squareAmount = 0.95;
int squareLength;
const float piece_scale_factor = 0.8;

#define MAX_A 255
bool debug = true;
#define EMPTY (Color){30, 30, 30, 255}
Color palette[] = {EMPTY,  MAROON, ORANGE,  DARKGREEN, DARKBLUE, DARKPURPLE, DARKBROWN,
                   RED,    GOLD,   LIME,    BLUE,      VIOLET,   BROWN,      PINK,
                   YELLOW, GREEN,  SKYBLUE, PURPLE,    BEIGE};

void random_pal_idx(uint8_t pal_idxs[ARRAY_LENGTH(palette) - 1], int n) {
  int total = ARRAY_LENGTH(palette) - 1;
  for (int i = 0; i != total; i++) pal_idxs[i] = i + 1;
  for (int i = 0; i != n; i++) {
    int j = i + rand() % (total - i);

    uint8_t temp = pal_idxs[i];
    pal_idxs[i] = pal_idxs[j];
    pal_idxs[j] = temp;
  }
}

typedef bool* Shape;

typedef struct ScreenPos {
  int x;
  int y;
} ScreenPos;

typedef struct Canvas {
  ScreenPos origin;
} Canvas;

typedef struct Grid {
  uint8_t* arr;
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
  ScreenPos origin;
} Drag;

typedef struct Piece {
  uint8_t pal_idx;
  Shape shape;
  Drag drag;
} Piece;

typedef struct Pieces {
  Piece* arr;
  Canvas canvas;
} Pieces;

typedef struct Size {
  int width;
  int height;
} Size;

ScreenPos ScaleScreenPos(ScreenPos spos, float scale_factor) {
  return (ScreenPos){spos.x * scale_factor, spos.y * scale_factor};
}

CanvasPos ScaleCanvasPos(CanvasPos cpos, float scale_factor) {
  return (CanvasPos){cpos.x * scale_factor, cpos.y * scale_factor};
}

ScreenPos CanvasToScreen(CanvasPos cpos, Canvas c) {
  return (ScreenPos){cpos.x + c.origin.x, cpos.y + c.origin.y};
}
CanvasPos ScreenToCanvas(ScreenPos cpos, Canvas c) {
  return (CanvasPos){cpos.x - c.origin.x, cpos.y - c.origin.y};
}

CanvasPos GridToCanvas(GridPos gp) {
  return (CanvasPos){gp.x * squareLength, gp.y * squareLength};
}

ScreenPos GridToScreen(GridPos gp, Canvas c) {
  return CanvasToScreen(GridToCanvas(gp), c);
}

Rectangle ScreenToRectangle(ScreenPos sp) {
  return (Rectangle){sp.x, sp.y, squareLength * squareAmount, squareLength * squareAmount};
}

GridPos CanvasToGrid(CanvasPos cp) {
  if (cp.x < 0 || cp.y < 0) return (GridPos){-1, -1};  // ensure negatives dont get displayes
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

void SwapGrid(int width, int height) {
  if ((width > height && rows > cols) || (width < height && rows < cols)) {
    int temp = cols;
    cols = rows;
    rows = temp;
  }
}

void ReCalc(void) {
#if defined(PLATFORM_ANDROID)
  const int screenWidth = GetScreenWidth();
  const int screenHeight = GetScreenHeight();
#else
  const int screenWidth = GetMonitorWidth(GetCurrentMonitor()) * windowSize;
  const int screenHeight = GetMonitorHeight(GetCurrentMonitor()) * windowSize;
#endif
  SwapGrid(screenWidth, screenHeight);
  if (screenHeight > screenWidth) {
    pieces_per_grid_length = cols / piece_length;
    int num_piece_rows = CEIL_DIV(num_pieces, pieces_per_grid_length);
    int maxTall = screenHeight / (rows + num_piece_rows * piece_length);
    int maxWide = screenWidth / (cols);
    squareLength = (maxTall < maxWide ? maxTall : maxWide);
    screen_width = squareLength * cols;
    screen_height = squareLength * (rows + num_piece_rows * piece_length);
    pieces_at_bottom = true;
  } else {
    pieces_per_grid_length = rows / piece_length;
    int num_piece_cols = CEIL_DIV(num_pieces, pieces_per_grid_length);
    int maxTall = screenHeight / (rows);
    int maxWide = screenWidth / (cols + num_piece_cols * piece_length);
    squareLength = (maxTall < maxWide ? maxTall : maxWide);
    screen_width = squareLength * (cols + num_piece_cols * piece_length);
    screen_height = squareLength * rows;
    pieces_at_bottom = false;
  }
  SetWindowSize(screen_width, screen_height);
}

bool MouseCollisionDetected(ScreenPos mouse, ScreenPos objpos, Size size) {
  if (mouse.x > objpos.x && mouse.x < objpos.x + size.width && mouse.y > objpos.y &&
      mouse.y < objpos.y + size.height)
    return true;
  return false;
}

bool DoesCoordFit(int coord, int bound, int size) {
  if (coord < 0 || coord > bound - size) return false;
  return true;
}

Size GetPieceSize(const Piece* piece) {
  Size size = {0, 0};
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (piece->shape[PIECE_IDX(col, row)]) {
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

bool IsSpaceOccupied(const Piece* piece, GridPos gpos, Grid* grid) {
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (!piece->shape[PIECE_IDX(col, row)]) continue;
      GridPos rec_pos = AddGridPos(gpos, (GridPos){col, row});
      if (grid->arr[GRID_IDX(rec_pos.x, rec_pos.y)]) return false;
    }
  }
  return true;
}

bool DoesPieceFit(const Piece* piece, GridPos gpos, Grid* grid) {
  Size size = GetPieceSize(piece);
  return DoesCoordFit(gpos.x, cols, size.width) && DoesCoordFit(gpos.y, rows, size.height) &&
         IsSpaceOccupied(piece, gpos, grid);
}

void GetFullCols(const Grid* grid, bool* full_cols) {
  for (int col = 0; col != cols; col++) {
    int colCount = 0;
    for (int row = 0; row != rows; row++) {
      if (grid->arr[GRID_IDX(col, row)]) {
        colCount++;
      }
    }
    if (colCount == rows) {
      full_cols[col] = true;
    }
  }
}

void GetFullRows(const Grid* grid, bool* full_rows) {
  for (int row = 0; row != rows; row++) {
    int rowCount = 0;
    for (int col = 0; col != cols; col++) {
      if (grid->arr[GRID_IDX(col, row)]) {
        rowCount++;
      }
    }
    if (rowCount == cols) {
      full_rows[row] = true;
    }
  }
}

void ClearSquares(Grid* grid) {
  bool* full_cols = calloc(cols, sizeof(*full_cols));
  GetFullCols(grid, full_cols);
  bool* full_rows = calloc(rows, sizeof(*full_rows));
  GetFullRows(grid, full_rows);
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      if (full_cols[col] || full_rows[row]) {
        grid->arr[GRID_IDX(col, row)] = 0;
      }
    }
  }
  free(full_rows);
  free(full_cols);
}

Grid GridCopy(Grid grid) {
  uint8_t* new_arr = malloc(cols * rows * sizeof(*grid.arr));
  memcpy(new_arr, grid.arr, cols * rows * sizeof(*grid.arr));
  grid.arr = new_arr;
  return grid;
}

Pieces PiecesCopy(const Pieces* src) {
  Pieces dst = *src;

  size_t pieces_size = num_pieces * sizeof(src->arr[0]);
  size_t shapes_size = num_pieces * piece_length * piece_length * sizeof(*src->arr[0].shape);
  size_t total = pieces_size + shapes_size;

  void* block = malloc(total);
  if (!block) {
    perror("malloc");
    exit(1);
  }

  memcpy(block, src->arr, total);
  dst.arr = block;
  return dst;
}

Grid GridCreate() {
  Grid grid;
  grid.canvas = (Canvas){{0, 0}};
  grid.arr = malloc(cols * rows * sizeof(*grid.arr));
  return grid;
}

void GridDestroy(Grid* grid) {
  if (grid->arr != NULL) {
    free(grid->arr);
  }
}

Pieces PiecesCreate(void) {
  Pieces p = {0};
  size_t pieces_size = num_pieces * sizeof(p.arr[0]);
  size_t shapes_size = num_pieces * piece_length * piece_length * sizeof(*p.arr[0].shape);
  size_t total = pieces_size + shapes_size;

  // one big allocation
  void* block = malloc(total);
  if (!block) {
    fprintf(stderr, "malloc failed\n");
    exit(1);
  }

  p.arr = block;
  bool* shape_pool = (bool*)((char*)block + pieces_size);

  for (int i = 0; i < num_pieces; i++) {
    p.arr[i].shape = shape_pool + i * piece_length * piece_length;
  }
  return p;
}

void PiecesDestroy(Pieces* p) {
  free(p->arr);
  p->arr = NULL;
}

bool DropPiece(Piece* piece, GridPos gpos, Grid* grid);

bool IsPiecesEmpty(const Pieces* pieces);

bool DoPiecesFitRecurse(Grid* grid_ptr, Pieces* pieces_ptr, int rem_levels) {
  if (IsPiecesEmpty(pieces_ptr) || rem_levels == 0) return true;
  for (int p_idx = 0; p_idx != num_pieces; p_idx++) {
    if (pieces_ptr->arr[p_idx].pal_idx == 0) continue;
    for (int col = 0; col != cols; col++) {
      for (int row = 0; row != rows; row++) {
        GridPos gpos = {col, row};
        if (!DoesPieceFit(&pieces_ptr->arr[p_idx], gpos, grid_ptr)) continue;
        Grid grid = GridCopy(*grid_ptr);
        Pieces pieces = PiecesCopy(pieces_ptr);
        if (!DropPiece(&pieces.arr[p_idx], gpos, &grid)) {
          PiecesDestroy(&pieces);
          GridDestroy(&grid);
          continue;
        }
        pieces.arr[p_idx].pal_idx = 0;
        ClearSquares(&grid);
        bool success = DoPiecesFitRecurse(&grid, &pieces, rem_levels - 1);
        PiecesDestroy(&pieces);
        GridDestroy(&grid);
        if (success) return true;
      }
    }
  }
  return false;
}

bool DoPiecesFit(Grid* grid_ptr, Pieces* pieces_ptr, int rem_levels) {
  Grid grid = GridCopy(*grid_ptr);
  Pieces pieces = PiecesCopy(pieces_ptr);

  bool retval = DoPiecesFitRecurse(&grid, &pieces, rem_levels);

  PiecesDestroy(&pieces);
  GridDestroy(&grid);
  return retval;
}

void CanPlacePiece(Pieces* pieces, Grid* grid, gamestate* gstate) {
  if (!DoPiecesFit(grid, pieces, 1)) *gstate = game_over;  // only 1 level of look ahead
}

bool IsTopRowEmpty(Shape shape) {
  for (int col = 0; col != piece_length; col++)
    if (shape[PIECE_IDX(col, 0)]) return false;
  return true;
}
bool IsLeftColEmpty(Shape shape) {
  for (int row = 0; row != piece_length; row++)
    if (shape[PIECE_IDX(0, row)]) return false;
  return true;
}

void RemoveTopRow(Shape shape) {
  for (int row = 0; row != piece_length - 1; row++) {
    for (int col = 0; col != piece_length; col++) {
      shape[PIECE_IDX(col, row)] = shape[PIECE_IDX(col, row + 1)];
    }
  }
  for (int col = 0; col != piece_length; col++) {
    shape[PIECE_IDX(col, piece_length - 1)] = false;
  }
}

void RemoveLeftCol(Shape shape) {
  for (int col = 0; col != piece_length - 1; col++) {
    for (int row = 0; row != piece_length; row++) {
      shape[PIECE_IDX(col, row)] = shape[PIECE_IDX(col + 1, row)];
    }
  }
  for (int row = 0; row != piece_length; row++) {
    shape[PIECE_IDX(piece_length - 1, row)] = false;
  }
}

void BuildPiece(Piece* piece, int prob) {
  bool is_empty;
  do {
    is_empty = true;
    for (int col = 0; col < piece_length; col++) {
      for (int row = 0; row < piece_length; row++) {
        int rnd = rand() % 100;
        bool b = rnd < prob;
        if (b) is_empty = false;
        piece->shape[PIECE_IDX(col, row)] = b;
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
  if (pieces_at_bottom) {
    return GridToCanvas((GridPos){(piece_idx % pieces_per_grid_length) * piece_length,
                                  (piece_idx / pieces_per_grid_length * piece_length)});
  } else
    return GridToCanvas((GridPos){(piece_idx / pieces_per_grid_length * piece_length),
                                  (piece_idx % pieces_per_grid_length) * piece_length});
}

GridPos GetShadowPos(ScreenPos spos, Grid* grid) {
  ScreenPos effective_dropping_pos =
      AddScreenPos(spos, (ScreenPos){squareLength / 2,  // draw shadow
                                     +squareLength / 2});
  return CanvasToGrid(ScreenToCanvas(effective_dropping_pos, grid->canvas));
}

void BuildPieces(Pieces* pieces, Grid* grid) {
  int attempts = 0;
  int prob = square_probability_max;
  uint8_t pal_idxs[ARRAY_LENGTH(palette) - 1];
  random_pal_idx(pal_idxs, num_pieces);
  while (true) {
    attempts++;
    if (attempts % 10 == 0 && prob > square_probability_min) prob--;
    for (int i = 0; i != num_pieces; i++) {
      BuildPiece(&pieces->arr[i], prob);
      pieces->arr[i].pal_idx = pal_idxs[i];
    }
    if (DoPiecesFit(grid, pieces, INT_MAX)) break;
  }
  printf("attempts = %d\n", attempts);
}

bool IsPiecesEmpty(const Pieces* pieces) {
  for (int i = 0; i != num_pieces; i++) {
    if (pieces->arr[i].pal_idx != 0) return false;
  }
  return true;
}

void RefillPieces(Pieces* pieces, Grid* grid) {
  if (IsPiecesEmpty(pieces)) BuildPieces(pieces, grid);
}

void GridInit(Grid* grid) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      grid->arr[GRID_IDX(col, row)] = 0;  // empty index = 0
    }
  }
}

bool DropPiece(Piece* piece, GridPos gpos, Grid* grid) {
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (!piece->shape[PIECE_IDX(col, row)]) continue;
      GridPos rec_pos = AddGridPos(gpos, (GridPos){col, row});
      grid->arr[GRID_IDX(rec_pos.x, rec_pos.y)] = piece->pal_idx;
    }
  }
  piece->pal_idx = 0;
  return true;
}
void RenderGrid(Grid* grid) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      Rectangle rec =
          ScreenToRectangle(CanvasToScreen(GridToCanvas((GridPos){col, row}), grid->canvas));
      Color c;
      c = palette[grid->arr[GRID_IDX(col, row)]];
      DrawRectangleRec(rec, c);
    }
  }
}

Rectangle ScaleRectangle(Rectangle rect, ScreenPos centre, float scale_factor) {
  return (Rectangle){centre.x - scale_factor * (centre.x - rect.x),
                     centre.y - scale_factor * (centre.y - rect.y), rect.width * scale_factor,
                     rect.height * scale_factor};
}

ScreenPos GetPieceCentre(ScreenPos piece_top_left) {
  return AddScreenPos(piece_top_left, (ScreenPos){squareLength * piece_length / 2,
                                                  squareLength * piece_length / 2});
}

void DrawPiece(const Piece* piece, ScreenPos piece_pos, int a, float scale_factor) {
  Canvas mc = {(ScreenPos){0, 0}};
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (piece->shape[PIECE_IDX(col, row)]) {
        ScreenPos rec_pos = AddScreenPos(piece_pos, GridToScreen((GridPos){col, row}, mc));
        Rectangle unscaled_rect = ScreenToRectangle(rec_pos);
        ScreenPos centrepos = GetPieceCentre(piece_pos);
        Rectangle rec = ScaleRectangle(unscaled_rect, centrepos, scale_factor);
        Color c = palette[piece->pal_idx];
        c.a = a;
        DrawRectangleRec(rec, c);
      }
    }
  }
}

ScreenPos GetDraggingPiecePos(const Pieces* pieces, int drag_idx) {
#if defined(PLATFORM_ANDROID)
  float sf = 2;
#else
  float sf = 1;
#endif
  ScreenPos mousePos = {GetMouseX(), GetMouseY()};
  return AddScreenPos(
      CanvasToScreen(GetPieceHomePos(drag_idx), pieces->canvas),
      ScaleScreenPos(SubScreenPos(mousePos, pieces->arr[drag_idx].drag.origin), sf));
}

void DrawPieces(Pieces* pieces, Grid* grid) {
  int drag_idx = -1;
  for (int i = 0; i != num_pieces; i++) {
    if (pieces->arr[i].pal_idx == 0) continue;
    if (pieces->arr[i].drag.dragging) {
      drag_idx = i;  // save idx of drag pieces for later drawing
    } else {
      ScreenPos s = CanvasToScreen(GetPieceHomePos(i), pieces->canvas);
      DrawPiece(&pieces->arr[i], s, MAX_A, piece_scale_factor);
    }
  }
  if (drag_idx != -1) {
    ScreenPos piece_pos = GetDraggingPiecePos(pieces, drag_idx);
    GridPos gpos = GetShadowPos(piece_pos, grid);
    if (DoesPieceFit(&pieces->arr[drag_idx], gpos, grid)) {
      DrawPiece(&pieces->arr[drag_idx], GridToScreen(gpos, grid->canvas), transparency, 1);
    }

    DrawPiece(&pieces->arr[drag_idx], piece_pos, MAX_A, 1);
  }
}

void OnMouseClick(Pieces* pieces) {
  if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
  ScreenPos mousePos = {GetMouseX(), GetMouseY()};
  for (int i = 0; i != num_pieces; i++) {
    if (pieces->arr[i].pal_idx == 0) continue;
    ScreenPos p_origin = CanvasToScreen(GetPieceHomePos(i), pieces->canvas);
    if (MouseCollisionDetected(mousePos, p_origin,
                               (Size){piece_length * squareLength, piece_length * squareLength})) {
      pieces->arr[i].drag.dragging = true;
      pieces->arr[i].drag.origin = mousePos;
      return;
    }
  }
}

void OnMouseRelease(Pieces* pieces, Grid* grid) {
  for (int p_idx = 0; p_idx != num_pieces; p_idx++) {
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && pieces->arr[p_idx].drag.dragging) {
      if (pieces->arr[p_idx].drag.dragging) {
        ScreenPos piece_pos = GetDraggingPiecePos(pieces, p_idx);
        GridPos gpos = GetShadowPos(piece_pos, grid);
        if (DoesPieceFit(&pieces->arr[p_idx], gpos, grid))
          DropPiece(&pieces->arr[p_idx], gpos, grid);
        pieces->arr[p_idx].drag.dragging = false;
        return;
      }
    }
  }
}

int main(void) {
  srand(time(NULL));
#if defined(PLATFORM_ANDROID)
  InitWindow(0, 0, "BlockGame");
#else
  InitWindow(800, 600, "BlockGame");
  int x = GetMonitorWidth(GetCurrentMonitor()) * (1 - windowSize) / 2;
  int y = GetMonitorHeight(GetCurrentMonitor()) * (1 - windowSize) / 2;
  SetWindowPosition(x, y);
#endif
  ReCalc();
  SetTargetFPS(60);

  Grid grid = GridCreate();
  Pieces pieces = PiecesCreate();

  if (pieces_at_bottom) {
#if defined(PLATFORM_ANDROID)
    Canvas origin = {0, 0};
    printf("GetScreenHeight = %d\n", GetScreenHeight());
    ScreenPos canvas_offset = ScaleScreenPos(
        SubScreenPos(
            (ScreenPos){GetScreenWidth(), GetScreenHeight()},
            GridToScreen((GridPos){cols, num_pieces / pieces_per_grid_length * piece_length + rows},
                         origin)),
        0.5);

    grid.canvas = (Canvas){canvas_offset};
    pieces.canvas = (Canvas){AddScreenPos(canvas_offset, GridToScreen((GridPos){0, rows}, origin))};
#else
    pieces.canvas = (Canvas){0, GridToCanvas((GridPos){0, rows}).y};
#endif
  } else
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
        // stop = true;
        break;
      case game_over:
        BeginDrawing();
        ClearBackground((Color){46, 46, 46, 255});
        RenderGrid(&grid);
        DrawPieces(&pieces, &grid);
        DrawRectangleRec((Rectangle){0, 0, screen_width, screen_height}, (Color){0, 0, 0, fade});
        EndDrawing();
        if (fade < MAX_A - 1) fade += 2;
        break;
      default:
        exit(1);
    }
  }
  CloseWindow();
  GridDestroy(&grid);
  PiecesDestroy(&pieces);
  return 0;
}
