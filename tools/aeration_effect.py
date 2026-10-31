#!/usr/bin/env python3
"""Compare water quality while the aerator runs and while it is off.

Input (one or more files):
  * gateway serial logs: lines starting with "CSV," (firmware/libraries/PondCore/src/PondGateway.h),
    optionally prefixed with the Arduino IDE timestamp "HH:MM:SS.mmm -> ";
  * or a CSV file with a header containing at least `aerator` and `do_mgl`, plus an
    optional `time` column (ISO 8601 or Unix seconds).

Why not just compare the two means: when the node switches the aerator on low dissolved
oxygen (DO), "on" periods are low-DO periods by construction, and DO also follows a daily
cycle. The script therefore also reports
  * the difference within the same hour of day (needs timestamps), and
  * the DO rate of change (mg/L per hour) during on and off periods.
For a clean result, run the aerator on a fixed schedule (e.g. alternate days) for two weeks.

Usage: python3 tools/aeration_effect.py gateway_log.txt [more files] [--column turbidity_ntu]
Standard library only.
"""
from __future__ import annotations

import argparse
import csv
import math
import re
import statistics
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Iterable, List, Optional

GATEWAY_COLUMNS = ["ms", "seq", "temp_c", "ph", "turbidity_ntu", "tds_ppm", "do_mgl",
                   "aerator", "vcc_v", "status_hex", "loss_pct"]
IDE_TIMESTAMP = re.compile(r"^(\d{2}):(\d{2}):(\d{2})(?:\.\d+)?\s*->\s*")


@dataclass
class Sample:
    hour: Optional[int]       # hour of day, if known
    t_hours: Optional[float]  # monotonic time in hours, if known
    aerator: int
    values: dict


def _number(text: str) -> float:
    try:
        value = float(text)
    except (TypeError, ValueError):
        return math.nan
    return value


def _parse_time(text: str):
    """Returns (hour_of_day, hours_since_epoch) or (None, None)."""
    text = (text or "").strip()
    if not text:
        return None, None
    try:
        seconds = float(text)
        moment = datetime.fromtimestamp(seconds, tz=timezone.utc)
        return moment.hour, seconds / 3600.0
    except ValueError:
        pass
    try:
        moment = datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError:
        return None, None
    return moment.hour, moment.timestamp() / 3600.0


def read_samples(lines: Iterable[str]) -> List[Sample]:
    lines = list(lines)
    samples: List[Sample] = []
    if any("CSV," in line for line in lines):
        day_offset = 0.0
        previous_clock = None
        for line in lines:
            hour = None
            clock_hours = None
            match = IDE_TIMESTAMP.match(line)
            if match:
                h, m, s = (int(g) for g in match.groups())
                hour = h
                clock = h + m / 60 + s / 3600
                if previous_clock is not None and clock < previous_clock:
                    day_offset += 24.0  # crossed midnight
                previous_clock = clock
                clock_hours = clock + day_offset
                line = line[match.end():]
            line = line.strip()
            if not line.startswith("CSV,"):
                continue
            fields = line[4:].split(",")
            if len(fields) != len(GATEWAY_COLUMNS):
                continue
            row = dict(zip(GATEWAY_COLUMNS, fields))
            if clock_hours is None:
                clock_hours = _number(row["ms"]) / 3_600_000.0
            aerator = int(_number(row["aerator"])) if not math.isnan(_number(row["aerator"])) else -1
            if aerator not in (0, 1):
                continue
            values = {k: _number(v) for k, v in row.items() if k not in ("status_hex",)}
            samples.append(Sample(hour, clock_hours, aerator, values))
        return samples

    reader = csv.DictReader(lines)
    for row in reader:
        aerator = _number(row.get("aerator", ""))
        if aerator not in (0.0, 1.0):
            continue
        hour, t_hours = _parse_time(row.get("time", ""))
        values = {k: _number(v) for k, v in row.items() if k not in ("time",)}
        samples.append(Sample(hour, t_hours, int(aerator), values))
    return samples


def _clean(values):
    return [v for v in values if not math.isnan(v)]


def state_summary(samples: List[Sample], column: str):
    result = {}
    for state in (0, 1):
        values = _clean(s.values.get(column, math.nan) for s in samples if s.aerator == state)
        result[state] = (len(values), statistics.fmean(values) if values else math.nan,
                         statistics.median(values) if values else math.nan)
    return result


def same_hour_difference(samples: List[Sample], column: str):
    """Mean(on) - mean(off) within each hour of day, weighted by min(n_on, n_off)."""
    total = 0.0
    weight = 0
    for hour in range(24):
        on = _clean(s.values.get(column, math.nan) for s in samples if s.hour == hour and s.aerator == 1)
        off = _clean(s.values.get(column, math.nan) for s in samples if s.hour == hour and s.aerator == 0)
        if on and off:
            w = min(len(on), len(off))
            total += w * (statistics.fmean(on) - statistics.fmean(off))
            weight += w
    return (total / weight, weight) if weight else (math.nan, 0)


def rate_by_state(samples: List[Sample], column: str, max_gap_hours: float = 0.5):
    """Mean rate of change (units per hour) between consecutive samples in the same state."""
    rates = {0: [], 1: []}
    ordered = sorted((s for s in samples if s.t_hours is not None), key=lambda s: s.t_hours)
    for a, b in zip(ordered, ordered[1:]):
        dt = b.t_hours - a.t_hours
        va, vb = a.values.get(column, math.nan), b.values.get(column, math.nan)
        if a.aerator == b.aerator and 0 < dt <= max_gap_hours and not math.isnan(va) and not math.isnan(vb):
            rates[a.aerator].append((vb - va) / dt)
    return {state: (len(r), statistics.fmean(r) if r else math.nan) for state, r in rates.items()}


def report(samples: List[Sample], column: str) -> str:
    out = [f"samples: {len(samples)} (aerator on: {sum(s.aerator == 1 for s in samples)}, "
           f"off: {sum(s.aerator == 0 for s in samples)}), column: {column}"]
    summary = state_summary(samples, column)
    for state, label in ((1, "aerator ON "), (0, "aerator OFF")):
        n, mean, median = summary[state]
        out.append(f"  {label}: n={n:5d}  mean={mean:8.3f}  median={median:8.3f}")
    diff, weight = same_hour_difference(samples, column)
    out.append(f"  ON - OFF within the same hour of day: {diff:+.3f} (weight {weight})"
               if weight else "  ON - OFF within the same hour of day: n/a (no timestamps or no overlap)")
    rates = rate_by_state(samples, column)
    out.append(f"  rate of change while ON : {rates[1][1]:+.3f} per hour (n={rates[1][0]})")
    out.append(f"  rate of change while OFF: {rates[0][1]:+.3f} per hour (n={rates[0][0]})")
    out.append("  note: with DO-triggered control, ON periods start at low DO; prefer the same-hour")
    out.append("        difference and the rates, or a fixed on/off schedule (see docstring).")
    return "\n".join(out)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("files", nargs="+")
    parser.add_argument("--column", default="do_mgl", help="value to compare (default: do_mgl)")
    args = parser.parse_args(argv)
    lines: List[str] = []
    for name in args.files:
        with open(name, encoding="utf-8", errors="replace") as handle:
            lines.extend(handle.read().splitlines())
    samples = read_samples(lines)
    if not samples:
        print("no samples with aerator state 0/1 found", file=sys.stderr)
        return 1
    print(report(samples, args.column))
    return 0


if __name__ == "__main__":
    sys.exit(main())
