// =========================================================
// LF_PowerGrid - Placement action for Logic Gate Kits
//
// v4.1: Removed ActionCondition override. The previous override
// used IsPlacingLocal()/GetHologramLocal() which are CLIENT-ONLY
// APIs — on dedicated server they always return false/null,
// silently rejecting every placement attempt.
//
// Vanilla ActionPlaceObject.ActionCondition handles client/server
// correctly, and LFPG_HologramMod already bypasses collision
// (IsColliding() returns false for all LFPG kits), so the
// "small item" validation issue no longer applies.
//
// Constructor, SetupAnimation, GetStanceMask all inherited
// from LFPG_ActionPlaceGeneric.
// =========================================================

class LFPG_ActionPlaceLogicGate : LFPG_ActionPlaceGeneric
{
};
