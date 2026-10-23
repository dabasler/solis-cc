# qSOLIS scalar-field naming rules (GUI and CLI)

qSOLIS stores every persistent calculation result as a scalar field on the input
point cloud (or on a mesh's associated vertex cloud). GUI and CLI use the same
naming function, so these rules are deterministic and suitable for automation.

## Canonical grammar

The base name is:

```text
SOLIS <component-label> [<unit>] D<day> T<clock> <time-zone> [dur<hours>h dt<minutes>m] [<spatial>] R<resolution>
```

If that exact name already exists, qSOLIS appends a uniqueness suffix:

```text
 #2
 #3
 ...
```

Thus the complete form is:

```text
SOLIS <component-label> [<unit>] D<day> T<clock> <time-zone> [dur<hours>h dt<minutes>m] [<spatial>] R<resolution>[ #N]
```

### `<component-label>`

Base component is one of:

```text
Direct
Diffuse
Total
```

For mean irradiance, ` Mean` is appended:

```text
Direct Mean
Diffuse Mean
Total Mean
```

For visibility mode, ` Visibility` is appended after any `Mean` token:

```text
Direct Visibility
Diffuse Visibility
Direct Mean Visibility
```

`Total Visibility` is not generated because Total is undefined for visibility.

### `<unit>`

The unit appears inside literal square brackets:

```text
[fraction]   visibility mode
[W/m2]       instantaneous or mean irradiance
[Wh/m2]      integrated energy with WH selected
[kWh/m2]     integrated energy with KWH selected
```

### `D<day>`

Integer day of year, **not zero padded**:

```text
D1
D32
D175
D366
```

The fractional part of day-of-year is represented by the following time token,
not in the `D` token.

### `T<clock>`

Clock time is always zero-padded:

```text
T0800       08:00:00
T1200       12:00:00
T120530     12:05:30
```

Seconds are omitted when they are zero. Therefore the token is normally `T` plus
4 digits, or `T` plus 6 digits when non-zero seconds are present.

### `<time-zone>`

UTC calculations use:

```text
UTC
```

Local fixed-offset calculations use `L` followed by the signed UTC offset:

```text
L+1
L+2
L+5.5
L-5
L-9.5
```

This token describes how the clock is interpreted. It is separate from the
integration-duration token.

### `dur<hours>h dt<minutes>m`

Present for Mean and Integrated Energy calculations; absent for Instantaneous.
Examples:

```text
dur2h dt5m
dur24h dt10m
dur0.5h dt1m
```

Numbers are formatted compactly (trailing decimal zeroes removed). `dur25h`
means a 25-hour calculation window; it is **not** a UTC offset.

### `<spatial>`

For irradiance/energy modes:

```text
H   horizontal-reference irradiance
S   true surface irradiance
```

Visibility mode omits this one-letter token because `Visibility` is already
explicit in the component label.

### `R<resolution>`

The square visibility-render resolution:

```text
R512
R1024
R2048
```

### Duplicate suffix

If a name already exists, qSOLIS never overwrites it. It appends:

```text
 #2
 #3
 #4
```

The first result has no suffix. In the pathological case of nearly one million
identical duplicates, qSOLIS may use an 8-character UUID fragment instead of a
numeric suffix.

## Examples

```text
SOLIS Direct [W/m2] D175 T1200 UTC H R1024
SOLIS Direct [W/m2] D175 T1200 UTC H R1024 #2
SOLIS Diffuse [W/m2] D175 T1300 UTC H R1024
SOLIS Total [Wh/m2] D175 T0000 UTC dur24h dt5m H R1024
SOLIS Direct Mean [W/m2] D175 T1200 L+2 dur2h dt5m S R2048
SOLIS Direct Visibility [fraction] D175 T1200 UTC R1024
```

## Recommended extraction strategy for CLI workflows

The most robust method is to capture qSOLIS stdout. Each persistent field is
reported as:

```text
SOLIS: created scalar field '<exact field name>'
```

For example:

```bash
CloudCompare ... -SOLIS ... 2>&1 | tee solis.log
sed -n "s/.*SOLIS: created scalar field '\(.*\)'/\1/p" solis.log
```

This avoids reconstructing names and automatically handles `#2`, `#3`, etc.

CloudCompare commands that accept a scalar-field name can then be given the
captured name. Because qSOLIS names contain spaces and square brackets, quote the
full name in shell scripts.

Example shell variable:

```bash
SF_NAME="SOLIS Direct [W/m2] D175 T1200 UTC H R1024"
```

When using CloudCompare command-line operations that accept scalar-field names,
pass it as one quoted argument.

## Parsing names programmatically

A practical regular expression for normal numeric duplicate suffixes is:

```regex
^SOLIS (Direct|Diffuse|Total)( Mean)?( Visibility)? \[(fraction|W/m2|Wh/m2|kWh/m2)\] D([1-9][0-9]{0,2}) T([0-9]{4}|[0-9]{6}) (UTC|L[+-][0-9]+(?:\.[0-9]+)?)(?: dur([0-9]+(?:\.[0-9]+)?)h dt([0-9]+(?:\.[0-9]+)?)m)?(?: ([HS]))? R([0-9]+)(?: #([2-9][0-9]*))?$
```

Do not use the visible name as the sole source of scientific provenance. It is a
compact identifier, not a complete parameter record.

## Persistent metadata

For CloudCompare project/BIN workflows, qSOLIS also stores JSON metadata on the
associated point cloud:

```text
qSOLIS/result/<result-uuid>
qSOLIS/batch/<batch-uuid>
qSOLIS/lastRunId
qSOLIS/lastBatchId
qSOLIS/schemaVersion
qSOLIS/pluginVersion
```

Each result record includes the exact field name, `resultId`, `runId`, optional
`batchId`, component, unit, exact fractional DOY, site coordinates, time mode,
UTC offset, Linke turbidity, temporal/spatial modes, duration, timestep,
resolution, depth bias, sampling settings, qSOLIS plugin version, and creation timestamp.

Direct/Diffuse/Total from one time point share a `runId`. All time points from a
batch share a `batchId`.

If a user manually renames a qSOLIS scalar field after creation, the stored
metadata still contains the original name. Current metadata-driven cleanup can
then remove the stale metadata record but cannot safely identify the manually
renamed field; rename qSOLIS result fields only if you intend to manage them
manually afterward.
