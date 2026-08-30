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

**1322 functions, 55496 bytes.** Verify all of them, plus the reconstructing
build and six negative controls, with one command:

```bash
python tools/verify.py
```

Every match is also a row in `src/manifest.txt`, so `tools/build.py` compiles
it, resolves its relocations against the retail bytes and splices it into
`.text`. Nothing here is a match on `match.py`'s word-comparison alone.

SPLIT: 469 hand-written, 35608 bytes; 818 generated, 8192 bytes; 35 upstream, 11696 bytes.
<!-- the line above is regenerated; edit tools/matched_table.py, not this -->

The two halves are not comparable and the count should never be quoted
without them. The hand-written functions were read off the disassembly, one
at a time, and each one taught something. The generated ones are
single-expression accessors and constant returns that `tools/gen_typeids.py`
and `tools/gen_accessors.py` wrote from each function's own encoding: real
matches, compiled and compared exactly like the others, needed for a link,
and worth having because each pins a field offset -- but the generated half
says far less about how much of this game has been read than the hand-written
half does, however much larger it gets. (The counts live in the SPLIT line
above, which is regenerated; repeating them here is how a document comes to
contradict itself two commits later.)

**The retail build did NOT use one optimisation level everywhere.** 141 of
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
| `82806FD0` | 84 | 220 | `chunked_at.cpp` | - | `/O2 /Os` |
| `8262F5D0` | 136 | 206 | `m_bin_free.cpp` | BinAlloc | `/O2` |
| `82667EE0` | 152 | 180 | `m_vector_reserve.cpp` | VectorGrow | `/O2` |
| `82662EA0` | 124 | 161 | `z2_handle_acquire.cpp` | - | `/O2 /Os` |
| `8215A420` | 64 | 147 | `c_hash_upper.cpp` | - | `/O2` |
| `82600BD8` | 16 | 135 | `global_field.cpp` | - | `/O2` |
| `82667E58` | 136 | 132 | `m_vector_reserve.cpp` | VectorReserve | `/O2` |
| `82806CB0` | 88 | 132 | `z2_child_id_is.cpp` | - | `/O2 /Os` |
| `82806D08` | 20 | 132 | `a_report_badthis.cpp` | - | `/O2` |
| `8217E808` | 76 | 116 | `m_tree_lookup.cpp` | - | `/O2` |
| `821A4628` | 28 | 108 | `ctor_vt.cpp` | - | `/O2` |
| `82663370` | 60 | 105 | `b_release_ref.cpp` | - | `/O2 /Os` |
| `82160880` | 36 | 100 | `a_copy_fields.cpp` | - | `/O2` |
| `8219EB58` | 96 | 84 | `c_bucket_find.cpp` | - | `/O2` |
| `8253FD70` | 28 | 82 | `array_add.cpp` | - | `/O2` |
| `826918F8` | 44 | 82 | `d_basis_identity.cpp` | - | `/O2` |
| `82663260` | 44 | 77 | `m_ref_ctor.cpp` | - | `/O2 /Os` |
| `826A3648` | 52 | 75 | `c_share_static.cpp` | - | `/O2 /Os` |
| `82606EC8` | 160 | 68 | `f_arena_alloc.cpp` | - | `/O2` |
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
| `8215ED28` | 48 | 36 | `b_bounds_at.cpp` | - | `/O2` |
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
| `8215E5B0` | 28 | 26 | `arg_shuffle.cpp` | - | `/O2` |
| `821636A8` | 24 | 26 | `chain5.cpp` | - | `/O2` |
| `822481B0` | 40 | 25 | `m_flag_guard.cpp` | - | `/O2` |
| `82254A88` | 84 | 25 | `f_find_state.cpp` | - | `/O2` |
| `825FE880` | 48 | 25 | `m_ctor_94.cpp` | - | `/O2` |
| `82665388` | 8 | 25 | `a_fwd24.cpp` | - | `/O2` |
| `82666360` | 12 | 25 | `a_fwd24_self.cpp` | - | `/O2` |
| `82677028` | 20 | 25 | `owner_clear.cpp` | ClearAndHandle | `/O2` |
| `82677040` | 20 | 25 | `owner_clear.cpp` | ClearAndHandleOther | `/O2` |
| `82545348` | 48 | 24 | `m_span_out.cpp` | - | `/O2` |
| `82606FD8` | 160 | 24 | `h_arena_twin.cpp` | - | `/O2` |
| `82724A68` | 44 | 24 | `m_tls_reserve.cpp` | - | `/O2` |
| `827FE818` | 136 | 24 | `z2_release_four.cpp` | ReleaseFour | `/O2` |
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
| `82155080` | 204 | 16 | `e_normalize4.cpp` | QuatNormalize | `/O2` |
| `8224E080` | 20 | 16 | `vcall_f8_40.cpp` | - | `/O2` |
| `8252BE30` | 44 | 16 | `g_out_or_err36.cpp` | - | `/O2` |
| `82600A60` | 36 | 16 | `m_list_head.cpp` | - | `/O2` |
| `8260FEB0` | 76 | 16 | `g_find_by_key.cpp` | - | `/O2` |
| `826C0F28` | 40 | 16 | `g_sum_chain.cpp` | - | `/O2` |
| `82724A98` | 28 | 16 | `m_store_ge.cpp` | - | `/O2` |
| `827841D8` | 84 | 16 | `m_release.cpp` | Release | `/O2 /Os` |
| `828864E0` | 20 | 16 | `vcall_arg_adj.cpp` | - | `/O2 /Os` |
| `822E2048` | 28 | 15 | `g_fwd_6args.cpp` | - | `/O2` |
| `825BFFF0` | 236 | 15 | `a3_bitread32.cpp` | - | `/O2` |
| `82703E28` | 124 | 15 | `h_blk_ctor.cpp` | BlkOwnerConstruct | `/O2 /Os` |
| `821FF908` | 168 | 14 | `e_can_use.cpp` | - | `/O2` |
| `822021F8` | 16 | 14 | `stride116.cpp` | - | `/O2` |
| `8220F810` | 72 | 14 | `g_vec3_pick.cpp` | - | `/O2` |
| `822B7F60` | 352 | 14 | `k7_state_enter.cpp` | - | `/O2` |
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
| `821A5350` | 36 | 11 | `m_state_1or2.cpp` | - | `/O2` |
| `821AC2F0` | 72 | 11 | `j_inv_or_clamp.cpp` | - | `/O2` |
| `826C0FC8` | 24 | 11 | `stride24.cpp` | - | `/O2` |
| `827DAC60` | 76 | 11 | `k_find_pair.cpp` | - | `/O2 /Os` |
| `82166FD0` | 16 | 10 | `fwd_vec3.cpp` | - | `/O2` |
| `82167FE0` | 212 | 10 | `j_scale_pair.cpp` | - | `/O2` |
| `821A5270` | 48 | 10 | `m_copy_adjust.cpp` | - | `/O2` |
| `822020B0` | 16 | 10 | `chain2_156.cpp` | - | `/O2` |
| `822165C0` | 128 | 10 | `p2_kind_pos.cpp` | - | `/O2` |
| `822547C8` | 624 | 10 | `k8_apply_state.cpp` | - | `/O2` |
| `82287E80` | 160 | 10 | `p3_slot_search.cpp` | - | `/O2` |
| `8262FE10` | 60 | 10 | `k_short_release.cpp` | - | `/O2` |
| `8262FF90` | 52 | 10 | `k_global_release_tls.cpp` | - | `/O2` |
| `82631D98` | 36 | 10 | `k_bits_or_zero.cpp` | - | `/O2` |
| `821A4FB0` | 20 | 9 | `fwd_global_n.cpp` | - | `/O2` |
| `822CEE08` | 64 | 9 | `k_chunk_at.cpp` | - | `/O2` |
| `8253FD90` | 148 | 9 | `q2_masked_sum.cpp` | - | `/O2` |
| `8253FE28` | 28 | 9 | `zero48.cpp` | - | `/O2` |
| `825FAB98` | 100 | 9 | `p1_find_name.cpp` | - | `/O2` |
| `82603948` | 20 | 9 | `null_call0.cpp` | - | `/O2` |
| `82704688` | 40 | 9 | `m_select2.cpp` | - | `/O2 /Os` |
| `827245E0` | 32 | 9 | `ring_index2.cpp` | - | `/O2` |
| `827C5180` | 20 | 9 | `k_vcall148.cpp` | - | `/O2 /Os` |
| `8214CCB8` | 84 | 8 | `j_reset_state.cpp` | - | `/O2` |
| `8215C9C0` | 96 | 8 | `q6_slot_update.cpp` | - | `/O2` |
| `821EE668` | 24 | 8 | `m_fwd_ctx.cpp` | - | `/O2` |
| `821F5EE0` | 88 | 8 | `q5_kind_in_set.cpp` | IsActiveKind | `/O2` |
| `8225B450` | 44 | 8 | `m_state_2to4.cpp` | - | `/O2` |
| `82265D88` | 40 | 8 | `m_ready_not255.cpp` | - | `/O2` |
| `825BD9B0` | 48 | 8 | `m_read_le32.cpp` | - | `/O2` |
| `825FA9E8` | 176 | 8 | `r3_flush_pending.cpp` | - | `/O2` |
| `825FEFC8` | 132 | 8 | `r1_list_remove.cpp` | - | `/O2` |
| `82639C28` | 16 | 8 | `chain_add48.cpp` | - | `/O2` |
| `82724588` | 56 | 8 | `q4_count_leading.cpp` | - | `/O2` |
| `8279D958` | 20 | 8 | `m_vcall4_f8.cpp` | - | `/O2` |
| `8287E3D0` | 112 | 8 | `q7_iter_advance.cpp` | - | `/O2 /Os` |
| `82151690` | 44 | 7 | `m_max_pair.cpp` | - | `/O2` |
| `82152358` | 100 | 7 | `s5_mtx_row_lengths.cpp` | MtxRowLengths | `/O2` |
| `82156050` | 16 | 7 | `link_node.cpp` | - | `/O2` |
| `82156060` | 52 | 7 | `r5_pop_front.cpp` | - | `/O2` |
| `8215A460` | 80 | 7 | `s5_hash_upper_n.cpp` | - | `/O2` |
| `8215BC20` | 128 | 7 | `t2_scan_slots.cpp` | - | `/O2` |
| `8215E6D0` | 84 | 7 | `s5_find_pair64.cpp` | - | `/O2` |
| `82167A18` | 76 | 7 | `s5_linked_leaf.cpp` | HasLinkedLeaf | `/O2` |
| `8219EBB8` | 84 | 7 | `s5_bucket_free_slot.cpp` | - | `/O2` |
| `8219FCA8` | 48 | 7 | `z4_speed_check_a.cpp` | - | `/O2` |
| `821A5378` | 20 | 7 | `eq2_208.cpp` | - | `/O2` |
| `821AAFA8` | 92 | 7 | `s5_registry_find.cpp` | - | `/O2` |
| `821BCA48` | 44 | 7 | `m_ctor_7zero.cpp` | - | `/O2` |
| `821C77A8` | 28 | 7 | `m_enqueue12.cpp` | Enqueue | `/O2` |
| `821FAE48` | 48 | 7 | `r4_init_const.cpp` | - | `/O2` |
| `8224BCA0` | 88 | 7 | `s5_kind_adjust_call.cpp` | - | `/O2` |
| `822553D8` | 48 | 7 | `z4_speed_check_b.cpp` | - | `/O2` |
| `82265D30` | 20 | 7 | `set0_255.cpp` | - | `/O2` |
| `82540798` | 56 | 7 | `r6_strcat.cpp` | - | `/O2` |
| `82548F10` | 28 | 7 | `zero5_20first.cpp` | - | `/O2` |
| `825492F8` | 100 | 7 | `t1_slot_attach.cpp` | - | `/O2` |
| `825E0118` | 172 | 7 | `t3_init_defaults.cpp` | - | `/O2 /Os` |
| `826378D8` | 56 | 7 | `r7_table_reset.cpp` | - | `/O2` |
| `82724620` | 64 | 7 | `r8_next_used.cpp` | - | `/O2` |
| `82727258` | 16 | 7 | `stride8.cpp` | - | `/O2` |
| `8214CC48` | 16 | 6 | `or_flag.cpp` | - | `/O2` |
| `82155580` | 124 | 6 | `a5_quat_mul_norm.cpp` | - | `/O2` |
| `821581C0` | 180 | 6 | `a1_grid_range16.cpp` | - | `/O2` |
| `8216CDA0` | 40 | 6 | `m_flag_dispatch.cpp` | - | `/O2` |
| `8219FC90` | 24 | 6 | `eq1_2260.cpp` | - | `/O2` |
| `821A5490` | 24 | 6 | `cmp_set.cpp` | - | `/O2` |
| `821A7E58` | 72 | 6 | `u4_slot_register.cpp` | - | `/O2` |
| `821A93C8` | 24 | 6 | `eq1_144_36.cpp` | - | `/O2` |
| `822038C0` | 60 | 6 | `t7_ctor_args5.cpp` | - | `/O2` |
| `822553C0` | 24 | 6 | `eq1_2264.cpp` | - | `/O2` |
| `8225D500` | 56 | 6 | `t5_magic_check.cpp` | - | `/O2` |
| `8225FDD8` | 20 | 6 | `zero3.cpp` | - | `/O2` |
| `82272AA0` | 28 | 6 | `m_first_or_self.cpp` | - | `/O2` |
| `822CEE48` | 64 | 6 | `u2_chunk_at32.cpp` | - | `/O2` |
| `822D2978` | 24 | 6 | `m_upcast_call.cpp` | - | `/O2` |
| `822D40F8` | 32 | 6 | `copy3_68.cpp` | - | `/O2` |
| `822D4118` | 32 | 6 | `copy3_72.cpp` | - | `/O2` |
| `82543F60` | 24 | 6 | `tail_or_zero.cpp` | - | `/O2` |
| `8259C6F0` | 132 | 6 | `a4_bitread24.cpp` | - | `/O2` |
| `825BD930` | 16 | 6 | `bit_test.cpp` | - | `/O2` |
| `825BE440` | 68 | 6 | `u3_init_zero.cpp` | - | `/O2` |
| `825FB3E0` | 180 | 6 | `a2_grid_range15.cpp` | - | `/O2` |
| `826043B0` | 60 | 6 | `t8_current_notify.cpp` | NotifyIfCurrent | `/O2` |
| `82649240` | 20 | 6 | `zero64_68_0.cpp` | - | `/O2` |
| `82697608` | 16 | 6 | `guard_arg3.cpp` | - | `/O2` |
| `826A46C0` | 60 | 6 | `u1_share_tagged2.cpp` | - | `/O2 /Os` |
| `826C1260` | 48 | 6 | `t6_last_flag.cpp` | - | `/O2` |
| `827007F8` | 76 | 6 | `u5_ctor_str.cpp` | - | `/O2 /Os` |
| `82727028` | 20 | 6 | `store_sum.cpp` | - | `/O2` |
| `8276DDC0` | 112 | 6 | `u8_cursor_skip12.cpp` | - | `/O2 /Os` |
| `827827B8` | 40 | 6 | `m_release.cpp` | AddRef | `/O2` |
| `828133B8` | 28 | 6 | `two_vtables_b.cpp` | - | `/O2 /Os` |
| `82831A48` | 196 | 6 | `b1_extent_bits.cpp` | - | `/O2` |
| `82858720` | 316 | 6 | `b4_warp_point.cpp` | WarpPoint | `/O2 /Os` |
| `8288A788` | 28 | 6 | `two_vtables.cpp` | - | `/O2 /Os` |
| `8214C778` | 16 | 5 | `b5_set_field36.cpp` | - | `/O2` |
| `8214C788` | 20 | 5 | `b8_set_pair_2c28.cpp` | - | `/O2` |
| `8215D038` | 32 | 5 | `c3_path_join.cpp` | - | `/O2` |
| `8215F268` | 28 | 5 | `c2_fwd_callback.cpp` | - | `/O2` |
| `8216C240` | 152 | 5 | `g5_chunked_bit.cpp` | - | `/O2` |
| `8216E778` | 116 | 5 | `f5_relocate_chains.cpp` | - | `/O2` |
| `8219E748` | 144 | 5 | `g2_ctor_three_vec.cpp` | - | `/O2` |
| `821A5328` | 40 | 5 | `c7_ready_flag.cpp` | - | `/O2` |
| `821A6B38` | 16 | 5 | `b6_slot_at83.cpp` | - | `/O2` |
| `821F6C40` | 156 | 5 | `g7_release_slot.cpp` | ReleaseSlot | `/O2` |
| `82202BC8` | 28 | 5 | `store_floats.cpp` | - | `/O2` |
| `8224DBB0` | 40 | 5 | `c8_bits_16_19.cpp` | IsAvailable | `/O2` |
| `8224DF58` | 24 | 5 | `ctor_vt2.cpp` | - | `/O2` |
| `82250B88` | 24 | 5 | `eq0_stride16.cpp` | - | `/O2` |
| `8225F168` | 56 | 5 | `d5_kind_notify.cpp` | - | `/O2` |
| `8225FAC0` | 56 | 5 | `d6_job_ready.cpp` | - | `/O2` |
| `822D0BE8` | 32 | 5 | `deref_or_zero.cpp` | - | `/O2` |
| `822DA8B0` | 284 | 5 | `h4_state_copy.cpp` | - | `/O2` |
| `822DD9F8` | 228 | 5 | `h6_find_entry.cpp` | - | `/O2` |
| `8252D9A0` | 36 | 5 | `c4_fwd_global6.cpp` | - | `/O2` |
| `8252F5D8` | 56 | 5 | `d7_block_info.cpp` | - | `/O2` |
| `8253F5D8` | 192 | 5 | `h5_dsp_ctor.cpp` | ??0DspModule@@QAA@XZ | `/O2` |
| `825478F8` | 116 | 5 | `f6_list_find_key.cpp` | - | `/O2` |
| `8259C778` | 104 | 5 | `f1_bitread16.cpp` | - | `/O2` |
| `825A36C0` | 128 | 5 | `f7_rate_from_scale.cpp` | - | `/O2` |
| `825CB000` | 72 | 5 | `k1_round_pct.cpp` | - | `/O2 /Os` |
| `825E3598` | 24 | 5 | `vcall_global_2.cpp` | - | `/O2 /Os` |
| `825E35C8` | 24 | 5 | `vcall_global_4.cpp` | - | `/O2 /Os` |
| `825E35E0` | 40 | 5 | `d1_vcall_global_6.cpp` | - | `/O2 /Os` |
| `825E41D8` | 16 | 5 | `zero2.cpp` | - | `/O2` |
| `825FA780` | 20 | 5 | `c1_socket_wrap.cpp` | - | `/O2` |
| `825FAB10` | 136 | 5 | `f8_slot_index.cpp` | SlotIndex | `/O2` |
| `825FAC00` | 112 | 5 | `f3_pack_three_fields.cpp` | SlotSet | `/O2` |
| `826009D8` | 48 | 5 | `d3_pool_pop_free.cpp` | - | `/O2` |
| `8261AB90` | 124 | 5 | `y1_release_each.cpp` | ?ReleaseEach@@YAXPAUList8261AB90@@@Z | `/O2` |
| `82698EE8` | 104 | 5 | `f2_pair_lookup.cpp` | - | `/O2` |
| `826B26A8` | 76 | 5 | `k2_field_scaled.cpp` | - | `/O2 /Os` |
| `826C1470` | 8 | 5 | `b7_inner_at40.cpp` | - | `/O2` |
| `826DD4A0` | 96 | 5 | `k4_slot_remove.cpp` | ?SlotRemove@@YAXPAUSlotOwner@@PAUSlotItem@@@Z | `/O2` |
| `827103D8` | 52 | 5 | `d4_handler_or_default.cpp` | - | `/O2 /Os` |
| `827245C0` | 28 | 5 | `ring_index.cpp` | - | `/O2` |
| `8272CB68` | 16 | 5 | `load_global_store.cpp` | - | `/O2` |
| `82761AD0` | 36 | 5 | `c5_flag_pair.cpp` | MarkDirty2 | `/O2 /Os` |
| `82761AF8` | 36 | 5 | `c5_flag_pair.cpp` | MarkDirty8 | `/O2 /Os` |
| `82772FC0` | 152 | 5 | `g6_cursor_step.cpp` | CursorStep | `/O2 /Os` |
| `82784DE0` | 68 | 5 | `d8_record_clear.cpp` | - | `/O2` |
| `82784F90` | 140 | 5 | `g1_ctor_two_vt.cpp` | ConstructItem | `/O2 /Os` |
| `82790710` | 260 | 5 | `h7_read_varint.cpp` | ?ReadVarint@@YAIPAUVarintCtx@@IPAI@Z | `/O2 /Os` |
| `827C4FB0` | 24 | 5 | `ptr_or_null.cpp` | - | `/O2` |
| `827FE808` | 16 | 5 | `and_byte.cpp` | - | `/O2 /Os` |
| `827FEE48` | 44 | 5 | `d2_link_front_flag.cpp` | - | `/O2 /Os` |
| `8280D210` | 112 | 5 | `f4_skip_free_slots.cpp` | - | `/O2 /Os` |
| `8282E258` | 92 | 5 | `k3_delta_copy.cpp` | - | `/O2` |
| `82836078` | 324 | 5 | `h8_rotate_triples.cpp` | - | `/O2` |
| `8214FDE8` | 84 | 4 | `l13_add_scaled.cpp` | Accumulate | `/O2` |
| `82150E18` | 128 | 4 | `n11_setup_or_uniform.cpp` | - | `/O2` |
| `82158850` | 124 | 4 | `m38_pair_find_or_add.cpp` | - | `/O2` |
| `8215BCF0` | 80 | 4 | `l14_table_lookup.cpp` | - | `/O2` |
| `821838E8` | 16 | 4 | `h3_fwd_global_arg.cpp` | - | `/O2` |
| `8219B9A0` | 16 | 4 | `h2_fwd_two_fields.cpp` | - | `/O2` |
| `8219ED88` | 84 | 4 | `l9_deep_ready_a.cpp` | IsReadyKind7 | `/O2` |
| `821A5390` | 40 | 4 | `n7_state2_or_flag.cpp` | - | `/O2` |
| `821A93E0` | 24 | 4 | `l7_sub_kind2.cpp` | - | `/O2` |
| `821F0108` | 52 | 4 | `w5_3guard2tail.cpp` | - | `/O2` |
| `821F6B70` | 208 | 4 | `z3_set_state.cpp` | - | `/O2` |
| `821F7B18` | 40 | 4 | `n8_reset_two_floats.cpp` | - | `/O2` |
| `821FC180` | 128 | 4 | `n18_flag_move_notify.cpp` | - | `/O2` |
| `821FE858` | 24 | 4 | `l8_store_then_vcall0.cpp` | - | `/O2` |
| `821FF818` | 64 | 4 | `w5_chain4null.cpp` | - | `/O2` |
| `82203890` | 16 | 4 | `h1_inner_bit0.cpp` | - | `/O2` |
| `822152C0` | 96 | 4 | `m29_set_frame_axes.cpp` | - | `/O2` |
| `82218FB0` | 36 | 4 | `n1_submit12.cpp` | - | `/O2` |
| `82226F38` | 48 | 4 | `w4_scan4_clear.cpp` | - | `/O2` |
| `82249E18` | 92 | 4 | `l17_set_sel_pending.cpp` | - | `/O2` |
| `8224BA20` | 84 | 4 | `l10_deep_ready_b.cpp` | IsReadyKind7 | `/O2` |
| `82251AC8` | 24 | 4 | `m20_clear_and_zero64.cpp` | - | `/O2` |
| `82252EF8` | 36 | 4 | `n2_flag_dispatch2.cpp` | - | `/O2` |
| `822598E0` | 72 | 4 | `l11_kind1_not_pending.cpp` | IsKind1AndIdle | `/O2` |
| `822AE7C0` | 32 | 4 | `m24_init_and_notify.cpp` | - | `/O2` |
| `822D2528` | 24 | 4 | `table624.cpp` | - | `/O2` |
| `822D2550` | 24 | 4 | `m21_table_byte.cpp` | - | `/O2` |
| `822D3E60` | 36 | 4 | `n3_arg_kind_1or2.cpp` | - | `/O2` |
| `822E0D80` | 16 | 4 | `k6_swap_forward.cpp` | - | `/O2` |
| `82540838` | 64 | 4 | `z1_strupr.cpp` | - | `/O2` |
| `82542518` | 124 | 4 | `m39_ensure_config.cpp` | - | `/O2` |
| `82543D08` | 52 | 4 | `w5_init52.cpp` | - | `/O2` |
| `82547880` | 116 | 4 | `m32_list_find_key48.cpp` | - | `/O2` |
| `825492B8` | 32 | 4 | `m25_out_or_err37.cpp` | - | `/O2` |
| `8257EC60` | 128 | 4 | `n16_dsp_sink_ctor.cpp` | DspSink | `/O2` |
| `82589FF8` | 52 | 4 | `w5_clear_list.cpp` | - | `/O2` |
| `8258A730` | 44 | 4 | `w4_sentinel_b.cpp` | - | `/O2` |
| `825914B8` | 96 | 4 | `m30_query_ready.cpp` | - | `/O2` |
| `825ACA90` | 68 | 4 | `w5_aca90.cpp` | - | `/O2` |
| `825B9970` | 20 | 4 | `l1_clear_triple_b.cpp` | - | `/O2` |
| `825B9E20` | 20 | 4 | `l2_low3_clear.cpp` | - | `/O2` |
| `825BE3D8` | 36 | 4 | `n4_zero5_ret0.cpp` | - | `/O2` |
| `825DB4C0` | 120 | 4 | `m36_adjust_counts.cpp` | - | `/O2 /Os` |
| `825DB730` | 20 | 4 | `l3_bits300_set.cpp` | - | `/O2 /Os` |
| `825DEB20` | 132 | 4 | `n19_list_ctor_dup.cpp` | ListOwner | `/O2` |
| `825FCCB8` | 44 | 4 | `w5_zero8w.cpp` | - | `/O2` |
| `825FF468` | 36 | 4 | `n5_set_bit_pair.cpp` | - | `/O2` |
| `82600AD0` | 28 | 4 | `list_insert.cpp` | - | `/O2` |
| `8261B1A8` | 100 | 4 | `m31_hash_find_node.cpp` | - | `/O2` |
| `826225E8` | 128 | 4 | `n20_find_by_name.cpp` | - | `/O2` |
| `82627BB8` | 116 | 4 | `m33_pool_free.cpp` | ?PoolFree@@YAXPAUPool@@PAX@Z | `/O2` |
| `82631F30` | 72 | 4 | `n15_slot_clear.cpp` | ClearSlot | `/O2` |
| `82638B48` | 92 | 4 | `l19_physics_system_ctor.cpp` | ??0hkpPhysicsSystem@@QAA@XZ | `/O2` |
| `8264B718` | 32 | 4 | `m26_current_entry_byte.cpp` | - | `/O2` |
| `82674E98` | 84 | 4 | `l12_format_size.cpp` | - | `/O2 /Os` |
| `826770C8` | 20 | 4 | `l4_lock_if_present.cpp` | - | `/O2` |
| `826770F8` | 20 | 4 | `l5_unlock_if_present.cpp` | - | `/O2` |
| `8268D810` | 120 | 4 | `m37_vec_remove.cpp` | ?VecOwnerRemove@@YAXPAUVecOwner@@PAX@Z | `/O2` |
| `826B2618` | 144 | 4 | `n9_desc_equal.cpp` | - | `/O2` |
| `826C6298` | 124 | 4 | `n12_compact_nulls.cpp` | - | `/O2` |
| `826DB0A0` | 124 | 4 | `n13_compact_nulls2.cpp` | - | `/O2` |
| `826F99A0` | 68 | 4 | `l20_shape_collection_ctor.cpp` | ??0hkpShapeCollection@@QAA@IE@Z | `/O2` |
| `82726170` | 28 | 4 | `m23_elem12_addr.cpp` | - | `/O2` |
| `8272CB78` | 36 | 4 | `n6_copy4_bytes.cpp` | - | `/O2` |
| `82761AA8` | 40 | 4 | `w4_and2_flag.cpp` | - | `/O2 /Os` |
| `8277DFF0` | 132 | 4 | `n10_pair_integrate.cpp` | - | `/O2 /Os` |
| `8277F310` | 56 | 4 | `w4_zeroctor.cpp` | - | `/O2` |
| `827865F0` | 24 | 4 | `m22_relative_point_call.cpp` | - | `/O2` |
| `82786608` | 32 | 4 | `m27_relative_rect_call.cpp` | - | `/O2` |
| `82790F80` | 20 | 4 | `l6_vcall0_second_arg.cpp` | - | `/O2 /Os` |
| `82791438` | 128 | 4 | `n14_read_le32.cpp` | ReadEntryLe32 | `/O2 /Os` |
| `8279B6B0` | 224 | 4 | `y2_handler_id.cpp` | - | `/O2` |
| `827A7C98` | 20 | 4 | `store_two.cpp` | - | `/O2` |
| `827B7660` | 84 | 4 | `l16_after_last_sep.cpp` | - | `/O2 /Os` |
| `827DA5E0` | 92 | 4 | `m28_ring_push.cpp` | - | `/O2 /Os` |
| `82151670` | 28 | 3 | `l31_short_not_neg1.cpp` | - | `/O2` |
| `82156728` | 36 | 3 | `m55_push_global_list.cpp` | - | `/O2` |
| `82157C58` | 20 | 3 | `m43_clear_rec516.cpp` | - | `/O2` |
| `82158E50` | 24 | 3 | `l23_memcpy_globals.cpp` | - | `/O2` |
| `8215A278` | 56 | 3 | `m66_count_set_bits.cpp` | - | `/O2` |
| `8215A9F8` | 44 | 3 | `m62_clamp_float.cpp` | - | `/O2` |
| `8215BCB0` | 28 | 3 | `l32_table_key.cpp` | - | `/O2` |
| `821A2258` | 24 | 3 | `l24_deep_not_neg1.cpp` | - | `/O2` |
| `821A4318` | 36 | 3 | `m56_set_five_floats_flag.cpp` | - | `/O2` |
| `821A5160` | 28 | 3 | `m50_copy_vec116.cpp` | - | `/O2` |
| `821F61B8` | 60 | 3 | `m69_pick_code.cpp` | - | `/O2` |
| `821F6250` | 28 | 3 | `m51_prefix_and_format.cpp` | - | `/O2` |
| `821F7AF8` | 32 | 3 | `m54_guard_global_flag.cpp` | - | `/O2` |
| `821FF7B8` | 44 | 3 | `l38_set_deep_float.cpp` | - | `/O2` |
| `822038B0` | 16 | 3 | `w4_bit1_of56.cpp` | - | `/O2` |
| `8223C1F0` | 40 | 3 | `m59_two_flags_dispatch.cpp` | - | `/O2` |
| `822481D8` | 44 | 3 | `l39_post_if_enabled.cpp` | - | `/O2` |
| `8224B698` | 24 | 3 | `l26_upcast_64.cpp` | - | `/O2` |
| `8224CD60` | 24 | 3 | `l27_mark_bits_18.cpp` | - | `/O2` |
| `8224E9F0` | 20 | 3 | `m44_store_two_globals.cpp` | - | `/O2` |
| `8224EA18` | 28 | 3 | `m52_out_two_consts.cpp` | - | `/O2` |
| `82250DB8` | 36 | 3 | `m57_init_two_consts.cpp` | - | `/O2` |
| `82250DE0` | 56 | 3 | `m67_set_identity_quat.cpp` | ?ResetNode@@YAXPAUNode2D@@@Z | `/O2` |
| `82251218` | 56 | 3 | `m68_cached_id_or_default.cpp` | - | `/O2` |
| `82251AB0` | 20 | 3 | `m45_set_name64.cpp` | - | `/O2` |
| `8225D080` | 20 | 3 | `m46_global_chain_call.cpp` | - | `/O2` |
| `82261D20` | 24 | 3 | `l28_clear_and_mark.cpp` | - | `/O2` |
| `82265E90` | 44 | 3 | `l40_bounds_at.cpp` | - | `/O2` |
| `82265EC0` | 68 | 3 | `l47_index_of.cpp` | - | `/O2` |
| `82271898` | 20 | 3 | `m47_guard_count_call.cpp` | - | `/O2` |
| `822AC580` | 28 | 3 | `m53_bind_global_first.cpp` | - | `/O2` |
| `822D2510` | 20 | 3 | `m48_table_elem.cpp` | - | `/O2` |
| `822D91C8` | 20 | 3 | `m49_guard_flag_call.cpp` | - | `/O2` |
| `822DF630` | 52 | 3 | `m63_sum_below_limit.cpp` | - | `/O2` |
| `825409E8` | 64 | 3 | `z1_memcmp_n.cpp` | - | `/O2` |
| `82540B18` | 228 | 3 | `y2_atoi_back.cpp` | - | `/O2` |
| `825476F8` | 64 | 3 | `l46_count_list.cpp` | - | `/O2` |
| `82581448` | 40 | 3 | `m60_wstrcopy_n.cpp` | - | `/O2` |
| `825A39C8` | 372 | 3 | `y2_rate_table.cpp` | - | `/O2` |
| `825ACB20` | 40 | 3 | `m61_clear_and_addref.cpp` | - | `/O2` |
| `825F4740` | 48 | 3 | `w4_sentinel_a.cpp` | - | `/O2 /Os` |
| `82600960` | 60 | 3 | `m70_hash_until_brace.cpp` | - | `/O2` |
| `82606158` | 20 | 3 | `l21_global_pair_first.cpp` | - | `/O2` |
| `8261A3D8` | 12 | 3 | `w4_get12.cpp` | - | `/O2` |
| `8262FE90` | 16 | 3 | `w4_store_g8.cpp` | - | `/O2` |
| `82691B70` | 24 | 3 | `l29_col0_length.cpp` | - | `/O2 /Os` |
| `82691B88` | 24 | 3 | `l30_col1_length.cpp` | - | `/O2 /Os` |
| `82698E08` | 60 | 3 | `l43_store_quad.cpp` | SetRow | `/O2` |
| `826A7950` | 16 | 3 | `m40_untag_plus8.cpp` | - | `/O2` |
| `82706950` | 36 | 3 | `m58_release_if_not_default.cpp` | - | `/O2 /Os` |
| `827249B8` | 68 | 3 | `l48_fill_neg1.cpp` | - | `/O2` |
| `82727138` | 16 | 3 | `m41_elem4_at0.cpp` | - | `/O2` |
| `8276D308` | 48 | 3 | `w4_get_or_null.cpp` | - | `/O2` |
| `82786628` | 60 | 3 | `m71_sync_point_delta.cpp` | - | `/O2` |
| `827882D0` | 16 | 3 | `m42_forward_zeros.cpp` | - | `/O2` |
| `827EE558` | 52 | 3 | `m64_value_or_vcall.cpp` | - | `/O2 /Os` |
| `82807B50` | 44 | 3 | `l41_kind_filter.cpp` | - | `/O2` |
| `8283F298` | 52 | 3 | `m65_clear_eight_slots.cpp` | - | `/O2` |
| `82897128` | 20 | 3 | `l22_release_handle_8.cpp` | - | `/O2` |
| `82103008` | 20 | 2 | `y1_byte_state.cpp` | ?SetByte2902@@YAXPAUByteState@@E@Z | `/O2 /Os` |
| `8215A6F8` | 312 | 2 | `y2_tokenize_into.cpp` | - | `/O2` |
| `8215BCD0` | 32 | 2 | `z4_table_second.cpp` | - | `/O2` |
| `821A9900` | 244 | 2 | `y2_hash_by_kind.cpp` | - | `/O2` |
| `821FA2B8` | 236 | 2 | `y2_locale_known.cpp` | - | `/O2` |
| `82216918` | 304 | 2 | `m_line_of_sight.cpp` | TtCheckLineOfSight | `/O2` |
| `822607F0` | 120 | 2 | `grid_indices.cpp` | - | `/O2` |
| `825407D0` | 100 | 2 | `z1_strcat_n.cpp` | - | `/O2` |
| `825492D8` | 32 | 2 | `z4_out_or_err37_b.cpp` | - | `/O2` |
| `825FC978` | 240 | 2 | `y2_range_lookup.cpp` | RangeIdOf | `/O2` |
| `82606F68` | 108 | 2 | `y1_arena_flip.cpp` | - | `/O2` |
| `82639C38` | 20 | 2 | `fadd_fwd.cpp` | - | `/O2` |
| `826779C8` | 72 | 2 | `y2_clear_slot_150.cpp` | ClearSlot150 | `/O2` |
| `8267ACC0` | 236 | 2 | `m_hkpworld_ctor.cpp` | ??0hkpWorld@@QAA@XZ | `/O2` |
| `826FE5B8` | 16 | 2 | `set_vtable.cpp` | SetVTableD170 | `/O2` |
| `826FE5C8` | 16 | 2 | `set_vtable.cpp` | SetVTableD180 | `/O2` |
| `82713100` | 332 | 2 | `y2_segment_kind.cpp` | - | `/O2 /Os` |
| `82157C08` | 80 | 1 | `y1_clear_block.cpp` | - | `/O2` |
| `8215BCA0` | 12 | 1 | `z4_flag_byte.cpp` | - | `/O2` |
| `8215CCC0` | 240 | 1 | `y2_crc_table.cpp` | - | `/O2` |
| `821ADFC8` | 132 | 1 | `y1_ctor_s32_m60.cpp` | ??0Obj821ADFC8@@QAA@XZ | `/O2` |
| `821AE340` | 132 | 1 | `y1_ctor_m20_s100_b.cpp` | - | `/O2` |
| `821AE3D8` | 132 | 1 | `y1_ctor_m20_s100.cpp` | - | `/O2` |
| `821AE470` | 132 | 1 | `y1_ctor_m20_s100_c.cpp` | - | `/O2` |
| `821AE548` | 132 | 1 | `z3_ctor_inner_20.cpp` | - | `/O2` |
| `821C1D58` | 240 | 1 | `y2_enable_slot.cpp` | - | `/O2` |
| `821D9A78` | 100 | 1 | `y1_ctor_s1340.cpp` | ??0Obj821D9A78@@QAA@XZ | `/O2` |
| `82524E90` | 256 | 1 | `y2_query_info.cpp` | - | `/O2` |
| `825FEF80` | 72 | 1 | `z3_set_contains.cpp` | - | `/O2` |
| `82691928` | 124 | 1 | `y1_mtx23_lerp.cpp` | - | `/O2 /Os` |
| `826DD438` | 104 | 1 | `y1_slot_detach.cpp` | ?SlotDetach@@YAXPAUDOwner@@PAUDItem@@@Z | `/O2` |
| `82712E28` | 88 | 1 | `y1_msg_send.cpp` | - | `/O2 /Os` |
| `8289FA50` | 268 | 1 | `m_mixer_clear.cpp` | - | `/O2` |
| `82103028` | 20 | 0 | `y1_byte_state.cpp` | ?SetByte2901@@YAXPAUByteState@@E@Z | `/O2 /Os` |
| `82103048` | 20 | 0 | `y1_byte_state.cpp` | ?SetByte28FF@@YAXPAUByteState@@E@Z | `/O2 /Os` |
| `82103068` | 20 | 0 | `y1_byte_state.cpp` | ?SetByte28FE@@YAXPAUByteState@@E@Z | `/O2 /Os` |
| `82103088` | 20 | 0 | `y1_byte_state.cpp` | ?SetByte28FD@@YAXPAUByteState@@E@Z | `/O2 /Os` |
| `82103318` | 56 | 0 | `y1_field_state.cpp` | ?SetField1@@YAXPAUFieldState@@I@Z | `/O2 /Os` |
| `82103358` | 56 | 0 | `y1_field_state.cpp` | ?SetField2@@YAXPAUFieldState@@I@Z | `/O2 /Os` |
| `82103940` | 144 | 0 | `y1_fmt_toggle0.cpp` | - | `/O2 /Os` |
| `821039D8` | 144 | 0 | `y1_fmt_toggle1.cpp` | - | `/O2 /Os` |
| `82103A70` | 144 | 0 | `y1_fmt_toggle2.cpp` | - | `/O2 /Os` |
| `82103C98` | 36 | 0 | `y1_float_state.cpp` | ?SetFloat29CC@@YAXPAUFloatState@@I@Z | `/O2 /Os` |
| `82103CC8` | 36 | 0 | `y1_float_state.cpp` | ?SetFloat29C4@@YAXPAUFloatState@@I@Z | `/O2 /Os` |
| `82103CF8` | 28 | 0 | `y1_float_state.cpp` | ?SetFloat29D0@@YAXPAUFloatState@@I@Z | `/O2 /Os` |
| `82103D20` | 36 | 0 | `y1_float_state.cpp` | ?SetFloat29C8@@YAXPAUFloatState@@I@Z | `/O2 /Os` |
| `821F61F8` | 84 | 0 | `y1_dispatch_notify.cpp` | - | `/O2` |
| `822306D8` | 344 | 0 | `y2_bind_lazy_six.cpp` | - | `/O2` |
| `82232420` | 324 | 0 | `y2_bind_lazy_five.cpp` | - | `/O2` |
| `8224BCF8` | 20 | 0 | `z4_vcall6_field12.cpp` | - | `/O2` |
| `82578558` | 200 | 0 | `y2_wave_format_ok.cpp` | CheckWaveFormat | `/O2` |
| `82583860` | 216 | 0 | `y2_query_info3.cpp` | - | `/O2` |
| `825E45E8` | 136 | 0 | `y1_attach_flag.cpp` | - | `/O2 /Os` |
| `82636A58` | 88 | 0 | `y1_set_ref.cpp` | - | `/O2` |
| `826491E0` | 96 | 0 | `y1_deleting_dtor.cpp` | - | `/O2` |
| `82663320` | 80 | 0 | `y1_release_delete.cpp` | - | `/O2 /Os` |
| `82696938` | 116 | 0 | `y1_count_children.cpp` | - | `/O2` |
| `826969B8` | 140 | 0 | `y1_pack_child.cpp` | - | `/O2` |
| `82696A60` | 108 | 0 | `y1_bind_child.cpp` | - | `/O2` |
| *(818 generated)* | 8192 | - | `vt_typeid_*`, `vt_const_*`, `vt_acc_*` | one expression each | `/O2` |
| *(35 upstream)* | 11696 | - | `thirdparty/ogg_vorbis/` | libogg 1.1.3 + libvorbis 1.2.0, obtained not recovered | `/O2` |
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
| a trailing `clrlwi rD,rS,24` on the RESULT | the return type is `bool`, not `u8`. See below -- this one reads backwards |

**A trailing `clrlwi r3,r11,24` means `bool`, and that is the opposite of
what it looks like.** The mask says "keep the low byte", so it reads as a
byte-width return, and `u8` is the natural guess. It is wrong. `u8`, `char`
and `int` returns all let MSVC compute the 0/1 directly into r3 and stop --
one instruction shorter, and no mask. Only `bool` normalises, and it is the
normalisation that forces the value into a scratch register first, so there
is something to normalise from.

So the register allocation that looks like the stall is a CONSEQUENCE of the
return type, not a cause, and no amount of reordering reaches it. `u8` and
`bool` are the same width and carry the same values here, and they are not
the same code.

Measured on `sub_821A5350`: sixteen shapes at both optimisation levels.
Every `bool` return is 9 of 9 at both levels -- member function, free
function, and an inlined `bool`-returning helper alike. Every `u8`, `char`
or `int` return is 4 of 9 at both. Branchy spellings (two `if`s, a `switch`,
an int accumulator) score 2 or 3 of 9 and are not the shape at all. The
optimisation level carries no information for this function.

This is the companion to the note further down that a materialised-then-
masked bool indicates an inlined helper: the mask means `bool` SOMEWHERE,
and the first place to look is the return type of the function in hand.

**When the value is a BIT, the tell is the ROTATE AMOUNT, not a trailing
mask.** Found on `sub_8224DBB0`, and it is the same rule reached from the
other end. Written inline -- as `flags & 0x10000`, as `(flags >> 16) & 1`,
or as a one-bit bitfield -- MSVC tests the bit IN PLACE and emits

    rlwinm r10,r11,0,15,15        no rotate; the bit stays where it is

which scores 8 of 10 with every branch, `li` and `cmplwi` already correct. A
`bool`-returning accessor has to normalise the bit to 0 or 1, and the rotate
does it for free:

    rlwinm r10,r11,16,31,31       rotated down to bit 31

which is 10 of 10. So there is no redundant `clrlwi` to notice here -- the
normalisation is absorbed into the rotate, and a rotate amount that lands
the bit at 31 is the signature.

What the bytes do NOT decide is how the accessor is spelled: a mask, a
shift-and-mask, and a one-bit bitfield all compile identically. So a match
of this shape asserts a `bool` return; it asserts no bitfield layout, and a
source that claims one is claiming more than was measured.

**Naming a value in a local can move the CONDITION REGISTER FIELD.** From
`sub_825E35E0`. Reading a global three times -- writing `g_singleton` out at
each use -- gives 8 of 8; hoisting it into `GT6* g = g_singleton;` first
gives 6 of 8, and the difference is not a register but which CR field the
compare lands in, `cr6` against `cr0`.

The flag axis cannot reach this: all 72 combinations `flagsweep.py` tries
score 6 of 8, and the two levels fail in DIFFERENT places -- `/O2` gets
`cr6` right but gives the vtable slot a fresh r10, `/Os` reuses r11 and
moves the compare to `cr0`. Sixteen source shapes were measured; only
un-naming the global reaches 8 of 8.

This is the same lever as `a_vcall4_or_neg1` with a different symptom.
There, repeating the read produced a pointless `mr`; here the global's value
already arrives in r3, so there is no `mr` to produce and the pressure comes
out in the CR field instead. **A difference in CR field is therefore worth
treating as a naming question, not a comparison question.**

**Taking a member's ADDRESS stops MSVC hoisting a later load across the
store.** From `sub_827FEE48`. Two constant offsets off one base provably
cannot alias, so MSVC freely moves a load of `o->flags` above a store to
`o->head`, and every ordinary spelling sits at 9 of 11 with exactly that
transposition. 28 of the 72 flag combinations reproduce the same 9 of 11,
`/Ou` included, so again this is not the level.

Four spellings reach 11 of 11 and they are all one change -- introduce a
pointer TO the member and store through it:

    LNode** pp = &o->head;  *pp = n;      // or the local declared up front,
                                          // or an LNode*& reference,
                                          // or a static helper taking LNode**

Nine other axes were ruled out and are worth not re-trying: `volatile`
flags, signed flags, member vs free function, an inlined helper on either
half, a nested sub-struct, `1 << 10` spelled out, and a named flags local.
All nine stay at 9 of 11.

**A constant folded into an index is NOT evidence that the constant was in
the source.** When a member array's stride is applied by a shift or `mulli`,
MSVC folds the array's BYTE OFFSET into the index instead of adding it to
the base afterwards. `sub_821A6B38` and `sub_82858720` show it
independently: arrays at byte offsets 332 and 144 compile to `(i + 83) * 4`
and `(t + 6) * 24`. So an `addi` of 83 before a scale does not mean anyone
wrote 83 -- it means the array starts at 332 and the elements are 4 bytes.
Reading it the other way invents a constant and then looks for somewhere to
put it.

**`lwzx` operand order is not uniform WITHIN one retail function**, so
"pick the right source convention" cannot always work. `sub_826377B0` sits
at 69 of 74 with every instruction and every register correct; the five
differences are all the rA/rB order of an indexed address, which addresses
the same location either way. The reason it cannot be fixed is that retail
itself is inconsistent: `slots[i].key` is base-first at the loop peel
(`82637824`) and index-first at the loop bottom (`826378C0`). No single
spelling in one translation unit produces both.

Six shapes were measured and the SAME five words differ in every one:
pointer arithmetic (67 of 74), declaration order (69), the two guards as one
`&&` (69), `while` plus `goto` (69), and an inlined `At()` accessor used
partially and then everywhere (69). A free function scores 67 with all seven
inverted; the member form flips exactly two into agreement, which is the
best available. `flagsweep.py`'s 72 combinations top out at 69 on plain
`/O2`. This is the counter-example to the operand-order lever, and it means
a residue of two to five indexed-address words is sometimes the floor.

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

### Levers that cracked a recorded stall

Six functions in `src/attempts.txt` came out after being written down as
stalls, and three of them had a MECHANISM recorded saying why they could
not. Those three are the reason this section exists: the measurement was
right every time and the conclusion drawn from it was wrong every time, in
the same direction -- an observation about why the compiler did something
became a claim that nothing could be done about it.

**A NAMED const-qualified local view breaks a CSE tie.** `sub_82667EE0`
(`VectorGrow`) was one word short on `mullw`'s operand order, which is
decided by which read of `v->count` becomes the CSE representative. Writing

    const ReserveVector* c = v;

and reading the second occurrence through `c` gives 32 of 32. The local must
be **named**: an inline cast and an inlined `const` accessor are both folded
back to the same value number and change nothing. A `const&`, a base-class
pointer, a different struct at the same offset, a union view and an `s32*`
view all work as well.

Its LIMIT, measured on the arena twins `sub_82606EC8`/`sub_82606FD8`, which
it does NOT crack: the view has to name a field reached through a pointer
parameter. It cannot touch a global's `lis`/`addi` address expression --
which is the same reason declaration order cannot reach those two either.

**A load's position relative to a store it might alias is SOURCE ORDER.**
MSVC will not hoist such a load, so the order is readable straight off the
listing. `sub_82600AD0` (`list_insert`) needs

    node->next = head->next;      // the load happens here
    node->prev = head;

and the stores then come out in the OPPOSITE order from the source, because
the `prev` store is the one instruction available to cover the load's
latency. This is the third exception to "store order is source order", and
it is the useful one, because it says when to expect the rule to invert.
`sub_8259C6F0` is the same lever from the other side: it writes a bit
position back AFTER three `lbz`s, and putting the store first shifts the
whole schedule (7 of 32 against 33 of 33).

Its limit, from `sub_826C1480`: incoming STACK-PARAMETER loads are not
pinned this way, so no source read order reaches them.

**A default return materialised ABOVE a guard is a single return through a
zero-initialised accumulator, not an early return.** `sub_82806FD0` was
recorded as a branch-probability call, `bgtlr` against `ble-`. It is not.

    void* r = 0;
    if (i <= total) r = ...;
    return r;

An early `return 0;` puts `li r3,0` in the entry block, which clobbers r3 as
the scratch for the address arithmetic, forces an `mr` and defers the base
load: eight of the eleven wrong words were downstream of that one
instruction. 21 of 21 at `/O2 /Os`.

`sub_825BFFF0` is the converse and shows the shape does not always go that
way: there an initialiser before the `if` is the right length but MSVC
hoists `li r3,-1` into the entry block and renames every register after it
(25 of 59), while writing the first guard as a jump INTO the read leaves the
assignment in the second block where the image has it (57 of 57). **No
`if`/`else` spelling expresses "the failure value is set on one path only";
the position of the materialisation is the thing to control.**

**`fadds` operand order IS source-readable, and an earlier note here was too
broad.** The existing claim that commutative float operand order carries no
information is about `a * b` against `b * a` -- the SAME two values in either
order -- and that stands. It does not cover which of two DIFFERENT values
lands in the A slot, and that one follows the ordinary `add` read-order rule.
On `sub_821AEC78`, declaring `f32 v = *p;` before `m` puts the other operand
in rA in both adds; spelling `*p` out at each use moves the CSE
representative after `m` and both come out with `*p` in rA. Worth separating,
because the broad reading of the old note says not to try.

**`/fp:fast` reassociation is triggered by a NAMED LOCAL too, not just a
literal.** The note above is written about `a * C * b` with `C` a constant;
naming `f32 k = 60.0f;` and writing `dt * rate * k` reassociates to
`(dt * k) * rate` and loses the first multiply just as hard.

**The AND-mask index lever works on `stwx` as well as `lwzx`.**
`sub_826DD4A0` was 23 of 24 with the STORE's operand order inverted;
`o->items[IndexOf(o, it) & 0x3FFFFFFF] = 0;` is 24 of 24. Same mechanism and
same invisibility -- the mask is absorbed into the `rlwinm` the `* 4` already
needed. (The index is negative on one path there and wraps to the same
address, so the mask changes nothing the function computes.)

**An AND-mask on an index flips `lwzx` operand order, and the mask is
invisible.** `sub_8215ED28`, 12 of 12, from `items[i & 0x3FFFFFFF]`. MSVC
matches `base + (index << scale)` as an addressing mode and puts the base in
rA; a masked index misses that pattern and falls back to a generic add, which
puts the index in rA. Keeping all 30 low bits is absorbed into the `rlwinm`
that the `* 4` already needed, so the scaling word is byte-identical and
ONLY the load's operand order moves. Nothing else in the function changes,
which is what makes this usable rather than a guess.

**Count the masked bools.** One trailing mask per inlined bool helper, plus
one for a bool return. `sub_821F5EE0` has two masks, so ONE helper, so the
trailing comparisons are further terms of the same `||` chain:
`return IsMoving(d) || d->kind == 1 || d->kind == 6;` -- 22 of 22. The
discriminator: every true exit reaching one shared `li r11,1` is the
short-circuit form, whereas a private `li r3,1 ; blr` is a separate
statement.

**Merging loop exits.** `sub_8217E808`, 16 of 16. Writing the exit test at
the END of each arm rather than as the loop condition merges both tests into
one latch and all three zero-returns into one block planted immediately
after it. The tell is that the exit block sits BETWEEN the latch and the
out-of-line arm.

**`mulli rD,rS,<small constant>` is an `/Os` PROPERTY, not a source shape --
and it is the loudest flag signal found so far.** Ten spellings of the same
indexing were measured on `sub_8280D210`: a subscript with an `int`,
`unsigned` or struct-loaded index, `(char*)e + i * 48`, `e + i`,
`i * sizeof(E)`, the offset in a local, a 12-`int` element, an `__int64`
index, and the whole loop rewritten. **All ten** expand to
`rlwinm`/`add`/`rlwinm` at `/O2`, and **all ten** emit `mulli ...,48` at
`/O2 /Os`.

So a `mulli` by a small constant is worth treating the way a coalesced
register is treated, except that it is far easier to see: it says try the
level before touching the source. (It remains true, as the idiom table says,
that the constant is the element size -- the two readings do not conflict.)

**`/fp:fast` REASSOCIATES `a * C * b`, and parentheses stop it.**
`(float)n * 0.4f * r->scale` already associates left in C, but MSVC sinks
the constant and emits `(scale * n) * 0.4f` -- 26 of 28 on `sub_825A36C0`.
Writing `((float)n * 0.4f) * r->scale` is 28 of 28. Thirteen shapes were
measured, and the rule is about SEPARATION rather than order: every spelling
that makes the first product a separate expression matches -- parentheses, a
temporary, `*=`, an inlined helper -- and every unparenthesised three-factor
chain fails identically whichever order it is written in.

**The address-of lever must be applied at EVERY site, and sometimes needs a
call boundary.** `sub_8216E778` needs `u32* s = &t->nodes; *s = delta + *s;`
at all three update sites; applying it only where the dead `addi` survives
leaves the inner copy missing, at 10 of 28. `sub_825FAC00` is stronger: a
bare `int*` local is not enough there, eleven spellings sit at 16 of 26, and
only an inlined `static void Pack(int* p, int mask, int v)` reaches 28 of 28.

**A list sentinel with NO null check says the head is a whole node, not a
bare link.** `static_cast<Item*>(&r->head)` on a `Link` member cannot be
proven non-null, so it costs `addic.`/`bne-`/`li` -- three words that are not
in the image. Declaring the head as a full `Item` at +260 makes `&r->head` a
plain `addi r10,r3,260` and puts its own `next` at 352, the exact word the
loop starts from. One layout produces both constants, which is what makes it
the right one rather than merely a shorter one.

**At `/Os`, an `if` guard plus a `do/while` is TAIL-MERGED with the loop's
own increment** (80 bytes against 112). The plain rotated `while` is the
shape that survives.

**MSVC's `rlwimi` for a bitfield insert is fully decodable, and the shift is
NOT the field position.** The kept mask names the field's low bit
(`keep = (1 << P) - 1`, so `ME = 31 - P`), but MSVC materialises the ODD PART
of the inserted constant and folds the power of two into the rotate, then
CSEs the odd parts across inserts. In `sub_82700B30` one `li r10,1` serves
the values 8, 32 and 2 at shifts 7, 6 and 7. All five inserts reproduce
exactly once that is read correctly, so an `rlwimi` run is worth decoding
rather than guessing at.

### Constructors, and what a DELETED store proves

Constructors are the densest structural targets in the image -- one match
pins a whole layout -- and they have their own levers, because most of a
constructor is stores and MSVC's dead-store elimination is unusually visible
in them.

**A dead vptr store can pin a BASE-CLASS BOUNDARY.** `sub_8253F5D8` stores
two vtables to +0x00. MSVC deletes the first as dead -- 180 bytes against the
image's 192 -- unless something it cannot analyse sits between them. The
global load IS emitted between the two stores, and a load from a global
cannot be disambiguated from `*(void**)this`, so the store survives. But
that only happens if the read is a BASE member initialiser. DSE failing is
therefore what located the base boundary, at 0x1C. A store the compiler
deletes is evidence about the class shape, not just a missing word.

**Stores emitted before a class's own vptr belong to a BASE of it.** A
constructor stores its vptr before any of its own member initialisers, but
AFTER its base constructors have run. `sub_8253F5D8` writes 0x04, 0x08, 0x0C
and 0x10 ahead of the first vptr, so those four live in a base of the base.
Getting that wrong displaced eleven words.

**A member initialiser and the same assignment in the body are different
code.** `id10 = -1` written in the body runs after the global read, keeps the
-1 live one value longer than the volatile registers allow, and spills to
r31 -- a `std`/`ld` pair, 200 bytes against 192.

**`__lwsync()` from the XDK's `ppcintrinsics.h` is a real DSE barrier**, and
on `sub_82784F90` it is what keeps a repeated four-float store group alive;
writing the group inline through a helper instead loses one store.

**Naming `&member` AFTER a guard rather than before decides whether the base
pointer is formed at all.** On `sub_82772FC0`, `Cursor* c = &p->cursor;`
INSIDE the guarded block is 38 of 38; the identical declaration before the
guard is 6 of 38; and spelling `p->cursor.x` at every use folds everything
into r3 so the base is never formed, at 4 of 38.

**Two independent chains above an `if` are scheduled in source DECLARATION
order.** On `sub_8216C240`, declaring `w` before the bit mask rather than
after was worth twelve words, 25 to 37 of 38, with nothing else changed.

**A comparison emitted far ahead of its branch means the `if` is at the TOP
of the source**, with the body written out once per arm rather than shared.
On the same function that reading took it from 1 of 38 to 25 of 38, and it
is also what produces the record form `divw.` whose CR0 both arms use as the
trip-count test while the cheap `subf` stays duplicated. Hoisting the common
work above the `if` instead gives a plain `divw` and a separate `cmpwi cr6`,
at every one of eight loop spellings.

**Source order of switch arms decides which arm owns a merged tail** -- after
layout order has stopped carrying information. On `sub_82790710` the arms lay
out identically either way, because the decision tree emits its fall-through
case first. But ascending source order keeps the LATER arms' copies of the
shared tail with forward branches (37 of 65), and descending keeps the FIRST
arm's copy with backward branches (65 of 65). Spelling the join out
structurally with `goto` is worse than both, at 10 of 65, because forcing the
join re-allocates every register.

**A NEGATIVE result worth as much as the others: `or` operand order is not
source-readable.** It belongs with `mullw` and the commutative float
operators, not with `add`/`subf`. Sixteen spellings on `sub_8216C240` --
`|=`, both explicit orders, the word or the address in a local either way,
two helpers with swapped parameters, `s32` and `u32` in all combinations --
plus all 72 flag combinations give the identical instruction. That one word
is the entire remaining difference in that function, and it is not reachable.

**Two layouts recovered this way, both recorded in their sources:**

* `sub_822DA8B0` -- a hand-written copy of an 0x88-byte state record, 35
  fields, every type named by its load instruction. The proof that it is
  hand-written rather than an implicit copy is that **0x34 exists and is
  deliberately not copied**: every other 4-byte slot in the range is, and
  0x38 is a byte needing no alignment, so there is no hole to explain it.
* `sub_8253F5D8` -- an audio DSP module constructor. The string
  `"mod_dspi.cpp"` sits at 8205E630, immediately before its two vtables, so
  this one has its ORIGINAL FILENAME. 26 offsets, four circular-list
  sentinels of the form `{this+off, this+off, 0}`, and a 44100.0f at 0xF8.
  `DspBase`'s size is asserted at 0x1C; the outer size is NOT asserted,
  because the last field at 0x110 gives a floor of 0x114 and smallest-
  possible was not measured.

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
181 matched function(s) classified
  /O2 only     85
  /Os only     32
  insensitive  64   <- carries NO evidence, excluded from the pairs

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

**Do not read a list from here.** This section carried one for three
revisions and was wrong by the third, in the way that costs most: it listed
`82806FD0` as "branch polarity -- a probability decision" and `82600AD0` as
"a reloaded field the compiler will not keep in a register", and both are
now matched, neither for the recorded reason. A stale list of stalls does
not merely go out of date; it tells the next reader not to try.

The live figure, at both optimisation levels, with relocated words excluded:

```bash
python tools/sweep.py --attempts
```

`src/attempts.txt` is the list, and each source file carries its own
measurement and what has been ruled out.

**The pattern worth keeping from three revisions of this table.** Of the
functions ever written down here as stalls, the great majority came out
later, and they came out in three ways, in this order of frequency:

1. **The optimisation level.** Whole cohorts of these were never stalls at
   all -- they wanted `/O2 /Os`. `mulli` by a small constant and a
   coalesced destination register are the two loud signatures.
2. **A source shape nobody had tried**, usually one that controls WHERE a
   value is materialised or WHICH read becomes the CSE representative,
   rather than one that reorders statements. See "Levers that cracked a
   recorded stall" above -- three of those six had a recorded mechanism
   explaining why they were unreachable, and the mechanism was right while
   the conclusion was wrong every time.
3. **A layout fact**, most often in a constructor, where a store the
   compiler deletes is evidence about a base-class boundary.

What has NOT yet moved anything: `tools/permuter.py`'s mutations, which do
not reach register allocation. That remains the one worth writing.

**And this section named a matched function as a boundary for two
revisions.** It said `sub_8216C240`'s last word "is an `or` operand order,
which sixteen spellings and 72 flag combinations show is not
source-readable". Both halves of that were true as measured. It came out at
38 of 38 anyway, by applying the AND-mask lever to the **index rather than
the operator** -- a masked index misses MSVC's `base + (index << scale)`
pattern, the expression tree is rebuilt, and the `or` falls out the other
way round. The sixteen spellings had all been spellings of the operator.

This section had already been corrected once for listing solved functions,
and its opening paragraph says so. It went stale again anyway, in the same
direction, three hundred lines from a generated table that had the right
answer the whole time. So the boundaries are not listed here any more.

**`HANDBOOK.md`'s "Still genuinely open" is now the only list**, and
`tools/open_stalls.py --check` reads it on every `verify.py` run: an address
in that list that turns out to be in `src/manifest.txt` is a failing check,
and so is an address no file tracks, a missing heading, or a list that names
nothing. That guard covers a delimited list, not prose — this section is
free to keep naming `8216C240`, `82806FD0` and `82600AD0`, because naming
functions that CAME OUT is the useful half of it. `tools/sweep.py
--attempts` remains the live scoreboard.

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
