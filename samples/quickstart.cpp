#include <c4/conf/conf.hpp>
#include <vector>
#include <iostream>

// this will be used to create the default config tree
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

// these are command line options for changing config through raw YAML, handled by c4conf:
constexpr const c4::conf::OptSpec conf_specs[] = {
    {"-n", "--node", "[<targetpath>=]<validyaml>",
                     "Load explicit YAML into a target node. "
                     "<targetpath> is optional, and defaults to the root level.",
                     c4::conf::Opt::set_node},
    {"-f", "--file", "[<targetpath>=]<filename>",
                     "Load a YAML file and merge into a target node. "
                     "<targetpath> is optional, and defaults to the root level",
                     c4::conf::Opt::load_file},
    {"-d", "--dir", "[<targetpath>=]<dirname>",
                     "Consecutively load all files in a directory as YAML. "
                     "All files are visited even if the "
                     "Files are visited in alphabetical order. "
                     "<targetpath> is optional, and defaults to the root level.",
                     c4::conf::Opt::load_dir},
};
// these are command line options to change the config through a custom action:
enum { help = 0, set_foo, set_bar };
constexpr const c4::conf::OptSpec app_specs[] = {
    {"-h", "--help", "", "show help", {}},
    {"-sf", "--set-foo", "", "set several values inside foo", {}},
    {"-sb", "--set-bar", "", "set several values inside bar, and set baz as well", {}},
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
        parse_opts(argc, argv, conf_specs, C4_COUNTOF(conf_specs), &cfg_args);
        // now apply the selected input options onto the defaults tree
        c4::conf::Workspace workspace(&tree);
        workspace.apply_opts(cfg_args);
    }
};

void show_help()
{
    c4::conf::print_help(std::cout, conf_specs, C4_COUNTOF(conf_specs), "direct options");
    std::cout << "\n";
    c4::conf::print_help(std::cout, app_specs, C4_COUNTOF(app_specs), "conf options");
    std::cout << "\n";
}

void setfoo(c4::yml::Tree &tree)
{
    // a prior naked YAML option may have changed foo to a different type.
    // so ensure that it has the same struct as the defaults
    if(!tree["foo"].is_seq() || tree["foo"].num_children() < 3)
        throw std::runtime_error("cannot setfoo()");
    tree["foo"][2] = "foo2, actually footoo";
    tree["foo"][3].change_type(c4::yml::SEQ);
    tree["foo"][3][0] = "foo3elm0";
    tree["foo"][3][1] = "foo3elm1";
    tree["foo"][3][2] = "foo3elm2";
}

void setbar(c4::yml::Tree &tree)
{
    if(!tree["bar"].is_map() || tree["bar"].num_children() < 3)
        throw std::runtime_error("cannot setbar()");
    tree["bar"]["add more"] = "one";
    tree["bar"]["add more again"] = "two";
    tree["baz"] = "definitely maybe";
}


int main(int argc, char **argv)
{
    Config cfg(&argc, &argv);
    for(auto *arg = argv + 1; arg < argv + argc; ++arg)
    {
        if(app_specs[help].matches(*arg))
        {
            show_help();
            return 0;
        }
        else if(app_specs[set_foo].matches(*arg))
        {
            setfoo(cfg.tree);
        }
        else if(app_specs[set_bar].matches(*arg))
        {
            setbar(cfg.tree);
        }
        else
        {
            std::cout << "unknown argument: " << *arg << "\n";
            return arg - argv;
        }
    }
    // now you can deserialize the config tree
    // into your program's native data structures.
    // Since this is an example, we stop here and just
    // dump the tree to the output:
    std::cout << cfg.tree;
}
