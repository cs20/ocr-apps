/* Auto-generated by R-Stream without advanced optimizations */

#include <rstream_ocr.h>
#include <ocr.h>
struct _IO_FILE;
union ocrGuidUnion_t;
struct ocrEdtDepStruct_t;
struct __anon_21;
union __args_imp1_22;
union ocrGuidUnion_t;
struct ocrEdtDepStruct_t;
struct __anon_23;
union __args_imp_main_24;
union ocrGuidUnion_t;
struct _IO_marker;
struct _IO_FILE {
   int _flags;
   char* _IO_read_ptr;
   char* _IO_read_end;
   char* _IO_read_base;
   char* _IO_write_base;
   char* _IO_write_ptr;
   char* _IO_write_end;
   char* _IO_buf_base;
   char* _IO_buf_end;
   char* _IO_save_base;
   char* _IO_backup_base;
   char* _IO_save_end;
   struct _IO_marker* _markers;
   struct _IO_FILE* _chain;
   int _fileno;
   int _flags2;
   long _old_offset;
   unsigned short _cur_column;
   signed char _vtable_offset;
   char _shortbuf[1];
   void* _lock;
   long _offset;
   void* __pad1;
   void* __pad2;
   void* __pad3;
   void* __pad4;
   unsigned long __pad5;
   int _mode;
   char _unused2[20];
};
struct __anon_21 {
   float* B;
   float* A;
   int IT0;
   int IT1;
};
union __args_imp1_22 {
   struct __anon_21 data;
   double padding[3];
};
struct __anon_23 {
   float* B;
   float* A;
};
union __args_imp_main_24 {
   struct __anon_23 data;
   double padding[2];
};
struct _IO_marker {
   struct _IO_marker* _next;
   struct _IO_FILE* _sbuf;
   int _pos;
};
/*
 * Forward declarations of functions
 */
int check_vectors(char const*, char const*, void const*, void const*, int);
int fprintf(struct _IO_FILE*, char const*, ...);
void initialize(void);
void initialize_once(void);
void show(void);
void kernel(void);
int check(void);
double flops_per_trial(void);
void imp(float* A, float* B);
static ocrGuid_t imp1(unsigned int paramc, unsigned long* paramv, unsigned int 
   depc, ocrEdtDep_t* depv);
void* rocrArgs(ocrEdtDep_t*);
void x86_dma_get(void const volatile*, void volatile*, unsigned long, long, 
   long, unsigned long, int);
void x86_dma_wait(int);
void x86_dma_put(void const volatile*, void volatile*, unsigned long, long, 
   long, unsigned long, int);
static ocrGuid_t imp_main(unsigned int paramc, unsigned long* paramv, 
   unsigned int depc, ocrEdtDep_t* depv);
void rocrExit(void);
/*
 * Forward declarations of global variables
 */
extern int add_flop;
extern int mul_flop;
extern struct _IO_FILE* stderr;
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
extern struct _IO_FILE* stderr;
static float a[5000];
static float b[5000];
static float aa[5000];
static float bb[5000];
int nb_samples = 25000000;
char const* function_name = "one_d_imperfect_jacobi";
/*
 * Definitions of functions
 */
/*
 * ../src/one_d_imperfect_jacobi.c:54.6
 */
void initialize(void)
{
   int i;
   for (i = 0; i < 5000; i++) {
      float _t1;
      _t1 = (float)(i / 2);
      bb[i] = _t1;
      b[i] = _t1;
      aa[i] = _t1;
      a[i] = _t1;
   }
}

/*
 * ../src/one_d_imperfect_jacobi.c:62.6
 */
void initialize_once(void)
{
   initialize();
}

/*
 * ../src/one_d_imperfect_jacobi.c:66.6
 */
void show(void)
{
   int i;
   int i_1;
   for (i = 0; i < 8; i++) {
      fprintf(stderr, "e[%d] = %10.10f\n", i, (double)a[i]);
   }
   for (i_1 = 4992; i_1 < 5000; i_1++) {
      fprintf(stderr, "e[%d] = %10.10f\n", i_1, (double)a[i_1]);
   }
}

/*
 * ../src/one_d_imperfect_jacobi.c:83.6
 */
void kernel(void)
{
   union __args_imp_main_24* allArgs;
   ocrGuid_t _t1;
   union __args_imp_main_24* _t2;
   rocrDeclareType(imp_main, 1, (unsigned int)1, 1);
   rocrDeclareType(imp1, 0, (unsigned int)2360, 0);
   rocrInit();
   _t1 = rocrAlloc((void**)&allArgs, 16ul);
   _t2 = allArgs;
   _t2->data.B = b;
   _t2->data.A = a;
   rocrExecute(1, _t1);
   rocrExit();
}

/*
 * ../src/one_d_imperfect_jacobi.c:87.5
 */
int check(void)
{
   int t;
   for (t = 0; t < 5000; t++) {
      int i;
      int i_1;
      int j;
      for (i_1 = 2, i = 1; i_1 < 4999; ) {
         float _t1;
         float _t2;
         int _t3;
         _t1 = aa[i];
         _t2 = aa[i_1];
         _t3 = i_1 + 1;
         bb[i_1] = 0.3330000043f * (_t1 + _t2 + aa[_t3]);
         i++;
         i_1 = _t3;
      }
      for (j = 2; j < 4999; j++) {
         aa[j] = bb[j];
      }
   }
   return check_vectors("a", "aa", a, aa, 5000);
}

/*
 * ../src/one_d_imperfect_jacobi.c:94.8
 */
double flops_per_trial(void)
{
   return (double)(mul_flop + add_flop) * 2.0 * 5000.0 * 5000.0;
}

void imp(float* A, float* B)
{
   union __args_imp_main_24* allArgs;
   ocrGuid_t _t1;
   union __args_imp_main_24* _t2;
   rocrDeclareType(imp_main, 1, (unsigned int)1, 1);
   rocrDeclareType(imp1, 0, (unsigned int)2360, 0);
   rocrInit();
   _t1 = rocrAlloc((void**)&allArgs, 16ul);
   _t2 = allArgs;
   _t2->data.B = B;
   _t2->data.A = A;
   rocrExecute(1, _t1);
   rocrExit();
}

static ocrGuid_t imp1(unsigned int paramc, unsigned long* paramv, unsigned int 
   depc, ocrEdtDep_t* depv)
{
   union __args_imp1_22 imp1_args;
   float* B_l;
   float* A_l;
   float* B;
   float* A;
   int IT0;
   int IT1;
   ocrGuid_t _t1;
   ocrGuid_t _t2;
   int _t3;
   int i;
   imp1_args = *(union __args_imp1_22*)rocrArgs(depv);
   _t1 = rocrAlloc((void**)&B_l, 2044ul);
   _t2 = rocrAlloc((void**)&A_l, 2048ul);
   IT1 = imp1_args.data.IT1;
   IT0 = imp1_args.data.IT0;
   B = imp1_args.data.B;
   x86_dma_get(B + ((__maxs_32(256 * IT0 + -9744, __maxs_32(256 * IT0 + -256 *
      IT1 + 255, 0))) + -256 * IT0 + (256 * IT1 + -253)), B_l + (__maxs_32(256 *
      IT0 + -9744, __maxs_32(256 * IT0 + -256 * IT1 + 255, 0))), (unsigned long
      )(unsigned int)((__mins_32(__mins_32(-256 * IT0 + 10254, -256 * IT0 + 256
      * IT1 + 255), __mins_32(-256 * IT1 + 14996, __mins_32(510, 256 * IT0 +
      -256 * IT1 + 5252)))) * 4), (long)0, (long)0, (unsigned long)(
      unsigned int)1, 0);
   A = imp1_args.data.A;
   x86_dma_get(A + ((__maxs_32(256 * IT0 + -9744, __maxs_32(256 * IT0 + -256 *
      IT1 + 254, 0))) + -256 * IT0 + (256 * IT1 + -253)), A_l + (__maxs_32(256 *
      IT0 + -9744, __maxs_32(256 * IT0 + -256 * IT1 + 254, 0))), (unsigned long
      )(unsigned int)((__mins_32(__mins_32(-256 * IT0 + 10256, -256 * IT0 + 256
      * IT1 + 258), __mins_32(-256 * IT1 + 14997, __mins_32(512, 256 * IT0 +
      -256 * IT1 + 5253)))) * 4), (long)0, (long)0, (unsigned long)(
      unsigned int)1, 0);
   _t3 = (__mins_32(-128 * IT0 + 4999, 127));
   x86_dma_wait(0);
   for (i = (__maxs_32(0, -128 * IT0 + 128 * IT1 + -2498)); i <= _t3; i++) {
      int _t4;
      int j;
      if (i + 128 * IT0 + -128 * IT1 >= 0) {
         B_l[256 * IT0 + (-256 * IT1 + 255)] = 0.3330000043f * (A_l[256 * IT0 +
            (-256 * IT1 + 254)] + A_l[256 * IT0 + (-256 * IT1 + 255)] + A_l[256
            * IT0 + (-256 * IT1 + 256)]);
      }
      for (_t4 = (__mins_32(255, 2 * i + 256 * IT0 + -256 * IT1 + 4996)), j = (
              __maxs_32(2 * i + 256 * IT0 + -256 * IT1 + 1, 0)); j <= _t4; j++)
          {
         B_l[-2 * i + (j + 255)] = 0.3330000043f * (A_l[-2 * i + (j + 254)] +
            A_l[-2 * i + (j + 255)] + A_l[-2 * i + (j + 256)]);
         A_l[-2 * i + (j + 254)] = B_l[-2 * i + (j + 254)];
      }
      if (- i + -128 * IT0 + 128 * IT1 >= 2371) {
         A_l[256 * IT0 + (-256 * IT1 + 5251)] = B_l[256 * IT0 + (-256 * IT1 +
            5251)];
      }
   }
   x86_dma_put(B_l + (__maxs_32(256 * IT0 + -9743, __maxs_32(256 * IT0 + -256 *
      IT1 + 255, 1))), B + ((__maxs_32(256 * IT0 + -9743, __maxs_32(256 * IT0 +
      -256 * IT1 + 255, 1))) + -256 * IT0 + (256 * IT1 + -253)), (unsigned long
      )(unsigned int)((__mins_32(__mins_32(-256 * IT0 + 10254, -256 * IT0 + 256
      * IT1 + 256), __mins_32(-256 * IT1 + 14995, __mins_32(510, 256 * IT0 +
      -256 * IT1 + 5251)))) * 4), (long)0, (long)0, (unsigned long)(
      unsigned int)1, 1);
   x86_dma_put(A_l + (__maxs_32(256 * IT0 + -9744, __maxs_32(256 * IT0 + -256 *
      IT1 + 255, 0))), A + ((__maxs_32(256 * IT0 + -9744, __maxs_32(256 * IT0 +
      -256 * IT1 + 255, 0))) + -256 * IT0 + (256 * IT1 + -253)), (unsigned long
      )(unsigned int)((__mins_32(__mins_32(-256 * IT0 + 10254, -256 * IT0 + 256
      * IT1 + 255), __mins_32(-256 * IT1 + 14996, __mins_32(510, 256 * IT0 +
      -256 * IT1 + 5252)))) * 4), (long)0, (long)0, (unsigned long)(
      unsigned int)1, 1);
   x86_dma_wait(1);
   ocrDbDestroy(_t1);
   ocrDbDestroy(_t2);
   return NULL_GUID;
}

static ocrGuid_t imp_main(unsigned int paramc, unsigned long* paramv, 
   unsigned int depc, ocrEdtDep_t* depv)
{
   union __args_imp_main_24 imp_main_args;
   union __args_imp1_22* allArgs;
   float* B;
   float* A;
   int i;
   int i_1;
   for (imp_main_args = *(union __args_imp_main_24*)rocrArgs(depv),
        A = imp_main_args.data.A,
        B = imp_main_args.data.B,
        i = 0;
        i <= 39; i++) {
      int _t1;
      int j;
      for (_t1 = (__mins_32(58, i + 20)), j = i; j <= _t1; j++) {
         ocrGuid_t _t2;
         union __args_imp1_22* _t3;
         _t2 = rocrAlloc((void**)&allArgs, 24ul);
         _t3 = allArgs;
         _t3->data.B = B;
         _t3->data.A = A;
         _t3->data.IT0 = i;
         _t3->data.IT1 = j;
         rocrDeclareTask(0, _t2, (unsigned int)(j + 59 * i));
      }
   }
   for (i_1 = 0; i_1 <= 39; i_1++) {
      int _t4;
      int j;
      for (_t4 = (__mins_32(- i_1 + 96, i_1 + 20)), j = i_1; j <= _t4; j++) {
         int _t5;
         int k;
         for (_t5 = (__mins_32(39, __mins_32(i_1 + 1, i_1 + j + 1 >> 1))), k = (
                 __maxs_32(i_1 + j + -57, __maxs_32(i_1 + j + -19 + 1 >> 1, i_1
                 ))); k <= _t5; k++) {
            rocrDeclareDependence(0, (unsigned int)(j + 59 * i_1), 0, (
               unsigned int)(i_1 + j + (-1 * k + 1) + 59 * k));
         }
      }
   }
   rocrScheduleAll();
   return NULL_GUID;
}

