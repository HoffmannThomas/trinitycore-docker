-- SoloCraft: disable WoW 12.x content tuning for creature templates that only
-- spawn inside instance maps (instance_template). Content tuning otherwise sets
-- the client-visible level and scaling fields and applies large level-gap penalties.
--
-- Idempotent: safe to run more than once.
-- Excludes entries that also spawn outside instances (shared entries), so
-- open-world behavior is unchanged.
-- Apply to the world database, then restart worldserver.

UPDATE creature_template_difficulty AS d
SET d.ContentTuningID = 0,
    d.LevelScalingDeltaMin = 0,
    d.LevelScalingDeltaMax = 0
WHERE (d.ContentTuningID <> 0 OR d.LevelScalingDeltaMin <> 0 OR d.LevelScalingDeltaMax <> 0)
  AND d.Entry IN (SELECT c.id FROM creature AS c WHERE c.map IN (SELECT map FROM instance_template))
  AND d.Entry NOT IN (SELECT c2.id FROM creature AS c2 WHERE c2.map NOT IN (SELECT map FROM instance_template));