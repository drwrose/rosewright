#ifndef HAND_TABLE_H
#define HAND_TABLE_H

// This file defines the structures built up by config_watch.py and
// stored in resources/generated_table.c to represent the different
// clock hands and their various bitmap permutations.


struct __attribute__((__packed__)) FaceDef {
  uint8_t resource_id;
};

struct __attribute__((__packed__)) FaceColorDef {
  // Clock face background, c1, c2, c3
  uint8_t cb_argb8, c1_argb8, c2_argb8, c3_argb8;

  // Date window background, c1
  uint8_t db_argb8, d1_argb8;
};

// A table of center positions, one for each different bitmap for a
// hand.  This point in the bitmap is the "center" or "pivot" point of
// the bitmap, and indicates the point that corresponds to the hinge
// of the hand.  This point is placed at the place_x, place_y point of
// the hand when it is drawn.
struct __attribute__((__packed__)) BitmapHandCenterRow {
  int8_t cx;
  int8_t cy;
};

// A table of hand positions, one for each different "step" defined
// for each hand (as stored in HandDef.num_steps; see NUM_STEPS_HOUR,
// NUM_STEPS_MINUTE, etc.).  Each hand position defines a bitmap index
// and a flip x/flip y flag.
struct __attribute__((__packed__)) BitmapHandTableRow {
  // bitmap_index is an index into the bitmap_centers table, and also
  // references a particular bitmap and/or mask from the resource
  // file.
  unsigned int bitmap_index:6;

  // flip_x and flip_y are true if the bitmap should be flipped in X
  // and/or Y before drawing.
  bool flip_x:1;
  bool flip_y:1;
};

// A vector definition is an array of groups, where each group is a
// contiguous path.
struct __attribute__((__packed__)) VectorHandGroup {
  // The Pebble path for the hand at 12:00.  This will be rotated to
  // the appropriate angle at runtime.
  GPathInfo path_info;
};

// The array of groups that makes up a vector definition.
struct __attribute__((__packed__)) VectorHand {
  // This is the color channel to be used when drawing vector hands,
  // on Basalt only.
  uint8_t paint_channel;

  uint8_t num_groups;
  struct VectorHandGroup *group;
};

struct __attribute__((__packed__)) HandDef {
  // The number of steps around the circle for this particular hand.
  // This is also stored in the symbol NUM_STEPS_HOUR,
  // NUM_STEPS_MINUTE, and so on.
  uint8_t num_steps;

  // If a bitmap hand is available, all of the bitmap images should
  // appear consecutively in the resource file.  (config_watch.py will
  // ensure this.)  This field defines the resource ID number of the
  // first bitmap image.  If resource_mask_id is the same as
  // resource_id, it means that the bitmap hand does not use a mask
  // and just draws itself with no transparency.  If resource_mask_id
  // is different, it means that the bitmap hand *does* use a mask to
  // implement transparency, and resource_mask_id is the resource ID
  // number of the first bitmap mask image (which should have the same
  // number of entries as the primary bitmaps).
  uint8_t resource_id, resource_mask_id;

  // This defines the position on the Pebble face of the pivot point
  // of the bitmap.
  uint8_t place_x, place_y;

  // This is true if the bitmaps are compressed using the rle_bwd
  // mechanism, or false if they are ordinary PNG's.  (We never use
  // RLE compression on second hands, for instance, to avoid the
  // unneeded cost of constantly decompressing these things.)
  bool use_rle;

  // The table of center values, one for each of bitmap_index.
  struct BitmapHandCenterRow *bitmap_centers;

  // The table of hand positions, one for each of NUM_STEPS.  This
  // table is NULL if bitmaps are not in use for this hand.
  struct BitmapHandTableRow *bitmap_table;

  // The vector definition for the hand, or NULL if vectors are not in
  // use for this hand.
  struct VectorHand *vector_hand;
};

// These symbols are used to define the stacking order for hands.
#define STACKING_ORDER_HOUR 1
#define STACKING_ORDER_MINUTE 2
#define STACKING_ORDER_SECOND 3
#define STACKING_ORDER_CHRONO_MINUTE 4
#define STACKING_ORDER_CHRONO_SECOND 5
#define STACKING_ORDER_CHRONO_TENTH 6
#define STACKING_ORDER_DONE 0

// A table for bluetooth and battery icons, as well as date windows.
struct __attribute__((__packed__)) IndicatorTable {
  uint8_t x, y;
  bool invert;
};

// A handy symbol for wrapping resource ID's that only exist on Aplite.
#ifdef PBL_PLATFORM_APLITE
#define APLITE_RESOURCE(resource_id) (resource_id)
#else
#define APLITE_RESOURCE(resource_id) (0)
#endif  // PBL_PLATFORM_APLITE

#endif

