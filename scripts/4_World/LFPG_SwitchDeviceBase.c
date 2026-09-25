// Electrical state shared by momentary and latched switches.
// Pulse callbacks and persisted latch state remain on the original leaf types.
class LFPG_SwitchDeviceBase : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_SwitchOn = false;
    protected bool m_Overloaded = false;

    void LFPG_SwitchDeviceBase()
    {
        string varPowered  = "m_PoweredNet";
        string varSwitch   = "m_SwitchOn";
        string varOverload = "m_Overloaded";
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varSwitch);
        RegisterNetSyncVariableBool(varOverload);

        string pIn  = "input_1";
        string pOut = "output_1";
        string lIn  = "Input 1";
        string lOut = "Output 1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);
    }

    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsGateCapable() { return true; }
    override bool LFPG_IsGateOpen() { return m_SwitchOn; }
    override float LFPG_GetConsumption() { return 0.0; }
    override float LFPG_GetCapacity() { return LFPG_DEFAULT_PASSTHROUGH_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }
    override void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded != val)
        {
            m_Overloaded = val;
            SetSynchDirty();
        }
        #endif
    }
    bool LFPG_GetSwitchOn() { return m_SwitchOn; }
    protected void LFPG_ResetSwitchState()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            dirty = true;
        }
        if (m_SwitchOn)
        {
            m_SwitchOn = false;
            dirty = true;
        }
        if (dirty)
        {
            SetSynchDirty();
        }
        #endif
    }
    override void LFPG_OnKilled()
    {
        LFPG_ResetSwitchState();
    }
    override void LFPG_OnWiresCut()
    {
        LFPG_ResetSwitchState();
    }
}
