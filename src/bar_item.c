#include "bar_item.h"
#include "alias.h"
#include "graph_data.h"
#include "misc/helpers.h"
#include <stdint.h>
#include <string.h>

struct bar_item* bar_item_create() {
  struct bar_item* bar_item = malloc(sizeof(struct bar_item));
  memset(bar_item, 0, sizeof(struct bar_item));
  return bar_item;
}

void bar_item_inherit_from_item(struct bar_item* bar_item, struct bar_item* ancestor) {
  bar_item->lazy = ancestor->lazy;
  bar_item->updates = ancestor->updates;
  bar_item->drawing = ancestor->drawing;
  
  text_destroy(&bar_item->icon);
  text_destroy(&bar_item->label);
  
  bar_item->icon = ancestor->icon;
  bar_item->label = ancestor->label;
  text_clear_pointers(&bar_item->icon);
  text_clear_pointers(&bar_item->label);
  text_set_font(&bar_item->icon, string_copy(ancestor->icon.font_name), true);
  text_set_font(&bar_item->label, string_copy(ancestor->label.font_name), true);
  text_set_string(&bar_item->icon, string_copy(ancestor->icon.string), true);
  text_set_string(&bar_item->label, string_copy(ancestor->label.string), true);

  bar_item->update_frequency = ancestor->update_frequency;
  bar_item->cache_scripts = ancestor->cache_scripts;

  bar_item->background = ancestor->background;
  bar_item->y_offset = ancestor->y_offset;
}

void bar_item_init(struct bar_item* bar_item, struct bar_item* default_item) {
  bar_item->needs_update = true;
  bar_item->lazy = false;
  bar_item->drawing = true;
  bar_item->updates = true;
  bar_item->nospace = false;
  bar_item->selected = false;
  bar_item->counter = 0;
  bar_item->name = "";
  bar_item->type = BAR_ITEM;
  bar_item->update_frequency = 0;
  bar_item->cache_scripts = false;
  bar_item->script = "";
  bar_item->click_script = "";
  bar_item->position = BAR_POSITION_RIGHT;
  bar_item->associated_display = 0;
  bar_item->associated_space = 0;
  bar_item->associated_bar = 0;

  bar_item->y_offset = 0;
  bar_item->num_rects = 0;
  bar_item->bounding_rects = NULL;

  bar_item->has_alias = false;
  bar_item->has_graph = false;

  text_init(&bar_item->icon);
  text_init(&bar_item->label);
  background_init(&bar_item->background);
  
  if (default_item) bar_item_inherit_from_item(bar_item, default_item);

  strncpy(&bar_item->signal_args.name[0][0], "NAME", 255);
  strncpy(&bar_item->signal_args.name[1][0], "SELECTED", 255);
  strncpy(&bar_item->signal_args.value[1][0], "false", 255);
}

void bar_item_append_associated_space(struct bar_item* bar_item, uint32_t bit) {
  bar_item->associated_space |= bit;
  if (bar_item->type == BAR_COMPONENT_SPACE) {
    bar_item->associated_space = bit;
    char sid_str[32];
    sprintf(sid_str, "%u", get_set_bit_position(bit));
    strncpy(&bar_item->signal_args.value[2][0], sid_str, 255);
  }
}

void bar_item_append_associated_display(struct bar_item* bar_item, uint32_t bit) {
  bar_item->associated_display |= bit;
  if (bar_item->type == BAR_COMPONENT_SPACE) {
    bar_item->associated_display = bit;
    char did_str[32];
    sprintf(did_str, "%u", get_set_bit_position(bit));
    strncpy(&bar_item->signal_args.value[3][0], did_str, 255);
  }
}

bool bar_item_is_shown(struct bar_item* bar_item) {
  if (bar_item->associated_bar & UINT32_MAX) return true;
  else return false;
}

void bar_item_append_associated_bar(struct bar_item* bar_item, uint32_t bit) {
  bar_item->associated_bar |= bit;
}

void bar_item_remove_associated_bar(struct bar_item* bar_item, uint32_t bit) {
  bar_item->associated_bar &= ~bit; 
}

void bar_item_reset_associated_bar(struct bar_item* bar_item) {
  bar_item->associated_bar = 0;
}

bool bar_item_update(struct bar_item* bar_item, bool forced) {
  if (!bar_item->updates || (bar_item->update_frequency == 0 && !forced)) return false;
  bar_item->counter++;
  if (bar_item->update_frequency <= bar_item->counter || forced) {
    bar_item->counter = 0;

    // Script Update
    if (strlen(bar_item->script) > 0) {
      fork_exec(bar_item->script, &bar_item->signal_args);
    }

    // Alias Update
    if (bar_item->has_alias && bar_item_is_shown(bar_item)) {
      alias_update_image(&bar_item->alias);
      bar_item_needs_update(bar_item);
      return true;
    }
  }
  return false;
}

void bar_item_needs_update(struct bar_item* bar_item) {
  bar_item->needs_update = true;
}

void bar_item_clear_needs_update(struct bar_item* bar_item) {
  bar_item->needs_update = false;
}

void bar_item_set_name(struct bar_item* bar_item, char* name) {
  if (!name) return;
  if (bar_item->name && strcmp(bar_item->name, name) == 0) { free(name); return; }
  if (name != bar_item->name && !bar_item->name) free(bar_item->name);
  bar_item->name = name;
  strncpy(&bar_item->signal_args.value[0][0], name, 255);
}

void bar_item_set_type(struct bar_item* bar_item, char type) {
  bar_item->type = type;
  if (type == BAR_COMPONENT_SPACE) {
    bar_item->updates = false;
    strncpy(&bar_item->signal_args.name[2][0], "SID", 255);
    strncpy(&bar_item->signal_args.value[2][0], "0", 255);
    strncpy(&bar_item->signal_args.name[3][0], "DID", 255);
    strncpy(&bar_item->signal_args.value[3][0], "0", 255);
  }
  else if (type == BAR_COMPONENT_ALIAS) {
    bar_item->update_frequency = 1;
    bar_item->has_alias = true;
  }
}

void bar_item_set_script(struct bar_item* bar_item, char* script) {
  if (!script) return;
  if (bar_item->script && strcmp(bar_item->script, script) == 0) { free(script); return; }
  if (script != bar_item->script && !bar_item->script) free(bar_item->script);
  if (bar_item->cache_scripts && file_exists(resolve_path(script))) bar_item->script = read_file(resolve_path(script));
  else bar_item->script = script;
}

void bar_item_set_click_script(struct bar_item* bar_item, char* script) {
  if (!script) return;
  if (bar_item->click_script && strcmp(bar_item->click_script, script) == 0) { free(script); return; }
  if (script != bar_item->click_script && !bar_item->click_script) free(bar_item->click_script);
  if (bar_item->cache_scripts && file_exists(resolve_path(script))) bar_item->click_script = read_file(resolve_path(script));
  else bar_item->click_script = script;
}

void bar_item_set_drawing(struct bar_item* bar_item, bool state) {
  if (bar_item->drawing == state) return;
  bar_item->drawing = state;
  bar_item_needs_update(bar_item);
}

void bar_item_on_click(struct bar_item* bar_item) {
  if (!bar_item) return;
  if (bar_item && strlen(bar_item->click_script) > 0)
    fork_exec(bar_item->click_script, &bar_item->signal_args);
}

void bar_item_set_yoffset(struct bar_item* bar_item, int offset) {
  if (bar_item->y_offset == offset) return;
  bar_item->y_offset = offset;
  bar_item_needs_update(bar_item);
}

CGRect bar_item_construct_bounding_rect(struct bar_item* bar_item) {
  CGRect bounding_rect;
  bounding_rect.origin = bar_item->icon.line.bounds.origin;
  bounding_rect.origin.x -= bar_item->icon.padding_left;
  bounding_rect.origin.y = bar_item->icon.line.bounds.origin.y < bar_item->label.line.bounds.origin.y ? bar_item->icon.line.bounds.origin.y : bar_item->label.line.bounds.origin.y;
  bounding_rect.size.width = bar_item->label.line.bounds.size.width + bar_item->icon.line.bounds.size.width
                             + bar_item->icon.padding_left + bar_item->icon.padding_right
                             + bar_item->label.padding_right + bar_item->label.padding_left;
  bounding_rect.size.height = bar_item->label.line.bounds.size.height > bar_item->icon.line.bounds.size.height ? bar_item->label.line.bounds.size.height : bar_item->icon.line.bounds.size.height;
  return bounding_rect;
}

void bar_item_set_bounding_rect_for_display(struct bar_item* bar_item, uint32_t adid, CGPoint bar_origin) {
  if (bar_item->num_rects < adid) {
    bar_item->bounding_rects = (CGRect**) realloc(bar_item->bounding_rects, sizeof(CGRect*) * adid);
    memset(bar_item->bounding_rects, 0, sizeof(CGRect*) * adid);
    bar_item->num_rects = adid;
  }
  if (!bar_item->bounding_rects[adid - 1]) {
    bar_item->bounding_rects[adid - 1] = malloc(sizeof(CGRect));
    memset(bar_item->bounding_rects[adid - 1], 0, sizeof(CGRect));
  }
  CGRect rect = bar_item_construct_bounding_rect(bar_item);
  bar_item->bounding_rects[adid - 1]->origin.x = rect.origin.x + bar_origin.x;
  bar_item->bounding_rects[adid - 1]->origin.y = rect.origin.y + bar_origin.y;
  bar_item->bounding_rects[adid - 1]->size = rect.size;
}

void bar_item_destroy(struct bar_item* bar_item) {
  if (bar_item->name) free(bar_item->name);
  if (bar_item->script && !bar_item->cache_scripts) free(bar_item->script);
  if (bar_item->click_script && !bar_item->cache_scripts) free(bar_item->click_script);

  text_destroy(&bar_item->icon);
  text_destroy(&bar_item->label);

  if (bar_item->bounding_rects) {  
    for (int j = 0; j < bar_item->num_rects; j++) {
      free(bar_item->bounding_rects[j]);
    }
    free(bar_item->bounding_rects);
  }
  if (bar_item->has_graph) {
    graph_data_destroy(&bar_item->graph_data);
  }
  free(bar_item);
}

void bar_item_serialize(struct bar_item* bar_item, FILE* rsp) {
  fprintf(rsp, "{\n"
               "\t\"name\": \"%s\",\n"
               "\t\"type\": \"%c\",\n"
               "\t\"text\": {\n"
               "\t\t\"icon\": \"%s\",\n"
               "\t\t\"label\": \"%s\",\n"
               "\t\t\"icon_font\": \"%s\",\n"
               "\t\t\"label_font\": \"%s\"\n"
               "\t},\n"
               "\t\"geometry\": {\n"
               "\t\t\"position\": \"%c\",\n"
               "\t\t\"nospace\": %d,\n"
               "\t\t\"background_padding_left\": %d,\n"
               "\t\t\"background_padding_right\": %d,\n"
               "\t\t\"icon_padding_left\": %d,\n"
               "\t\t\"icon_padding_right\": %d,\n"
               "\t\t\"label_padding_left\": %d,\n"
               "\t\t\"label_padding_right\": %d\n"
               "\t},\n"
               "\t\"style\": {\n"
               "\t\t\"icon_color:\": \"0x%x\",\n"
               "\t\t\"icon_highlight_color:\": \"0x%x\",\n"
               "\t\t\"label_color:\": \"0x%x\",\n"
               "\t\t\"label_highlight_color:\": \"0x%x\",\n"
               "\t\t\"draws_background\": %d,\n"
               "\t\t\"background_height\": %u,\n"
               "\t\t\"background_corner_radius\": %u,\n"
               "\t\t\"background_border_width\": %u,\n"
               "\t\t\"background_color:\": \"0x%x\",\n"
               "\t\t\"background_border_color:\": \"0x%x\"\n"
               "\t},\n"
               "\t\"state\": {\n"
               "\t\t\"drawing\": %d,\n"
               "\t\t\"updates\": %d,\n"
               "\t\t\"lazy\": %d,\n"
               "\t\t\"cache_scripts\": %d,\n"
               "\t\t\"associated_bar_mask\": %u,\n"
               "\t\t\"associated_display_mask\": %u,\n"
               "\t\t\"associated_space_mask\": %u,\n"
               "\t\t\"update_mask\": %u\n"
               "\t},\n"
               "\t\"bounding_rects\": {\n",
               bar_item->name,
               bar_item->type,
               bar_item->icon.string,
               bar_item->label.string,
               bar_item->icon.font_name,
               bar_item->label.font_name,
               bar_item->position,
               bar_item->nospace,
               bar_item->background.padding_left,
               bar_item->background.padding_right,
               bar_item->icon.padding_left,
               bar_item->icon.padding_right,
               bar_item->label.padding_left,
               bar_item->label.padding_right,
               hex_from_rgba_color(bar_item->icon.color),
               hex_from_rgba_color(bar_item->icon.highlight_color),
               hex_from_rgba_color(bar_item->label.color),
               hex_from_rgba_color(bar_item->label.highlight_color),
               bar_item->background.enabled,
               bar_item->background.height,
               bar_item->background.corner_radius,
               bar_item->background.border_width,
               hex_from_rgba_color(bar_item->background.color),
               hex_from_rgba_color(bar_item->background.border_color),
               bar_item->drawing,
               bar_item->updates,
               bar_item->lazy,
               bar_item->cache_scripts,
               bar_item->associated_bar,
               bar_item->associated_display,
               bar_item->associated_space,
               bar_item->update_mask);

  for (int i = 0; i < bar_item->num_rects; i++) {
    if (!bar_item->bounding_rects[i]) continue;
    fprintf(rsp, "\t\t\"display-%d\": {\n"
            "\t\t\t\"origin\": [ %f, %f ],\n"
            "\t\t\t\"size\": [ %f, %f ]\n\t\t}",
            i + 1,
            bar_item->bounding_rects[i]->origin.x,
            bar_item->bounding_rects[i]->origin.y,
            bar_item->bounding_rects[i]->size.width,
            bar_item->bounding_rects[i]->size.height);
    if (i < bar_item->num_rects - 1) fprintf(rsp, ",\n");
  } 
  fprintf(rsp, "\n\t}\n}\n");
}
