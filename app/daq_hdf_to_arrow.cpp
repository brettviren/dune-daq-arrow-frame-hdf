// daq_hdf_to_arrow — non-Phlex end-to-end pipeline for visual validation
// (ddm-3j8.1.16):
//
//   DUNE DAQ HDF5  ->  dune-daq-hdf (fragments)
//                  ->  dune-daq-codec (decode -> dense ADC + stream meta)
//                  ->  OnlineOfflineChannelMap (online -> OFFLINE channel id)
//                  ->  dune-daq-wct (WCT IFrame: one trace per offline channel)
//                  ->  wire-cell-arrow (IFrame -> wc.frame Arrow tables)
//                  ->  arrow-hdf (Arrow tables -> HDF5)
//
// The output HDF5 is the SAME wc.frame representation the real Phlex pipeline
// writes (type "wc.frame"; member tables traces/frame_tags/trace_tags/cmm via
// to_arrow_sparse), one frame per DAQ trigger record, at /<record group>/.
//
// Usage:
//   daq_hdf_to_arrow <in.hdf5> <out.hdf5> <channelmap.txt> [--tick=SECONDS] [--record=N]
//
//   channelmap.txt : a DUNE ChannelMap*.txt (12- or 13-col; auto-detected). Use
//                    the table matching the detector of <in.hdf5>.
//   --tick         : sample period in seconds (default 0.5e-6 = 2 MHz). The DAQ
//                    payload does not carry it; it is detector configuration.
//   --record=N     : only the N-th record (0-based) in file order; default all.

#include "dune_daq_hdf/DaqHdf5File.hpp"
#include "dune_daq_codec/Decode.hpp"
#include "dune_daq_wct/OnlineOfflineChannelMap.hpp"
#include "dune_daq_wct/ToFrame.hpp"
#include "wire_cell_arrow/Converters.hpp"
#include "arrow_hdf/Address.hpp"
#include "arrow_hdf/Hdf5File.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

// TPC ADC fragment types this pipeline decodes (dune-daq-codec descriptors).
bool is_tpc_adc(dune_daq::FragmentType t)
{
    return t == dune_daq::FragmentType::kWIBEth || t == dune_daq::FragmentType::kTDEEth;
}

int fail(const std::string& msg)
{
    std::cerr << "daq_hdf_to_arrow: " << msg << "\n";
    return 1;
}

}  // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> pos;
    double tick = 0.5e-6;   // 2 MHz default
    long only_record = -1;  // -1 = all
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--tick=", 0) == 0) tick = std::strtod(a.c_str() + 7, nullptr);
        else if (a.rfind("--record=", 0) == 0) only_record = std::strtol(a.c_str() + 9, nullptr, 10);
        else pos.push_back(a);
    }
    if (pos.size() != 3) {
        return fail("usage: daq_hdf_to_arrow <in.hdf5> <out.hdf5> <channelmap.txt> "
                    "[--tick=SECONDS] [--record=N]");
    }
    const std::string in_path = pos[0], out_path = pos[1], map_path = pos[2];

    try {
        dune_daq_hdf::DaqHdf5File in(in_path);
        dune_daq_wct::OnlineOfflineChannelMap cmap(map_path);
        std::cerr << "channel map: " << map_path << " (schema "
                  << dune_daq_codec::OnlineOfflineChannels::schema_name(cmap.channels().schema())
                  << ", " << cmap.channels().size() << " channels)\n";

        auto out_r = arrow_hdf::Hdf5File::create(out_path);
        if (!out_r.ok()) return fail("create " + out_path + ": " + out_r.status().ToString());
        arrow_hdf::Hdf5File out = std::move(*out_r);

        const auto records = in.records();
        std::cerr << in_path << ": " << records.size() << " record(s)\n";

        long written = 0;
        for (std::size_t ri = 0; ri < records.size(); ++ri) {
            if (only_record >= 0 && static_cast<long>(ri) != only_record) continue;
            const auto& rid = records[ri];

            WireCell::ITrace::vector traces;
            long n_frag = 0, n_skip = 0, n_invalid = 0;
            for (const auto& fi : in.fragments(rid)) {
                if (!is_tpc_adc(fi.type)) continue;
                auto bytes = in.read_bytes(fi.dataset_path);
                dune_daq_codec::DecodedFragment dec;
                try {
                    dec = dune_daq_codec::decode(std::span<const std::byte>(bytes));
                } catch (const std::exception& e) {
                    std::cerr << "  skip " << fi.dataset_path << ": " << e.what() << "\n";
                    ++n_skip;
                    continue;
                }
                auto tr = dune_daq_wct::traces_from(dec.adc, dec.meta, cmap);
                for (const auto& t : tr) if (t->channel() < 0) ++n_invalid;
                traces.insert(traces.end(), tr.begin(), tr.end());
                ++n_frag;
            }

            if (traces.empty()) {
                std::cerr << "  " << rid.group << ": no TPC ADC traces; skipped\n";
                continue;
            }

            // Emit traces in ascending OFFLINE channel-id order. The DAQ delivers
            // them in online/hardware (per-fragment) order; after the online->offline
            // map is applied, sorting here makes the frame's row order follow the
            // offline convention (planes become contiguous channel ranges), which
            // is what offline consumers/displays expect. (Any unmapped channels,
            // id < 0, sort to the front.)
            std::stable_sort(traces.begin(), traces.end(),
                             [](const WireCell::ITrace::pointer& a, const WireCell::ITrace::pointer& b) {
                                 return a->channel() < b->channel();
                             });

            auto frame = dune_daq_wct::make_frame(static_cast<int>(rid.number), 0.0, tick, traces);
            auto fr = WireCell::Arrow::to_arrow_sparse(frame);
            if (!fr.ok()) return fail("to_arrow_sparse: " + fr.status().ToString());

            std::map<std::string, std::shared_ptr<arrow::Table>> members{
                {"traces", fr->traces},
                {"frame_tags", fr->frame_tags},
                {"trace_tags", fr->trace_tags},
                {"cmm", fr->cmm}};
            arrow_hdf::Address base(std::vector<std::string>{rid.group});
            auto st = out.write_tables(base, members, "wc.frame");
            if (!st.ok()) return fail("write_tables " + base.path() + ": " + st.ToString());

            std::cerr << "  " << rid.group << ": " << traces.size() << " traces from " << n_frag
                      << " fragment(s)";
            if (n_skip) std::cerr << ", " << n_skip << " skipped";
            if (n_invalid) std::cerr << ", " << n_invalid << " UNMAPPED channels (id<0 — wrong map?)";
            std::cerr << " -> " << base.path() << "\n";
            ++written;
        }

        std::cerr << "wrote " << written << " frame(s) to " << out_path << "\n";
        return written > 0 ? 0 : fail("no frames written");
    } catch (const std::exception& e) {
        return fail(e.what());
    }
}
