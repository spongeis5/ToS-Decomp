# Matched functions

Byte-for-byte matches against the retail image, compiled with the original
XDK 8276 toolchain (`cl.exe` 15.00.8153) at `/O2 /Gy /GS- /fp:fast`, or at
`/O2 /Os /Gy /GS- /fp:fast` where the `flags` column says so.

**This table is generated.** `python tools/matched_table.py` rebuilds it from
`src/manifest.txt`; `--check` fails if it has drifted, and `tools/verify.py`
runs that. The byte counts are the COMPILED lengths, not the inventory's --
the inventory is wrong in both directions, short where a tail call's dead
`blr` was not counted and long where one `.pdata` row covers several
frameless bodies.

**179 functions, 9776 bytes.** Verify all of them, plus the reconstructing
build and five negative controls, with one command:

```bash
python tools/verify.py
```

Every match is also a row in `src/manifest.txt`, so `tools/build.py` compiles
it, resolves its relocations against the retail bytes and splices it into
`.text`. Nothing here is a match on `match.py`'s word-comparison alone.

**The retail build did NOT use one optimisation level everywhere.** 32 of
these need `/O2 /Os`; the rest need plain `/O2`. See "Flags are a property of
the translation unit" below -- this was claimed the other way round for a
while and the claim was wrong.

| address | bytes | callers | source | symbol | flags |
|---|---|---|---|---|---|
| `82662E08` | 152 | 730 | `m_handle_release.cpp` | ReleaseHandle | `/O2 /Os` |
| `8262F658` | 164 | 420 | `m_bin_free.cpp` | BinFree | `/O2` |
| `82662938` | 96 | 367 | `m_handle_release.cpp` | AcquireEntry | `/O2 /Os` |
| `82807B38` | 20 | 314 | `guard_tailcall.cpp` | - | `/O2` |
| `82805D20` | 108 | 276 | `m_node_destroy.cpp` | - | `/O2` |
| `82602EA0` | 104 | 240 | `m_track_or_add.cpp` | - | `/O2` |
| `82603108` | 64 | 237 | `c_release_guarded.cpp` | - | `/O2` |
| `8262F5D0` | 136 | 206 | `m_bin_free.cpp` | BinAlloc | `/O2` |
| `8215A420` | 64 | 147 | `c_hash_upper.cpp` | - | `/O2` |
| `82600BD8` | 16 | 135 | `global_field.cpp` | - | `/O2` |
| `82667E58` | 136 | 132 | `m_vector_reserve.cpp` | VectorReserve | `/O2` |
| `82806D08` | 20 | 132 | `a_report_badthis.cpp` | - | `/O2` |
| `821A4628` | 28 | 108 | `ctor_vt.cpp` | - | `/O2` |
| `82663370` | 60 | 105 | `b_release_ref.cpp` | - | `/O2 /Os` |
| `82160880` | 36 | 100 | `a_copy_fields.cpp` | - | `/O2` |
| `8219EB58` | 96 | 84 | `c_bucket_find.cpp` | - | `/O2` |
| `8253FD70` | 28 | 82 | `array_add.cpp` | - | `/O2` |
| `826918F8` | 44 | 82 | `d_basis_identity.cpp` | - | `/O2` |
| `82663260` | 44 | 77 | `m_ref_ctor.cpp` | - | `/O2 /Os` |
| `826A3648` | 52 | 75 | `c_share_static.cpp` | - | `/O2 /Os` |
| `822D2450` | 24 | 59 | `table_index.cpp` | - | `/O2` |
| `8224E6F8` | 40 | 54 | `a_item_vcall1.cpp` | - | `/O2` |
| `8214D998` | 96 | 50 | `d_normalize3.cpp` | Normalize3 | `/O2` |
| `82540750` | 28 | 49 | `string_utils.cpp` | StrCopy | `/O2` |
| `827C5198` | 20 | 48 | `vcall116.cpp` | - | `/O2 /Os` |
| `8287E440` | 172 | 45 | `c_iter_equal.cpp` | IterEqual | `/O2 /Os` |
| `826376E0` | 100 | 42 | `c_probe_map.cpp` | - | `/O2` |
| `8224E178` | 40 | 41 | `a_vcall4_or_neg1.cpp` | - | `/O2` |
| `82805F48` | 52 | 40 | `b_setstr_len.cpp` | - | `/O2` |
| `8253A1C0` | 44 | 38 | `a_out_or_err.cpp` | - | `/O2` |
| `82540728` | 36 | 37 | `string_utils.cpp` | StrLen | `/O2` |
| `8262FB50` | 48 | 37 | `b_free_sentinel.cpp` | - | `/O2` |
| `82637590` | 56 | 37 | `b_free_items.cpp` | - | `/O2` |
| `8214D640` | 176 | 35 | `d_normalize_to.cpp` | NormalizeTo | `/O2` |
| `82151C50` | 48 | 35 | `d_wrap_2pi.cpp` | - | `/O2` |
| `82154A68` | 124 | 34 | `d_mtx_translate_row.cpp` | - | `/O2` |
| `8252D9C8` | 32 | 33 | `b_fwd_global5.cpp` | - | `/O2` |
| `826A32E8` | 60 | 33 | `b_hash_lower.cpp` | - | `/O2 /Os` |
| `827007E8` | 16 | 32 | `set_vtable_827007E8.cpp` | - | `/O2 /Os` |
| `82602F98` | 20 | 31 | `a_tls_field.cpp` | - | `/O2` |
| `826632F8` | 40 | 31 | `c_atomic_inc4.cpp` | - | `/O2` |
| `825FD7C0` | 60 | 28 | `d_attach_notify.cpp` | - | `/O2` |
| `82662F20` | 36 | 28 | `c_handle_lookup.cpp` | - | `/O2 /Os` |
| `821559D8` | 140 | 27 | `f_name_lookup.cpp` | - | `/O2` |
| `8261B2F8` | 52 | 27 | `d_flag_vcall.cpp` | - | `/O2` |
| `826C6C60` | 112 | 27 | `f_vec_compact.cpp` | - | `/O2` |
| `821636A8` | 24 | 26 | `chain5.cpp` | - | `/O2` |
| `822481B0` | 40 | 25 | `m_flag_guard.cpp` | - | `/O2` |
| `82254A88` | 84 | 25 | `f_find_state.cpp` | - | `/O2` |
| `82665388` | 8 | 25 | `a_fwd24.cpp` | - | `/O2` |
| `82666360` | 12 | 25 | `a_fwd24_self.cpp` | - | `/O2` |
| `82677028` | 20 | 25 | `owner_clear.cpp` | ClearAndHandle | `/O2` |
| `82677040` | 20 | 25 | `owner_clear.cpp` | ClearAndHandleOther | `/O2` |
| `82545348` | 48 | 24 | `m_span_out.cpp` | - | `/O2` |
| `82724A68` | 44 | 24 | `m_tls_reserve.cpp` | - | `/O2` |
| `82540770` | 40 | 23 | `string_utils.cpp` | StrCopyN | `/O2` |
| `825408B0` | 68 | 21 | `m_strcmp_n.cpp` | - | `/O2` |
| `825408F8` | 108 | 20 | `m_stricmp.cpp` | StrCompareI | `/O2` |
| `82600A08` | 84 | 20 | `f_pool_link.cpp` | - | `/O2` |
| `82637688` | 84 | 20 | `f_hash_probe.cpp` | - | `/O2` |
| `826731B0` | 40 | 20 | `d_init_obj_1.cpp` | - | `/O2` |
| `827156B8` | 88 | 20 | `f_state_advance.cpp` | - | `/O2 /Os` |
| `8277E170` | 52 | 20 | `e_ones_zeros8.cpp` | - | `/O2` |
| `82164040` | 120 | 19 | `h_bump_groups.cpp` | BumpAll | `/O2` |
| `82255408` | 144 | 19 | `h_kind_allows.cpp` | KindAllows | `/O2` |
| `82600BB0` | 20 | 19 | `vcall_arg2.cpp` | - | `/O2` |
| `826A3350` | 20 | 19 | `null_tailcall.cpp` | - | `/O2` |
| `826C5E00` | 28 | 19 | `vcall_global_arg.cpp` | - | `/O2` |
| `821A4FA0` | 16 | 18 | `fwd_global.cpp` | - | `/O2` |
| `82691C50` | 156 | 18 | `e_mtx23_mul.cpp` | - | `/O2 /Os` |
| `826A3328` | 36 | 18 | `m_init_512.cpp` | - | `/O2` |
| `8224E7C0` | 16 | 17 | `arr_index0.cpp` | - | `/O2` |
| `8224E080` | 20 | 16 | `vcall_f8_40.cpp` | - | `/O2` |
| `8252BE30` | 44 | 16 | `g_out_or_err36.cpp` | - | `/O2` |
| `82600A60` | 36 | 16 | `m_list_head.cpp` | - | `/O2` |
| `8260FEB0` | 76 | 16 | `g_find_by_key.cpp` | - | `/O2` |
| `826C0F28` | 40 | 16 | `g_sum_chain.cpp` | - | `/O2` |
| `82724A98` | 28 | 16 | `m_store_ge.cpp` | - | `/O2` |
| `827841D8` | 84 | 16 | `m_release.cpp` | Release | `/O2 /Os` |
| `828864E0` | 20 | 16 | `vcall_arg_adj.cpp` | - | `/O2 /Os` |
| `822E2048` | 28 | 15 | `g_fwd_6args.cpp` | - | `/O2` |
| `82703E28` | 124 | 15 | `h_blk_ctor.cpp` | BlkOwnerConstruct | `/O2 /Os` |
| `821FF908` | 168 | 14 | `e_can_use.cpp` | - | `/O2` |
| `822021F8` | 16 | 14 | `stride116.cpp` | - | `/O2` |
| `8220F810` | 72 | 14 | `g_vec3_pick.cpp` | - | `/O2` |
| `82540968` | 124 | 14 | `m_stricmp.cpp` | StrCompareNI | `/O2` |
| `82677058` | 8 | 14 | `m_fwd_84.cpp` | - | `/O2` |
| `826919A8` | 76 | 14 | `e_mtx23_xform.cpp` | - | `/O2 /Os` |
| `82691CF0` | 164 | 14 | `e_mtx23_premul.cpp` | - | `/O2 /Os` |
| `826A4528` | 56 | 14 | `g_share_tagged.cpp` | - | `/O2 /Os` |
| `826C0F50` | 100 | 14 | `h_chain_nth.cpp` | - | `/O2` |
| `827C6D08` | 112 | 14 | `h_cursor_skip.cpp` | - | `/O2 /Os` |
| `82161630` | 112 | 13 | `j_normalize_or_default.cpp` | NormalizeOrDefault | `/O2` |
| `821675B8` | 56 | 13 | `i_sum_parts.cpp` | - | `/O2` |
| `8219FCD8` | 108 | 13 | `i_state_idle.cpp` | IsIdle | `/O2` |
| `82218F90` | 28 | 13 | `g_fwd_shift4.cpp` | - | `/O2` |
| `8252D950` | 36 | 13 | `g_fwd_global_a.cpp` | - | `/O2` |
| `8252D978` | 36 | 13 | `g_fwd_global_b.cpp` | - | `/O2` |
| `82540878` | 52 | 13 | `i_strcmp.cpp` | - | `/O2` |
| `82600A88` | 56 | 13 | `m_unlink_free.cpp` | - | `/O2` |
| `8264B6F0` | 40 | 13 | `h_gate_pos.cpp` | - | `/O2` |
| `82692590` | 88 | 13 | `i_charclass.cpp` | - | `/O2 /Os` |
| `82799B98` | 76 | 13 | `j_reset_range.cpp` | - | `/O2 /Os` |
| `821A99F8` | 176 | 12 | `i_canon_switch.cpp` | - | `/O2` |
| `8228AF08` | 24 | 12 | `m_enqueue4.cpp` | Enqueue | `/O2` |
| `825FEF00` | 124 | 12 | `k_sorted_insert.cpp` | - | `/O2` |
| `82639C68` | 8 | 12 | `m_get_48.cpp` | - | `/O2` |
| `826A9E88` | 84 | 12 | `i_release_ref4.cpp` | - | `/O2 /Os` |
| `827727D0` | 84 | 12 | `i_lex_advance.cpp` | ScanNext | `/O2 /Os` |
| `82799878` | 28 | 12 | `m_scale_sq.cpp` | - | `/O2 /Os` |
| `827FE8A0` | 36 | 12 | `m_cached_or_make.cpp` | - | `/O2` |
| `8214C578` | 52 | 11 | `i_list_find_key.cpp` | - | `/O2` |
| `821AC2F0` | 72 | 11 | `j_inv_or_clamp.cpp` | - | `/O2` |
| `826C0FC8` | 24 | 11 | `stride24.cpp` | - | `/O2` |
| `827DAC60` | 76 | 11 | `k_find_pair.cpp` | - | `/O2 /Os` |
| `82166FD0` | 16 | 10 | `fwd_vec3.cpp` | - | `/O2` |
| `82167FE0` | 212 | 10 | `j_scale_pair.cpp` | - | `/O2` |
| `822020B0` | 16 | 10 | `chain2_156.cpp` | - | `/O2` |
| `8262FE10` | 60 | 10 | `k_short_release.cpp` | - | `/O2` |
| `8262FF90` | 52 | 10 | `k_global_release_tls.cpp` | - | `/O2` |
| `82631D98` | 36 | 10 | `k_bits_or_zero.cpp` | - | `/O2` |
| `821A4FB0` | 20 | 9 | `fwd_global_n.cpp` | - | `/O2` |
| `822CEE08` | 64 | 9 | `k_chunk_at.cpp` | - | `/O2` |
| `8253FE28` | 28 | 9 | `zero48.cpp` | - | `/O2` |
| `82603948` | 20 | 9 | `null_call0.cpp` | - | `/O2` |
| `82704688` | 40 | 9 | `m_select2.cpp` | - | `/O2 /Os` |
| `827245E0` | 32 | 9 | `ring_index2.cpp` | - | `/O2` |
| `827C5180` | 20 | 9 | `k_vcall148.cpp` | - | `/O2 /Os` |
| `8214CCB8` | 84 | 8 | `j_reset_state.cpp` | - | `/O2` |
| `821EE668` | 24 | 8 | `m_fwd_ctx.cpp` | - | `/O2` |
| `8225B450` | 44 | 8 | `m_state_2to4.cpp` | - | `/O2` |
| `82265D88` | 40 | 8 | `m_ready_not255.cpp` | - | `/O2` |
| `825BD9B0` | 48 | 8 | `m_read_le32.cpp` | - | `/O2` |
| `82639C28` | 16 | 8 | `chain_add48.cpp` | - | `/O2` |
| `8279D958` | 20 | 8 | `m_vcall4_f8.cpp` | - | `/O2` |
| `82151690` | 44 | 7 | `m_max_pair.cpp` | - | `/O2` |
| `82156050` | 16 | 7 | `link_node.cpp` | - | `/O2` |
| `821A5378` | 20 | 7 | `eq2_208.cpp` | - | `/O2` |
| `821BCA48` | 44 | 7 | `m_ctor_7zero.cpp` | - | `/O2` |
| `821C77A8` | 28 | 7 | `m_enqueue12.cpp` | Enqueue | `/O2` |
| `82265D30` | 20 | 7 | `set0_255.cpp` | - | `/O2` |
| `82548F10` | 28 | 7 | `zero5_20first.cpp` | - | `/O2` |
| `82727258` | 16 | 7 | `stride8.cpp` | - | `/O2` |
| `8214CC48` | 16 | 6 | `or_flag.cpp` | - | `/O2` |
| `8216CDA0` | 40 | 6 | `m_flag_dispatch.cpp` | - | `/O2` |
| `8219FC90` | 24 | 6 | `eq1_2260.cpp` | - | `/O2` |
| `821A5490` | 24 | 6 | `cmp_set.cpp` | - | `/O2` |
| `821A93C8` | 24 | 6 | `eq1_144_36.cpp` | - | `/O2` |
| `822553C0` | 24 | 6 | `eq1_2264.cpp` | - | `/O2` |
| `8225FDD8` | 20 | 6 | `zero3.cpp` | - | `/O2` |
| `82272AA0` | 28 | 6 | `m_first_or_self.cpp` | - | `/O2` |
| `822D2978` | 24 | 6 | `m_upcast_call.cpp` | - | `/O2` |
| `822D40F8` | 32 | 6 | `copy3_68.cpp` | - | `/O2` |
| `822D4118` | 32 | 6 | `copy3_72.cpp` | - | `/O2` |
| `82543F60` | 24 | 6 | `tail_or_zero.cpp` | - | `/O2` |
| `825BD930` | 16 | 6 | `bit_test.cpp` | - | `/O2` |
| `82649240` | 20 | 6 | `zero64_68_0.cpp` | - | `/O2` |
| `82697608` | 16 | 6 | `guard_arg3.cpp` | - | `/O2` |
| `82727028` | 20 | 6 | `store_sum.cpp` | - | `/O2` |
| `827827B8` | 40 | 6 | `m_release.cpp` | AddRef | `/O2` |
| `828133B8` | 28 | 6 | `two_vtables_b.cpp` | - | `/O2 /Os` |
| `8288A788` | 28 | 6 | `two_vtables.cpp` | - | `/O2 /Os` |
| `82202BC8` | 28 | 5 | `store_floats.cpp` | - | `/O2` |
| `8224DF58` | 24 | 5 | `ctor_vt2.cpp` | - | `/O2` |
| `82250B88` | 24 | 5 | `eq0_stride16.cpp` | - | `/O2` |
| `822D0BE8` | 32 | 5 | `deref_or_zero.cpp` | - | `/O2` |
| `825E3598` | 24 | 5 | `vcall_global_2.cpp` | - | `/O2 /Os` |
| `825E35C8` | 24 | 5 | `vcall_global_4.cpp` | - | `/O2 /Os` |
| `825E41D8` | 16 | 5 | `zero2.cpp` | - | `/O2` |
| `827245C0` | 28 | 5 | `ring_index.cpp` | - | `/O2` |
| `8272CB68` | 16 | 5 | `load_global_store.cpp` | - | `/O2` |
| `827C4FB0` | 24 | 5 | `ptr_or_null.cpp` | - | `/O2` |
| `827FE808` | 16 | 5 | `and_byte.cpp` | - | `/O2 /Os` |
| `822D2528` | 24 | 4 | `table624.cpp` | - | `/O2` |
| `827A7C98` | 20 | 4 | `store_two.cpp` | - | `/O2` |
| `82216918` | 304 | 2 | `m_line_of_sight.cpp` | TtCheckLineOfSight | `/O2` |
| `822607F0` | 120 | 2 | `grid_indices.cpp` | - | `/O2` |
| `826FE5B8` | 16 | 2 | `set_vtable.cpp` | SetVTableD170 | `/O2` |
| `826FE5C8` | 16 | 2 | `set_vtable.cpp` | SetVTableD180 | `/O2` |
---

## How these were found

The technique is unchanged and is still the whole of it: **read the target's
register discipline out of the disassembly instead of guessing plausible C.**
Which value it keeps live, and for how long, is the specification.

What made the batch work was doing it in bulk -- dump forty candidates with
their disassembly, write the twenty that read clearly, compile all twenty at
once. Roughly 70% match on the first attempt. The rest fail in ways that are
usually one edit from correct, and the edit is visible in the diff.

### Idioms worth recognising, because each one appeared several times

| what you see | what it is |
|---|---|
| `addi -1 ; cntlzw ; rlwinm rX,rY,27,31,31` | `x == 1`, branchless. Without the `addi`, `x == 0` |
| `rlwinm rX,rY,2,0,29` then `lwzx` | `array[i]` on 4-byte elements |
| `rlwinm r11,r4,1 ; add r11,r4,r11 ; rlwinm r11,r11,3` | `i * 24` built as `(i + i*2) * 8` |
| `mulli` | a stride that is not a power of two -- the number is the element size |
| `lis` + `addi` + a second `addi` | a field inside a global array element; the second addi is the field offset |
| `addi rX,rY,-1` before an update-form load or store | a biased pointer so `lbzu`/`stwu` can increment and access in one instruction |
| `mtctr` + `bdnz` | a counted loop; the `li` before `mtctr` is the trip count |
| `beqlr` / `bnelr` | a guard written as a conditional RETURN, so the body is the fall-through |
| `lwz rX,0(rY) ; lwz rX,n(rX) ; mtctr ; bctr` | a virtual call; the slot index is `n / 4` |
| `cmplwi rX,0 ; addi rY,rX,n ; bne- ; li rY,0` | a BASE-CLASS UPCAST: `static_cast<Base*>(p)` must keep null null. The null test guards only the adjustment -- the pointer is often dereferenced right after regardless |
| `addic rD,rS,-1 ; subfe rT,rD,rS` | branchless `x != 0`: no carry when rS is 0, carry otherwise |
| `rlwinm rA,rS,1,31,31` then `srawi` and `adde` | a branchless SIGNED comparison; the `subfc` in it is there for the CARRY, and its difference is never used |

### Levers, measured this session

Each of these was found by a match that would not come out any other way, and
each has been tried on other functions since. They are ordered by how often
they have paid.

**A `do/while` is the one loop MSVC never rotates.** Written as `while (n--)`
or as `for (;;) { if (n-- == 0) break; ... }`, the compiler PEELS the first
test out in front of the loop and puts a second copy at the bottom -- 48
bytes where the target has 40, with the pointer setup stranded in the middle.
The target's loop top was the test itself, reached by falling into it. So:
**when the target's loop top is a branch target reached by fall-through, with
no peeled copy of the test ahead of it, the source is a `do/while`.** That
was the whole of `StrCopyN`.

**Un-naming a local.** `sub_8224E178` went 0/10 to 10/10 by DELETING a local.
With `Target* t = h->target; if (t) return t->vt->slot[4](t);` MSVC loads
straight into r3. With `h->target` spelled out at all three uses it CSEs into
r11, tests the scratch, and materialises r3 with a separate `rlwinm
rD,rS,0,0,31` copy -- which is the target. **When a target carries an
apparently pointless `mr`/`rotlwi` into an argument register, remove the
named local before trying anything else.**

**Naming a sub-expression as a local.** The exact opposite, and it is not a
contradiction: it decides whether a value is computed early or late.
`sub_82154A68` written as three direct assignments is 148 bytes and 0 of 31
words, because each store might alias the input and MSVC finishes one element
before touching it again. Naming the three dot products as locals first lets
them all be computed up front -- 30 of 30.

**The aliasing tell says which of two mirror-image float functions you have.**
`sub_8214D998` loads x/y/z once and never reloads; `sub_8214D640` reloads y
and z after every store. Copying into locals to explain the single load is
wrong in both cases. The first writes members through ONE pointer, so MSVC
already knows nothing aliases; the second writes through a different pointer
than it reads, and the reloads are the aliasing it could not remove.

**`x > 0` versus `x != 0` on an unsigned is worth five words, not one.**
`sub_825FD7C0` spends `subf`/`cntlzw`/`rlwinm`/`cmplwi`/`beqlr` where `if (a
== b)` gives `cmplw`/`bnelr`. Every branch spelling folds -- plain, inverted,
early-return, bool local, inline predicate, while/switch/goto, pointer
difference, xor -- and so does every level from `/Od` to `/Ox`. Only
`unsigned same = (...); if (same > 0)` forces the value to be materialised.

**Operand order of `==` is readable off the `subf`.** MSVC emits `a == b` as
`subf rD,rA,rB`, computing `b - a`. Getting the order backwards costs exactly
one word and nothing else, which is why it is worth reading rather than
guessing.

**`lwzx` operand order is set by the displacement, and inlining destroys it.**
Compiling the same subscript three ways:

| shape | emitted |
|---|---|
| free function, `a->items[i]`, items at offset **0** | `lwzx r3,r10,r11` -- index in rA |
| free function, items at offset 4 / 8 / 12 | `lwzx r3,r11,r10` -- base in rA |
| **member** function, `items[i]`, items at offset 8 | `lwzx r3,r10,r11` -- index in rA |

So `this->items[i]` and `a->items[i]` compile to different operand orders for
the same address. But sixteen attempts to carry the member flavour through
the inliner -- `__forceinline`, `operator[]`, template member, base class,
reference member, union, three pointer-arithmetic spellings -- all normalise
to base-first once inlined. That makes `sub_8215ED28` an address-selection
stall rather than a shape problem.

**`#pragma intrinsic(strlen)` is distinguishable from a hand-written loop.**
Both emit the same seven-instruction walk, but a hand-written loop folds the
`-1` into the argument register (`addi r5,r11,-1`) while the intrinsic keeps
it in r11 and follows with `rotlwi r5,r11,0` -- an explicit 32-to-64
zero-extension of `size_t`. **A trailing `rlwinm rX,rY,0,0,31` after an
inlined string measure is the tell.**

**`lwz rX,0(r13)` is `__declspec(thread)`.** r13 holds the thread block. The
diagnostic is that the slot offset does NOT fold: the compiler will not
combine a relocated immediate with anything, so a member offset always
appears as a separate `addi`. `li <slot>` + `lwz rX,0(r13)` + `lwzx` READS
the variable; `lwz` + `li` + `add` takes its ADDRESS. The relocation is
IMAGE_REL_PPC_TOCREL14 and `build.py` resolves it from the retail word.

**A `||` chain and a sequence of `if`s are different code.** In
`sub_8287E440` the predicate written as separate `if (x) return true;`
statements makes MSVC materialise the LAST comparison branchlessly
(`subfc`/`eqv`/`rlwinm`/`addze`/`clrlwi`) and give every early exit its own
`li r11,1 ; b`. Written as one short-circuit expression the early exits
branch into a SHARED `li r11,1` and the last term uses a real `cmpw`/`ble-`.
That is two words per inlined copy -- 188 bytes against the target's 172.

**"The return value is `this`" is a separate lever from "make it a member."**
On `sub_826A3648` the compiler hoists an address computation above a store,
which costs the register reuse. Eleven rearrangements did not move it --
store first, local pointer, `p += 4`, volatile slot/member/refcount, an
array-element address, a cast-free `long[2]` view, an inlined member AddRef,
incrementing through the stored pointer, a plain member function. A
CONSTRUCTOR reorders it, and so does `operator=`; a `void` member function of
the same class does not. What the two that work have in common is that **r3
is live out**.

**`rlwinm rD,rS,0,0,31` is the fingerprint of a common subexpression being
copied** -- including `rotlwi r11,r11,0`, a move to itself. Writing
`t->slots[i]` out three times produces it; naming it in a local makes it
disappear. Combined with the un-naming lever above, that gives a two-way
control: **the presence of a pointless-looking register move says the source
repeated the expression, and its absence says the source named it.**

**Signedness sometimes has to be split WITHIN one expression.** In
`sub_8215A420` the character must be UNSIGNED where the loop tests it
(`cmplwi` on the raw byte) and SIGNED inside the case fold. All-unsigned lets
the compiler prove `c & 0x40` non-negative and collapse a mask and a shift
into one `rlwinm` -- 68 bytes with two extra `mr`s. All-signed forces an
`extsb` for the loop test -- 72 bytes. `char c = (char)*s;` inside a
`while (*s != 0)` over a `const u8*` gives 16 of 16.

**A hand-declared "intrinsic" that is not one compiles WITHOUT A WARNING and
emits a real call.** `__lwsync` is not a function: `ppcintrinsics.h` defines
it as `__emit(0x7C2004AC)`, the raw instruction word. Declared by hand as
`extern "C" void __lwsync(void);` with `#pragma intrinsic(__lwsync)`, cl.exe
accepts both lines silently and emits `bl __lwsync` -- which forces a stack
frame, spills `this` to r31, and turns a tail call into a `bctrl`: 132 bytes
against the target's 84. Nothing in the diagnostics says so. **The giveaway
is a `bl` in your own object where the target has none**, so disassemble the
object rather than reading only the diff. Include the XDK header.

**THE `/Os` SIGNATURE IS REGISTER COALESCING.** This is the single most
useful thing to know when a function will not match. `/O2` gives a
short-lived value its own fresh register; `/O2 /Os` reuses the one already
holding the input. Every one of these was right at `/Os` and two words wrong
at `/O2`, with identical instructions in identical order:

```
m_scale_sq       fmuls f13,f0,f0     vs  fmuls f0,f0,f0
m_select2        rlwinm r10,r11,2    vs  rlwinm r11,r11,2
m_ref_ctor       addi r9,r11,...     vs  addi r11,r11,...
m_release        addic. r6,r10,-1    vs  addic. r11,r11,-1
g_share_tagged   ori r5,r11,1        vs  ori r11,r11,1
c_share_static   addi r6,r11,4       vs  addi r11,r11,4
```

**So: when the instructions are right and a destination register is FRESH
where the target reuses its source, that is the optimisation level and not
the source. Change the flag before touching a line of C.** Six of the
project's `/Os` functions were found this way and none of them had a source
shape to find -- several were checked by rewriting the expression, and the
rewrite compiled to the same thing at that level every time.

**Source operand order of a COMMUTATIVE FLOAT operator is not readable, so
do not spend edits there.** Under `/fp:fast`, `a*b` and `b*a` compile to
byte-identical code, and so do `a*b + c*d` and `c*d + a*b`. Verified on three
functions, including all 16 flip combinations of `sub_82155080` and all four
assignments of `sub_82691C50`. What decides which operand lands in the A slot
of an `fmuls` is HOW THE VALUE WAS PRODUCED, not how the source wrote it.
That is the opposite of `subf` and `add`, where the order IS readable.

**Whether a snapshot is scalars or an array changes register assignment.**
`sub_82691C50` sits at 31 of 39 with six scalar locals -- and fourteen shapes
(member function, references, `Mtx23 s = *this`, const locals, late locals,
compute-all-then-store, mixed member and local reads, aliased pointers) are
all 31 of 39 or worse. `float s[6]` is 39 of 39.

**Spelling a repeated sub-expression through a LONGER CHAIN defeats CSE.**
`sub_821FF908` reloads a field it already had in a register, with no store in
between. With both reads written `e->limit` MSVC common-subexpressions them
and everything after shifts by a word -- 17 of 41, and nine source shapes
plus a 2,304-combination flag sweep could not fix it. Writing the second read
as `s->entry->limit` -- same value, same register, longer expression --
produces the reload: 40 of 40. **When a target reloads a field it already
had, and nothing stored in between, the source spelled the two reads
differently.**

**`/fp:precise` is distinguishable, and it is a trap.** It adds a `bso-`
unordered check after `fcmpu` on any float comparison. On `sub_821FF908` it
produced the right SIZE for entirely the wrong reason, and a flag sweep
ranked it first. Size agreement is not evidence.

**The `/Os` signature also appears as a TRANSPOSITION, not only as
fresh-versus-reused.** `sub_827DAC60` at `/O2` has identical instructions in
identical order and seven words wrong, every one of them a register NAME: a
loop carrying both an index and an induction pointer gets the two swapped
between r10 and r11. So when a loop carrying both comes out transposed,
change the flag before rewriting the loop.

**Branch polarity scales with the number of guards that SHARE an exit.**
`sub_821675B8` scored **2 of 14** written as three flat
`if (x == 0) return 0;` guards -- not the one word the polarity note above
would suggest. MSVC plants the shared `li r3,0 ; blr` immediately after the
FIRST test, inverts that test to `bne-` to jump over it, and makes the other
two guards branch BACKWARD into it: same instructions, same 56 bytes, twelve
words displaced. Nesting the positive path and putting the single
`return 0` last is 14 of 14. **The tell is which DIRECTION the guards
branch** -- all forward to a common tail means the failure path is written
last.

The same thing one level up: `sub_822CEE08` written `if (n == 0) return 0;`
first lays a SECOND copy of the zero return after the loop's break test (72
bytes, five wrong words); writing the interesting path first turns that test
into a backward `beq+` into the single zero return already there (64 bytes,
16 of 16).

**`||` versus a sequence of `if`s, seen from the OUTER side.** On
`sub_8219FCD8`, `if (a) return 0; if (b) return 0; return 1;` does two things
at once: it plants a private `li r3,0 ; blr` after the first test instead of
sharing the one at the end, AND it turns the final `return 1` into a
branchless `cntlzw`/`rlwinm`. 12 of 27. `if (a || b) return 0; return 1;`
gives both guards the same forward exit and keeps the tail branchy: 27 of 27.
The `sub_8287E440` note above wants `||` for the INNER predicate; this is the
same operator deciding the outer shape.

**A materialised-then-masked bool is an inlined helper.** A `li` pair that
already produces 0 or 1, followed by a redundant `clrlwi ...,24` before the
test, is what a `bool`-returning inlined function leaves behind. A bare
`if (a && b)` branches out of each term and never builds a value at all.

**`mullw` operand order is NOT readable**, unlike `add` and `subf`: `a * b`
and `b * a` are byte-identical. Verified on three multiplies together and on
one alone. Together with the commutative-float result above, that is two of
the three commutative cases settled as unreadable -- only integer `add`
carries information.

**A signed `cmpwi rX,0` on a value that is then DEREFERENCED means the field
is an `int` holding a pointer, not a pointer.** Every pointer null test in
this image is `cmplwi`; no spelling of a pointer comparison produces `cmpwi`.
That one word was the whole of `sub_82631D98`.

**MSVC lays switch case bodies out in SOURCE ORDER, and does not invent
groups.** Measured both ways on `sub_827261D8`: moving one case group costs
12 words, and splitting a two-value group into two identical bodies costs the
same 12. So block order in the image IS source order, and two non-adjacent
case values sharing a block were written as one group.

**The TLS forms, complete.** `lwz rX,0(r13)` plus a bare `li <slot>` is
`__declspec(thread)` in all three shapes: `+ lwzx` READS the variable,
`+ stwx` WRITES it, and `+ add` takes its ADDRESS. The slot is an
IMAGE_REL_PPC_TOCREL14 relocation and never folds with anything.

**A reload inside a LOOP CONDITION is the normal shape, not the CSE-defeat
lever.** `sub_825FEF00` reloads its count for the loop-bottom test while
keeping the entry load live elsewhere, and a plain `while (i < m_count)`
produces exactly that. The lever above is for a reload with NO loop around
it.

**`add` operand order moves with local DECLARATION order, not read order.**
This is the control for the rule above, and it was found by changing nothing
else: in `sub_826C0F50` a character-for-character identical
`return q->items + r;` emits `add r3,r10,r11` with `s32 r` declared before
`NthNode* q`, and `add r3,r11,r10` with the two declarations swapped. It also
explains why the arena stall is out of reach -- both its operands are global
fields, and there are no locals to reorder.

**Two nesting levels of inlining keep a base pointer alive; one does not.**
`sub_82164040` loads from a materialised group base (`addi r11,r3,12`, then
`8(r11)`) and RELOADS a word it tested one instruction earlier from
`20(r3)` -- nothing stores in between, so it is not aliasing, it is two CSE
trees. A flat body folds every base into r3; a single helper taking the group
pointer folds too; only `Pair(&s->g[i], d)` calling `Ptr(&g->tail, d)` keeps
`r3+12` as a value. The same nesting leaves the DEAD `addi r10,r3,32` in
`sub_82703E28`. **When a target computes an address whose result is never
read, look for an inlined helper, and give it two levels.**

**A store with no dependency on the surrounding computation is HOISTED, so
its emitted position does not say where it was written.** On `sub_82154ED8`
the lone integer store is emitted first, before any float work, and must be
written LAST: first gives 2 of 44 with even the component loads misallocated;
last gives the target's exact register assignment. This is a real exception
to "store order is source order", and it costs the whole function.

**Integer and float stores are TWO streams, interleaved one-for-one by
dual-issue scheduling; each stream's internal order is source order.**
`sub_8214CCB8` matched first try by writing all seven integer stores in their
emitted relative order and then all five float stores in theirs. Reading the
merged order back as source order is actively worse -- 1 of 29 against 5 of
29 on `sub_82202B50`.

**A second `cmplwi` on a register just compared is a SIGNEDNESS SPLIT.** MSVC
reuses cr6 from a signed `cmpwi` for a following `!= 0`, because equality
ignores signedness. So a redundant-looking second compare means the source
changed signedness between the two tests. One word, and a reliable
diagnostic: `sub_8264B6F0`, and again in `sub_827C6D08` where compare 1 is
`cmpw` and compares 2 and 3 are `cmplw` on the same pair.

**A named local for a NARROW field read twice changes scheduling.**
`u16 off = d->off1;` used at a compare and an addition is 41 of 53; spelling
`d->off1` at both is 49 of 49 -- it moves the `cmplwi` past three stores,
swaps two load issue orders and changes a register. Companion to the
CSE-defeat lever, reached from a load rather than from a comparison.

**Naming a struct's array member in a local reorders the PROLOGUE.**
`sub_82703E28` spelled `b->data` at its three uses aligns in place and sinks
the vtable store to eleventh; `char* d = o->blk.data;` aligns through a
second register and issues it seventh. Nine spellings of the arithmetic were
identical -- the base pointer was the variable that mattered.

**MSVC bitfields on this target allocate MSB-FIRST.** `struct { u8 a:4;
u8 b:4; }` with `a = 11` emits `rlwimi rD,rS,4,0,27`: the FIRST-declared
member is the high nibble. Measured in both directions with a two-function
probe.

**`add` operand order is readable, the same way `subf` order is.** For
`a + b`, MSVC puts in **rA the operand whose SOURCE READ comes later**, using
the CSE representative's position rather than the emitted schedule. Measured
over 24 probe functions. Two functions with identical instruction schedules
can differ by exactly this and nothing else -- it is the whole of what still
separates `sub_82606EC8` from matching, at 33 of 40 words.

**An inlined hand-written `strcmp` is not the `strcmp` intrinsic.** Inlining
a hand-written body gets a loop-invariant-delta transform -- one incrementing
pointer plus `subf`/`lbzx` -- and scores 4 to 7 of 35. `#include <string.h>`
and a plain call keeps BOTH pointers incrementing: 35 of 35. Companion to the
`strlen` note above.

**A memory round-trip defeats constant folding where a local does not.**
`g.field = 0; ... g.field + need` keeps an `mr` and an `add`: the store is
dead-store-eliminated but its register is forwarded. `int off = 0; ...
off + need` folds to a constant every time, even with a loop in between.
Nine spellings tried; only the memory form survives.

**`clrlwi.` versus `clrlwi` + `cmplwi cr6` is an `/Os` property, not a source
shape.** That one word is the whole reason `sub_827156B8` needs `/O2 /Os`;
nine spellings of the same bitfield test at `/O2` all split it into two
instructions.

**Naming a sub-expression can move loads ACROSS a guard**, which is the
opposite of what the naming lever above usually buys. Writing
`Group* g = &t->groups[group];` pushed the `items` load to the far side of a
`count <= 0` guard and cost 31 of 35 words; spelling `t->groups[group]` out
at both reads puts both loads ahead of it.

**A second exception to "store order is source order":** MSVC will schedule a
store ACROSS other stores to the same object when the offsets are distinct.
An arena's cursor store at offset 12 moves ahead of two swap stores at 0 and
4 despite being written after them in source.

**Store order is source order, EXCEPT across an address computation.**
`sub_826731B0` emits its stores at 16, 6, 0, 8, 12 and all five source
orderings give identical bytes: MSVC schedules a cheap `stfs`/`sth` into the
gap while a `lis`/`addi` is in flight. `sub_825FE880` is the same and is
still unmatched because of it -- its vtable store will not move to the front
in any source order tried, including a real C++ constructor.

### Names are RECOVERABLE for about a hundred functions

`sub_82216918` is in the manifest as **`TtCheckLineOfSight`**, and that name
is not invented. The function pushes the string at 8200BA04 into the
profiler, and that string is its own name.

`tools/profnames.py` recovers 100+ of these -- `TtzCam2Player_update`,
`TtcheckSupport`, `TtSetSurfVel`, `TtzNPCSteering_ApplySteering_hover`,
`TtWatchDog:FreeMem`. Every one of them can be named truthfully instead of
described, and the name says what the function is for before a line of it is
read. README.md gap 4 says "names are invented, not recovered"; for this
population that is no longer true.

**The profiler scope is an inlined six-instruction macro** and recognising it
is most of the work on any `Tt*` function:

```
lwz   r31,0(r13)         the thread block
li    r30,48
lwzx  r10,r30,r31        t_profiler -- the __declspec(thread) READ form
lwz   r3,12(r10)         end
lwz   r9,4(r10)          cursor
cmplw cr6,r9,r3 ; bge-   skip when full
stw   r6,0(r9)           the NAME
mftb  r5                 the time base
stw   r5,4(r9)
addi  r7,r9,12           entries are 12 bytes
stw   r7,4(r10)
```

The same six instructions appear again at the end with `"Et"` at 820074E4 --
end of timer. So a `Tt*` function is: push name, body, push "Et", return.

### Vector copies: three separate wrong answers

`TtCheckLineOfSight` builds two points as 16-byte vectors, and each of the
three obvious ways to write that is wrong in its own way:

* **A struct of four floats aligned to 16 is not enough.** It copies with
  `ld`/`std` -- two 64-bit integer moves -- and no vector register appears.
  Only a genuine `__vector4` emits `lvx128`/`stvx128`. 24 of 76 against 60.
* **Two separate destinations written directly** give twelve `stfs` and no
  copy at all. The target rebuilds ONE scratch slot for both points.
* **A helper RETURNING the vector by value** is what fixes the store order.
  With the scratch and both copies written inline, the two `stvx128` stores
  come out transposed and nothing moves them -- not declaring the results up
  front in either order, and not `/Os`, which is 19 of 76.
  `Vec4 to = MakePoint(a); Vec4 from = MakePoint(b);` is 62 of 62.

### Branch polarity is source order, not a flag

`beq-` jumping AWAY to a zero return means the interesting path is the
fall-through, so it must be written first:

```c
if (p) return Query(p);     /* matches   */
return 0;

if (!p) return 0;           /* does not: polarity inverts */
return Query(p);
```

Two functions in this batch failed on exactly that and matched once the
positive path was written first.

### Store order is source order

Where a function writes several fields, the emitted order is the source
order, even when it is not address order. `sub_82649240` writes 64, 68, then
0; `sub_82548F10` writes 20 then 4, 8, 12, 16; `sub_82202BC8` interleaves an
integer store between the fourth and fifth float stores. Each was written in
the target's own order and matched.


### Flags are a property of the translation unit

For a long time this file said every match was found at one uniform
`/O2 /Gy /GS- /fp:fast`, and offered that uniformity as evidence about how
the title was built. **That was wrong.** Functions listed here as stalls were
never stalls: they match under `/O2 /Os`, and under `/O1`, and not under
`/O2`.

The reason it took so long to see is a bad piece of reasoning that is worth
keeping. `sub_827007E8` was found to match under `/Os` early on, and the idea
was dismissed because "its two nearest neighbours are the identical idiom and
use the other register". Those neighbours are **8.5 KB away**. That is not the
same translation unit and it was never evidence of anything.

The right test is adjacency, because a translation unit is contiguous. That
test is now automated -- `python tools/flagpairs.py` compiles every matched
function at BOTH levels, classifies it, and reports every adjacent pair:

```
178 matched function(s) classified
  /O2 only     83
  /Os only     32
  insensitive  63   <- carries NO evidence, excluded from the pairs

34 informative adjacent pair(s), 34 agreements, 0 disagreements
```

**Excluding the insensitive half is the whole discipline here.** Most small
accessors compile identically at both levels, so a table that counted them
would report near-total agreement whatever the truth was. Only a pair where
BOTH sides are sensitive can confirm or refute anything.

The longest run is the string routines, five functions and four consecutive
agreeing pairs, all `/O2` only:

```
82540728  StrLen         /O2 only
82540750  StrCopy        /O2 only   gap 40
82540770  StrCopyN       /O2 only   gap 32
825408B0  StrCompareN    /O2 only   gap 320
825408F8  StrCompareI    /O2 only   gap 72
82540968  StrCompareNI   /O2 only   gap 112
```

Four more runs have since appeared: `82600A08`-`82600A88`-`82600BB0`-
`82600BD8` (four functions, `/O2`), `82637590`-`82637688`-`826376E0`
(three, `/O2`), `826919A8`-`82691C50`-`82691CF0` (three, `/Os` -- the 2x3
matrix routines), and `82662F20`-`82663260`-`82663370` (`/Os`).

`tools/flagpairs.py` prints any DISAGREEING pair in full and says plainly
that such a pair would mean this claim needs rewriting rather than
defending. There are none.

### Translation-unit context does NOT affect codegen

The obvious next worry was that a function's bytes might depend on what else
is in its file, which would mean matching required reconstructing whole
translation units. It does not. The same function was compiled six ways --
alone, with a companion before it, after it, both sides, and with a
register-hungry function on either side -- and produced **byte-identical code
every time**. Whatever decides register allocation is inside the function.

### The member-function lever

`sub_826C0FC8` produced the right six instructions with `r10` and `r11`
exactly TRANSPOSED, and six free-function shapes -- index, pointer
arithmetic, explicit stride, index in a local, base in a local, unsigned
index -- all gave 2 of 6. Writing it as a member function matched 6/6.

So `this` is not simply the first parameter as far as register allocation is
concerned. **When registers come out transposed and the first argument looks
like an object pointer, try the member form.**

It is a lever, not a rule: the same change was tried on four other
transposed functions and moved none of them.

---

## What still resists

Six remain, down from eleven. Eight of the original list were not stalls at
all -- they wanted `/Os` -- and one more (`sub_822D0BE8`) came down to `x > 0`
against `x != 0`, which compile to different branch conditions for an
unsigned value.

| function | bytes | the free choice |
|---|---|---|
| `82806FD0` | 84 | branch polarity -- `bgtlr` vs `ble-`, a probability decision |
| `826C1480` | 76 | instruction order -- where one store sits among five loads |
| `8215E5B0` | 28 | register assignment across an argument permutation |
| `82600AD0` | 28 | a reloaded field the compiler will not keep in a register |
| `82639C38` | 20 | an extra `mr` to keep the object alive across a float load |
| `827618E8` | 136 | loop rotation; the target keeps counts in callee-saved r30/r31 |

None of them matches at `/O2`, `/O1` or `/O2 /Os`, so the flag explanation is
exhausted for these. `tools/permuter.py` has not moved any of them either.

---

## Two facts about sizes that cost time

**The recorded size can be SHORT.** MSVC appends an unreachable `blr` after a
tail call, and a body computed from *reachable* code does not count it.
`sub_82807B38` is recorded as 16 bytes and its code is 20. `match.py`
reconciles this, bounded by the next known function start and only when the
extra words actually agree.

**A COMDAT is padded**, so trailing nops and zeros are trimmed before
comparison and what was trimmed is reported. Checked against all matches: no
real trailing instruction has ever been eaten.

---

## Out of scope, and how that was found

`sub_82917B88` matched byte for byte and then failed the build with
"falls outside .text". It is in **BINK** -- RAD's prebuilt video codec, which
is executable but is not `.text`, is middleware, and has no business being
decompiled here. `candidates.py` now excludes that range. The match is
correct and is not counted.

---

## Browsing this in objdiff

objdiff compares two OBJECT FILES per unit. This project has neither shape
lying around -- the target is a linked retail image and the base is a COFF
object -- so `tools/objdiff_export.py` synthesizes both as PowerPC ELF
relocatables and writes `objdiff.json`:

```bash
python tools/objdiff_export.py
objdiff-cli report generate -p . -o build/objdiff/report.json
```

Verified end to end against objdiff-cli 3.8.0: it reads the synthesized
objects, decodes them as PowerPC, and reports

```
units       70 total, 56 complete
functions   70 total, 56 matched
code        2008 bytes total, 1332 matched
matched     66.33%   fuzzy 85.51%
```

Two decisions worth knowing:

**The export includes the functions that do NOT match**, from
`src/attempts.txt`. A unit list where every row reads 100% shows nothing; the
near-misses are the reason to open a visual diff at all. They are kept out of
`src/manifest.txt` because that is what `build.py` verifies and a
non-matching row there would break the build.

**The base has its relocations already resolved**, as `build.py` does. A
relocation's address is chosen by the original linker and is not knowable
from source, so emitting the base un-patched would show every `bl` and every
`lis`/`addi` pair as a difference even for a function that verifies
perfectly.

**objdiff will not decode VMX128.** None of the currently matched functions
contain any -- checked, 0 of 325 instructions -- but the engine's vector
maths will not render. `tools/disasm.py` is the reader that knows it.

### The near-misses, as objdiff scores them

| unit | fuzzy match |
|---|---|
| `sub_827C5198 (vcall116)` | 98.0% |
| `sub_828864E0 (vcall_arg_adj)` | 98.0% |
| `sub_827007E8 (set_vtable_827007E8)` | 97.5% |
| `sub_8288A788 (two_vtables)` | 97.1% |
| `sub_828133B8 (two_vtables_b)` | 97.1% |
| `sub_825E35C8 (vcall_global_4)` | 96.7% |
| `sub_825E3598 (vcall_global_2)` | 96.7% |
| `sub_826C1480 (init12)` | 89.5% |
| `sub_82600AD0 (list_insert)` | 71.4% |
| `sub_82639C38 (fadd_fwd)` | 65.8% |
| `sub_827FE808 (and_byte)` | 58.8% |
| `sub_82806FD0 (chunked_at)` | 57.1% |
| `sub_827618E8 (wstr_compare)` | 38.4% |
| `sub_8215E5B0 (arg_shuffle)` | 12.1% |

---

## The permuter

`tools/permuter.py` mutates a source in ways that cannot change what it
computes, compiles each mutation with the real XDK compiler, and scores it
against the retail bytes -- the decomp-permuter idea, sized to this project.

```bash
python tools/permuter.py src/vcall116.cpp 827C5198 --iters 500
python tools/permuter.py --selftest
```

**It validates against a known answer.** `sub_826C0FC8` scores 2/6 as a free
function and 6/6 as a member; `--selftest` requires the permuter to
rediscover that, and it does, in about 8 mutations. A search tool that cannot
find an answer already known by hand has no business reporting a negative.

Mutations: `reorder` (swap adjacent independent statements), `invert` (branch
polarity), `member` (free function to member), `compare` (`x != 0` versus
`x > 0`, which compile to *different branch conditions* for an unsigned
value), `inline` (remove a local that only names a subexpression), `temp`,
`sign`.

**Two bugs it had, both found by validating rather than by running it:**
substituting the parameter name into raw text rewrote `the target's own` in a
COMMENT to `the target'this own`; and converting `sub_826C1480` to a member
silently shadowed the member `f[12]` with its parameter `int f`. Mutations
now rewrite code only, and refuse when a parameter collides with a member.

**What it has not done is crack a single stall.** Six of them are the same
shape -- a chained load where the target REUSES `r11` and we allocate fresh
registers -- and none of the seven mutations reaches register allocation.
That is the honest result, and it says the next mutation to write is one that
changes register pressure rather than statement order.
