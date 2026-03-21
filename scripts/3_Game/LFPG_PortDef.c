// =========================================================
// LF_PowerGrid - Port Definition (v4.0, Refactor)
//
// Lightweight struct describing a single port on an LFPG device.
// Stored in LFPG_DeviceBase.m_Ports array.
// No logic — pure data.
//
// v4.0: Extracted from per-device inline methods into shared struct.
// =========================================================

class LFPG_PortDef
{
    string m_Name;    // "input_1", "output_1", etc.
    int    m_Dir;     // LFPG_PortDir.IN or LFPG_PortDir.OUT
    string m_Label;   // Human-readable: "Input", "Output 1", etc.

    void LFPG_PortDef()
    {
        m_Name  = "";
        m_Dir   = LFPG_PortDir.IN;
        m_Label = "";
    }
};
