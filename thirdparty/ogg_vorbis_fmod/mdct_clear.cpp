// sub_825C27F8 -- mdct_clear, FMOD's vendored libvorbis 1.2.0 mdct.c
//
// NOTE FOR tools/sweep.py: this file needs the three vendored include
// directories, which sweep's fixed /O2 and /O2 /Os command lines do not
// carry, so a bare sweep will report it as "would not compile" rather than
// as a match. The manifest row's flags= column is what builds it. Verified
// with tools/match.py and with tools/build.py's own relocation-resolved
// comparison; both accept it at 128 of 128 bytes.

/* mdct_clear -- 825C27F8, from FMOD's vendored libvorbis 1.2.0 mdct.c.
 *
 * The body is upstream (thirdparty/ogg_vorbis/vorbis/lib/mdct.c:339).  Three
 * things in the image are NOT upstream and are reconstructed here:
 *
 *  1. an unused leading parameter.  Retail keeps `l` in r4 and never reads
 *     r3 -- `mr r30,r4` / `cmplwi cr6,r4,0` / `lwz r3,8(r4)`.  mdct_backward
 *     (825C29B8) and mdct_butterflies (825C2748) both take their first
 *     declared argument in r3, so this is a real extra parameter and not a
 *     whole-file calling-convention difference.  The bytes say it EXISTS and
 *     is UNUSED; they say nothing about its type or name.
 *
 *  2. _ogg_free is a three-argument allocator call, `free(ptr, file, line)`,
 *     not the one-argument `free` that ogg/include/ogg/os_types.h defines.
 *     The retail target is 8252D9C8, which shuffles r3/r4/r5 up one and tail
 *     calls the FMOD heap.
 *
 *  3. the __LINE__ arguments are 348 and 349, seven more than the vendored
 *     file's 341 and 342.  mdct_init's two allocations ARE at the vendored
 *     lines 53 and 54, so the seven lines were inserted between -- which is
 *     what modifying mdct_init (an int return and two null guards) costs.
 *     #line reproduces the count without touching the vendored tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vorbis/codec.h"
#include "mdct.h"
#include "os.h"
#include "misc.h"

extern "C" void ogg_mem_free(void *ptr, const char *file, long line);

#undef _ogg_free
#define _ogg_free(x) ogg_mem_free((x), __FILE__, __LINE__)

/* THE IMAGE HAS TWO memset IMPLEMENTATIONS AND THEY ARE NOT THUNKS.
 * 82301150 opens `mflr r12 ; bl 828A75CC`; 828A8C50 is a compact byte loop
 * (`addi r0,r5,1 ; mtctr ; ori`). FMOD's code calls the first, the game's
 * own code calls the second, and both are reached by `bl` from matched
 * functions -- so one C name for both makes build.py report
 * WOULD NOT LINK: one symbol, two addresses. The bytes verify either way,
 * because each call site is patched from the retail word independently;
 * it is the SOURCE MODEL that would be wrong, and a real link would refuse
 * it.
 *
 * So it is named for the address it is, which is also what the project does
 * for `g_arena_829A195C`. `#pragma function` is no longer needed: MSVC
 * expands `memset` inline at /O2 unless told otherwise, but it has no
 * reason to expand a function it has never heard of, and the emitted bytes
 * are identical -- verified with match.py after the rename. */
extern "C" void *memset_82301150(void *dst, int c, unsigned int n);

/* The filename on the #line is the image's own: the string at 820629A0 that
 * both frees pass is "..\lib\ogg_vorbis\vorbis\lib\mdct.c", FMOD's __FILE__.
 * It costs nothing -- the reference is a REFHI/REFLO pair that build.py
 * resolves out of the retail word either way -- but it keeps the compiling
 * machine's own path out of the object. */
#line 346 "..\\lib\\ogg_vorbis\\vorbis\\lib\\mdct.c"
void mdct_clear(void *owner, mdct_lookup *l){
  if(l){
    if(l->trig)_ogg_free(l->trig);
    if(l->bitrev)_ogg_free(l->bitrev);
    memset_82301150(l,0,sizeof(*l));
  }
}
