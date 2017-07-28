#define E_COMP_WL
#include "e_mod_main.h"

void
_e_gesture_conf_value_check(E_Gesture_Config_Data* gconfig)
{
   E_Gesture_Conf_Edd *conf;
   if (!gconfig->conf) gconfig->conf = E_NEW(E_Gesture_Conf_Edd, 1);
   EINA_SAFETY_ON_NULL_RETURN(gconfig->conf);

   conf = gconfig->conf;

   if (!conf->key_device_name) conf->key_device_name = strdup(E_GESTURE_KEYBOARD_DEVICE);
   if (conf->edge_swipe.time_done <= 0.0) conf->edge_swipe.time_done = E_GESTURE_EDGE_SWIPE_DONE_TIME;
   if (conf->edge_swipe.time_begin <= 0.0) conf->edge_swipe.time_begin = E_GESTURE_EDGE_SWIPE_START_TIME;
   if (conf->edge_swipe.area_offset <= 0) conf->edge_swipe.area_offset = E_GESTURE_EDGE_SWIPE_START_AREA;
   if (conf->edge_swipe.min_length <= 0) conf->edge_swipe.min_length = E_GESTURE_EDGE_SWIPE_DIFF_FAIL;
   if (conf->edge_swipe.max_length <= 0) conf->edge_swipe.max_length = E_GESTURE_EDGE_SWIPE_DIFF_SUCCESS;
   if (conf->edge_swipe.compose_key <= 0) conf->edge_swipe.compose_key = E_GESTURE_EDGE_SWIPE_COMBINE_KEY;
   if (conf->edge_swipe.back_key <= 0) conf->edge_swipe.back_key = E_GESTURE_EDGE_SWIPE_BACK_KEY;

   if (conf->edge_drag.time_begin <= 0.0) conf->edge_drag.time_begin = E_GESTURE_EDGE_DRAG_START_TIME;
   if (conf->edge_drag.area_offset <= 0) conf->edge_drag.area_offset = E_GESTURE_EDGE_DRAG_START_AREA;
   if (conf->edge_drag.diff_length <= 0) conf->edge_drag.diff_length = E_GESTURE_EDGE_DRAG_DIFF;

   if (conf->tap.repeats_max <= 0) conf->tap.repeats_max = E_GESTURE_TAP_REPEATS_MAX;
   if (conf->tap.time_start <= 0.0) conf->tap.time_start = E_GESTURE_TAP_START_TIME;
   if (conf->tap.time_done <= 0.0) conf->tap.time_done = E_GESTURE_TAP_DONE_TIME;
   if (conf->tap.time_interval <= 0.0) conf->tap.time_interval = E_GESTURE_TAP_INTERVAL_TIME;
   if (conf->tap.moving_range <= 0) conf->tap.moving_range = E_GESTURE_TAP_MOVING_RANGE;

   if (conf->pan.time_start <= 0.0) conf->pan.time_start = E_GESTURE_PAN_START_TIME;
   if (conf->pan.moving_range <= 0) conf->pan.moving_range = E_GESTURE_PAN_MOVING_RANGE;

   if (conf->pinch.moving_distance_range <= 0.0) conf->pinch.moving_distance_range = E_GESTURE_PINCH_MOVING_DISTANCE_RANGE;
}

void
e_gesture_conf_init(E_Gesture_Config_Data *gconfig)
{
   gconfig->conf_edd = E_CONFIG_DD_NEW("Gesture_Config", E_Gesture_Conf_Edd);
#undef T
#undef D
#define T E_Gesture_Conf_Edd
#define D gconfig->conf_edd
   E_CONFIG_VAL(D, T, key_device_name, STR);
   E_CONFIG_VAL(D, T, event_keep, CHAR);

   E_CONFIG_VAL(D, T, edge_swipe.time_done, DOUBLE);
   E_CONFIG_VAL(D, T, edge_swipe.time_begin, DOUBLE);
   E_CONFIG_VAL(D, T, edge_swipe.area_offset, INT);
   E_CONFIG_VAL(D, T, edge_swipe.min_length, INT);
   E_CONFIG_VAL(D, T, edge_swipe.max_length, INT);
   E_CONFIG_VAL(D, T, edge_swipe.compose_key, INT);
   E_CONFIG_VAL(D, T, edge_swipe.back_key, INT);
   E_CONFIG_VAL(D, T, edge_swipe.default_enable_back, CHAR);

   E_CONFIG_VAL(D, T, edge_drag.time_begin, DOUBLE);
   E_CONFIG_VAL(D, T, edge_drag.area_offset, INT);
   E_CONFIG_VAL(D, T, edge_drag.diff_length, INT);

   E_CONFIG_VAL(D, T, tap.repeats_max, INT);
   E_CONFIG_VAL(D, T, tap.time_start, DOUBLE);
   E_CONFIG_VAL(D, T, tap.time_done, DOUBLE);
   E_CONFIG_VAL(D, T, tap.time_interval, DOUBLE);
   E_CONFIG_VAL(D, T, tap.moving_range, INT);

   E_CONFIG_VAL(D, T, pan.time_start, DOUBLE);
   E_CONFIG_VAL(D, T, pan.moving_range, INT);

   E_CONFIG_VAL(D, T, pinch.moving_distance_range, DOUBLE);

#undef T
#undef D
   gconfig->conf = e_config_domain_load("module.gesture", gconfig->conf_edd);

   if (!gconfig->conf)
     {
        GTWRN("Failed to find module.keyrouter config file.\n");
     }
   _e_gesture_conf_value_check(gconfig);
}

void
e_gesture_conf_deinit(E_Gesture_Config_Data *gconfig)
{
   if (gconfig->conf)
     {
        E_FREE(gconfig->conf->key_device_name);
        E_FREE(gconfig->conf);
     }
   E_CONFIG_DD_FREE(gconfig->conf_edd);
   E_FREE(gconfig);
}
