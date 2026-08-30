// sub_825BDFB0 -- ogg_stream_pagein, FMOD's vendored libogg 1.1.3 framing.c.
// 1064 bytes, and it ENDS exactly where 825BE3D8 begins.
//
// NOTE FOR tools/sweep.py: this file needs the vendored include directories
// AND thirdparty/ogg_vorbis_fmod ahead of them, which sweep's fixed /O2 and
// /O2 /Os command lines do not carry, so a bare sweep will report it as
// "would not compile" rather than as a match. The manifest row's flags=
// column is what builds it -- note the LEADING /Ithirdparty/ogg_vorbis_fmod,
// which is what makes the reconstructed ogg/ogg.h beside this file win the
// angle-bracket search for <ogg/ogg.h>.

/* The body is upstream (thirdparty/ogg_vorbis/ogg/src/framing.c:673) and is
 * copied unchanged. Four things about the image are NOT the vendored file,
 * and three of them are the standard FMOD set already recorded in the
 * README beside this file:
 *
 *  1. AN UNUSED LEADING PARAMETER. `os` arrives in r4 and `og` in r5; r3 is
 *     never read before `lbz r3,17(r30)` overwrites it at 825BDFEC. This is
 *     the predicted case -- the function reallocates, and the README's rule
 *     says a libogg/libvorbis function that transitively allocates carries
 *     one extra leading argument. Predicted before it was compiled, not
 *     fitted afterwards.
 *
 *  2. _ogg_realloc IS A FOUR-ARGUMENT CALL, `realloc(ptr, bytes, file,
 *     line)` at 8252D9A0 -- r3=ptr, r4=bytes, r5=file, r6=line at
 *     825BE168..825BE180 -- not the two-argument `realloc` that
 *     ogg/include/ogg/os_types.h defines.
 *
 *  3. THE __LINE__ IMMEDIATES ARE NOT RELOCATED, so they had to be
 *     reproduced exactly: 222 for _os_body_expand's realloc and 229/230 for
 *     _os_lacing_expand's two. The vendored file has those same three
 *     reallocs at 231, 238 and 239 -- NINE more, and the same nine for all
 *     three, so nine lines were removed above line 228 rather than the
 *     helpers being rewritten. #line reproduces the count without touching
 *     the pristine tree, and sets __FILE__ to the image's own path (the
 *     string at 82062840, "..\lib\ogg_vorbis\ogg\src\framing.c") rather
 *     than this machine's build path, which is what tools/test_privacy.py
 *     requires.
 *
 *  4. TWO MEMBER TYPES IN ogg_stream_state. `lacing_vals` is 16-bit and
 *     `granule_vals` is 32-bit, against upstream's `int *` and
 *     `ogg_int64_t *`. That is read off the element scaling and the access
 *     width -- see the banner on ogg/ogg.h beside this file, which carries
 *     the addresses. Every struct OFFSET is unchanged, because only the
 *     pointee types move.
 *
 * The seven ogg_page_* accessors are `static` here where framing.c has them
 * extern. They are inlined into this function either way -- the disassembly
 * reads header[4], header[5], header[6..13], header[14..17], header[18..21]
 * and header[26] directly -- and `static` only stops an unused out-of-line
 * copy being emitted into an object that already has its own manifest rows
 * for ogg_page_continued, ogg_page_eos and ogg_page_granulepos elsewhere.
 */

#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>

extern void *ogg_mem_realloc(void *ptr, long bytes, const char *file,
			     long line);

#undef _ogg_realloc
#define _ogg_realloc(p,x) ogg_mem_realloc((p),(x),__FILE__,__LINE__)

static int ogg_page_version(ogg_page *og){
  return((int)(og->header[4]));
}

static int ogg_page_continued(ogg_page *og){
  return((int)(og->header[5]&0x01));
}

static int ogg_page_bos(ogg_page *og){
  return((int)(og->header[5]&0x02));
}

static int ogg_page_eos(ogg_page *og){
  return((int)(og->header[5]&0x04));
}

static ogg_int64_t ogg_page_granulepos(ogg_page *og){
  unsigned char *page=og->header;
  ogg_int64_t granulepos=page[13]&(0xff);
  granulepos= (granulepos<<8)|(page[12]&0xff);
  granulepos= (granulepos<<8)|(page[11]&0xff);
  granulepos= (granulepos<<8)|(page[10]&0xff);
  granulepos= (granulepos<<8)|(page[9]&0xff);
  granulepos= (granulepos<<8)|(page[8]&0xff);
  granulepos= (granulepos<<8)|(page[7]&0xff);
  granulepos= (granulepos<<8)|(page[6]&0xff);
  return(granulepos);
}

static int ogg_page_serialno(ogg_page *og){
  return(og->header[14] |
	 (og->header[15]<<8) |
	 (og->header[16]<<16) |
	 (og->header[17]<<24));
}

static long ogg_page_pageno(ogg_page *og){
  return(og->header[18] |
	 (og->header[19]<<8) |
	 (og->header[20]<<16) |
	 (og->header[21]<<24));
}

/* The #line below puts _os_body_expand's realloc on 222 and
 * _os_lacing_expand's two on 229 and 230, which is what the image's
 * unrelocated __LINE__ immediates are. */
#line 219 "..\\lib\\ogg_vorbis\\ogg\\src\\framing.c"
static void _os_body_expand(ogg_stream_state *os,int needed){
  if(os->body_storage<=os->body_fill+needed){
    os->body_storage+=(needed+1024);
    os->body_data=_ogg_realloc(os->body_data,os->body_storage*sizeof(*os->body_data));
  }
}

static void _os_lacing_expand(ogg_stream_state *os,int needed){
  if(os->lacing_storage<=os->lacing_fill+needed){
    os->lacing_storage+=(needed+32);
    os->lacing_vals=_ogg_realloc(os->lacing_vals,os->lacing_storage*sizeof(*os->lacing_vals));
    os->granule_vals=_ogg_realloc(os->granule_vals,os->lacing_storage*sizeof(*os->granule_vals));
  }
}

#line 673 "..\\lib\\ogg_vorbis\\ogg\\src\\framing.c"
/* THE IMAGE HAS TWO memcpy IMPLEMENTATIONS AND THEY ARE NOT THUNKS.
 * 82300E58 is the one FMOD's code calls; 828A8CF0 is a dcbt-prefetching
 * copy the game's own code calls, and both are reached by `bl` from
 * already-matched functions. One C name for both makes build.py report
 * WOULD NOT LINK: one symbol, two addresses. Every byte still verifies
 * -- each call site is patched from the retail word independently --
 * but the SOURCE MODEL would be wrong, and a real link would refuse it.
 *
 * Named for the address it is, as this project already does for
 * g_arena_829A195C. The emitted bytes are identical to the plain
 * `memcpy` spelling; verified with match.py after the rename. */
extern void *memcpy_82300E58(void *dst, const void *src, unsigned int n);

int ogg_stream_pagein(void *owner, ogg_stream_state *os, ogg_page *og){
  unsigned char *header=og->header;
  unsigned char *body=og->body;
  long           bodysize=og->body_len;
  int            segptr=0;

  int version=ogg_page_version(og);
  int continued=ogg_page_continued(og);
  int bos=ogg_page_bos(og);
  int eos=ogg_page_eos(og);
  ogg_int64_t granulepos=ogg_page_granulepos(og);
  int serialno=ogg_page_serialno(og);
  long pageno=ogg_page_pageno(og);
  int segments=header[26];

  /* clean up 'returned data' */
  {
    long lr=os->lacing_returned;
    long br=os->body_returned;

    /* body data */
    if(br){
      os->body_fill-=br;
      if(os->body_fill)
	memmove(os->body_data,os->body_data+br,os->body_fill);
      os->body_returned=0;
    }

    if(lr){
      /* segment table */
      if(os->lacing_fill-lr){
	memmove(os->lacing_vals,os->lacing_vals+lr,
		(os->lacing_fill-lr)*sizeof(*os->lacing_vals));
	memmove(os->granule_vals,os->granule_vals+lr,
		(os->lacing_fill-lr)*sizeof(*os->granule_vals));
      }
      os->lacing_fill-=lr;
      os->lacing_packet-=lr;
      os->lacing_returned=0;
    }
  }

  /* check the serial number */
  if(serialno!=os->serialno)return(-1);
  if(version>0)return(-1);

  _os_lacing_expand(os,segments+1);

  /* are we in sequence? */
  if(pageno!=os->pageno){
    int i;

    /* unroll previous partial packet (if any) */
    for(i=os->lacing_packet;i<os->lacing_fill;i++)
      os->body_fill-=os->lacing_vals[i]&0xff;
    os->lacing_fill=os->lacing_packet;

    /* make a note of dropped data in segment table */
    if(os->pageno!=-1){
      os->lacing_vals[os->lacing_fill++]=0x400;
      os->lacing_packet++;
    }
  }

  /* are we a 'continued packet' page?  If so, we may need to skip
     some segments */
  if(continued){
    if(os->lacing_fill<1 ||
       os->lacing_vals[os->lacing_fill-1]==0x400){
      bos=0;
      for(;segptr<segments;segptr++){
	int val=header[27+segptr];
	body+=val;
	bodysize-=val;
	if(val<255){
	  segptr++;
	  break;
	}
      }
    }
  }

  if(bodysize){
    _os_body_expand(os,bodysize);
    memcpy_82300E58(os->body_data+os->body_fill,body,bodysize);
    os->body_fill+=bodysize;
  }

  {
    int saved=-1;
    while(segptr<segments){
      int val=header[27+segptr];
      os->lacing_vals[os->lacing_fill]=val;
      os->granule_vals[os->lacing_fill]=-1;

      if(bos){
	os->lacing_vals[os->lacing_fill]|=0x100;
	bos=0;
      }

      if(val<255)saved=os->lacing_fill;

      os->lacing_fill++;
      segptr++;

      if(val<255)os->lacing_packet=os->lacing_fill;
    }

    /* set the granulepos on the last granuleval of the last full packet */
    if(saved!=-1){
      os->granule_vals[saved]=granulepos;
    }

  }

  if(eos){
    os->e_o_s=1;
    if(os->lacing_fill>0)
      os->lacing_vals[os->lacing_fill-1]|=0x200;
  }

  os->pageno=pageno+1;

  return(0);
}
