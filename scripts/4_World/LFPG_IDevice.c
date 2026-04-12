// =========================================================
// LF_PowerGrid - Device API (v0.7.44)
//
// Universal device detection:
//   - LFPG-native devices: via LFPG_* script methods
//   - Vanilla devices: via ComponentEnergyManager + config
//
// Vanilla devices get deterministic IDs ("vp:TYPE:QX:QY:QZ")
// based on position + type. These survive server restarts.
// If a device is moved, the ID changes and wires are orphaned.
//
// v0.7.44 (Level 2): Added ResolveByNetworkId / ResolveAndIdentify
//   helpers for centralized NetworkID-first entity resolution.
//   All client->server RPCs MUST use these instead of FindById.
// =========================================================

class LFPG_DeviceAPI
{
    // ----- Universal detection -----

    // Is this entity any kind of electrical device?
    static bool IsElectricDevice(EntityAI e)
    {
        if (!e)
            return false;

        // LFPG-native device
        string devId = GetDeviceId(e);
        if (devId != "")
            return true;

        // v0.7.38 (BugFix): Restrict vanilla to known types.
        // Previously accepted ANY entity with CompEM (flashlights, stoves,
        // radios, barrels...). Now only PowerGenerator and Spotlight.
        if (IsVanillaSource(e))
            return true;

        if (IsVanillaConsumer(e))
            return true;

        return false;
    }

    // Is this entity an energy source (generator)?
    static bool IsEnergySource(EntityAI e)
    {
        if (!e)
            return false;

        // LFPG-native source
        if (IsSource(e))
            return true;

        // Vanilla: check config for isEnergySource = 1
        if (IsVanillaSource(e))
            return true;

        return false;
    }

    // Is this entity an energy consumer (lamp, fridge, etc)?
    static bool IsEnergyConsumer(EntityAI e)
    {
        if (!e)
            return false;

        // LFPG-native consumer: has ANY input port (not just "input_main")
        int portCount = CallInt(e, "LFPG_GetPortCount", null, 0);
        if (portCount > 0)
        {
            int idx;
            for (idx = 0; idx < portCount; idx = idx + 1)
            {
                Param1<int> pIdx = new Param1<int>(idx);
                int dir = CallInt(e, "LFPG_GetPortDir", pIdx, -1);
                if (dir == LFPG_PortDir.IN)
                    return true;
            }
        }

        // v0.7.38 (BugFix): Use IsVanillaConsumer (restricted whitelist).
        // Previously checked CompEM + !IsVanillaSource which accepted
        // flashlights, stoves, radios, etc. This path is the server-side
        // gate in HandleLFPG_FinishWiring — must be consistent with client.
        if (IsVanillaConsumer(e))
            return true;

        return false;
    }

    // Get device ID, or generate a deterministic one for vanilla devices.
    // LFPG-native: uses persistent random ID (survives restarts via OnStoreSave).
    // Vanilla: uses "vp:TYPE:QX:QY:QZ" (position-based, survives restarts).
    //   QX/QY/QZ = position * 100, rounded to int (1cm quantization, v0.7.3).
    //   If a device is moved, its ID changes and wires are orphaned (by design).
    static string GetOrCreateDeviceId(EntityAI e)
    {
        if (!e)
            return "";

        // LFPG-native: use real ID
        string id = GetDeviceId(e);
        if (id != "")
            return id;

        // Vanilla: deterministic position + type hash
        return MakeVanillaId(e);
    }

    // Build a deterministic vanilla device ID from position + type.
    // Format: "vp:TypeName:QX:QY:QZ"
    // v0.7.3: 1cm quantization (pos * 100) for tighter precision.
    // (v0.7.2 used 10cm which could collide in dense builds.)
    // BREAKING: existing vanilla wire IDs will change on upgrade.
    // Self-heal will prune orphaned wires on first restart.
    //
    // KNOWN EDGE CASE: if two devices of the same type are placed at
    // the exact same coordinate (via a building mod with disabled
    // collisions), they will share the same ID. Their wire networks
    // will merge visually and logically. This is extremely rare in
    // normal gameplay and not worth adding UUID complexity for.
    // Document for admin troubleshooting of "phantom cables".
    static string MakeVanillaId(EntityAI e)
    {
        if (!e)
            return "";

        vector pos = e.GetPosition();
        string typeName = e.GetType();

        int qx = Math.Round(pos[0] * 100.0);
        int qy = Math.Round(pos[1] * 100.0);
        int qz = Math.Round(pos[2] * 100.0);

        return "vp:" + typeName + ":" + qx.ToString() + ":" + qy.ToString() + ":" + qz.ToString();
    }

    // Parse a "vp:TYPE:QX:QY:QZ" ID back into type name and approximate position.
    // Returns true if parsing succeeded, false otherwise.
    // outPos is the reconstructed position (1cm precision, v0.7.3).
    static bool ParseVanillaId(string deviceId, out string outType, out vector outPos)
    {
        outType = "";
        outPos = "0 0 0";

        if (deviceId.IndexOf("vp:") != 0)
            return false;

        // Strip "vp:" prefix
        string rest = deviceId.Substring(3, deviceId.Length() - 3);

        // Find first colon after type name
        int c1 = rest.IndexOf(":");
        if (c1 <= 0)
            return false;

        outType = rest.Substring(0, c1);
        string coords = rest.Substring(c1 + 1, rest.Length() - c1 - 1);

        // Parse "QX:QY:QZ"
        int c2 = coords.IndexOf(":");
        if (c2 <= 0)
            return false;

        string sX = coords.Substring(0, c2);
        string yzPart = coords.Substring(c2 + 1, coords.Length() - c2 - 1);

        int c3 = yzPart.IndexOf(":");
        if (c3 <= 0)
            return false;

        string sY = yzPart.Substring(0, c3);
        string sZ = yzPart.Substring(c3 + 1, yzPart.Length() - c3 - 1);

        // Reconstruct position from quantized ints (divide by 100, v0.7.3)
        outPos[0] = sX.ToInt() / 100.0;
        outPos[1] = sY.ToInt() / 100.0;
        outPos[2] = sZ.ToInt() / 100.0;

        return true;
    }

    // Resolve a "vp:" device ID to an entity by scanning nearby objects.
    // Uses GetObjectsAtPosition for spatial search, then filters by type.
    // Returns null if no match found within searchRadius.
    // v0.7.3: reduced default radius from 0.5m to 0.25m (25x the 1cm precision).
    static EntityAI ResolveVanillaDevice(string deviceId, float searchRadius = 0.25)
    {
        string typeName;
        vector targetPos;
        if (!ParseVanillaId(deviceId, typeName, targetPos))
            return null;

        // Spatial search (2D radius on XZ, we check Y manually)
        array<Object> objects = new array<Object>;
        g_Game.GetObjectsAtPosition(targetPos, searchRadius, objects, null);

        EntityAI bestMatch = null;
        float bestDist = searchRadius + 1.0;

        int i;
        for (i = 0; i < objects.Count(); i = i + 1)
        {
            Object obj = objects[i];
            if (!obj)
                continue;

            EntityAI ent = EntityAI.Cast(obj);
            if (!ent)
                continue;

            // Type must match exactly
            if (ent.GetType() != typeName)
                continue;

            // Full 3D distance check (handles multi-floor buildings)
            float dist = vector.Distance(ent.GetPosition(), targetPos);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestMatch = ent;
            }
        }

        // Auto-register in DeviceRegistry for future O(1) lookups
        if (bestMatch)
        {
            LFPG_DeviceRegistry.Get().Register(bestMatch, deviceId);
        }

        return bestMatch;
    }

    // ----- Config-based detection -----

    // Detect vanilla energy source via CompEM runtime properties.
    // Sources: high energy capacity (storage), zero consumption (usage).
    // e.g. PowerGenerator: energyMax=50000, usage=0
    //      Spotlight:      energyMax=0,     usage=0.1
    protected static bool IsVanillaSource(EntityAI e)
    {
        if (!e)
            return false;

        // Skip LFPG devices (handled by IsSource/LFPG_IsSource)
        string devId = GetDeviceId(e);
        if (devId != "")
            return false;

        // Method 1: class hierarchy check (most reliable)
        // PowerGenerator is the DayZ base class for all generators
        if (e.IsKindOf("PowerGenerator"))
            return true;

        // Method 2: CompEM runtime values (catches mod generators)
        // Sources have high storage capacity and zero consumption
        ComponentEnergyManager em = e.GetCompEM();
        if (em)
        {
            float maxEnergy = em.GetEnergyMax();
            float usage = em.GetEnergyUsage();

            if (maxEnergy > 0 && usage < 0.001)
                return true;
        }

        return false;
    }

    // ----- Power delivery -----

    // Set powered state on any device (LFPG or vanilla consumer).
    // LFPG devices: calls LFPG_SetPowered via dynamic dispatch.
    // Vanilla consumers: uses CompEM SwitchOn/Off directly.
    // NOTE: PlugThisInto/UnplugThis crash due to internal cable/attachment logic,
    //       so we control CompEM state manually.
    // v0.7.4: guards against redundant SwitchOn/Off calls and
    //         respects vanilla CanWork() gating for mod compatibility.
    static void SetPowered(Object obj, bool powered)
    {
        if (!obj)
            return;

        // v4.0: Cast fast-path — direct call for production devices
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
        if (dev)
        {
            dev.LFPG_SetPowered(powered);
            return;
        }

        string objType = obj.GetType();

        // Try LFPG-native method (test/legacy devices)
        Param1<bool> p = new Param1<bool>(powered);
        CallVoid(obj, "LFPG_SetPowered", p);

        // If this is an LFPG device, the call above handled it
        EntityAI e = EntityAI.Cast(obj);
        if (!e)
            return;

        string devId = GetDeviceId(e);
        if (devId != "")
        {
            LFPG_Util.Debug("[SetPowered] LFPG path for " + objType + " powered=" + powered.ToString());
            return;
        }

        // Vanilla consumer: direct CompEM control
        ComponentEnergyManager em = e.GetCompEM();
        if (!em)
            return;

        // v0.7.4: skip if already in desired state.
        // Avoids redundant SwitchOn/Off that can cause animation/sound
        // glitches and interfere with mod state machines.
        bool currentlyOn = em.IsSwitchedOn();
        if (powered && currentlyOn)
        {
            // v4.7: Even if switched on, vanilla CompEM may have drained
            // its energy (IsSwitchedOn=true but IsWorking=false).
            // Top up to keep CanWork() returning true.
            float topUpEnergy = em.GetEnergy();
            if (topUpEnergy < LFPG_VANILLA_ENERGY_POOL * 0.5)
            {
                em.SetEnergy(LFPG_VANILLA_ENERGY_POOL);
            }
            return;
        }
        if (!powered && !currentlyOn)
            return;

        LFPG_Util.Debug("[SetPowered] Vanilla path for " + objType + " powered=" + powered.ToString());

        if (powered)
        {
            // v4.7: Inject energy into CompEM so vanilla CanWork() returns true.
            // Vanilla CanWork() requires either internal energy OR a plugged source.
            // LFPG cannot use PlugThisInto (crashes with cable/attachment logic),
            // so we inject energy directly. The consumer thinks it has its own power.
            // ValidateConsumerStates tops up periodically to prevent depletion.
            float curEnergy = em.GetEnergy();
            if (curEnergy < LFPG_VANILLA_ENERGY_POOL)
            {
                em.SetEnergy(LFPG_VANILLA_ENERGY_POOL);
                LFPG_Util.Debug("[SetPowered] Injected energy for " + objType + " (" + curEnergy.ToString() + " -> " + LFPG_VANILLA_ENERGY_POOL.ToString() + ")");
            }
            em.SwitchOn();
        }
        else
        {
            em.SwitchOff();
            // v4.7: Drain injected energy on power-off.
            // Prevents vanilla consumer from staying powered after disconnection.
            em.SetEnergy(0);
        }
    }

    // Check if a device is a vanilla consumer (has CompEM, no LFPG device ID)
    // v4.6: Vanilla Compat — accepts any entity with CompEM.GetEnergyUsage() > 0
    // unless blacklisted. Spotlight always accepted (bypass blacklist).
    // When VanillaCompatEnabled=false, falls back to Spotlight-only (legacy).
    static bool IsVanillaConsumer(EntityAI e)
    {
        if (!e)
            return false;

        // Has LFPG ID = LFPG device, not vanilla
        string devId = GetDeviceId(e);
        if (devId != "")
            return false;

        // Spotlight always accepted (backward compat, bypass blacklist)
        string spotlightCls = "Spotlight";
        if (e.IsKindOf(spotlightCls))
            return true;

        // v4.6: Extended vanilla compat
        LFPG_ServerSettings st = LFPG_Settings.Get();
        if (!st.VanillaCompatEnabled)
            return false;

        // Must have CompEM with positive energy usage
        ComponentEnergyManager em = e.GetCompEM();
        if (!em)
            return false;

        float usage = em.GetEnergyUsage();
        if (usage <= 0.0)
            return false;

        // Check blacklist (cached per typename)
        if (LFPG_Settings.IsVanillaBlacklistedEntity(e))
            return false;

        return true;
    }

    // ----- Low-level dynamic calls -----

    protected static int CallInt(Class inst, string fn, Class parms = null, int fallback = 0)
    {
        if (!inst) return fallback;
        int ret = fallback;
        int ok = g_Game.GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    protected static bool CallBool(Class inst, string fn, Class parms = null, bool fallback = false)
    {
        if (!inst) return fallback;
        bool ret = fallback;
        int ok = g_Game.GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    protected static string CallString(Class inst, string fn, Class parms = null, string fallback = "")
    {
        if (!inst) return fallback;
        string ret = fallback;
        int ok = g_Game.GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    protected static vector CallVector(Class inst, string fn, Class parms = null, vector fallback = "0 0 0")
    {
        if (!inst) return fallback;
        vector ret = fallback;
        int ok = g_Game.GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    protected static void CallVoid(Class inst, string fn, Class parms = null)
    {
        if (!inst) return;
        g_Game.GameScript.CallFunctionParams(inst, fn, NULL, parms);
    }

    protected static float CallFloat(Class inst, string fn, Class parms = null, float fallback = 0.0)
    {
        if (!inst) return fallback;
        float ret = fallback;
        int ok = g_Game.GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    // ----- LFPG-native device wrappers -----

    // v4.0: Cast fast-path — avoids reflexion for 95% of devices.
    static string GetDeviceId(Object obj)
    {
        if (!obj)
            return "";
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
        if (dev)
            return dev.LFPG_GetDeviceId();
        return CallString(obj, "LFPG_GetDeviceId", null, "");
    }

    static bool HasPort(Object obj, string portName, int dir)
    {
        if (!obj) return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
        if (dev)
            return dev.LFPG_HasPort(portName, dir);
        Param2<string, int> p = new Param2<string, int>(portName, dir);
        return CallBool(obj, "LFPG_HasPort", p, false);
    }

    static vector GetPortWorldPos(Object obj, string portName)
    {
        Param1<string> p = new Param1<string>(portName);
        vector fb = "0 0 0";
        if (obj) fb = obj.GetPosition();
        return CallVector(obj, "LFPG_GetPortWorldPos", p, fb);
    }

    static bool IsSource(Object obj)
    {
        if (!obj)
            return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
        if (dev)
            return dev.LFPG_IsSource();
        return CallBool(obj, "LFPG_IsSource", null, false);
    }

    static bool GetSourceOn(Object obj)
    {
        if (!obj)
            return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
        if (dev)
            return dev.LFPG_GetSourceOn();
        return CallBool(obj, "LFPG_GetSourceOn", null, false);
    }

	static bool IsGateOpen(EntityAI e)
    {
        if (!e) return true;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_IsGateOpen();
        return CallBool(e, "LFPG_IsGateOpen", null, true);
    }

    static bool IsGateCapable(EntityAI e)
    {
        if (!e) return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_IsGateCapable();
        return CallBool(e, "LFPG_IsGateCapable", null, false);
    }

    static bool GetPowered(Object obj)
    {
        if (!obj) return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
        if (dev)
            return dev.LFPG_IsPowered();
        return CallBool(obj, "LFPG_IsPowered", null, false);
    }

    static bool CanConnectTo(Object obj, Object other, string myPort, string otherPort)
    {
        if (!obj) return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
        if (dev)
            return dev.LFPG_CanConnectTo(other, myPort, otherPort);
        Param3<Object, string, string> p = new Param3<Object, string, string>(other, myPort, otherPort);
        return CallBool(obj, "LFPG_CanConnectTo", p, false);
    }

    // ----- Generic wire-owner helpers -----
    // These use dynamic dispatch so ANY device implementing the
    // LFPG wire API (Generator, Splitter, future devices) works
    // without needing explicit class casts.

    // Does this object own a wire store?
    static bool HasWireStore(Object obj)
    {
        if (!obj) return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
        if (dev)
            return dev.LFPG_HasWireStore();
        return CallBool(obj, "LFPG_HasWireStore", null, false);
    }

    // Get wires JSON for broadcasting (avoids out-param issues)
    static string GetWiresJSON(Object obj)
    {
        if (!obj) return "";
        LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
        if (wo)
            return wo.LFPG_GetWiresJSON();
        return CallString(obj, "LFPG_GetWiresJSON", null, "");
    }

    // Add a wire to any wire-owning device
    static bool AddDeviceWire(Object obj, LFPG_WireData wd)
    {
        if (!obj || !wd) return false;
        LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
        if (wo)
            return wo.LFPG_AddWire(wd);
        Param1<LFPG_WireData> p = new Param1<LFPG_WireData>(wd);
        return CallBool(obj, "LFPG_AddWire", p, false);
    }

    // Clear all wires from any wire-owning device
    static bool ClearDeviceWires(Object obj)
    {
        if (!obj) return false;
        LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
        if (wo)
            return wo.LFPG_ClearWires();
        return CallBool(obj, "LFPG_ClearWires", null, false);
    }

    // Clear wires for a specific creator
    static bool ClearDeviceWiresForCreator(Object obj, string creatorId)
    {
        if (!obj || creatorId == "") return false;
        LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
        if (wo)
            return wo.LFPG_ClearWiresForCreator(creatorId);
        Param1<string> p = new Param1<string>(creatorId);
        return CallBool(obj, "LFPG_ClearWiresForCreator", p, false);
    }

    // Prune wires targeting missing devices
    static bool PruneDeviceMissingTargets(Object obj)
    {
        if (!obj) return false;
        LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
        if (wo)
            return wo.LFPG_PruneMissingTargets();
        return CallBool(obj, "LFPG_PruneMissingTargets", null, false);
    }

    static array<ref LFPG_WireData> GetDeviceWires(Object obj)
    {
        if (!obj) return null;
        LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
        if (wo)
            return wo.LFPG_GetWires();
        array<ref LFPG_WireData> ret;
        int ok = g_Game.GameScript.CallFunctionParams(obj, "LFPG_GetWires", ret, null);
        if (ok != 0)
            return ret;
        return null;
    }

    // ----- Device type classification (Sprint 4.1) -----

    // Resolve the LFPG_DeviceType for any device (LFPG-native or vanilla).
    // LFPG devices: calls LFPG_GetDeviceType via dynamic dispatch.
    // Vanilla devices: heuristic based on CompEM (source vs consumer).
    // Returns LFPG_DeviceType.UNKNOWN if the device cannot be classified.
    static int GetDeviceType(EntityAI e)
    {
        if (!e)
            return LFPG_DeviceType.UNKNOWN;

        // v4.0: Cast fast-path
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_GetDeviceType();

        // Try LFPG-native dynamic dispatch (test/legacy)
        int nativeType = CallInt(e, "LFPG_GetDeviceType", null, -1);
        if (nativeType >= 0)
            return nativeType;

        // Vanilla heuristic: source or consumer based on CompEM
        if (IsVanillaSource(e))
            return LFPG_DeviceType.SOURCE;

        if (IsVanillaConsumer(e))
            return LFPG_DeviceType.CONSUMER;

        return LFPG_DeviceType.UNKNOWN;
    }

    // ----- Energy / load (v0.7.8) -----

    // Get consumption rate (units/s) for a consumer device.
    // LFPG devices: LFPG_GetConsumption().
    // Vanilla devices: v4.6 settings-based resolution (custom > CompEM > default).
    // Returns 0 for sources or devices without consumption.
    static float GetConsumption(EntityAI e)
    {
        if (!e) return 0.0;

        // v4.0: Cast fast-path
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_GetConsumption();

        // LFPG native (test devices / legacy)
        float val = CallFloat(e, "LFPG_GetConsumption", null, -1.0);
        if (val >= 0.0)
            return val;

        // v0.7.47: PASSTHROUGH devices default to zero self-consumption.
        // Protects future passthrough types that forget to declare
        // LFPG_GetConsumption() — without this, IsEnergyConsumer below
        // returns true (they have an IN port) and assigns 10.0 erroneously.
        int devTypeGuard = GetDeviceType(e);
        if (devTypeGuard == LFPG_DeviceType.PASSTHROUGH)
            return 0.0;

        // v4.6: Vanilla-compat consumer — settings-based resolution
        // Priority: custom entry > CompEM.GetEnergyUsage > VanillaDefaultConsumption
        if (IsEnergyConsumer(e))
            return LFPG_Settings.GetVanillaConsumption(e);

        return 0.0;
    }

    // Get capacity (units/s) for a source device.
    // LFPG devices: LFPG_GetCapacity().
    // Vanilla: uses default (CompEM.GetEnergyMax is total stored, not rate).
    // Returns 0 for consumers.
    static float GetCapacity(EntityAI e)
    {
        if (!e) return 0.0;

        // v4.0: Cast fast-path
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_GetCapacity();

        // LFPG native (test/legacy)
        float val = CallFloat(e, "LFPG_GetCapacity", null, -1.0);
        if (val >= 0.0)
            return val;

        // Vanilla source: use default capacity
        if (IsEnergySource(e))
            return LFPG_DEFAULT_SOURCE_CAPACITY;

        return 0.0;
    }

    // Get current load ratio on a source device (0.0 = idle, 1.0 = full).
    // Only meaningful for sources. Set by PropagateFrom on server.
    static float GetLoadRatio(EntityAI e)
    {
        if (!e) return 0.0;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_GetLoadRatio();
        return CallFloat(e, "LFPG_GetLoadRatio", null, 0.0);
    }

    // Set load ratio on a source device (server only).
    static void SetLoadRatio(EntityAI e, float ratio)
    {
        if (!e) return;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
        {
            dev.LFPG_SetLoadRatio(ratio);
            return;
        }
        Param1<float> p = new Param1<float>(ratio);
        CallVoid(e, "LFPG_SetLoadRatio", p);
    }

    // v1.0: Overloaded state (all-off policy: demand > capacity).
    // True when the device's downstream demand exceeds available power.
    static bool GetOverloaded(EntityAI e)
    {
        if (!e) return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_GetOverloaded();
        return CallBool(e, "LFPG_GetOverloaded", null, false);
    }

    static void SetOverloaded(EntityAI e, bool val)
    {
        if (!e) return;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
        {
            dev.LFPG_SetOverloaded(val);
            return;
        }
        Param1<bool> p = new Param1<bool>(val);
        CallVoid(e, "LFPG_SetOverloaded", p);
    }

    // ----- Port enumeration (with vanilla fallback) -----

    // Total number of ports on this device.
    // LFPG devices define LFPG_GetPortCount().
    // Vanilla: 1 port (output for sources, input for consumers).
    static int GetPortCount(EntityAI e)
    {
        if (!e) return 0;

        // v4.0: Cast fast-path
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_GetPortCount();

        // Try LFPG dynamic dispatch (test/legacy)
        int count = CallInt(e, "LFPG_GetPortCount", null, 0);
        if (count > 0) return count;

        // Vanilla fallback
        if (IsVanillaSource(e)) return 1;
        if (IsVanillaConsumer(e)) return 1;
        return 0;
    }

    // Port name at index (e.g. "output_1", "input_main").
    static string GetPortName(EntityAI e, int idx)
    {
        if (!e) return "";

        Param1<int> p = new Param1<int>(idx);
        string name = CallString(e, "LFPG_GetPortName", p, "");
        if (name != "") return name;

        // Vanilla fallback (only idx 0)
        if (idx != 0) return "";
        if (IsVanillaSource(e)) return "output_1";
        if (IsVanillaConsumer(e)) return "input_main";
        return "";
    }

    // Port direction at index (LFPG_PortDir.IN or OUT).
    // Returns -1 if port doesn't exist.
    static int GetPortDir(EntityAI e, int idx)
    {
        if (!e) return -1;

        Param1<int> p = new Param1<int>(idx);
        int dir = CallInt(e, "LFPG_GetPortDir", p, -1);
        if (dir >= 0) return dir;

        // Vanilla fallback
        if (idx != 0) return -1;
        if (IsVanillaSource(e)) return LFPG_PortDir.OUT;
        if (IsVanillaConsumer(e)) return LFPG_PortDir.IN;
        return -1;
    }

    // Human-readable port label (e.g. "Output 1", "Input 1").
    static string GetPortLabel(EntityAI e, int idx)
    {
        if (!e) return "";

        Param1<int> p = new Param1<int>(idx);
        string label = CallString(e, "LFPG_GetPortLabel", p, "");
        if (label != "") return label;

        // Vanilla fallback
        if (idx != 0) return "";
        if (IsVanillaSource(e)) return "Output 1";
        if (IsVanillaConsumer(e)) return "Input 1";
        return "";
    }

    // =========================================================
    // v0.7.44 (Level 2): Centralized NetworkID-first resolution.
    //
    // ARCHITECTURE RULE: In ALL client->server RPC flows, entity
    // resolution MUST use NetworkID as primary key. DeviceId from
    // the client is informational/correlational, never authoritative.
    //
    // Reason: DeviceId is replicated via SyncVars with eventual
    // consistency. There is a window after Kit placement where the
    // client's DeviceId differs from the server's. NetworkID is
    // assigned atomically by the engine and is immediately coherent.
    //
    // Usage in RPC handlers:
    //   EntityAI obj = LFPG_DeviceAPI.ResolveByNetworkId(low, high);
    //   if (!obj) { /* reject RPC */ return; }
    //   string realId = LFPG_DeviceAPI.GetOrCreateDeviceId(obj);
    //
    // See docs/ARCHITECTURE_NetworkID.md for full documentation.
    // =========================================================

    // Resolve entity by engine NetworkID. Returns null if not found.
    // This is the ONLY correct way to resolve entities from client RPCs.
    static EntityAI ResolveByNetworkId(int netLow, int netHigh)
    {
        if (netLow == 0 && netHigh == 0)
            return null;

        Object rawObj = g_Game.GetObjectByNetworkId(netLow, netHigh);
        if (!rawObj)
            return null;

        EntityAI ent = EntityAI.Cast(rawObj);
        return ent;
    }

    // Resolve by NetworkID, then obtain the authoritative DeviceId.
    // Returns the entity and populates realDeviceId (out parameter).
    // If entity is found but has no DeviceId, generates one (vanilla).
    // Convenience wrapper for the common RPC handler pattern.
    static EntityAI ResolveAndIdentify(int netLow, int netHigh, out string realDeviceId)
    {
        realDeviceId = "";

        EntityAI ent = ResolveByNetworkId(netLow, netHigh);
        if (!ent)
            return null;

        realDeviceId = GetOrCreateDeviceId(ent);
        return ent;
    }

    // ----- v3.0: RF toggle capability (Intercom system) -----

    // Check if a device can be toggled remotely via RF signal.
    // Default: false (existing devices are unaffected).
    // Override LFPG_IsRFCapable() → true on RF-capable subclasses.
    static bool IsRFCapable(EntityAI e)
    {
        if (!e) return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_IsRFCapable();
        return CallBool(e, "LFPG_IsRFCapable", null, false);
    }

    // Execute remote toggle on an RF-capable device.
    // Returns true if the toggle was successfully executed.
    static bool RemoteToggle(EntityAI e)
    {
        if (!e) return false;
        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(e);
        if (dev)
            return dev.LFPG_RemoteToggle();
        return CallBool(e, "LFPG_RemoteToggle", null, false);
    }
};
