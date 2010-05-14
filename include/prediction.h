#ifndef __prediction_h_included__
#define __prediction_h_included__

#include "xstr.h"

struct prediction_t {
  int timestamp;
  xstr* str;
};

/* $BM=B,$5$l$?J8;zNs$r3JG<$9$k(B */
int anthy_traverse_record_for_prediction(xstr*, struct prediction_t*);


#endif
