# The dungeons the tactics tables cover, in batches of five. map = the instance map id; scenario = the Diagnostics/ScenarioDefs.h kDungeons row
# (or kMcBosses) it belongs to; bosses = creature entries in the order of the run (first one = the scenario goal).
BATCHES = {
 1: [
  dict(key='deadmines', map=36, name='The Deadmines', scenario='deadmines_rhahkzor',
       bosses=[644, 642, 1763, 646, 647, 639, 645]),
  dict(key='rfc', map=389, name='Ragefire Chasm', scenario='rfc_oggleflint',
       bosses=[11517, 11520, 11518, 11519]),
  dict(key='wc', map=43, name='Wailing Caverns', scenario='wc_cobrahn',
       bosses=[3669, 3653, 3670, 3671, 3673, 3674, 5775]),
  dict(key='sfk', map=33, name='Shadowfang Keep', scenario='sfk_rethilgore',
       bosses=[3914, 3886, 3887, 4278, 4279, 4274, 3927, 4275]),
  dict(key='bfd', map=48, name='Blackfathom Deeps', scenario='bfd_ghamoora',
       bosses=[4887, 4831, 6243, 12902, 4832, 4830, 4829]),
 ],
 2: [
  dict(key='stockade', map=34, name='The Stockade', scenario='stockade_targorr',
       bosses=[1696, 1666, 1717, 1716, 1663, 1720]),
  dict(key='sm', map=189, name='Scarlet Monastery', scenario='smgraveyard_vishas',
       bosses=[3983, 4543, 3974, 6487, 3975, 3977, 3976, 4542, 3981]),
  dict(key='rfk', map=47, name='Razorfen Kraul', scenario='rfk_aggem',
       bosses=[4424, 4420, 4421, 4422, 4428]),
  dict(key='gnomer', map=90, name='Gnomeregan', scenario='gnomeregan_electrocutioner',
       bosses=[7361, 6228, 7079, 7800, 6235, 6229]),
  dict(key='rfd', map=129, name='Razorfen Downs', scenario='rfd_glutton',
       bosses=[7355, 7356, 7357, 7358, 7354, 8567]),
 ],
 3: [
  dict(key='uldaman', map=70, name='Uldaman', scenario='uldaman_revelosh',
       bosses=[6910, 7228, 7023, 7206, 7291, 4854, 2748]),
  dict(key='zf', map=209, name="Zul'Farrak", scenario='zf_antusul',
       bosses=[8127, 7272, 7271, 7796, 7275, 7795, 7273, 7267, 7797]),
  dict(key='brd', map=230, name='Blackrock Depths', scenario='brd_roccor',
       bosses=[9025, 9018, 9319, 9016, 9017, 9056, 9041, 9042, 9938, 9019, 9156, 9033, 8983, 9027, 9028, 9029, 9030, 9031, 9032, 9034, 9035, 9036, 9037, 9038, 9039, 9040]),
  dict(key='mc', map=409, name='Molten Core (trash)', scenario='mc_garr', bosses=[]),
 ],
}
