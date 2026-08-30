// sub_825C1D28 -- mdct_init, FMOD's vendored libvorbis 1.2.0 mdct.c
//
// NOTE FOR tools/sweep.py: this file needs the three vendored include
// directories, which sweep's fixed /O2 and /O2 /Os command lines do not
// carry, so a bare sweep will report it as "would not compile" rather than
// as a match. The manifest row's flags= column is what builds it. Verified
// with tools/match.py and with tools/build.py's own relocation-resolved
// comparison; both accept it at 712 of 712 bytes.

/* mdct_init -- 825C1D28, from FMOD's vendored libvorbis 1.2.0 mdct.c.
 *
 * The body is upstream (thirdparty/ogg_vorbis/vorbis/lib/mdct.c:52) and is
 * copied unchanged.  Four things about the image are NOT the vendored file:
 *
 *  1. It is compiled as C++.  That is what puts a `frsp` after each libm
 *     call and makes the log2n line single precision.  MSVC's math.h
 *     declares `inline float log(float)` as `(float)log((double)x)` in its
 *     __cplusplus block, so `log((float)n)/log(2.f)` emits
 *         bl log ; frsp ; lfd 2.0 ; bl log ; frsp ; fdivs ; fadds ;
 *         bl floor ; frsp ; fctiwz
 *     which is exactly 825C1D98..825C1DD0.  Compiled as C the same line is
 *     all double -- one `fdiv`, no `frsp` -- which is what the vendored .c
 *     produces and why it scores 3 of 132.  Four independent signatures
 *     agree (both logs, the divide, the floor).
 *
 *  2. An unused leading parameter.  `lookup` arrives in r4 and `n` in r5;
 *     r3 is never read before `rlwinm r3,r29,2,0,29` overwrites it.
 *     mdct_backward (825C29B8) and mdct_butterflies (825C2748) both take
 *     their first declared argument in r3, so this is a parameter and not a
 *     whole-file convention.  The bytes say it EXISTS and is UNUSED; they do
 *     not say what it is.
 *
 *  3. _ogg_malloc is a three-argument call, `alloc(bytes, file, line)`
 *     (8252D950), not the one-argument `malloc` os_types.h defines.  The
 *     __LINE__ arguments are 53 and 54 -- the vendored file's own line
 *     numbers for these two allocations, which is what dates the edits
 *     below them.
 *
 *  4. It returns int, and null-checks both allocations after log2n has been
 *     computed and stored.  Both guards branch forward to one shared
 *     `li r3,-139` planted after the success epilogue, so the failure path
 *     is written last.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vorbis/codec.h"
#include "mdct.h"
#include "os.h"
#include "misc.h"

extern "C" void *ogg_mem_alloc(long bytes, const char *file, long line);

#undef _ogg_malloc
#define _ogg_malloc(x) ogg_mem_alloc((x), __FILE__, __LINE__)

/* The filename on the #line is the image's own: the string at 820629A0 that
 * both allocations pass is "..\lib\ogg_vorbis\vorbis\lib\mdct.c", FMOD's
 * __FILE__.  It costs nothing -- the reference is a REFHI/REFLO pair that
 * build.py resolves out of the retail word either way -- but it keeps the
 * compiling machine's own path out of the object. */
#line 52 "..\\lib\\ogg_vorbis\\vorbis\\lib\\mdct.c"
int mdct_init(void *owner, mdct_lookup *lookup,int n){
  int   *bitrev=(int *)_ogg_malloc(sizeof(*bitrev)*(n/4));
  DATA_TYPE *T=(DATA_TYPE *)_ogg_malloc(sizeof(*T)*(n+n/4));

  int i;
  int n2=n>>1;
  int log2n=lookup->log2n=rint(log((float)n)/log(2.f));
  if(bitrev==NULL || T==NULL)
    return -139;
  lookup->n=n;
  lookup->trig=T;
  lookup->bitrev=bitrev;

/* trig lookups... */

  for(i=0;i<n/4;i++){
    T[i*2]=FLOAT_CONV(cos((M_PI/n)*(4*i)));
    T[i*2+1]=FLOAT_CONV(-sin((M_PI/n)*(4*i)));
    T[n2+i*2]=FLOAT_CONV(cos((M_PI/(2*n))*(2*i+1)));
    T[n2+i*2+1]=FLOAT_CONV(sin((M_PI/(2*n))*(2*i+1)));
  }
  for(i=0;i<n/8;i++){
    T[n+i*2]=FLOAT_CONV(cos((M_PI/n)*(4*i+2))*.5);
    T[n+i*2+1]=FLOAT_CONV(-sin((M_PI/n)*(4*i+2))*.5);
  }

  /* bitreverse lookup... */

  {
    int mask=(1<<(log2n-1))-1,i,j;
    int msb=1<<(log2n-2);
    for(i=0;i<n/8;i++){
      int acc=0;
      for(j=0;msb>>j;j++)
	if((msb>>j)&i)acc|=1<<j;
      bitrev[i*2]=((~acc)&mask)-1;
      bitrev[i*2+1]=acc;

    }
  }
  lookup->scale=FLOAT_CONV(4.f/n);
  return 0;
}
