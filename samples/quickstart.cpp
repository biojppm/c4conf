#include <c4/conf/conf.hpp>
#include <vector>
#include <iostream>

using namespace c4::conf;

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

// foo action
void setfoo(Tree &tree, csubstr)
{
    // a prior config option may have changed foo to a different type.
    // so ensure that it has the same struct as the defaults
    if(!tree["foo"].is_seq() || tree["foo"].num_children() < 4)
        C4_ERROR("cannot setfoo()");
    tree["foo"][2] = "foo2, actually footoo";
    tree["foo"][3].change_type(c4::yml::SEQ);
    tree["foo"][3][0] = "foo3elm0";
    tree["foo"][3][1] = "foo3elm1";
    tree["foo"][3][2] = "foo3elm2";
}

// bar action
void setbar(Tree &tree, csubstr)
{
    // a prior config option may have changed bar to a different type.
    // so ensure that it has the same struct as the defaults
    if(!tree["bar"].is_map() || tree["bar"].num_children() < 3)
        C4_ERROR("cannot setbar()");
    tree["bar"]["add more"] = "one";
    tree["bar"]["add more again"] = "two";
    tree["baz"] = "definitely maybe";
}

// foo3 action
void setfoo3(Tree &tree, csubstr foo3val)
{
    // a prior config option may have changed foo to a different type.
    // so ensure that it has the same struct as the defaults
    if(!tree["foo"].is_seq() || tree["foo"].num_children() < 4)
        C4_ERROR("cannot setfoo3()");
    tree["foo"][3] = foo3val;
}

// bar2 action
void setbar2(Tree &tree, csubstr bar2val)
{
    // a prior config option may have changed foo to a different type.
    // so ensure that it has the same struct as the defaults
    if(!tree["bar"].is_map() || tree["bar"].num_children() < 3)
        C4_ERROR("cannot setbar2()");
    tree["bar"]["bar2"] = bar2val; // ensure the value is copied to the tree's arena
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// Create the specs for the command line options to be handled by c4conf
constexpr const ConfigActionSpec conf_specs[] = {
    {ConfigAction::callback, &setfoo , "-sf" , "--set-foo"     , {}           , "set foo"     },
    {ConfigAction::callback, &setbar , "-sb" , "--set-bar"     , {}           , "set bar"     },
    {ConfigAction::callback, &setfoo3, "-sf3", "--set-foo3-val", "<foo3val>"  , "set foo3 val"},
    {ConfigAction::callback, &setbar2, "-sb2", "--set-bar2-val", "[<bar2val>]", "set bar2 val"},
    spec_for<ConfigAction::set_node> ("-cn", "--conf-node"),
    spec_for<ConfigAction::load_file>("-cf", "--conf-file"),
    spec_for<ConfigAction::load_dir> ("-cd", "--conf-dir"),
};

// Our configuration tree
struct Config
{
    Tree tree; // this is the configuration tree
    Config(int *argc, char ***argv) : tree()
    {
        // create the defaults tree
        parse("(defaults)", default_settings, &tree);
        // parse the input options (argc, argv), filtering out
        // config options, and gathering them into this temporary.
        // Any options not listed in conf_specs are ignored.
        std::vector<ParsedOpt> cfg_args;
        if(!parse_opts(argc, argv, conf_specs, C4_COUNTOF(conf_specs), &cfg_args))
            C4_ERROR("error parsing arguments");
        // now apply the config options onto the defaults tree
        Workspace workspace(&tree);
        workspace.apply_opts(cfg_args);
    }
};

void show_help(const char *exename)
{
    auto dump = [](csubstr s){ std::cout << s; };
    print_help(dump, conf_specs, C4_COUNTOF(conf_specs), "conf options");
    std::cout << "\n";
    std::cout << R"(
--------
EXAMPLES
--------
  exe -n foo=[these,will,be,appended]
  exe -n foo=~ -n foo=[these,will,override]

)";
}

int main(int argc, char **argv)
{
    Config cfg(&argc, &argv);
    // at this point, any c4::conf arguments are
    // filtered out of argc and argv
    for(auto *arg = argv + 1; arg < argv + argc; ++arg)
    {
        csubstr s{*arg, strlen(*arg)};
        if(s == "-h" || s == "--help")
        {
            show_help(argv[0]);
            return 0;
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
