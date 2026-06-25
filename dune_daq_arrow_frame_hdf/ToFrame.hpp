#ifndef DUNE_DAQ_ARROW_FRAME_HDF_TOFRAME_HPP
#define DUNE_DAQ_ARROW_FRAME_HDF_TOFRAME_HPP

// Bridge decoded DUNE DAQ ADC data into the WCT data model (ddm-3j8.1.5).
//
// A decoded readout (dune_daq_codec::DenseAdc, one stream's channels x ticks,
// plus StreamMeta) becomes WCT ITraces with OFFLINE channel IDs (via a
// ChannelMap), which assemble into a WCT IFrame. That IFrame is exactly the
// `traces` part of the Arrow `wc.frame` (via the existing wire-cell-arrow
// to_arrow_dense); `frame_tags`/`trace_tags`/`cmm` are conceptually EMPTY for a
// raw readout.
//
// What the DAQ does NOT provide and must come from configuration / external
// knowledge: the sample period `tick` (differs between detector parts) and the
// frame `time`/`ident` (derived from the DAQ timestamp + the detector clock).
// Hence make_frame() takes these explicitly. (Conversely some DAQ metadata —
// crate/slot/stream, etc. — has no wc.frame home and is dropped here.)
//
// Depends on dune-daq-codec and WCT (WireCellIface/WireCellAux). Pure data
// transform — no HDF5, no Phlex, no Arrow (Arrow framing is downstream).

#include "dune_daq_codec/ChannelMap.hpp"

#include "dune_daq_codec/Decode.hpp"

#include "WireCellIface/IFrame.h"
#include "WireCellIface/ITrace.h"

namespace dune_daq_arrow_frame_hdf {

/// Build one WCT ITrace per channel of a decoded readout: channel id via the
/// ChannelMap (offline), tbin = 0, charge = the channel's ticks (int16 -> float).
WireCell::ITrace::vector traces_from(const dune_daq_codec::DenseAdc& adc,
                                     const dune_daq_codec::StreamMeta& meta,
                                     const dune_daq_codec::ChannelMap& channel_map);

/// Assemble a dense WCT IFrame from accumulated traces. `time` and `tick` are in
/// WCT system-of-units (seconds); they come from configuration / the DAQ
/// timestamp, NOT from the ADC payload.
WireCell::IFrame::pointer make_frame(int ident, double time, double tick,
                                     const WireCell::ITrace::vector& traces);

}  // namespace dune_daq_arrow_frame_hdf

#endif  // DUNE_DAQ_ARROW_FRAME_HDF_TOFRAME_HPP
