-- Classic 1.60 (WoW Forever): 203 spell_script_names rows that bind a retail script to a spell of the 1.60 client and never do anything.
-- Each one failed Validate() (the script needs retail spells the client lacks: retail talents, glyphs, Mixology, TBC/WotLK bosses) or
-- every handler of the script was dropped because it did not match the 1.60 spell data ("did not match dbc effect data"). Together
-- they were the 266 spell script lines of every start log (948952c1, 260). Nothing changes in game: the scripts were never attached.
-- The vanilla behaviour of these spells comes from the 1.60 spell data or from the classic scripts (Forbearance, Weakened Soul,
-- Immolate, Holy Shock, Consecration, Shadowburn, ...). Safe to run again.
DELETE FROM `spell_script_names` WHERE
    (`ScriptName` = 'spell_akilzon_eagle_swoop_primer' AND `spell_id` = 44733) OR
    (`ScriptName` = 'spell_alar_ashtongue_ruse_master' AND `spell_id` = 39555) OR
    (`ScriptName` = 'spell_archimonde_drain_world_tree_dummy' AND `spell_id` = 39142) OR
    (`ScriptName` = 'spell_azhiccar_player_detection' AND `spell_id` = 1217758) OR
    (`ScriptName` = 'spell_black_morass_time_rift_channel_trigger' AND `spell_id` = 31388) OR
    (`ScriptName` = 'spell_darkweaver_syth_summon_elementals' AND `spell_id` = 33595) OR
    (`ScriptName` = 'spell_defender_of_azeroth_death_gate_selector' AND `spell_id` = 316990) OR
    (`ScriptName` = 'spell_dru_abundance' AND `spell_id` = 774) OR
    (`ScriptName` = 'spell_dru_barkskin' AND `spell_id` = 22812) OR
    (`ScriptName` = 'spell_dru_cultivation' AND `spell_id` = 774) OR
    (`ScriptName` = 'spell_dru_elunes_favored' AND `spell_id` = 5487) OR
    (`ScriptName` = 'spell_dru_entangling_roots' AND `spell_id` = 339) OR
    (`ScriptName` = 'spell_dru_flower_walk' AND `spell_id` = 22812) OR
    (`ScriptName` = 'spell_dru_germination' AND `spell_id` = 774) OR
    (`ScriptName` = 'spell_dru_glyph_of_stars' AND `spell_id` = 24858) OR
    (`ScriptName` = 'spell_dru_guardian_of_elune_healing' AND `spell_id` = 22842) OR
    (`ScriptName` = 'spell_dru_inner_peace' AND `spell_id` = 740) OR
    (`ScriptName` = 'spell_dru_moonfire' AND `spell_id` = 8921) OR
    (`ScriptName` = 'spell_dru_omen_of_clarity' AND `spell_id` = 16864) OR
    (`ScriptName` = 'spell_dru_rake' AND `spell_id` = 1822) OR
    (`ScriptName` = 'spell_dru_travel_form' AND `spell_id` = 1066) OR
    (`ScriptName` = 'spell_dru_travel_form_dummy' AND `spell_id` = 783) OR
    (`ScriptName` = 'spell_dru_twin_moonfire' AND `spell_id` = 8921) OR
    (`ScriptName` = 'spell_eastern_kingdoms_duskwither_spire_down' AND `spell_id` = 34452) OR
    (`ScriptName` = 'spell_eastern_kingdoms_duskwither_spire_up' AND `spell_id` = 34448) OR
    (`ScriptName` = 'spell_fel_lord_caza_dies_02' AND `spell_id` = 210103) OR
    (`ScriptName` = 'spell_gen_arena_drink' AND `spell_id` IN (430, 431, 432, 1133, 1135, 1137, 10250, 22734)) OR
    (`ScriptName` = 'spell_gen_feign_death_all_flags_uninteractible' AND `spell_id` = 28559) OR
    (`ScriptName` = 'spell_gen_mixology_bonus' AND `spell_id` IN (673, 2367, 2374, 2378, 3160, 3164, 3166, 3219, 3220, 3222, 3223, 3593, 7844, 8212, 11328, 11334, 11348, 11349, 11390, 11396, 11405, 11406, 11474, 17535, 17537, 17538, 17539, 17626, 17627, 17628, 17629, 21920, 24361, 24363, 26276)) OR
    (`ScriptName` = 'spell_gen_remove_impairing_auras' AND `spell_id` = 20589) OR
    (`ScriptName` = 'spell_gen_set_health_100' AND `spell_id` = 24931) OR
    (`ScriptName` = 'spell_gen_set_health_500' AND `spell_id` = 24959) OR
    (`ScriptName` = 'spell_gen_skinning' AND `spell_id` = 8613) OR
    (`ScriptName` = 'spell_gen_trainer_heal_cooldown' AND `spell_id` = 132334) OR
    (`ScriptName` = 'spell_gen_trigger_exclude_caster_aura_spell' AND `spell_id` = 172) OR
    (`ScriptName` = 'spell_gen_trigger_exclude_target_aura_spell' AND `spell_id` IN (100, 2823, 20484, 20707)) OR
    (`ScriptName` = 'spell_hallow_end_candy_pirate' AND `spell_id` = 24926) OR
    (`ScriptName` = 'spell_hallows_end_powder_blast' AND `spell_id` = 191547) OR
    (`ScriptName` = 'spell_hun_emergency_salve' AND `spell_id` = 5384) OR
    (`ScriptName` = 'spell_hun_latent_poison_trigger' AND `spell_id` = 19434) OR
    (`ScriptName` = 'spell_hun_multi_shot' AND `spell_id` = 2643) OR
    (`ScriptName` = 'spell_hun_posthaste' AND `spell_id` = 781) OR
    (`ScriptName` = 'spell_hun_wilderness_medicine' AND `spell_id` = 136) OR
    (`ScriptName` = 'spell_icecrown_plant_battle_standard' AND `spell_id` = 4338) OR
    (`ScriptName` = 'spell_illidan_remove_parasitic_shadowfiend' AND `spell_id` = 41923) OR
    (`ScriptName` = 'spell_item_fate_rune_of_unsurpassed_vigor' AND `spell_id` = 7434) OR
    (`ScriptName` = 'spell_item_mechanical_dragonling' AND `spell_id` = 23076) OR
    (`ScriptName` = 'spell_lady_vashj_remove_tainted_cores_effect' AND `spell_id` = 39496) OR
    (`ScriptName` = 'spell_mage_arcane_explosion' AND `spell_id` = 1449) OR
    (`ScriptName` = 'spell_mage_cone_of_cold' AND `spell_id` = 120) OR
    (`ScriptName` = 'spell_mage_firestarter' AND `spell_id` IN (133, 11366)) OR
    (`ScriptName` = 'spell_mage_frostbolt' AND `spell_id` = 116) OR
    (`ScriptName` = 'spell_mage_heat_shimmer_remove' AND `spell_id` = 2948) OR
    (`ScriptName` = 'spell_mage_ice_barrier' AND `spell_id` = 11426) OR
    (`ScriptName` = 'spell_mage_incanters_flow' AND `spell_id` = 1463) OR
    (`ScriptName` = 'spell_mage_scald' AND `spell_id` = 2948) OR
    (`ScriptName` = 'spell_mage_scorch' AND `spell_id` = 2948) OR
    (`ScriptName` = 'spell_magtheridon_quake_initial' AND `spell_id` = 30576) OR
    (`ScriptName` = 'spell_magtheridon_shadow_grasp_cube' AND `spell_id` = 30420) OR
    (`ScriptName` = 'spell_midsummer_test_ribbon_pole_channel' AND `spell_id` IN (29705, 29726, 29727)) OR
    (`ScriptName` = 'spell_moroes_garrote_remove' AND `spell_id` = 37870) OR
    (`ScriptName` = 'spell_morogrim_tidewalker_summon_water_globules' AND `spell_id` = 37866) OR
    (`ScriptName` = 'spell_nagrand_fire_bomb_target_summon_effect' AND `spell_id` = 31960) OR
    (`ScriptName` = 'spell_omor_the_unscarred_orbital_strike_target' AND `spell_id` = 35642) OR
    (`ScriptName` = 'spell_omor_the_unscarred_orbital_strike_whip' AND `spell_id` = 32185) OR
    (`ScriptName` = 'spell_pal_blessing_of_kings' AND `spell_id` = 20217) OR
    (`ScriptName` = 'spell_pal_blessing_of_protection' AND `spell_id` = 1022) OR
    (`ScriptName` = 'spell_pal_divine_shield' AND `spell_id` = 642) OR
    (`ScriptName` = 'spell_pal_hand_of_sacrifice' AND `spell_id` = 6940) OR
    (`ScriptName` = 'spell_pal_holy_ritual' AND `spell_id` IN (1022, 6940)) OR
    (`ScriptName` = 'spell_pal_holy_shock_damage_visual' AND `spell_id` = 25912) OR
    (`ScriptName` = 'spell_pal_holy_shock_heal_visual' AND `spell_id` = 25914) OR
    (`ScriptName` = 'spell_pal_lay_on_hands' AND `spell_id` = 633) OR
    (`ScriptName` = 'spell_pathaleon_summon_nether_wraiths' AND `spell_id` = 35284) OR
    (`ScriptName` = 'spell_pri_assured_safety' AND `spell_id` = 17) OR
    (`ScriptName` = 'spell_pri_atonement_effect' AND `spell_id` IN (17, 2061)) OR
    (`ScriptName` = 'spell_pri_crystalline_reflection' AND `spell_id` = 17) OR
    (`ScriptName` = 'spell_pri_dark_indulgence' AND `spell_id` = 8092) OR
    (`ScriptName` = 'spell_pri_empowered_renew_heal' AND `spell_id` = 139) OR
    (`ScriptName` = 'spell_pri_empyreal_blaze_extend' AND `spell_id` = 14914) OR
    (`ScriptName` = 'spell_pri_entropic_rift' AND `spell_id` = 8092) OR
    (`ScriptName` = 'spell_pri_expiation' AND `spell_id` = 8092) OR
    (`ScriptName` = 'spell_pri_holy_word_salvation_cooldown_reduction' AND `spell_id` = 2050) OR
    (`ScriptName` = 'spell_pri_inescapable_torment' AND `spell_id` = 8092) OR
    (`ScriptName` = 'spell_pri_lasting_words_serenity' AND `spell_id` = 2050) OR
    (`ScriptName` = 'spell_pri_levitate' AND `spell_id` = 1706) OR
    (`ScriptName` = 'spell_pri_mind_devourer' AND `spell_id` = 8092) OR
    (`ScriptName` = 'spell_pri_power_word_fortitude' AND `spell_id` = 21562) OR
    (`ScriptName` = 'spell_pri_power_word_shield' AND `spell_id` = 17) OR
    (`ScriptName` = 'spell_pri_prayerful_litany' AND `spell_id` = 596) OR
    (`ScriptName` = 'spell_pri_sanctuary_trigger' AND `spell_id` = 585) OR
    (`ScriptName` = 'spell_pri_schism' AND `spell_id` = 8092) OR
    (`ScriptName` = 'spell_pri_shadow_covenant' AND `spell_id` = 34433) OR
    (`ScriptName` = 'spell_pri_shadow_protection' AND `spell_id` = 27683) OR
    (`ScriptName` = 'spell_pri_twilight_equilibrium_shadow_word_pain' AND `spell_id` = 589) OR
    (`ScriptName` = 'spell_rog_airborne_irritant' AND `spell_id` = 2094) OR
    (`ScriptName` = 'spell_rog_backstab' AND `spell_id` = 53) OR
    (`ScriptName` = 'spell_rog_blackjack' AND `spell_id` IN (2094, 6770)) OR
    (`ScriptName` = 'spell_rog_cloaked_in_shadows' AND `spell_id` = 1856) OR
    (`ScriptName` = 'spell_rog_deadly_poison' AND `spell_id` = 2818) OR
    (`ScriptName` = 'spell_rog_improved_garrote' AND `spell_id` = 1784) OR
    (`ScriptName` = 'spell_rog_improved_garrote_damage' AND `spell_id` = 703) OR
    (`ScriptName` = 'spell_rog_improved_shiv' AND `spell_id` = 5938) OR
    (`ScriptName` = 'spell_rog_leeching_poison' AND `spell_id` IN (2823, 8679)) OR
    (`ScriptName` = 'spell_rog_premeditation' AND `spell_id` = 1784) OR
    (`ScriptName` = 'spell_rog_prey_on_the_weak' AND `spell_id` IN (408, 1833)) OR
    (`ScriptName` = 'spell_rog_rupture' AND `spell_id` = 1943) OR
    (`ScriptName` = 'spell_rog_shadow_focus' AND `spell_id` = 1784) OR
    (`ScriptName` = 'spell_rog_shot_in_the_dark' AND `spell_id` = 1784) OR
    (`ScriptName` = 'spell_rog_soothing_darkness' AND `spell_id` = 1856) OR
    (`ScriptName` = 'spell_rog_stealth' AND `spell_id` = 1784) OR
    (`ScriptName` = 'spell_rog_vanish' AND `spell_id` = 1856) OR
    (`ScriptName` = 'spell_rog_vanish_aura' AND `spell_id` = 11327) OR
    (`ScriptName` = 'spell_serpent_shrine_coilfang_water' AND `spell_id` = 37025) OR
    (`ScriptName` = 'spell_sha_artifact_stormflurry_stormstrike' AND `spell_id` = 17364) OR
    (`ScriptName` = 'spell_sha_deluge' AND `spell_id` IN (1064, 8004)) OR
    (`ScriptName` = 'spell_sha_earth_shock' AND `spell_id` = 8042) OR
    (`ScriptName` = 'spell_sha_spirit_wolf' AND `spell_id` = 2645) OR
    (`ScriptName` = 'spell_sha_stormflurry_stormstrike' AND `spell_id` = 17364) OR
    (`ScriptName` = 'spell_skeram_true_fulfillment' AND `spell_id` = 785) OR
    (`ScriptName` = 'spell_terokkar_cancel_shadowy_disguise' AND `spell_id` = 32780) OR
    (`ScriptName` = 'spell_the_black_stalker_summon_spore_strider' AND `spell_id` = 38756) OR
    (`ScriptName` = 'spell_warl_backdraft' AND `spell_id` = 17962) OR
    (`ScriptName` = 'spell_warl_conflagrate' AND `spell_id` = 17962) OR
    (`ScriptName` = 'spell_warl_deaths_embrace_dots' AND `spell_id` = 980) OR
    (`ScriptName` = 'spell_warl_doom' AND `spell_id` = 603) OR
    (`ScriptName` = 'spell_warl_immolate' AND `spell_id` = 348) OR
    (`ScriptName` = 'spell_warl_roaring_blaze' AND `spell_id` = 17962) OR
    (`ScriptName` = 'spell_warl_seduction' AND `spell_id` = 6358) OR
    (`ScriptName` = 'spell_warl_shadow_bolt' AND `spell_id` = 686) OR
    (`ScriptName` = 'spell_warl_shadowburn' AND `spell_id` = 17877) OR
    (`ScriptName` = 'spell_warl_soul_fire' AND `spell_id` = 6353) OR
    (`ScriptName` = 'spell_warl_volatile_agony' AND `spell_id` = 980) OR
    (`ScriptName` = 'spell_warr_bloodthirst_enrage' AND `spell_id` = 23881) OR
    (`ScriptName` = 'spell_warr_charge' AND `spell_id` = 100) OR
    (`ScriptName` = 'spell_warr_cold_steel_hot_blood_bloodthirst' AND `spell_id` = 23881) OR
    (`ScriptName` = 'spell_warr_deft_experience' AND `spell_id` = 23881) OR
    (`ScriptName` = 'spell_warr_improved_whirlwind_cleave' AND `spell_id` IN (1464, 1715, 23881, 23922)) OR
    (`ScriptName` = 'spell_warr_meat_cleaver_damage_bonus_thunder_clap' AND `spell_id` = 6343) OR
    (`ScriptName` = 'spell_warr_mortal_strike' AND `spell_id` = 12294) OR
    (`ScriptName` = 'spell_warr_overpowering_finish' AND `spell_id` = 7384) OR
    (`ScriptName` = 'spell_warr_shield_block' AND `spell_id` = 2565) OR
    (`ScriptName` = 'spell_warr_thunder_clap' AND `spell_id` = 6343) OR
    (`ScriptName` = 'spell_warr_thunder_clap_rend' AND `spell_id` = 6343) OR
    (`ScriptName` = 'spell_warr_vicious_contempt' AND `spell_id` = 23881) OR
    (`ScriptName` = 'spell_wotf_shared_cd' AND `spell_id` = 7744) OR
    (`ScriptName` = 'spell_xt002_heart_overload_periodic' AND `spell_id` = 62791);
