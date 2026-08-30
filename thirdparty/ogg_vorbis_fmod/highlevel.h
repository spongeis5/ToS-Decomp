/* highlevel.h -- RECONSTRUCTED, not upstream.
 *
 * The only difference from thirdparty/ogg_vorbis/vorbis/lib/highlevel.h is that
 * every `double` in it is a `float`.  16 substitution(s), no other change.
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

 function: highlevel encoder setup struct seperated out for vorbisenc clarity
 last mod: $Id: highlevel.h 13293 2007-07-24 00:09:47Z xiphmont $

 ********************************************************************/

typedef struct highlevel_byblocktype {
  float  tone_mask_setting;
  float  tone_peaklimit_setting;
  float  noise_bias_setting;
  float  noise_compand_setting;
} highlevel_byblocktype;
  
typedef struct highlevel_encode_setup {
  void *setup;
  int   set_in_stone;

  float  base_setting;
  float  long_setting;
  float  short_setting;
  float  impulse_noisetune;

  int    managed;
  long   bitrate_min;
  long   bitrate_av;
  float  bitrate_av_damp;
  long   bitrate_max;
  long   bitrate_reservoir;
  float  bitrate_reservoir_bias;
  
  int impulse_block_p;
  int noise_normalize_p;

  float  stereo_point_setting;
  float  lowpass_kHz;

  float  ath_floating_dB;
  float  ath_absolute_dB;

  float  amplitude_track_dBpersec;
  float  trigger_setting;
  
  highlevel_byblocktype block[4]; /* padding, impulse, transition, long */

} highlevel_encode_setup;

