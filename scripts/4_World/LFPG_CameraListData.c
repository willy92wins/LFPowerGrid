#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid - Camera list data (v0.9.2 - Sprint B)
//
// Pure data class for server → client camera list response.
// Used by:
//   Server: HandleLFPG_RequestCameraList builds array of entries
//   Client: HandleLFPG_CameraListResponse reads and passes to CameraViewport
//
// No entity references. No logic. Safe for 3_Game.
// =========================================================

class LFPG_CameraListEntry
{
    vector m_Pos;
    vector m_Ori;
    string m_Label;
    int m_NetLow;
    int m_NetHigh;
    float m_YawOffset;
    float m_PitchOffset;

    void LFPG_CameraListEntry()
    {
        m_Pos   = "0 0 0";
        m_Ori   = "0 0 0";
        m_Label = "";
        m_NetLow = 0;
        m_NetHigh = 0;
        m_YawOffset = 0.0;
        m_PitchOffset = 0.0;
    }
};
#endif

#ifdef SERVER
// Server-side data-surface stub required by LFPG_CameraViewport.EnterFromList.
// The client data class above remains under its existing compilation boundary.
class LFPG_CameraListEntry
{
    vector m_Pos;
    vector m_Ori;
    string m_Label;
    int m_NetLow;
    int m_NetHigh;
    float m_YawOffset;
    float m_PitchOffset;

    void LFPG_CameraListEntry()
    {
        m_Pos = "0 0 0";
        m_Ori = "0 0 0";
        m_Label = "";
        m_NetLow = 0;
        m_NetHigh = 0;
        m_YawOffset = 0.0;
        m_PitchOffset = 0.0;
    }
};
#endif
