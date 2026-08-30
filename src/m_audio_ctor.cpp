#include "types.h"

// sub_82583290 -- constructor of a 50-METHOD CLASS. 272 B, 3 callers.
//
// Chosen for STRUCTURE rather than for throughput. Matching it pins 46 field
// offsets in one go, and the vtable it stores at +0 -- 82060AA8 -- holds
// **fifty consecutive code pointers**, every one of them already a known
// function start. So this does not decompile a function so much as identify
// a class: its layout, its size (at least 0xF8), and fifty methods that can
// now be attributed to it rather than guessed at individually.
//
// WHAT IT IS. Two of the constants say it outright: 44100.0f at +0x4C is a
// sample rate and 360.0f at +0x70 and +0x74 is a pair of angles, alongside
// 10000.0f at +0x6C and 1.0f in four places. Fifty virtual methods, a
// distance-like maximum, two cone angles and a gain -- this is an audio
// emitter or voice.
//
// TWO INTRUSIVE LIST HEADS, both the idiom src/m_list_head.cpp documents:
// `addi r10,r3,4` stored into +4 and +8, and `addi r10,r3,220` stored into
// +0xDC and +0xE0. An empty circular list whose sentinel is the head's own
// node.
//
// AND +0xDC, +0xE0, +0xE4 ARE WRITTEN TWICE -- once early and once as the
// last three stores of the function. That is not a scheduling artefact, it
// is two separate initialisations of the same sub-object: a member whose own
// constructor ran, and then the body re-initialising it. Reproducing it
// needs the writes to appear twice in the source too.
//
// +0xA4 is stored `this`, so the object holds a pointer to itself -- the
// usual reason is a base-class or interface pointer that other code reads
// without knowing the derived type.
//
// The store order below is the two-stream rule from MATCHED.md: every
// INTEGER store in its emitted relative order, then every FLOAT store in
// its emitted relative order. The emitted alternation between them is
// dual-issue scheduling, not the source alternating.
//
// NEAR MISS: 42 of 56 non-relocated words at the EXACT size of 272 bytes --
// up from 1 of 53 at 260, and the thing that moved it refutes what this file
// used to say.
//
// The store ORDER is exactly right: 35 integer stores emitted in precisely
// the target's sequence, which is what the two-stream rule predicts for a
// 46-field constructor and is the strongest confirmation of that lever so
// far.
//
// WHAT WAS MISSING was three words: MSVC's dead-store elimination removed
// the EARLY write of +0xDC/+0xE0/+0xE4, because the last three stores of the
// function overwrite them. This file concluded from that -- and HANDBOOK.md
// repeated it -- that "the retail source has an INLINING BOUNDARY that a
// single translation unit cannot express". THAT WAS WRONG. The measurement
// was right and the conclusion was not, which is the same failure the
// project has now recorded four times.
//
// WRITING THE EARLY GROUP THROUGH A POINTER TO THE MEMBER keeps it:
//
//     static void ResetVoice(void** v)
//     {
//         ((s32*)v)[2] = 0;
//         v[0] = v;
//         v[1] = v;
//     }
//     ...
//     ResetVoice((void**)&a->voiceNext);
//
// Once the address is taken, MSVC can no longer prove that the later
// `a->voiceNext = &a->voiceNext` overwrites the same location, and both
// groups survive: 272 bytes, the target's size, and 42 of 56 words. A bare
// `void** v = (void**)&a->voiceNext;` local instead of the helper is
// identical, so it is the pointer and not the call.
//
// THE DIRECTION MATTERS, and this is the sharp part. Pinning the LATE group
// instead -- helper or bare local -- is 288 bytes, sixteen OVER, and 0 of
// 56: it keeps both groups AND stops MSVC folding the tail's three stores
// into the ones already there. Pinning both is likewise 288. Only the early
// group may be pinned. Six shapes were measured to establish that.
//
// This is the same lever, and the same direction of error, as sub_82700DF8
// in this batch: another constructor whose duplicated stores DSE removed,
// also called an inlining boundary, also solved inside one file -- there by
// real base classes plus an inlined helper on a bitfield word, here by one
// pointer.
//
// WHAT IS STILL WRONG, fourteen words, is the head of the function, and it is
// an ADDRESS CREATION ORDER rather than a store order. Registers are handed
// out as values are created, descending r11, r10, r9, ...:
//
//      target  zero=r11, &a->next=r10, vtable=r9 (lis and addi in place),
//              1.0f base=r8, 10000 base=r7, 360 base=r6, 44100 base=r5,
//              0.0f base=r4
//      ours    zero=r11, vtable lis=r10, 1.0f base=r9, 10000 base=r8,
//              360 base=r7, 44100 base=r6, 0.0f base=r5, vtable addi=r4
//              (a SECOND value, not written in place), &a->next last of all
//
// The target creates `&a->next` second and the whole vtable address third,
// ahead of every float pool base, and issues `stw r11,12(r3)` (f0C = 0) then
// next, then prev, then the vtable, filling the gaps between them with the
// `lis` halves it needs later. Ours materialises all six addresses ahead of
// every store and splits the vtable's lis/addi across two registers, and the
// three small constants downstream (`li 1`, `li 128`, `li -1`) are renamed
// with them.
//
// THE FIVE FLOAT POOL BASES ARE ALREADY RIGHT, relative to each other: their
// creation order is the order the constants are first STORED -- 1.0f, 10000,
// 360, 44100, 0.0f -- and the float statement order below produces it.
//
// THE BASE-CLASS READING OF THE STORE ORDER IS CONFIRMED, and it is worth
// having even though it did not change the score. MATCHED.md, on
// sub_8253F5D8: "stores emitted before a class's own vptr belong to a BASE of
// it". Written as a polymorphic class -- one virtual function, so the vptr is
// compiler-managed at +0, and a real `AudioLinkBase { next; prev; f0C; }`
// pushed to +4 whose constructor writes f0C, next, prev -- the emitted store
// order becomes exactly f0C, next, prev, vptr, with no scheduling coincidence
// left in it. It scores the same 42 of 56, which is the useful part: the
// residue was never the store order. A NON-polymorphic base is a layout
// refutation rather than a shape one -- MSVC puts it at offset 0, so `vt`
// lands at 0x0C and the whole function is 0 of 55 at 268 bytes.
//
// MEASURED AND RULED OUT, beyond the four in the paragraph below:
//   a real `AudioEmitter::AudioEmitter()` constructor, vt a plain member  42
//   the polymorphic base-class form above                                 42
//   the f0C/next/prev group written through a pointer to a 12-byte
//     sub-struct at +4 -- the address-of lever rather than a base class   42
//   all 16 ways of naming zero, `&a->next` and the vtable pointer in
//     locals ahead of the stores, in every order: naming the zero or the
//     vtable pointer is byte-identical at 42; naming the LINK pointer is
//     280 bytes and 12 of 56 in all eight combinations that include it
//   all 72 flag combinations from tools/flagsweep.py: 44 give this same
//     42 of 68 and 28 give 41 of 68, so the level is /O2 and flags are done
//
// Four shapes were measured earlier and none is more than a word better:
// `&a->vt` pinned (42), `&a->f0C` pinned (43), the next/prev pair pinned
// (42), and the vtable written first in source (43). The last two are the
// informative pair -- moving the vtable assignment to the front of the source
// does NOT move its store to the front of the output, so the hoist is the
// scheduler's and not the source's.
//
// The `self` store -- `a->self = a` -- is still emitted earlier than its
// source position, which is the "a store with no dependency is hoisted"
// lever, and no source order moves it.
struct AudioVT;
extern const AudioVT kAudioVTable_82060AA8;

struct AudioEmitter
{
    /* 0x00 */ const AudioVT* vt;
    /* 0x04 */ void* next;
    /* 0x08 */ void* prev;
    /* 0x0C */ s32   f0C;
    /* 0x10 */ char  unk0010[0x18 - 0x10];
    /* 0x18 */ s32   f18;
    /* 0x1C */ s32   f1C;
    /* 0x20 */ s32   f20;
    /* 0x24 */ char  unk0024[0x30 - 0x24];
    /* 0x30 */ s32   f30;
    /* 0x34 */ s32   f34;
    /* 0x38 */ s32   f38;
    /* 0x3C */ s32   f3C;
    /* 0x40 */ s32   f40;
    /* 0x44 */ s32   f44;
    /* 0x48 */ f32   f48;
    /* 0x4C */ f32   sampleRate;
    /* 0x50 */ f32   f50;
    /* 0x54 */ s32   f54;
    /* 0x58 */ char  unk0058[4];
    /* 0x5C */ f32   f5C;
    /* 0x60 */ f32   f60;
    /* 0x64 */ f32   f64;
    /* 0x68 */ f32   f68;
    /* 0x6C */ f32   f6C;
    /* 0x70 */ f32   innerAngle;
    /* 0x74 */ f32   outerAngle;
    /* 0x78 */ f32   f78;
    /* 0x7C */ s32   f7C;
    /* 0x80 */ s32   f80;
    /* 0x84 */ s32   f84;
    /* 0x88 */ char  unk0088[4];
    /* 0x8C */ s32   f8C;
    /* 0x90 */ s32   f90;
    /* 0x94 */ s32   f94;
    /* 0x98 */ char  unk0098[4];
    /* 0x9C */ s32   f9C;
    /* 0xA0 */ char  unk00A0[4];
    /* 0xA4 */ void* self;
    /* 0xA8 */ char  unk00A8[0xB4 - 0xA8];
    /* 0xB4 */ s32   fB4;
    /* 0xB8 */ char  unk00B8[0xC0 - 0xB8];
    /* 0xC0 */ s32   fC0;
    /* 0xC4 */ s32   fC4;
    /* 0xC8 */ s32   fC8;
    /* 0xCC */ s32   fCC;
    /* 0xD0 */ s32   fD0;
    /* 0xD4 */ s32   fD4;
    /* 0xD8 */ char  unk00D8[4];
    /* 0xDC */ void* voiceNext;
    /* 0xE0 */ void* voicePrev;
    /* 0xE4 */ s32   fE4;
    /* 0xE8 */ char  unk00E8[4];
    /* 0xEC */ s32   fEC;
    /* 0xF0 */ s32   fF0;
    /* 0xF4 */ s32   fF4;
};
ASSERT_OFFSET(AudioEmitter, sampleRate, 0x4C);
ASSERT_OFFSET(AudioEmitter, innerAngle, 0x70);
ASSERT_OFFSET(AudioEmitter, self, 0xA4);
ASSERT_OFFSET(AudioEmitter, voiceNext, 0xDC);
ASSERT_OFFSET(AudioEmitter, fF4, 0xF4);

/* The EARLY reset of the voice link. It has to go through a POINTER: written
   as three member stores it is dead-store-eliminated against the identical
   group at the end of the constructor, and the body comes out 260 bytes with
   1 of 53 words. Taking the address removes MSVC's proof that the two groups
   write the same locations. Pinning the LATE group instead is 288 bytes --
   it also stops the tail folding -- so only this one may be pinned. */
static void ResetVoice(void** v)
{
    ((s32*)v)[2] = 0;
    v[0]         = v;
    v[1]         = v;
}

void ConstructAudioEmitter(AudioEmitter* a)
{
    a->f0C       = 0;
    a->next      = &a->next;
    a->prev      = &a->next;
    a->vt        = &kAudioVTable_82060AA8;
    ResetVoice((void**)&a->voiceNext);
    a->f1C       = 0;
    a->f18       = 0;
    a->f20       = 0;
    a->f44       = 1;
    a->f54       = 128;
    a->f30       = 0;
    a->f34       = 0;
    a->f38       = -1;
    a->fB4       = 0;
    a->fC0       = 0;
    a->f40       = 0;
    a->f84       = 0;
    a->f94       = 0;
    a->f9C       = 0;
    a->f8C       = 0;
    a->f90       = 0;
    a->fD0       = 0;
    a->fC4       = 0;
    a->fD4       = 0;
    a->self      = a;
    a->f7C       = 0;
    a->f80       = 0;
    a->fC8       = 0;
    a->fCC       = 0;
    a->fEC       = 0;
    a->fF0       = 0;
    a->fF4       = 0;
    a->f3C       = 0;
    a->voiceNext = &a->voiceNext;
    a->voicePrev = &a->voiceNext;
    a->fE4       = 0;

    a->f68        = 1.0f;
    a->f6C        = 10000.0f;
    a->innerAngle = 360.0f;
    a->outerAngle = 360.0f;
    a->f78        = 1.0f;
    a->f48        = 1.0f;
    a->sampleRate = 44100.0f;
    a->f50        = 0.0f;
    a->f5C        = 0.0f;
    a->f60        = 0.0f;
    a->f64        = 0.0f;
}
