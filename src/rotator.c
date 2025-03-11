#include "rotator.h"
#include "event.h"

void rotator_manager_init(struct rotator_manager* rotator_manager) {
  rotator_manager->rotators = NULL;
  rotator_manager->rotator_count = 0;
  rotator_manager->enabled_rotator_count = 0;
  rotator_manager->display_link = NULL;
}

static CVReturn rotator_frame_callback(CVDisplayLinkRef display_link, const CVTimeStamp* now, const CVTimeStamp* output_time, CVOptionFlags flags, CVOptionFlags* flags_out, void* context) {
  struct event event = { (void*)output_time, ROTATOR_REFRESH };
  event_post(&event);
  return kCVReturnSuccess;
}


void rotator_manager_renew_display_link(struct rotator_manager* rotator_manager) {
  rotator_manager_destroy_display_link(rotator_manager);

  CVDisplayLinkCreateWithActiveCGDisplays(&rotator_manager->display_link);
  CVDisplayLinkSetOutputCallback(rotator_manager->display_link,
                                 rotator_frame_callback,
                                 rotator_manager);

  CVDisplayLinkStart(rotator_manager->display_link);
}

void rotator_manager_destroy_display_link(struct rotator_manager* rotator_manager) {
  if (rotator_manager->display_link) {
    CVDisplayLinkStop(rotator_manager->display_link);
    CVDisplayLinkRelease(rotator_manager->display_link);
    rotator_manager->display_link = NULL;
  }
}

void rotator_manager_add(struct rotator_manager *rotator_manager, struct rotator *rotator) {
  rotator_manager->rotators = realloc(rotator_manager->rotators,
                                 sizeof(struct rotator*)
                                        * ++rotator_manager->rotator_count);
  rotator_manager->rotators[rotator_manager->rotator_count - 1] = rotator;

  rotator->inited = true;

  update_enabled_rotator_count(rotator_manager);

  if (!rotator_manager->display_link && rotator_manager->enabled_rotator_count > 0) rotator_manager_renew_display_link(rotator_manager);
}

void rotator_manager_remove(struct rotator_manager *rotator_manager, struct rotator *rotator) {
  if (rotator_manager->rotator_count == 1) {
    free(rotator_manager->rotators);
    rotator_manager->rotators = NULL;
    rotator_manager->rotator_count = 0;
    rotator_manager->enabled_rotator_count = 0;
    rotator_manager_destroy_display_link(rotator_manager);
  } else {
    struct rotator* tmp[rotator_manager->rotator_count - 1];
    int count = 0;
    for (int i = 0; i < rotator_manager->rotator_count; i++) {
      if (rotator_manager->rotators[i] == rotator) continue;
      tmp[count++] = rotator_manager->rotators[i];
    }
    rotator_manager->rotator_count--;
    rotator_manager->rotators = realloc(
                                 rotator_manager->rotators,
                                 sizeof(struct rotator*)*rotator_manager->rotator_count);

    memcpy(rotator_manager->rotators,
           tmp,
           sizeof(struct rotator*)*rotator_manager->rotator_count);
  }

  rotator_destroy(rotator_manager, rotator);

  if (rotator_manager->enabled_rotator_count == 0) {
    rotator_manager_destroy_display_link(rotator_manager);
  }
}

void rotator_manager_destroy(struct rotator_manager* rotator_manager) {
  if (!rotator_manager) {
    return;
  }
  rotator_manager_destroy_display_link(rotator_manager);
  for (int i = 0; i < rotator_manager->rotator_count; i++) {
    rotator_destroy(rotator_manager, rotator_manager->rotators[i]);
  }
  if (rotator_manager->rotators) free(rotator_manager->rotators);
}

bool rotator_manager_update(struct rotator_manager *rotator_manager, CVTimeStamp* output_time) {
  bool needs_refresh = false;

  for (int i = 0; i < rotator_manager->rotator_count; i++) {
    needs_refresh |= rotator_update(rotator_manager->rotators[i], output_time);
  }

  return needs_refresh;
}

struct rotator* rotator_create(void* target, CGFloat init_rotation, CGFloat rotate_rate, rotator_function* update_function) {
  struct rotator* rotator = malloc(sizeof(struct rotator));
  memset(rotator, 0, sizeof(struct rotator));
  rotator->current_rotation = init_rotation;
  rotator->rotate_rate = rotate_rate;
  rotator->update_function = update_function;
  rotator->enabled = false;
  rotator->target = target;
  pthread_mutex_init(&rotator->mutex, NULL);
  return rotator;
}

void update_enabled_rotator_count(struct rotator_manager* rotator_manager) {
  int count = 0;
  for (int i = 0; i < rotator_manager->rotator_count; i++) {
    if (rotator_manager->rotators[i]->enabled) {
      count++;
    }
  }
  rotator_manager->enabled_rotator_count = count;
}

void rotator_start(struct rotator_manager* rotator_manager, struct rotator* rotator) {
  if (!rotator->inited) {
    rotator_manager_add(rotator_manager, rotator);
  }
  rotator->enabled = true;
  update_enabled_rotator_count(rotator_manager);
  if (!rotator_manager->display_link) {
    rotator_manager_renew_display_link(rotator_manager);
  }
}

void rotator_stop(struct rotator_manager* rotator_manager, struct rotator* rotator) {
  rotator->enabled = false;
  update_enabled_rotator_count(rotator_manager);
  if (rotator_manager->enabled_rotator_count == 0) {
    rotator_manager_destroy_display_link(rotator_manager);
  }
}

void rotator_destroy(struct rotator_manager* rotator_manager, struct rotator* rotator) {
  if (!rotator) {
    return;
  }
  rotator_stop(rotator_manager, rotator);
  pthread_mutex_destroy(&rotator->mutex);
  free(rotator);
}

bool rotator_update(struct rotator *rotator, CVTimeStamp* output_time) {
  bool needs_update = false;
  if (!rotator->enabled) {
    needs_update = false;
  } else if (rotator->update_function) {
    needs_update |= rotator->update_function(rotator->target, output_time);
  }
  bool found_item = false;
  for (int i = 0; i < g_bar_manager.bar_item_count; i++) {
    if (needs_update
        && (rotator->target >= (void*)g_bar_manager.bar_items[i])
        && (rotator->target < ((void*)g_bar_manager.bar_items[i]
                                 + sizeof(struct bar_item)         ))) {

      bar_item_needs_update(g_bar_manager.bar_items[i]);
      found_item = true;
    }
  }

  if (!found_item && needs_update) g_bar_manager.bar_needs_update = true;


  return needs_update;
}
