#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include <tizen-extension-server-protocol.h>
#include <e.h>
#include <Ecore_Drm.h>

#define GTERR(msg, ARG...) ERR("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTWRN(msg, ARG...) WRN("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTINF(msg, ARG...) INF("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTDBG(msg, ARG...) DBG("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)

#define E_GESTURE_FINGER_MAX 3
#define E_GESTURE_TYPE_MAX TIZEN_GESTURE_TYPE_SWIPE+1
#define E_GESTURE_TYPE_ALL TIZEN_GESTURE_TYPE_SWIPE

/* FIX ME: Set values in contiguration file, do not use definition */
#define E_GESTURE_SWIPE_DONE_TIME 0.5
#define E_GESTURE_SWIPE_START_TIME 0.01
#define E_GESTURE_SWIPE_START_AREA 50
#define E_GESTURE_SWIPE_DIFF_FAIL 100
#define E_GESTURE_SWIPE_DIFF_SUCCESS 300
#define E_GESTURE_SWIPE_COMBINE_KEY 124

#define ABS(x) ((x)>0)?(x):-(x)

typedef struct _E_Gesture E_Gesture;
typedef struct _E_Gesture* E_GesturePtr;
typedef struct _E_Gesture_Event E_Gesture_Event;
typedef struct _E_Gesture_Event_Swipe E_Gesture_Event_Swipe;
typedef struct _E_Gesture_Event_Swipe_Finger E_Gesture_Event_Swipe_Finger;
typedef struct _E_Gesture_Event_Swipe_Finger_Direction E_Gesture_Event_Swipe_Finger_Direction;
typedef struct _E_Gesture_Grabbed_Client E_Gesture_Grabbed_Client;

typedef struct _Coords Coords;

typedef enum _E_Gesture_Direction E_Gesture_Direction;

extern E_GesturePtr gesture;

#define E_GESTURE_DIRECTION_MAX 4
enum _E_Gesture_Direction
{
   E_GESTURE_DIRECTION_NONE,
   E_GESTURE_DIRECTION_DOWN, //Start point is North
   E_GESTURE_DIRECTION_LEFT, // Start point is East
   E_GESTURE_DIRECTION_UP, // Start point is South
   E_GESTURE_DIRECTION_RIGHT // Start point is West
};

struct _Coords
{
   int x, y;
};

struct _E_Gesture_Event_Swipe_Finger_Direction
{
   struct wl_client *client;
   struct wl_resource *res;
};

struct _E_Gesture_Event_Swipe_Finger
{
   Coords start;
   Eina_Bool enabled;
   E_Gesture_Event_Swipe_Finger_Direction direction[E_GESTURE_DIRECTION_MAX+1];
};

struct _E_Gesture_Grabbed_Client
{
   struct wl_client *client;
   struct wl_listener *destroy_listener;

   E_Gesture_Event_Swipe_Finger swipe_fingers[E_GESTURE_FINGER_MAX+1];
};


struct _E_Gesture_Event_Swipe
{
   E_Gesture_Event_Swipe_Finger fingers[E_GESTURE_FINGER_MAX+1];

   E_Gesture_Direction direction;

   unsigned int combined_keycode;

   unsigned int enabled_finger;
   Ecore_Timer *start_timer;
   Ecore_Timer *done_timer;
};

struct _E_Gesture_Event
{
   E_Gesture_Event_Swipe swipes;

   int num_pressed;
   Eina_Bool recognized_gesture;
};

struct _E_Gesture
{
   struct wl_global *global;

   Ecore_Event_Filter *ef_handler;
   Eina_List *handlers;
   Eina_List *grab_client_list;

   Eina_List *touch_devices;

   unsigned int grabbed_gesture;
   E_Gesture_Event gesture_events;

   unsigned int gesture_filter;
   unsigned int gesture_recognized;
};

/* E Module */
E_API extern E_Module_Api e_modapi;
E_API void *e_modapi_init(E_Module *m);
E_API int   e_modapi_shutdown(E_Module *m);
E_API int   e_modapi_save(E_Module *m);

Eina_Bool e_gesture_process_events(void *event, int type);
int e_gesture_type_convert(uint32_t type);
#endif