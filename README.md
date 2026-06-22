# dune-daq-arrow

End-to-end, **non-Phlex** pipeline gluing the DUNE-DAQ → WCT → Arrow packages
into a single tool, for visual/end-to-end validation of the channel map and the
decode chain (ddm-3j8.1.16).

```
DUNE DAQ HDF5
  → dune-daq-hdf      (fragments)
  → dune-daq-codec    (decode → dense ADC + stream metadata)
  → OnlineOfflineChannelMap   (online → OFFLINE channel id)
  → dune-daq-wct      (WCT IFrame: one trace per offline channel)
  → wire-cell-arrow   (IFrame → wc.frame Arrow tables, sparse)
  → arrow-hdf         (Arrow tables → HDF5)
```

The output HDF5 is the **same `wc.frame` representation the Phlex pipeline
writes** (product type `wc.frame`; member tables `traces`, `frame_tags`,
`trace_tags`, `cmm`), one frame per DAQ trigger record at `/<record group>/`.

## Build

Depends on the installed Configs for `wire_cell_arrow`, `arrow_hdf`,
`dune_daq_hdf`, `dune_daq_wct`, `dune_daq_codec` (and Arrow / WireCell / Boost /
HDF5 from the Spack view).

```
cmake -S source/dune-daq-arrow -B builds/dune-daq-arrow \
  -DCMAKE_PREFIX_PATH="$PWD/install;$PWD/local"
cmake --build builds/dune-daq-arrow
```

## Use

```
daq_hdf_to_arrow <in.hdf5> <out.hdf5> <channelmap.txt> [--tick=SECONDS] [--record=N]
```

- `channelmap.txt` — a DUNE `ChannelMap*.txt` (12- or 13-column, auto-detected);
  use the table matching the detector of `<in.hdf5>` (see
  `dune-daq-codec/data/channelmaps`).
- `--tick` — sample period in seconds (default `0.5e-6` = 2 MHz). The DAQ payload
  does not carry it; it is detector configuration.
- `--record=N` — only the N-th record (0-based, file order); default all.

It reports per record how many traces/fragments were processed and, crucially,
how many channels came back **UNMAPPED** (offline id `< 0`) — a large count means
the channel map (or the channel-index convention) does not match the data.

### Extracting one trigger record

A whole DAQ file is ~4 GB. Extract a single record into its own file with the
stock HDF5 tool `h5copy`:

```
h5copy -i big.hdf5 -o one_record.hdf5 \
  -s /TriggerRecord00150.0000 -d /TriggerRecord00150.0000 -p
```

## Note on the channel-index convention (ddm-3j8.1.16)

The decoder emits a stream-local channel index `c = 0..63` (= `streamchan`),
which matches the **12-column warm** maps directly. The **13-column cold** maps
key on `wibframechan` (0..255), a different numbering — so running a cold map
against WIBEth data shows many UNMAPPED channels. Resolving that pairing per
detector/frame-format is the open part of ddm-3j8.1.16; this tool is the
diagnostic for it.
