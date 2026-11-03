-- "Hardcore - Cursed!" (challenges 617 and 618): the trial aura 943601 ticks into a dummy spell that nothing
-- handled, so no curse ever landed. aura_coa_cursed_trial now drives the curses (mod-coa-challenges,
-- CoA.Challenges.Curses.cpp). The remaining scripts replace curse triggers that use Ascension's unimplemented
-- "trigger random spell" effect, the Recall teleport and the Reflection self-damage.
DELETE FROM `spell_script_names`
WHERE `spell_id` IN (943601, 901809, 901816, 901830, 901864, 901868, 901852, 901853, 901854);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(943601, 'aura_coa_cursed_trial'),
(901809, 'aura_coa_curse_affliction'),
(901816, 'aura_coa_curse_affliction'),
(901830, 'aura_coa_curse_chaos'),
(901864, 'aura_coa_curse_recall'),
(901868, 'aura_coa_curse_whispers'),
(901852, 'aura_coa_curse_reflection'),
(901853, 'aura_coa_curse_reflection'),
(901854, 'aura_coa_curse_reflection');

-- Mending (heal the target you hit) and Reflection (hurt yourself) proc from damage the player deals. Only Major
-- Reflection carries proc flags in the client data (69972: melee, ranged and spell damage done); Mending and the
-- other two Reflections use the same. Paranoia (fear when hit) keeps its taken-damage flags at a 25% chance with
-- an 8 second cooldown instead of fearing on every hit.
DELETE FROM `spell_proc` WHERE `SpellId` IN (901804, 901852, 901853, 901854, 901874);
INSERT INTO `spell_proc` (`SpellId`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `Chance`, `Cooldown`) VALUES
(901804, 69972, 1, 2, 33, 0),
(901852, 69972, 1, 2, 100, 0),
(901853, 69972, 1, 2, 100, 0),
(901854, 69972, 1, 2, 100, 0),
(901874, 139944, 1, 2, 25, 8000);
