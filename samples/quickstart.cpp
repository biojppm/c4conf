#include <c4/conf/conf.hpp>
#include <vector>
#include <iostream>

// these are the default settings
const char default_settings[] = R"(
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely
)";

// these are the command line options we are interested in:
constexpr const c4::conf::OptSpec specs[] = {
    {"-n", "--node", "dst.node='{new: values}'", c4::conf::Opt::set_node},
    {"-f", "--file", "[dst.node=]filename.yml", c4::conf::Opt::load_file},
    {"-d", "--dir", "[dst.node=]directory_containing_yml_files", c4::conf::Opt::load_dir},
};


struct Config
{
    c4::yml::Tree tree; // this is the configuration tree
    Config(int *argc, char ***argv) : tree()
    {
        // create the defaults tree
        c4::yml::parse("(defaults)", default_settings, &tree);
        // parse the input options (argc, argv), filtering out cfg
        // args and gathering them into the output
        std::vector<c4::conf::OptArg> cfg_args;
        parse_opts(argc, argv, specs, C4_COUNTOF(specs), &cfg_args);
        // now apply the selected input options onto the defaults tree
        c4::conf::Workspace workspace(&tree);
        workspace.apply_opts(cfg_args);
    }
};


constexpr const char sep[] = "=====================================\n";

void showargs(const char *what, int argc, const char *const *argv)
{
    std::cout << sep << what << " ARGS:\n";
    for(int i = 0; i < argc; ++i)
        std::cout << "arg[" << i << "]=" << argv[i] << "\n";
    std::cout << sep;
}

int main(int argc, char **argv)
{
    showargs("ORIGINAL", argc, argv);
    Config cfg(&argc, &argv);
    showargs("REMAINING", argc, argv);
    std::cout << sep << "RESULTING TREE:\n" << cfg.tree << sep;
}
