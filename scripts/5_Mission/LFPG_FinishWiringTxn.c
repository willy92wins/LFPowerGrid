// FinishWiring store+graph transaction. Result codes and request bag for
// LFPG_NetworkManagerImpl.TryCommitFinishWiring. New file in 5_Mission so
// the facade in 4_World stays untouched (World arena budget).

class LFPG_FinishWiringTxn
{
    static const int OK = 0;
    static const int DENIED_FULL = 1;
    static const int DENIED_GRAPH = 2;
    static const int DENIED_STORE = 3;
    static const int DENIED_FOREIGN = 4;
}

class LFPG_FinishWiringTxnRequest
{
    EntityAI m_SrcObj;
    string m_SrcRealId;
    string m_DstRealId;
    string m_SrcPort;
    string m_DstPort;
    ref LFPG_WireData m_Wire;
    string m_CreatorId;
    bool m_AllowOthers;
    bool m_IsLfpgOwner;
}

class LFPG_FinishWiringRemovedWire
{
    string m_OwnerId;
    bool m_IsLfpgOwner;
    int m_Index;
    ref LFPG_WireData m_Wire;
}

// Full pre-detach sequence of one owner store. Restore must replay this
// order: the next rebuild admits edges in array order, not by identity.
class LFPG_FinishWiringStoreSnapshot
{
    string m_OwnerId;
    bool m_IsLfpgOwner;
    ref array<ref LFPG_WireData> m_Wires;
}
