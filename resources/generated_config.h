
// The number of subdivisions around the face for each kind of hand.
#define NUM_STEPS_HOUR 48
#define NUM_STEPS_MINUTE 60
#define NUM_STEPS_SECOND 60
#define NUM_STEPS_CHRONO_MINUTE 30
#define NUM_STEPS_CHRONO_SECOND 60
#define NUM_STEPS_CHRONO_TENTH 10

// The location of the hands on the face.
#define HOUR_HAND_X 72
#define HOUR_HAND_Y 84

#define MINUTE_HAND_X 72
#define MINUTE_HAND_Y 84

// The order in which the hands are layered on top of each other.
int stacking_order[] = {
  STACKING_ORDER_HOUR, STACKING_ORDER_MINUTE, STACKING_ORDER_SECOND, STACKING_ORDER_CHRONO_MINUTE, STACKING_ORDER_CHRONO_SECOND, STACKING_ORDER_CHRONO_TENTH, STACKING_ORDER_DONE
};

#if 0
  // The following definition is meant for debugging only.  It enables
  // a quick hack to make minutes fly by like seconds, so you can
  // easily see the hands in several different orientations around the
  // face.
  #define FAST_TIME 1
#endif

#if 1
  #define SHOW_DAY_CARD 1
  #define DAY_CARD_X 52
  #define DAY_CARD_Y 109
  #define DAY_CARD_ON_BLACK 0
  #define DAY_CARD_BOLD 0
#endif

#if 1
  #define SHOW_DATE_CARD 1
  #define DATE_CARD_X 92
  #define DATE_CARD_Y 109
  #define DATE_CARD_ON_BLACK 0
  #define DATE_CARD_BOLD 0
#endif

#if 1
  #define SHOW_SECOND_HAND 1
  #define SECOND_HAND_X 72
  #define SECOND_HAND_Y 84
#endif

#if 1
  #define ENABLE_HOUR_BUZZER 1
#endif

#if 0
  #define MAKE_CHRONOGRAPH 1
#endif

#if 0
  #define SHOW_CHRONO_MINUTE_HAND 1
  #define CHRONO_MINUTE_HAND_X 72
  #define CHRONO_MINUTE_HAND_Y 84
#endif

#if 0
  #define SHOW_CHRONO_SECOND_HAND 1
  #define CHRONO_SECOND_HAND_X 72
  #define CHRONO_SECOND_HAND_Y 84
#endif

#if 0
  #define SHOW_CHRONO_TENTH_HAND 1
  #define CHRONO_TENTH_HAND_X 72
  #define CHRONO_TENTH_HAND_Y 84
#endif

