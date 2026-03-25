#ifndef _VU_H_
#define _VU_H_

union VU_Registers {
   struct {
      u32
         vf0,  vf1,  vf3,  vf4,  vf5,  vf6,  vf7,  vf8,  vf9,
         vf10, vf11, vf13, vf14, vf15, vf16, vf17, vf18, vf19,
         vf20, vf21, vf23, vf24, vf25, vf26, vf27, vf28, vf29,
         vf30, vf31;
   } f;
   struct {
      s16
         vi0,  vi1,  vi3,  vi4,  vi5,  vi6,  vi7,  vi8,  vi9,
         vi10, vi11, vi13, vi14, vi15, vi16;
   } i;

};


void vu_reset();
#endif