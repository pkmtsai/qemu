DEF_HELPER_FLAGS_3(andes_v5_bfo_x, TCG_CALL_NO_RWG, tl, tl, tl, tl)
DEF_HELPER_FLAGS_3(andes_v5_fb_x, TCG_CALL_NO_RWG, tl, tl, tl, tl)
DEF_HELPER_FLAGS_2(andes_nfcvt_bf16_s, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(andes_nfcvt_s_bf16, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_5(andes_vfwcvt_s_bf16, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(andes_vfncvt_bf16_s, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_7(vfpmad_vf, void, ptr, ptr, i64, i64, ptr, env, i32)
DEF_HELPER_6(vln8_v, void, ptr, ptr, tl, i32, env, i32)
DEF_HELPER_6(vln8_v_mask, void, ptr, ptr, tl, i32, env, i32)
DEF_HELPER_6(vd4dots_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vd4dots_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vd4dotu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vd4dotu_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vd4dotsu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vd4dotsu_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vzext_vf2_b, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vzext_vf4_h, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vzext_vf8_w, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vsext_vf2_b, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vsext_vf4_h, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vsext_vf8_w, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_4(vle4_v, void, ptr, tl, env, i32)
DEF_HELPER_5(vfwcvt_f_n_v16, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vfwcvt_f_n_v32, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vfwcvt_f_nu_v16, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vfwcvt_f_nu_v32, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vfwcvt_f_b_v16, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vfwcvt_f_b_v32, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vfwcvt_f_bu_v16, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_5(vfwcvt_f_bu_v32, void, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vqmaccu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vqmaccu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vqmacc_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vqmacc_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vqmaccsu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vqmaccsu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vqmaccu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vqmaccu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vqmacc_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vqmacc_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vqmaccsu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vqmaccsu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vqmaccus_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vqmaccus_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_FLAGS_2(andes_ace, TCG_CALL_NO_RWG, tl, env, tl)
DEF_HELPER_FLAGS_2(andes_v5_hsp_check, TCG_CALL_NO_RWG, void, env, tl)
