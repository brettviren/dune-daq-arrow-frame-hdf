// Test the file-driven ChannelMap through the bridge (ddm-3j8.1.13).
//
// Reference-free: writes a small synthetic 12-column (warm) ChannelMap*.txt
// whose online key matches the StreamMeta below, then checks that traces_from()
// emits the OFFLINE channel ids from the parsed map (not the online identity).

#include "dune_daq_codec/OnlineOfflineChannelMap.hpp"
#include "dune_daq_arrow_frame_hdf/ToFrame.hpp"

#include "dune_daq_codec/Decode.hpp"

#include <fstream>
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
    // StreamMeta the readout will carry: det=3, crate=12, slot=2, stream=7.
    const dune_daq_codec::StreamMeta meta{5, 3, 12, 2, 7, 0xABCDull};

    // A warm 12-col table: offlchan detid detelement crate slot stream streamchan
    //                      plane chan_in_plane femb asic asicchan
    // local channel c (= streamchan) maps to offline 1000+c for this stream.
    const std::string map_path = "bridge_chanmap.txt";
    {
        std::ofstream os(map_path);
        for (unsigned c = 0; c < 4; ++c) {
            os << (1000 + c) << " " << meta.det_id << " 0 " << meta.crate_id << " "
               << meta.slot_id << " " << meta.stream_id << " " << c << "  0 " << c
               << " 1 1 0\n";
        }
    }

    dune_daq_codec::OnlineOfflineChannelMap cmap(map_path);
    check(cmap.channels().schema() ==
              dune_daq_codec::OnlineOfflineChannels::Schema::warm_electronics_12,
          "bridge map: detected warm 12-col schema");
    check(cmap.channels().size() == 4, "bridge map: 4 rows parsed");

    // A 4-channel x 8-tick readout.
    dune_daq_codec::DenseAdc adc;
    adc.n_channels = 4;
    adc.n_ticks = 8;
    adc.adcs.resize(adc.n_channels * adc.n_ticks);
    for (unsigned c = 0; c < adc.n_channels; ++c)
        for (std::size_t t = 0; t < adc.n_ticks; ++t)
            adc.adcs[c * adc.n_ticks + t] = static_cast<std::int16_t>(c * 100 + t);

    auto traces = traces_from(adc, meta, cmap);
    check(traces.size() == 4, "one trace per channel");

    bool ids_ok = true;
    for (unsigned c = 0; c < adc.n_channels; ++c)
        if (traces[c]->channel() != static_cast<int>(1000 + c)) ids_ok = false;
    check(ids_ok, "trace channel ids are the OFFLINE ids from the file map");

    // Sanity: these differ from the placeholder identity mapping.
    dune_daq_codec::IdentityChannelMap ident;
    check(cmap.offline(meta.det_id, meta.crate_id, meta.slot_id, meta.stream_id, 0) !=
              ident.offline(meta.det_id, meta.crate_id, meta.slot_id, meta.stream_id, 0),
          "file map differs from identity placeholder");

    // Charge still passes through unchanged.
    bool charge_ok = true;
    for (unsigned c = 0; c < adc.n_channels; ++c) {
        const auto& q = traces[c]->charge();
        if (q.size() != adc.n_ticks) { charge_ok = false; continue; }
        for (std::size_t t = 0; t < adc.n_ticks; ++t)
            if (q[t] != static_cast<float>(adc.at(c, t))) charge_ok = false;
    }
    check(charge_ok, "trace charge == decoded ADC samples");

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "dune-daq-wct file ChannelMap OK\n";
    return 0;
}
