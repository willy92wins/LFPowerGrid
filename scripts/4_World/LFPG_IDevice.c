// =========================================================
// LF_PowerGrid - Device API (v0.7.11)
//
// Universal device detection:
//   - LFPG-native devices: via LFPG_* script methods
//   - Vanilla devices: via ComponentEnergyManager + config
//
// Vanilla devices get deterministic IDs ("vp:TYPE:QX:QY:QZ")
// based on position + type. These survive server restarts.
// If a device is moved, the ID changes and wires are orphaned.
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

        // Vanilla: has ComponentEnergyManager
        ComponentEnergyManager em = e.GetCompEM();
        if (em)
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

        // Vanilla: has EM but is NOT a source
        ComponentEnergyManager em = e.GetCompEM();
        if (em)
        {
            if (!IsVanillaSource(e))
                return true;
        }

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
        GetGame().GetObjectsAtPosition(targetPos, searchRadius, objects, null);

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

        string objType = obj.GetType();

        // Try LFPG-native method first
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
            return;
        if (!powered && !currentlyOn)
            return;

        LFPG_Util.Debug("[SetPowered] Vanilla path for " + objType + " powered=" + powered.ToString());

        if (powered)
        {
            // v0.7.4: respect vanilla/mod gating.
            // Only switch on if the device can actually work
            // (has fuel, correct attachments, etc.).
            // Prevents bypassing battery/plug requirements from mods.
            if (em.CanWork())
            {
                em.SwitchOn();
            }
            else
            {
                LFPG_Util.Debug("[SetPowered] Skipped SwitchOn for " + objType + " (CanWork=false)");
            }
        }
        else
        {
            em.SwitchOff();
        }
    }

    // Check if a device is a vanilla consumer (has CompEM, no LFPG device ID)
    static bool IsVanillaConsumer(EntityAI e)
    {
        if (!e)
            return false;

        // Has LFPG ID = LFPG device, not vanilla
        string devId = GetDeviceId(e);
        if (devId != "")
            return false;

        // v0.7.38 (BugFix): Restrict to known vanilla consumer types.
        // Previously any entity with CompEM was accepted, which included
        // flashlights, stoves, barrels, radios, etc. Now only accepts
        // Spotlight (tripod-mounted lights) — the only vanilla consumer
        // type that makes sense for LFPG wiring.
        if (e.IsKindOf("Spotlight"))
            return true;

        return false;
    }

    // ----- Low-level dynamic calls -----

    protected static int CallInt(Class inst, string fn, Class parms = null, int fallback = 0)
    {
        if (!inst) return fallback;
        int ret = fallback;
        int ok = GetGame().GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    protected static bool CallBool(Class inst, string fn, Class parms = null, bool fallback = false)
    {
        if (!inst) return fallback;
        bool ret = fallback;
        int ok = GetGame().GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    protected static string CallString(Class inst, string fn, Class parms = null, string fallback = "")
    {
        if (!inst) return fallback;
        string ret = fallback;
        int ok = GetGame().GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    protected static vector CallVector(Class inst, string fn, Class parms = null, vector fallback = "0 0 0")
    {
        if (!inst) return fallback;
        vector ret = fallback;
        int ok = GetGame().GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    protected static void CallVoid(Class inst, string fn, Class parms = null)
    {
        if (!inst) return;
        GetGame().GameScript.CallFunctionParams(inst, fn, NULL, parms);
    }

    protected static float CallFloat(Class inst, string fn, Class parms = null, float fallback = 0.0)
    {
        if (!inst) return fallback;
        float ret = fallback;
        int ok = GetGame().GameScript.CallFunctionParams(inst, fn, ret, parms);
        if (ok) return ret;
        return fallback;
    }

    // ----- LFPG-native device wrappers -----

    static string GetDeviceId(Object obj)
    {
        return CallString(obj, "LFPG_GetDeviceId", null, "");
    }

    static bool HasPort(Object obj, string portName, int dir)
    {
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
        return CallBool(obj, "LFPG_IsSource", null, false);
    }

    static bool GetSourceOn(Object obj)
    {
        return CallBool(obj, "LFPG_GetSourceOn", null, false);
    }

    // v0.7.38 (BugFix): Generic powered-state getter.
    // Reads LFPG_IsPowered from LFPG-native devices (SyncVar m_PoweredNet).
    // Used by status display to show correct state instead of CompEM.
    static bool GetPowered(Object obj)
    {
        return CallBool(obj, "LFPG_IsPowered", null, false);
    }

    static bool CanConnectTo(Object obj, Object other, string myPort, string otherPort)
    {
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
        return CallBool(obj, "LFPG_HasWireStore", null, false);
    }

    // Get wires JSON for broadcasting (avoids out-param issues)
    static string GetWiresJSON(Object obj)
    {
        return CallString(obj, "LFPG_GetWiresJSON", null, "");
    }

    // Add a wire to any wire-owning device
    static bool AddDeviceWire(Object obj, LFPG_WireData wd)
    {
        if (!obj || !wd) return false;
        Param1<LFPG_WireData> p = new Param1<LFPG_WireData>(wd);
        return CallBool(obj, "LFPG_AddWire", p, false);
    }

    // Clear all wires from any wire-owning device
    static bool ClearDeviceWires(Object obj)
    {
        return CallBool(obj, "LFPG_ClearWires", null, false);
    }

    // Clear wires for a specific creator
    static bool ClearDeviceWiresForCreator(Object obj, string creatorId)
    {
        if (!obj || creatorId == "") return false;
        Param1<string> p = new Param1<string>(creatorId);
        return CallBool(obj, "LFPG_ClearWiresForCreator", p, false);
    }

    // Prune wires targeting missing devices
    static bool PruneDeviceMissingTargets(Object obj)
    {
        return CallBool(obj, "LFPG_PruneMissingTargets", null, false);
    }

    // Get the array of wires from any wire-owning device.
    // Uses CallFunctionParams with array return type.
    static array<ref LFPG_WireData> GetDeviceWires(Object obj)
    {
        if (!obj) return null;
        array<ref LFPG_WireData> ret;
        int ok = GetGame().GameScript.CallFunctionParams(obj, "LFPG_GetWires", ret, null);
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

        // Try LFPG-native dynamic dispatch first
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
    // Vanilla devices: CompEM.GetEnergyUsage() (DayZ native).
    // Returns 0 for sources or devices without consumption.
    static float GetConsumption(EntityAI e)
    {
        if (!e) return 0.0;

        // LFPG native
        float val = CallFloat(e, "LFPG_GetConsumption", null, -1.0);
        if (val >= 0.0)
            return val;

        // Vanilla fallback: CompEM usage
        ComponentEnergyManager em = e.GetCompEM();
        if (em)
        {
            float usage = em.GetEnergyUsage();
            if (usage > 0.0)
                return usage;
        }

        // Unknown consumer: use default
        if (IsEnergyConsumer(e))
            return LFPG_DEFAULT_CONSUMER_CONSUMPTION;

        return 0.0;
    }

    // Get capacity (units/s) for a source device.
    // LFPG devices: LFPG_GetCapacity().
    // Vanilla: uses default (CompEM.GetEnergyMax is total stored, not rate).
    // Returns 0 for consumers.
    static float GetCapacity(EntityAI e)
    {
        if (!e) return 0.0;

        // LFPG native
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
        return CallFloat(e, "LFPG_GetLoadRatio", null, 0.0);
    }

    // Set load ratio on a source device (server only).
    static void SetLoadRatio(EntityAI e, float ratio)
    {
        if (!e) return;
        Param1<float> p = new Param1<float>(ratio);
        CallVoid(e, "LFPG_SetLoadRatio", p);
    }

    // v0.7.8: Overload bitmask (which output wires exceed capacity)
    // Bit N = 1 means wire at index N is overloaded.
    static int GetOverloadMask(EntityAI e)
    {
        if (!e) return 0;
        return CallInt(e, "LFPG_GetOverloadMask", null, 0);
    }

    static void SetOverloadMask(EntityAI e, int mask)
    {
        if (!e) return;
        Param1<int> p = new Param1<int>(mask);
        CallVoid(e, "LFPG_SetOverloadMask", p);
    }

    // v0.7.35 (F1.3): Warning bitmask (partial allocation — demand > allocated).
    // Bit N = 1 means wire at index N is receiving power but less than demanded.
    // Used by CableRenderer for per-wire WARNING_LOAD visual state.
    static int GetWarningMask(EntityAI e)
    {
        if (!e) return 0;
        return CallInt(e, "LFPG_GetWarningMask", null, 0);
    }

    static void SetWarningMask(EntityAI e, int mask)
    {
        if (!e) return;
        Param1<int> p = new Param1<int>(mask);
        CallVoid(e, "LFPG_SetWarningMask", p);
    }

    // ----- Port enumeration (with vanilla fallback) -----

    // Total number of ports on this device.
    // LFPG devices define LFPG_GetPortCount().
    // Vanilla: 1 port (output for sources, input for consumers).
    static int GetPortCount(EntityAI e)
    {
        if (!e) return 0;

        // Try LFPG dynamic dispatch
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
};
