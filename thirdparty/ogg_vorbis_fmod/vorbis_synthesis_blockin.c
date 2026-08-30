// sub_825BF448 -- vorbis_synthesis_blockin, FMOD's vendored libvorbis 1.2.0
// block.c. 2472 bytes, and it ENDS exactly where vorbis_synthesis_pcmout
// (825BFDF0, already matched) begins.
//
// NOTE FOR tools/sweep.py: this file needs the three vendored include
// directories, which sweep's fixed /O2 and /O2 /Os command lines do not
// carry, so a bare sweep will report it as "would not compile" rather than
// as a match. The manifest row's flags= column is what builds it.

/* The body is upstream (thirdparty/ogg_vorbis/vorbis/lib/block.c:696) and is
 * copied unchanged except for the one line marked FMOD below. Two things
 * about the image are not the vendored file:
 *
 *  1. THE STRUCT LAYOUT. `hs=ci->halfrate_flag` is read from 3532 and
 *     `b->sample_count` from 120; the pristine 1.2.0 headers put them at
 *     3656 and 128, and so do 1.2.2's, so the release is not what moves
 *     them. The reconstructed bitrate.h and highlevel.h beside this file
 *     carry the measurement and the arithmetic. Nothing here forces an
 *     offset -- the headers express it.
 *
 *  2. A `(long)` CAST ON THE SHORT-PAGE TRIM, and nothing else. Retail
 *     narrows `b->sample_count - v->granulepos` to 32 bits -- `extsw` both
 *     operands, one `subf` before the branch, then `sraw` in each arm
 *     (825BFD00..825BFD2C). Upstream's uncast expression stays 64-bit and
 *     MSVC emits `srad`. Adding the cast at both uses, and NOT naming a
 *     local for it, is 611 of 611.
 *
 *     The distinction matters and cost the last four bytes. Hoisting the
 *     value into `long extra=...` -- which is the shape upstream itself
 *     uses fifteen lines further down, so it looks like the obvious
 *     reading -- gives the right `sraw` but lets MSVC lift the
 *     `v->pcm_current` load above the `if(vb->eofflag)` into the dominator
 *     block, where retail loads it separately in each arm: 546 of 610 and
 *     2468 bytes against 2472. Leaving the expression spelled out at both
 *     uses keeps the two arms independent and the load duplicated. The
 *     four `fmuls` operand-order flips at 825BF6CC / 825BF79C / 825BF97C /
 *     825BFB50 were downstream of this one instruction and disappeared
 *     with it -- they were never an independent difference.
 *
 * NOT compiled as C++, unlike mdct_init.cpp and mdct_clear.cpp beside it.
 * That is forced rather than chosen: codec_internal.h pulls in psy.h and
 * so backends.h, whose `vorbis_func_residue` has a function-pointer member
 * literally named `class` (backends.h:95), and C++ will not parse it. mdct.c
 * includes none of that chain, which is why it could be C++ there. Whether
 * FMOD renamed the member and built this file as C++ too is NOT_MEASURED --
 * nothing in these 2472 bytes distinguishes the two.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include "vorbis/codec.h"
#include "codec_internal.h"
#include "window.h"
#include "mdct.h"
#include "lpc.h"
#include "registry.h"
#include "misc.h"

int vorbis_synthesis_blockin(vorbis_dsp_state *v,vorbis_block *vb){
  vorbis_info *vi=v->vi;
  codec_setup_info *ci=vi->codec_setup;
  private_state *b=v->backend_state;
  int hs=ci->halfrate_flag;
  int i,j;

  if(!vb)return(OV_EINVAL);
  if(v->pcm_current>v->pcm_returned  && v->pcm_returned!=-1)return(OV_EINVAL);

  v->lW=v->W;
  v->W=vb->W;
  v->nW=-1;

  if((v->sequence==-1)||
     (v->sequence+1 != vb->sequence)){
    v->granulepos=-1; /* out of sequence; lose count */
    b->sample_count=-1;
  }

  v->sequence=vb->sequence;

  if(vb->pcm){  /* no pcm to process if vorbis_synthesis_trackonly
		   was called on block */
    int n=ci->blocksizes[v->W]>>(hs+1);
    int n0=ci->blocksizes[0]>>(hs+1);
    int n1=ci->blocksizes[1]>>(hs+1);

    int thisCenter;
    int prevCenter;

    v->glue_bits+=vb->glue_bits;
    v->time_bits+=vb->time_bits;
    v->floor_bits+=vb->floor_bits;
    v->res_bits+=vb->res_bits;

    if(v->centerW){
      thisCenter=n1;
      prevCenter=0;
    }else{
      thisCenter=0;
      prevCenter=n1;
    }

    /* v->pcm is now used like a two-stage double buffer.  We don't want
       to have to constantly shift *or* adjust memory usage.  Don't
       accept a new block until the old is shifted out */

    for(j=0;j<vi->channels;j++){
      /* the overlap/add section */
      if(v->lW){
	if(v->W){
	  /* large/large */
	  float *w=_vorbis_window_get(b->window[1]-hs);
	  float *pcm=v->pcm[j]+prevCenter;
	  float *p=vb->pcm[j];
	  for(i=0;i<n1;i++)
	    pcm[i]=pcm[i]*w[n1-i-1] + p[i]*w[i];
	}else{
	  /* large/small */
	  float *w=_vorbis_window_get(b->window[0]-hs);
	  float *pcm=v->pcm[j]+prevCenter+n1/2-n0/2;
	  float *p=vb->pcm[j];
	  for(i=0;i<n0;i++)
	    pcm[i]=pcm[i]*w[n0-i-1] +p[i]*w[i];
	}
      }else{
	if(v->W){
	  /* small/large */
	  float *w=_vorbis_window_get(b->window[0]-hs);
	  float *pcm=v->pcm[j]+prevCenter;
	  float *p=vb->pcm[j]+n1/2-n0/2;
	  for(i=0;i<n0;i++)
	    pcm[i]=pcm[i]*w[n0-i-1] +p[i]*w[i];
	  for(;i<n1/2+n0/2;i++)
	    pcm[i]=p[i];
	}else{
	  /* small/small */
	  float *w=_vorbis_window_get(b->window[0]-hs);
	  float *pcm=v->pcm[j]+prevCenter;
	  float *p=vb->pcm[j];
	  for(i=0;i<n0;i++)
	    pcm[i]=pcm[i]*w[n0-i-1] +p[i]*w[i];
	}
      }

      /* the copy section */
      {
	float *pcm=v->pcm[j]+thisCenter;
	float *p=vb->pcm[j]+n;
	for(i=0;i<n;i++)
	  pcm[i]=p[i];
      }
    }

    if(v->centerW)
      v->centerW=0;
    else
      v->centerW=n1;

    /* deal with initial packet state; we do this using the explicit
       pcm_returned==-1 flag otherwise we're sensitive to first block
       being short or long */

    if(v->pcm_returned==-1){
      v->pcm_returned=thisCenter;
      v->pcm_current=thisCenter;
    }else{
      v->pcm_returned=prevCenter;
      v->pcm_current=prevCenter+
	((ci->blocksizes[v->lW]/4+
	ci->blocksizes[v->W]/4)>>hs);
    }

  }

  /* track the frame number... This is for convenience, but also
     making sure our last packet doesn't end with added padding.  If
     the last packet is partial, the number of samples we'll have to
     return will be past the vb->granulepos.

     This is not foolproof!  It will be confused if we begin
     decoding at the last page after a seek or hole.  In that case,
     we don't have a starting point to judge where the last frame
     is.  For this reason, vorbisfile will always try to make sure
     it reads the last two marked pages in proper sequence */

  if(b->sample_count==-1){
    b->sample_count=0;
  }else{
    b->sample_count+=ci->blocksizes[v->lW]/4+ci->blocksizes[v->W]/4;
  }

  if(v->granulepos==-1){
    if(vb->granulepos!=-1){ /* only set if we have a position to set to */

      v->granulepos=vb->granulepos;

      /* is this a short page? */
      if(b->sample_count>v->granulepos){
	/* corner case; if this is both the first and last audio page,
	   then spec says the end is cut, not beginning */
	if(vb->eofflag){
	  /* trim the end */
	  /* no preceeding granulepos; assume we started at zero (we'd
	     have to in a short single-page stream) */
	  /* granulepos could be -1 due to a seek, but that would result
	     in a long count, not short count */

	  v->pcm_current-=(long)(b->sample_count-v->granulepos)>>hs; /* FMOD */
	}else{
	  /* trim the beginning */
	  v->pcm_returned+=(long)(b->sample_count-v->granulepos)>>hs; /* FMOD */
	  if(v->pcm_returned>v->pcm_current)
	    v->pcm_returned=v->pcm_current;
	}

      }

    }
  }else{
    v->granulepos+=ci->blocksizes[v->lW]/4+ci->blocksizes[v->W]/4;
    if(vb->granulepos!=-1 && v->granulepos!=vb->granulepos){

      if(v->granulepos>vb->granulepos){
	long extra=v->granulepos-vb->granulepos;

	if(extra)
	  if(vb->eofflag){
	    /* partial last frame.  Strip the extra samples off */
	    v->pcm_current-=extra>>hs;
	  } /* else {Shouldn't happen *unless* the bitstream is out of
	       spec.  Either way, believe the bitstream } */
      } /* else {Shouldn't happen *unless* the bitstream is out of
	   spec.  Either way, believe the bitstream } */
      v->granulepos=vb->granulepos;
    }
  }

  /* Update, cleanup */

  if(vb->eofflag)v->eofflag=1;
  return(0);

}
