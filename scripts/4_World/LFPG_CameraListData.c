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

    void LFPG_CameraListEntry()
    {
        m_Pos   = "0 0 0";
        m_Ori   = "0 0 0";
        m_Label = "";
    }
};
