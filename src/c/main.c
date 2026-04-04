#include <pebble.h>

#define CELL_SIZE 2
#define BATCH 2
static uint8_t fps = 15;

#define SETTINGS_KEY 1

typedef struct ClaySettings {
  GColor fg_color;
  GColor bg_color;
  uint16_t activation_time;
} ClaySettings;

static ClaySettings settings;

static Window *s_main_window;
static Layer *sand_layer;
static GRect bounds;
static int width;
static int height;
static int frame = 0;

static GFont font;
static char time_buf[8];

static int rows;
static int cols;

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define ABS(a) (a < 0 ? -a : a)

typedef enum CellType {
  CellTypeAir = 0,
  CellTypeSand,
  CellTypeBedrock,
} CellType;

typedef enum Direction {
  DirectionU,
  DirectionUR,
  DirectionR,
  DirectionDR,
  DirectionD,
  DirectionDL,
  DirectionL,
  DirectionUL,
  DirectionNone,
} Direction;

typedef struct Cell {
  CellType type;
} Cell;

static Cell bedrock = (Cell){.type = CellTypeBedrock};
static Cell *cells = NULL;
static Direction gravity = DirectionD;
static size_t len;

static AppTimer *frame_timer;
static AppTimer *activation_timer = NULL;
static bool is_resetting = false;

static void default_settings() {
  settings.fg_color = GColorIcterine;
  settings.bg_color = GColorBlack;
  settings.activation_time = 10000;
}

static Cell *get_cell(int x, int y) {
  if (x >= 0 && x < cols && y >= 0 && y < rows) {
    size_t idx = ((y + rows) % rows) * cols + ((x + cols) % cols);
    return &cells[idx];
  }
  return &bedrock;
}

static bool byte_get_bit(uint8_t *byte, uint8_t bit) {
  return ((*byte) >> bit) & 1;
}

static GColor get_pixel_color(GBitmapDataRowInfo info, GPoint point) {
#if defined(PBL_COLOR)
  return (GColor){.argb = info.data[point.x]};
#else
  uint8_t byte = point.x / 8;
  uint8_t bit = point.x % 8;
  return byte_get_bit(&info.data[byte], bit) ? GColorWhite : GColorBlack;
#endif
}

static void draw_time(Layer *layer, GContext *ctx) {
  graphics_context_set_text_color(ctx, settings.fg_color);
  graphics_draw_text(ctx, time_buf, font,
                     GRect(0, bounds.size.h / 2 - 20, bounds.size.w, 40),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void frame_redraw(Layer *layer, GContext *ctx) {
  if (is_resetting) {
    draw_time(layer, ctx);

    is_resetting = false;
    GBitmap *fb = graphics_capture_frame_buffer(ctx);
    for (int y = 0; y < bounds.size.h; y += CELL_SIZE) {
      GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
      for (int x = info.min_x; x <= info.max_x; x += CELL_SIZE) {
        GColor color = get_pixel_color(info, GPoint(x, y));
        // printf("%d %d", color.argb, settings.fg_color.argb);
        uint8_t compare = settings.fg_color.argb;
#ifdef PBL_BW
        if (settings.fg_color.argb != GColorBlack.argb &&
            settings.fg_color.argb != GColorWhite.argb) {
          compare = GColorWhite.argb;
        }
#endif
        bool is_solid = color.argb == compare;
        if (is_solid) {
          Cell *cell = get_cell(x / CELL_SIZE, y / CELL_SIZE);
          cell->type = CellTypeSand;
        }
      }
    }
    graphics_release_frame_buffer(ctx, fb);
  } else if (activation_timer) {
    size_t i = 0;
    graphics_context_set_fill_color(ctx, settings.fg_color);
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        Cell *cell = get_cell(x, y);
        if (cell->type != CellTypeAir) {
          graphics_fill_rect(
              ctx, GRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE), 0,
              0);
        }
      }
    }
  } else {
    draw_time(layer, ctx);
  }
}

static void move_cell(uint8_t x, uint8_t y, uint8_t x1, uint8_t y1, uint8_t x2,
                      uint8_t y2, uint8_t x3, uint8_t y3) {
  Cell *cell = get_cell(x, y);
  if (cell->type == CellTypeSand) {
    Cell *next = get_cell(x1, y1);
    if (next->type == CellTypeAir) {
      next->type = cell->type;
      cell->type = CellTypeAir;
    } else {
      next = get_cell(x2, y2);
      if (next->type == CellTypeAir) {
        next->type = cell->type;
        cell->type = CellTypeAir;
      } else {
        next = get_cell(x3, y3);
        if (next->type == CellTypeAir) {
          next->type = cell->type;
          cell->type = CellTypeAir;
        } else {
        }
      }
    }
  }
}

static void next_gen_d() {
  for (int y = rows - 1; y >= 0; y--) {
    if (frame % 2) {
      for (int x = 0; x < cols; x++) {
        move_cell(x, y, x, y + 1, x - 1, y + 1, x + 1, y + 1);
      }
    } else {
      for (int x = cols - 1; x >= 0; x--) {
        move_cell(x, y, x, y + 1, x - 1, y + 1, x + 1, y + 1);
      }
    }
  }
}

static void next_gen_dr() {
  for (int y = rows - 1; y >= 0; y--) {
    if (frame % 2) {
      for (int x = 0; x < cols; x++) {
        move_cell(x, y, x + 1, y + 1, x + 1, y, x, y + 1);
      }
    } else {
      for (int x = cols - 1; x >= 0; x--) {
        move_cell(x, y, x + 1, y + 1, x + 1, y, x, y + 1);
      }
    }
  }
}

static void next_gen_dl() {
  for (int y = rows - 1; y >= 0; y--) {
    if (frame % 2) {
      for (int x = 0; x < cols; x++) {
        move_cell(x, y, x - 1, y + 1, x - 1, y, x, y + 1);
      }
    } else {
      for (int x = cols - 1; x >= 0; x--) {
        move_cell(x, y, x - 1, y + 1, x - 1, y, x, y + 1);
      }
    }
  }
}

static void next_gen_u() {
  for (int y = 0; y < rows; y++) {
    if (frame % 2) {
      for (int x = 0; x < cols; x++) {
        move_cell(x, y, x, y - 1, x - 1, y - 1, x + 1, y - 1);
      }
    } else {
      for (int x = cols - 1; x >= 0; x--) {
        move_cell(x, y, x, y - 1, x - 1, y - 1, x + 1, y - 1);
      }
    }
  }
}

static void next_gen_l() {
  for (int x = 0; x < cols; x++) {
    if (frame % 2) {
      for (int y = 0; y < rows; y++) {
        move_cell(x, y, x - 1, y, x - 1, y - 1, x - 1, y + 1);
      }
    } else {
      for (int y = rows - 1; y > 0; y--) {
        move_cell(x, y, x - 1, y, x - 1, y - 1, x - 1, y + 1);
      }
    }
  }
}

static void next_gen_r() {
  for (int x = cols - 1; x >= 0; x--) {
    if (frame % 2) {
      for (int y = 0; y < rows; y++) {
        move_cell(x, y, x + 1, y, x + 1, y - 1, x + 1, y + 1);
      }
    } else {
      for (int y = rows - 1; y > 0; y--) {
        move_cell(x, y, x + 1, y, x + 1, y - 1, x + 1, y + 1);
      }
    }
  }
}

static void new_frame(void *data) {
  for (size_t i = 0; i < BATCH; i++) {
    switch (gravity) {
    case DirectionD:
      next_gen_d();
      break;
    case DirectionDR:
      next_gen_dr();
      break;
    case DirectionDL:
      next_gen_dl();
      break;
    case DirectionU:
    case DirectionUR:
    case DirectionUL:
      next_gen_u();
      break;
    case DirectionR:
      next_gen_r();
      break;
    case DirectionL:
      next_gen_l();
      break;
    case DirectionNone:
      // Don't update
      break;
    }
    frame++;
  }
  layer_mark_dirty(sand_layer);
  if (activation_timer) {
    frame_timer = app_timer_register(1000 / fps, new_frame, NULL);
  }
}

static void reset() {
  frame = 0;
  cols = width / CELL_SIZE;
  rows = height / CELL_SIZE;
  len = rows * cols;
  if (cells != NULL) {
    free(cells);
  }
  cells = calloc(len, sizeof(Cell));
  // for (size_t i = 0; i < len; i++) {
  //   uint8_t val = rand() % 256;
  //   val &= rand() % 256;
  //   cells[i] = val;
  // }
  // for (size_t i = 0; i < len; i++) {
  //   Cell *cell = &cells[i];
  //   if (rand() % 3 == 0) {
  //     cell->type = CellTypeSand;
  //   }
  // }
  // if (frame_timer == NULL) {
  //   new_frame(NULL);
  // }
}

static void accel_data_handler(AccelData *data, uint32_t num_samples) {
#ifdef DEMO
  gravity = DirectionD;
#else
  int16_t deadzone = 256;
  if (data->x > -deadzone && data->x < deadzone && data->y > -deadzone &&
      data->y < deadzone) {
    gravity = DirectionNone;
    return;
  }
  int16_t x = 12 * data->x - 5 * data->y;
  int16_t y = 5 * data->x + 12 * data->y;
  if (x > 0) {
    if (y > 0) {
      if (x > y) {
        gravity = DirectionR;
      } else {
        gravity = DirectionUR;
      }
    } else {
      if (x > -y) {
        gravity = DirectionDR;
      } else {
        gravity = DirectionD;
      }
    }
  } else {
    if (y > 0) {
      if (-x > y) {
        gravity = DirectionUL;
      } else {
        gravity = DirectionU;
      }
    } else {
      if (x < y) {
        gravity = DirectionL;
      } else {
        gravity = DirectionDL;
      }
    }
  }
#endif
}

static void end_sim() {
  gravity = DirectionNone;
  activation_timer = NULL;
  accel_data_service_unsubscribe();
  reset();
}

static void accel_tap_handler(AccelAxisType axis, int32_t count) {
  if (activation_timer) {
    app_timer_reschedule(activation_timer, settings.activation_time);
  } else {
    accel_data_service_subscribe(1, accel_data_handler);
    activation_timer =
        app_timer_register(settings.activation_time, end_sim, NULL);
    reset();
    is_resetting = true;
    new_frame(NULL);
  }
}

static void update_time(struct tm *tick_time, TimeUnits units_changed) {
  strftime(time_buf, sizeof(time_buf), clock_is_24h_style() ? "%H:%M" : "%I:%M",
           tick_time);
  layer_mark_dirty(sand_layer);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  bounds = layer_get_bounds(window_layer);
  sand_layer = layer_create(bounds);
  width = bounds.size.w;
  height = bounds.size.h;
  reset();

  font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);

  layer_add_child(window_layer, sand_layer);
  layer_set_update_proc(sand_layer, frame_redraw);

  tick_timer_service_subscribe(MINUTE_UNIT, update_time);
  time_t curr = time(NULL);
  struct tm *time = localtime(&curr);
  update_time(time, 0);
  accel_tap_service_subscribe(accel_tap_handler);
}

static void main_window_unload(Window *window) {
  layer_destroy(sand_layer);
  window_destroy(s_main_window);
  free(cells);
}

static void load_settings() {
  default_settings();
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *fg_color_t = dict_find(iter, MESSAGE_KEY_FGColor);
  if (fg_color_t) {
    settings.fg_color = GColorFromHEX(fg_color_t->value->int32);
  }
  Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_BGColor);
  if (bg_color_t) {
    settings.bg_color = GColorFromHEX(bg_color_t->value->int32);
    window_set_background_color(s_main_window, settings.bg_color);
  }
  Tuple *activation_time_t = dict_find(iter, MESSAGE_KEY_ActivationTime);
  if (activation_time_t) {
    settings.activation_time = (uint16_t)activation_time_t->value->int32;
  }
  save_settings();
  layer_mark_dirty(sand_layer);
}

static void init() {
  load_settings();
  s_main_window = window_create();
  window_set_background_color(s_main_window, settings.bg_color);
  window_set_window_handlers(s_main_window, (WindowHandlers){
                                                .load = main_window_load,
                                                .unload = main_window_unload,
                                            });
  window_stack_push(s_main_window, true);
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 128);
}

int main(void) {
  init();
  app_event_loop();
}
