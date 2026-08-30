/* bitrate.h -- RECONSTRUCTED, not upstream.
 *
 * The only difference from thirdparty/ogg_vorbis/vorbis/lib/bitrate.h is that
 * every `double` in it is a `float`.  3 substitution(s), no other change.
 *
 * WHY, and it is a measurement rather than a preference.  Two offsets in the
 * retail image are read directly out of already-identified code:
 *
 *   codec_setup_info.halfrate_flag  at 3532   (825BF470, vorbis_synthesis_blockin)
 *   private_state.sample_count      at 120    (825BF4D4 / 825BFC84 / 825BFCF0)
 *
 * Compiled against the pristine 1.2.0 headers with the title's own cl.exe
 * those land at 3656 and 128 -- and identically at 3656 and 128 against
 * libvorbis 1.2.2, so the release is not what moves them.  Both members sit
 * at the END of their struct, behind encoder-only sub-structures the decoder
 * never touches: `bitrate_manager_state` for one, `bitrate_manager_info`
 * and `highlevel_encode_setup` for the other.
 *
 * Dropping every double to a float in these two headers gives
 *
 *   sizeof(bitrate_manager_state)   48 -> 40   =>  sample_count   at 120
 *   sizeof(bitrate_manager_info)    32 -> 24   =>  hi             at 3384
 *   sizeof(highlevel_encode_setup) 264 -> 148  =>  halfrate_flag  at 3532
 *
 * -- both target numbers exactly, from one hypothesis with no free
 * parameter.  Fitting one offset would prove nothing; hitting two
 * independent ones is why this is written down as a finding.
 *
 * WHAT THE BYTES DO NOT SAY: only the two offsets are measured.  Any other
 * layout summing the same way would be indistinguishable here, and no
 * decoder-side code in the image reads a field of either struct, so nothing
 * else constrains them.
 */
/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2007             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: bitrate tracking and management
 last mod: $Id: bitrate.h 13293 2007-07-24 00:09:47Z xiphmont $

 ********************************************************************/

#ifndef _V_BITRATE_H_
#define _V_BITRATE_H_

#include "vorbis/codec.h"
#include "codec_internal.h"
#include "os.h"

/* encode side bitrate tracking */
typedef struct bitrate_manager_state {
  int            managed;

  long           avg_reservoir;
  long           minmax_reservoir;
  long           avg_bitsper;
  long           min_bitsper;
  long           max_bitsper;

  long           short_per_long;
  float          avgfloat;

  vorbis_block  *vb;
  int            choice;
} bitrate_manager_state;

typedef struct bitrate_manager_info{
  long           avg_rate;
  long           min_rate;
  long           max_rate;
  long           reservoir_bits;
  float          reservoir_bias;

  float          slew_damp;

} bitrate_manager_info;

extern void vorbis_bitrate_init(vorbis_info *vi,bitrate_manager_state *bs);
extern void vorbis_bitrate_clear(bitrate_manager_state *bs);
extern int vorbis_bitrate_managed(vorbis_block *vb);
extern int vorbis_bitrate_addblock(vorbis_block *vb);
extern int vorbis_bitrate_flushpacket(vorbis_dsp_state *vd, ogg_packet *op);

#endif
