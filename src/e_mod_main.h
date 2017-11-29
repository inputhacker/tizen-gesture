#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include <e.h>
#include <tizen-extension-server-protocol.h>
#include <Ecore_Input_Evas.h>

#define GTERR(msg, ARG...) ERR("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTWRN(msg, ARG...) WRN("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTINF(msg, ARG...) INF("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTDBG(msg, ARG...) DBG("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)

#define E_GESTURE_SERVER_CLIENT (void *)0x1

#define E_GESTURE_TYPE_EDGE_SWIPE TIZEN_GESTURE_TYPE_EDGE_SWIPE
#define E_GESTURE_TYPE_EDGE_DRAG TIZEN_GESTURE_TYPE_EDGE_DRAG
#define E_GESTURE_TYPE_TAP TIZEN_GESTURE_TYPE_TAP
#define E_GESTURE_TYPE_PALM_COVER TIZEN_GESTURE_TYPE_PALM_COVER
#define E_GESTURE_TYPE_PAN TIZEN_GESTURE_TYPE_PAN
#define E_GESTURE_TYPE_PINCH TIZEN_GESTURE_TYPE_PINCH
#define E_GESTURE_TYPE_PALM_SWIPE TIZEN_GESTURE_TYPE_PALM_SWIPE

#define E_GESTURE_FINGER_MAX 3
#define E_GESTURE_TYPE_MAX (E_GESTURE_TYPE_PALM_SWIPE + 1)
#define E_GESTURE_TYPE_ALL (E_GESTURE_TYPE_EDGE_SWIPE | E_GESTURE_TYPE_EDGE_DRAG | E_GESTURE_TYPE_TAP | E_GESTURE_TYPE_PAN | E_GESTURE_TYPE_PINCH | E_GESTURE_TYPE_PALM_COVER | E_GESTURE_TYPE_PALM_SWIPE)
#define E_GESTURE_KEYBOARD_NAME "Gesture Keyboard"
#define E_GESTURE_AUX_HINT_GESTURE_DISABLE "wm.policy.win.gesture.disable"

/* FIX ME: Set values in contiguration file, do not use definition */
#define E_GESTURE_KEYBOARD_DEVICE "Any"

#define E_GESTURE_EDGE_SWIPE_DONE_TIME 0.5
#define E_GESTURE_EDGE_SWIPE_START_TIME 0.01
#define E_GESTURE_EDGE_SWIPE_START_AREA 50
#define E_GESTURE_EDGE_SWIPE_DIFF_FAIL 100
#define E_GESTURE_EDGE_SWIPE_DIFF_SUCCESS 300
/* FIX ME: Key code will be get from keymap */
#define E_GESTURE_EDGE_SWIPE_COMBINE_KEY 124
#define E_GESTURE_EDGE_SWIPE_BACK_KEY 166
#define E_GESTURE_EDGE_SWIPE_BACK_DEFAULT_ENABLE EINA_TRUE

#define E_GESTURE_EDGE_DRAG_START_TIME 0.01
#define E_GESTURE_EDGE_DRAG_START_AREA 50
#define E_GESTURE_EDGE_DRAG_DIFF 10

#define E_GESTURE_TAP_REPEATS_MAX 3
#define E_GESTURE_TAP_START_TIME 0.05
#define E_GESTURE_TAP_DONE_TIME 1.0
#define E_GESTURE_TAP_INTERVAL_TIME 1.0
#define E_GESTURE_TAP_MOVING_RANGE 25

#define E_GESTURE_PAN_START_TIME 0.05
#define E_GESTURE_PAN_MOVING_RANGE 15

#define E_GESTURE_PINCH_MOVING_DISTANCE_RANGE 15.0

#define ABS(x) (((x)>0)?(x):-(x))
#define RAD2DEG(x) ((x) * 57.295779513)

typedef struct _E_Gesture E_Gesture;
typedef struct _E_Gesture* E_GesturePtr;
typedef struct _E_Gesture_Event E_Gesture_Event;
typedef struct _E_Gesture_Event_Edge E_Gesture_Event_Edge;
typedef struct _E_Gesture_Event_Edge_Swipe E_Gesture_Event_Edge_Swipe;
typedef struct _E_Gesture_Event_Edge_Drag E_Gesture_Event_Edge_Drag;
typedef struct _E_Gesture_Event_Edge_Finger E_Gesture_Event_Edge_Finger;
typedef struct _E_Gesture_Event_Edge_Finger_Edge E_Gesture_Event_Edge_Finger_Edge;
typedef struct _E_Gesture_Grabbed_Client E_Gesture_Grabbed_Client;
typedef struct _E_Gesture_Conf_Edd E_Gesture_Conf_Edd;
typedef struct _E_Gesture_Config_Data E_Gesture_Config_Data;

typedef struct _E_Gesture_Event_Tap_Finger_Repeats E_Gesture_Event_Tap_Finger_Repeats;
typedef struct _E_Gesture_Event_Tap_Finger E_Gesture_Event_Tap_Finger;
typedef struct _E_Gesture_Event_Tap E_Gesture_Event_Tap;

typedef struct _E_Gesture_Event_Pan E_Gesture_Event_Pan;

typedef struct _E_Gesture_Event_Pinch E_Gesture_Event_Pinch;

typedef struct _E_Gesture_Event_Palm_Cover E_Gesture_Event_Palm_Cover;

typedef struct _Coords Coords;
typedef struct _Rect Rect;
typedef struct _E_Gesture_Finger E_Gesture_Finger;
typedef struct _E_Gesture_Event_Info E_Gesture_Event_Info;
typedef struct _E_Gesture_Event_Client E_Gesture_Event_Client;
typedef struct _E_Gesture_Select_Surface E_Gesture_Select_Surface;
typedef struct _E_Gesture_Activate_Surface_Info E_Gesture_Activate_Surface_Info;
typedef struct _E_Gesture_Activate_Info E_Gesture_Activate_Info;

typedef enum _E_Gesture_Edge E_Gesture_Edge;
typedef enum _E_Gesture_Event_State E_Gesture_Event_State;
typedef enum _E_Gesture_Tap_State E_Gesture_Tap_State;
typedef enum _E_Gesture_PanPinch_State E_Gesture_PanPinch_State;

extern E_GesturePtr gesture;

#define E_GESTURE_EDGE_MAX 4

#define E_GESTURE_EDGE_ALL ((1 << E_GESTURE_EDGE_TOP) | (1 << E_GESTURE_EDGE_RIGHT) | (1 << E_GESTURE_EDGE_BOTTOM) | (1 << E_GESTURE_EDGE_LEFT))

enum _E_Gesture_Event_State
{
   E_GESTURE_EVENT_STATE_PROPAGATE,
   E_GESTURE_EVENT_STATE_KEEP,
   E_GESTURE_EVENT_STATE_IGNORE
};

enum _E_Gesture_Tap_State
{
   E_GESTURE_TAP_STATE_NONE,
   E_GESTURE_TAP_STATE_READY, // tap is required, idle
   E_GESTURE_TAP_STATE_START, // first finger is pressed
   E_GESTURE_TAP_STATE_PROCESS, // all fingers are pressed or first release
   E_GESTURE_TAP_STATE_WAIT, // all fingers are released and wait next tap
   E_GESTURE_TAP_STATE_DONE
};

enum _E_Gesture_PanPinch_State
{
   E_GESTURE_PANPINCH_STATE_NONE,
   E_GESTURE_PANPINCH_STATE_READY,
   E_GESTURE_PANPINCH_STATE_START,
   E_GESTURE_PANPINCH_STATE_MOVING,
   E_GESTURE_PANPINCH_STATE_DONE
};

struct _Coords
{
   int x, y;
};

struct _Rect
{
   int x1, y1;
   int x2, y2;
};

struct _E_Gesture_Finger
{
   Eina_Bool pressed;
   Coords axis;
};

struct _E_Gesture_Event_Info
{
   int type;
   void *event;
};

struct _E_Gesture_Activate_Surface_Info
{
   Eina_Bool active;
   struct wl_resource *surface;
};

struct _E_Gesture_Activate_Info
{
   Eina_Bool active;
   struct wl_client *client;
   Eina_List *surfaces;
};

struct _E_Gesture_Conf_Edd
{
   char *key_device_name;              // The name of keyboard device to generate key events
   Eina_Bool event_keep;               // 1: Do not propagate events to client immediatly until recognizing gestures
                                       // 0: Propagate events immediatly but send a cancel event after recognizing gestures (default)
   struct
   {
      double time_done;                // The duration to recognize a edge swipe
      double time_begin;               // The minimun of time to distinguish if it is a gesture or normal touch
      int area_offset;                 // Edge region to start to calculate
      int min_length;                  // Limit size to move cross direction
      int max_length;                  // Moving length to recognize edge swipe
      int compose_key;                 // Keycode of composed with gesture
      int back_key;                    // Keycode of back key
      Eina_Bool default_enable_back;   // Convert a top edge swipe to back key
   } edge_swipe;
   struct
     {
        double time_begin;             // The minimun of time to distinguish if it is a gesture or normal touch
        int area_offset;               // Edge region to start to calculate
        int diff_length;               // Moved length to generate edge drag events
     } edge_drag;
   struct
     {
        int repeats_max;               // Maximum number of repeats
        double time_start;             // Minimun of time to distinguish if it is a gesture or normal touch
        double time_done;              // Duration to release all fingers
        double time_interval;          // Interval duration between taps
        int moving_range;              // Limit region duriong taps
     } tap;
   struct
     {
        double time_start;             // The minimun of time to distinguish if it is a gesture or normal touch
        int moving_range;              // Moved length to generate pan events
     } pan;
   struct
     {
        double moving_distance_range;  // Moved length to generate pinch events
     } pinch;
};

struct _E_Gesture_Config_Data
{
   E_Module *module;
   E_Config_DD *conf_edd;
   E_Gesture_Conf_Edd *conf;
};

struct _E_Gesture_Event_Client
{
   struct wl_client *client;
   struct wl_resource *res;
};

struct _E_Gesture_Select_Surface
{
   struct wl_resource *surface;
   struct wl_resource *res;
};

struct _E_Gesture_Event_Edge_Finger_Edge
{
   struct wl_client *client;
   struct wl_resource *res;
   unsigned int sp;
   unsigned int ep;
};

struct _E_Gesture_Event_Edge_Finger
{
   Coords start;
   Eina_Bool enabled;
   Eina_List *edge[E_GESTURE_EDGE_MAX + 1];
};

struct _E_Gesture_Event_Edge
{
   E_Gesture_Activate_Info activation;
   E_Gesture_Event_Edge_Finger fingers[E_GESTURE_FINGER_MAX + 2];

   unsigned int edge;

   unsigned int enabled_finger;
   unsigned int enabled_edge;
};

struct _E_Gesture_Event_Edge_Swipe
{
   E_Gesture_Event_Edge base;

   unsigned int combined_keycode;
   unsigned int back_keycode;

   Ecore_Timer *start_timer;
   Ecore_Timer *done_timer;
};

struct _E_Gesture_Event_Edge_Drag
{
   E_Gesture_Event_Edge base;

   Ecore_Timer *start_timer;

   int idx;

   Coords start_point;
   Coords center_point;
};

struct _E_Gesture_Event_Tap_Finger_Repeats
{
   struct wl_client *client;
   struct wl_resource *res;
};

struct _E_Gesture_Event_Tap_Finger
{
   Eina_Bool enabled;
   unsigned int max_repeats;
   E_Gesture_Event_Tap_Finger_Repeats repeats[E_GESTURE_TAP_REPEATS_MAX + 1];
};

struct _E_Gesture_Event_Tap
{
   E_Gesture_Activate_Info activation;
   E_Gesture_Event_Tap_Finger fingers[E_GESTURE_FINGER_MAX + 2];
   E_Gesture_Tap_State state;
   unsigned int enabled_finger;
   unsigned int current_finger;
   unsigned int repeats;
   unsigned int max_fingers;

   /* pressed timer */
   Ecore_Timer *start_timer;
   /* release timer */
   Ecore_Timer *done_timer;
   /* interval timer */
   Ecore_Timer *interval_timer;

   Rect base_rect;
};

struct _E_Gesture_Event_Pan
{
   E_Gesture_Activate_Info activation;
   E_Gesture_Event_Client fingers[E_GESTURE_FINGER_MAX + 2];
   E_Gesture_PanPinch_State state;
   Coords start_point;
   Coords prev_point;
   Coords center_point;
   int num_pan_fingers;

   Ecore_Timer *start_timer;
   Ecore_Timer *move_timer;
};

struct _E_Gesture_Event_Pinch
{
   E_Gesture_Activate_Info activation;
   E_Gesture_Event_Client fingers[E_GESTURE_FINGER_MAX + 2];
   E_Gesture_PanPinch_State state;
   double distance;
   int num_pinch_fingers;

   Ecore_Timer *start_timer;
   Ecore_Timer *move_timer;
};

struct _E_Gesture_Event_Palm_Cover
{
   E_Gesture_Activate_Info activation;
   E_Gesture_Event_Client client_info;
   Eina_List *select_surface_list;
   unsigned int start_time;
};

struct _E_Gesture_Grabbed_Client
{
   struct wl_client *client;
   struct wl_listener *destroy_listener;
   unsigned int grabbed_gesture;

   E_Gesture_Event_Edge_Finger edge_swipe_fingers[E_GESTURE_FINGER_MAX + 2];
   E_Gesture_Event_Edge_Finger edge_drag_fingers[E_GESTURE_FINGER_MAX + 2];
   E_Gesture_Event_Tap_Finger tap_fingers[E_GESTURE_FINGER_MAX + 2];
   E_Gesture_Event_Client pan_fingers[E_GESTURE_FINGER_MAX + 2];
   E_Gesture_Event_Client pinch_fingers[E_GESTURE_FINGER_MAX + 2];
   E_Gesture_Event_Client palm_cover;
};

struct _E_Gesture_Event
{
   E_Gesture_Event_Edge_Swipe edge_swipes;
   E_Gesture_Event_Edge_Drag edge_drags;
   E_Gesture_Event_Tap taps;
   E_Gesture_Event_Pan pans;
   E_Gesture_Event_Pinch pinchs;
   E_Gesture_Event_Palm_Cover palm_covers;

   E_Gesture_Finger base_point[E_GESTURE_FINGER_MAX + 2];

   int num_pressed;
   Eina_Bool recognized_gesture;
   Eina_Bool event_keep;
};

struct _E_Gesture
{
   struct wl_global *global;
   E_Gesture_Config_Data *config;
   Eina_Bool enable;
   Eina_Bool enabled_window;

   Ecore_Event_Filter *ef_handler;
   Eina_List *handlers;
   Eina_List *grab_client_list;
   Eina_List *select_surface_list;
   Eina_List *hooks;

   struct
   {
      Eina_List *touch_devices;
      int uinp_fd;
      char *kbd_identifier;
      char *kbd_name;
      Evas_Device *kbd_device;
   } device;

   unsigned int grabbed_gesture;
   E_Gesture_Event gesture_events;
   E_Gesture_Event_State event_state;

   Eina_List *event_queue;

   unsigned int gesture_filter;
   unsigned int gesture_recognized;
};

/* E Module */
E_API extern E_Module_Api e_modapi;
E_API void *e_modapi_init(E_Module *m);
E_API int   e_modapi_shutdown(E_Module *m);
E_API int   e_modapi_save(E_Module *m);

void e_gesture_event_deactivate_check(void);
Eina_Bool e_gesture_process_events(void *event, int type);
int e_gesture_type_convert(uint32_t type);

/* Config */
void e_gesture_conf_init(E_Gesture_Config_Data *gconfig);
void e_gesture_conf_deinit(E_Gesture_Config_Data *gconfig);

/* Device control */
void e_gesture_device_shutdown(void);
E_Gesture_Event_State e_gesture_device_add(E_Input_Event_Input_Device_Add *ev);
E_Gesture_Event_State e_gesture_device_del(E_Input_Event_Input_Device_Del *ev);
Eina_Bool e_gesture_is_touch_device(const Evas_Device *dev);
void e_gesture_device_keydev_set(char *option);

void e_gesture_event_filter_enable(Eina_Bool enabled);

/* Util functions */
unsigned int e_gesture_util_tap_max_fingers_get(void);
unsigned int e_gesture_util_tap_max_repeats_get(unsigned int);

#endif
