// Tests for the DUNE-DAQ -> WCT bridge (ddm-3j8.1.5).

#include "dune_daq_codec/ChannelMap.hpp"
#include "dune_daq_arrow_frame_hdf/ToFrame.hpp"

#include "dune_daq_codec/Decode.hpp"

#include <iostream>
#include <string>

using namespace dune_daq_arrow_frame_hdf;

static int fails = 0;
static void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++fails;
}

int main()
{
    // A small decoded readout: 4 channels x 8 ticks.
    dune_daq_codec::DenseAdc adc;
    adc.n_channels = 4;
    adc.n_ticks = 8;
    adc.adcs.resize(adc.n_channels * adc.n_ticks);
    for (unsigned c = 0; c < adc.n_channels; ++c)
        for (std::size_t t = 0; t < adc.n_ticks; ++t)
            adc.adcs[c * adc.n_ticks + t] = static_cast<std::int16_t>(c * 100 + t);

    dune_daq_codec::StreamMeta meta{5, 3, 12, 2, 7, 0xABCDull};  // version,det,crate,slot,stream,ts

    dune_daq_codec::IdentityChannelMap cmap;
    auto traces = traces_from(adc, meta, cmap);
    check(traces.size() == 4, "one trace per channel");

    bool chan_ok = true, charge_ok = true, tbin_ok = true;
    for (unsigned c = 0; c < adc.n_channels; ++c) {
        const auto& tr = traces[c];
        if (tr->channel() != cmap.offline(meta.det_id, meta.crate_id, meta.slot_id, meta.stream_id, c))
            chan_ok = false;
        if (tr->tbin() != 0) tbin_ok = false;
        const auto& q = tr->charge();
        if (q.size() != adc.n_ticks) { charge_ok = false; continue; }
        for (std::size_t t = 0; t < adc.n_ticks; ++t)
            if (q[t] != static_cast<float>(adc.at(c, t))) charge_ok = false;
    }
    check(chan_ok, "trace channel id comes from the ChannelMap (offline)");
    check(tbin_ok, "trace tbin == 0");
    check(charge_ok, "trace charge == decoded ADC samples");

    // Distinct channels mapped distinctly.
    check(traces[0]->channel() != traces[1]->channel(), "distinct channels -> distinct ids");

    // Assemble a dense frame; tick/time are config (not from the DAQ payload).
    const double time = 1.5e-3, tick = 0.5;
    auto frame = make_frame(/*ident=*/77, time, tick, traces);
    check(frame->ident() == 77, "frame ident");
    check(frame->time() == time, "frame time (from config)");
    check(frame->tick() == tick, "frame tick (from config; not in DAQ data)");
    auto ftr = frame->traces();
    check(ftr && ftr->size() == 4, "frame carries the 4 traces");
    check(frame->frame_tags().empty() && frame->trace_tags().empty(),
          "raw readout has no frame/trace tags");

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "dune-daq-wct bridge OK\n";
    return 0;
}
