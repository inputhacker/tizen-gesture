#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>

#ifdef _E_GESTURE_DEBUG_
static char *
_e_gesture_event_type_print(int type)
{
   if (type == ECORE_EVENT_KEY_DOWN) return "KeyDown";
   else if (type == ECORE_EVENT_KEY_UP) return "KeyUp";
   else if (type == ECORE_EVENT_MOUSE_BUTTON_DOWN) return "MouseDown";
   else if (type == ECORE_EVENT_MOUSE_BUTTON_UP) return "MouseUp";
   else if (type == ECORE_EVENT_MOUSE_MOVE) return "MouseMove";
   else return "Unknown";
}

static void
_e_gesture_event_print(void)
{
   Eina_List *l;
   E_Gesture_Event_Info *data;
   int c = 0;

   EINA_LIST_FOREACH(gesture->event_queue, l, data)
     {
        GTERR("[%d]%s event is queue\n", ++c, _e_gesture_event_type_print(data->type));
     }
}
#endif //_E_GESTURE_DEBUG_

static void _e_gesture_send_edge_drag(int fingers, int x, int y, int edge, int mode);

static void
_e_gesture_event_queue(int type, void *event)
{
   E_Gesture_Event_Info *e_info = NULL;
   e_info = E_NEW(E_Gesture_Event_Info, 1);
   EINA_SAFETY_ON_NULL_RETURN(e_info);

   if (type == ECORE_EVENT_KEY_DOWN ||
       type == ECORE_EVENT_KEY_UP)
     {
        e_info->event = E_NEW(Ecore_Event_Key, 1);
        e_info->type = type;
        memcpy(e_info->event, event, sizeof(Ecore_Event_Key));
     }
   else if (type == ECORE_EVENT_MOUSE_BUTTON_DOWN ||
       type == ECORE_EVENT_MOUSE_BUTTON_UP)
     {
        e_info->event = E_NEW(Ecore_Event_Mouse_Button, 1);
        e_info->type = type;
        memcpy(e_info->event, event, sizeof(Ecore_Event_Mouse_Button));
     }
   else if (type == ECORE_EVENT_MOUSE_MOVE)
     {
        e_info->event = E_NEW(Ecore_Event_Mouse_Move, 1);
        e_info->type = type;
        memcpy(e_info->event, event, sizeof(Ecore_Event_Mouse_Move));
     }
    else goto error;

   gesture->event_queue = eina_list_append(gesture->event_queue, e_info);
   return;

error:
   if (e_info->event) E_FREE(e_info->event);
   E_FREE(e_info);
}

static void
_e_gesture_event_flush(void)
{
   Eina_List *l, *l_next;
   E_Gesture_Event_Info *data;

   if (gesture->event_state == E_GESTURE_EVENT_STATE_IGNORE ||
      gesture->gesture_events.recognized_gesture) return;
   if (gesture->gesture_filter != E_GESTURE_TYPE_ALL) return;

   gesture->event_state = E_GESTURE_EVENT_STATE_PROPAGATE;

   EINA_LIST_FOREACH_SAFE(gesture->event_queue, l, l_next, data)
     {
        if (data->type == ECORE_EVENT_MOUSE_BUTTON_DOWN)
          {
             ecore_event_evas_mouse_button_down(NULL, ECORE_EVENT_MOUSE_BUTTON_DOWN, data->event);
          }
        else if (data->type == ECORE_EVENT_MOUSE_BUTTON_UP)
          {
             ecore_event_evas_mouse_button_up(NULL, ECORE_EVENT_MOUSE_BUTTON_UP, data->event);
          }
        else if (data->type == ECORE_EVENT_MOUSE_MOVE)
          {
             ecore_event_evas_mouse_move(NULL, ECORE_EVENT_MOUSE_MOVE, data->event);
          }
        else if (data->type == ECORE_EVENT_KEY_DOWN)
          {
             ecore_event_evas_key_down(NULL, ECORE_EVENT_KEY_DOWN, data->event);
          }
        else if (data->type == ECORE_EVENT_KEY_UP)
          {
             ecore_event_evas_key_up(NULL, ECORE_EVENT_KEY_UP, data->event);
          }
        E_FREE(data->event);
        E_FREE(data);
        gesture->event_queue = eina_list_remove_list(gesture->event_queue, l);
     }
}

static void
_e_gesture_event_drop(void)
{
   Eina_List *l, *l_next;
   E_Gesture_Event_Info *data;

   gesture->event_state = E_GESTURE_EVENT_STATE_IGNORE;

   EINA_LIST_FOREACH_SAFE(gesture->event_queue, l, l_next, data)
     {
        E_FREE(data->event);
        E_FREE(data);
        gesture->event_queue = eina_list_remove_list(gesture->event_queue, l);
     }
}

static void
_e_gesture_util_center_axis_get(int num_finger, int *x, int *y)
{
   int i;
   int calc_x = 0, calc_y = 0;

   if (num_finger <= 0)
     {
        *x = 0;
        *y = 0;
        return;
     }

   for (i = 1; i <= num_finger; i++)
     {
        calc_x += gesture->gesture_events.base_point[i].axis.x;
        calc_y += gesture->gesture_events.base_point[i].axis.y;
     }

   calc_x = (int)(calc_x / num_finger);
   calc_y = (int)(calc_y / num_finger);

   *x = calc_x;
   *y = calc_y;
}

static double
_e_gesture_util_distance_get(int x1, int y1, int x2, int y2)
{
   double distance;

   distance = sqrt(((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1)));

   return distance;
}

static double
_e_gesture_util_distances_get(int num_finger)
{
   int i, cx = 0, cy = 0;
   double distance = 0.0;

   _e_gesture_util_center_axis_get(num_finger, &cx, &cy);

   for (i = 1; i <= num_finger; i++)
     {
        distance +=  _e_gesture_util_distance_get(cx, cy,
                       gesture->gesture_events.base_point[i].axis.x,
                       gesture->gesture_events.base_point[i].axis.y);
     }

   return distance;
}

static double
_e_gesture_util_angle_get(int x1, int y1, int x2, int y2)
{
   double angle, xx, yy;

   xx = fabs(x2 - x1);
   yy = fabs(y2 - y1);

   angle = atan2(yy, xx);
   if ((x1 > x2) && (y1 > y2)) angle = angle + M_PI_2;
   else if ((x2 > x1) && (y2 > y1)) angle = angle + M_PI_2;

   angle = RAD2DEG(angle);
   return angle;
}

static void
_e_gesture_util_rect_get(int finger, int *x1, int *y1, int *x2, int *y2)
{
   int i;

   *x1 = *x2 = gesture->gesture_events.base_point[1].axis.x;
   *y1 = *y2 = gesture->gesture_events.base_point[1].axis.y;

   for (i = 2; i < finger + 1; i++)
     {
        if (gesture->gesture_events.base_point[i].axis.x < *x1)
          *x1 = gesture->gesture_events.base_point[i].axis.x;
        else if (gesture->gesture_events.base_point[i].axis.x > *x2)
          *x2 = gesture->gesture_events.base_point[i].axis.x;

        if (gesture->gesture_events.base_point[i].axis.y < *y1)
          *y1 = gesture->gesture_events.base_point[i].axis.y;
        else if (gesture->gesture_events.base_point[i].axis.y > *y2)
          *y2 = gesture->gesture_events.base_point[i].axis.y;
     }
}

static struct wl_resource *
_e_gesture_util_eclient_surface_get(E_Client *ec)
{
   if (!ec) return NULL;
   if (e_object_is_del(E_OBJECT(ec))) return NULL;
   if (!ec->comp_data) return NULL;

   return ec->comp_data->surface;
}

static void
_e_gesture_edge_swipe_cancel(void)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;

   if (edge_swipes->start_timer)
     {
        ecore_timer_del(edge_swipes->start_timer);
        edge_swipes->start_timer = NULL;
     }
   if (edge_swipes->done_timer)
     {
        ecore_timer_del(edge_swipes->done_timer);
        edge_swipes->done_timer = NULL;
     }

   edge_swipes->base.enabled_finger = 0x0;
   edge_swipes->base.edge = E_GESTURE_EDGE_NONE;

   gesture->gesture_filter |= E_GESTURE_TYPE_EDGE_SWIPE;
}

static void
_e_gesture_keyevent_free(void *data EINA_UNUSED, void *ev)
{
   Ecore_Event_Key *e = ev;

   eina_stringshare_del(e->keyname);
   eina_stringshare_del(e->key);
   eina_stringshare_del(e->compose);

   if (e->dev) ecore_device_unref(e->dev);

   E_FREE(e);
}

/* Optional: This function is currently used to generate back key.
 *           But how about change this function to generate every key?
 *           _e_gesture_send_key(char *keyname, Eina_Bool pressed)
 */
static void
_e_gesture_send_back_key(Eina_Bool pressed)
{
   Ecore_Event_Key *ev;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   E_Keyrouter_Event_Data *key_data;

   EINA_SAFETY_ON_NULL_RETURN(e_comp_wl->xkb.keymap);

   ev = E_NEW(Ecore_Event_Key, 1);
   EINA_SAFETY_ON_NULL_RETURN(ev);
   key_data = E_NEW(E_Keyrouter_Event_Data, 1);
   EINA_SAFETY_ON_NULL_GOTO(key_data, failed);

   ev->key = (char *)eina_stringshare_add("XF86Back");
   ev->keyname = (char *)eina_stringshare_add(ev->key);
   ev->compose = (char *)eina_stringshare_add(ev->key);
   ev->timestamp = (int)(ecore_time_get()*1000);
   ev->same_screen = 1;
   ev->keycode = conf->edge_swipe.back_key;
   ev->dev = ecore_device_ref(gesture->device.kbd_device);
   ev->window = e_comp->ee_win;
   ev->data = key_data;

   if (pressed)
     ecore_event_add(ECORE_EVENT_KEY_DOWN, ev, _e_gesture_keyevent_free, NULL);
   else
     ecore_event_add(ECORE_EVENT_KEY_UP, ev, _e_gesture_keyevent_free, NULL);

   return;

failed:
   if (ev) E_FREE(ev);
}

static void
_e_gesture_send_edge_swipe(int fingers, int x, int y, int edge)
{
   Ecore_Event_Mouse_Button *ev_cancel;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   Eina_List *l;
   E_Gesture_Event_Edge_Finger_Edge *edata;
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;
   int bp = -1;
   E_Event_Gesture_Edge_Swipe *ev_swipe;

   if (gesture->gesture_events.event_keep)
     {
        _e_gesture_event_drop();
     }
   else
     {
        ev_cancel = E_NEW(Ecore_Event_Mouse_Button, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_cancel);

        ev_cancel->timestamp = (int)(ecore_time_get()*1000);
        ev_cancel->same_screen = 1;

        ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_CANCEL, ev_cancel, NULL, NULL);
     }

   if (conf->edge_swipe.default_enable_back &&
       edge == E_GESTURE_EDGE_TOP)
     {
        _e_gesture_send_back_key(EINA_TRUE);
        _e_gesture_send_back_key(EINA_FALSE);
        goto finish;
     }

   if (edge == E_GESTURE_EDGE_TOP ||
       edge == E_GESTURE_EDGE_BOTTOM)
     {
        bp = x;
     }
   else if (edge == E_GESTURE_EDGE_RIGHT ||
       edge == E_GESTURE_EDGE_LEFT)
     {
        bp = y;
     }

   EINA_LIST_FOREACH(edge_swipes->base.fingers[fingers].edge[edge], l, edata)
     {
        if (bp >= edata->sp && bp <= edata->ep)
          {
             GTINF("Send edge_swipe gesture (fingers: %d, edge: %d) to client: %p\n", fingers, edge, edata->client);
             if (edata->client == E_GESTURE_SERVER_CLIENT)
               {
                  ev_swipe = E_NEW(E_Event_Gesture_Edge_Swipe, 1);
                  EINA_SAFETY_ON_NULL_RETURN(ev_swipe);

                  ev_swipe->mode = E_GESTURE_MODE_DONE;
                  ev_swipe->edge = edge;
                  ev_swipe->fingers = fingers;
                  ev_swipe->sx = x;
                  ev_swipe->sy = y;
                  ecore_event_add(E_EVENT_GESTURE_EDGE_SWIPE, ev_swipe, NULL, NULL);
               }
             else
               tizen_gesture_send_edge_swipe(edata->res, E_GESTURE_MODE_DONE, fingers, x, y, edge);
             break;
          }
     }

finish:
   _e_gesture_edge_swipe_cancel();
   gesture->gesture_events.recognized_gesture |= E_GESTURE_TYPE_EDGE_SWIPE;
}

static E_Gesture_Event_State
_e_gesture_process_device_add(void *event)
{
   return e_gesture_device_add(event);
}

static E_Gesture_Event_State
_e_gesture_process_device_del(void *event)
{
   return e_gesture_device_del(event);
}

static Eina_Bool
_e_gesture_event_edge_check(E_Gesture_Event_Edge_Finger *fingers, int type, unsigned int edge)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   Eina_List *l;
   E_Gesture_Event_Edge_Finger_Edge *edata;
   Coords coords;

   if (type == E_GESTURE_TYPE_EDGE_SWIPE)
     {
        if ((conf->edge_swipe.default_enable_back) &&
            (edge == E_GESTURE_EDGE_TOP ||
            ((edge_swipes->combined_keycode == conf->edge_swipe.compose_key) &&
            (edge_swipes->base.edge == E_GESTURE_EDGE_LEFT))))
          {
             return EINA_TRUE;
          }
     }

   coords.x = fingers->start.x;
   coords.y = fingers->start.y;

   EINA_LIST_FOREACH(fingers->edge[edge], l, edata)
     {
        if (edge == E_GESTURE_EDGE_TOP ||
            edge == E_GESTURE_EDGE_BOTTOM)
          {
             if ((coords.x >= edata->sp) &&
                 (coords.x <= edata->ep))
               {
                  return EINA_TRUE;
               }
          }
        else if ((edge == E_GESTURE_EDGE_LEFT) ||
                 (edge == E_GESTURE_EDGE_RIGHT))
          {
             if (coords.y >= edata->sp &&
                 coords.y <= edata->ep)
               {
                  return EINA_TRUE;
               }
          }
     }

   return EINA_FALSE;
}

static Eina_Bool
_e_gesture_timer_edge_swipe_start(void *data)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;
   int idx = gesture->gesture_events.num_pressed;
   int i;

   GTDBG("Edge_Swipe start timer is expired. Currently alived edge_swipe fingers: 0x%x\n", edge_swipes->base.enabled_finger);

   for (i = E_GESTURE_FINGER_MAX; i > idx; i--)
     {
        edge_swipes->base.enabled_finger &= ~(1 << i);
     }
   if ((edge_swipes->base.enabled_finger == 0x0) ||
       (_e_gesture_event_edge_check(&edge_swipes->base.fingers[idx], E_GESTURE_TYPE_EDGE_SWIPE, edge_swipes->base.edge) == EINA_FALSE))
     {
        _e_gesture_edge_swipe_cancel();
        if (gesture->gesture_events.event_keep)
          _e_gesture_event_flush();
     }
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_gesture_timer_edge_swipe_done(void *data)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;

   GTDBG("Edge_Swipe done timer is expired. Currently alived edge_swipe fingers: 0x%x\n", edge_swipes->base.enabled_finger);

   _e_gesture_edge_swipe_cancel();
   if (gesture->gesture_events.event_keep)
     _e_gesture_event_flush();

   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_process_edge_swipe_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   int i;
   unsigned int idx = ev->multi.device+1;

   if (!edge_swipes->base.activation.active) return;

   if (gesture->gesture_events.recognized_gesture)
     _e_gesture_edge_swipe_cancel();

   if (gesture->gesture_events.num_pressed == 1)
     {
        for (i = 1; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             if (edge_swipes->base.fingers[i].enabled)
               {
                  edge_swipes->base.enabled_finger |= (1 << i);
               }
          }

        if (ev->y < conf->edge_swipe.area_offset)
          edge_swipes->base.edge = E_GESTURE_EDGE_TOP;
        else if (ev->y > e_comp->h - conf->edge_swipe.area_offset)
          edge_swipes->base.edge = E_GESTURE_EDGE_BOTTOM;
        else if (ev->x < conf->edge_swipe.area_offset)
          edge_swipes->base.edge = E_GESTURE_EDGE_LEFT;
        else if (ev->x > e_comp->w - conf->edge_swipe.area_offset)
          edge_swipes->base.edge = E_GESTURE_EDGE_RIGHT;

        if (!((1 << (edge_swipes->base.edge)) & edge_swipes->base.enabled_edge))
          edge_swipes->base.edge = E_GESTURE_EDGE_NONE;

        if (edge_swipes->base.edge != E_GESTURE_EDGE_NONE)
          {
             edge_swipes->base.fingers[idx].start.x = ev->x;
             edge_swipes->base.fingers[idx].start.y = ev->y;
             edge_swipes->start_timer = ecore_timer_add(conf->edge_swipe.time_begin, _e_gesture_timer_edge_swipe_start, NULL);
             edge_swipes->done_timer = ecore_timer_add(conf->edge_swipe.time_done, _e_gesture_timer_edge_swipe_done, NULL);
          }
        else
          {
             _e_gesture_edge_swipe_cancel();
             if (gesture->gesture_events.event_keep)
               _e_gesture_event_flush();
          }
     }
   else
     {
        edge_swipes->base.enabled_finger &= ~(1 << (gesture->gesture_events.num_pressed - 1));
        if (edge_swipes->start_timer == NULL)
          {
             _e_gesture_edge_swipe_cancel();
             if (gesture->gesture_events.event_keep)
               _e_gesture_event_flush();
          }
     }
}

static void
_e_gesture_process_edge_swipe_move(Ecore_Event_Mouse_Move *ev)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   Coords diff;
   unsigned int idx = ev->multi.device+1;

   if (!edge_swipes->base.activation.active) return;

   if (!(edge_swipes->base.enabled_finger & (1 << idx)))
     return;

   diff.x = ABS(edge_swipes->base.fingers[idx].start.x - ev->x);
   diff.y = ABS(edge_swipes->base.fingers[idx].start.y - ev->y);

   switch(edge_swipes->base.edge)
     {
        case E_GESTURE_EDGE_TOP:
           if (diff.x > conf->edge_swipe.min_length)
             {
                _e_gesture_edge_swipe_cancel();
                if (gesture->gesture_events.event_keep)
                  _e_gesture_event_flush();
                break;
             }
           if (diff.y > conf->edge_swipe.max_length)
             {
                _e_gesture_send_edge_swipe(idx, edge_swipes->base.fingers[idx].start.x, edge_swipes->base.fingers[idx].start.y, E_GESTURE_EDGE_TOP);
             }
           break;
        case E_GESTURE_EDGE_LEFT:
           if (diff.y > conf->edge_swipe.min_length)
             {
                _e_gesture_edge_swipe_cancel();
                if (gesture->gesture_events.event_keep)
                  _e_gesture_event_flush();
                break;
             }
           if (diff.x > conf->edge_swipe.max_length)
             {
                _e_gesture_send_edge_swipe(idx, edge_swipes->base.fingers[idx].start.x, edge_swipes->base.fingers[idx].start.y, E_GESTURE_EDGE_LEFT);
             }
           break;
        case E_GESTURE_EDGE_BOTTOM:
           if (diff.x > conf->edge_swipe.min_length)
             {
                _e_gesture_edge_swipe_cancel();
                if (gesture->gesture_events.event_keep)
                  _e_gesture_event_flush();
                break;
             }
           if (diff.y > conf->edge_swipe.max_length)
             {
                _e_gesture_send_edge_swipe(idx, edge_swipes->base.fingers[idx].start.x, edge_swipes->base.fingers[idx].start.y, E_GESTURE_EDGE_BOTTOM);
             }
           break;
        case E_GESTURE_EDGE_RIGHT:
           if (diff.y > conf->edge_swipe.min_length)
             {
                _e_gesture_edge_swipe_cancel();
                if (gesture->gesture_events.event_keep)
                  _e_gesture_event_flush();
                break;
             }
           if (diff.x > conf->edge_swipe.max_length)
             {
                _e_gesture_send_edge_swipe(idx, edge_swipes->base.fingers[idx].start.x, edge_swipes->base.fingers[idx].start.y, E_GESTURE_EDGE_RIGHT);
             }
           break;
        default:
           GTWRN("Invalid edge(%d)\n", edge_swipes->base.edge);
           break;
     }
}

static void
_e_gesture_process_edge_swipe_up(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;

   if (!edge_swipes->base.activation.active) return;
   _e_gesture_edge_swipe_cancel();
   if (gesture->gesture_events.event_keep)
     _e_gesture_event_flush();
}

static void
_e_gesture_edge_drag_cancel(void)
{
   E_Gesture_Event_Edge_Drag *edge_drags = &gesture->gesture_events.edge_drags;
   Coords current_point = {0, };

   if (gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_EDGE_DRAG)
     {
        _e_gesture_util_center_axis_get(edge_drags->idx, &current_point.x, &current_point.y);
        _e_gesture_send_edge_drag(edge_drags->idx, current_point.x, current_point.y, edge_drags->base.edge, E_GESTURE_MODE_END);
     }

   if (edge_drags->start_timer)
     {
        ecore_timer_del(edge_drags->start_timer);
        edge_drags->start_timer = NULL;
     }

   edge_drags->base.enabled_finger = 0x0;
   edge_drags->base.edge = E_GESTURE_EDGE_NONE;
   edge_drags->start_point.x = edge_drags->center_point.x = 0;
   edge_drags->start_point.y = edge_drags->center_point.y = 0;
   edge_drags->idx = 0;

   gesture->gesture_filter |= E_GESTURE_TYPE_EDGE_DRAG;
   gesture->gesture_events.recognized_gesture &= ~E_GESTURE_TYPE_EDGE_DRAG;
}

static void
_e_gesture_send_edge_drag(int fingers, int x, int y, int edge, int mode)
{
   Ecore_Event_Mouse_Button *ev_cancel;
   Eina_List *l;
   E_Gesture_Event_Edge_Finger_Edge *edata;
   E_Gesture_Event_Edge_Drag *edge_drags = &gesture->gesture_events.edge_drags;
   int bp = -1;
   E_Event_Gesture_Edge_Drag *ev_drag;

   if (gesture->gesture_events.event_keep)
     {
        _e_gesture_event_drop();
     }
   else
     {
        ev_cancel = E_NEW(Ecore_Event_Mouse_Button, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_cancel);

        ev_cancel->timestamp = (int)(ecore_time_get()*1000);
        ev_cancel->same_screen = 1;

        ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_CANCEL, ev_cancel, NULL, NULL);
     }

   if (edge == E_GESTURE_EDGE_TOP ||
       edge == E_GESTURE_EDGE_BOTTOM)
     {
        bp = edge_drags->start_point.x;
     }
   else if (edge == E_GESTURE_EDGE_RIGHT ||
       edge == E_GESTURE_EDGE_LEFT)
     {
        bp = edge_drags->start_point.y;
     }

   EINA_LIST_FOREACH(edge_drags->base.fingers[fingers].edge[edge], l, edata)
     {
        if (bp >= edata->sp && bp <= edata->ep)
          {
             GTINF("Send edge drag gesture (fingers: %d, edge: %d) to client: %p\n", fingers, edge, edata->client);
             if (edata->client == E_GESTURE_SERVER_CLIENT)
               {
                  ev_drag = E_NEW(E_Event_Gesture_Edge_Drag, 1);
                  EINA_SAFETY_ON_NULL_RETURN(ev_drag);

                  ev_drag->mode = E_GESTURE_MODE_DONE;
                  ev_drag->edge = edge;
                  ev_drag->fingers = fingers;
                  ev_drag->cx = x;
                  ev_drag->cy = y;
                  ecore_event_add(E_EVENT_GESTURE_EDGE_DRAG, ev_drag, NULL, NULL);
               }
             else
             tizen_gesture_send_edge_drag(edata->res, mode, fingers, x, y, edge);
             break;
          }
     }

   gesture->gesture_events.recognized_gesture |= E_GESTURE_TYPE_EDGE_DRAG;
}

static Eina_Bool
_e_gesture_timer_edge_drag_start(void *data)
{
   E_Gesture_Event_Edge_Drag *edge_drags = &gesture->gesture_events.edge_drags;
   int idx = gesture->gesture_events.num_pressed;
   int i;
   Coords start_point = {0, };

   for (i = E_GESTURE_FINGER_MAX; i > idx; i--)
     {
        edge_drags->base.enabled_finger &= ~(1 << i);
     }

   if ((edge_drags->base.enabled_finger == 0x0) ||
       (_e_gesture_event_edge_check(&edge_drags->base.fingers[idx], E_GESTURE_TYPE_EDGE_DRAG, edge_drags->base.edge) == EINA_FALSE))
     {
        _e_gesture_edge_drag_cancel();
        if (gesture->gesture_events.event_keep)
          _e_gesture_event_flush();
     }
   else
     {
        _e_gesture_util_center_axis_get(idx, &start_point.x, &start_point.y);
        edge_drags->start_point.x = edge_drags->center_point.x = start_point.x;
        edge_drags->start_point.y = edge_drags->center_point.y = start_point.y;
        edge_drags->idx = idx;
        _e_gesture_send_edge_drag(edge_drags->idx, edge_drags->center_point.x, edge_drags->center_point.y, edge_drags->base.edge, E_GESTURE_MODE_BEGIN);
     }

   ecore_timer_del(edge_drags->start_timer);
   edge_drags->start_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_process_edge_drag_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Edge_Drag *edge_drags = &gesture->gesture_events.edge_drags;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   int i;
   unsigned int idx = ev->multi.device+1;

   if (!edge_drags->base.activation.active) return;

   if (gesture->gesture_events.recognized_gesture)
     _e_gesture_edge_drag_cancel();

   if (gesture->gesture_events.num_pressed == 1)
     {
        for (i = 1; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             if (edge_drags->base.fingers[i].enabled)
               {
                  edge_drags->base.enabled_finger |= (1 << i);
               }
          }

        if (ev->y < conf->edge_drag.area_offset)
          edge_drags->base.edge = E_GESTURE_EDGE_TOP;
        else if (ev->y > e_comp->h - conf->edge_drag.area_offset)
          edge_drags->base.edge = E_GESTURE_EDGE_BOTTOM;
        else if (ev->x < conf->edge_drag.area_offset)
          edge_drags->base.edge = E_GESTURE_EDGE_LEFT;
        else if (ev->x > e_comp->w - conf->edge_drag.area_offset)
          edge_drags->base.edge = E_GESTURE_EDGE_RIGHT;

        if (!((1 << (edge_drags->base.edge)) & edge_drags->base.enabled_edge))
          edge_drags->base.edge = E_GESTURE_EDGE_NONE;

        if (edge_drags->base.edge != E_GESTURE_EDGE_NONE)
          {
             edge_drags->base.fingers[idx].start.x = ev->x;
             edge_drags->base.fingers[idx].start.y = ev->y;
             edge_drags->start_timer = ecore_timer_add(conf->edge_drag.time_begin, _e_gesture_timer_edge_drag_start, NULL);
          }
        else
          {
             _e_gesture_edge_drag_cancel();
             if (gesture->gesture_events.event_keep)
               _e_gesture_event_flush();
          }
     }
   else
     {
        edge_drags->base.enabled_finger &= ~(1 << (gesture->gesture_events.num_pressed - 1));
        if (edge_drags->start_timer == NULL)
          {
             _e_gesture_edge_drag_cancel();
             if (gesture->gesture_events.event_keep)
               _e_gesture_event_flush();
          }
     }
}

static void
_e_gesture_process_edge_drag_move(Ecore_Event_Mouse_Move *ev)
{
   E_Gesture_Event_Edge_Drag *edge_drags = &gesture->gesture_events.edge_drags;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   double distance;
   Coords current_point = {0, };
   unsigned int idx = ev->multi.device+1;

   if (!edge_drags->base.activation.active) return;

   if (!(edge_drags->base.enabled_finger & (1 << idx)))
     return;

   if (edge_drags->start_timer) return;

   _e_gesture_util_center_axis_get(gesture->gesture_events.num_pressed, &current_point.x, &current_point.y);
   distance = _e_gesture_util_distance_get(edge_drags->center_point.x, edge_drags->center_point.y, current_point.x, current_point.y);
   if (distance < (double)conf->edge_drag.diff_length)
     {
        return;
     }

   edge_drags->center_point.x = current_point.x;
   edge_drags->center_point.y = current_point.y;

   _e_gesture_send_edge_drag(edge_drags->idx,
      edge_drags->center_point.x, edge_drags->center_point.y,
      edge_drags->base.edge, E_GESTURE_MODE_UPDATE);
}

static void
_e_gesture_process_edge_drag_up(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Edge_Drag *edge_drags = &gesture->gesture_events.edge_drags;

   if (!edge_drags->base.activation.active) return;
   _e_gesture_edge_drag_cancel();
   if (gesture->gesture_events.event_keep)
     _e_gesture_event_flush();
}


static void
_e_gesture_pan_send(int mode, int fingers, int cx, int cy, struct wl_resource *res, struct wl_client *client)
{
   Ecore_Event_Mouse_Button *ev_cancel;
   E_Event_Gesture_Pan *ev_pan;

   if (mode == E_GESTURE_MODE_BEGIN)
     {
        ev_cancel = E_NEW(Ecore_Event_Mouse_Button, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_cancel);

        ev_cancel->timestamp = (int)(ecore_time_get()*1000);
        ev_cancel->same_screen = 1;

        ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_CANCEL, ev_cancel, NULL, NULL);
     }

   GTINF("Send pan gesture %d fingers. (%d, %d) to client: %p, mode: %d\n", fingers, cx, cy, client, mode);
   if (client == E_GESTURE_SERVER_CLIENT)
     {
        ev_pan = E_NEW(E_Event_Gesture_Pan, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_pan);

        ev_pan->mode = mode;
        ev_pan->fingers = fingers;
        ev_pan->cx = cx;
        ev_pan->cy = cy;

        ecore_event_add(E_EVENT_GESTURE_PAN, ev_pan, NULL, NULL);
     }

   gesture->gesture_events.recognized_gesture |= E_GESTURE_TYPE_PAN;
}

static void
_e_gesture_pan_cancel(void)
{
   E_Gesture_Event_Pan *pans = &gesture->gesture_events.pans;

   if (pans->start_timer)
     {
        ecore_timer_del(pans->start_timer);
        pans->start_timer = NULL;
     }
   if (pans->move_timer)
     {
        ecore_timer_del(pans->move_timer);
        pans->move_timer = NULL;
     }

   if (pans->state == E_GESTURE_PANPINCH_STATE_MOVING)
     _e_gesture_pan_send(E_GESTURE_MODE_END, pans->num_pan_fingers, 0, 0,
                         pans->fingers[pans->num_pan_fingers].res,
                         pans->fingers[pans->num_pan_fingers].client);

   gesture->gesture_filter |= E_GESTURE_TYPE_PAN;
   pans->state = E_GESTURE_PANPINCH_STATE_DONE;
}

static Eina_Bool
_e_gesture_timer_pan_start(void *data)
{
   E_Gesture_Event_Pan *pans = &gesture->gesture_events.pans;
   int num_pressed = gesture->gesture_events.num_pressed;
   int i;

   if (pans->fingers[num_pressed].client)
     {
        for (i = 1; i <= num_pressed; i++)
          {
             pans->start_point.x += gesture->gesture_events.base_point[i].axis.x;
             pans->start_point.y += gesture->gesture_events.base_point[i].axis.y;
          }
        pans->center_point.x = pans->start_point.x = (int)(pans->start_point.x / num_pressed);
        pans->center_point.y = pans->start_point.y = (int)(pans->start_point.y / num_pressed);
        pans->state = E_GESTURE_PANPINCH_STATE_START;
     }
   else
     {
        _e_gesture_pan_cancel();
     }
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_process_pan_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Pan *pans = &gesture->gesture_events.pans;

   if (!pans->activation.active) return;

   if (gesture->gesture_events.recognized_gesture &&
       !((gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_PAN) ||
       (gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_PINCH)))
     _e_gesture_pan_cancel();

   if (gesture->gesture_events.num_pressed == 1)
     {
        pans->state = E_GESTURE_PANPINCH_STATE_READY;
        if (pans->start_timer) ecore_timer_del(pans->start_timer);
        pans->start_timer = ecore_timer_add(E_GESTURE_PAN_START_TIME, _e_gesture_timer_pan_start, NULL);
     }
}

static void
_e_gesture_process_pan_move(Ecore_Event_Mouse_Move *ev)
{
   E_Gesture_Event_Pan *pans = &gesture->gesture_events.pans;
   Coords cur_point = {0,};
   int idx, diff_x, diff_y, mode;

   if (!pans->activation.active) return;

   if (gesture->gesture_events.recognized_gesture &&
       !((gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_PAN) ||
       (gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_PINCH)))
     _e_gesture_pan_cancel();

   idx = gesture->gesture_events.num_pressed;
   if (idx <= 0) return;
   if (pans->state == E_GESTURE_PANPINCH_STATE_READY) return;

   _e_gesture_util_center_axis_get(gesture->gesture_events.num_pressed, &cur_point.x, &cur_point.y);

   diff_x = cur_point.x - pans->center_point.x;
   diff_y = cur_point.y - pans->center_point.y;

   if ((ABS(diff_x) > E_GESTURE_PAN_MOVING_RANGE) || (ABS(diff_y) > E_GESTURE_PAN_MOVING_RANGE))
     {
        switch (pans->state)
          {
             case E_GESTURE_PANPINCH_STATE_START:
                mode = E_GESTURE_MODE_BEGIN;
                pans->state = E_GESTURE_PANPINCH_STATE_MOVING;
                pans->num_pan_fingers = idx;
                break;
             case E_GESTURE_PANPINCH_STATE_MOVING:
                mode = E_GESTURE_MODE_UPDATE;
                break;
             default:
                return;
          }

        if (ABS(diff_x) > E_GESTURE_PAN_MOVING_RANGE) pans->center_point.x = cur_point.x;
        if (ABS(diff_y) > E_GESTURE_PAN_MOVING_RANGE) pans->center_point.y = cur_point.y;

        _e_gesture_pan_send(mode, idx, cur_point.x, cur_point.y, pans->fingers[idx].res, pans->fingers[idx].client);

        if (mode == E_GESTURE_MODE_BEGIN)
          mode = E_GESTURE_MODE_UPDATE;
     }
}

static void
_e_gesture_process_pan_up(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Pan *pans = &gesture->gesture_events.pans;

   if (!pans->activation.active) return;
   _e_gesture_pan_cancel();
}

static void
_e_gesture_pinch_send(int mode, int fingers, double distance, double angle, int cx, int cy, struct wl_resource *res, struct wl_client *client)
{
   Ecore_Event_Mouse_Button *ev_cancel;
   E_Event_Gesture_Pinch *ev_pinch;

   if (mode == E_GESTURE_MODE_BEGIN)
     {
        ev_cancel = E_NEW(Ecore_Event_Mouse_Button, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_cancel);

        ev_cancel->timestamp = (int)(ecore_time_get()*1000);
        ev_cancel->same_screen = 1;

        ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_CANCEL, ev_cancel, NULL, NULL);
     }

   GTINF("Send pinch gesture (fingers: %d, distance: %lf, angle: %lf, cx: %d, cy: %d) to client: %p, mode: %d\n", fingers, distance, angle, cx, cy, client, mode);
   if (client == E_GESTURE_SERVER_CLIENT)
     {
        ev_pinch = E_NEW(E_Event_Gesture_Pinch, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_pinch);

        ev_pinch->mode = mode;
        ev_pinch->fingers = fingers;
        ev_pinch->distance = distance;
        ev_pinch->angle = angle;
        ev_pinch->cx = cx;
        ev_pinch->cy = cy;

        ecore_event_add(E_EVENT_GESTURE_PINCH, ev_pinch, NULL, NULL);
     }

   gesture->gesture_events.recognized_gesture |= E_GESTURE_TYPE_PINCH;
}

static void
_e_gesture_pinch_cancel(void)
{
   E_Gesture_Event_Pinch *pinchs = &gesture->gesture_events.pinchs;

   if (pinchs->start_timer)
     {
        ecore_timer_del(pinchs->start_timer);
        pinchs->start_timer = NULL;
     }
   if (pinchs->move_timer)
     {
        ecore_timer_del(pinchs->move_timer);
        pinchs->move_timer = NULL;
     }

   if (pinchs->state == E_GESTURE_PANPINCH_STATE_MOVING)
     _e_gesture_pinch_send(E_GESTURE_MODE_END, pinchs->num_pinch_fingers, 0.0, 0.0, 0, 0,
                           pinchs->fingers[pinchs->num_pinch_fingers].res,
                           pinchs->fingers[pinchs->num_pinch_fingers].client);

   gesture->gesture_filter |= E_GESTURE_TYPE_PINCH;
   pinchs->state = E_GESTURE_PANPINCH_STATE_DONE;
}

static Eina_Bool
_e_gesture_timer_pinch_start(void *data)
{
   E_Gesture_Event_Pinch *pinch = &gesture->gesture_events.pinchs;
   int num_pressed = gesture->gesture_events.num_pressed;

   if (pinch->fingers[num_pressed].client)
     {
        pinch->distance = _e_gesture_util_distances_get(num_pressed);
        pinch->state = E_GESTURE_PANPINCH_STATE_START;
     }
   else
     {
        _e_gesture_pinch_cancel();
     }
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_process_pinch_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Pinch *pinch = &gesture->gesture_events.pinchs;

   if (!pinch->activation.active) return;

   if (gesture->gesture_events.recognized_gesture &&
       !((gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_PAN) ||
       (gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_PINCH)))
     _e_gesture_pinch_cancel();

   if (gesture->gesture_events.num_pressed == 1)
     {
        pinch->state = E_GESTURE_PANPINCH_STATE_READY;
        if (pinch->start_timer) ecore_timer_del(pinch->start_timer);
        pinch->start_timer = ecore_timer_add(E_GESTURE_PAN_START_TIME, _e_gesture_timer_pinch_start, NULL);
     }
}

static void
_e_gesture_process_pinch_move(Ecore_Event_Mouse_Move *ev)
{
   E_Gesture_Event_Pinch *pinch = &gesture->gesture_events.pinchs;
   int idx, mode, cx = 0, cy = 0;
   double current_distance = 0.0, diff = 0.0, angle = 0.0;

   if (!pinch->activation.active) return;

   if (gesture->gesture_events.recognized_gesture &&
       !((gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_PAN) ||
       (gesture->gesture_events.recognized_gesture & E_GESTURE_TYPE_PINCH)))
     _e_gesture_pan_cancel();

   idx = gesture->gesture_events.num_pressed;
   if (idx <= 0) return;
   if (pinch->state == E_GESTURE_PANPINCH_STATE_READY) return;

   current_distance = _e_gesture_util_distances_get(idx);
   diff = current_distance - pinch->distance;

   if (ABS(diff) > E_GESTURE_PINCH_MOVING_DISTANCE_RANGE)
     {
        pinch->distance = current_distance;
        switch (pinch->state)
          {
             case E_GESTURE_PANPINCH_STATE_START:
                mode = E_GESTURE_MODE_BEGIN;
                pinch->state = E_GESTURE_PANPINCH_STATE_MOVING;
                pinch->num_pinch_fingers = idx;
                break;
             case E_GESTURE_PANPINCH_STATE_MOVING:
                mode = E_GESTURE_MODE_UPDATE;
                break;
             default:
                return;
          }

        if (idx == 2) angle = _e_gesture_util_angle_get(gesture->gesture_events.base_point[1].axis.x,
                                                        gesture->gesture_events.base_point[1].axis.y,
                                                        gesture->gesture_events.base_point[2].axis.x,
                                                        gesture->gesture_events.base_point[2].axis.y);
        else angle = 0.0;

        _e_gesture_util_center_axis_get(idx, &cx, &cy);

        _e_gesture_pinch_send(mode, idx, pinch->distance, angle, cx, cy, pinch->fingers[idx].res, pinch->fingers[idx].client);
     }
}

static void
_e_gesture_process_pinch_up(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Pinch *pinch = &gesture->gesture_events.pinchs;

   if (!pinch->activation.active) return;
   _e_gesture_pinch_cancel();
}


unsigned int
e_gesture_util_tap_max_fingers_get(void)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;
   int i;
   unsigned int max = 0;

   for (i = 0; i < E_GESTURE_FINGER_MAX +1; i++)
     {
        if (taps->fingers[i].enabled) max = i;
     }

   return max;
}

unsigned int
e_gesture_util_tap_max_repeats_get(unsigned int fingers)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   int i;
   unsigned int max = 0;

   for (i = 0; i < conf->tap.repeats_max + 1; i++)
     {
        if (taps->fingers[fingers].repeats[i].client) max = i;
     }

   return max;
}

static void
_e_gesture_tap_cancel(void)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;

   if (taps->start_timer)
     {
        ecore_timer_del(taps->start_timer);
        taps->start_timer = NULL;
     }
   if (taps->done_timer)
     {
        ecore_timer_del(taps->done_timer);
        taps->done_timer = NULL;
     }
   if (taps->interval_timer)
     {
        ecore_timer_del(taps->interval_timer);
        taps->interval_timer = NULL;
     }

   taps->repeats = 0;
   taps->enabled_finger = 0;
   taps->current_finger = 0;
   taps->state = E_GESTURE_TAP_STATE_READY;
   taps->base_rect.x1 = 0;
   taps->base_rect.y1 = 0;
   taps->base_rect.x2 = 0;
   taps->base_rect.y2 = 0;
   gesture->gesture_filter |= E_GESTURE_TYPE_TAP;
   _e_gesture_event_flush();
   gesture->gesture_events.recognized_gesture &= ~E_GESTURE_TYPE_TAP;
}

static void
_e_gesture_send_tap(int fingers, int repeats, struct wl_client *client, struct wl_resource *res)
{
   E_Event_Gesture_Tap *ev_tap;

   GTINF("Send Tap gesture. %d fingers %d repeats to client (%p)\n", fingers, repeats, client);
   if (client == E_GESTURE_SERVER_CLIENT)
     {
        ev_tap = E_NEW(E_Event_Gesture_Tap, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_tap);

        ev_tap->mode = E_GESTURE_MODE_DONE;
        ev_tap->fingers = fingers;
        ev_tap->repeats = repeats;

        ecore_event_add(E_EVENT_GESTURE_TAP, ev_tap, NULL, NULL);
     }
   else
     tizen_gesture_send_tap(res, E_GESTURE_MODE_DONE, fingers, repeats);
   _e_gesture_event_drop();
   gesture->gesture_events.recognized_gesture |= E_GESTURE_TYPE_TAP;

   _e_gesture_tap_cancel();
}

static Eina_Bool
_e_gesture_timer_tap_start(void *data)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;

   if (taps->fingers[taps->enabled_finger].enabled)
     {
        taps->state = E_GESTURE_TAP_STATE_PROCESS;
     }
   else
     {
        _e_gesture_tap_cancel();
     }

   taps->start_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_gesture_timer_tap_done(void *data)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;

   if (gesture->gesture_events.num_pressed)
     {
        _e_gesture_tap_cancel();
     }

   taps->done_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_gesture_timer_tap_interval(void *data)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;

   if (taps->fingers[taps->enabled_finger].repeats[taps->repeats].client)
     {
        _e_gesture_send_tap(taps->enabled_finger, taps->repeats,
           taps->fingers[taps->enabled_finger].repeats[taps->repeats].client,
           taps->fingers[taps->enabled_finger].repeats[taps->repeats].res);
        gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
        gesture->gesture_events.recognized_gesture &= ~E_GESTURE_TYPE_TAP;
        gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;
     }
   else
     {
        /* All fingers are released. */
        gesture->gesture_filter = E_GESTURE_TYPE_ALL;
        _e_gesture_tap_cancel();

        gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
        gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;
     }

   taps->interval_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_tap_start(void)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   taps->state = E_GESTURE_TAP_STATE_START;
   if (!taps->start_timer)
     {
        taps->start_timer = ecore_timer_add(conf->tap.time_start, _e_gesture_timer_tap_start, NULL);
     }
   if (!taps->done_timer)
     {
        taps->done_timer = ecore_timer_add(conf->tap.time_done, _e_gesture_timer_tap_done, NULL);
     }
}

static void
_e_gesture_tap_done(void)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   if (taps->repeats >= conf->tap.repeats_max)
     _e_gesture_tap_cancel();

   if (!taps->fingers[taps->enabled_finger].enabled)
      _e_gesture_tap_cancel();

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_TAP) &&
       gesture->gesture_events.num_pressed == 0)
     {
        taps->state = E_GESTURE_TAP_STATE_WAIT;
        taps->repeats++;
        if (taps->done_timer)
          {
             ecore_timer_del(taps->done_timer);
             taps->done_timer = NULL;
          }
        taps->current_finger = 0;
        if (taps->repeats == taps->fingers[taps->enabled_finger].max_repeats)
          {
             ecore_timer_del(taps->interval_timer);
             _e_gesture_timer_tap_interval(NULL);
          }
        else
          {
             if (!taps->interval_timer)
               {
                  taps->interval_timer = ecore_timer_add(conf->tap.time_interval, _e_gesture_timer_tap_interval, NULL);
               }
          }
     }
}

static void
_e_gesture_process_tap_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;

   if (!taps->activation.active) return;

   if (gesture->gesture_events.recognized_gesture)
     _e_gesture_tap_cancel();

   if (taps->enabled_finger < gesture->gesture_events.num_pressed)
       taps->enabled_finger = gesture->gesture_events.num_pressed;

   taps->current_finger++;

   if (taps->enabled_finger > taps->max_fingers)
     _e_gesture_tap_cancel();

   if (taps->state == E_GESTURE_TAP_STATE_READY ||
       taps->state == E_GESTURE_TAP_STATE_START ||
       taps->state == E_GESTURE_TAP_STATE_PROCESS)
     {
        _e_gesture_util_rect_get(taps->enabled_finger, &taps->base_rect.x1, &taps->base_rect.y1, &taps->base_rect.x2, &taps->base_rect.y2);
     }

   switch (taps->state)
     {
        case E_GESTURE_TAP_STATE_NONE:
           return;

        case E_GESTURE_TAP_STATE_READY:
           _e_gesture_tap_start();
           break;

        case E_GESTURE_TAP_STATE_START:
           break;

        case E_GESTURE_TAP_STATE_PROCESS:
           _e_gesture_tap_cancel();
           break;

        case E_GESTURE_TAP_STATE_WAIT:
           if (taps->interval_timer)
             {
                ecore_timer_del(taps->interval_timer);
                taps->interval_timer = NULL;
             }
           _e_gesture_tap_start();
           break;

        case E_GESTURE_TAP_STATE_DONE:
           break;

        default:
           break;
     }
}

static void
_e_gesture_process_tap_move(Ecore_Event_Mouse_Move *ev)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   Rect current_rect = {0, };
   int xx1, yy1, xx2, yy2;

   if (!taps->activation.active) return;

   if (gesture->gesture_events.recognized_gesture)
     _e_gesture_tap_cancel();

   _e_gesture_util_rect_get(taps->enabled_finger, &current_rect.x1, &current_rect.y1, &current_rect.x2, &current_rect.y2);

   xx1 = taps->base_rect.x1 - current_rect.x1;
   yy1 = taps->base_rect.y1 - current_rect.y1;
   xx2 = taps->base_rect.x2 - current_rect.x2;
   yy2 = taps->base_rect.y2 - current_rect.y2;

   if (ABS(xx1) > conf->tap.moving_range ||
       ABS(yy1) > conf->tap.moving_range ||
       ABS(xx2) > conf->tap.moving_range ||
       ABS(yy2) > conf->tap.moving_range)
     {
        GTDBG("%d finger moving too large diff: (%d, %d)(%d, %d)\n", ev->multi.device, xx1, yy1, xx2, yy2);
        _e_gesture_tap_cancel();
     }
}

static void
_e_gesture_process_tap_up(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Tap *taps = &gesture->gesture_events.taps;

   if (!taps->activation.active) return;

   if (gesture->gesture_events.recognized_gesture)
     _e_gesture_tap_cancel();

   if (taps->enabled_finger != taps->current_finger)
     _e_gesture_tap_cancel();

   switch (taps->state)
     {
        case E_GESTURE_TAP_STATE_NONE:
           return;

        case E_GESTURE_TAP_STATE_READY:
           _e_gesture_tap_cancel();
           break;

        case E_GESTURE_TAP_STATE_START:
           taps->state = E_GESTURE_TAP_STATE_PROCESS;
           if (taps->start_timer)
             {
                ecore_timer_del(taps->start_timer);
                taps->start_timer = NULL;
             }
           _e_gesture_tap_done();
           break;

        case E_GESTURE_TAP_STATE_PROCESS:
           _e_gesture_tap_done();
           break;

        case E_GESTURE_TAP_STATE_WAIT:
           if (taps->interval_timer)
             {
                ecore_timer_del(taps->interval_timer);
                taps->interval_timer = NULL;
             }
           _e_gesture_tap_start();
           break;

        case E_GESTURE_TAP_STATE_DONE:
           break;

        default:
           break;
     }
}

static E_Gesture_Event_State
_e_gesture_process_mouse_button_down(void *event)
{
   Ecore_Event_Mouse_Button *ev = event;

   if (!gesture->grabbed_gesture)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }
   if (e_gesture_is_touch_device(ev->dev) == EINA_FALSE)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }
   if (ev->multi.device > E_GESTURE_FINGER_MAX)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }

   gesture->gesture_events.base_point[ev->multi.device + 1].pressed = EINA_TRUE;
   gesture->gesture_events.base_point[ev->multi.device + 1].axis.x = ev->x;
   gesture->gesture_events.base_point[ev->multi.device + 1].axis.y = ev->y;

   if (gesture->gesture_events.recognized_gesture)
     {
        return E_GESTURE_EVENT_STATE_IGNORE;
     }

   if (gesture->gesture_events.num_pressed == 1)
     {
        if (gesture->gesture_filter == E_GESTURE_TYPE_ALL)
          {
             gesture->event_state = E_GESTURE_EVENT_STATE_PROPAGATE;
          }
     }

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_EDGE_SWIPE))
     {
        _e_gesture_process_edge_swipe_down(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_EDGE_DRAG))
     {
        _e_gesture_process_edge_drag_down(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PAN))
     {
        _e_gesture_process_pan_down(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PINCH))
     {
        _e_gesture_process_pinch_down(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_TAP))
     {
        _e_gesture_process_tap_down(ev);
     }

   return gesture->event_state;
}

static E_Gesture_Event_State
_e_gesture_process_mouse_button_up(void *event)
{
   Ecore_Event_Mouse_Button *ev = event;

   if (!gesture->grabbed_gesture)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }
   if (e_gesture_is_touch_device(ev->dev) == EINA_FALSE)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }
   if (ev->multi.device > E_GESTURE_FINGER_MAX)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }

   gesture->gesture_events.base_point[ev->multi.device + 1].pressed = EINA_FALSE;
   gesture->gesture_events.base_point[ev->multi.device + 1].axis.x = 0;
   gesture->gesture_events.base_point[ev->multi.device + 1].axis.y = 0;

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_EDGE_SWIPE))
     {
        _e_gesture_process_edge_swipe_up(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_EDGE_DRAG))
     {
        _e_gesture_process_edge_drag_up(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PAN))
     {
        _e_gesture_process_pan_up(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PINCH))
     {
        _e_gesture_process_pinch_up(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_TAP))
     {
        _e_gesture_process_tap_up(ev);
     }

   if (gesture->gesture_events.recognized_gesture)
     {
        if (gesture->gesture_events.num_pressed == 0)
          {
             gesture->gesture_events.recognized_gesture = 0x0;
          }
        return E_GESTURE_EVENT_STATE_IGNORE;
     }

   return gesture->event_state;
}


static E_Gesture_Event_State
_e_gesture_process_mouse_move(void *event)
{
   Ecore_Event_Mouse_Move *ev = event;

   if (e_gesture_is_touch_device(ev->dev) == EINA_FALSE)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }
   if (ev->multi.device > E_GESTURE_FINGER_MAX)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }
   if (gesture->gesture_events.num_pressed == 0)
     {
        return gesture->event_state;
     }
   if (!gesture->grabbed_gesture)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }

   if (gesture->gesture_events.base_point[ev->multi.device + 1].pressed != EINA_TRUE)
     {
        return gesture->event_state;
     }
   gesture->gesture_events.base_point[ev->multi.device + 1].axis.x = ev->x;
   gesture->gesture_events.base_point[ev->multi.device + 1].axis.y = ev->y;

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_EDGE_SWIPE))
     {
        _e_gesture_process_edge_swipe_move(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_EDGE_DRAG))
     {
        _e_gesture_process_edge_drag_move(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PAN))
     {
        _e_gesture_process_pan_move(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PINCH))
     {
        _e_gesture_process_pinch_move(ev);
     }
   if (!(gesture->gesture_filter & E_GESTURE_TYPE_TAP))
     {
        _e_gesture_process_tap_move(ev);
     }

   return gesture->event_state;
}

static E_Gesture_Event_State
_e_gesture_process_key_down(void *event)
{
   Ecore_Event_Key *ev = event;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   if (ev->keycode == conf->edge_swipe.compose_key)
     {
        gesture->gesture_events.edge_swipes.combined_keycode = conf->edge_swipe.compose_key;
     }

   return E_GESTURE_EVENT_STATE_PROPAGATE;
}

static E_Gesture_Event_State
_e_gesture_process_key_up(void *event)
{
   Ecore_Event_Key *ev = event;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   if (ev->keycode == conf->edge_swipe.compose_key)
     {
        gesture->gesture_events.edge_swipes.combined_keycode = 0;
     }

   return E_GESTURE_EVENT_STATE_PROPAGATE;
}

static void
_e_gesture_send_palm_cover(void)
{
   Ecore_Event_Mouse_Button *ev_cancel;
   E_Gesture_Event_Palm_Cover *palm_covers = &gesture->gesture_events.palm_covers;
   int duration = 0, time;
   int cx = 0, cy = 0;
   unsigned int size = 0;
   wl_fixed_t pressure;
   struct wl_resource *surface = NULL, *res = NULL, *focus_surface = NULL;
   Eina_List *l;
   E_Gesture_Select_Surface *sdata;
   E_Event_Gesture_Palm_Cover *ev_palm_cover;

   time = (int)(ecore_time_get()*1000);

   if (gesture->event_state == E_GESTURE_EVENT_STATE_KEEP)
     {
        _e_gesture_event_drop();
     }
   else if (gesture->event_state == E_GESTURE_EVENT_STATE_PROPAGATE)
     {
        ev_cancel = E_NEW(Ecore_Event_Mouse_Button, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_cancel);

        ev_cancel->timestamp = time;
        ev_cancel->same_screen = 1;

        ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_CANCEL, ev_cancel, NULL, NULL);
     }

   _e_gesture_util_center_axis_get(gesture->gesture_events.num_pressed, &cx, &cy);

   GTINF("Send palm_cover gesture to client: %p\n", palm_covers->client_info.client);

   if (palm_covers->client_info.client == E_GESTURE_SERVER_CLIENT)
     {
        ev_palm_cover = E_NEW(E_Event_Gesture_Palm_Cover, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_palm_cover);

        ev_palm_cover->mode = E_GESTURE_MODE_BEGIN;
        ev_palm_cover->duration = duration;
        ev_palm_cover->cx = cx;
        ev_palm_cover->cy = cy;
        ev_palm_cover->size = size;
        ev_palm_cover->pressure = 0.0;
        ecore_event_add(E_EVENT_GESTURE_PALM_COVER, ev_palm_cover, NULL, NULL);

        ev_palm_cover = E_NEW(E_Event_Gesture_Palm_Cover, 1);
        EINA_SAFETY_ON_NULL_RETURN(ev_palm_cover);

        ev_palm_cover->mode = E_GESTURE_MODE_END;
        ev_palm_cover->duration = duration;
        ev_palm_cover->cx = cx;
        ev_palm_cover->cy = cy;
        ev_palm_cover->size = size;
        ev_palm_cover->pressure = 0.0;
        ecore_event_add(E_EVENT_GESTURE_PALM_COVER, ev_palm_cover, NULL, NULL);
     }
   else
     {
        pressure = wl_fixed_from_double(0.0);

        focus_surface = _e_gesture_util_eclient_surface_get(e_client_focused_get());
        if (focus_surface)
          {
             EINA_LIST_FOREACH(palm_covers->select_surface_list, l, sdata)
               {
                  if (focus_surface == sdata->surface)
                    {
                       surface = sdata->surface;
                       res = sdata->res;
                       break;
                    }
               }
          }

        if (!surface || !res)
          {
             res = palm_covers->client_info.res;
             surface = NULL;
          }

        tizen_gesture_send_palm_cover(palm_covers->client_info.res, surface, E_GESTURE_MODE_BEGIN, duration, size, pressure, cx, cy);
        tizen_gesture_send_palm_cover(palm_covers->client_info.res, surface, E_GESTURE_MODE_END, duration, size, pressure, cx, cy);
     }

   gesture->event_state = E_GESTURE_EVENT_STATE_IGNORE;
   gesture->gesture_events.recognized_gesture |= E_GESTURE_TYPE_PALM_COVER;
}

static void
_e_gesture_process_palm_cover(int val)
{
   if (gesture->gesture_events.recognized_gesture)
     {
        return;
     }

   _e_gesture_send_palm_cover();
}

static void
_e_gesture_process_palm(int val)
{
   if (val <= 0) return;
   if (!gesture->grabbed_gesture) return;

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PALM_COVER) &&
       gesture->gesture_events.palm_covers.activation.active)
     {
        _e_gesture_process_palm_cover(val);
     }
}

static E_Gesture_Event_State
_e_gesture_process_axis_update(void *event)
{
   Ecore_Event_Axis_Update *ev = event;
   int i;

   for (i = 0; i < ev->naxis; i++)
     {
        if (ev->axis[i].label == ECORE_AXIS_LABEL_TOUCH_PALM)
          {
             _e_gesture_process_palm((int)ev->axis[i].value);
          }
     }
   return E_GESTURE_EVENT_STATE_PROPAGATE;
}

void
e_gesture_event_deactivate_check(void)
{
   if (gesture->gesture_events.num_pressed <= 0) return;
   if (gesture->gesture_filter == E_GESTURE_TYPE_ALL) return;

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_EDGE_SWIPE) &&
       gesture->gesture_events.edge_swipes.base.activation.active)
     {
        _e_gesture_edge_swipe_cancel();
     }

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_TAP) &&
       gesture->gesture_events.taps.activation.active)
     {
        _e_gesture_tap_cancel();
     }

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PAN) &&
       gesture->gesture_events.pans.activation.active)
     {
        _e_gesture_pan_cancel();
     }

   if (!(gesture->gesture_filter & E_GESTURE_TYPE_PINCH) &&
       gesture->gesture_events.pinchs.activation.active)
     {
        _e_gesture_pinch_cancel();
     }

}

/* Function for checking the existing grab for a key and sending key event(s) */
Eina_Bool
e_gesture_process_events(void *event, int type)
{
   E_Gesture_Event_State res = E_GESTURE_EVENT_STATE_PROPAGATE;
   Eina_Bool ret = EINA_TRUE;

   if (type == ECORE_EVENT_MOUSE_BUTTON_DOWN)
     res = _e_gesture_process_mouse_button_down(event);
   else if (type == ECORE_EVENT_MOUSE_BUTTON_UP)
     res = _e_gesture_process_mouse_button_up(event);
   else if (type == ECORE_EVENT_MOUSE_MOVE)
     res = _e_gesture_process_mouse_move(event);
   else if (type == ECORE_EVENT_KEY_DOWN)
     res = _e_gesture_process_key_down(event);
   else if (type == ECORE_EVENT_KEY_UP)
     res = _e_gesture_process_key_up(event);
   else if (type == ECORE_EVENT_DEVICE_ADD)
     res = _e_gesture_process_device_add(event);
   else if (type == ECORE_EVENT_DEVICE_DEL)
     res = _e_gesture_process_device_del(event);
   else if (type == ECORE_EVENT_AXIS_UPDATE)
     res = _e_gesture_process_axis_update(event);
   else return ret;

   switch (res)
     {
        case E_GESTURE_EVENT_STATE_PROPAGATE:
           ret = EINA_TRUE;
           break;
        case E_GESTURE_EVENT_STATE_KEEP:
           _e_gesture_event_queue(type, event);
           ret = EINA_FALSE;
           break;
        case E_GESTURE_EVENT_STATE_IGNORE:
           ret = EINA_FALSE;
           break;
        default:
           return ret;
     }

   if (gesture->gesture_events.num_pressed == 0&&
       type == ECORE_EVENT_MOUSE_BUTTON_UP)
     {
        if (gesture->grabbed_gesture & E_GESTURE_TYPE_TAP ||
             ((gesture->grabbed_gesture & E_GESTURE_TYPE_EDGE_SWIPE) &&
              gesture->gesture_events.event_keep))
          gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
        gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;
     }

   return ret;
}
