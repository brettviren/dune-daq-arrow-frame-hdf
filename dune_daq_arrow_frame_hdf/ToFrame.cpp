#include "dune_daq_arrow_frame_hdf/ToFrame.hpp"

#include "WireCellAux/SimpleFrame.h"
#include "WireCellAux/SimpleTrace.h"

#include <memory>
#include <vector>

namespace dune_daq_arrow_frame_hdf {

WireCell::ITrace::vector traces_from(const dune_daq_codec::DenseAdc& adc,
                                     const dune_daq_codec::StreamMeta& meta,
                                     const dune_daq_codec::ChannelMap& channel_map)
{
    WireCell::ITrace::vector traces;
    traces.reserve(adc.n_channels);
    for (unsigned c = 0; c < adc.n_channels; ++c) {
        const int chid = channel_map.offline(meta.det_id, meta.crate_id, meta.slot_id,
                                             meta.stream_id, c);
        WireCell::ITrace::ChargeSequence charge(adc.n_ticks);
        for (std::size_t t = 0; t < adc.n_ticks; ++t) {
            charge[t] = static_cast<float>(adc.at(c, t));
        }
        traces.push_back(std::make_shared<WireCell::Aux::SimpleTrace>(chid, 0, charge));
    }
    return traces;
}

WireCell::IFrame::pointer make_frame(int ident, double time, double tick,
                                     const WireCell::ITrace::vector& traces)
{
    return std::make_shared<WireCell::Aux::SimpleFrame>(ident, time, traces, tick);
}

}  // namespace dune_daq_arrow_frame_hdf
