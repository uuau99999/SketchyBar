#pragma once
#include <CoreVideo/CoreVideo.h>

extern struct bar_manager g_bar_manager;

#define ROTATOR_FUNCTION(name) bool name(void* target, CVTimeStamp* output_time);
typedef ROTATOR_FUNCTION(rotator_function);

#define ROTATION_START(r) \
{\
  rotator_start(&g_bar_manager.rotator_manager, r);\
}

#define ROTATION_STOP(r) \
{\
  rotator_stop(&g_bar_manager.rotator_manager, r);\
}

#define ROTATION_DESTROY(r) \
{\
  rotator_manager_remove(&g_bar_manager.rotator_manager, r);\
}


// Structure to encapsulate the state of the image rotator
struct rotator {
    CGFloat current_rotation;      // Current rotation (degrees)
    CGFloat rotate_rate;     // Rotation speed (degrees/second)
    pthread_mutex_t mutex;        // Mutex
    rotator_function* update_function;  // Frame callback function
    bool enabled;
    bool inited;
    void* target;
};

struct rotator_manager {
  struct rotator** rotators;
  uint32_t rotator_count;
  uint32_t enabled_rotator_count;
  CVDisplayLinkRef display_link; // Display link
};

void rotator_manager_init(struct rotator_manager* rotator_manager);
void rotator_manager_renew_display_link(struct rotator_manager* rotator_manager);
void rotator_manager_destroy_display_link(struct rotator_manager* rotator_manager);
void rotator_manager_add(struct rotator_manager* rotator_manager, struct rotator* rotator);
void rotator_manager_remove(struct rotator_manager* rotator_manager, struct rotator* rotator);
void rotator_manager_destroy(struct rotator_manager* rotator_manager);
bool rotator_manager_update(struct rotator_manager* rotator_manager, CVTimeStamp* output_time);

struct rotator* rotator_create(void* target, CGFloat init_rotation, CGFloat rotate_rate, rotator_function* update_function);
void rotator_start(struct rotator_manager* rotator_manager, struct rotator* rotator);
void rotator_stop(struct rotator_manager* rotator_manager, struct rotator* rotator);
void rotator_destroy(struct rotator_manager* rotator_manager, struct rotator* rotator);
void update_enabled_rotator_count(struct rotator_manager* rotator_manager);
bool rotator_update(struct rotator* rotator, CVTimeStamp* output_time);

