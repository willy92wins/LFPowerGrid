"""Offline source slice: inspector SOURCE overload uses the node flag.

P1/P2/N1 on LFPG_DeviceInspector.c. M1 runs the same predicates against
the aa96d35 SOURCE blocks, stored as literals (no git history).
"""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
INSPECTOR = ROOT / "scripts/4_World/LFPG_DeviceInspector.c"

# SOURCE builder at aa96d35 (merge of #84). Literal: do not git-show.
OLD_BUILDER = """
        if (deviceType == LFPG_DeviceType.SOURCE)
        {
            boolA = LFPG_DeviceAPI.GetSourceOn(device);
            floatA = LFPG_DeviceAPI.GetLoadRatio(device);
            floatF = LFPG_DeviceAPI.GetCapacity(device);
        }
        bool changed = (!m_ClientSnapshotValid || deviceType != m_SnapshotDeviceType || intA != m_SnapshotIntA || intB != m_SnapshotIntB || intC != m_SnapshotIntC || boolA != m_SnapshotBoolA || boolB != m_SnapshotBoolB || boolC != m_SnapshotBoolC || floatA != m_SnapshotFloatA || floatB != m_SnapshotFloatB || floatC != m_SnapshotFloatC || floatD != m_SnapshotFloatD || floatE != m_SnapshotFloatE || floatF != m_SnapshotFloatF || floatG != m_SnapshotFloatG || snapshotEntity != m_SnapshotEntity);
        m_SnapshotBoolC = boolC;
"""

# SOURCE render at aa96d35. Literal: do not git-show.
OLD_RENDER = """
        if (devType == LFPG_DeviceType.SOURCE)
        {
			bool sourceOn = m_SnapshotBoolA;
            if (sourceOn)
            {
				float loadRatio = m_SnapshotFloatA;
                int loadPct = Math.Round(loadRatio * 100.0);
                if (loadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD)
                {
                    statusText = Loc("#STR_LFPG_INSPECT_OVERLOAD");
                    statusText = statusText + "  ";
                    statusColor = COL_RED_ERROR;
                }
                else
                {
                    statusText = Loc("#STR_LFPG_INSPECT_ACTIVE");
                    statusText = statusText + "  ";
                    statusColor = COL_GREEN_OK;
                }
                string barStr = BuildLoadBar(loadRatio);
                statusText = statusText + barStr;
                statusText = statusText + " ";
                statusText = statusText + loadPct.ToString();
                statusText = statusText + "%";
            }
            else
            {
                statusText = Loc("#STR_LFPG_INSPECT_INACTIVE");
                statusColor = COL_GRAY_DIM;
            }
        }
"""


def extract_block(src, start_token, required=True):
    idx = src.find(start_token)
    if idx < 0:
        if required:
            raise ValueError("block start missing: " + start_token)
        return ""
    brace = src.find("{", idx)
    depth, pos = 1, brace + 1
    while depth:
        if src[pos] == "{":
            depth = depth + 1
        elif src[pos] == "}":
            depth = depth - 1
        pos = pos + 1
    return src[idx:pos]


def source_builder(src):
    fn = extract_block(src, "protected bool ClientDataChanged(EntityAI device)")
    return extract_block(fn, "if (deviceType == LFPG_DeviceType.SOURCE)")


def source_render(src):
    marker = 'string statusText = "";'
    idx = src.find(marker)
    if idx < 0:
        raise ValueError("statusText marker missing")
    return extract_block(src[idx:], "if (devType == LFPG_DeviceType.SOURCE)")


def p1_ok(builder, full_fn):
    """SOURCE branch assigns GetOverloaded into a var that lands on m_SnapshotBoolD;
    m_SnapshotBoolD is part of changed."""
    if "GetOverloaded(device)" not in builder:
        return False
    if "boolD = LFPG_DeviceAPI.GetOverloaded(device)" not in builder:
        return False
    if "m_SnapshotBoolD = boolD" not in full_fn:
        return False
    if "boolD != m_SnapshotBoolD" not in full_fn:
        return False
    return True


def p2_ok(render):
    overload = extract_block(render, "if (m_SnapshotBoolD)", required=False)
    if not overload:
        return False
    if '#STR_LFPG_INSPECT_OVERLOAD' not in overload:
        return False
    if "BuildLoadBar" in overload.split("else")[0]:
        return False
    active = extract_block(render, "else")
    if '#STR_LFPG_INSPECT_ACTIVE' not in active:
        return False
    if "BuildLoadBar" not in active:
        return False
    if "%" not in active:
        return False
    return True


def n1_ok(render):
    return "loadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD" not in render


class InspectorSourceOverload(unittest.TestCase):
    def setUp(self):
        self.src = INSPECTOR.read_text(encoding="utf-8")
        self.fn = extract_block(
            self.src, "protected bool ClientDataChanged(EntityAI device)")
        self.builder = source_builder(self.src)
        self.render = source_render(self.src)

    def test_p1_builder_snapshots_overloaded_flag(self):
        self.assertTrue(p1_ok(self.builder, self.fn))

    def test_p2_render_overload_from_flag_active_keeps_bar(self):
        self.assertTrue(p2_ok(self.render))

    def test_n1_render_drops_load_ratio_threshold(self):
        self.assertTrue(n1_ok(self.render))

    def test_m1_old_blocks_fail_predicates(self):
        old_fn = OLD_BUILDER
        old_builder = extract_block(
            OLD_BUILDER, "if (deviceType == LFPG_DeviceType.SOURCE)")
        old_render = extract_block(
            OLD_RENDER, "if (devType == LFPG_DeviceType.SOURCE)")
        self.assertFalse(p1_ok(old_builder, old_fn))
        self.assertFalse(p2_ok(old_render))
        self.assertFalse(n1_ok(old_render))


if __name__ == "__main__":
    result = unittest.main(exit=False)
    sys.exit(0 if result.result.wasSuccessful() else 1)
