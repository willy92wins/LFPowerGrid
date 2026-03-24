// =========================================================
// LF_PowerGrid - Actions (v0.7.37)
//
// v0.7.14: LFPG_FormatFloat hardened against NaN/overflow.
//          OnExecuteServer hardened with null/empty guards.
//
// v0.7.35 (Bloque F): ToggleSource text debounce — prevents
//   scroll menu flicker when net-synced switch state oscillates
//   during client-server roundtrip. 800ms hysteresis window.
//
// v0.7.37 (Bug fix): Sparkplug validation in ToggleSource.
//   - ActionCondition: hide "Turn On" without valid sparkplug
//   - OnExecuteServer: server-side gate before LFPG_ToggleSource
//   Both client and server now enforce sparkplug requirement.
//
// v0.7.48 (Bug 1): GetCursorWorldPosSkipDevices fallback to first
//   device hit position when ALL raycast results are electrical
//   devices. Fixes waypoint placement failure in building interiors.
//
// Per-port scroll actions:
//   Port0..Port6: aim at device with CableReel -> shows each
//                 port with label + connection status.
//   PlaceWaypoint: terrain during session
//   CancelWiring:  terrain during session
//   CutWires:      source with Pliers
//   CutPort0..6:   per-port cut with Pliers
//   ToggleSource:  LFPG generators
//   DebugStatus:   any device with CableReel
// =========================================================

// ---------------------------------------------------------
// Helper: unified raycast cache for cursor-related queries.
//
// v0.7.10: All cursor consumers share a single 50m raycast
// cached per-frame. During active wiring this eliminates
// ~10 redundant rays/s from IsCursorOnDevice overlapping
// with the preview ray. Cost: 1 ray per frame when ANY
// consumer refreshes, 0 when nobody asks.
//
// Consumers:
//   IsCursorOnDevice   — ActionCondition, per-frame, 100ms throttle
//   GetCursorWorldPos  — preview, per-frame (no extra throttle)
//   GetCursorWorldPosSkipDevices — waypoint, per-click
//
// Cache invalidation: tick time comparison. Multiple calls
// in the same simulation tick reuse the same results array.
// IsCursorOnDevice adds its own 100ms throttle on top to
// avoid even reading the cache when the scroll menu doesn't
// need it refreshed.
// ---------------------------------------------------------
class LFPG_ActionRaycast
{
    // ---- Unified ray cache ----
    // One sorted raycast per tick, shared by all consumers.
    // Range matches rendering cull distance (nothing beyond it matters).
    protected static float s_RayCacheTime = -1.0;
    protected static ref array<ref RaycastRVResult> s_RayCacheResults;
    protected static bool  s_RayCacheValid = false;

    // v0.7.10: Minimum refresh interval for the shared ray cache.
    // At 60fps (16.7ms/frame), 30ms means the ray fires every ~2 frames
    // = ~33 rays/s instead of ~60. The preview cursor position is at most
    // 1 frame stale, which is imperceptible since camera movement is smooth.
    // At 30fps the ray still fires every frame (33ms > 30ms budget).
    static const float     RAY_CACHE_TTL_S   = 0.030;   // 30ms ≈ 33Hz

    // ---- IsCursorOnDevice throttle (100ms on top of frame cache) ----
    protected static float s_DeviceCheckTime = -1.0;
    protected static bool  s_DeviceCheckResult = false;
    static const float     DEVICE_CHECK_TTL_S = 0.1; // 100ms

    // ---- Refresh the shared ray cache if stale ----
    // This is the ONLY place DayZPhysics.RaycastRVProxy is called
    // for cursor queries in the entire mod.
    // v0.7.10: TTL-based dedup. Multiple calls within RAY_CACHE_TTL_S
    // reuse the same results. Subsumes per-frame dedup.
    protected static void RefreshRayCache(PlayerBase player)
    {
        float now = GetGame().GetTickTime();

        // Still fresh: reuse cached results
        if (s_RayCacheValid)
        {
            float elapsed = now - s_RayCacheTime;
            if (elapsed >= 0.0 && elapsed < RAY_CACHE_TTL_S)
                return;
        }

        // Stale or first call: fire one ray matching rendering bubble
        vector from = GetGame().GetCurrentCameraPosition();
        vector dir  = GetGame().GetCurrentCameraDirection();
        vector to   = from + dir * LFPG_CULL_DISTANCE_M;

        RaycastRVParams rp = new RaycastRVParams(from, to, player, 0);
        rp.sorted = true;

        s_RayCacheResults = new array<ref RaycastRVResult>;
        DayZPhysics.RaycastRVProxy(rp, s_RayCacheResults);
        s_RayCacheTime  = now;
        s_RayCacheValid = true;
    }

    // ---- IsCursorOnDevice ----
    // Called per-frame by ActionCondition (scroll menu).
    // Own 100ms throttle: avoids refreshing cache when not needed.
    // When wiring IS active, preview already refreshes the cache
    // per-frame via GetCursorWorldPos, so this just reads it free.
    static bool IsCursorOnDevice(PlayerBase player)
    {
        if (!player)
            return false;

        // 100ms throttle — skip entirely if checked recently
        float now = GetGame().GetTickTime();
        float elapsed = now - s_DeviceCheckTime;
        if (s_DeviceCheckTime >= 0.0 && elapsed < DEVICE_CHECK_TTL_S)
        {
            return s_DeviceCheckResult;
        }

        // Refresh shared cache (no-op if same frame as preview)
        RefreshRayCache(player);
        s_DeviceCheckTime = now;

        if (!s_RayCacheResults || s_RayCacheResults.Count() == 0)
        {
            s_DeviceCheckResult = false;
            return false;
        }

        // Check first hit: must be within interact range AND be a device
        ref RaycastRVResult first = s_RayCacheResults.Get(0);
        Object hitObj = first.obj;
        if (!hitObj)
        {
            s_DeviceCheckResult = false;
            return false;
        }

        // Distance check (replaces the old 3m ray range limit)
        // v0.7.10: Uses shared DistSq helper for consistency
        vector camPos = GetGame().GetCurrentCameraPosition();
        if (LFPG_WorldUtil.DistSq(camPos, first.pos) > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
        {
            s_DeviceCheckResult = false;
            return false;
        }

        EntityAI e = EntityAI.Cast(hitObj);
        if (!e)
        {
            s_DeviceCheckResult = false;
            return false;
        }

        s_DeviceCheckResult = LFPG_DeviceAPI.IsElectricDevice(e);
        return s_DeviceCheckResult;
    }

    // ---- GetCursorWorldPos ----
    // Returns first hit position (any object or terrain).
    // Used by preview (per-frame) and anywhere needing raw cursor hit.
    static bool GetCursorWorldPos(PlayerBase player, out vector hitPos)
    {
        hitPos = "0 0 0";
        if (!player)
            return false;

        RefreshRayCache(player);

        if (!s_RayCacheResults || s_RayCacheResults.Count() == 0)
            return false;

        hitPos = s_RayCacheResults.Get(0).pos;
        return true;
    }

    // ---- GetCursorTargetDevice (v0.7.12 — Sprint 2 B1) ----
    // Returns the electrical device under cursor within interact range.
    // Cost: zero extra raycasts (reads from shared cache).
    // Used by WiringClient for snap-to-port and semáforo evaluation.
    // Returns null if no device under cursor or out of range.
    static EntityAI GetCursorTargetDevice(PlayerBase player)
    {
        if (!player)
            return null;

        RefreshRayCache(player);

        if (!s_RayCacheResults || s_RayCacheResults.Count() == 0)
            return null;

        ref RaycastRVResult first = s_RayCacheResults.Get(0);
        Object hitObj = first.obj;
        if (!hitObj)
            return null;

        // Distance check (interact range)
        vector camPos = GetGame().GetCurrentCameraPosition();
        if (LFPG_WorldUtil.DistSq(camPos, first.pos) > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return null;

        EntityAI e = EntityAI.Cast(hitObj);
        if (!e)
            return null;

        if (!LFPG_DeviceAPI.IsElectricDevice(e))
            return null;

        return e;
    }

    // ---- GetCursorTargetDeviceWithProximity (v1.2.1) ----
    // Same as GetCursorTargetDevice but with a proximity sphere fallback
    // when the raycast misses. Used by DeviceInspector to handle models
    // with small Geometry LODs (e.g., Furnace).
    // Does NOT affect wiring precision — WiringClient still uses
    // the strict raycast-only GetCursorTargetDevice.
    static EntityAI GetCursorTargetDeviceWithProximity(PlayerBase player)
    {
        if (!player)
            return null;

        // First try the exact raycast
        EntityAI exact = GetCursorTargetDevice(player);
        if (exact)
            return exact;

        // Fallback: search for devices near the aim point
        RefreshRayCache(player);

        // Get the aim intersection point (where the ray hit the world)
        vector aimPos = "0 0 0";
        bool hasAimPos = false;
        if (s_RayCacheResults && s_RayCacheResults.Count() > 0)
        {
            ref RaycastRVResult aimHit = s_RayCacheResults.Get(0);
            aimPos = aimHit.pos;
            hasAimPos = true;
        }

        // If ray hit nothing, use a point in front of camera
        if (!hasAimPos)
        {
            vector camFrom = GetGame().GetCurrentCameraPosition();
            vector camDir = GetGame().GetCurrentCameraDirection();
            float probeDistM = 3.0;
            aimPos = camFrom + camDir * probeDistM;
        }

        // Sphere search around aim point
        float proxyRadius = 1.5;
        array<Object> nearby = new array<Object>;
        array<CargoBase> proxyCargo = new array<CargoBase>;
        GetGame().GetObjectsAtPosition3D(aimPos, proxyRadius, nearby, proxyCargo);

        // Find the closest electrical device IN FRONT of the camera
        vector camCheck = GetGame().GetCurrentCameraPosition();
        vector camDirCheck = GetGame().GetCurrentCameraDirection();
        float maxSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        float bestDistSq = maxSq;
        EntityAI bestDevice = null;

        int pi;
        for (pi = 0; pi < nearby.Count(); pi = pi + 1)
        {
            Object pObj = nearby[pi];
            if (!pObj)
                continue;

            EntityAI pEntity = EntityAI.Cast(pObj);
            if (!pEntity)
                continue;

            if (!LFPG_DeviceAPI.IsElectricDevice(pEntity))
                continue;

            // Distance from camera check
            vector devPos = pEntity.GetPosition();
            float pDistSq = LFPG_WorldUtil.DistSq(camCheck, devPos);
            if (pDistSq > maxSq)
                continue;

            // Direction check: device must be roughly in front of camera.
            // toDevice · camDir > 0.5 ≈ within ~60° cone.
            vector toDevice = vector.Direction(camCheck, devPos);
            toDevice.Normalize();
            float dotVal = vector.Dot(toDevice, camDirCheck);
            if (dotVal < 0.5)
                continue;

            if (pDistSq < bestDistSq)
            {
                bestDistSq = pDistSq;
                bestDevice = pEntity;
            }
        }

        return bestDevice;
    }

    // ---- GetCursorWorldPosSkipDevices ----
    // v0.7.48 (Bug 1): Fallback to first device hit position when ALL
    // raycast results are electrical devices. Common in building interiors
    // where ceiling lamp mesh is the only object under cursor.
    // The world position is valid for waypoints; we only skip the device
    // to avoid mesh-snap, not to reject the position entirely.
    static bool GetCursorWorldPosSkipDevices(PlayerBase player, out vector hitPos)
    {
        hitPos = "0 0 0";
        if (!player)
            return false;

        RefreshRayCache(player);

        if (!s_RayCacheResults || s_RayCacheResults.Count() == 0)
            return false;

        // v0.7.48: Track first device hit as fallback for interior placement
        vector firstDevicePos = "0 0 0";
        bool hasDeviceFallback = false;

        int i;
        for (i = 0; i < s_RayCacheResults.Count(); i = i + 1)
        {
            ref RaycastRVResult rr = s_RayCacheResults.Get(i);
            Object hitObj = rr.obj;

            // Accept hits with no object (terrain)
            if (!hitObj)
            {
                hitPos = rr.pos;
                return true;
            }

            // Skip electrical devices but remember first device position
            EntityAI e = EntityAI.Cast(hitObj);
            if (e && LFPG_DeviceAPI.IsElectricDevice(e))
            {
                if (!hasDeviceFallback)
                {
                    firstDevicePos = rr.pos;
                    hasDeviceFallback = true;
                }
                continue;
            }

            hitPos = rr.pos;
            return true;
        }

        // v0.7.48: All hits were devices — use first device hit position.
        // Raycast world pos is on the surface behind the device mesh,
        // perfectly valid for waypoint placement.
        if (hasDeviceFallback)
        {
            hitPos = firstDevicePos;
            return true;
        }

        return false;
    }
};

// =========================================================
// PER-PORT ACTION BASE
//
// Each subclass (Port0..Port5) sets m_PortIndex.
// Shows port label + connection status in scroll menu.
// Bidirectional: first click starts session, second finishes.
// WiringClient.Finish handles direction swap if needed.
// =========================================================
class ActionLFPG_PortBase : ActionSingleUseBase 
{
    protected int m_PortIndex;

    void ActionLFPG_PortBase()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "LFPG Port";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !item)
            return false;

        if (!item.IsInherited(LFPG_CableReel))
            return false;

        // Block wiring actions while Sorter panel is open.
        // SetDisabled(true) blocks movement/inventory but NOT
        // CCTCursor actions — they fire every frame on whatever
        // the player's cursor points at behind the UI.
        if (LFPG_SorterView.IsOpen())
            return false;

        if (!target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        EntityAI e = EntityAI.Cast(targetObj);
        if (!e)
            return false;

        if (!LFPG_DeviceAPI.IsElectricDevice(e))
            return false;

        // Check port exists at this index
        int portCount = LFPG_DeviceAPI.GetPortCount(e);
        if (m_PortIndex >= portCount)
            return false;

        int portDir = LFPG_DeviceAPI.GetPortDir(e, m_PortIndex);
        if (portDir < 0)
            return false;

        // Build display text
        string portLabel = LFPG_DeviceAPI.GetPortLabel(e, m_PortIndex);
        string portName  = LFPG_DeviceAPI.GetPortName(e, m_PortIndex);
        string devId     = LFPG_DeviceAPI.GetOrCreateDeviceId(e);

        // Get connection info (client only) - both OUT and IN are 1:1
        string connType = "";
        if (!GetGame().IsDedicatedServer())
        {
            LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
            if (renderer)
            {
                connType = renderer.GetConnectionType(devId, portName, portDir);
            }
        }

        if (connType != "")
        {
            // Port occupied: show Replace (always available for rewiring or cutting)
            m_Text = "Replace " + portLabel + " (" + connType + ")";
        }
        else
        {
            // Port empty: text depends on wiring session state
            bool sessionActive = false;
            if (!GetGame().IsDedicatedServer())
            {
                sessionActive = LFPG_WiringClient.Get().IsActive();
            }

            if (sessionActive)
            {
                m_Text = "Connect to " + portLabel;
            }
            else
            {
                m_Text = "Wire from " + portLabel;
            }
        }

        return true;
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        EntityAI e = EntityAI.Cast(targetObj);
        if (!e)
            return;

        int portDir = LFPG_DeviceAPI.GetPortDir(e, m_PortIndex);
        string portName = LFPG_DeviceAPI.GetPortName(e, m_PortIndex);
        string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(e);

        int low = 0;
        int high = 0;
        e.GetNetworkID(low, high);

        LFPG_WiringClient wc = LFPG_WiringClient.Get();
        PlayerBase localPlayer = PlayerBase.Cast(GetGame().GetPlayer());

        if (!wc.IsActive())
        {
            // No session: START wiring from this port (any direction)
            wc.Start(devId, low, high, portName, portDir);

            LFPG_Diag.ServerEcho("[PortAction] START from " + e.GetType() + " port=" + portName + " dir=" + portDir.ToString());

            if (localPlayer)
            {
                localPlayer.MessageStatus("[LFPG] Wiring from " + e.GetType() + ":" + portName);
            }
        }
        else
        {
            // Session active: FINISH wiring to this port (any direction)
            // WiringClient.Finish handles direction validation + swap
            LFPG_Diag.ServerEcho("[PortAction] FINISH to " + e.GetType() + " port=" + portName + " dir=" + portDir.ToString());

            wc.Finish(devId, low, high, portName, portDir);
        }
    }
};

// ---------------------------------------------------------
// Port subclasses (index 0..6 = max 7 ports per device)
// ---------------------------------------------------------
class ActionLFPG_Port0 : ActionLFPG_PortBase
{
    void ActionLFPG_Port0() { m_PortIndex = 0; }
};

class ActionLFPG_Port1 : ActionLFPG_PortBase
{
    void ActionLFPG_Port1() { m_PortIndex = 1; }
};

class ActionLFPG_Port2 : ActionLFPG_PortBase
{
    void ActionLFPG_Port2() { m_PortIndex = 2; }
};

class ActionLFPG_Port3 : ActionLFPG_PortBase
{
    void ActionLFPG_Port3() { m_PortIndex = 3; }
};

class ActionLFPG_Port4 : ActionLFPG_PortBase
{
    void ActionLFPG_Port4() { m_PortIndex = 4; }
};

class ActionLFPG_Port5 : ActionLFPG_PortBase
{
    void ActionLFPG_Port5() { m_PortIndex = 5; }
};

class ActionLFPG_Port6 : ActionLFPG_PortBase
{
    void ActionLFPG_Port6() { m_PortIndex = 6; }
};

// ---------------------------------------------------------
// PLACE WAYPOINT - during session, on terrain
// ---------------------------------------------------------
class ActionLFPG_PlaceWaypoint : ActionSingleUseBase
{
    void ActionLFPG_PlaceWaypoint()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_ADD_WAYPOINT";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined;
        m_ConditionTarget = new CCTNone;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !item)
            return false;

        if (!item.IsInherited(LFPG_CableReel))
            return false;

        if (GetGame().IsDedicatedServer())
            return true;

        if (!LFPG_WiringClient.Get().IsActive())
            return false;

        // Hide when looking at electrical device (port actions handle that)
        if (LFPG_ActionRaycast.IsCursorOnDevice(player))
            return false;

        return true;
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        LFPG_WiringClient wc = LFPG_WiringClient.Get();
        if (!wc.IsActive())
            return;

        vector hitPos;
        bool hit = LFPG_ActionRaycast.GetCursorWorldPosSkipDevices(player, hitPos);
        if (!hit)
            return;

        wc.AddWaypoint(hitPos);

        int wpCount = wc.GetWaypointCount();
        player.MessageStatus("[LFPG] Waypoint " + wpCount.ToString());
    }
};

// ---------------------------------------------------------
// CANCEL WIRING - during session, on terrain
// ---------------------------------------------------------
class ActionLFPG_CancelWiring : ActionSingleUseBase
{
    void ActionLFPG_CancelWiring()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_CANCEL_WIRING";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined;
        m_ConditionTarget = new CCTNone;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !item)
            return false;

        if (!item.IsInherited(LFPG_CableReel))
            return false;

        if (GetGame().IsDedicatedServer())
            return true;

        if (!LFPG_WiringClient.Get().IsActive())
            return false;

        // v0.7.23 (Bug 7): Removed IsCursorOnDevice check.
        // Cancel must be available even when looking at a device,
        // so the player doesn't have to look at the floor to cancel.
        // Port actions and Cancel coexist in the scroll menu.

        return true;
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        LFPG_WiringClient.Get().Cancel();

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (player)
        {
            player.MessageStatus("[LFPG] Wiring cancelled.");
        }
    }
	
	override bool AddActionJuncture(ActionData action_data)
    {
        return true;
    }
};

// ---------------------------------------------------------
// CUT WIRES - with Pliers on any source
// ---------------------------------------------------------
class ActionLFPG_CutWires : ActionSingleUseBase
{
    void ActionLFPG_CutWires()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_CUT_WIRES";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !item)
            return false;

        if (!item.IsKindOf("Pliers"))
            return false;

        if (!target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        EntityAI e = EntityAI.Cast(targetObj);
        if (!e)
            return false;

        if (!LFPG_DeviceAPI.IsEnergySource(e))
            return false;

        // v0.7.10: DistSq avoids sqrt in per-frame ActionCondition
        return LFPG_WorldUtil.DistSq(player.GetPosition(), e.GetPosition()) <= LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        EntityAI e = EntityAI.Cast(targetObj);
        if (!e)
            return;

        int low = 0;
        int high = 0;
        e.GetNetworkID(low, high);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.CUT_WIRES);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }
};

// ---------------------------------------------------------
// CUT PORT - per-port wire cutting with Pliers
// Base class: determines port index from m_PortIndex.
// Only shows when port is occupied. Works on both OUT and IN.
// ---------------------------------------------------------
class ActionLFPG_CutPortBase : ActionSingleUseBase
{
    protected int m_PortIndex = -1;

    void ActionLFPG_CutPortBase()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "Cut Port";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (m_PortIndex < 0)
            return false;

        if (!player || !item)
            return false;

        if (!item.IsKindOf("Pliers"))
            return false;

        if (!target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        EntityAI e = EntityAI.Cast(targetObj);
        if (!e)
            return false;

        // Check device has this port
        int portCount = LFPG_DeviceAPI.GetPortCount(e);
        if (m_PortIndex >= portCount)
            return false;

        // v0.7.10: DistSq avoids sqrt in per-frame ActionCondition
        if (LFPG_WorldUtil.DistSq(player.GetPosition(), e.GetPosition()) > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        string portLabel = LFPG_DeviceAPI.GetPortLabel(e, m_PortIndex);
        string portName  = LFPG_DeviceAPI.GetPortName(e, m_PortIndex);
        string devId     = LFPG_DeviceAPI.GetOrCreateDeviceId(e);
        int portDir      = LFPG_DeviceAPI.GetPortDir(e, m_PortIndex);

        // Check if port is occupied.
        // Client: uses CableRenderer connection cache (fast O(1) lookup).
        // Server: checks wire data directly (authoritative).
        string connType = "";

        if (!GetGame().IsDedicatedServer())
        {
            // CLIENT: use cached connection info from CableRenderer
            LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
            if (renderer)
            {
                connType = renderer.GetConnectionType(devId, portName, portDir);
            }
        }
        else
        {
            // SERVER: check actual wire data
            if (portDir == LFPG_PortDir.OUT)
            {
                // OUT port: check if device owns a wire from this port
                if (LFPG_DeviceAPI.HasWireStore(e))
                {
                    array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(e);
                    if (wires)
                    {
                        int wi;
                        for (wi = 0; wi < wires.Count(); wi = wi + 1)
                        {
                            LFPG_WireData wd = wires[wi];
                            if (wd && wd.m_SourcePort == portName)
                            {
                                connType = "connected";
                                break;
                            }
                        }
                    }
                }
                else
                {
                    // Vanilla source
                    array<ref LFPG_WireData> vWires = LFPG_NetworkManager.Get().GetVanillaWires(devId);
                    if (vWires)
                    {
                        int vwi;
                        for (vwi = 0; vwi < vWires.Count(); vwi = vwi + 1)
                        {
                            LFPG_WireData vwd = vWires[vwi];
                            if (vwd)
                            {
                                string sp = vwd.m_SourcePort;
                                if (sp == "")
                                {
                                    sp = "output_1";
                                }
                                if (sp == portName)
                                {
                                    connType = "connected";
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else if (portDir == LFPG_PortDir.IN)
            {
                // IN port: check if any source targets this port
                int inCount = LFPG_NetworkManager.Get().CountWiresTargeting(devId, portName);
                if (inCount > 0)
                {
                    connType = "connected";
                }
            }
        }

        // Only show cut action when port has a connection
        if (connType == "")
            return false;

        m_Text = "Cut " + portLabel + " (" + connType + ")";
        return true;
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        EntityAI e = EntityAI.Cast(targetObj);
        if (!e)
            return;

        string portName = LFPG_DeviceAPI.GetPortName(e, m_PortIndex);
        int portDir     = LFPG_DeviceAPI.GetPortDir(e, m_PortIndex);

        int low = 0;
        int high = 0;
        e.GetNetworkID(low, high);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.CUT_PORT);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(portName);
        rpc.Write(portDir);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }
};

class ActionLFPG_CutPort0 : ActionLFPG_CutPortBase
{
    void ActionLFPG_CutPort0() { m_PortIndex = 0; }
};

class ActionLFPG_CutPort1 : ActionLFPG_CutPortBase
{
    void ActionLFPG_CutPort1() { m_PortIndex = 1; }
};

class ActionLFPG_CutPort2 : ActionLFPG_CutPortBase
{
    void ActionLFPG_CutPort2() { m_PortIndex = 2; }
};

class ActionLFPG_CutPort3 : ActionLFPG_CutPortBase
{
    void ActionLFPG_CutPort3() { m_PortIndex = 3; }
};

class ActionLFPG_CutPort4 : ActionLFPG_CutPortBase
{
    void ActionLFPG_CutPort4() { m_PortIndex = 4; }
};

class ActionLFPG_CutPort5 : ActionLFPG_CutPortBase
{
    void ActionLFPG_CutPort5() { m_PortIndex = 5; }
};

class ActionLFPG_CutPort6 : ActionLFPG_CutPortBase
{
    void ActionLFPG_CutPort6() { m_PortIndex = 6; }
};

// ---------------------------------------------------------
// TOGGLE SOURCE - on/off (LFPG generators only)
// ---------------------------------------------------------
class ActionLFPG_ToggleSource : ActionInteractBase
{
    // v0.7.35 (Bloque F): Debounce cache to prevent scroll menu text flicker.
    // After a toggle, the net-synced switch state can oscillate briefly on the
    // client. Without debounce, m_Text flips between "Turn On" / "Turn Off"
    // every frame during that window, causing visible flicker in the scroll menu.
    // Fix: suppress text changes for DEBOUNCE_MS after the last change.
    // s_LastTargetLow/High tracks which generator owns the cache — if the player
    // switches target, text updates immediately with no debounce.
    protected static float s_LastChangeMs   = -1.0;
    protected static int s_LastTargetLow  = -1;
    protected static int s_LastTargetHigh = -1;

    static const float TOGGLE_TEXT_DEBOUNCE_MS = 800.0;

    void ActionLFPG_ToggleSource()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_TOGGLE_SOURCE";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINone;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        LF_TestGenerator gen = LF_TestGenerator.Cast(targetObj);
        if (!gen)
            return false;

        // v0.7.10: DistSq avoids sqrt in per-frame ActionCondition
        if (LFPG_WorldUtil.DistSq(player.GetPosition(), gen.GetPosition()) > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        // v0.7.37 (Bug fix): Sparkplug gate + debounced dynamic text.
        // When OFF: only show action if sparkplug is present and not ruined.
        //   Prevents turning on without sparkplug entirely.
        // When ON:  always show "Turn Off" (player must be able to shut down).
        // IsSparkPlugValid uses FindAttachmentBySlotName which works on
        // both client and server (attachment state is synced via network).
        bool isOn = gen.LFPG_GetSwitchState();

        string newText = "Turn On Generator";
        if (isOn)
        {
            newText = "Turn Off Generator";
        }
        else
        {
            // Generator is OFF — only allow turning on with valid sparkplug
            if (!LFPG_DeviceLifecycle.IsSparkPlugValid(gen))
                return false;
        }

        // Identity check: different generator → update immediately, no debounce.
        int tLow = 0;
        int tHigh = 0;
        gen.GetNetworkID(tLow, tHigh);

        if (tLow != s_LastTargetLow || tHigh != s_LastTargetHigh)
        {
            m_Text = newText;
            s_LastTargetLow = tLow;
            s_LastTargetHigh = tHigh;
            s_LastChangeMs = GetGame().GetTime();
            return true;
        }

        // Same generator: only allow text change if debounce window expired.
        if (newText != m_Text)
        {
            float nowMs = GetGame().GetTime();
            if (s_LastChangeMs >= 0.0)
            {
                float elapsed = nowMs - s_LastChangeMs;
                if (elapsed >= 0.0 && elapsed < TOGGLE_TEXT_DEBOUNCE_MS)
                {
                    return true;  // suppress change, keep current m_Text
                }
            }
            m_Text = newText;
            s_LastChangeMs = nowMs;
        }

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LF_TestGenerator gen = LF_TestGenerator.Cast(targetObj);
        if (!gen)
            return;

        // v0.7.37 (Bug fix): Server-side sparkplug gate before toggle.
        // If player wants to turn ON but sparkplug is missing/ruined,
        // block and give clear feedback. Turn OFF always allowed.
        bool wasOn = gen.LFPG_GetSwitchState();

        if (!wasOn && !LFPG_DeviceLifecycle.IsSparkPlugValid(gen))
        {
            // Trying to turn ON without valid sparkplug — block
            PlayerBase blockPlayer = PlayerBase.Cast(action_data.m_Player);
            if (blockPlayer)
            {
                blockPlayer.MessageStatus("[LFPG] Cannot start: needs Spark Plug");
            }
            return;
        }

        gen.LFPG_ToggleSource();

        // Feedback: show resulting state
        PlayerBase execPlayer = PlayerBase.Cast(action_data.m_Player);
        if (execPlayer)
        {
            bool nowOn = gen.LFPG_GetSwitchState();

            if (!nowOn)
            {
                execPlayer.MessageStatus("[LFPG] Generator OFF");
            }
            else
            {
                execPlayer.MessageStatus("[LFPG] Generator ON - producing power");
            }
        }
    }
};

// ---------------------------------------------------------
// DEBUG STATUS - any electrical device (now with per-port info)
// ---------------------------------------------------------
class ActionLFPG_DebugStatus : ActionSingleUseBase
{
    void ActionLFPG_DebugStatus()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_STATUS";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !item)
            return false;

        if (!item.IsInherited(LFPG_CableReel))
            return false;

        if (!target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        EntityAI dev = EntityAI.Cast(targetObj);
        if (!dev)
            return false;

        // v2.6: Suppress DebugStatus on linked Sorters — the Sorter
        // has its own full config panel (LFPG_ActionOpenSorterPanel).
        // Showing both DebugStatus + Port actions clutters the scroll menu.
        string sorterType = "LFPG_Sorter";
        if (dev.IsKindOf(sorterType))
        {
            LFPG_Sorter sorterCast = LFPG_Sorter.Cast(dev);
            if (sorterCast)
            {
                if (sorterCast.LFPG_IsLinked())
                {
                    return false;
                }
            }
        }

        return LFPG_DeviceAPI.IsElectricDevice(dev);
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        EntityAI dev = EntityAI.Cast(targetObj);
        if (!dev)
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(dev);
        bool isSrc = LFPG_DeviceAPI.IsEnergySource(dev);
        bool isCon = LFPG_DeviceAPI.IsEnergyConsumer(dev);

        // Header
        string role = "Device";
        if (isSrc && isCon)
        {
            role = "Source+Consumer";
        }
        else if (isSrc)
        {
            role = "Source";
        }
        else if (isCon)
        {
            role = "Consumer";
        }
        player.MessageStatus("[LFPG] === " + dev.GetType() + " (" + role + ") ===");

        // v0.7.38 (BugFix): LFPG-native devices use LFPG_IsPowered.
        // CompEM shows "Off / 0.0/0.0" for LFPG devices because they
        // manage power via m_PoweredNet, not vanilla's energy system.
        string devIdForStatus = LFPG_DeviceAPI.GetDeviceId(dev);
        if (devIdForStatus != "")
        {
            // LFPG-native device: read powered state from LFPG API
            bool lfpgPowered = LFPG_DeviceAPI.GetPowered(dev);
            string lfpgState = "Off";
            if (lfpgPowered)
            {
                lfpgState = "Powered";
            }
            if (isSrc)
            {
                // Sources: show capacity
                float srcCap = LFPG_DeviceAPI.GetCapacity(dev);
                string srcCapStr = LFPG_FormatFloat(srcCap);
                player.MessageStatus("[LFPG] State: " + lfpgState + " | Capacity: " + srcCapStr + " W");
            }
            else if (isCon)
            {
                // Consumers: show consumption
                float conDemand = LFPG_DeviceAPI.GetConsumption(dev);
                string conDemStr = LFPG_FormatFloat(conDemand);
                player.MessageStatus("[LFPG] State: " + lfpgState + " | Consumption: " + conDemStr + " W");
            }
            else
            {
                player.MessageStatus("[LFPG] State: " + lfpgState);
            }
        }
        else
        {
            // Vanilla device: fall back to CompEM
            ComponentEnergyManager em = dev.GetCompEM();
            if (em)
            {
                string state = "Off";
                if (em.IsWorking())
                {
                    state = "Working";
                }
                else if (em.IsSwitchedOn())
                {
                    state = "On (no power)";
                }

                float energy = em.GetEnergy();
                float energyMax = em.GetEnergyMax();
                string energyStr = LFPG_FormatFloat(energy) + " / " + LFPG_FormatFloat(energyMax);
                player.MessageStatus("[LFPG] State: " + state + " | Energy: " + energyStr);
            }
        }

        // Per-port connection info
        int portCount = LFPG_DeviceAPI.GetPortCount(dev);
        LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();

        int pi;
        for (pi = 0; pi < portCount; pi = pi + 1)
        {
            string pName  = LFPG_DeviceAPI.GetPortName(dev, pi);
            string pLabel = LFPG_DeviceAPI.GetPortLabel(dev, pi);
            int pDir      = LFPG_DeviceAPI.GetPortDir(dev, pi);

            string dirStr = "?";
            if (pDir == LFPG_PortDir.OUT)
            {
                dirStr = "OUT";
            }
            else if (pDir == LFPG_PortDir.IN)
            {
                dirStr = "IN";
            }

            string connInfo = "Empty";
            if (renderer)
            {
                string ct = renderer.GetConnectionType(devId, pName, pDir);
                if (ct != "")
                {
                    connInfo = ct;
                }
            }

            player.MessageStatus("[LFPG] " + pLabel + " [" + dirStr + "] -> " + connInfo);
        }

        // Wiring session state
        bool wiringActive = LFPG_WiringClient.Get().IsActive();
        player.MessageStatus("[LFPG] Wiring active: " + wiringActive.ToString());
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);

        // v0.7.14: Defensive null checks
        if (!action_data)
            return;

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        EntityAI dev = EntityAI.Cast(targetObj);
        if (!dev)
            return;

        string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(dev);

        // v0.7.14: Guard empty device ID (device not yet registered)
        if (devId == "")
        {
            LFPG_Util.Warn("[DebugStatus] OnExecuteServer: devId empty for " + dev.GetType());
            return;
        }

        int wireOut = 0;
        array<ref LFPG_WireData> wiresOut = LFPG_NetworkManager.Get().GetWiresForDevice(devId);
        if (wiresOut)
        {
            wireOut = wiresOut.Count();
        }

        string sm = "[LFPG] Status(srv): " + dev.GetType();
        sm = sm + " id=" + devId;
        sm = sm + " wires=" + wireOut.ToString();

        ComponentEnergyManager emSrv = dev.GetCompEM();
        if (emSrv)
        {
            sm = sm + " em.working=" + emSrv.IsWorking().ToString();

            // v0.7.14: Separate retrieval from formatting for safety
            float srvEnergy = emSrv.GetEnergy();
            sm = sm + " em.energy=" + LFPG_FormatFloat(srvEnergy);
        }

        LFPG_Util.Info(sm);
    }

    // v0.7.14: Hardened against NaN / Infinity from CompEM invalid state.
    // LF_TestLamp blocks OnWork(), so CompEM energy tracking can return NaN.
    // Enforce Script (int)NaN = undefined behavior = crash.
    protected string LFPG_FormatFloat(float val)
    {
        // NaN guard: NaN != NaN is the only reliable test
        if (val != val)
        {
            return "NaN";
        }

        // Overflow guards: prevent (int) cast on huge values
        if (val > 999999.0)
        {
            return "999999+";
        }
        if (val < -999999.0)
        {
            return "-999999-";
        }

        float rounded = Math.Round(val * 10.0) / 10.0;

        // Post-arithmetic NaN guard (Round can propagate NaN)
        if (rounded != rounded)
        {
            return "NaN";
        }

        bool negative = false;
        if (rounded < 0)
        {
            negative = true;
            rounded = -rounded;
        }

        int intPart = (int)rounded;
        int decPart = Math.AbsInt((int)((rounded - intPart) * 10.0));

        string result = intPart.ToString() + "." + decPart.ToString();
        if (negative)
        {
            result = "-" + result;
        }
        return result;
    }
};
