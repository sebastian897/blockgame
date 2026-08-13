#include <limits.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(PLATFORM_DESKTOP)
#include "raygui.h"
#include "../styles/jungle/style_jungle.h"
#else
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "style_jungle.h"
#endif

#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

int rows = 9;
int cols = 9;
int piece_length = 3;
int num_pieces = 3;
int pieces_per_grid_length;
int screen_width;
int screen_height;
bool pieces_at_bottom;

#define GRID_IDX(col, row) ((col) * rows + (row))
#define PIECE_IDX(col, row) ((col) * piece_length + (row))

enum {
  square_probability_min = 20,
  transparency = 127,
  phone_bottom_offset = 1,
  fps = 60,
  combo_time = fps * 10
};

typedef enum gamestate { menu = 0, playing, game_over } gamestate;

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

void shuffle_pal_idxs(uint8_t pal_idxs[ARRAY_LENGTH(palette) - 1], int n) {
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

typedef struct PixelSize {
  int width;
  int height;
} PixelSize;

typedef struct GridSize {
  int width;
  int height;
} GridSize;

typedef struct Canvas {
  ScreenPos origin;
  PixelSize size;
  struct Canvas* next;
} Canvas;

typedef struct Grid {
  uint8_t* arr;
  Canvas canvas;
} Grid;

typedef struct Score {
  int val;
  Canvas canvas;
  int combo_timer;
  int combo;
  int score_text_timer;
  int temp_score;
  int combo_lost_timer;
} Score;

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

typedef struct Move {
  GridPos pos;
  int p_idx;
} Move;

typedef struct Moves {
  Move* arr;
} Moves;

typedef struct Hint {
  Canvas canvas;
  bool hint;
} Hint;

typedef struct Game {
  Score score;
  Grid grid;
  Pieces pieces;
  Moves moves;
  gamestate gstate;
  Hint hint;
  int fade;
  int square_probability;
} Game;

typedef struct MenuOp {
  int rows;
  int cols;
  int num_pieces;
  int piece_length;
  int square_probability;
  char* button_label;
} MenuOp;




MenuOp menu_ops[] = {{9, 9, 3, 3, 75, "Normal"}, {15, 4, 4, 3, 75, "Slim"}, {5, 5, 3, 3, 35, "Small"}, {15, 15, 3, 3, 45, "Strategy"}};

Canvas origin = {{0, 0}, {0, 0}, NULL};
Canvas screen_canvas = {0};

void ShiftCanvas(Canvas* head, ScreenPos offest) {
  Canvas* curr = head;
  while (curr != NULL) {
    curr->origin.x += offest.x;
    curr->origin.y += offest.y;
    curr = curr->next;
  }
}

Canvas* PrependCanvas(Canvas* head, Canvas* new) {
  new->next = head;
  return new;
}

PixelSize GetCanvasSize(const Canvas* canvas) {
  PixelSize max_size = {0};
  const Canvas* curr = canvas;
  while (curr != NULL) {
    PixelSize total_size = {curr->size.width + (curr->origin.x - canvas->origin.x),
                            curr->size.height + (curr->origin.y - canvas->origin.y)};
    if (total_size.width > max_size.width) max_size.width = total_size.width;
    if (total_size.height > max_size.height) max_size.height = total_size.height;
    curr = curr->next;
  }
  return max_size;
}

void CenterCanvas(const Canvas* c1, bool c1_all_canvases, Canvas* c2, bool c2_all_canvases, bool x,
                  bool y) {
  PixelSize c1_size = c1_all_canvases ? GetCanvasSize(c1) : c1->size;
  PixelSize c2_size = c2_all_canvases ? GetCanvasSize(c2) : c2->size;
  ScreenPos offset = {x ? c1->origin.x + (c1_size.width - c2_size.width) / 2 : 0,
                      y ? c1->origin.y + (c1_size.height - c2_size.height) / 2 : 0};
  ShiftCanvas(c2, offset);
}

Canvas* StackAbove(Canvas* top, Canvas* bot) {
  int dy = bot->origin.y - top->origin.y;
  int max_height = GetCanvasSize(top).height;

  Canvas* curr = bot;
  while (curr != NULL) {
    curr->origin.y -= dy;
    curr->origin.y += max_height;
    curr = curr->next;
  }
  PrependCanvas(bot, top);
  return top;
}

Canvas* StackLeft(Canvas* left, Canvas* right) {
  int dx = right->origin.x - left->origin.x;
  int max_width = GetCanvasSize(left).width;

  Canvas* curr = right;
  while (curr != NULL) {
    curr->origin.x -= dx;
    curr->origin.x += max_width;
    curr = curr->next;
  }
  PrependCanvas(right, left);
  return left;
}

PixelSize VectorToPixel(Vector2 v) {
  return (PixelSize){v.x, v.y};
}

PixelSize GridToPixel(GridSize size) {
  return (PixelSize){size.width * squareLength, size.height * squareLength};
}

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
bool EqualGridPos(GridPos gp1, GridPos gp2) {
  return gp1.x == gp2.x && gp1.y == gp2.y;
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
  if (screenHeight > screenWidth) {  // rows+1 for score canvas
    pieces_per_grid_length = cols / piece_length;
    int num_piece_rows = CEIL_DIV(num_pieces, pieces_per_grid_length);
    int maxTall = screenHeight / ((rows + 1) + num_piece_rows * piece_length);
    int maxWide = screenWidth / (cols);
    squareLength = (maxTall < maxWide ? maxTall : maxWide);
    screen_width = squareLength * cols;
    screen_height = squareLength * ((rows + 1) + num_piece_rows * piece_length);
    pieces_at_bottom = true;
  } else {
    pieces_per_grid_length = rows / piece_length;
    int num_piece_cols = CEIL_DIV(num_pieces, pieces_per_grid_length);
    int maxTall = screenHeight / (rows + 1);
    int maxWide = screenWidth / (cols + num_piece_cols * piece_length);
    squareLength = (maxTall < maxWide ? maxTall : maxWide);
    screen_width = squareLength * (cols + num_piece_cols * piece_length);
    screen_height = squareLength * (rows + 1);
    pieces_at_bottom = false;
  }

#if defined(PLATFORM_DESKTOP)
  SetWindowSize(screen_width, screen_height);
#endif
}

void PositionCanvases(Grid* grid, Pieces* pieces, Score* score, Hint* hint) {
  hint->canvas = (Canvas){{0}, {squareLength/2.0, squareLength/2.0}, NULL};
  score->canvas = (Canvas){{0}, GridToPixel((GridSize){cols, 1}), NULL};
  grid->canvas = (Canvas){{0}, GridToPixel((GridSize){cols, rows}), NULL};
  if (pieces_at_bottom) {
    pieces->canvas = (Canvas){
        {0},
        GridToPixel((GridSize){cols, CEIL_DIV(num_pieces, pieces_per_grid_length) * piece_length}),
        NULL};
    Canvas* total_canvas = StackAbove(&score->canvas, StackAbove(&grid->canvas, &pieces->canvas));
    CenterCanvas(&screen_canvas, true, total_canvas, true, true, true);    
    CenterCanvas(&score->canvas, false, &hint->canvas, false, false, true);
  } else {
    pieces->canvas =
        (Canvas){{0},
                 GridToPixel((GridSize){num_pieces / pieces_per_grid_length * piece_length, rows}),
                 NULL};
    Canvas* total_canvas = StackAbove(&score->canvas, StackLeft(&grid->canvas, &pieces->canvas));
    CenterCanvas(&screen_canvas, true, total_canvas, true, true, true);
    CenterCanvas(&score->canvas, false, &hint->canvas, false, false, true);
  }
}

bool MouseCollisionDetected(ScreenPos mouse, ScreenPos objpos, PixelSize size) {
  if (mouse.x > objpos.x && mouse.x < objpos.x + size.width && mouse.y > objpos.y &&
      mouse.y < objpos.y + size.height)
    return true;
  return false;
}

bool DoesCoordFit(int coord, int bound, int size) {
  if (coord < 0 || coord > bound - size) return false;
  return true;
}

PixelSize GetPieceSize(const Piece* piece) {
  PixelSize size = {0, 0};
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

bool IsSpaceFree(const Grid* grid, const Piece* piece, GridPos gpos) {
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (!piece->shape[PIECE_IDX(col, row)]) continue;
      GridPos rec_pos = AddGridPos(gpos, (GridPos){col, row});
      if (grid->arr[GRID_IDX(rec_pos.x, rec_pos.y)]) return false;
    }
  }
  return true;
}

bool DoesPieceFit(const Grid* grid, const Piece* piece, GridPos gpos) {
  PixelSize size = GetPieceSize(piece);
  return DoesCoordFit(gpos.x, cols, size.width) && DoesCoordFit(gpos.y, rows, size.height) &&
         IsSpaceFree(grid, piece, gpos);
}

void IncreaseCombo(Score* score) {
  if (score->combo_timer > 0) {
    score->combo += 1;
  }
  score->combo_timer = combo_time;
}

void UpdateCombo(Score* score) {
  if (score->combo_timer < 0) {
    if (score->combo > 1) score->combo_lost_timer = fps * 1.5;
    score->combo = 0;
    score->combo_timer = combo_time;
  }
}

void UpdateScore(Score* score, int squares_cleared) {
  if (squares_cleared > 0) {
    IncreaseCombo(score);
    int val = squares_cleared * score->combo;
    score->temp_score = val;
    score->val += val;
    score->score_text_timer = fps * 1.5;
  }
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

int ClearSquares(Grid* grid) {
  bool* full_cols = calloc(cols, sizeof(*full_cols));
  GetFullCols(grid, full_cols);
  bool* full_rows = calloc(rows, sizeof(*full_rows));
  GetFullRows(grid, full_rows);
  int squares_cleared = 0;
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      if (full_cols[col] || full_rows[row]) {
        grid->arr[GRID_IDX(col, row)] = 0;
        squares_cleared++;
      }
    }
  }
  free(full_rows);
  free(full_cols);
  return squares_cleared;
}

Grid GridCreate() {
  Grid grid;
  grid.arr = malloc(cols * rows * sizeof(*grid.arr));
  return grid;
}

void GridDestroy(Grid* grid) {
  if (grid->arr != NULL) {
    free(grid->arr);
    grid->arr = NULL;
  }
}

Grid GridCopy(const Grid* src) {
  Grid dst = *src;
  uint8_t* new_arr = malloc(cols * rows * sizeof(*src->arr));
  memcpy(new_arr, src->arr, cols * rows * sizeof(*src->arr));
  dst.arr = new_arr;
  return dst;
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
  if (p->arr) {
    free(p->arr);
    p->arr = NULL;
  }
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

Moves MovesCreate(void) {
  Moves m = {0};
  size_t total = num_pieces * sizeof(m.arr[0]);

  // one big allocation
  void* block = malloc(total);
  if (!block) {
    fprintf(stderr, "malloc failed\n");
    exit(1);
  }

  m.arr = block;
  return m;
}

void MovesDestroy(Moves* m) {
  if (m->arr) {
    free(m->arr);
    m->arr = NULL;
  }
}

Moves MovesCopy(const Moves* src) {
  Moves dst = *src;

  size_t total = num_pieces * sizeof(src->arr[0]);

  void* block = malloc(total);
  if (!block) {
    perror("malloc");
    exit(1);
  }

  memcpy(block, src->arr, total);
  dst.arr = block;
  return dst;
}

Move EmptyMove(void) {
  Move move = {0};
  move.p_idx = -1;
  return move;
}

Hint HintCreate(void) {
  Hint hint = {0};
  return hint;
}

bool EqualMove(Move m1, Move m2) {
  return m1.p_idx == m2.p_idx && EqualGridPos(m1.pos, m2.pos);
}

void EmptyMoves(Moves* m) {
  for (int i = 0; i > num_pieces; i ++) {
    m->arr[i].p_idx = -1;
  }
}

int FindFirstMove(Moves* m) {
  // printf("hello\n");
  for (int i = num_pieces-1; i >= 0; i--) {
    // printf("i=%d\n", i);
    // printf("p_idx=%d\n", m->arr[i].p_idx);
    if (m->arr[i].p_idx>=0) return i; 
  }
  return -1;
}

int DropPiece(Grid* grid, Piece* piece, GridPos gpos);

bool IsPiecesEmpty(const Pieces* pieces);

bool DoPiecesFit(const Grid* grid_ptr, const Pieces* pieces_ptr, Moves* moves_ptr, int rem_levels) {
  if (IsPiecesEmpty(pieces_ptr) || rem_levels == 0) return true;
  for (int p_idx = 0; p_idx != num_pieces; p_idx++) {
    if (pieces_ptr->arr[p_idx].pal_idx == 0) continue;
    for (int col = 0; col != cols; col++) {
      for (int row = 0; row != rows; row++) {
        GridPos gpos = {col, row};
        if (!DoesPieceFit(grid_ptr, &pieces_ptr->arr[p_idx], gpos)) continue;
        Grid grid = GridCopy(grid_ptr);
        Pieces pieces = PiecesCopy(pieces_ptr);
        
        // Moves moves = MovesCopy(moves_ptr);
        // printf("rem_levels = %d\n", rem_levels);
        if (moves_ptr!=NULL) {
          int m_idx = rem_levels-1;
          moves_ptr->arr[m_idx].p_idx = p_idx;
          moves_ptr->arr[m_idx].pos = gpos;
        }
        DropPiece(&grid, &pieces.arr[p_idx], gpos);
        bool success = DoPiecesFit(&grid, &pieces, moves_ptr, rem_levels - 1);

        PiecesDestroy(&pieces);
        GridDestroy(&grid);
        if (success) return true;
      }
    }
  }
  return false;
}

void CanPlacePiece(Grid* grid, Pieces* pieces, gamestate* gstate) {
  if (!DoPiecesFit(grid, pieces, NULL, 1)) *gstate = game_over;  // only 1 level of look ahead
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

void BuildPieces(Grid* grid, Pieces* pieces, Moves* moves, int prob) {
  int attempts = 0;
  uint8_t pal_idxs[ARRAY_LENGTH(palette) - 1];
  shuffle_pal_idxs(pal_idxs, num_pieces);
  while (true) {
    attempts++;
    if (attempts % 10 == 0 && prob > square_probability_min) prob--;
    for (int i = 0; i != num_pieces; i++) {
      BuildPiece(&pieces->arr[i], prob);
      pieces->arr[i].pal_idx = pal_idxs[i];
    }
    if (DoPiecesFit(grid, pieces, moves, num_pieces)) break;
  }
  printf("attempts = %d\n", attempts);
  // for (int i = 0; i != num_pieces; i++) {
  //   printf("move: idx=%d pos=(%d,%d)\n", moves->arr[i].p_idx, moves->arr[i].pos.x, moves->arr[i].pos.y);
  // }
}

bool IsPiecesEmpty(const Pieces* pieces) {
  for (int i = 0; i != num_pieces; i++) {
    if (pieces->arr[i].pal_idx != 0) return false;
  }
  return true;
}

void RefillPieces(Grid* grid, Pieces* pieces, Moves* moves, int prob) {
  if (IsPiecesEmpty(pieces)) {
    BuildPieces(grid, pieces, moves, prob);
  }
}

void GridInit(Grid* grid) {
  for (int col = 0; col != cols; col++) {
    for (int row = 0; row != rows; row++) {
      grid->arr[GRID_IDX(col, row)] = 0;  // empty index = 0
    }
  }
}

int DropPiece(Grid* grid, Piece* piece, GridPos gpos) {
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (!piece->shape[PIECE_IDX(col, row)]) continue;
      GridPos rec_pos = AddGridPos(gpos, (GridPos){col, row});
      grid->arr[GRID_IDX(rec_pos.x, rec_pos.y)] = piece->pal_idx;
    }
  }
  piece->pal_idx = 0;
  return ClearSquares(grid);
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
  for (int col = 0; col != piece_length; col++) {
    for (int row = 0; row != piece_length; row++) {
      if (piece->shape[PIECE_IDX(col, row)]) {
        ScreenPos rec_pos = AddScreenPos(piece_pos, GridToScreen((GridPos){col, row}, origin));
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

void DrawPieces(Grid* grid, Pieces* pieces) {
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
    if (DoesPieceFit(grid, &pieces->arr[drag_idx], gpos)) {
      DrawPiece(&pieces->arr[drag_idx], GridToScreen(gpos, grid->canvas), transparency, 1);
    }

    DrawPiece(&pieces->arr[drag_idx], piece_pos, MAX_A, 1);
  }
}

void DrawHint(Grid* grid, Pieces* pieces, Moves* moves, bool hint) {
  if (!hint) return;
  int move_idx = FindFirstMove(moves);
  if (move_idx<0) {
    // printf("move not found");
    return;
  }
  // printf("Gp: (%d, %d)\n", moves->arr[move_idx].pos.x, moves->arr[move_idx].pos.y);
  DrawPiece(&pieces->arr[moves->arr[move_idx].p_idx], GridToScreen(moves->arr[move_idx].pos, grid->canvas), transparency, 1);
}

void DrawScore(Grid* grid, Score* score) {
  const char* score_text = TextFormat("%d", score->val);
  const char* temp_score_text = TextFormat(" +%d", score->temp_score);
  const char* combo_text = TextFormat(" x%d", score->combo);
  const char* combo_lost_text = TextFormat("x1");
  Vector2 score_text_size =
      MeasureTextEx(GetFontDefault(), score_text, squareLength, squareLength / 10.0);
  Vector2 temp_score_text_size =
      MeasureTextEx(GetFontDefault(), temp_score_text, squareLength / 2.0, squareLength / 10.0);
  Vector2 combo_text_size =
      MeasureTextEx(GetFontDefault(), combo_text, squareLength / 2.0, squareLength / 10.0);

  Canvas score_text_canvas = {{0}, VectorToPixel(score_text_size), NULL};
  CenterCanvas(&grid->canvas, false, &score_text_canvas, false, true, false);
  CenterCanvas(&score->canvas, false, &score_text_canvas, false, false, true);

  Canvas temp_score_text_canvas = {{0}, VectorToPixel(temp_score_text_size), NULL};
  Canvas combo_text_canvas = {{0}, VectorToPixel(combo_text_size), NULL};
  StackLeft(&score_text_canvas, StackLeft(&temp_score_text_canvas, &combo_text_canvas));
  CenterCanvas(&score->canvas, false, &temp_score_text_canvas, true, false, true);

  DrawText(score_text, score_text_canvas.origin.x,
           score_text_canvas.origin.y + squareLength * (1 - squareAmount), squareLength, LIGHTGRAY);

  if (score->score_text_timer > 0) {
    score->combo_lost_timer = 0;
    DrawText(temp_score_text, temp_score_text_canvas.origin.x, temp_score_text_canvas.origin.y,
             squareLength / 2, GREEN);
    if (score->combo > 1) {
      DrawText(combo_text, combo_text_canvas.origin.x, combo_text_canvas.origin.y, squareLength / 2,
               MAROON);
    }
    score->score_text_timer--;
  } else if (score->combo_lost_timer > 0) {
    DrawText(combo_lost_text, combo_text_canvas.origin.x, combo_text_canvas.origin.y,
             squareLength / 2, MAROON);
    score->combo_lost_timer--;
  }
}

void OnMouseClick(Pieces* pieces) {
  if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
  ScreenPos mousePos = {GetMouseX(), GetMouseY()};
  for (int i = 0; i != num_pieces; i++) {
    if (pieces->arr[i].pal_idx == 0) continue;
    ScreenPos p_origin = CanvasToScreen(GetPieceHomePos(i), pieces->canvas);
    if (MouseCollisionDetected(
            mousePos, p_origin,
            (PixelSize){piece_length * squareLength, piece_length * squareLength})) {
      pieces->arr[i].drag.dragging = true;
      pieces->arr[i].drag.origin = mousePos;
      return;
    }
  }
}

bool OnMouseRelease(Grid* grid, Pieces* pieces, Moves* moves, int* squares_cleared, bool* hint) {
  bool dropped = false;
  if (!IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) return dropped;
  for (int p_idx = 0; p_idx != num_pieces; p_idx++) {
    if (pieces->arr[p_idx].drag.dragging) {
      ScreenPos piece_pos = GetDraggingPiecePos(pieces, p_idx);
      GridPos gpos = GetShadowPos(piece_pos, grid);
      if (DoesPieceFit(grid, &pieces->arr[p_idx], gpos)) {
        *squares_cleared = DropPiece(grid, &pieces->arr[p_idx], gpos);
        Move move = (Move){gpos, p_idx};
        int m_idx = FindFirstMove(moves);
        if (EqualMove(move, moves->arr[m_idx])) {
          moves->arr[m_idx].p_idx = -1;
          if (m_idx==0) *hint = false;
        }
        dropped = true;
      }
      pieces->arr[p_idx].drag.dragging = false;
      return dropped;
    }
  }
  return dropped;
}

void GameInit(Game* game) {
  game->hint = HintCreate();
  game->fade = 0;
  game->score = (Score){0};
  game->grid = GridCreate();
  game->pieces = PiecesCreate();
  game->moves = MovesCreate();

#if defined(PLATFORM_DESKTOP)
  int x = GetMonitorWidth(GetCurrentMonitor()) * (1 - windowSize) / 2;
  int y = GetMonitorHeight(GetCurrentMonitor()) * (1 - windowSize) / 2;
  SetWindowPosition(x, y);
#endif
  ReCalc();
  screen_canvas.size = (PixelSize){GetScreenWidth(), GetScreenHeight()};
  PositionCanvases(&game->grid, &game->pieces, &game->score, &game->hint);
  GridInit(&game->grid);
  BuildPieces(&game->grid, &game->pieces, &game->moves, game->square_probability);
  game->score.combo_timer = combo_time;
}

void RenderMenu(Game* game) {
  PixelSize b_size = {GetScreenWidth() / 2, (GetScreenHeight() / 4 / ARRAY_LENGTH(menu_ops))};
  for (int b_idx = 0; b_idx != ARRAY_LENGTH(menu_ops); b_idx++) {
    MenuOp* op = &menu_ops[b_idx];
    if (GuiButton((Rectangle){b_size.width / 2.0,
                              (GetScreenHeight() - ((1 - b_idx) * b_size.height * 3)) / 2.0,
                              b_size.width, b_size.height},
                  menu_ops[b_idx].button_label)) {
      cols = op->cols;
      rows = op->rows;
      num_pieces = op->num_pieces;
      piece_length = op->piece_length;
      game->square_probability = op->square_probability;
      GameInit(game);
      game->gstate = playing;
    }
  }
}

void DrawHintButton(Hint* hint) {
  if (GuiButton((Rectangle){hint->canvas.origin.x, hint->canvas.origin.y, squareLength/2.0, squareLength/2.0}, "H")) {
    hint->hint = !hint->hint;
  }
}

int main(void) {
  srand(time(NULL));
  SetTargetFPS(fps);
#if defined(PLATFORM_ANDROID)
  InitWindow(0, 0, "BlockGame");
#else
  InitWindow(800, 600, "BlockGame");
#endif
  GuiLoadStyleJungle();
  GuiSetStyle(DEFAULT, TEXT_SIZE, 50);

  Game game = {0};
  bool stop = false;
  while (!WindowShouldClose() && !stop) {
    switch (game.gstate) {
      case menu:
        BeginDrawing();
        RenderMenu(&game);
        EndDrawing();
        break;
      case playing:
        OnMouseClick(&game.pieces);
        UpdateCombo(&game.score);
        int squares_cleared = 0;
        if (OnMouseRelease(&game.grid, &game.pieces, &game.moves, &squares_cleared, &game.hint.hint)) {
          // only check for gameover if sth actually changed
          CanPlacePiece(&game.grid, &game.pieces, &game.gstate);
        }
        UpdateScore(&game.score, squares_cleared);
        RefillPieces(&game.grid, &game.pieces, &game.moves, game.square_probability);

        BeginDrawing();
        ClearBackground((Color){46, 46, 46, 255});
        RenderGrid(&game.grid);
        DrawScore(&game.grid, &game.score);
        DrawHintButton(&game.hint);
        DrawHint(&game.grid, &game.pieces, &game.moves, game.hint.hint);
        DrawPieces(&game.grid, &game.pieces);
        EndDrawing();
        game.score.combo_timer--;
        break;
      case game_over:
        BeginDrawing();
        ClearBackground((Color){46, 46, 46, 255});
        RenderGrid(&game.grid);
        DrawPieces(&game.grid, &game.pieces);
        DrawRectangleRec((Rectangle){0, 0, screen_width, screen_height},
                         (Color){0, 0, 0, game.fade});
        EndDrawing();
        if (game.fade < MAX_A - 1) {
          game.fade += 2;
        } else {
          GridDestroy(&game.grid);
          PiecesDestroy(&game.pieces);
// #if defined(PLATFORM_DESKTOP)
//           SetWindowSize(800, 600);
// #endif
          game.gstate = menu;
        }
        break;
      default:
        exit(1);
    }
  }
  CloseWindow();
  // may already be freed in game over gstate, in which case double free is prevented in *Destroy
  // but in case user quit while playing or while on menu, free now.
  MovesDestroy(&game.moves);
  GridDestroy(&game.grid);
  PiecesDestroy(&game.pieces);
  return 0;
}
