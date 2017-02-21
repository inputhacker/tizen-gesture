#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>
#include "e_service_quickpanel.h"

E_GesturePtr gesture = NULL;
E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Gesture Module of Window Manager" };

static E_Gesture_Config_Data *_e_gesture_init(E_Module *m);
static void _e_gesture_init_handlers(void);
static void _e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data);

static void
_e_gesture_set_client_to_list(E_Gesture_Grabbed_Client *gclient, struct wl_client *client, int mode, int fingers, int edge, int repeat)
{
   switch (mode)
     {
        case TIZEN_GESTURE_TYPE_EDGE_SWIPE:
           if (edge & TIZEN_GESTURE_EDGE_TOP)
             gclient->edge_swipe_fingers[fingers].edge[E_GESTURE_EDGE_TOP].client = client;
           if (edge & TIZEN_GESTURE_EDGE_RIGHT)
             gclient->edge_swipe_fingers[fingers].edge[E_GESTURE_EDGE_RIGHT].client = client;
           if (edge & TIZEN_GESTURE_EDGE_BOTTOM)
             gclient->edge_swipe_fingers[fingers].edge[E_GESTURE_EDGE_BOTTOM].client = client;
           if (edge & TIZEN_GESTURE_EDGE_LEFT)
             gclient->edge_swipe_fingers[fingers].edge[E_GESTURE_EDGE_LEFT].client = client;
           break;
        case TIZEN_GESTURE_TYPE_TAP:
           gclient->tap_fingers[fingers].repeats[repeat].client = client;
           break;
        case TIZEN_GESTURE_TYPE_PAN:
           gclient->pan_fingers[fingers].client = client;
           break;
        case TIZEN_GESTURE_TYPE_PINCH:
           gclient->pinch_fingers[fingers].client = client;
           break;
        default:
           return;
     }
   gclient->grabbed_gesture |= mode;
}

/* Function for registering wl_client destroy listener */
int
e_gesture_add_client_destroy_listener(struct wl_client *client, int mode, int fingers, unsigned int edge, unsigned int repeat)
{
   struct wl_listener *destroy_listener = NULL;
   Eina_List *l;
   E_Gesture_Grabbed_Client *grabbed_client, *data;

   EINA_LIST_FOREACH(gesture->grab_client_list, l, data)
     {
        if (data->client == client)
          {
             _e_gesture_set_client_to_list(data, client, mode, fingers, edge, repeat);

             return TIZEN_GESTURE_ERROR_NONE;
          }
     }

   destroy_listener = E_NEW(struct wl_listener, 1);
   if (!destroy_listener)
     {
        GTERR("Failed to allocate memory for wl_client destroy listener !\n");
        return TIZEN_GESTURE_ERROR_NO_SYSTEM_RESOURCES;
     }

   grabbed_client = E_NEW(E_Gesture_Grabbed_Client, 1);
   if (!grabbed_client)
     {
        GTERR("Failed to allocate memory to save client information !\n");
        return TIZEN_GESTURE_ERROR_NO_SYSTEM_RESOURCES;
     }

   destroy_listener->notify = _e_gesture_wl_client_cb_destroy;
   wl_client_add_destroy_listener(client, destroy_listener);
   grabbed_client->client = client;
   grabbed_client->destroy_listener = destroy_listener;
   _e_gesture_set_client_to_list(grabbed_client, client, mode, fingers, edge, repeat);

   gesture->grab_client_list = eina_list_append(gesture->grab_client_list, grabbed_client);

   return TIZEN_KEYROUTER_ERROR_NONE;
}

static void
_e_gesture_edge_swipe_current_list_check(void)
{
   int i, j;
   E_Gesture_Event *gev;

   gev = &gesture->gesture_events;
   for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
     {
        for (j = 0; j < E_GESTURE_EDGE_MAX+1; j++)
          {
             if (gev->edge_swipes.fingers[i].edge[j].client)
               {
                  return;
               }
          }
        gev->edge_swipes.fingers[i].enabled = EINA_FALSE;
     }
   gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_EDGE_SWIPE;
   if (gev->edge_swipes.event_keep) gesture->event_state = E_GESTURE_EVENT_STATE_PROPAGATE;
}

static void
_e_gesture_tap_current_list_check(void)
{
   int i;
   E_Gesture_Event *gev;

   gev = &gesture->gesture_events;

   gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_TAP;
   gev->taps.state = E_GESTURE_TAP_STATE_NONE;
   gesture->event_state = E_GESTURE_EVENT_STATE_PROPAGATE;
   for (i = 0; i < E_GESTURE_FINGER_MAX; i++)
     {
        if (gev->taps.fingers[i].enabled)
          {
             gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_TAP;
             gev->taps.state = E_GESTURE_TAP_STATE_READY;
             gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
             break;
          }
     }

   gev->taps.max_fingers = e_gesture_util_tap_max_fingers_get();
}

static void
_e_gesture_pan_current_list_check(void)
{
   int i;
   E_Gesture_Event *gev;

   gev = &gesture->gesture_events;

   gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_TAP;
   gev->pans.state = E_GESTURE_PANPINCH_STATE_NONE;

   for (i = 0; i < E_GESTURE_FINGER_MAX; i++)
     {
        if (gev->pans.fingers[i].client)
          {
             gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_PAN;
             gev->pans.state = E_GESTURE_PANPINCH_STATE_READY;
             break;
          }
     }
}

static void
_e_gesture_pinch_current_list_check(void)
{
   int i;
   E_Gesture_Event *gev;

   gev = &gesture->gesture_events;

   gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_PINCH;
   gev->pinchs.state = E_GESTURE_PANPINCH_STATE_NONE;

   for (i = 0; i < E_GESTURE_FINGER_MAX; i++)
     {
        if (gev->pinchs.fingers[i].client)
          {
             gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_PINCH;
             gev->pinchs.state = E_GESTURE_PANPINCH_STATE_READY;
             break;
          }
     }
}

static void
_e_gesture_remove_client_destroy_listener(struct wl_client *client, unsigned int mode, unsigned int fingers, unsigned int edge, unsigned int repeat)
{
   Eina_List *l, *l_next;
   E_Gesture_Grabbed_Client *data;
   int i, j;

   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l, l_next, data)
     {
        if (data->client == client)
          {
             if ((mode & TIZEN_GESTURE_TYPE_EDGE_SWIPE) &&
                 (data->grabbed_gesture & TIZEN_GESTURE_TYPE_EDGE_SWIPE))
               {
                  _e_gesture_set_client_to_list(data, NULL, TIZEN_GESTURE_TYPE_EDGE_SWIPE, fingers, edge, 0);
                  data->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_EDGE_SWIPE;
                  for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                    {
                       if (data->edge_swipe_fingers[i].edge[E_GESTURE_EDGE_TOP].client ||
                           data->edge_swipe_fingers[i].edge[E_GESTURE_EDGE_RIGHT].client ||
                           data->edge_swipe_fingers[i].edge[E_GESTURE_EDGE_BOTTOM].client ||
                           data->edge_swipe_fingers[i].edge[E_GESTURE_EDGE_LEFT].client)
                            {
                               data->grabbed_gesture |= TIZEN_GESTURE_TYPE_EDGE_SWIPE;
                               break;
                            }
                    }
               }

             if ((mode & TIZEN_GESTURE_TYPE_TAP) &&
                 (data->grabbed_gesture & TIZEN_GESTURE_TYPE_TAP))
               {
                  _e_gesture_set_client_to_list(data, NULL, TIZEN_GESTURE_TYPE_TAP, fingers, 0, repeat);
                  data->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_TAP;
                  for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                    {
                       for (j = 0; j < E_GESTURE_TAP_REPEATS_MAX + 1; j++)
                         {
                            if (data->tap_fingers[i].repeats[j].client)
                              {
                                 data->grabbed_gesture |= TIZEN_GESTURE_TYPE_TAP;
                                 break;
                              }
                         }
                    }
               }

             if ((mode & TIZEN_GESTURE_TYPE_PAN) &&
                 (data->grabbed_gesture & TIZEN_GESTURE_TYPE_PAN))
               {
                  _e_gesture_set_client_to_list(data, NULL, TIZEN_GESTURE_TYPE_PAN, fingers, 0, 0);
                  data->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_PAN;
                  for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                    {
                       if (data->pan_fingers[i].client)
                         {
                            data->grabbed_gesture |= TIZEN_GESTURE_TYPE_PAN;
                            break;
                         }
                    }
               }

             if ((mode & TIZEN_GESTURE_TYPE_PINCH) &&
                 (data->grabbed_gesture & TIZEN_GESTURE_TYPE_PINCH))
               {
                  _e_gesture_set_client_to_list(data, NULL, TIZEN_GESTURE_TYPE_PINCH, fingers, 0, 0);
                  data->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_PINCH;
                  for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                    {
                       if (data->pan_fingers[i].client)
                         {
                            data->grabbed_gesture |= TIZEN_GESTURE_TYPE_PINCH;
                            break;
                         }
                    }
               }

             if (!data->grabbed_gesture)
               {
                  wl_list_remove(&data->destroy_listener->link);
                  E_FREE(data->destroy_listener);
                  gesture->grab_client_list = eina_list_remove(gesture->grab_client_list, data);
                  E_FREE(data);
               }
          }
     }
}

static int
_e_gesture_grab_pan(struct wl_client *client, struct wl_resource *resource, uint32_t fingers)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("The client %p request to grab pan gesture, fingers: %d\n", client, fingers);

   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   if (gev->pans.fingers[fingers].client)
     {
        GTWRN("%d finger is already grabbed by %p client\n", fingers, gev->pans.fingers[fingers].client);
        ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
        goto finish;
     }

   e_gesture_add_client_destroy_listener(client, TIZEN_GESTURE_TYPE_PAN, fingers, 0, 0);

   gev->pans.fingers[fingers].client = client;
   gev->pans.fingers[fingers].res = resource;
   gev->pans.state = E_GESTURE_PANPINCH_STATE_READY;

   gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_PAN;
   gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;

finish:
   return ret;
}

static int
_e_gesture_ungrab_pan(struct wl_client *client, struct wl_resource *resource, uint32_t fingers)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("The client %p request to ungrab pan gesture, fingers: %d\n", client, fingers);

   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   if (gev->pans.fingers[fingers].client == client)
     {
        gev->pans.fingers[fingers].client = NULL;
        gev->pans.fingers[fingers].res = NULL;
     }

   _e_gesture_remove_client_destroy_listener(client, TIZEN_GESTURE_TYPE_PAN, fingers, 0, 0);
   _e_gesture_pan_current_list_check();

finish:
   return ret;
}

static int
_e_gesture_grab_pinch(struct wl_client *client, struct wl_resource *resource, uint32_t fingers)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("The client %p request to grab pinch gesture, fingers: %d\n", client, fingers);

   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   if (gev->pinchs.fingers[fingers].client)
     {
        GTWRN("%d finger is already grabbed by %p client\n", fingers, gev->pinchs.fingers[fingers].client);
        ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
        goto finish;
     }

   e_gesture_add_client_destroy_listener(client, TIZEN_GESTURE_TYPE_PINCH, fingers, 0, 0);

   gev->pinchs.fingers[fingers].client = client;
   gev->pinchs.fingers[fingers].res = resource;

   gev->pinchs.state = E_GESTURE_PANPINCH_STATE_READY;
   gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_PINCH;
   gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;

finish:
   return ret;
}

static int
_e_gesture_ungrab_pinch(struct wl_client *client, struct wl_resource *resource, uint32_t fingers)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("The client %p request to ungrab pinch gesture, fingers: %d\n", client, fingers);

   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   if (gev->pinchs.fingers[fingers].client == client)
     {
        gev->pinchs.fingers[fingers].client = NULL;
        gev->pinchs.fingers[fingers].res = NULL;
     }

   _e_gesture_remove_client_destroy_listener(client, TIZEN_GESTURE_TYPE_PINCH, fingers, 0, 0);
   _e_gesture_pinch_current_list_check();

finish:
   return ret;
}

static void
_e_gesture_cb_grab_edge_swipe(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t fingers, uint32_t edge)
{
   E_Gesture_Event *gev;
   unsigned int grabbed_edge = 0x0;

   GTINF("client %p is request grab gesture, fingers: %d, edge: 0x%x\n", client, fingers, edge);
   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        tizen_gesture_send_grab_edge_swipe_notify(resource, fingers, edge, TIZEN_GESTURE_ERROR_INVALID_DATA);
        goto out;
     }

   gev = &gesture->gesture_events;

   if (edge & TIZEN_GESTURE_EDGE_TOP)
     {
        if (gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_TOP].client)
           {
              grabbed_edge |= TIZEN_GESTURE_EDGE_TOP;
           }
        else
           {
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_TOP].client = client;
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_TOP].res = resource;
           }
     }
   if (edge & TIZEN_GESTURE_EDGE_RIGHT)
     {
        if (gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_RIGHT].client)
           {
              grabbed_edge |= TIZEN_GESTURE_EDGE_RIGHT;
           }
        else
           {
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_RIGHT].client = client;
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_RIGHT].res = resource;
           }
     }
   if (edge & TIZEN_GESTURE_EDGE_BOTTOM)
     {
        if (gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_BOTTOM].client)
           {
              grabbed_edge |= TIZEN_GESTURE_EDGE_BOTTOM;
           }
        else
           {
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_BOTTOM].client = client;
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_BOTTOM].res = resource;
           }
     }
   if (edge & TIZEN_GESTURE_EDGE_LEFT)
     {
        if (gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_LEFT].client)
           {
              grabbed_edge |= TIZEN_GESTURE_EDGE_LEFT;
           }
        else
           {
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_LEFT].client = client;
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_LEFT].res = resource;
           }
     }

   if (grabbed_edge)
     tizen_gesture_send_grab_edge_swipe_notify(resource, fingers, grabbed_edge, TIZEN_GESTURE_ERROR_GRABBED_ALREADY);

   e_gesture_add_client_destroy_listener(client, TIZEN_GESTURE_TYPE_EDGE_SWIPE, fingers, edge & ~grabbed_edge, 0);
   gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_EDGE_SWIPE;
   gev->edge_swipes.fingers[fingers].enabled = EINA_TRUE;
   if (gev->edge_swipes.event_keep) gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
   gev->edge_swipes.enabled_edge |= grabbed_edge;

   if (!grabbed_edge)
     tizen_gesture_send_grab_edge_swipe_notify(resource, fingers, edge, TIZEN_GESTURE_ERROR_NONE);

out:
   return;
}

static void
_e_gesture_cb_ungrab_edge_swipe(struct wl_client *client,
                           struct wl_resource *resouce,
                           uint32_t fingers, uint32_t edge)
{
   E_Gesture_Event *gev;
   unsigned int ungrabbed_edge = 0x0;
   int ret = TIZEN_GESTURE_ERROR_NONE;
   int i, j;

   GTINF("client %p is request ungrab edge swipe gesture, fingers: %d, edge: 0x%x, client: %p\n", client, fingers, edge, gesture->gesture_events.edge_swipes.fingers[0].edge[3].client);

   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto notify;
     }

   gev = &gesture->gesture_events;

   if (edge & TIZEN_GESTURE_EDGE_TOP)
     {
        if ((gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_TOP].client) &&
            (gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_TOP].client == client))
           {
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_TOP].client = NULL;
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_TOP].res = NULL;
           }
        else
           {
              ungrabbed_edge |= TIZEN_GESTURE_EDGE_TOP;
           }
     }
   if (edge & TIZEN_GESTURE_EDGE_RIGHT)
     {
        if ((gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_RIGHT].client) &&
            (gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_RIGHT].client == client))
           {
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_RIGHT].client = NULL;
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_RIGHT].res = NULL;
           }
        else
           {
              ungrabbed_edge |= TIZEN_GESTURE_EDGE_RIGHT;
           }
     }
   if (edge & TIZEN_GESTURE_EDGE_BOTTOM)
     {
        if ((gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_BOTTOM].client) &&
            (gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_BOTTOM].client == client))
           {
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_BOTTOM].client = NULL;
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_BOTTOM].res = NULL;
           }
        else
           {
              ungrabbed_edge |= TIZEN_GESTURE_EDGE_BOTTOM;
           }
     }
   if (edge & TIZEN_GESTURE_EDGE_LEFT)
     {
        if ((gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_LEFT].client) &&
            (gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_LEFT].client == client))
           {
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_LEFT].client = NULL;
              gev->edge_swipes.fingers[fingers].edge[E_GESTURE_EDGE_LEFT].res = NULL;
           }
        else
           {
              ungrabbed_edge |= TIZEN_GESTURE_EDGE_LEFT;
           }
     }

   if (edge & ~ungrabbed_edge)
     {
        _e_gesture_remove_client_destroy_listener(client, TIZEN_GESTURE_TYPE_EDGE_SWIPE, fingers, edge & ~ungrabbed_edge, 0);
        _e_gesture_edge_swipe_current_list_check();
     }

   gev->edge_swipes.enabled_edge &= ~edge;
   for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
     {
        for (j = 1; j < E_GESTURE_EDGE_MAX+1; j++)
          {
             if (gev->edge_swipes.fingers[i].edge[j].client)
               {
                  gev->edge_swipes.enabled_edge |= (1 <<  (j - 1));
               }
          }
        if (gev->edge_swipes.enabled_edge == E_GESTURE_EDGE_ALL) break;
     }

notify:
   tizen_gesture_send_grab_edge_swipe_notify(resouce, fingers, edge, ret);
   return;
}

static void
_e_gesture_cb_grab_tap(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t fingers, uint32_t repeats)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("client %p requested to grab tap gesture (fingers: %d, repeats: %d)\n", client, fingers, repeats);

   if (fingers > E_GESTURE_FINGER_MAX || repeats > E_GESTURE_TAP_REPEATS_MAX)
     {
        GTWRN("Not supported fingers /repeats bigger than their maximum values\n");
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   if (gev->taps.fingers[fingers].repeats[repeats].client)
     {
        GTWRN("%d finger %d repeats is already grabbed by %p client\n", fingers, repeats, gev->taps.fingers[fingers].repeats[repeats].client);
        ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
        goto finish;
     }

   gev->taps.fingers[fingers].repeats[repeats].client = client;
   gev->taps.fingers[fingers].repeats[repeats].res = resource;
   gev->taps.fingers[fingers].enabled = EINA_TRUE;

   e_gesture_add_client_destroy_listener(client, TIZEN_GESTURE_TYPE_TAP, fingers, 0, repeats);

   if (gev->taps.max_fingers < fingers)
     gev->taps.max_fingers = fingers;
   if (gev->taps.fingers[fingers].max_repeats < repeats)
     gev->taps.fingers[fingers].max_repeats = repeats;

   gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_TAP;
   gev->taps.state = E_GESTURE_TAP_STATE_READY;
   gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
   gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;

finish:
   tizen_gesture_send_tap_notify(resource, fingers, repeats, ret);
}

static void
_e_gesture_cb_ungrab_tap(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t fingers, uint32_t repeats)
{
   int i;
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("client %p requested to ungrab tap gesture (fingers: %d, repeats: %d)\n", client, fingers, fingers);

   if (fingers > E_GESTURE_FINGER_MAX || repeats > E_GESTURE_TAP_REPEATS_MAX)
     {
        GTWRN("Not supported fingers /repeats bigger than their maximum values\n");
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   if (gev->taps.fingers[fingers].repeats[repeats].client == client)
     {
        gev->taps.fingers[fingers].repeats[repeats].client = NULL;
        gev->taps.fingers[fingers].repeats[repeats].res = NULL;
     }

   gev->taps.fingers[fingers].enabled = EINA_FALSE;
   for (i = 0; i < E_GESTURE_TAP_REPEATS_MAX; i++)
     {
        if (gev->taps.fingers[fingers].repeats[i].client)
          {
             gev->taps.fingers[fingers].enabled = EINA_TRUE;
             break;
          }
     }
   gev->taps.fingers[fingers].max_repeats = e_gesture_util_tap_max_repeats_get(fingers);

   _e_gesture_remove_client_destroy_listener(client, TIZEN_GESTURE_TYPE_TAP, fingers, 0, repeats);
   _e_gesture_tap_current_list_check();

finish:
   tizen_gesture_send_tap_notify(resource, fingers, repeats, ret);
}

static void
_e_gesture_cb_grab_pan(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t fingers)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_grab_pan(client, resource, fingers);

   tizen_gesture_send_pan_notify(resource, fingers, ret);
}

static void
_e_gesture_cb_ungrab_pan(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t fingers)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_ungrab_pan(client, resource, fingers);

   tizen_gesture_send_pan_notify(resource, fingers, ret);
}

static void
_e_gesture_cb_grab_pinch(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t fingers)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_grab_pinch(client, resource, fingers);

   tizen_gesture_send_pinch_notify(resource, fingers, ret);
}

static void
_e_gesture_cb_ungrab_pinch(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t fingers)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_ungrab_pinch(client, resource, fingers);

   tizen_gesture_send_pinch_notify(resource, fingers, ret);
}

static const struct tizen_gesture_interface _e_gesture_implementation = {
   _e_gesture_cb_grab_edge_swipe,
   _e_gesture_cb_ungrab_edge_swipe,
   _e_gesture_cb_grab_tap,
   _e_gesture_cb_ungrab_tap,
   _e_gesture_cb_grab_pan,
   _e_gesture_cb_ungrab_pan,
   _e_gesture_cb_grab_pinch,
   _e_gesture_cb_ungrab_pinch
};

/* tizen_gesture global object destroy function */
static void
_e_gesture_cb_destory(struct wl_resource *resource)
{
   /* TODO : destroy resources if exist */
}

/* tizen_keyrouter global object bind function */
static void
_e_gesture_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_GesturePtr gesture_instance = data;
   struct wl_resource *resource;

   resource = wl_resource_create(client, &tizen_gesture_interface, MIN(version, 1), id);

   GTDBG("wl_resource_create(...,tizen_gesture_interface,...)\n");

   if (!resource)
     {
        GTERR("Failed to create resource ! (version :%d, id:%d)\n", version, id);
        wl_client_post_no_memory(client);
	 return;
     }

   wl_resource_set_implementation(resource, &_e_gesture_implementation, gesture_instance, _e_gesture_cb_destory);
}

static Eina_Bool
_e_gesture_event_filter(void *data, void *loop_data EINA_UNUSED, int type, void *event)
{
   (void) data;

   if (type == ECORE_EVENT_MOUSE_BUTTON_DOWN)
     {
        gesture->gesture_events.num_pressed++;
     }
   else if (type == ECORE_EVENT_MOUSE_BUTTON_UP)
     {
        gesture->gesture_events.num_pressed--;
        if (gesture->gesture_events.num_pressed < 0)
          gesture->gesture_events.num_pressed = 0;
        if (gesture->gesture_events.num_pressed == 0)
          {
             if (!gesture->enable && gesture->enabled_window)
               {
                  e_gesture_event_filter_enable(EINA_TRUE);
                  return EINA_TRUE;
               }
             else if (gesture->enable && !gesture->enabled_window)
               {
                  e_gesture_event_filter_enable(EINA_FALSE);
                  return e_gesture_process_events(event, type);
               }
          }
     }
   if (!gesture->enable) return EINA_TRUE;

   return e_gesture_process_events(event, type);
}

static void
_e_gesture_window_gesture_disabled_change(E_Client *ec)
{
   if (ec->gesture_disable && gesture->enable)
     {
        GTINF("Gesture disabled window\n");
        gesture->enabled_window = EINA_FALSE;
     }
   else if (!ec->gesture_disable && !gesture->enable)
     {
        GTINF("Gesture enabled window\n");
        gesture->enabled_window = EINA_TRUE;
     }

   if (gesture->gesture_events.num_pressed == 0)
     {
        e_gesture_event_filter_enable(gesture->enabled_window);
     }
}

static Eina_Bool
_e_gesture_cb_client_focus_in(void *data, int type, void *event)
{
   E_Client *ec;
   E_Event_Client *ev = (E_Event_Client *)event;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ev, ECORE_CALLBACK_PASS_ON);
   ec = ev->ec;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec, ECORE_CALLBACK_PASS_ON);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec->comp_data, ECORE_CALLBACK_PASS_ON);

   _e_gesture_window_gesture_disabled_change(ec);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_e_gesture_cb_aux_hint_change(void *data EINA_UNUSED, E_Client *ec)
{
   E_Client *qp_ec;
   if (e_object_is_del(E_OBJECT(ec)) || !ec->comp_data) return;
   if (!ec->comp_data->aux_hint.changed) return;

   qp_ec = e_service_quickpanel_client_get();

   /* Return if the aux hint change didn't happen to the focused ec */
   if ((ec != qp_ec) &&
       (ec != e_client_focused_get()))
     return;

   _e_gesture_window_gesture_disabled_change(ec);
}

static void
_e_gesture_init_handlers(void)
{
   gesture->ef_handler = ecore_event_filter_add(NULL, _e_gesture_event_filter, NULL, NULL);

   gesture->handlers = eina_list_append(gesture->handlers,
                                        ecore_event_handler_add(E_EVENT_CLIENT_FOCUS_IN,
                                                                _e_gesture_cb_client_focus_in, NULL));

   gesture->hooks = eina_list_append(gesture->hooks,
                                     e_client_hook_add(E_CLIENT_HOOK_AUX_HINT_CHANGE,
                                                       _e_gesture_cb_aux_hint_change, NULL));
}

static void
_e_gesture_deinit_handlers(void)
{
   Ecore_Event_Handler *event_handler;
   E_Client_Hook *hook;

   ecore_event_filter_del(gesture->ef_handler);

   EINA_LIST_FREE(gesture->handlers, event_handler)
     {
        ecore_event_handler_del(event_handler);
     }

   EINA_LIST_FREE(gesture->hooks, hook)
     {
        e_client_hook_del(hook);
     }
}

static E_Gesture_Config_Data *
_e_gesture_init(E_Module *m)
{
   E_Gesture_Config_Data *gconfig = NULL;
   gesture = E_NEW(E_Gesture, 1);

   if (!gesture)
     {
        GTERR("Failed to allocate memory for gesture !\n");
        return NULL;
     }

   if (!e_comp)
     {
        GTERR("Failed to initialize gesture module ! (e_comp == NULL)\n");
        goto err;
     }

   /* Add filtering mechanism
    * FIXME: Add handlers after first gesture is grabbed
    */
   _e_gesture_init_handlers();

   /* Init config */
   gconfig = E_NEW(E_Gesture_Config_Data, 1);
   EINA_SAFETY_ON_NULL_GOTO(gconfig, err);
   gconfig->module = m;

   e_gesture_conf_init(gconfig);
   EINA_SAFETY_ON_NULL_GOTO(gconfig->conf, err);
   gesture->config = gconfig;

   GTDBG("config value\n");
   GTDBG("keyboard: %s, time_done: %lf, time_begin: %lf\n", gconfig->conf->key_device_name, gconfig->conf->edge_swipe.time_done, gconfig->conf->edge_swipe.time_begin);
   GTDBG("area_offset: %d, min_length: %d, max_length: %d\n", gconfig->conf->edge_swipe.area_offset, gconfig->conf->edge_swipe.min_length, gconfig->conf->edge_swipe.max_length);
   GTDBG("compose key: %d, back: %d, default: %d\n", gconfig->conf->edge_swipe.compose_key, gconfig->conf->edge_swipe.back_key, gconfig->conf->edge_swipe.default_enable_back);

   gesture->global = wl_global_create(e_comp_wl->wl.disp, &tizen_gesture_interface, 1, gesture, _e_gesture_cb_bind);
   if (!gesture->global)
     {
        GTERR("Failed to create global !\n");
        goto err;
     }

   gesture->gesture_filter = E_GESTURE_TYPE_MAX;

   gesture->gesture_events.edge_swipes.event_keep = gconfig->conf->edge_swipe.event_keep;
   if (gconfig->conf->edge_swipe.default_enable_back)
     {
        gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_EDGE_SWIPE;
        gesture->gesture_events.edge_swipes.fingers[1].enabled = EINA_TRUE;
        gesture->gesture_events.edge_swipes.fingers[1].edge[E_GESTURE_EDGE_TOP].client = (void *)0x1;
        gesture->gesture_events.edge_swipes.fingers[1].edge[E_GESTURE_EDGE_TOP].res = (void *)0x1;
        gesture->gesture_events.edge_swipes.enabled_edge |= TIZEN_GESTURE_EDGE_TOP;
        if (gesture->gesture_events.edge_swipes.event_keep)
          {
             gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
          }
     }

   e_gesture_device_keydev_set(gesture->config->conf->key_device_name);
   gesture->enable = EINA_TRUE;

   return gconfig;

err:
   if (gconfig) e_gesture_conf_deinit(gconfig);
   if (gesture && gesture->ef_handler) ecore_event_filter_del(gesture->ef_handler);
   if (gesture) E_FREE(gesture);

   return NULL;
}

E_API void *
e_modapi_init(E_Module *m)
{
   return _e_gesture_init(m);
}

E_API int
e_modapi_shutdown(E_Module *m)
{
   Eina_List *l, *l_next;
   E_Gesture_Grabbed_Client *gclient;
   E_Gesture_Event_Info *event_info;
   E_Gesture_Config_Data *gconfig = m->data;

   e_gesture_conf_deinit(gconfig);
   e_gesture_device_shutdown();
   _e_gesture_deinit_handlers();

   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l, l_next, gclient)
     {
        if (gclient->destroy_listener)
          {
             wl_list_remove(&gclient->destroy_listener->link);
             E_FREE(gclient->destroy_listener);
          }
        E_FREE(gclient);
        gesture->grab_client_list = eina_list_remove_list(gesture->grab_client_list, l);
     }

   EINA_LIST_FOREACH_SAFE(gesture->event_queue, l, l_next, event_info)
     {
        E_FREE(event_info->event);
        E_FREE(event_info);
        gesture->event_queue = eina_list_remove_list(gesture->event_queue, l);
     }

   return 1;
}

E_API int
e_modapi_save(E_Module *m)
{
   /* Save something to be kept */
   E_Gesture_Config_Data *gconfig = m->data;
   e_config_domain_save("module.gesture",
                        gconfig->conf_edd,
                        gconfig->conf);
   return 1;
}

static void
_e_gesture_remove_client_edge_swipe(struct wl_client *client, E_Gesture_Grabbed_Client *gclient)
{
   int i, j;

   for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
     {
        for (j = 0; j < E_GESTURE_EDGE_MAX + 1; j++)
          {
             if (gclient->edge_swipe_fingers[i].edge[j].client)
               {
                  if (gesture->gesture_events.edge_swipes.fingers[i].edge[j].client == client)
                    {
                       gesture->gesture_events.edge_swipes.fingers[i].edge[j].client = NULL;
                       gesture->gesture_events.edge_swipes.fingers[i].edge[j].res = NULL;
                    }
               }
             gclient->edge_swipe_fingers[i].edge[j].client = NULL;
          }
     }
}

static void
_e_gesture_remove_client_tap(struct wl_client *client, E_Gesture_Grabbed_Client *gclient)
{
   int i, j;

   for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
     {
        for (j = 0; j < E_GESTURE_TAP_REPEATS_MAX + 1; j++)
          {
             if (gclient->tap_fingers[i].repeats[j].client)
               {
                  if (gesture->gesture_events.taps.fingers[i].repeats[j].client == client)
                    {
                       gesture->gesture_events.taps.fingers[i].repeats[j].client = NULL;
                       gesture->gesture_events.taps.fingers[i].repeats[j].res = NULL;
                    }
               }
             gclient->tap_fingers[i].repeats[j].client = NULL;
          }
     }
}

static void
_e_gesture_remove_client_pan(struct wl_client *client, E_Gesture_Grabbed_Client *gclient)
{
   int i;

   for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
     {
        if (gclient->pan_fingers[i].client)
          {
             gesture->gesture_events.pans.fingers[i].client = NULL;
             gesture->gesture_events.pans.fingers[i].res = NULL;
          }
        gclient->pan_fingers[i].client = NULL;
     }
}

static void
_e_gesture_remove_client_pinch(struct wl_client *client, E_Gesture_Grabbed_Client *gclient)
{
   int i;

   for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
     {
        if (gclient->pinch_fingers[i].client)
          {
             gesture->gesture_events.pinchs.fingers[i].client = NULL;
             gesture->gesture_events.pinchs.fingers[i].res = NULL;
          }
        gclient->pinch_fingers[i].client = NULL;
     }
}

static void
_e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data)
{
   struct wl_client *client = data;
   Eina_List *l_list, *l_next;
   E_Gesture_Grabbed_Client *client_data;
   unsigned int removed_gesture = 0x0;

   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l_list, l_next, client_data)
     {
        if (client_data->client == client)
          {
             if (client_data->grabbed_gesture & TIZEN_GESTURE_TYPE_EDGE_SWIPE)
               {
                  _e_gesture_remove_client_edge_swipe(client, client_data);
                  removed_gesture |= TIZEN_GESTURE_TYPE_EDGE_SWIPE;
               }
             if (client_data->grabbed_gesture & TIZEN_GESTURE_TYPE_TAP)
               {
                  _e_gesture_remove_client_tap(client, client_data);
                  removed_gesture |= TIZEN_GESTURE_TYPE_TAP;
               }
             if (client_data->grabbed_gesture & TIZEN_GESTURE_TYPE_PAN)
               {
                  _e_gesture_remove_client_pan(client, client_data);
                  removed_gesture |= TIZEN_GESTURE_TYPE_PAN;
               }
             if (client_data->grabbed_gesture & TIZEN_GESTURE_TYPE_PINCH)
               {
                  _e_gesture_remove_client_pinch(client, client_data);
                  removed_gesture |= TIZEN_GESTURE_TYPE_PINCH;
               }
          }
     }

   if (removed_gesture & TIZEN_GESTURE_TYPE_EDGE_SWIPE)
     _e_gesture_edge_swipe_current_list_check();
   if (removed_gesture & TIZEN_GESTURE_TYPE_TAP)
     _e_gesture_tap_current_list_check();
   if (removed_gesture & TIZEN_GESTURE_TYPE_PAN)
     _e_gesture_pan_current_list_check();
   if (removed_gesture & TIZEN_GESTURE_TYPE_PINCH)
     _e_gesture_pinch_current_list_check();

   wl_list_remove(&l->link);
   E_FREE(l);

   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l_list, l_next, client_data)
     {
        if (client_data->client == client)
          {
             gesture->grab_client_list = eina_list_remove(gesture->grab_client_list, client_data);
             E_FREE(client_data);
          }
     }
}

void
e_gesture_event_filter_enable(Eina_Bool enabled)
{
   if (enabled && !gesture->enable)
     {
        GTINF("Gestures will be enabled by for now.\n");
        gesture->enable = EINA_TRUE;
     }
   else if (!enabled && gesture->enable)
     {
        GTINF("Gestures will be enabled from now.\n");
        gesture->enable = EINA_FALSE;
     }
}
