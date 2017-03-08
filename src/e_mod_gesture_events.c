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
   if (e_info) E_FREE(e_info);
}

static void
_e_gesture_event_flush(void)
{
   Eina_List *l, *l_next;
   E_Gesture_Event_Info *data;

   if (gesture->event_state == E_GESTURE_EVENT_STATE_IGNORE ||
      gesture->gesture_events.recognized_gesture) return;

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

   edge_swipes->enabled_finger = 0x0;
   edge_swipes->edge = E_GESTURE_EDGE_NONE;

   gesture->gesture_filter |= TIZEN_GESTURE_TYPE_EDGE_SWIPE;
}

static void
_e_gesture_keyevent_free(void *data EINA_UNUSED, void *ev)
{
   Ecore_Event_Key *e = ev;

   eina_stringshare_del(e->keyname);
   eina_stringshare_del(e->key);
   eina_stringshare_del(e->compose);

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

   EINA_SAFETY_ON_NULL_RETURN(e_comp_wl->xkb.keymap);

   ev = E_NEW(Ecore_Event_Key, 1);
   EINA_SAFETY_ON_NULL_RETURN(ev);

   ev->key = (char *)eina_stringshare_add("XF86Back");
   ev->keyname = (char *)eina_stringshare_add(ev->key);
   ev->compose = (char *)eina_stringshare_add(ev->key);
   ev->timestamp = (int)(ecore_time_get()*1000);
   ev->same_screen = 1;
   ev->keycode = conf->edge_swipe.back_key;
   ev->dev = gesture->device.kbd_device;

   if (pressed)
     ecore_event_add(ECORE_EVENT_KEY_DOWN, ev, _e_gesture_keyevent_free, NULL);
   else
     ecore_event_add(ECORE_EVENT_KEY_UP, ev, _e_gesture_keyevent_free, NULL);
}

static void
_e_gesture_send_edge_swipe(int fingers, int x, int y, int edge, struct wl_client *client, struct wl_resource *res)
{
   enum tizen_gesture_edge dir = 0;
   Ecore_Event_Mouse_Button *ev_cancel;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;

   switch (edge)
     {
        case E_GESTURE_EDGE_TOP:
           dir = TIZEN_GESTURE_EDGE_TOP;
           break;
        case E_GESTURE_EDGE_LEFT:
           dir = TIZEN_GESTURE_EDGE_LEFT;
           break;
        case E_GESTURE_EDGE_BOTTOM:
           dir = TIZEN_GESTURE_EDGE_BOTTOM;
           break;
        case E_GESTURE_EDGE_RIGHT:
           dir = TIZEN_GESTURE_EDGE_RIGHT;
           break;
     }

   if (edge_swipes->event_keep)
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

   GTINF("Send edge_swipe gesture (edge: %d) to client: %p\n", dir, client);

   if (conf->edge_swipe.default_enable_back &&
       edge == E_GESTURE_EDGE_TOP)
     {
        _e_gesture_send_back_key(EINA_TRUE);
        _e_gesture_send_back_key(EINA_FALSE);
        goto finish;
     }
   
   tizen_gesture_send_edge_swipe(res, fingers, TIZEN_GESTURE_MODE_DONE, x, y, dir);

finish:
   _e_gesture_edge_swipe_cancel();
   gesture->gesture_events.recognized_gesture |= TIZEN_GESTURE_TYPE_EDGE_SWIPE;
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
_e_gesture_timer_edge_swipe_start(void *data)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;
   int idx = gesture->gesture_events.num_pressed;
   int i;

   GTDBG("Edge_Swipe start timer is expired. Currently alived edge_swipe fingers: 0x%x\n", edge_swipes->enabled_finger);

   for (i = E_GESTURE_FINGER_MAX; i > idx; i--)
     {
        edge_swipes->enabled_finger &= ~(1 << i);
     }
   if ((edge_swipes->edge == E_GESTURE_EDGE_TOP && !edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_TOP].client) ||
       (edge_swipes->edge == E_GESTURE_EDGE_LEFT && !edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_LEFT].client) ||
       (edge_swipes->edge == E_GESTURE_EDGE_BOTTOM && !edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_BOTTOM].client) ||
       (edge_swipes->edge == E_GESTURE_EDGE_RIGHT && !edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_RIGHT].client))
     {
        if (edge_swipes->event_keep)
          _e_gesture_event_flush();
        _e_gesture_edge_swipe_cancel();
     }
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_gesture_timer_edge_swipe_done(void *data)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;

   GTDBG("Edge_Swipe done timer is expired. Currently alived edge_swipe fingers: 0x%x\n", edge_swipes->enabled_finger);

   if (edge_swipes->event_keep)
     _e_gesture_event_flush();
   _e_gesture_edge_swipe_cancel();

   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_process_edge_swipe_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   int i;
   unsigned int idx = ev->multi.device+1;

   if (gesture->gesture_events.num_pressed == 1)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             if (edge_swipes->fingers[i].enabled)
               {
                  edge_swipes->enabled_finger |= (1 << i);
               }
          }

        if (ev->y < conf->edge_swipe.area_offset)
          edge_swipes->edge = E_GESTURE_EDGE_TOP;
        else if (ev->y > e_comp->h - conf->edge_swipe.area_offset)
          edge_swipes->edge = E_GESTURE_EDGE_BOTTOM;
        else if (ev->x < conf->edge_swipe.area_offset)
          edge_swipes->edge = E_GESTURE_EDGE_RIGHT;
        else if (ev->x > e_comp->w - conf->edge_swipe.area_offset)
          edge_swipes->edge = E_GESTURE_EDGE_LEFT;

        if (!((1 << (edge_swipes->edge - 1)) & edge_swipes->enabled_edge))
          edge_swipes->edge = E_GESTURE_EDGE_NONE;

        if (edge_swipes->edge != E_GESTURE_EDGE_NONE)
          {
             edge_swipes->fingers[idx].start.x = ev->x;
             edge_swipes->fingers[idx].start.y = ev->y;
             edge_swipes->start_timer = ecore_timer_add(conf->edge_swipe.time_begin, _e_gesture_timer_edge_swipe_start, NULL);
             edge_swipes->done_timer = ecore_timer_add(conf->edge_swipe.time_done, _e_gesture_timer_edge_swipe_done, NULL);
          }
        else
          {
             if (edge_swipes->event_keep)
               _e_gesture_event_flush();
             _e_gesture_edge_swipe_cancel();
          }
     }
   else
     {
        edge_swipes->enabled_finger &= ~(1 << (gesture->gesture_events.num_pressed - 1));
        if (edge_swipes->start_timer == NULL)
          {
             if (edge_swipes->event_keep)
               _e_gesture_event_flush();
             _e_gesture_edge_swipe_cancel();
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

   if (!(edge_swipes->enabled_finger & (1 << idx)))
     return;

   diff.x = ABS(edge_swipes->fingers[idx].start.x - ev->x);
   diff.y = ABS(edge_swipes->fingers[idx].start.y - ev->y);

   switch(edge_swipes->edge)
     {
        case E_GESTURE_EDGE_TOP:
           if (diff.x > conf->edge_swipe.min_length)
             {
                if (edge_swipes->event_keep)
                  _e_gesture_event_flush();
                _e_gesture_edge_swipe_cancel();
                break;
             }
           if (diff.y > conf->edge_swipe.max_length)
             {
                _e_gesture_send_edge_swipe(idx, edge_swipes->fingers[idx].start.x, edge_swipes->fingers[idx].start.y, edge_swipes->edge, edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_TOP].client, edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_TOP].res);
             }
           break;
        case E_GESTURE_EDGE_LEFT:
           if (diff.y > conf->edge_swipe.min_length)
             {
                if (edge_swipes->event_keep)
                  _e_gesture_event_flush();
                _e_gesture_edge_swipe_cancel();
                break;
             }
           if (diff.x > conf->edge_swipe.max_length)
             {
                _e_gesture_send_edge_swipe(idx, edge_swipes->fingers[idx].start.x, edge_swipes->fingers[idx].start.y, edge_swipes->edge, edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_LEFT].client, edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_LEFT].res);
             }
           break;
        case E_GESTURE_EDGE_BOTTOM:
           if (diff.x > conf->edge_swipe.min_length)
             {
                if (edge_swipes->event_keep)
                  _e_gesture_event_flush();
                _e_gesture_edge_swipe_cancel();
                break;
             }
           if (diff.y > conf->edge_swipe.max_length)
             {
                _e_gesture_send_edge_swipe(idx, edge_swipes->fingers[idx].start.x, edge_swipes->fingers[idx].start.y, edge_swipes->edge, edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_BOTTOM].client, edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_BOTTOM].res);
             }
           break;
        case E_GESTURE_EDGE_RIGHT:
           if (diff.y > conf->edge_swipe.min_length)
             {
                if (edge_swipes->event_keep)
                  _e_gesture_event_flush();
                _e_gesture_edge_swipe_cancel();
                break;
             }
           if (diff.x > conf->edge_swipe.max_length)
             {
                _e_gesture_send_edge_swipe(idx, edge_swipes->fingers[idx].start.x, edge_swipes->fingers[idx].start.y, edge_swipes->edge, edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_RIGHT].client, edge_swipes->fingers[idx].edge[E_GESTURE_EDGE_RIGHT].res);
             }
           break;
        default:
           GTWRN("Invalid edge(%d)\n", edge_swipes->edge);
           break;
     }
}

static void
_e_gesture_process_edge_swipe_up(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;

   if (edge_swipes->event_keep)
     _e_gesture_event_flush();
   _e_gesture_edge_swipe_cancel();
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

   if (!(gesture->gesture_filter & TIZEN_GESTURE_TYPE_EDGE_SWIPE))
     {
        _e_gesture_process_edge_swipe_down(ev);
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

   if (gesture->gesture_events.recognized_gesture)
     {
        if (gesture->gesture_events.num_pressed == 0)
          {
             gesture->gesture_events.recognized_gesture = 0x0;
          }
        return E_GESTURE_EVENT_STATE_IGNORE;
     }

   if (!(gesture->gesture_filter & TIZEN_GESTURE_TYPE_EDGE_SWIPE))
     {
        _e_gesture_process_edge_swipe_up(ev);
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
   if (gesture->gesture_events.num_pressed == 0)
     {
        return gesture->event_state;
     }
   if (!gesture->grabbed_gesture)
     {
        return E_GESTURE_EVENT_STATE_PROPAGATE;
     }
   if (gesture->gesture_events.recognized_gesture)
     {
        return E_GESTURE_EVENT_STATE_IGNORE;
     }

   if (!(gesture->gesture_filter & TIZEN_GESTURE_TYPE_EDGE_SWIPE))
     {
        _e_gesture_process_edge_swipe_move(ev);
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
        if ((gesture->grabbed_gesture & TIZEN_GESTURE_TYPE_EDGE_SWIPE) &&
            gesture->gesture_events.edge_swipes.event_keep)
        gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
        gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;
     }

   return ret;
}
