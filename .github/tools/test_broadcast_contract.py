"""Static producer/consumer contract checks; does not execute Enforce or the engine."""
import re
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
def body(text, name):
    text = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/', '', text)
    start = re.search(r'\b' + name + r'\([^;\n]*\)\s*\{', text).end()
    pos, depth = start, 1
    while depth:
        depth += (text[pos] == '{') - (text[pos] == '}')
        pos += 1
    return text[start:pos-1]
def block(text, marker):
    start = text.index('{', text.index(marker)) + 1
    pos, depth = start, 1
    while depth:
        depth += (text[pos] == '{') - (text[pos] == '}')
        pos += 1
    return start, pos, text[start:pos-1]
class BroadcastContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = (ROOT/'scripts/5_Mission/LFPG_NetworkManagerImpl.c').read_text(encoding='utf-8')
        cls.client = (ROOT/'scripts/4_World/LFPG_RPCClientHandler.c').read_text(encoding='utf-8')
    def test_client_wire_contract(self):
        self.assertEqual(re.findall(r'ctx.Read\((\w+)\)', body(self.client, 'HandleSyncOwnerWiresV2')), ['ownerDeviceId','low','high','json','generation'])
        self.assertEqual(re.findall(r'ctx.Read\((\w+)\)', body(self.client, 'HandleSyncOwnerWiresDelta')), ['ownerDeviceId','low','high','generation','entryCount','operation','wireJson'])
    def test_producer_fields_and_explicit_recipient(self):
        payloads = {
            'BroadcastOwnerWires':['(int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2','ownerId','low','high','json','snapshotGeneration'],
            'BroadcastOwnerSnapshot':['(int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2','snapshot.m_OwnerDeviceId','snapshot.m_OwnerLow','snapshot.m_OwnerHigh','snapshot.m_JSON','snapshot.m_Generation'],
            'BroadcastOwnerWireDelta':['(int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_DELTA','ownerId','low','high','generation','entryCount','operations[e]','entryJsons[e]'],
            'BroadcastVanillaWires':['(int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2','ownerDeviceId','low','high','json','vanillaSnapshotGeneration'],
        }
        for name, fields in payloads.items():
            with self.subTest(name=name):
                code=body(self.server,name)
                self.assertEqual(re.findall(r'rpc.Write\((.*)\);',code),fields)
                self.assertRegex(code,r'PlayerIdentity recipient = \w+.GetIdentity\(\);\s*if \(!recipient\)\s*continue;')
                target='player' if name=='BroadcastOwnerSnapshot' else 'pb'
                self.assertRegex(code,rf'rpc.Send\({target}, LFPG_RPC_CHANNEL, (true|guaranteed|bRpcGuaranteed), recipient\);')
    def test_buffer_lifetime_is_one_broadcast_not_one_recipient(self):
        for name in ['BroadcastOwnerWires','BroadcastOwnerSnapshot','BroadcastOwnerWireDelta','BroadcastVanillaWires']:
            with self.subTest(name=name):
                code=body(self.server,name)
                self.assertIn('ScriptRPC rpc = null;', code)
                self.assertEqual(code.count('new ScriptRPC()'),1)
                start,end,serialization=block(code,'if (!rpc)')
                self.assertIn('new ScriptRPC()',serialization)
                self.assertNotIn('rpc.Write',code[:start]+code[end:])
                self.assertNotIn('rpc.Send',serialization)
                self.assertNotIn('rpc.Reset',code)
                self.assertNotIn('rpc = null;',code[code.index('m_ReusableBroadcastPlayers.Count()'):])
                self.assertLess(code.index('if (!recipient)'),start)
if __name__=='__main__':
    unittest.main()
