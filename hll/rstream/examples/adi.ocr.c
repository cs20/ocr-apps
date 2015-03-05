/* Auto-generated by R-Stream without advanced optimizations */

#include <rstream_ocr.h>
#include <ocr.h>
union ocrGuidUnion_t;
struct ocrEdtDepStruct_t;
struct __anon_21;
union __args_ad1_22;
union ocrGuidUnion_t;
struct ocrEdtDepStruct_t;
struct __anon_23;
union __args_ad_main_24;
union ocrGuidUnion_t;
struct __anon_21 {
   float (* R)[500];
   float (* P)[500];
   float (* Q)[500];
   int IT0;
   int IT1;
   int IT2;
};
union __args_ad1_22 {
   struct __anon_21 data;
   double padding[5];
};
struct __anon_23 {
   float (* R)[500];
   float (* P)[500];
   float (* Q)[500];
};
union __args_ad_main_24 {
   struct __anon_23 data;
   double padding[3];
};
/*
 * Forward declarations of functions
 */
void print_submatrix(char const*, void const*, int, int, int, int, int, int);
int check_vectors(char const*, char const*, void const*, void const*, int);
void initialize(void);
void initialize_once(void);
void show(void);
void kernel(void);
int check(void);
double flops_per_trial(void);
void ad(float (* P)[500], float (* Q)[500], float (* R)[500]);
static ocrGuid_t ad1(unsigned int paramc, unsigned long* paramv, unsigned int
   depc, ocrEdtDep_t* depv);
void* rocrArgs(ocrEdtDep_t*);
void x86_dma_get(void const volatile*, void volatile*, unsigned long, long,
   long, unsigned long, int);
void x86_dma_wait(int);
void x86_dma_put(void const volatile*, void volatile*, unsigned long, long,
   long, unsigned long, int);
static ocrGuid_t ad_main(unsigned int paramc, unsigned long* paramv,
   unsigned int depc, ocrEdtDep_t* depv);
void rocrExit(void);
/*
 * Forward declarations of global variables
 */
extern int add_flop;
extern int mul_flop;
/* This can be redefined to be the appropriate 'inline' keyword */
#ifndef RSTREAM_INLINE
#define RSTREAM_INLINE
#endif
#ifdef GPU_TARGET
    #define __maxs_32(x,y) \
 max((int)(x),(int)(y))
#else
static RSTREAM_INLINE int __maxs_32(int x, int y) {
   return x > y ? x : y;
}
#endif
#ifdef GPU_TARGET
    #define __mins_32(x,y) \
 min((int)(x),(int)(y))
#else
static RSTREAM_INLINE int __mins_32(int x, int y) {
   return x < y ? x : y;
}
#endif
/*
 * Definitions of global variables
 */
extern int add_flop;
extern int mul_flop;
static float X[500][500];
static float A[500][500];
static float B[500][500];
static float TX[500][500];
static float TA[500][500];
static float TB[500][500];
int nb_samples = 250000;
char const* function_name = "adi";
/*
 * Definitions of functions
 */
/*
 * ../src/adi.c:71.6
 */
void initialize(void)
{
   int i;
   int i_1;
   int i_2;
   int i_3;
   for (i = 0,
        i_1 = 0,
        i_2 = 0,
        i_3 = 0;
        i < 500;
        i++,
        i_1++,
        i_2 += 0,
        i_3++) {
      float* _t1;
      float* _t2;
      float* _t3;
      float* _t4;
      float* _t5;
      float* _t6;
      int j;
      int j_1;
      int j_2;
      for (_t5 = B[i],
           _t6 = TB[i],
           _t4 = TA[i],
           _t3 = A[i],
           _t2 = TX[i],
           _t1 = X[i],
           j = 0,
           j_1 = i_1,
           j_2 = i_2;
           j < 500; j++, j_2 += i_3, j_1++) {
         float _t7;
         float _t8;
         float _t9;
         _t7 = (float)((double)j_1 + 100.23);
         _t2[j] = _t7;
         _t1[j] = _t7;
         _t8 = (float)((double)j_2 * 3.23);
         _t4[j] = _t8;
         _t3[j] = _t8;
         _t9 = (float)((double)(j_2 * j_1) + 55.5);
         _t6[j] = _t9;
         _t5[j] = _t9;
      }
   }
}

/*
 * ../src/adi.c:83.6
 */
void initialize_once(void)
{
   initialize();
}

/*
 * ../src/adi.c:87.6
 */
void show(void)
{
   void const* _t1;
   _t1 = A;
   print_submatrix("A", _t1, 500, 500, 0, 8, 0, 8);
   print_submatrix("A", _t1, 500, 500, 492, 500, 492, 500);
}

/*
 * ../src/adi.c:97.6
 */
void kernel(void)
{
   union __args_ad_main_24* allArgs;
   ocrGuid_t _t1;
   union __args_ad_main_24* _t2;
   rocrDeclareType(ad_main, 1, (unsigned int)1, 1);
   rocrDeclareType(ad1, 0, (unsigned int)3969, 0);
   rocrInit();
   _t1 = rocrAlloc((void**)&allArgs, 24ul);
   _t2 = allArgs;
   _t2->data.R = X;
   _t2->data.P = A;
   _t2->data.Q = B;
   rocrExecute(1, _t1);
   rocrExit();
}

/*
 * ../src/adi.c:101.5
 */
int check(void)
{
   int t;
   for (t = 0; t < 500; t++) {
      int i1;
      int i1_1;
      int i1_2;
      for (i1 = 0; i1 < 500; i1++) {
         float* _t1;
         float* _t2;
         float* _t3;
         int i2;
         int i2_1;
         for (_t3 = TB[i1],
              _t2 = TA[i1],
              _t1 = TX[i1],
              i2 = 1,
              i2_1 = 0;
              i2 < 500; i2++, i2_1++) {
            float* _t4;
            float* _t5;
            float _t6;
            _t4 = _t2 + i2;
            _t5 = _t3 + i2_1;
            _t1[i2] = _t1[i2] - _t1[i2_1] * *_t4 / *_t5;
            _t6 = *_t4;
            _t3[i2] = _t3[i2] - _t6 * _t6 / *_t5;
         }
      }
      for (i1_1 = 1, i1_2 = 0; i1_1 < 500; i1_1++, i1_2++) {
         float* _t7;
         float* _t8;
         float* _t9;
         float* _t10;
         float* _t11;
         int i2;
         for (_t11 = TB[i1_1],
              _t9 = TA[i1_1],
              _t10 = TB[i1_2],
              _t8 = TX[i1_2],
              _t7 = TX[i1_1],
              i2 = 0;
              i2 < 500; i2++) {
            float* _t12;
            float* _t13;
            float _t14;
            _t12 = _t9 + i2;
            _t13 = _t10 + i2;
            _t7[i2] = _t7[i2] - _t8[i2] * *_t12 / *_t13;
            _t14 = *_t12;
            _t11[i2] = _t11[i2] - _t14 * _t14 / *_t13;
         }
      }
   }
   return check_vectors("A", "TA", A, TA, 500);
}

/*
 * ../src/adi.c:107.8
 */
double flops_per_trial(void)
{
   return (double)(mul_flop + add_flop) * 4.0 * 500.0 * 500.0 * 500.0;
}

void ad(float (* P)[500], float (* Q)[500], float (* R)[500])
{
   union __args_ad_main_24* allArgs;
   ocrGuid_t _t1;
   union __args_ad_main_24* _t2;
   rocrDeclareType(ad_main, 1, (unsigned int)1, 1);
   rocrDeclareType(ad1, 0, (unsigned int)3969, 0);
   rocrInit();
   _t1 = rocrAlloc((void**)&allArgs, 24ul);
   _t2 = allArgs;
   _t2->data.R = R;
   _t2->data.P = P;
   _t2->data.Q = Q;
   rocrExecute(1, _t1);
   rocrExit();
}

static ocrGuid_t ad1(unsigned int paramc, unsigned long* paramv, unsigned int
   depc, ocrEdtDep_t* depv)
{
   union __args_ad1_22 ad1_args;
   float (* R_l)[24];
   float (* P_l)[24];
   float (* Q_l)[24];
   float (* R)[500];
   float (* P)[500];
   float (* Q)[500];
   int IT0;
   int IT1;
   ocrGuid_t _t1;
   ocrGuid_t _t2;
   ocrGuid_t _t3;
   int _t4;
   int i;
   ad1_args = *(union __args_ad1_22*)rocrArgs(depv);
   _t1 = rocrAlloc((void**)&R_l, 48000ul);
   _t2 = rocrAlloc((void**)&P_l, 48000ul);
   _t3 = rocrAlloc((void**)&Q_l, 48000ul);
   IT1 = ad1_args.data.IT1;
   IT0 = ad1_args.data.IT0;
   R = ad1_args.data.R;
   x86_dma_get(R[0] + ((__maxs_32(8 * IT0 + -492, __maxs_32(8 * IT0 + -16 * IT1
      + 7, 0))) + -8 * IT0 + (16 * IT1 + -7)), R_l[0] + (__maxs_32(8 * IT0 +
      -492, __maxs_32(8 * IT0 + -16 * IT1 + 7, 0))), (unsigned long)(
      unsigned int)((__mins_32(__mins_32(-8 * IT0 + 515, -8 * IT0 + 16 * IT1 +
      16), __mins_32(-16 * IT1 + 998, __mins_32(23, 8 * IT0 + -16 * IT1 + 506))
      )) * 4), (long)2000, (long)96, (unsigned long)(unsigned int)500, 0);
   Q = ad1_args.data.Q;
   P = ad1_args.data.P;
   if (- IT0 + 2 * IT1 >= 61) {
      x86_dma_get(R[0] + 499, R_l[0] + (8 * IT0 + (-16 * IT1 + 506)), (
         unsigned long)(unsigned int)4, (long)2000, (long)96, (unsigned long)(
         unsigned int)499, 0);
      x86_dma_get(R[499] + 499, R_l[499] + (8 * IT0 + (-16 * IT1 + 506)), (
         unsigned long)(unsigned int)4, (long)0, (long)0, (unsigned long)(
         unsigned int)1, 0);
   }
   if (IT0 + -2 * IT1 >= -60) {
      x86_dma_get(R[0] + (-8 * IT0 + (16 * IT1 + 16)), R_l[0] + 23, (
         unsigned long)(unsigned int)4, (long)2000, (long)96, (unsigned long)(
         unsigned int)499, 0);
      x86_dma_get(R[499] + (-8 * IT0 + (16 * IT1 + 16)), R_l[499] + 23, (
         unsigned long)(unsigned int)4, (long)0, (long)0, (unsigned long)(
         unsigned int)1, 0);
   }
   x86_dma_get(P[0] + ((__maxs_32(8 * IT0 + -491, __maxs_32(8 * IT0 + -16 * IT1
      + 8, 1))) + -8 * IT0 + (16 * IT1 + -7)), P_l[0] + (__maxs_32(8 * IT0 +
      -491, __maxs_32(8 * IT0 + -16 * IT1 + 8, 1))), (unsigned long)(
      unsigned int)((__mins_32(__mins_32(-8 * IT0 + 515, -8 * IT0 + 16 * IT1 +
      16), __mins_32(-16 * IT1 + 998, __mins_32(23, 8 * IT0 + -16 * IT1 + 506))
      )) * 4), (long)2000, (long)96, (unsigned long)(unsigned int)500, 0);
   if (IT0 + -2 * IT1 >= 0) {
      x86_dma_get(P[1] + 0, P_l[1] + (8 * IT0 + (-16 * IT1 + 7)), (
         unsigned long)(unsigned int)4, (long)2000, (long)96, (unsigned long)(
         unsigned int)499, 0);
   }
   if (IT0 == 62 && IT1 >= 32) {
      x86_dma_get(P[1] + (16 * IT1 + -499), P_l[1] + (8 * IT0 + -492), (
         unsigned long)(unsigned int)4, (long)2000, (long)96, (unsigned long)(
         unsigned int)499, 0);
   }
   if (IT0 <= 61 && - IT0 + 2 * IT1 >= 1) {
      x86_dma_get(P[1] + (-8 * IT0 + (16 * IT1 + -7)), P_l[1] + 0, (
         unsigned long)(unsigned int)4, (long)2000, (long)96, (unsigned long)(
         unsigned int)499, 0);
   }
   x86_dma_get(Q[0] + ((__maxs_32(8 * IT0 + -492, __maxs_32(8 * IT0 + -16 * IT1
      + 7, 0))) + -8 * IT0 + (16 * IT1 + -7)), Q_l[0] + (__maxs_32(8 * IT0 +
      -492, __maxs_32(8 * IT0 + -16 * IT1 + 7, 0))), (unsigned long)(
      unsigned int)((__mins_32(__mins_32(-8 * IT0 + 515, -8 * IT0 + 16 * IT1 +
      16), __mins_32(-16 * IT1 + 998, __mins_32(23, 8 * IT0 + -16 * IT1 + 506))
      )) * 4), (long)2000, (long)96, (unsigned long)(unsigned int)500, 0);
   if (- IT0 + 2 * IT1 >= 61) {
      x86_dma_get(Q[0] + 499, Q_l[0] + (8 * IT0 + (-16 * IT1 + 506)), (
         unsigned long)(unsigned int)4, (long)2000, (long)96, (unsigned long)(
         unsigned int)499, 0);
   }
   if (IT0 + -2 * IT1 >= -60) {
      x86_dma_get(Q[0] + (-8 * IT0 + (16 * IT1 + 16)), Q_l[0] + 23, (
         unsigned long)(unsigned int)4, (long)2000, (long)96, (unsigned long)(
         unsigned int)500, 0);
   }
   if (- IT0 + 2 * IT1 >= 61) {
      x86_dma_get(Q[499] + 499, Q_l[499] + (8 * IT0 + (-16 * IT1 + 506)), (
         unsigned long)(unsigned int)4, (long)0, (long)0, (unsigned long)(
         unsigned int)1, 0);
   }
   x86_dma_wait(0);
   if (IT0 + -2 * IT1 == -63 && IT1 >= 32) {
      int _t5;
      int i_1;
      for (_t5 = 16 * IT1, i_1 = 16 * IT1 + -498; i_1 <= _t5; i_1++) {
         float _t6;
         R_l[i_1 + (-16 * IT1 + 499)][2] = R_l[i_1 + (-16 * IT1 + 499)][2] - R_l
            [i_1 + (-16 * IT1 + 498)][2] * P_l[i_1 + (-16 * IT1 + 499)][2] / Q_l
            [i_1 + (-16 * IT1 + 498)][2];
         _t6 = P_l[i_1 + (-16 * IT1 + 499)][2];
         Q_l[i_1 + (-16 * IT1 + 499)][2] = Q_l[i_1 + (-16 * IT1 + 499)][2] - _t6
            * _t6 / Q_l[i_1 + (-16 * IT1 + 498)][2];
      }
   }
   for (_t4 = (__mins_32(-8 * IT0 + 499, 7)), i = (__maxs_32(0, -8 * IT0 + 16 *
           IT1 + -498)); i <= _t4; i++) {
      int _t7;
      int j;
      for (_t7 = (__mins_32(15, i + 8 * IT0 + -16 * IT1 + 498)), j = (__maxs_32
              (i + 8 * IT0 + -16 * IT1, 0)); j <= _t7; j++) {
         float _t8;
         int _t9;
         int k;
         R_l[0][-1 * i + (j + 8)] = R_l[0][-1 * i + (j + 8)] - R_l[0][-1 * i + (
            j + 7)] * P_l[0][-1 * i + (j + 8)] / Q_l[0][-1 * i + (j + 7)];
         _t8 = P_l[0][-1 * i + (j + 8)];
         Q_l[0][-1 * i + (j + 8)] = Q_l[0][-1 * i + (j + 8)] - _t8 * _t8 / Q_l[
            0][-1 * i + (j + 7)];
         for (_t9 = i + 8 * IT0 + 499, k = i + 8 * IT0 + 1; k <= _t9; k++) {
            float _t10;
            float _t11;
            R_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 8)] = R_l[-1 * i + (k +
               -8 * IT0)][-1 * i + (j + 8)] - R_l[-1 * i + (k + -8 * IT0)][-1 *
               i + (j + 7)] * P_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 8)] /
               Q_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 7)];
            _t10 = P_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 8)];
            Q_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 8)] = Q_l[-1 * i + (k +
               -8 * IT0)][-1 * i + (j + 8)] - _t10 * _t10 / Q_l[-1 * i + (k + -8
               * IT0)][-1 * i + (j + 7)];
            R_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 7)] = R_l[-1 * i + (k +
               -8 * IT0)][-1 * i + (j + 7)] - R_l[-1 * i + k + (-8 * IT0 + -1)]
               [-1 * i + (j + 7)] * P_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 7
               )] / Q_l[-1 * i + k + (-8 * IT0 + -1)][-1 * i + (j + 7)];
            _t11 = P_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 7)];
            Q_l[-1 * i + (k + -8 * IT0)][-1 * i + (j + 7)] = Q_l[-1 * i + (k +
               -8 * IT0)][-1 * i + (j + 7)] - _t11 * _t11 / Q_l[-1 * i + k + (
               -8 * IT0 + -1)][-1 * i + (j + 7)];
         }
      }
      if (- i + -8 * IT0 + 16 * IT1 >= 484) {
         int _t12;
         int j_1;
         for (_t12 = i + 8 * IT0 + 499, j_1 = i + 8 * IT0 + 1; j_1 <= _t12; j_1
                 ++) {
            float _t13;
            R_l[-1 * i + (j_1 + -8 * IT0)][8 * IT0 + (-16 * IT1 + 506)] = R_l[
               -1 * i + (j_1 + -8 * IT0)][8 * IT0 + (-16 * IT1 + 506)] - R_l[-1
               * i + j_1 + (-8 * IT0 + -1)][8 * IT0 + (-16 * IT1 + 506)] * P_l[
               -1 * i + (j_1 + -8 * IT0)][8 * IT0 + (-16 * IT1 + 506)] / Q_l[-1
               * i + j_1 + (-8 * IT0 + -1)][8 * IT0 + (-16 * IT1 + 506)];
            _t13 = P_l[-1 * i + (j_1 + -8 * IT0)][8 * IT0 + (-16 * IT1 + 506)];
            Q_l[-1 * i + (j_1 + -8 * IT0)][8 * IT0 + (-16 * IT1 + 506)] = Q_l[
               -1 * i + (j_1 + -8 * IT0)][8 * IT0 + (-16 * IT1 + 506)] - _t13 *
               _t13 / Q_l[-1 * i + j_1 + (-8 * IT0 + -1)][8 * IT0 + (-16 * IT1 +
               506)];
         }
      }
   }
   x86_dma_put(R_l[0] + (__maxs_32(8 * IT0 + -491, __maxs_32(8 * IT0 + -16 * IT1
      + 8, 1))), R[0] + ((__maxs_32(8 * IT0 + -491, __maxs_32(8 * IT0 + -16 *
      IT1 + 8, 1))) + -8 * IT0 + (16 * IT1 + -7)), (unsigned long)(unsigned int
      )((__mins_32(__mins_32(-8 * IT0 + 515, -8 * IT0 + 16 * IT1 + 16),
      __mins_32(-16 * IT1 + 998, __mins_32(23, 8 * IT0 + -16 * IT1 + 506)))) * 4
      ), (long)96, (long)2000, (unsigned long)(unsigned int)500, 1);
   if (IT0 + -2 * IT1 >= 0) {
      x86_dma_put(R_l[1] + (8 * IT0 + (-16 * IT1 + 7)), R[1] + 0, (
         unsigned long)(unsigned int)4, (long)96, (long)2000, (unsigned long)(
         unsigned int)499, 1);
   }
   if (IT0 == 62 && IT1 >= 32) {
      x86_dma_put(R_l[1] + (8 * IT0 + -492), R[1] + (16 * IT1 + -499), (
         unsigned long)(unsigned int)4, (long)96, (long)2000, (unsigned long)(
         unsigned int)499, 1);
   }
   if (IT0 <= 61 && - IT0 + 2 * IT1 >= 1) {
      x86_dma_put(R_l[1] + 0, R[1] + (-8 * IT0 + (16 * IT1 + -7)), (
         unsigned long)(unsigned int)4, (long)96, (long)2000, (unsigned long)(
         unsigned int)499, 1);
   }
   x86_dma_put(Q_l[0] + (__maxs_32(8 * IT0 + -491, __maxs_32(8 * IT0 + -16 * IT1
      + 8, 1))), Q[0] + ((__maxs_32(8 * IT0 + -491, __maxs_32(8 * IT0 + -16 *
      IT1 + 8, 1))) + -8 * IT0 + (16 * IT1 + -7)), (unsigned long)(unsigned int
      )((__mins_32(__mins_32(-8 * IT0 + 515, -8 * IT0 + 16 * IT1 + 16),
      __mins_32(-16 * IT1 + 998, __mins_32(23, 8 * IT0 + -16 * IT1 + 506)))) * 4
      ), (long)96, (long)2000, (unsigned long)(unsigned int)500, 1);
   if (IT0 + -2 * IT1 >= 0) {
      x86_dma_put(Q_l[1] + (8 * IT0 + (-16 * IT1 + 7)), Q[1] + 0, (
         unsigned long)(unsigned int)4, (long)96, (long)2000, (unsigned long)(
         unsigned int)499, 1);
   }
   if (IT0 == 62 && IT1 >= 32) {
      x86_dma_put(Q_l[1] + (8 * IT0 + -492), Q[1] + (16 * IT1 + -499), (
         unsigned long)(unsigned int)4, (long)96, (long)2000, (unsigned long)(
         unsigned int)499, 1);
   }
   if (IT0 <= 61 && - IT0 + 2 * IT1 >= 1) {
      x86_dma_put(Q_l[1] + 0, Q[1] + (-8 * IT0 + (16 * IT1 + -7)), (
         unsigned long)(unsigned int)4, (long)96, (long)2000, (unsigned long)(
         unsigned int)499, 1);
   }
   x86_dma_wait(1);
   ocrDbDestroy(_t1);
   ocrDbDestroy(_t2);
   ocrDbDestroy(_t3);
   return NULL_GUID;
}

static ocrGuid_t ad_main(unsigned int paramc, unsigned long* paramv,
   unsigned int depc, ocrEdtDep_t* depv)
{
   union __args_ad_main_24 ad_main_args;
   union __args_ad1_22* allArgs;
   float (* R)[500];
   float (* P)[500];
   float (* Q)[500];
   int i;
   int i_1;
   for (ad_main_args = *(union __args_ad_main_24*)rocrArgs(depv),
        Q = ad_main_args.data.Q,
        P = ad_main_args.data.P,
        R = ad_main_args.data.R,
        i = 0;
        i <= 62; i++) {
      int _t1;
      int j;
      for (_t1 = (__mins_32(62, i + 63 >> 1)), j = (__maxs_32(i + -1 + 1 >> 1,
              0)); j <= _t1; j++) {
         ocrGuid_t _t2;
         union __args_ad1_22* _t3;
         _t2 = rocrAlloc((void**)&allArgs, 40ul);
         _t3 = allArgs;
         _t3->data.R = R;
         _t3->data.P = P;
         _t3->data.Q = Q;
         _t3->data.IT0 = i;
         _t3->data.IT1 = j;
         _t3->data.IT2 = 0;
         rocrDeclareTask(0, _t2, (unsigned int)(j + 63 * i));
      }
   }
   for (i_1 = 0; i_1 <= 62; i_1++) {
      int _t4;
      int j;
      for (_t4 = (__mins_32(- i_1 + 123, i_1 + 63 >> 1)), j = (__maxs_32(i_1 +
              -1 + 1 >> 1, 0)); j <= _t4; j++) {
         int _t5;
         int k;
         for (_t5 = (__mins_32(62, __mins_32(i_1 + 1, (2 * i_1 + 2 * j + 3) / 3
                 ))), k = (__maxs_32((2 * i_1 + 2 * j + -61 + 2) / 3, __maxs_32
                 (i_1 + j + -61, i_1))); k <= _t5; k++) {
            rocrDeclareDependence(0, (unsigned int)(j + 63 * i_1), 0, (
               unsigned int)(i_1 + j + (-1 * k + 1) + 63 * k));
         }
      }
   }
   rocrScheduleAll();
   return NULL_GUID;
}
