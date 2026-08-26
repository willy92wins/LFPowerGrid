// =========================================================
// LF_PowerGrid - Network Manager (World-arena facade)
//
// Public contract only. Every field, helper and real method body is
// hosted by the mission layer (LFPG_NetworkManagerImpl), whose arena
// has budget to spare while the World arena does not.
//
// Call sites are unchanged: they keep calling Get(), which builds the
// implementation through the MissionBaseWorld factory, the same idiom
// LFPG_ElecGraph already uses.
// =========================================================

class LFPG_OwnerBroadcastSnapshot
{
    string m_OwnerDeviceId;
    int m_OwnerLow;
    int m_OwnerHigh;
    string m_JSON;
    int m_Generation;
    vector m_OwnerPosition;
    ref array<vector> m_TargetPositions;
    bool m_BroadcastAll;

    void LFPG_OwnerBroadcastSnapshot(string ownerDeviceId, int ownerLow, int ownerHigh, string json, int generation, vector ownerPosition, array<vector> targetPositions)
    {
        m_OwnerDeviceId = ownerDeviceId;
        m_OwnerLow = ownerLow;
        m_OwnerHigh = ownerHigh;
        m_JSON = json;
        m_Generation = generation;
        m_OwnerPosition = ownerPosition;
        m_TargetPositions = new array<vector>;
        m_BroadcastAll = false;

        int targetIndex;
        if (targetPositions)
        {
            for (targetIndex = 0; targetIndex < targetPositions.Count(); targetIndex = targetIndex + 1)
                m_TargetPositions.Insert(targetPositions[targetIndex]);
        }
    }
}

class LFPG_NetworkManager
{
    protected static ref LFPG_NetworkManager s_Instance;
    static LFPG_NetworkManager Get()
    {
        if (!s_Instance)
        {
            MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
            if (mw)
                s_Instance = mw.LFPG_CreateNetworkManager();
            if (!s_Instance)
            {
                LFPG_Util.Error("[LFPG_NetworkManager] Mission factory unavailable - fallback base manager (sim degraded)");
                s_Instance = new LFPG_NetworkManager();
            }
        }
        return s_Instance;
    }

    static LFPG_NetworkManager GetExisting()
    {
        return s_Instance;
    }
    void StartServerScheduler()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] StartServerScheduler called without mission factory (fallback base)");
        #endif
    }

    void StopServerScheduler()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] StopServerScheduler called without mission factory (fallback base)");
        #endif
    }

    bool AllowPlayerAction(PlayerIdentity ident)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] AllowPlayerAction called without mission factory (fallback base)");
        #endif
        return false;
    }

    bool IsPortLocked(string lockKey)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] IsPortLocked called without mission factory (fallback base)");
        #endif
        return false;
    }

    void LockPort(string lockKey)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] LockPort called without mission factory (fallback base)");
        #endif
    }

    void UnlockPort(string lockKey)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnlockPort called without mission factory (fallback base)");
        #endif
    }

    bool IsStartupValidationDone()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] IsStartupValidationDone called without mission factory (fallback base)");
        #endif
        return false;
    }

    bool IsValidationActive()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] IsValidationActive called without mission factory (fallback base)");
        #endif
        return false;
    }

    bool AddVanillaWire(string ownerDeviceId, LFPG_WireData wd)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] AddVanillaWire called without mission factory (fallback base)");
        #endif
        return false;
    }

    array<ref LFPG_WireData> GetVanillaWires(string ownerDeviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] GetVanillaWires called without mission factory (fallback base)");
        #endif
        return null;
    }

    array<ref LFPG_WireData> GetWiresForDevice(string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] GetWiresForDevice called without mission factory (fallback base)");
        #endif
        return null;
    }

    int GetVanillaWireOwnerCount()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] GetVanillaWireOwnerCount called without mission factory (fallback base)");
        #endif
        return 0;
    }

    string GetVanillaWireOwnerKey(int idx)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] GetVanillaWireOwnerKey called without mission factory (fallback base)");
        #endif
        return "";
    }

    bool CheckCycleBeforeWire(string sourceId, string targetId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] CheckCycleBeforeWire called without mission factory (fallback base)");
        #endif
        return false;
    }

    bool CheckComponentSizeBeforeWire(string sourceId, string targetId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] CheckComponentSizeBeforeWire called without mission factory (fallback base)");
        #endif
        return false;
    }

    bool NotifyGraphWireAdded(string sourceId, string targetId, string sourcePort, string targetPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] NotifyGraphWireAdded called without mission factory (fallback base)");
        #endif
        return false;
    }

    void NotifyGraphWireRemoved(string sourceId, string targetId, string sourcePort, string targetPort)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] NotifyGraphWireRemoved called without mission factory (fallback base)");
        #endif
    }

    void BeginGraphMutation()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] BeginGraphMutation called without mission factory (fallback base)");
        #endif
    }

    void EndGraphMutation()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] EndGraphMutation called without mission factory (fallback base)");
        #endif
    }

    void PostBulkRebuildAndPropagate()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] PostBulkRebuildAndPropagate called without mission factory (fallback base)");
        #endif
    }

    void NotifyGraphDeviceRemoved(string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] NotifyGraphDeviceRemoved called without mission factory (fallback base)");
        #endif
    }

    LFPG_ElecGraph GetGraph()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] GetGraph called without mission factory (fallback base)");
        #endif
        return null;
    }

    bool IsPortReceivingPower(string deviceId, string portName)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] IsPortReceivingPower called without mission factory (fallback base)");
        #endif
        return false;
    }

    void RebuildReverseIdx()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RebuildReverseIdx called without mission factory (fallback base)");
        #endif
    }

    int CountWiresTargeting(string targetDeviceId, string targetPort)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] CountWiresTargeting called without mission factory (fallback base)");
        #endif
        return 0;
    }

    bool IsPortTargetedByPoweredSource(string targetDeviceId, string targetPort)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] IsPortTargetedByPoweredSource called without mission factory (fallback base)");
        #endif
        return false;
    }

    void ReverseIdxAdd(string targetDeviceId, string targetPort, string ownerDeviceId = "")
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] ReverseIdxAdd called without mission factory (fallback base)");
        #endif
    }

    void ReverseIdxRemove(string targetDeviceId, string targetPort, string ownerDeviceId = "")
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] ReverseIdxRemove called without mission factory (fallback base)");
        #endif
    }

    void PlayerWireCountAdd(string creatorId, int delta)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] PlayerWireCountAdd called without mission factory (fallback base)");
        #endif
    }

    int RemoveWiresTargeting(string targetDeviceId, string targetPort, string creatorId = "", bool allowOthers = true)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RemoveWiresTargeting called without mission factory (fallback base)");
        #endif
        return 0;
    }

    bool CanPlayerCreateAnotherWire(PlayerIdentity ident, out string reason)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] CanPlayerCreateAnotherWire called without mission factory (fallback base)");
        #endif
        return false;
    }

    bool ValidateWire(vector startPos, vector endPos, array<vector> waypoints, out string reason)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] ValidateWire called without mission factory (fallback base)");
        #endif
        return false;
    }

    void QueueBroadcastOwner(EntityAI owner)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] QueueBroadcastOwner called without mission factory (fallback base)");
        #endif
    }

    void QueueBroadcastOwnerSnapshot(EntityAI owner, array<vector> targetPositions, bool broadcastAll)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] QueueBroadcastOwnerSnapshot called without mission factory (fallback base)");
        #endif
    }

    void QueueBroadcastVanilla(string ownerDeviceId, EntityAI ownerObj)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] QueueBroadcastVanilla called without mission factory (fallback base)");
        #endif
    }

    void FlushBroadcasts()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] FlushBroadcasts called without mission factory (fallback base)");
        #endif
    }

    void BroadcastOwnerWires(EntityAI owner)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] BroadcastOwnerWires called without mission factory (fallback base)");
        #endif
    }

    void BroadcastOwnerWireDelta(EntityAI owner, array<int> operations, array<ref LFPG_WireData> deltaWires)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] BroadcastOwnerWireDelta called without mission factory (fallback base)");
        #endif
    }

    void BroadcastVanillaWires(string ownerDeviceId, EntityAI ownerObj)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] BroadcastVanillaWires called without mission factory (fallback base)");
        #endif
    }

    void SendVanillaWiresTo(PlayerBase player, string ownerDeviceId, EntityAI ownerObj)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] SendVanillaWiresTo called without mission factory (fallback base)");
        #endif
    }

    void SendFullSyncTo(PlayerBase player)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] SendFullSyncTo called without mission factory (fallback base)");
        #endif
    }

    void SendDeviceSyncTo(PlayerBase player, string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] SendDeviceSyncTo called without mission factory (fallback base)");
        #endif
    }

    void SendDeviceSyncToBatched(PlayerBase player, string deviceId, map<string, bool> sentOwners)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] SendDeviceSyncToBatched called without mission factory (fallback base)");
        #endif
    }

    void SendEmptyDeviceCableStateTo(PlayerBase player, EntityAI deviceObj, string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] SendEmptyDeviceCableStateTo called without mission factory (fallback base)");
        #endif
    }

    void SendOwnerBlobTo(PlayerBase player, EntityAI ownerObj, string ownerId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] SendOwnerBlobTo called without mission factory (fallback base)");
        #endif
    }

    void RequestPropagate(string sourceDeviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RequestPropagate called without mission factory (fallback base)");
        #endif
    }

    void TrackDeviceForPolling(string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] TrackDeviceForPolling called without mission factory (fallback base)");
        #endif
    }

    void UntrackDeviceFromPolling(string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UntrackDeviceFromPolling called without mission factory (fallback base)");
        #endif
    }

    void RequestGlobalSelfHeal(bool validationOnlyAfterCut = false)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RequestGlobalSelfHeal called without mission factory (fallback base)");
        #endif
    }

    void ValidateAllWiresAndPropagate()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] ValidateAllWiresAndPropagate called without mission factory (fallback base)");
        #endif
    }

    map<string, bool> GetCachedValidIds()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] GetCachedValidIds called without mission factory (fallback base)");
        #endif
        return null;
    }

    void MarkVanillaDirty()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] MarkVanillaDirty called without mission factory (fallback base)");
        #endif
    }

    void FlushVanillaIfDirty()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] FlushVanillaIfDirty called without mission factory (fallback base)");
        #endif
    }

    void FlushVanillaOnShutdown()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] FlushVanillaOnShutdown called without mission factory (fallback base)");
        #endif
    }

    void CutAllWiresFromMovedDevice(EntityAI device, vector previousOwnerPosition, string knownDeviceId = "")
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] CutAllWiresFromMovedDevice called without mission factory (fallback base)");
        #endif
    }

	void CutAllWiresFromDevice(EntityAI device, string knownDeviceId = "")
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] CutAllWiresFromDevice called without mission factory (fallback base)");
        #endif
    }

    bool LFPG_GetCachedSunState()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] LFPG_GetCachedSunState called without mission factory (fallback base)");
        #endif
        return false;
    }

    float LFPG_GetBTCPrice()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] LFPG_GetBTCPrice called without mission factory (fallback base)");
        #endif
        return 0.0;
    }

    bool LFPG_IsBTCPriceAvailable()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] LFPG_IsBTCPriceAvailable called without mission factory (fallback base)");
        #endif
        return false;
    }

    float LFPG_GetBTC24hChange()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] LFPG_GetBTC24hChange called without mission factory (fallback base)");
        #endif
        return 0.0;
    }

    void LFPG_RefreshPumpSprinklerLink(string sourceId, string removedTargetId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] LFPG_RefreshPumpSprinklerLink called without mission factory (fallback base)");
        #endif
    }

    void RegisterSolar(LFPG_SolarPanel panel)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterSolar called without mission factory (fallback base)");
        #endif
    }

    void UnregisterSolar(LFPG_SolarPanel panel)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterSolar called without mission factory (fallback base)");
        #endif
    }

    void RegisterT1Pump(LFPG_WaterPump pump)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterT1Pump called without mission factory (fallback base)");
        #endif
    }

    void UnregisterT1Pump(LFPG_WaterPump pump)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterT1Pump called without mission factory (fallback base)");
        #endif
    }

    void RegisterT2Pump(LFPG_WaterPump_T2 pump)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterT2Pump called without mission factory (fallback base)");
        #endif
    }

    void UnregisterT2Pump(LFPG_WaterPump_T2 pump)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterT2Pump called without mission factory (fallback base)");
        #endif
    }

    void RegisterSprinkler(LFPG_Sprinkler spr)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterSprinkler called without mission factory (fallback base)");
        #endif
    }

    void UnregisterSprinkler(LFPG_Sprinkler spr)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterSprinkler called without mission factory (fallback base)");
        #endif
    }

    void RegisterSorter(LFPG_Sorter sorter)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterSorter called without mission factory (fallback base)");
        #endif
    }

    void UnregisterSorter(LFPG_Sorter sorter)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterSorter called without mission factory (fallback base)");
        #endif
    }

    void BroadcastCargoRefreshToNearby(array<EntityAI> containers, string excludePlayerId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] BroadcastCargoRefreshToNearby called without mission factory (fallback base)");
        #endif
    }

    int HandleSorterRequestSort(LFPG_Sorter sorter, string excludePlayerId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] HandleSorterRequestSort called without mission factory (fallback base)");
        #endif
        return 0;
    }

    void RegisterMotionSensor(LFPG_MotionSensor sensor)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterMotionSensor called without mission factory (fallback base)");
        #endif
    }

    void UnregisterMotionSensor(LFPG_MotionSensor sensor)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterMotionSensor called without mission factory (fallback base)");
        #endif
    }

    void RegisterPressurePad(LFPG_PressurePad pad)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterPressurePad called without mission factory (fallback base)");
        #endif
    }

    void UnregisterPressurePad(LFPG_PressurePad pad)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterPressurePad called without mission factory (fallback base)");
        #endif
    }

    void RegisterLaserDetector(LFPG_LaserDetector laser)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterLaserDetector called without mission factory (fallback base)");
        #endif
    }

    void UnregisterLaserDetector(LFPG_LaserDetector laser)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterLaserDetector called without mission factory (fallback base)");
        #endif
    }

    void RegisterIntercom(LFPG_Intercom ic)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterIntercom called without mission factory (fallback base)");
        #endif
    }

    void UnregisterIntercom(LFPG_Intercom ic)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterIntercom called without mission factory (fallback base)");
        #endif
    }

    void RegisterFurnace(LFPG_Furnace furnace)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterFurnace called without mission factory (fallback base)");
        #endif
    }

    void UnregisterFurnace(LFPG_Furnace furnace)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterFurnace called without mission factory (fallback base)");
        #endif
    }

    void RegisterFridge(LFPG_Fridge fridge)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterFridge called without mission factory (fallback base)");
        #endif
    }

    void UnregisterFridge(LFPG_Fridge fridge)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterFridge called without mission factory (fallback base)");
        #endif
    }

    void RegisterStove(LFPG_ElectricStove stove)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterStove called without mission factory (fallback base)");
        #endif
    }

    void UnregisterStove(LFPG_ElectricStove stove)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterStove called without mission factory (fallback base)");
        #endif
    }

    void RegisterDoorController(LFPG_DoorController dc)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterDoorController called without mission factory (fallback base)");
        #endif
    }

    void UnregisterDoorController(LFPG_DoorController dc)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterDoorController called without mission factory (fallback base)");
        #endif
    }

    void RegisterBattery(EntityAI battery)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] RegisterBattery called without mission factory (fallback base)");
        #endif
    }

    void UnregisterBattery(EntityAI battery)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_NetworkManager] UnregisterBattery called without mission factory (fallback base)");
        #endif
    }

};
