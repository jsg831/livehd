// This file is distributed under the BSD 3-Clause License. See LICENSE for details.
#include "pass_compiler.hpp"

static Pass_plugin sample("pass_compiler", Pass_compiler::setup);


void Pass_compiler::setup() {
  Eprp_method m1("pass.compiler", "lnast to lgraph compilation", &Pass_compiler::compile);
  m1.add_label_optional("path",  "lgraph path", "lgdb");
  m1.add_label_optional("files", "files to process (comma separated)");
  m1.add_label_optional("odir",  "output directory", ".");
  m1.add_label_optional("gviz",  "dump graphviz");

  register_pass(m1);
}

Pass_compiler::Pass_compiler(const Eprp_var &var) : Pass("pass.compiler", var) {}


bool Pass_compiler::check_option_gviz(Eprp_var &var) { 
  bool gviz_en; 
  if (var.has_label("gviz")) { 
    auto gv = var.get("gviz"); 
    gviz_en   = gv != "false" && gv != "0"; 
  } else { 
    gviz_en = false; 
  } 
  return gviz_en; 
} 


void Pass_compiler::compile(Eprp_var &var) {
  Pass_compiler pc(var);
  auto path = pc.get_path(var);
  auto odir = pc.get_odir(var);
  bool gviz = pc.check_option_gviz(var);


  Lcompiler comp(path, odir, gviz);

  if (var.lnasts.empty()) {
    auto files = pc.get_files(var);
    if (files.empty()) {
      Pass::warn("nothing to compile. no files or lnast");
      return;
    }

    for (auto f : absl::StrSplit(files, ',')) {
      Pass::warn("todo: start from prp parser");
      /* comp.add(f); */
    }
  } else {
    for (const auto &lnast : var.lnasts) {
      comp.add(lnast);
    }
  }

  auto lgs = comp.wait_all();
  var.add(lgs);
}


