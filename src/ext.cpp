///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2019, Nefelus Inc
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
#include <errno.h>

#include "ext.h"
#include "logger.h"
#include "tm.hpp"

namespace OpenRCX {

extern "C" {
extern int Openrcx_Init(Tcl_Interp *interp);
}

template <>
const char* odb::ZTechModule<Ext>::_module = nullptr;

Ext::Ext() : odb::ZTechModule<Ext>(nullptr, nullptr)
{
  _ext = new extMain(5);
  _tree = NULL;
}

Ext::~Ext()
{
  delete _ext;
}

void Ext::init(Tcl_Interp* tcl_interp, odb::dbDatabase* db)
{
  _db = db;
  _ext->setDB(db);

  // Define swig TCL commands.
  Openrcx_Init(tcl_interp);
  // Eval encoded sta TCL sources.
  //sta::evalTclInit(tcl_interp, sta::openrcx_tcl_inits);
}

bool Ext::load_model(const std::string& name,
                     bool               lef_rc,
                     const std::string& file,
                     int                setMin,
                     int                setTyp,
                     int                setMax)
{
  if (lef_rc) {
    if (!_ext->checkLayerResistance())
      return TCL_ERROR;
    _ext->addExtModel();  // fprintf(stdout, "Using LEF RC values to
                          // extract!\n");
    notice(0, "Using LEF RC values to extract!\n");
  } else if (!file.empty()) {
    _ext->readExtRules(name.c_str(), file.c_str(), setMin, setTyp, setMax);
    int numOfNet, numOfRSeg, numOfCapNode, numOfCCSeg;
    _ext->getBlock()->getExtCount(
        numOfNet, numOfRSeg, numOfCapNode, numOfCCSeg);

    _ext->setupMapping(3 * numOfNet);
  } else {
    // fprintf(stdout, "\nHave to specify options:\n\t-lef_rc to read resistance
    // and capacitance values from LEF or \n\t-file to read high accuracy RC
    // models\n");
    notice(0,
           "\nHave to specify options:\n\t-lef_rc to read resistance and "
           "capacitance values from LEF or \n\t-file to read high accuracy RC "
           "models\n");
  }

  return 0;
}

bool Ext::read_process(const std::string& name, const std::string& file)
{
  _ext->readProcess(name.c_str(), file.c_str());

  return TCL_OK;
}

bool Ext::rules_gen(const std::string& name,
                    const std::string& dir,
                    const std::string& file,
                    bool               write_to_solver,
                    bool               read_from_solver,
                    bool               run_solver,
                    int                pattern,
                    bool               keep_file)
{
  _ext->rulesGen(name.c_str(),
                 dir.c_str(),
                 file.c_str(),
                 pattern,
                 write_to_solver,
                 read_from_solver,
                 run_solver,
                 keep_file);

  return TCL_OK;
}

bool Ext::metal_rules_gen(const std::string& name,
                          const std::string& dir,
                          const std::string& file,
                          bool               write_to_solver,
                          bool               read_from_solver,
                          bool               run_solver,
                          int                pattern,
                          bool               keep_file,
                          int                metal)
{
  _ext->metRulesGen(name.c_str(),
                    dir.c_str(),
                    file.c_str(),
                    pattern,
                    write_to_solver,
                    read_from_solver,
                    run_solver,
                    keep_file,
                    metal);
  return TCL_OK;
}

bool Ext::write_rules(const std::string& name,
                      const std::string& dir,
                      const std::string& file,
                      int                pattern,
                      bool               read_from_solver)
{
  _ext->writeRules(
      name.c_str(), dir.c_str(), file.c_str(), pattern, read_from_solver);
  return TCL_OK;
}

bool Ext::get_ext_metal_count(int& metal_count)
{
  extRCModel* m = _ext->getRCModel();
  metal_count   = m->getLayerCnt();
  return TCL_OK;
}

bool Ext::bench_net(const std::string& dir,
                    int                net,
                    bool               write_to_solver,
                    bool               read_from_solver,
                    bool               run_solver,
                    int                max_track_count)
{
  extMainOptions opt;

  opt._write_to_solver  = write_to_solver;
  opt._read_from_solver = read_from_solver;
  opt._run_solver       = run_solver;

  int netId    = net;
  int trackCnt = max_track_count;
  opt._topDir  = dir.c_str();

  if (netId == 0) {
    notice(0, "Net (=%d), should be a positive number\n", netId);
    return TCL_OK;
  }
  notice(0, "Benchmarking using 3d field solver net %d...\n", netId);
#ifdef ZUI
  ZPtr<ISdb> dbNetSdb = _ext->getBlock()->getNetSdb(_context, _ext->getTech());

  _ext->benchNets(&opt, netId, trackCnt, dbNetSdb);
#endif
  notice(0, "Finished.\n");

  return TCL_OK;
}

bool Ext::run_solver(const std::string& dir, int net, int shape)
{
  extMainOptions opt;
  opt._topDir  = dir.c_str();
  uint netId   = net;
  int  shapeId = shape;
  _ext->runSolver(&opt, netId, shapeId);
  return TCL_OK;
}

bool Ext::bench_wires(const std::string& block,
                      int                over_dist,
                      int                under_dist,
                      int                met,
                      int                over_met,
                      int                under_met,
                      int                len,
                      int                cnt,
                      const std::string& w,
                      const std::string& s,
                      const std::string& th,
                      const std::string& d,
                      const std::string& w_list,
                      const std::string& s_list,
                      const std::string& th_list,
                      const std::string& grid_list,
                      bool               default_lef_rules,
                      bool               nondefault_lef_rules,
                      const std::string& dir,
                      bool               Over,
                      bool               ddd,
                      bool               multiple_widths,
                      bool               write_to_solver,
                      bool               read_from_solver,
                      bool               run_solver,
                      bool               diag)
{
  extMainOptions opt;

  opt._topDir    = dir.c_str();
  opt._met       = met;
  opt._overDist  = over_dist;
  opt._underDist = under_dist;
  opt._overMet   = over_met;
  opt._over      = Over;
  opt._underMet  = under_met;
  opt._len       = 1000 * len;
  opt._wireCnt   = cnt;
  opt._name      = block.c_str();

  opt._default_lef_rules    = default_lef_rules;
  opt._nondefault_lef_rules = nondefault_lef_rules;

  opt._3dFlag          = ddd;
  opt._multiple_widths = multiple_widths;

  opt._write_to_solver  = write_to_solver;
  opt._read_from_solver = read_from_solver;
  opt._run_solver       = run_solver;
  opt._diag             = diag;

  Ath__parser parser;

  if (!th_list.empty()) {
    parser.mkWords(th_list.c_str());
    parser.getDoubleArray(&opt._thicknessTable, 0);
    opt._thListFlag = true;
  } else {
    parser.mkWords(th.c_str());
    parser.getDoubleArray(&opt._thicknessTable, 0);
    opt._thListFlag = false;
  }
  opt._listsFlag  = false;
  opt._wsListFlag = false;
  if (opt._default_lef_rules) {  // minWidth, minSpacing, minThickness, pitch
                                 // multiplied by grid_list
    // user sets default_flag and grid_list multipliers
    if (grid_list.empty()) {
      opt._gridTable.add(1.0);
    } else {
      parser.mkWords(grid_list.c_str());
      parser.getDoubleArray(&opt._gridTable, 0);
    }
    opt._listsFlag = true;
  } else if (opt._nondefault_lef_rules) {
    opt._listsFlag = true;
  } else if (!w_list.empty() && !s_list.empty()) {
    opt._listsFlag  = true;
    opt._wsListFlag = true;
    parser.mkWords(w_list.c_str());
    parser.getDoubleArray(&opt._widthTable, 0);
    parser.mkWords(s_list.c_str());
    parser.getDoubleArray(&opt._spaceTable, 0);
  } else {
    parser.mkWords(w.c_str());
    parser.getDoubleArray(&opt._widthTable, 0);
    parser.mkWords(s.c_str());
    parser.getDoubleArray(&opt._spaceTable, 0);
    parser.mkWords(th.c_str());
    parser.getDoubleArray(&opt._thicknessTable, 0);
    parser.mkWords(d.c_str());
    parser.getDoubleArray(&opt._densityTable, 0);
  }
  _ext->benchWires(&opt);

  return TCL_OK;
}

bool Ext::clean(bool all_models, bool ext_only)
{
  return TCL_OK;
}

bool Ext::define_process_corner(int ext_model_index, const std::string& name)
{
  char* cornerName = _ext->addRCCorner(name.c_str(), ext_model_index);

  if (cornerName != NULL) {
    notice(0, "Defined Extraction corner %s\n", cornerName);
    return TCL_OK;
  } else {
    return TCL_ERROR;
  }
}

bool Ext::define_derived_corner(const std::string& name,
                                const std::string& process_corner_name,
                                float              res_factor,
                                float              cc_factor,
                                float              gndc_factor)
{
  if (process_corner_name.empty()) {
    warning(0, "the original process corner name is required\n");
    return TCL_ERROR;
  }

  int model = _ext->getDbCornerModel(process_corner_name.c_str());

  char* cornerName = _ext->addRCCornerScaled(
      name.c_str(), model, res_factor, cc_factor, gndc_factor);

  if (cornerName != NULL) {
    notice(0, "Defined Derived Extraction corner %s\n", cornerName);
    return TCL_OK;
  } else {
    return TCL_ERROR;
  }
}

bool Ext::delete_corners()
{
  _ext->deleteCorners();
  return TCL_OK;
}

bool Ext::get_corners(std::list<std::string>& corner_list)
{
  _ext->getCorners(corner_list);
  return TCL_OK;
}

bool Ext::read_qcap(const std::string& file_name,
                    const std::string& cap_file,
                    bool               skip_bterms,
                    bool               no_qcap,
                    const std::string& design)
{
  if (file_name.empty()) {
    warning(0, "file option is required\n");
    return TCL_OK;
  }

  extMeasure m;
  if (!no_qcap)
    m.readQcap(_ext,
               file_name.c_str(),
               design.c_str(),
               cap_file.c_str(),
               skip_bterms,
               _db);
  else
    m.readAB(_ext,
             file_name.c_str(),
             design.c_str(),
             cap_file.c_str(),
             skip_bterms,
             _db);

  return TCL_OK;
}

bool Ext::get_ext_db_corner(int& index, const std::string& name)
{
  index = _ext->getDbCornerIndex(name.c_str());

  if (index < 0)
    warning(0, "extraction corner %s not found\n", name);

  return TCL_OK;
}

bool Ext::assembly(odb::dbBlock* block, odb::dbBlock* main_block)
{
  if (main_block == NULL) {
    notice(0,
           "Add parasitics of block %s onto nets/wires ...\n",
           block->getConstName());
  } else {
    notice(0,
           "Add parasitics of block %s onto block %s...\n",
           block->getConstName(),
           main_block->getConstName());
  }

  extMain::assemblyExt(main_block, block);

  return TCL_OK;
}

bool Ext::flatten(odb::dbBlock* block, bool spef)
{
  if (block == NULL) {
    error(0, "No block for flatten command\n");
  }
  _ext->addRCtoTop(block, spef);
  return TCL_OK;
}

bool Ext::extract(bool               min,
                  bool               max,
                  bool               typ,
                  int                set_min,
                  int                set_typ,
                  int                set_max,
                  bool               litho,
                  bool               wire_density,
                  const std::string& cmp_file,
                  const std::string& ext_model_file,
                  const std::string& net,
                  const std::string& bbox,
                  const std::string& ibox,
                  int                test,
                  int                cc_band_tracks,
                  int                signal_table,
                  int                cc_up,
                  uint               preserve_geom,
                  int                corner_cnt,
                  double             max_res,
                  bool               no_merge_via_res,
                  float              coupling_threshold,
                  int                context_depth,
                  int                cc_model,
                  bool               over_cell,
                  bool               remove_cc,
                  bool               remove_ext,
                  bool               unlink_ext,
                  bool               eco,
                  bool               no_gs,
                  bool               re_run,
                  bool               tile,
                  int                tiling,
                  bool               skip_m1_caps,
                  bool               power_grid,
                  const std::string& exclude_cells,
                  bool               skip_power_stubs,
                  const std::string& power_source_coords,
                  bool               lef_rc,
                  bool               rlog)
{
  //    fprintf(stdout, "extracting %s ...\n",
  //    _ext->getBlock()->getName().c_str());
  notice(0, "extracting %s ...\n", _ext->getBlock()->getName().c_str());

  if (lef_rc) {
    if (!_ext->checkLayerResistance())
      return TCL_ERROR;
    _ext->addExtModel();
    notice(0, "Using LEF RC values to extract!\n");
  }

  bool extract_power_grid_only = power_grid;
  bool skip                    = skip_power_stubs;
#ifdef ZUI
  if (extract_power_grid_only) {
    dbBlock* block = _ext->getBlock();
    if (block != NULL) {
      bool skipCutVias = true;
      block->initSearchBlock(_db->getTech(), true, true, _context, skipCutVias);
    } else {
      error(0, "There is no block to extract!\n");
      return TCL_ERROR;
    }
    _ext->setPowerExtOptions(skip_power_stubs,
                             exclude_cells.c_str(),
                             in_args->skip_m1_caps(),
                             in_args->power_source_coords());
  }
#endif

  const char* extRules      = ext_model_file.c_str();
  const char* cmpFile       = cmp_file.c_str();
  bool        density_model = wire_density;

  uint ccUp   = cc_up;
  uint ccFlag = cc_model;
  // uint ccFlag= 25;
  int         ccBandTracks       = cc_band_tracks;
  uint        use_signal_table   = signal_table;
  bool        merge_via_res      = no_merge_via_res ? false : true;
  uint        extdbg             = test;
  const char* nets               = net.c_str();
  bool        gs                 = no_gs ? false : true;
  double      ccThres            = coupling_threshold;
  int         ccContextDepth     = context_depth;
  bool        overCell           = over_cell;
  bool        btermThresholdFlag = tile;

  uint tilingDegree = tiling;

  odb::ZPtr<odb::ISdb> dbNetSdb = NULL;
  bool                 extSdb   = false;

  // ccBandTracks = 0;  // tttt no band CC for now
  if (extdbg == 100 || extdbg == 102)  // 101: activate reuse metal fill
                                       // 102: no bandTrack
    ccBandTracks = 0;
  if (ccBandTracks)
    eco = false;  // tbd
  else {
#ifdef ZUI
    dbNetSdb = _ext->getBlock()->getNetSdb();

    if (dbNetSdb == NULL) {
      extSdb = true;
      if (rlog)
        AthResourceLog("before get sdb", 0);

      dbNetSdb = _ext->getBlock()->getNetSdb(_context, _ext->getTech());

      if (rlog)
        AthResourceLog("after get sdb", 0);
    }
#endif
  }
  if (!_ext->checkLayerResistance())
    return TCL_ERROR;

  if (tilingDegree == 1)
    extdbg = 501;
  else if (tilingDegree == 10)
    extdbg = 603;
  else if (tilingDegree == 7)
    extdbg = 703;
  else if (tilingDegree == 77) {
    extdbg = 773;
    //_ext->rcGen(NULL, max_res, merge_via_res, 77, true, this);
  }
  /*
  else if (tilingDegree==77) {
          extdbg= 703;
          _ext->rcGen(NULL, max_res, merge_via_res, 77, true, this);
  }
  */
  else if (tilingDegree == 8)
    extdbg = 803;
  else if (tilingDegree == 9)
    extdbg = 603;
  else if (tilingDegree == 777) {
    extdbg = 777;

    uint                             cnt   = 0;
    odb::dbSet<odb::dbNet>           bnets = _ext->getBlock()->getNets();
    odb::dbSet<odb::dbNet>::iterator net_itr;
    for (net_itr = bnets.begin(); net_itr != bnets.end(); ++net_itr) {
      odb::dbNet* net = *net_itr;

      cnt += _ext->rcNetGen(net);
    }
    notice(0, "777: Final rc segments = %d\n", cnt);
    odb::dbRSeg* rc = odb::dbRSeg::getRSeg(_ext->getBlock(), 113);
  }

  if (_ext->makeBlockRCsegs(btermThresholdFlag,
                            cmpFile,
                            density_model,
                            litho,
                            nets,
                            bbox.c_str(),
                            ibox.c_str(),
                            ccUp,
                            ccFlag,
                            ccBandTracks,
                            use_signal_table,
                            max_res,
                            merge_via_res,
                            extdbg,
                            preserve_geom,
                            re_run,
                            eco,
                            gs,
                            rlog,
                            dbNetSdb,
                            ccThres,
                            ccContextDepth,
                            overCell,
                            extRules,
                            this)
      == 0)
    return TCL_ERROR;

  odb::dbBlock* topBlock = _ext->getBlock();
  if (tilingDegree == 1) {
    odb::dbSet<odb::dbBlock>           children = topBlock->getChildren();
    odb::dbSet<odb::dbBlock>::iterator itr;

    // Extraction

    notice(0, "List of extraction tile blocks: \n");
    for (itr = children.begin(); itr != children.end(); ++itr) {
      odb::dbBlock* blk = *itr;
      notice(0, "%s ", blk->getConstName());
    }
    notice(0, "\n");
  } else if (extdbg == 501) {
    odb::dbSet<odb::dbBlock>           children = topBlock->getChildren();
    odb::dbSet<odb::dbBlock>::iterator itr;

    // Extraction
    for (itr = children.begin(); itr != children.end(); ++itr) {
      odb::dbBlock* blk = *itr;
      notice(0, "Extacting block %s...\n", blk->getConstName());
      extMain* ext = new extMain(5);
      ext->setDB(_db);

      ext->setBlock(blk);

      if (ext->makeBlockRCsegs(btermThresholdFlag,
                               cmpFile,
                               density_model,
                               litho,
                               nets,
                               bbox.c_str(),
                               ibox.c_str(),
                               ccUp,
                               ccFlag,
                               ccBandTracks,
                               use_signal_table,
                               max_res,
                               merge_via_res,
                               0,
                               preserve_geom,
                               re_run,
                               eco,
                               gs,
                               rlog,
                               dbNetSdb,
                               ccThres,
                               ccContextDepth,
                               overCell,
                               extRules,
                               this)
          == 0) {
        warning(0, "Failed to Extact block %s...\n", blk->getConstName());
        return TCL_ERROR;
      }
    }

    for (itr = children.begin(); itr != children.end(); ++itr) {
      odb::dbBlock* blk = *itr;
      notice(0, "Assembly of block %s...\n", blk->getConstName());

      extMain::assemblyExt(topBlock, blk);
    }
    //_ext= new extMain(5);
    //_ext->setDB(_db);
    //_ext->setBlock(topBlock);
  }

  // report total net cap

  if (!extract_power_grid_only) {
    char netcapfile[500];
    sprintf(netcapfile, "%s.totCap", _ext->getBlock()->getConstName());
    _ext->reportTotalCap(netcapfile, true, false, 1.0, NULL, NULL);
  }

  if (!ccBandTracks) {
    if (extSdb && extdbg != 99) {
      if (rlog)
        odb::AthResourceLog("before remove sdb", 0);
      dbNetSdb->cleanSdb();
#ifdef ZUI
      _ext->getBlock()->resetNetSdb();
      if (rlog)
        AthResourceLog("after remove sdb", 0);
#endif
    }
  }

  //    fprintf(stdout, "Finished extracting %s.\n",
  //    _ext->getBlock()->getName().c_str());
  notice(0, "Finished extracting %s.\n\n", _ext->getBlock()->getName().c_str());
  return 0;
}

bool Ext::adjust_rc(float res_factor, float cc_factor, float gndc_factor)
{
  _ext->adjustRC(res_factor, cc_factor, gndc_factor);
  return 0;
}

bool Ext::init_incremental_spef(const std::string& origp,
                                const std::string& newp,
                                const std::string& reader,
                                bool               no_backslash,
                                const std::string& exclude_cells)
{
  _ext->initIncrementalSpef(origp.c_str(),
                            newp.c_str(),
                            reader.c_str(),
                            exclude_cells.c_str(),
                            no_backslash);
  return 0;
}

bool Ext::export_sdb(odb::ZPtr<odb::ISdb>& net_sdb,
                     odb::ZPtr<odb::ISdb>& cc_sdb)
{
  cc_sdb  = _ext->getCcSdb();
  net_sdb = _ext->getNetSdb();

  return 0;
}

bool Ext::attach_gui(odb::ZPtr<odb::IZdcr> dcr)
{
  // have to add it as a member of Tmg object
  using namespace odb;
  odb::ZPtr<odb::IZgui> igui;
  if (adsNewComponent(_context, ZCID(ZguiExt), igui) != Z_OK) {
    assert(0);
  }
  ZALLOCATED(igui);

  igui->setDcr(dcr);

  odb::dbBlock* block = NULL;
  odb::dbChip*  chip  = _db->getChip();
  if (chip)
    block = chip->getBlock();
  if (!block) {
    notice(0, "need db before attach_gui onto Tmg\n");
  } else {
    //_ext->setIgui(igui);
    igui->setGuiContext(_ext, block);
  }
  dcr->addGui(igui);

  notice(0, "Attached Crawler to Ext Technology Module ...\n");

  return TCL_OK;
}

bool Ext::attach(odb::ZPtr<odb::IZgui>& gui)
{
  using namespace odb;
  ZPtr<IZgui> igui;
  if (adsNewComponent(_context, ZCID(ZguiExt), igui) != Z_OK) {
    assert(0);
  }
  ZALLOCATED(igui);

  _ext->setIgui(igui);

  gui = igui;  // in case that is needed

  //	fprintf(stdout, "%s \n", _module);
  notice(0, "%s \n", _module);

  //    fprintf(stdout, "Attached Crawler to Ext Technology Module ...\n");
  notice(0, "Attached Crawler to Ext Technology Module ...\n");

  return 0;
}

bool Ext::write_spef_nets(odb::dbObject* block,
                          bool           flatten,
                          bool           parallel,
                          int            corner)
{
  _ext->write_spef_nets(flatten, parallel);

  return 0;
}

bool Ext::write_spef(const std::string& nets,
                     int                net_id,
                     const std::string& ext_corner_name,
                     int                corner,
                     int                debug,
                     bool               flatten,
                     bool               parallel,
                     bool               init,
                     bool               end,
                     bool               use_ids,
                     bool               no_name_map,
                     const std::string& N,
                     bool               term_junction_xy,
                     bool               single_pi,
                     const std::string& file,
                     bool               gz,
                     bool               stop_after_map,
                     bool               w_clock,
                     bool               w_conn,
                     bool               w_cap,
                     bool               w_cc_cap,
                     bool               w_res,
                     bool               no_c_num,
                     bool               prime_time,
                     bool               psta,
                     bool               no_backslash,
                     const std::string& exclude_cells,
                     const std::string& cap_units,
                     const std::string& res_units)
{
  if (end) {
    _ext->writeSPEF(true);
    return 0;
  }
  const char* name = ext_corner_name.c_str();

  uint netId = net_id;
  if (netId > 0) {
    _ext->writeSPEF(netId, single_pi, debug, corner, name);
    return 0;
  }
  bool useIds   = use_ids;
  bool stop     = stop_after_map;
  bool initOnly = init;
  if (!initOnly)
    initOnly = parallel && flatten;
  _ext->writeSPEF((char*) file.c_str(),
                  (char*) nets.c_str(),
                  useIds,
                  no_name_map,
                  (char*) N.c_str(),
                  term_junction_xy,
                  exclude_cells.c_str(),
                  cap_units.c_str(),
                  res_units.c_str(),
                  gz,
                  stop,
                  w_clock,
                  w_conn,
                  w_cap,
                  w_cc_cap,
                  w_res,
                  no_c_num,
                  initOnly,
                  single_pi,
                  no_backslash,
                  prime_time,
                  psta,
                  corner,
                  name,
                  flatten,
                  parallel);

  // fprintf(stdout, "Hello Extraction %s\n", "Ext::write_spef");
  return 0;
}

bool Ext::independent_spef_corner()
{
  _ext->setUniqueExttreeCorner();
  return 0;
}

bool Ext::read_spef(const std::string& file,
                    const std::string& net,
                    bool               force,
                    bool               use_ids,
                    bool               keep_loaded_corner,
                    bool               stamp_wire,
                    int                test_parsing,
                    const std::string& N,
                    bool               r_conn,
                    bool               r_cap,
                    bool               r_cc_cap,
                    bool               r_res,
                    float              cc_threshold,
                    float              cc_ground_factor,
                    int                app_print_limit,
                    int                corner,
                    const std::string& db_corner_name,
                    const std::string& calibrate_base_corner,
                    int                spef_corner,
                    bool               m_map,
                    bool               more_to_read,
                    float              length_unit,
                    int                fix_loop,
                    bool               no_cap_num_collapse,
                    const std::string& cap_node_map_file,
                    bool               log)
{
  // fprintf(stdout, "Hello Extraction %s\n", "Ext::read_spef");
  //    fprintf(stdout, "reading %s\n",in_args->file());
  notice(0, "reading %s\n", file.c_str());

  odb::ZPtr<odb::ISdb> netSdb    = NULL;
  bool                 stampWire = stamp_wire;
#ifdef ZUI
  if (stampWire)
    netSdb = _ext->getBlock()->getSignalNetSdb(_context, _db->getTech());
#endif
  bool useIds      = use_ids;
  uint testParsing = test_parsing;

  Ath__parser parser;
  char*       filename = (char*) file.c_str();
  if (!filename || !filename[0]) {
    notice(0, "Please input file name.\n");
    return 0;
  }
  parser.mkWords(filename);

  _ext->readSPEF(parser.get(0),
                 (char*) net.c_str(),
                 force,
                 useIds,
                 r_conn,
                 (char*) N.c_str(),
                 r_cap,
                 r_cc_cap,
                 r_res,
                 cc_threshold,
                 cc_ground_factor,
                 length_unit,
                 m_map,
                 no_cap_num_collapse,
                 (char*) cap_node_map_file.c_str(),
                 log,
                 corner,
                 0.0,
                 0.0,
                 NULL,
                 NULL,
                 NULL,
                 db_corner_name.c_str(),
                 calibrate_base_corner.c_str(),
                 spef_corner,
                 fix_loop,
                 keep_loaded_corner,
                 stampWire,
                 netSdb,
                 testParsing,
                 more_to_read,
                 false /*diff*/,
                 false /*calibrate*/,
                 app_print_limit);

  for (int ii = 1; ii < parser.getWordCnt(); ii++)
    _ext->readSPEFincr(parser.get(ii));

  return 0;
}

bool Ext::diff_spef(const std::string& net,
                    bool               use_ids,
                    bool               test_parsing,
                    const std::string& file,
                    const std::string& db_corner_name,
                    int                spef_corner,
                    const std::string& exclude_net_subword,
                    const std::string& net_subword,
                    const std::string& rc_stats_file,
                    bool               r_conn,
                    bool               r_cap,
                    bool               r_cc_cap,
                    bool               r_res,
                    int                ext_corner,
                    float              low_guard,
                    float              upper_guard,
                    bool               m_map,
                    bool               log)
{
  // fprintf(stdout, "Hello Extraction %s\n", "Ext::read_spef");
  //    fprintf(stdout, "diffing spef %s\n",in_args->file());
  if (file.empty()) {
    notice(0, "\nplease type in name of the spef file to diff, using -file\n");
    return 0;
  }
  notice(0, "diffing spef %s\n", file.c_str());

  bool useIds      = use_ids;
  bool testParsing = test_parsing;

  Ath__parser parser;
  parser.mkWords((char*) file.c_str());

  char* excludeSubWord = (char*) exclude_net_subword.c_str();
  char* subWord        = (char*) net_subword.c_str();
  char* statsFile      = (char*) rc_stats_file.c_str();

  _ext->readSPEF(parser.get(0),
                 (char*) net.c_str(),
                 false /*force*/,
                 useIds,
                 r_conn,
                 NULL /*N*/,
                 r_cap,
                 r_cc_cap,
                 r_res,
                 -1.0,
                 0.0 /*cc_ground_factor*/,
                 1.0 /*length_unit*/,
                 m_map,
                 false /*noCapNumCollapse*/,
                 NULL /*capNodeMapFile*/,
                 log,
                 ext_corner,
                 low_guard,
                 upper_guard,
                 excludeSubWord,
                 subWord,
                 statsFile,
                 db_corner_name.c_str(),
                 NULL,
                 spef_corner,
                 0 /*fix_loop*/,
                 false /*keepLoadedCorner*/,
                 false /*stampWire*/,
                 NULL /*netSdb*/,
                 testParsing,
                 false /*moreToRead*/,
                 true /*diff*/,
                 false /*calibrate*/,
                 0);

  // for (uint ii=1; ii<parser.getWordCnt(); ii++)
  //	_ext->readSPEFincr(parser.get(ii));

  return 0;
}

bool Ext::calibrate(const std::string& spef_file,
                    const std::string& db_corner_name,
                    int                corner,
                    int                spef_corner,
                    bool               m_map,
                    float              upper_limit,
                    float              lower_limit)
{
  if (spef_file.empty()) {
    notice(0,
           "\nplease type in name of the spef file to calibrate, using "
           "-spef_file\n");
    return 0;
  }
  notice(0, "calibrate on spef file  %s\n", spef_file.c_str());
  Ath__parser parser;
  parser.mkWords((char*) spef_file.c_str());
  _ext->calibrate(parser.get(0),
                  m_map,
                  upper_limit,
                  lower_limit,
                  db_corner_name.c_str(),
                  corner,
                  spef_corner);
  return 0;
}

bool Ext::match(const std::string& spef_file,
                const std::string& db_corner_name,
                int                corner,
                int                spef_corner,
                bool               m_map)
{
  if (spef_file.empty()) {
    notice(
        0,
        "\nplease type in name of the spef file to match, using -spef_file\n");
    return 0;
  }
  notice(0, "match on spef file  %s\n", spef_file.c_str());
  Ath__parser parser;
  parser.mkWords((char*) spef_file.c_str());
  _ext->match(
      parser.get(0), m_map, db_corner_name.c_str(), corner, spef_corner);
  return 0;
}

#if 0
TCL_METHOD ( Ext::setRules )
{
    ZIn_Ext_setRules * in_args = (ZIn_Ext_setRules *) in;
	_extModel= (ExtModel *) in_args->model();
	xm->getFringeCap(....)
}
#endif

bool Ext::set_block(const std::string& block_name,
                    odb::dbBlock*      block,
                    const std::string& inst_name,
                    odb::dbInst*       inst)
{
  if (!inst_name.empty()) {
    odb::dbChip*   chip = _db->getChip();
    odb::dbInst*   ii   = chip->getBlock()->findInst(inst_name.c_str());
    odb::dbMaster* m    = ii->getMaster();
    notice(0,
           "Inst=%s ==> %d %s of Master %d %s",
           inst_name.c_str(),
           ii->getId(),
           ii->getConstName(),
           m->getId(),
           m->getConstName());
  }
  if (block == NULL) {
    if (block_name.empty()) {
      warning(0, "commnad requires either dbblock or block name\n");
      return TCL_ERROR;
    }
    odb::dbChip*  chip  = _db->getChip();
    odb::dbBlock* child = chip->getBlock()->findChild(block_name.c_str());
    if (child == NULL) {
      warning(0, "Cannot find block with master name %s\n", block_name.c_str());
      return TCL_ERROR;
    }
    block = child;
  }

  delete _ext;
  _ext = new extMain(5);
  _ext->setDB(_db);

  _ext->setBlock(block);
  return 0;
}

bool Ext::report_total_cap(const std::string& file,
                           bool               res_only,
                           bool               cap_only,
                           float              ccmult,
                           const std::string& ref,
                           const std::string& read)
{
  _ext->reportTotalCap(
      file.c_str(), cap_only, res_only, ccmult, ref.c_str(), read.c_str());
  /*
          dbChip *chip= _db->getChip();
          dbBlock * child = chip->getBlock()->findChild(block_name);
  if (child==NULL) {
  warning(0, "Cannot find block with master name %s\n",
  block_name);
  return TCL_ERROR;
  }
  block= child;
  */
  return 0;
}

bool Ext::report_total_cc(const std::string& file,
                          const std::string& ref,
                          const std::string& read)
{
  _ext->reportTotalCc(file.c_str(), ref.c_str(), read.c_str());
  return 0;
}

bool Ext::dump(bool               open_tree_file,
               bool               close_tree_file,
               bool               cc_cap_geom,
               bool               cc_net_geom,
               bool               track_cnt,
               bool               signal,
               bool               power,
               int                layer,
               const std::string& file)
{
  // fprintf(stdout, "Hello Extraction %s\n", "Ext::read_spef");
  //    fprintf(stdout, "dumping %s\n",in_args->file());
  notice(0, "dumping %s\n", file.c_str());

  _ext->extDump((char*) file.c_str(),
                open_tree_file,
                close_tree_file,
                cc_cap_geom,
                cc_net_geom,
                track_cnt,
                signal,
                power,
                layer);

  return 0;
}

bool Ext::count(bool signal_wire_seg, bool power_wire_seg)
{
  _ext->extCount(signal_wire_seg, power_wire_seg);

  return 0;
}

bool Ext::rc_tree(float              max_cap,
                  uint               test,
                  int                net,
                  const std::string& print_tag)
{
  int   netId    = net;
  char* printTag = (char*) print_tag.c_str();

  odb::dbBlock* block = _ext->getBlock();

  if (_tree == NULL)
    _tree = new extRcTree(block);

  uint cnt;
  if (netId > 0)
    _tree->makeTree((uint) netId,
                    max_cap,
                    test,
                    true,
                    true,
                    cnt,
                    1.0 /*mcf*/,
                    printTag,
                    false /*for_buffering*/);
  else
    _tree->makeTree(max_cap, test);

  return TCL_OK;
}
bool Ext::net_stats(std::list<int>&    net_ids,
                    const std::string& tcap,
                    const std::string& ccap,
                    const std::string& ratio_cap,
                    const std::string& res,
                    const std::string& len,
                    const std::string& met_cnt,
                    const std::string& wire_cnt,
                    const std::string& via_cnt,
                    const std::string& seg_cnt,
                    const std::string& term_cnt,
                    const std::string& bterm_cnt,
                    const std::string& file,
                    const std::string& bbox,
                    const std::string& branch_len)
{
  Ath__parser parser;
  extNetStats limits;
  limits.reset();

  limits.update_double(&parser, tcap.c_str(), limits._tcap);
  limits.update_double(&parser, ccap.c_str(), limits._ccap);
  limits.update_double(&parser, ratio_cap.c_str(), limits._cc2tcap);
  limits.update_double(&parser, res.c_str(), limits._res);

  limits.update_int(&parser, len.c_str(), limits._len, 1000);
  limits.update_int(&parser, met_cnt.c_str(), limits._layerCnt);
  limits.update_int(&parser, wire_cnt.c_str(), limits._wCnt);
  limits.update_int(&parser, via_cnt.c_str(), limits._vCnt);
  limits.update_int(&parser, term_cnt.c_str(), limits._termCnt);
  limits.update_int(&parser, bterm_cnt.c_str(), limits._btermCnt);
  limits.update_bbox(&parser, bbox.c_str());

  FILE*       fp       = stdout;
  const char* filename = file.c_str();
  if (filename != NULL) {
    fp = fopen(filename, "w");
    if (fp == NULL) {
      warning(0, "Cannot open file %s\n", filename);
      return TCL_OK;
    }
  }
  bool skipDb    = false;
  bool skipRC    = false;
  bool skipPower = true;

  odb::dbBlock* block = _ext->getBlock();
  if (block == NULL) {
    warning(0, "There is no extracted block\n");
    skipRC = true;
    return TCL_OK;
  }
  std::list<int> list_of_nets;
  int            n = _ext->printNetStats(
      fp, block, &limits, skipRC, skipDb, skipPower, &list_of_nets);
  notice(0, "%d nets found\n", n);

  net_ids = list_of_nets;

  return TCL_OK;
}

}  // namespace OpenRCX