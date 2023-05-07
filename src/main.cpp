#include <fstream>
#include <filesystem>

#include "PodGenerator.hpp"

namespace fs = std::filesystem;

/// Resolve relative path removing .. directory parts
fs::path resolvePath(fs::path from, const fs::path& path) {
  for (auto& f : path) {
    if (f == "..") {
      from = from.parent_path();
    } else {
      from /= f;
    }
  }
  return from;
}

int do_main(const fs::path &outputFolder,
            const fs::path &capnFolder,
            const fs::path &tmplFolder,
            const std::vector<fs::path> &inputs) {


  auto kjfs = kj::newDiskFilesystem();
//  std::cerr << kjfs->getCurrent().listNames() << std::endl;

  //
  // Setup search folders: capnp then podgen
  //
  kj::Path kjp{};
  auto kjCapnPath = kjp.evalNative(capnFolder.string());
  auto kjCapnDir = kjfs->getRoot().openSubdir(kjCapnPath);

  auto kjTmplPath = kjp.evalNative(tmplFolder.string());
  auto kjTmplDir = kjfs->getRoot().openSubdir(kjTmplPath);

  const kj::ReadableDirectory *kjDirs[2] = {kjCapnDir.get(), kjTmplDir.get()};
  kj::ArrayPtr<const kj::ReadableDirectory *> kjImportDirs(kjDirs, 2);

  // Iterate through input files.  We assume their locations are relative from the current working directory as we
  // are invoked from cmake in the current CMakeFiles.txt directory.  Note that due to the kj filesystem's paranoia
  // this means that only files in the current directory and below are ok.
  // Todo: Relax the parent folder constraint by calculating a common parent for each input and imports??
  //
  for (auto &input: inputs) {
    // split into name and parent folder, convert to
    auto inputFolder   = input.parent_path();
    auto inputFilename = input.filename();

    if (!fs::exists(input)) {
      std::cout << "capnp file not found: " << input << std::endl;
      continue;
    }

    if (inputFilename.extension() != ".capnp") {
      std::cout << "not a capnp file: " << input << std::endl;
      continue;
    }

    std::cout << "parsing " << input << std::endl;

    //
    // Setup outputs.
    //
    auto podConvertSource = outputFolder / inputFilename;
    podConvertSource.replace_extension(".convert.cpp");

    auto podHeader = outputFolder / capnpToPodImport(inputFilename);
    auto podInclude = podHeader.filename();

    auto podConvertHeader = outputFolder / capnpToConvertImport(inputFilename);
    auto podConvertInclude = podConvertHeader.filename();

    //
    // Parse the input file itself.
    // Note: It is critical this parse as well as any imports below run from the same base directory, otherwise we will
    // get "Duplicate ID" errors.
    //
    SchemaInfo info;
    auto putType = [&info](PodGenStream &, ::capnp::StructSchema schema, ::capnp::Schema parent) {
      info.internalTypesByName.emplace(schema.getProto().getDisplayName(), schema);
      info.internalTypesById.emplace(schema.getProto().getId(), schema);
      info.schemaParentOf.emplace(schema.getProto().getId(), parent.getProto().getId());
    };

    ::capnp::SchemaParser parser;
    info.schema = parser.parseFromDirectory(kjfs->getCurrent(),
                                            kj::Path::parse(input.string()),
                                            kjImportDirs);
    auto namesp = getNamespace(info.schema);
    if (!namesp.empty()) {
      std::cout << "  found namespace " << namesp << std::endl;
    }
    info.importNamespaces.emplace(inputFilename.string(), namesp);

    PodGenStream dummy(std::cout, "", {});
    generateFromSchema(dummy, info.schema, putType);
    auto externalTypes = findExternalTypes(info.schema);
    info.externalTypes = std::move(externalTypes.typeMap);
    info.podHeaders = std::move(externalTypes.podHeaders);
    info.unions = findUnionFields(info.schema);

    for (auto& [alias, import] : getImportsFromCapnp(input.string())) {
      fs::path p = input.parent_path();

      if (import.rfind("/capnp/", 0) == 0) {
        continue;
      } else if (import[0] == '/') {
//        p = import.substr(1);
        continue;
      } else {
        p = resolvePath(p, import);
      }

      try {
        std::cout << "  parsing import " << p << std::endl;
        auto importSchema = parser.parseFromDirectory(kjfs->getCurrent(),
                                                      kj::Path::parse(p.string()),
                                                      kjImportDirs);
        auto ns = getNamespace(importSchema);
        std::cout << "  parsed import " << import;
        if (!ns.empty()) {
          std::cout << " with namespace " << ns;
        }
        std::cout << std::endl;

        auto countTypes = generateFromSchema(dummy, importSchema, putType);
        if (countTypes > 0) {
          info.importNamespaces.emplace(p.string(), ns);

          // stick the import's using alias in the map with the same namespace
          if (!alias.empty()) {
            info.importAliases.emplace(alias, p.string());
          }
        }

        externalTypes = findExternalTypes(importSchema);
        info.externalTypes.merge(externalTypes.typeMap);
        info.unions.merge(findUnionFields(importSchema));
      } catch (kj::Exception& e) {
        std::cout << "ignoring import exception: " << e.getDescription().cStr() << std::endl;
      }
    }

    auto generate = [&](const fs::path &tmpl, const fs::path &dest) {
      std::ifstream in(tmpl);
      if (!in.good()) {
        std::cout << "error loading template " << tmpl << ": " << strerror(errno) << std::endl;
        return false;
      }

      fs::create_directories(dest.parent_path());
      std::ofstream out(dest);
      if (!out.good()) {
        std::cout << "error writing to " << dest << ": " << strerror(errno) << std::endl;
        return false;
      }

      std::cout << "  generating file " << dest << std::endl;
      generateFile(in, out, info, inputFilename);
      return true;
    };

    // pod header
    bool result = generate(tmplFolder / "pod.hpp.tmpl", podHeader)
               && generate(tmplFolder / "pod_convert.hpp.tmpl", podConvertHeader)
               && generate(tmplFolder / "pod_convert.cpp.tmpl", podConvertSource);

    if (!result) {
      return 1;
    }
  }

  return 0;
}

fs::path check_path(std::string s)
{
  auto fsp = fs::canonical(s);
  if (!fs::exists(fsp) || !fs::is_directory(fsp)) {
    std::cerr << "folder not valid: " << fsp << std::endl;
  }
  return fsp;
}

int main(int argc, char **argv)
{
  if (argc <= 1) {
    std::cerr << "Usage: podgen [schemas...] -t template_dir -o output_dir -c capnp_root_dir" << std::endl;
    return 1;
  }

  // KJ Filesystem stuff is fscking painful.  Use std::filesystem to translate all input folders to absolute
  // canonical paths, we expect input files to be relative paths from cwd().
  fs::path outputFolder, capnFolder, tmplFolder;
  std::vector<fs::path> inputs;

  // protectively flip ''\' to '/'
  for (int i = 1; i < argc; i++) {
    if (i < argc - 1 && strcmp(argv[i], "-o") == 0) {
      outputFolder = check_path(argv[++i]);
    }
    else if (i < argc - 1 && strcmp(argv[i], "-c") == 0) {
      capnFolder = check_path(argv[++i]);
    }
    else if (i < argc - 1 && strcmp(argv[i], "-t") == 0) {
      tmplFolder = check_path(argv[++i]);
    }
    else {
      inputs.emplace_back(argv[i]);
    }
  }

  //  std::cerr << "out   " << outputFolder << std::endl;
  //  std::cerr << "capn  " << capnFolder << std::endl;
  //  std::cerr << "tmpl  " << tmplFolder << std::endl;
  //  for (auto &i : inputs)
  //    std::cerr << "input " << i << std::endl;

  try {
    return do_main(outputFolder, capnFolder, tmplFolder, inputs);
  }
  catch (kj::Exception & e) {
    std::cout << "exception: " << e.getDescription().cStr() << std::endl;
  }
  catch (std::exception & e) {
    std::cout << "exception: " << e.what() << std::endl;
  }

  return -1;
}