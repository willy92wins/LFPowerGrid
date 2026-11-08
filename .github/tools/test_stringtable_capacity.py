"""Stringtable capacity and Laser/Memory Cell description rows."""
from pathlib import Path
import csv
import unittest

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / "stringtable.csv"

KEYS = [
    "STR_LFPG_AND_Gate_Desc",
    "STR_LFPG_OR_Gate_Desc",
    "STR_LFPG_XOR_Gate_Desc",
    "STR_LFPG_PressurePad_Desc",
    "STR_LFPG_LaserDetector_Desc",
    "STR_LFPG_MemoryCell_Desc",
]

LASER = {
    "original": "Passes power while a player or an occupied vehicle crosses the laser beam. 100 u/s capacity.",
    "english": "Passes power while a player or an occupied vehicle crosses the laser beam. 100 u/s capacity.",
    "spanish": "Transmite energía mientras un jugador o un vehículo ocupado cruza el rayo láser. 100 u/s.",
    "german": "Leitet Strom, solange ein Spieler oder ein besetztes Fahrzeug den Laserstrahl kreuzt. 100 E/s.",
    "russian": "Передаёт энергию, пока игрок или транспорт с людьми пересекает лазерный луч. 100 ед/с.",
    "chinesesimp": "玩家或有人乘坐的载具穿过激光束时通电。容量 100 u/s。",
    "portuguese": "Transmite energia enquanto um jogador ou um veículo ocupado atravessa o feixe de laser. Capacidade de 100 u/s.",
}

MEMORY = {
    "original": "Stateful latch: 4 inputs (power, toggle, reset, set) + 2 outputs (normal, inverted). Each Toggle pulse flips the state. 100 u/s capacity, 1 u/s self-consumption.",
    "english": "Stateful latch: 4 inputs (power, toggle, reset, set) + 2 outputs (normal, inverted). Each Toggle pulse flips the state. 100 u/s capacity, 1 u/s self-consumption.",
    "spanish": "Latch con estado: 4 entradas (alimentación, toggle, reset, set) + 2 salidas (normal, invertida). Cada pulso en Toggle invierte el estado. 100 u/s, 1 u/s de autoconsumo.",
    "german": "Zustandsspeicher: 4 Eingänge (Strom, Toggle, Reset, Set) + 2 Ausgänge (normal, invertiert). Jeder Toggle-Impuls kehrt den Zustand um. 100 u/s, 1 u/s Eigenverbrauch.",
    "russian": "Защёлка: 4 входа (питание, переключение, сброс, установка) + 2 выхода (нормальный, инвертированный). Каждый импульс на входе переключения меняет состояние. 100 ед/с, 1 ед/с потребление.",
    "chinesesimp": "有状态锁存器：4 输入（电源、切换、复位、置位）+ 2 输出（正常、反相）。切换输入每收到一个脉冲就翻转状态。容量 100 u/s，自耗 1 u/s。",
    "portuguese": "Latch com estado: 4 entradas (energia, alternar, redefinir, definir) + 2 saídas (normal, invertida). Cada pulso em Alternar inverte o estado. Capacidade de 100 u/s, consumo próprio de 1 u/s.",
}

BANNED = ("20 u/s", "20 E/s", "20 ед/с")


def rows():
    with TABLE.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


class StringtableCapacity(unittest.TestCase):
    def test_s4_rows(self):
        data = rows()
        by_key = {row["Language"]: row for row in data}
        width = len(data[0].keys())
        for row in data:
            self.assertEqual(len(row.keys()), width)
        self.assertEqual(width, 8)
        text_cols = [c for c in data[0].keys() if c != "Language"]
        for key in KEYS:
            self.assertIn(key, by_key)
            row = by_key[key]
            for col in text_cols:
                self.assertIn("100", row[col], key + " " + col)
                for banned in BANNED:
                    self.assertNotIn(banned, row[col])
        laser = by_key["STR_LFPG_LaserDetector_Desc"]
        memory = by_key["STR_LFPG_MemoryCell_Desc"]
        for col, expected in LASER.items():
            self.assertEqual(laser[col], expected, col)
        for col, expected in MEMORY.items():
            self.assertEqual(memory[col], expected, col)


if __name__ == "__main__":
    unittest.main()
