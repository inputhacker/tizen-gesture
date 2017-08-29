#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>
#include "e_service_quickpanel.h"

E_GesturePtr gesture = NULL;
E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Gesture Module of Window Manager" };

static E_Gesture_Config_Data *_e_gesture_init(E_Module *m);
static void _e_gesture_init_handlers(void);
static void _e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data);
static void _e_gesture_wl_surface_cb_destroy(struct wl_listener *l, void *data);

static Eina_Bool
_e_gesture_edge_boundary_check(E_Gesture_Event_Edge_Finger *fingers, unsigned int edge, int sp, int ep)
{
   Eina_List *l;
   E_Gesture_Event_Edge_Finger_Edge *edata;

   EINA_LIST_FOREACH(fingers->edge[edge], l, edata)
     {
        if (!(sp > edata->ep || ep < edata->sp))
          return EINA_FALSE;
     }

   return EINA_TRUE;
}

static void
_e_gesture_edge_grab_add(E_Gesture_Event_Edge_Finger *fingers, struct wl_client *client, struct wl_resource *res, unsigned int edge, unsigned int sp, unsigned int ep)
{
   E_Gesture_Event_Edge_Finger_Edge *edata;

   edata = E_NEW(E_Gesture_Event_Edge_Finger_Edge, 1);
   EINA_SAFETY_ON_NULL_RETURN(edata);

   edata->client = client;
   edata->res = res;
   edata->sp = sp;
   edata->ep = ep;

   fingers->edge[edge] = eina_list_append(fingers->edge[edge], edata);
}

static void
_e_gesture_set_client_to_list(E_Gesture_Grabbed_Client *gclient, struct wl_client *client, int mode, int fingers, int edge, int repeat, int sp, int ep)
{
   switch (mode)
     {
        case E_GESTURE_TYPE_EDGE_SWIPE:
           _e_gesture_edge_grab_add(&gclient->edge_swipe_fingers[fingers], client, NULL, edge, sp, ep);
           gclient->edge_swipe_fingers[fingers].enabled = EINA_TRUE;
           break;
        case E_GESTURE_TYPE_EDGE_DRAG:
           _e_gesture_edge_grab_add(&gclient->edge_drag_fingers[fingers], client, NULL, edge, sp, ep);
           gclient->edge_drag_fingers[fingers].enabled = EINA_TRUE;
           break;
        case E_GESTURE_TYPE_TAP:
           gclient->tap_fingers[fingers].repeats[repeat].client = client;
           break;
        case E_GESTURE_TYPE_PAN:
           gclient->pan_fingers[fingers].client = client;
           break;
        case E_GESTURE_TYPE_PINCH:
           gclient->pinch_fingers[fingers].client = client;
           break;
        case E_GESTURE_TYPE_PALM_COVER:
           gclient->palm_cover.client = client;
           break;
        default:
           return;
     }
   gclient->grabbed_gesture |= mode;
}

/* Function for registering wl_client destroy listener */
int
e_gesture_add_client_destroy_listener(struct wl_client *client, int mode, int fingers, unsigned int edge, unsigned int repeat, int sp, int ep)
{
   struct wl_listener *destroy_listener = NULL;
   Eina_List *l;
   E_Gesture_Grabbed_Client *grabbed_client, *data;

   EINA_LIST_FOREACH(gesture->grab_client_list, l, data)
     {
        if (data->client == client)
          {
             _e_gesture_set_client_to_list(data, client, mode, fingers, edge, repeat, sp, ep);

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
   _e_gesture_set_client_to_list(grabbed_client, client, mode, fingers, edge, repeat, sp, ep);

   gesture->grab_client_list = eina_list_append(gesture->grab_client_list, grabbed_client);

   return TIZEN_GESTURE_ERROR_NONE;
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
             if (eina_list_count(gev->edge_swipes.base.fingers[i].edge[j]) != 0)
               {
                  return;
               }
          }
        gev->edge_swipes.base.fingers[i].enabled = EINA_FALSE;
     }
   gesture->grabbed_gesture &= ~E_GESTURE_TYPE_EDGE_SWIPE;
   if (gev->event_keep) gesture->event_state = E_GESTURE_EVENT_STATE_PROPAGATE;
}

static void
_e_gesture_edge_drag_current_list_check(void)
{
   int i, j;
   E_Gesture_Event *gev;

   gev = &gesture->gesture_events;
   for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
     {
        for (j = 0; j < E_GESTURE_EDGE_MAX+1; j++)
          {
             if (eina_list_count(gev->edge_drags.base.fingers[i].edge[j]) != 0)
               {
                  return;
               }
          }
        gev->edge_drags.base.fingers[i].enabled = EINA_FALSE;
     }
   gesture->grabbed_gesture &= ~E_GESTURE_TYPE_EDGE_SWIPE;
   if (gev->event_keep) gesture->event_state = E_GESTURE_EVENT_STATE_PROPAGATE;
}

static void
_e_gesture_tap_current_list_check(void)
{
   int i;
   E_Gesture_Event *gev;

   gev = &gesture->gesture_events;

   gesture->grabbed_gesture &= ~E_GESTURE_TYPE_TAP;
   gev->taps.state = E_GESTURE_TAP_STATE_NONE;
   gesture->event_state = E_GESTURE_EVENT_STATE_PROPAGATE;
   for (i = 0; i < E_GESTURE_FINGER_MAX; i++)
     {
        if (gev->taps.fingers[i].enabled)
          {
             gesture->grabbed_gesture |= E_GESTURE_TYPE_TAP;
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

   gesture->grabbed_gesture &= ~E_GESTURE_TYPE_TAP;
   gev->pans.state = E_GESTURE_PANPINCH_STATE_NONE;

   for (i = 0; i < E_GESTURE_FINGER_MAX; i++)
     {
        if (gev->pans.fingers[i].client)
          {
             gesture->grabbed_gesture |= E_GESTURE_TYPE_PAN;
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

   gesture->grabbed_gesture &= ~E_GESTURE_TYPE_PINCH;
   gev->pinchs.state = E_GESTURE_PANPINCH_STATE_NONE;

   for (i = 0; i < E_GESTURE_FINGER_MAX; i++)
     {
        if (gev->pinchs.fingers[i].client)
          {
             gesture->grabbed_gesture |= E_GESTURE_TYPE_PINCH;
             gev->pinchs.state = E_GESTURE_PANPINCH_STATE_READY;
             break;
          }
     }
}

static void
_e_gesture_remove_client_destroy_listener(struct wl_client *client, unsigned int mode, unsigned int fingers, unsigned int edge, unsigned int repeat, int sp, int ep)
{
   Eina_List *l, *l_next;
   E_Gesture_Grabbed_Client *data;
   int i, j;

   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l, l_next, data)
     {
        if (data->client == client)
          {
             if ((mode & E_GESTURE_TYPE_EDGE_SWIPE) &&
                 (data->grabbed_gesture & E_GESTURE_TYPE_EDGE_SWIPE))
               {
                  _e_gesture_set_client_to_list(data, NULL, E_GESTURE_TYPE_EDGE_SWIPE, fingers, edge, 0, sp, ep);
                  data->grabbed_gesture &= ~E_GESTURE_TYPE_EDGE_SWIPE;
                  for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                    {
                       if (data->edge_swipe_fingers[i].enabled)
                         {
                            data->grabbed_gesture |= E_GESTURE_TYPE_EDGE_SWIPE;
                            break;
                         }
                    }
               }

             if ((mode & E_GESTURE_TYPE_TAP) &&
                 (data->grabbed_gesture & E_GESTURE_TYPE_TAP))
               {
                  _e_gesture_set_client_to_list(data, NULL, E_GESTURE_TYPE_TAP, fingers, 0, repeat, 0, 0);
                  data->grabbed_gesture &= ~E_GESTURE_TYPE_TAP;
                  for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                    {
                       for (j = 0; j < E_GESTURE_TAP_REPEATS_MAX + 1; j++)
                         {
                            if (data->tap_fingers[i].repeats[j].client)
                              {
                                 data->grabbed_gesture |= E_GESTURE_TYPE_TAP;
                                 break;
                              }
                         }
                    }
               }

             if ((mode & E_GESTURE_TYPE_PAN) &&
                 (data->grabbed_gesture & E_GESTURE_TYPE_PAN))
               {
                  _e_gesture_set_client_to_list(data, NULL, E_GESTURE_TYPE_PAN, fingers, 0, 0, 0, 0);
                  data->grabbed_gesture &= ~E_GESTURE_TYPE_PAN;
                  for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                    {
                       if (data->pan_fingers[i].client)
                         {
                            data->grabbed_gesture |= E_GESTURE_TYPE_PAN;
                            break;
                         }
                    }
               }

             if ((mode & E_GESTURE_TYPE_PINCH) &&
                 (data->grabbed_gesture & E_GESTURE_TYPE_PINCH))
               {
                  _e_gesture_set_client_to_list(data, NULL, E_GESTURE_TYPE_PINCH, fingers, 0, 0, 0, 0);
                  data->grabbed_gesture &= ~E_GESTURE_TYPE_PINCH;
                  for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                    {
                       if (data->pan_fingers[i].client)
                         {
                            data->grabbed_gesture |= E_GESTURE_TYPE_PINCH;
                            break;
                         }
                    }
               }

             if ((mode & E_GESTURE_TYPE_PALM_COVER) &&
                 (data->grabbed_gesture & E_GESTURE_TYPE_PALM_COVER))
               {
                  _e_gesture_set_client_to_list(data, NULL, E_GESTURE_TYPE_PALM_COVER, 0, 0, 0, 0, 0);
                  data->grabbed_gesture &= ~E_GESTURE_TYPE_PALM_COVER;
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

#if 0
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

   e_gesture_add_client_destroy_listener(client, E_GESTURE_TYPE_PAN, fingers, 0, 0);

   gev->pans.fingers[fingers].client = client;
   gev->pans.fingers[fingers].res = resource;
   gev->pans.state = E_GESTURE_PANPINCH_STATE_READY;

   gesture->grabbed_gesture |= E_GESTURE_TYPE_PAN;
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

   _e_gesture_remove_client_destroy_listener(client, E_GESTURE_TYPE_PAN, fingers, 0, 0);
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

   e_gesture_add_client_destroy_listener(client, E_GESTURE_TYPE_PINCH, fingers, 0, 0);

   gev->pinchs.fingers[fingers].client = client;
   gev->pinchs.fingers[fingers].res = resource;

   gev->pinchs.state = E_GESTURE_PANPINCH_STATE_READY;
   gesture->grabbed_gesture |= E_GESTURE_TYPE_PINCH;
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

   _e_gesture_remove_client_destroy_listener(client, E_GESTURE_TYPE_PINCH, fingers, 0, 0);
   _e_gesture_pinch_current_list_check();

finish:
   return ret;
}
#endif

static int
_e_gesture_grab_palm_cover(struct wl_client *client, struct wl_resource *resource)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   gev = &gesture->gesture_events;

   GTINF("The client %p request to grab palm hover gesture\n", client);

   if (gev->palm_covers.client_info.client)
     {
        GTWRN("Palm hover is already grabbed by %p client\n", gev->palm_covers.client_info.client);
        goto finish;
     }

   e_gesture_add_client_destroy_listener(client, E_GESTURE_TYPE_PALM_COVER, 0, 0, 0, 0, 0);

   gev->palm_covers.client_info.client = client;
   gev->palm_covers.client_info.res = resource;

   gesture->grabbed_gesture |= E_GESTURE_TYPE_PALM_COVER;
   gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;

finish:
   return ret;
}

static int
_e_gesture_ungrab_palm_cover(struct wl_client *client, struct wl_resource *resource)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("The client %p request to ungrab palm hover gesture\n", client);

   gev = &gesture->gesture_events;

   if (gev->palm_covers.client_info.client == client)
     {
        gev->palm_covers.client_info.client = NULL;
        gev->palm_covers.client_info.client = NULL;
     }

   _e_gesture_remove_client_destroy_listener(client, E_GESTURE_TYPE_PALM_COVER, 0, 0, 0, 0, 0);
   gesture->grabbed_gesture &= ~E_GESTURE_TYPE_PALM_COVER;
   gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;

   return ret;
}

static int
_e_gesture_add_surface_destroy_listener(struct wl_resource *surface, int type)
{
   struct wl_listener *destroy_listener = NULL;
   Eina_List *l;
   struct wl_resource *surface_data;

   EINA_SAFETY_ON_NULL_RETURN_VAL(surface, TIZEN_GESTURE_ERROR_INVALID_DATA);
   if (type != E_GESTURE_TYPE_PALM_COVER)
     return TIZEN_GESTURE_ERROR_INVALID_DATA;

   EINA_LIST_FOREACH(gesture->select_surface_list, l, surface_data)
     {
        if (surface_data == surface)
          {
             return TIZEN_GESTURE_ERROR_NONE;
          }
     }

   destroy_listener = E_NEW(struct wl_listener, 1);
   if (!destroy_listener)
     {
        GTERR("Failed to allocate memory for wl_client destroy listener !\n");
        return TIZEN_GESTURE_ERROR_NO_SYSTEM_RESOURCES;
     }

   destroy_listener->notify = _e_gesture_wl_surface_cb_destroy;
   wl_resource_add_destroy_listener(surface, destroy_listener);

   gesture->select_surface_list = eina_list_append(gesture->select_surface_list, surface);

   return TIZEN_GESTURE_ERROR_NONE;
}


static int
_e_gesture_select_palm_cover(struct wl_client *client, struct wl_resource *resource,
                             struct wl_resource *surface)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;
   Eina_List *l;
   E_Gesture_Event_Palm_Cover *palm_covers = &gesture->gesture_events.palm_covers;
   E_Gesture_Select_Surface *sdata;

   EINA_LIST_FOREACH(palm_covers->select_surface_list, l, sdata)
     {
        if (sdata->surface == surface)
          {
             ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
             goto out;
          }
     }

   sdata = E_NEW(E_Gesture_Select_Surface, 1);
   if (!sdata)
     {
        ret = TIZEN_GESTURE_ERROR_NO_SYSTEM_RESOURCES;
        goto out;
     }

   sdata->surface = surface;
   sdata->res = resource;

   palm_covers->select_surface_list = eina_list_append(palm_covers->select_surface_list, sdata);

   _e_gesture_add_surface_destroy_listener(surface, E_GESTURE_TYPE_PALM_COVER);

out:
   return ret;
}

static void
_e_gesture_remove_surface_destroy_listener(struct wl_resource *surface, int type)
{
   Eina_List *l, *l_next;
   struct wl_resource *surface_data;
   struct wl_listener *destroy_listener;

   EINA_SAFETY_ON_NULL_RETURN(surface);
   if (type != E_GESTURE_TYPE_PALM_COVER) return;

   destroy_listener = wl_resource_get_destroy_listener(surface, _e_gesture_wl_surface_cb_destroy);
   if (!destroy_listener)
     {
        GTWRN("surface(%p) is not gesture selected surface\n", surface);
     }

   EINA_LIST_FOREACH_SAFE(gesture->select_surface_list, l, l_next, surface_data)
     {
        if (surface_data == surface)
          {
             gesture->select_surface_list = eina_list_remove_list(gesture->select_surface_list, l);
             break;
          }
     }

   wl_list_remove(&destroy_listener->link);
   E_FREE(destroy_listener);
}

static int
_e_gesture_deselect_palm_cover(struct wl_client *client, struct wl_resource *resource,
                             struct wl_resource *surface)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;
   Eina_List *l, *l_next;
   E_Gesture_Event_Palm_Cover *palm_covers = &gesture->gesture_events.palm_covers;
   E_Gesture_Select_Surface *sdata;

   EINA_LIST_FOREACH_SAFE(palm_covers->select_surface_list, l, l_next, sdata)
     {
        if (sdata->surface == surface)
          {
             palm_covers->select_surface_list = eina_list_remove_list(palm_covers->select_surface_list, l);
             E_FREE(sdata);
             break;
          }
     }

   _e_gesture_remove_surface_destroy_listener(surface, E_GESTURE_TYPE_PALM_COVER);

   return ret;
}

static Eina_Bool
_e_gesture_deactivate_find_surface(Eina_List *list, struct wl_resource *surface)
{
   Eina_List *l;
   E_Gesture_Activate_Surface_Info *idata;

   EINA_LIST_FOREACH(list, l, idata)
     {
        if (surface == idata->surface)
          {
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

static char *
_e_gesture_util_type_to_string(unsigned int type)
{
   switch (type)
     {
        case E_GESTURE_TYPE_EDGE_SWIPE:
          return "edge_swipe";
        case E_GESTURE_TYPE_TAP:
          return "tap";
        case E_GESTURE_TYPE_PALM_COVER:
          return "palm_cover";
        case E_GESTURE_TYPE_PAN:
          return "pan";
        case E_GESTURE_TYPE_PINCH:
          return "pinch";
        default:
          return "unknown";
     }
}

static void
_e_gesture_deactivate_list_unset(struct wl_client *client, struct wl_resource *surface, E_Gesture_Activate_Info *info, unsigned int type)
{
   Eina_List *l, *l_next;
   struct wl_resource *surface_data;

   if (surface)
     {
        EINA_LIST_FOREACH_SAFE(info->surfaces, l, l_next, surface_data)
          {
             if (surface_data == surface)
               {
                  info->surfaces = eina_list_remove_list(info->surfaces, l);
                  break;
               }
          }
     }
   else if (info->client && (info->client == client))
     {
        info->client = NULL;
        info->active = EINA_TRUE;
     }
   else
     {
        GTWRN("Failed to unset %s deactivate. surface: %p, client: %p (already deactivated client: %p)\n", _e_gesture_util_type_to_string(type), surface, client, info->client);
     }
}

static void
_e_gesture_deactivate_cb_client_listener(struct wl_listener *l, void *data)
{
   struct wl_client *client = data;

   _e_gesture_deactivate_list_unset(client, NULL, &gesture->gesture_events.edge_swipes.base.activation, E_GESTURE_TYPE_EDGE_SWIPE);
   _e_gesture_deactivate_list_unset(client, NULL, &gesture->gesture_events.taps.activation, E_GESTURE_TYPE_TAP);
   _e_gesture_deactivate_list_unset(client, NULL, &gesture->gesture_events.palm_covers.activation, E_GESTURE_TYPE_PALM_COVER);
   _e_gesture_deactivate_list_unset(client, NULL, &gesture->gesture_events.pans.activation, E_GESTURE_TYPE_PAN);
   _e_gesture_deactivate_list_unset(client, NULL, &gesture->gesture_events.pinchs.activation, E_GESTURE_TYPE_PINCH);

   wl_list_remove(&l->link);
   E_FREE(l);
}

static void
_e_gesture_deactivate_cb_surface_listener(struct wl_listener *l, void *data)
{
   struct wl_resource *surface = data;

   _e_gesture_deactivate_list_unset(NULL, surface, &gesture->gesture_events.edge_swipes.base.activation, E_GESTURE_TYPE_EDGE_SWIPE);
   _e_gesture_deactivate_list_unset(NULL, surface, &gesture->gesture_events.taps.activation, E_GESTURE_TYPE_TAP);
   _e_gesture_deactivate_list_unset(NULL, surface, &gesture->gesture_events.palm_covers.activation, E_GESTURE_TYPE_PALM_COVER);
   _e_gesture_deactivate_list_unset(NULL, surface, &gesture->gesture_events.pans.activation, E_GESTURE_TYPE_PAN);
   _e_gesture_deactivate_list_unset(NULL, surface, &gesture->gesture_events.pinchs.activation, E_GESTURE_TYPE_PINCH);

   wl_list_remove(&l->link);
   E_FREE(l);
}

static int
_e_gesture_deactivate_listener_add(struct wl_client *client, struct wl_resource * surface)
{
   struct wl_listener *destroy_listener = NULL;

   EINA_SAFETY_ON_FALSE_RETURN_VAL(client || surface, TIZEN_GESTURE_ERROR_INVALID_DATA);

   destroy_listener = E_NEW(struct wl_listener, 1);
   if (!destroy_listener)
     {
        GTERR("Failed to allocate memory for deactivate destroy listener !\n");
        return TIZEN_GESTURE_ERROR_NO_SYSTEM_RESOURCES;
     }

   if (surface)
     {
        destroy_listener->notify = _e_gesture_deactivate_cb_surface_listener;
        wl_resource_add_destroy_listener(surface, destroy_listener);
     }
   else
     {
        destroy_listener->notify = _e_gesture_deactivate_cb_client_listener;
        wl_client_add_destroy_listener(client, destroy_listener);
     }

   return TIZEN_GESTURE_ERROR_NONE;
}

static int
_e_gesture_deactivate_list_set(struct wl_client *client, struct wl_resource *surface, E_Gesture_Activate_Info *info, unsigned int type)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   if (surface)
     {
        if (!_e_gesture_deactivate_find_surface(info->surfaces, surface))
          {
             info->surfaces = eina_list_append(info->surfaces, surface);
             _e_gesture_deactivate_listener_add(client, surface);
          }
     }
   else if (!info->client)
     {
        info->client = client;
        info->active = EINA_FALSE;
        _e_gesture_deactivate_listener_add(client, surface);
     }
   else
     {
        GTWRN("Failed to deactivate %s !(request surface: %p, client: %p), already deactivated client: %p\n",
              _e_gesture_util_type_to_string(type), surface, client, info->client);
        ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
     }

   return ret;
}

static int
_e_gesture_deactivate_set(struct wl_client *client,
                        struct wl_resource *resource,
                        struct wl_resource *surface,
                        uint32_t type)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   if (type & E_GESTURE_TYPE_EDGE_SWIPE)
     {
        ret = _e_gesture_deactivate_list_set(client, surface, &gesture->gesture_events.edge_swipes.base.activation, E_GESTURE_TYPE_EDGE_SWIPE);
     }
   if (type & E_GESTURE_TYPE_TAP)
     {
        ret = _e_gesture_deactivate_list_set(client, surface, &gesture->gesture_events.taps.activation, E_GESTURE_TYPE_TAP);
     }
   if (type & E_GESTURE_TYPE_PALM_COVER)
     {
        ret = _e_gesture_deactivate_list_set(client, surface, &gesture->gesture_events.palm_covers.activation, E_GESTURE_TYPE_PALM_COVER);
     }
   if (type & E_GESTURE_TYPE_PAN)
     {
        ret = _e_gesture_deactivate_list_set(client, surface, &gesture->gesture_events.pans.activation, E_GESTURE_TYPE_PAN);
     }
   if (type & E_GESTURE_TYPE_PINCH)
     {
        ret = _e_gesture_deactivate_list_set(client, surface, &gesture->gesture_events.pinchs.activation, E_GESTURE_TYPE_PINCH);
     }

   return ret;
}

static int
_e_gesture_deactivate_unset(struct wl_client *client,
                          struct wl_resource *resource,
                          struct wl_resource *surface,
                          uint32_t type)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   if (type & E_GESTURE_TYPE_EDGE_SWIPE)
     {
        _e_gesture_deactivate_list_unset(client, surface, &gesture->gesture_events.edge_swipes.base.activation, E_GESTURE_TYPE_EDGE_SWIPE);
     }
   if (type & E_GESTURE_TYPE_TAP)
     {
        _e_gesture_deactivate_list_unset(client, surface, &gesture->gesture_events.taps.activation, E_GESTURE_TYPE_TAP);
     }
   if (type & E_GESTURE_TYPE_PALM_COVER)
     {
        _e_gesture_deactivate_list_unset(client, surface, &gesture->gesture_events.palm_covers.activation, E_GESTURE_TYPE_PALM_COVER);
     }
   if (type & E_GESTURE_TYPE_PAN)
     {
        _e_gesture_deactivate_list_unset(client, surface, &gesture->gesture_events.pans.activation, E_GESTURE_TYPE_PAN);
     }
   if (type & E_GESTURE_TYPE_PINCH)
     {
        _e_gesture_deactivate_list_unset(client, surface, &gesture->gesture_events.pinchs.activation, E_GESTURE_TYPE_PINCH);
     }

   return ret;
}

static void
_e_gesture_cb_grab_edge_swipe(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t fingers, uint32_t edge, uint32_t edge_size,
                   uint32_t start_point, uint32_t end_point)
{
   E_Gesture_Event *gev;
   int sp = 0, ep = 0;
   unsigned int ret = TIZEN_GESTURE_ERROR_NONE;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   GTINF("client %p is request grab gesture, fingers: %d, edge: 0x%x, edge_size: %d, point( %d - %d)\n", client, fingers, edge, edge_size, start_point, end_point);
   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto out;
     }

   gev = &gesture->gesture_events;

   if (edge < TIZEN_GESTURE_EDGE_TOP || edge > TIZEN_GESTURE_EDGE_LEFT)
     {
        GTWRN("Invalid edge(%d)\n", edge);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto out;
     }

   if (edge_size == TIZEN_GESTURE_EDGE_SIZE_PARTIAL)
     {
        sp = start_point;
        ep = end_point;
        if (((edge == TIZEN_GESTURE_EDGE_TOP) || (edge == TIZEN_GESTURE_EDGE_BOTTOM)) &&
            (ep > e_comp->w))
          ep = e_comp->w;
        else if (((edge == TIZEN_GESTURE_EDGE_RIGHT) || (edge == TIZEN_GESTURE_EDGE_LEFT)) &&
                 (ep > e_comp->h))
          ep = e_comp->h;
     }
   else if (edge_size == TIZEN_GESTURE_EDGE_SIZE_FULL)
     {
        sp = 0;
        if ((edge == TIZEN_GESTURE_EDGE_TOP) || (edge == TIZEN_GESTURE_EDGE_BOTTOM))
          ep = e_comp->w;
        else if ((edge == TIZEN_GESTURE_EDGE_RIGHT) || (edge == TIZEN_GESTURE_EDGE_LEFT))
          ep = e_comp->h;
     }
   else
     {
        GTWRN("Invalid edge_size(%d)\n", edge_size);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto out;
     }

   if ((conf->edge_swipe.default_enable_back) &&
       (edge == E_GESTURE_EDGE_TOP))
     {
        ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
     }
   else if (_e_gesture_edge_boundary_check(&gev->edge_swipes.base.fingers[fingers], edge, sp, ep))
     {
        _e_gesture_edge_grab_add(&gev->edge_swipes.base.fingers[fingers], client, resource, edge, sp, ep);

        e_gesture_add_client_destroy_listener(client, E_GESTURE_TYPE_EDGE_SWIPE, fingers, edge, 0, sp, ep);

        gesture->grabbed_gesture |= E_GESTURE_TYPE_EDGE_SWIPE;
        gev->edge_swipes.base.fingers[fingers].enabled = EINA_TRUE;
        if (gev->event_keep) gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
        gev->edge_swipes.base.enabled_edge |= (1 << edge);

        ret = TIZEN_GESTURE_ERROR_NONE;
     }
   else
     {
        ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
     }

out:
   tizen_gesture_send_grab_edge_swipe_notify(resource, fingers, edge, edge_size, start_point, end_point, ret);
   return;
}

static Eina_Bool
_e_gesture_edge_grabbed_client_check(struct wl_client *client)
{
   Eina_List *l;
   E_Gesture_Event *gev;
   int i, j;
   E_Gesture_Event_Edge_Finger_Edge *edata;

   gev = &gesture->gesture_events;

   for (i = 1; i < E_GESTURE_FINGER_MAX + 1; i++)
     {
        for (j = 1; j < E_GESTURE_EDGE_MAX + 1; j++)
          {
             EINA_LIST_FOREACH(gev->edge_swipes.base.fingers[i].edge[j], l, edata)
               {
                  if (edata->client == client) return EINA_TRUE;
               }
          }
     }
   return EINA_FALSE;
}

static void
_e_gesture_cb_ungrab_edge_swipe(struct wl_client *client,
                           struct wl_resource *resouce,
                           uint32_t fingers, uint32_t edge, uint32_t edge_size,
                           uint32_t start_point, uint32_t end_point)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;
   int i;
   Eina_List *l, *l_next;
   E_Gesture_Event_Edge_Finger_Edge *edata;
   Eina_Bool flag_removed = EINA_FALSE;
   int sp = 0, ep = 0;

   GTINF("client %p is request ungrab edge swipe gesture, fingers: %d, edge: 0x%x, edge_size: %d, (%d ~ %d)\n", client, fingers, edge, edge_size, start_point, end_point);

   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto notify;
     }

   gev = &gesture->gesture_events;

   if (edge_size == TIZEN_GESTURE_EDGE_SIZE_PARTIAL)
     {
        sp = start_point;
        ep = end_point;
        if (((edge == TIZEN_GESTURE_EDGE_TOP) || (edge == TIZEN_GESTURE_EDGE_BOTTOM)) &&
            (ep > e_comp->w))
          ep = e_comp->w;
        else if (((edge == TIZEN_GESTURE_EDGE_RIGHT) || (edge == TIZEN_GESTURE_EDGE_LEFT)) &&
                 (ep > e_comp->h))
          ep = e_comp->h;
     }
   else if (edge_size == TIZEN_GESTURE_EDGE_SIZE_FULL)
     {
        sp = 0;
        if ((edge == TIZEN_GESTURE_EDGE_TOP) || (edge == TIZEN_GESTURE_EDGE_BOTTOM))
          ep = e_comp->w;
        else if ((edge == TIZEN_GESTURE_EDGE_RIGHT) || (edge == TIZEN_GESTURE_EDGE_LEFT))
          ep = e_comp->h;
     }
   else
     {
        GTWRN("Invalid edge_size(%d)\n", edge_size);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto notify;
     }

   EINA_LIST_FOREACH_SAFE(gev->edge_swipes.base.fingers[fingers].edge[edge], l, l_next, edata)
     {
        if ((edata->client == client) && (edata->sp == sp) && (edata->ep == ep))
          {
             gev->edge_swipes.base.fingers[fingers].edge[edge] = eina_list_remove_list(
                gev->edge_swipes.base.fingers[fingers].edge[edge], l);
             E_FREE(edata);
             flag_removed = EINA_TRUE;
          }
     }

   if (flag_removed)
     {
        if (!_e_gesture_edge_grabbed_client_check(client))
          {
             _e_gesture_remove_client_destroy_listener(client, E_GESTURE_TYPE_EDGE_SWIPE, fingers, edge, 0, sp, ep);
             _e_gesture_edge_swipe_current_list_check();
          }
     }

   gev->edge_swipes.base.enabled_edge &= ~( 1 << edge);
   for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
     {
        if (eina_list_count(gev->edge_swipes.base.fingers[i].edge[edge]) > 0)
          {
             gev->edge_swipes.base.enabled_edge |= (1 << edge);
             break;
          }
     }

notify:
   tizen_gesture_send_grab_edge_swipe_notify(resouce, fingers, edge, edge_size, start_point, end_point, ret);
   return;
}

static int
_e_gesture_grab_edge_drag(struct wl_client *client,
                          struct wl_resource *resource,
                          uint32_t fingers, uint32_t edge, uint32_t edge_size,
                          uint32_t start_point, uint32_t end_point)
{
   E_Gesture_Event *gev;
   int sp = 0, ep = 0;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("client %p is request edge_drag grab, fingers: %d, edge: 0x%x, edge_size: %d, point( %d - %d)\n", client, fingers, edge, edge_size, start_point, end_point);
   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto out;
     }

   if (edge < TIZEN_GESTURE_EDGE_TOP || edge > TIZEN_GESTURE_EDGE_LEFT)
     {
        GTWRN("Invalid edge(%d)\n", edge);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto out;
     }

   if (edge_size == TIZEN_GESTURE_EDGE_SIZE_PARTIAL)
     {
        sp = start_point;
        ep = end_point;
        if (((edge == TIZEN_GESTURE_EDGE_TOP) || (edge == TIZEN_GESTURE_EDGE_BOTTOM)) &&
            (ep > e_comp->w))
          ep = e_comp->w;
        else if (((edge == TIZEN_GESTURE_EDGE_RIGHT) || (edge == TIZEN_GESTURE_EDGE_LEFT)) &&
                 (ep > e_comp->h))
          ep = e_comp->h;
     }
   else if (edge_size == TIZEN_GESTURE_EDGE_SIZE_FULL)
     {
        sp = 0;
        if ((edge == TIZEN_GESTURE_EDGE_TOP) || (edge == TIZEN_GESTURE_EDGE_BOTTOM))
          ep = e_comp->w;
        else if ((edge == TIZEN_GESTURE_EDGE_RIGHT) || (edge == TIZEN_GESTURE_EDGE_LEFT))
          ep = e_comp->h;
     }
   else
     {
        GTWRN("Invalid edge_size(%d)\n", edge_size);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto out;
     }

   gev = &gesture->gesture_events;

   if (_e_gesture_edge_boundary_check(&gev->edge_drags.base.fingers[fingers], edge, sp, ep))
     {
        _e_gesture_edge_grab_add(&gev->edge_drags.base.fingers[fingers], client, resource, edge, sp, ep);

        e_gesture_add_client_destroy_listener(client, E_GESTURE_TYPE_EDGE_DRAG, fingers, edge, 0, sp, ep);

        gesture->grabbed_gesture |= E_GESTURE_TYPE_EDGE_DRAG;
        gev->edge_drags.base.fingers[fingers].enabled = EINA_TRUE;
        if (gev->event_keep) gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;
        gev->edge_drags.base.enabled_edge |= (1 << edge);

        ret = TIZEN_GESTURE_ERROR_NONE;
     }
   else
     {
        ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
     }

out:
   return ret;
}

static int
_e_gesture_ungrab_edge_drag(struct wl_client *client,
                       struct wl_resource *resouce,
                       uint32_t fingers, uint32_t edge, uint32_t edge_size,
                       uint32_t start_point, uint32_t end_point)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;
   int i;
   Eina_List *l, *l_next;
   E_Gesture_Event_Edge_Finger_Edge *edata;
   Eina_Bool flag_removed = EINA_FALSE;
   int sp = 0, ep = 0;

   GTINF("client %p is request ungrab edge drag gesture, fingers: %d, edge: 0x%x, edge_size: %d, (%d ~ %d)\n", client, fingers, edge, edge_size, start_point, end_point);

   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto out;
     }

   if (edge_size == TIZEN_GESTURE_EDGE_SIZE_PARTIAL)
     {
        sp = start_point;
        ep = end_point;
        if (((edge == TIZEN_GESTURE_EDGE_TOP) || (edge == TIZEN_GESTURE_EDGE_BOTTOM)) &&
            (ep > e_comp->w))
          ep = e_comp->w;
        else if (((edge == TIZEN_GESTURE_EDGE_RIGHT) || (edge == TIZEN_GESTURE_EDGE_LEFT)) &&
                 (ep > e_comp->h))
          ep = e_comp->h;
     }
   else if (edge_size == TIZEN_GESTURE_EDGE_SIZE_FULL)
     {
        sp = 0;
        if ((edge == TIZEN_GESTURE_EDGE_TOP) || (edge == TIZEN_GESTURE_EDGE_BOTTOM))
          ep = e_comp->w;
        else if ((edge == TIZEN_GESTURE_EDGE_RIGHT) || (edge == TIZEN_GESTURE_EDGE_LEFT))
          ep = e_comp->h;
     }
   else
     {
        GTWRN("Invalid edge_size(%d)\n", edge_size);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto out;
     }

   gev = &gesture->gesture_events;

   EINA_LIST_FOREACH_SAFE(gev->edge_drags.base.fingers[fingers].edge[edge], l, l_next, edata)
     {
        if ((edata->client == client) && (edata->sp == sp) && (edata->ep == ep))
          {
             gev->edge_drags.base.fingers[fingers].edge[edge] = eina_list_remove_list(
                gev->edge_drags.base.fingers[fingers].edge[edge], l);
             E_FREE(edata);
             flag_removed = EINA_TRUE;
          }
     }

   if (flag_removed)
     {
        if (!_e_gesture_edge_grabbed_client_check(client))
          {
             _e_gesture_remove_client_destroy_listener(client, E_GESTURE_TYPE_EDGE_DRAG, fingers, edge, 0, sp, ep);
             _e_gesture_edge_drag_current_list_check();
          }
     }

   gev->edge_drags.base.enabled_edge &= ~( 1 << edge);
   for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
     {
        if (eina_list_count(gev->edge_drags.base.fingers[i].edge[edge]) > 0)
          {
             gev->edge_drags.base.enabled_edge |= (1 << edge);
             break;
          }
     }

out:
   return ret;
}


static void
_e_gesture_cb_grab_edge_drag(struct wl_client *client,
                        struct wl_resource *resource,
                        uint32_t fingers, uint32_t edge, uint32_t edge_size,
                        uint32_t start_point, uint32_t end_point)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_grab_edge_drag(client, resource, fingers, edge, edge_size, start_point, end_point);

   tizen_gesture_send_grab_edge_swipe_notify(resource, fingers, edge, edge_size, start_point, end_point, ret);
}

static void
_e_gesture_cb_ungrab_edge_drag(struct wl_client *client,
                          struct wl_resource *resource,
                          uint32_t fingers, uint32_t edge, uint32_t edge_size,
                          uint32_t start_point, uint32_t end_point)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_ungrab_edge_drag(client, resource, fingers, edge, edge_size, start_point, end_point);

   tizen_gesture_send_grab_edge_swipe_notify(resource, fingers, edge, edge_size, start_point, end_point, ret);
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

   e_gesture_add_client_destroy_listener(client, E_GESTURE_TYPE_TAP, fingers, 0, repeats, 0, 0);

   if (gev->taps.max_fingers < fingers)
     gev->taps.max_fingers = fingers;
   if (gev->taps.fingers[fingers].max_repeats < repeats)
     gev->taps.fingers[fingers].max_repeats = repeats;

   gesture->grabbed_gesture |= E_GESTURE_TYPE_TAP;
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

   _e_gesture_remove_client_destroy_listener(client, E_GESTURE_TYPE_TAP, fingers, 0, repeats, 0, 0);
   _e_gesture_tap_current_list_check();

finish:
   tizen_gesture_send_tap_notify(resource, fingers, repeats, ret);
}

static void
_e_gesture_cb_grab_palm_cover(struct wl_client *client,
                        struct wl_resource *resource)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_grab_palm_cover(client, resource);

   tizen_gesture_send_palm_cover_notify(resource, NULL, ret);
}

static void
_e_gesture_cb_ungrab_palm_cover(struct wl_client *client,
                        struct wl_resource *resource)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_ungrab_palm_cover(client, resource);

   tizen_gesture_send_palm_cover_notify(resource, NULL, ret);
}

static void
_e_gesture_cb_select_palm_cover(struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *surface)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_select_palm_cover(client, resource, surface);

   tizen_gesture_send_palm_cover_notify(resource, surface, ret);
}

static void
_e_gesture_cb_deselect_palm_cover(struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *surface)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   ret = _e_gesture_deselect_palm_cover(client, resource, surface);

   tizen_gesture_send_palm_cover_notify(resource, surface, ret);
}

static void
_e_gesture_cb_activate_set(struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *surface,
                           uint32_t type, uint32_t active)
{
   int ret = TIZEN_GESTURE_ERROR_NONE;

   if (!active)
     ret = _e_gesture_deactivate_set(client, resource, surface, type);
   else
     ret = _e_gesture_deactivate_unset(client, resource, surface, type);

   tizen_gesture_send_activate_notify(resource, surface, type, active, ret);
}

static void
_e_gesture_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct tizen_gesture_interface _e_gesture_implementation = {
   _e_gesture_cb_grab_edge_swipe,
   _e_gesture_cb_ungrab_edge_swipe,
   _e_gesture_cb_grab_edge_drag,
   _e_gesture_cb_ungrab_edge_drag,
   _e_gesture_cb_grab_tap,
   _e_gesture_cb_ungrab_tap,
   _e_gesture_cb_grab_palm_cover,
   _e_gesture_cb_ungrab_palm_cover,
   _e_gesture_cb_select_palm_cover,
   _e_gesture_cb_deselect_palm_cover,
   _e_gesture_cb_activate_set,
   _e_gesture_cb_destroy,
};

/* tizen_gesture global object bind function */
static void
_e_gesture_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_GesturePtr gesture_instance = data;
   struct wl_resource *resource;

   resource = wl_resource_create(client, &tizen_gesture_interface, MIN(version, 2), id);

   GTDBG("wl_resource_create(...,tizen_gesture_interface,...)\n");

   if (!resource)
     {
        GTERR("Failed to create resource ! (version :%d, id:%d)\n", version, id);
        wl_client_post_no_memory(client);
	 return;
     }

   wl_resource_set_implementation(resource, &_e_gesture_implementation, NULL, NULL);
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
_e_gesture_deactivate_surface_list_check(struct wl_resource *surface, E_Gesture_Activate_Info *info)
{
   Eina_Bool res;

   res = _e_gesture_deactivate_find_surface(info->surfaces, surface);

   if (res) info->active = EINA_FALSE;
   else info->active = EINA_TRUE;
}

static void
_e_gesture_deactivate_surface_check(E_Client *ec)
{
   struct wl_resource *surface;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->comp_data);

   surface = ec->comp_data->wl_surface;
   if (!surface) return;

   if (!gesture->gesture_events.edge_swipes.base.activation.client)
     {
        _e_gesture_deactivate_surface_list_check(surface, &gesture->gesture_events.edge_swipes.base.activation);
     }
   if (!gesture->gesture_events.taps.activation.client)
     {
        _e_gesture_deactivate_surface_list_check(surface, &gesture->gesture_events.taps.activation);
     }
   if (!gesture->gesture_events.palm_covers.activation.client)
     {
        _e_gesture_deactivate_surface_list_check(surface, &gesture->gesture_events.palm_covers.activation);
     }
   if (!gesture->gesture_events.pans.activation.client)
     {
        _e_gesture_deactivate_surface_list_check(surface, &gesture->gesture_events.pans.activation);
     }
   if (!gesture->gesture_events.pinchs.activation.client)
     {
        _e_gesture_deactivate_surface_list_check(surface, &gesture->gesture_events.pinchs.activation);
     }
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
   _e_gesture_deactivate_surface_check(ec);
   e_gesture_event_deactivate_check();

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

   GTDBG("gesture config value\n");
   GTDBG("key_device_name: %s, event_keep: %d\n", gconfig->conf->key_device_name, gconfig->conf->event_keep);
   GTDBG("edge_swipe\n");
   GTDBG("\ttime_done: %lf, time_begin: %lf, area_offset: %d\n", gconfig->conf->edge_swipe.time_done,
                                                                 gconfig->conf->edge_swipe.time_begin,
                                                                 gconfig->conf->edge_swipe.area_offset);
   GTDBG("\tmin_length: %d, max_length: %d, compose_key: %d, back_key: %d\n", gconfig->conf->edge_swipe.min_length,
                                                                              gconfig->conf->edge_swipe.max_length,
                                                                              gconfig->conf->edge_swipe.compose_key,
                                                                              gconfig->conf->edge_swipe.back_key);
   GTDBG("tap\n");
   GTDBG("\trepeats_max: %d, moving_range: %d\n", gconfig->conf->tap.repeats_max,
                                                  gconfig->conf->tap.moving_range);
   GTDBG("\ttime_start: %lf, time_done: %lf, time_interval: %lf\n", gconfig->conf->tap.time_start,
                                                                    gconfig->conf->tap.time_done,
                                                                    gconfig->conf->tap.time_interval);
   GTDBG("pan time_start: %lf, moving_range: %d\n", gconfig->conf->pan.time_start, gconfig->conf->pan.moving_range);
   GTDBG("pinch moving_distance_range: %lf\n", gconfig->conf->pinch.moving_distance_range);

   gesture->global = wl_global_create(e_comp_wl->wl.disp, &tizen_gesture_interface, 2, gesture, _e_gesture_cb_bind);
   if (!gesture->global)
     {
        GTERR("Failed to create global !\n");
        goto err;
     }

   gesture->gesture_filter = E_GESTURE_TYPE_MAX;

   gesture->gesture_events.edge_swipes.base.activation.active = EINA_TRUE;
   gesture->gesture_events.edge_drags.base.activation.active = EINA_TRUE;
   gesture->gesture_events.taps.activation.active = EINA_TRUE;
   gesture->gesture_events.palm_covers.activation.active = EINA_TRUE;
   gesture->gesture_events.pans.activation.active = EINA_TRUE;
   gesture->gesture_events.pinchs.activation.active = EINA_TRUE;

   gesture->gesture_events.event_keep = gconfig->conf->event_keep;
   if (gconfig->conf->edge_swipe.default_enable_back)
     {
        gesture->grabbed_gesture |= E_GESTURE_TYPE_EDGE_SWIPE;
        gesture->gesture_events.edge_swipes.base.fingers[1].enabled = EINA_TRUE;
        _e_gesture_edge_grab_add(&gesture->gesture_events.edge_swipes.base.fingers[1], (void *)0x1, (void *)0x1, E_GESTURE_EDGE_TOP, 0, 0);
        gesture->gesture_events.edge_swipes.base.enabled_edge |= (1 << TIZEN_GESTURE_EDGE_TOP);
        if (gesture->gesture_events.event_keep)
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
   Eina_List *l, *ll;
   E_Gesture_Event_Edge_Swipe *edge_swipes = &gesture->gesture_events.edge_swipes;
   E_Gesture_Event_Edge_Finger_Edge *edata;

   for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
     {
        if (gclient->edge_swipe_fingers[i].enabled)
          {
             for (j = 0; j < E_GESTURE_EDGE_MAX + 1; j++)
               {
                  EINA_LIST_FOREACH_SAFE(edge_swipes->base.fingers[i].edge[j], l, ll, edata)
                    {
                       if (edata->client == client)
                         {
                            edge_swipes->base.fingers[i].edge[j] = eina_list_remove_list(edge_swipes->base.fingers[i].edge[j], l);
                            E_FREE(edata);
                         }
                    }
                  EINA_LIST_FOREACH_SAFE(gclient->edge_swipe_fingers[i].edge[j], l, ll, edata)
                    {
                       if (edata->client == client)
                         {
                            gclient->edge_swipe_fingers[i].edge[j] = eina_list_remove_list(gclient->edge_swipe_fingers[i].edge[j], l);
                            E_FREE(edata);
                         }
                    }
               }
          }
     }
}

static void
_e_gesture_remove_client_edge_drag(struct wl_client *client, E_Gesture_Grabbed_Client *gclient)
{
   int i, j;
   Eina_List *l, *ll;
   E_Gesture_Event_Edge_Drag *edge_drags = &gesture->gesture_events.edge_drags;
   E_Gesture_Event_Edge_Finger_Edge *edata;

   for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
     {
        if (gclient->edge_drag_fingers[i].enabled)
          {
             for (j = 0; j < E_GESTURE_EDGE_MAX + 1; j++)
               {
                  EINA_LIST_FOREACH_SAFE(edge_drags->base.fingers[i].edge[j], l, ll, edata)
                    {
                       if (edata->client == client)
                         {
                            edge_drags->base.fingers[i].edge[j] = eina_list_remove_list(edge_drags->base.fingers[i].edge[j], l);
                            E_FREE(edata);
                         }
                    }
                  EINA_LIST_FOREACH_SAFE(gclient->edge_drag_fingers[i].edge[j], l, ll, edata)
                    {
                       if (edata->client == client)
                         {
                            gclient->edge_drag_fingers[i].edge[j] = eina_list_remove_list(gclient->edge_drag_fingers[i].edge[j], l);
                            E_FREE(edata);
                         }
                    }
               }
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
_e_gesture_remove_client_palm_cover(struct wl_client *client, E_Gesture_Grabbed_Client *gclient)
{
   if (gclient->palm_cover.client)
     {
        gesture->gesture_events.palm_covers.client_info.client = NULL;
        gesture->gesture_events.palm_covers.client_info.res = NULL;
     }
   gclient->palm_cover.client = NULL;
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
             if (client_data->grabbed_gesture & E_GESTURE_TYPE_EDGE_SWIPE)
               {
                  _e_gesture_remove_client_edge_swipe(client, client_data);
                  removed_gesture |= E_GESTURE_TYPE_EDGE_SWIPE;
               }
             if (client_data->grabbed_gesture & E_GESTURE_TYPE_EDGE_DRAG)
               {
                  _e_gesture_remove_client_edge_drag(client, client_data);
                  removed_gesture |= E_GESTURE_TYPE_EDGE_SWIPE;
               }
             if (client_data->grabbed_gesture & E_GESTURE_TYPE_TAP)
               {
                  _e_gesture_remove_client_tap(client, client_data);
                  removed_gesture |= E_GESTURE_TYPE_TAP;
               }
             if (client_data->grabbed_gesture & E_GESTURE_TYPE_PAN)
               {
                  _e_gesture_remove_client_pan(client, client_data);
                  removed_gesture |= E_GESTURE_TYPE_PAN;
               }
             if (client_data->grabbed_gesture & E_GESTURE_TYPE_PINCH)
               {
                  _e_gesture_remove_client_pinch(client, client_data);
                  removed_gesture |= E_GESTURE_TYPE_PINCH;
               }
             if (client_data->grabbed_gesture & E_GESTURE_TYPE_PALM_COVER)
               {
                  _e_gesture_remove_client_palm_cover(client, client_data);
                  removed_gesture |= E_GESTURE_TYPE_PALM_COVER;
               }
          }
     }

   if (removed_gesture & E_GESTURE_TYPE_EDGE_SWIPE)
     _e_gesture_edge_swipe_current_list_check();
   if (removed_gesture & E_GESTURE_TYPE_EDGE_DRAG)
     _e_gesture_edge_drag_current_list_check();
   if (removed_gesture & E_GESTURE_TYPE_TAP)
     _e_gesture_tap_current_list_check();
   if (removed_gesture & E_GESTURE_TYPE_PAN)
     _e_gesture_pan_current_list_check();
   if (removed_gesture & E_GESTURE_TYPE_PINCH)
     _e_gesture_pinch_current_list_check();
   if (removed_gesture & E_GESTURE_TYPE_PALM_COVER)
     gesture->grabbed_gesture &= ~E_GESTURE_TYPE_PALM_COVER;

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

   gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;
}

static void
_e_gesture_wl_surface_cb_destroy(struct wl_listener *l, void *data)
{
   struct wl_resource *surface = (struct wl_resource *)data;
   E_Gesture_Event_Palm_Cover *palm_covers = &gesture->gesture_events.palm_covers;
   Eina_List *list, *l_next;
   struct wl_resource *surface_data;
   E_Gesture_Select_Surface *sdata;

   EINA_LIST_FOREACH_SAFE(gesture->select_surface_list, list, l_next, surface_data)
     {
        if (surface_data == surface)
          {
             gesture->select_surface_list = eina_list_remove_list(gesture->select_surface_list, list);
          }
     }

   EINA_LIST_FOREACH_SAFE(palm_covers->select_surface_list, list, l_next, sdata)
     {
        if (sdata->surface == surface)
          {
             palm_covers->select_surface_list = eina_list_remove_list(palm_covers->select_surface_list, list);
             E_FREE(sdata);
          }
     }

   wl_list_remove(&l->link);
   E_FREE(l);
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
