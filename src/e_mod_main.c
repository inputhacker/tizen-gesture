#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>

E_GesturePtr gesture = NULL;
E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Gesture Module of Window Manager" };

static E_Gesture_Config_Data *_e_gesture_init(E_Module *m);
static void _e_gesture_init_handlers(void);


static void _e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data);

static void
_e_gesture_edge_swipe_set_client_to_list(struct wl_client *client, E_Gesture_Event_Edge_Swipe_Finger *fingers, unsigned int edge)
{
   if (edge & TIZEN_GESTURE_EDGE_TOP)
     fingers->edge[E_GESTURE_EDGE_TOP].client = client;
   if (edge & TIZEN_GESTURE_EDGE_RIGHT)
     fingers->edge[E_GESTURE_EDGE_RIGHT].client = client;
   if (edge & TIZEN_GESTURE_EDGE_BOTTOM)
     fingers->edge[E_GESTURE_EDGE_BOTTOM].client = client;
   if (edge & TIZEN_GESTURE_EDGE_LEFT)
     fingers->edge[E_GESTURE_EDGE_LEFT].client = client;
}

/* Function for registering wl_client destroy listener */
int
e_gesture_add_client_destroy_listener(struct wl_client *client, int mode EINA_UNUSED, int fingers, unsigned int edge)
{
   struct wl_listener *destroy_listener = NULL;
   Eina_List *l;
   E_Gesture_Grabbed_Client *grabbed_client, *data;

   EINA_LIST_FOREACH(gesture->grab_client_list, l, data)
     {
        if (data->client == client)
          {
             _e_gesture_edge_swipe_set_client_to_list(client, &data->edge_swipe_fingers[fingers], edge);

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
   _e_gesture_edge_swipe_set_client_to_list(client, &grabbed_client->edge_swipe_fingers[fingers], edge);

   gesture->grab_client_list = eina_list_append(gesture->grab_client_list, grabbed_client);

   return TIZEN_KEYROUTER_ERROR_NONE;
}

static void
_e_gesture_remove_client_destroy_listener(struct wl_client *client, unsigned int fingers, unsigned int edge)
{
   Eina_List *l, *l_next;
   E_Gesture_Grabbed_Client *data;
   int i;

   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l, l_next, data)
     {
        if (data->client == client)
          {
             _e_gesture_edge_swipe_set_client_to_list(NULL, &data->edge_swipe_fingers[fingers], edge);

             for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
               {
                  if (data->edge_swipe_fingers[i].edge[E_GESTURE_EDGE_TOP].client ||
                      data->edge_swipe_fingers[i].edge[E_GESTURE_EDGE_RIGHT].client ||
                      data->edge_swipe_fingers[i].edge[E_GESTURE_EDGE_BOTTOM].client ||
                      data->edge_swipe_fingers[i].edge[E_GESTURE_EDGE_LEFT].client)
                    {
                       return;
                    }
               }
             wl_list_remove(&data->destroy_listener->link);
             E_FREE(data->destroy_listener);
             gesture->grab_client_list = eina_list_remove(gesture->grab_client_list, data);
             E_FREE(data);
          }
     }
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

   e_gesture_add_client_destroy_listener(client, TIZEN_GESTURE_TYPE_EDGE_SWIPE, fingers, edge & ~grabbed_edge);
   gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_EDGE_SWIPE;
   gev->edge_swipes.fingers[fingers].enabled = EINA_TRUE;
   if (gev->edge_swipes.event_keep) gesture->event_state = E_GESTURE_EVENT_STATE_KEEP;

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
   int i, j;
   E_Gesture_Event *gev;
   unsigned int ungrabbed_edge = 0x0;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("client %p is request ungrab edge swipe gesture, fingers: %d, edge: 0x%x, client: %p\n", client, fingers, edge, gesture->gesture_events.edge_swipes.fingers[0].edge[3].client);

   if (fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
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
        _e_gesture_remove_client_destroy_listener(client, fingers, edge & ~ungrabbed_edge);
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             for (j = 0; j < E_GESTURE_EDGE_MAX+1; j++)
               {
                  if (gev->edge_swipes.fingers[i].edge[j].client)
                    {
                       goto finish;
                    }
               }
             gev->edge_swipes.fingers[i].enabled = EINA_FALSE;
          }
        gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_EDGE_SWIPE;
        if (gev->edge_swipes.event_keep) gesture->event_state = E_GESTURE_EVENT_STATE_PROPAGATE;
     }

finish:
   tizen_gesture_send_grab_edge_swipe_notify(resouce, fingers, edge, ret);
   return;
}

static const struct tizen_gesture_interface _e_gesture_implementation = {
   _e_gesture_cb_grab_edge_swipe,
   _e_gesture_cb_ungrab_edge_swipe
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
   if (e_object_is_del(E_OBJECT(ec)) || !ec->comp_data) return;
   if (!ec->comp_data->aux_hint.changed) return;

   /* Return if the aux hint change didn't happen to the focused ec */
   if (ec != e_client_focused_get()) return;

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
   E_Gesture_Config_Data *gconfig = m->data;
   e_gesture_conf_deinit(gconfig);
   e_gesture_device_shutdown();
   _e_gesture_deinit_handlers();
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
_e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data)
{
   struct wl_client *client = data;
   int i, j;
   Eina_List *l_list, *l_next;
   E_Gesture_Grabbed_Client *client_data;

   if (gesture->grabbed_gesture & TIZEN_GESTURE_TYPE_EDGE_SWIPE)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             for (j = 0; j < E_GESTURE_EDGE_MAX+1; j++)
               {
                  if (gesture->gesture_events.edge_swipes.fingers[i].edge[j].client == client)
                    {
                       gesture->gesture_events.edge_swipes.fingers[i].edge[j].client = NULL;
                       gesture->gesture_events.edge_swipes.fingers[i].edge[j].res = NULL;
                    }
               }
          }

        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             for (j = 0; j < E_GESTURE_EDGE_MAX+1; j++)
               {
                  if (gesture->gesture_events.edge_swipes.fingers[i].edge[j].client)
                    {
                       goto out;
                    }
               }
             gesture->gesture_events.edge_swipes.fingers[i].enabled = EINA_FALSE;
          }
        gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_EDGE_SWIPE;
     }

out:
   E_FREE(l);
   l = NULL;
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
