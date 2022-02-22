#include <c4/conf/conf.hpp>
#include <vector>
#include <string>
#include <iostream>

C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Wuseless-cast")
C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4702) // unreachable code

using namespace c4::conf;

// these are the settings used to fill the default config tree
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

// some custom actions for command line switches
void show_help(const char *exename);
void setfoo(Tree &tree, csubstr);
void setbar(Tree &tree, csubstr);
void setfoo3(Tree &tree, csubstr foo3val);
void setbar2(Tree &tree, csubstr bar2val);

// create the specs for the command line options to be handled by
// c4conf. These options will transform the config tree:
constexpr const ConfigActionSpec conf_specs[] = {
    // using an explicit csubstr() is required by GCC5, but not with
    // later versions, which will pick the proper csubstr constructor.
    spec_for<ConfigAction::set_node> (csubstr("-cn" ), csubstr("--conf-node"   )),
    spec_for<ConfigAction::load_file>(csubstr("-cf" ), csubstr("--conf-file"   )),
    spec_for<ConfigAction::load_dir> (csubstr("-cd" ), csubstr("--conf-dir"    )),
    spec_for(&setfoo ,                csubstr("-sf" ), csubstr("--set-foo"     ), csubstr({}           ), csubstr("call setfoo()")),
    spec_for(&setbar ,                csubstr("-sb" ), csubstr("--set-bar"     ), csubstr({}           ), csubstr("call setbar()")),
    spec_for(&setfoo3,                csubstr("-sf3"), csubstr("--set-foo3-val"), csubstr("<foo3val>"  ), csubstr("call setfoo3() with a required arg)")),
    spec_for(&setbar2,                csubstr("-sb2"), csubstr("--set-bar2-val"), csubstr("[<bar2val>]"), csubstr("call setbar2() with an optional arg)")),
};

// Load settings, and override them with any command-line arguments.
// The arguments registered above are filtered out of the input, and
// any other arguments will remain.
c4::yml::Tree makeconf(int *argc, char ***argv)
{
    // This is our config tree; fill it with the defaults.
    c4::yml::Tree tree = c4::yml::parse_in_arena("(defaults)", default_settings);
    // Parse the input args, filtering out the config options
    // registered above, and gathering them into the returned
    // container. Any options not listed in conf_specs are ignored and
    // will remain in (argc, argv). Note that this overload creating a
    // vector of ParsedOpt is chosen for brevity; you can use a
    // lower-level overload writing into a memory span.
    auto configs = parse_opts<std::vector<ParsedOpt>>(argc, argv, conf_specs, C4_COUNTOF(conf_specs));
    // After successfully parsing, you should also do validation, but
    // we skip that step in this sample.
    //
    // Now apply the config options onto the defaults tree.  All
    // options are handled in the order given by the user.
    c4::conf::Workspace workspace(&tree);
    workspace.apply_opts(configs);
    return tree;
}


int main(int argc, char **argv)
{
    // This is our resulting config tree:
    c4::yml::Tree tree = makeconf(&argc, &argv);
    // At this point, any c4conf arguments are filtered out of argc
    // and argv. If there are further arguments to be parsed by the
    // application, this is the occasion to do it. In this example, we
    // only have --help, and raise an error on any other argument. To
    // be clear, --help could be handled by parse_opts(), but we
    // choose to deal with it here instead to show that c4conf does
    // not take over the options of your program.
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
            return (int)(arg - argv);
        }
    }
    // Now you can deserialize the config tree
    // into your program's native data structures.
    // Since this is an example, we stop here and just
    // dump the tree to the output:
    std::cout << tree;
    return 0;
}


//-----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Because of the nature of this example, a prior config option given
// by the user may have changed foo/bar to a different type
// incompatible with the actions below. So these ensure that it has
// the proper structure:
void ensure_foo(Tree &tree)
{
    if(!tree["foo"].is_seq() || tree["foo"].num_children() < 4)
        C4_ERROR("cannot setfoo()");
}
void ensure_bar(Tree &tree)
{
    if(!tree["bar"].is_map() || tree["bar"].num_children() < 3)
        C4_ERROR("cannot setbar()");
}

void setfoo(Tree &tree, csubstr)
{
    ensure_foo(tree);
    tree["foo"][2] = "foo2, actually footoo";
    tree["foo"][3].change_type(c4::yml::SEQ);
    tree["foo"][3][0] = "foo3elm0";
    tree["foo"][3][1] = "foo3elm1";
    tree["foo"][3][2] = "foo3elm2";
}

void setbar(Tree &tree, csubstr)
{
    ensure_bar(tree);
    tree["bar"]["add more"] = "one";
    tree["bar"]["add more again"] = "two";
    tree["baz"] = "definitely maybe";
}

void setfoo3(Tree &tree, csubstr foo3val)
{
    ensure_foo(tree);
    tree["foo"][3] = foo3val;
}

void setbar2(Tree &tree, csubstr bar2val)
{
    ensure_bar(tree);
    tree["bar"]["bar2"] = bar2val;
}


void show_help(const char *exename)
{
    // c4conf enables you to show formatted help for the c4conf options
    auto dump = [](csubstr s){ std::cout << s; };
    print_help(dump, conf_specs, C4_COUNTOF(conf_specs), "conf options");
    // Now we show several usage examples together
    // with the resulting output:
    constexpr const char fg_white[] = "\033[97m";
    constexpr const char fg_dark_gray[] = "\033[90m";
    constexpr const char fg_reset[] = "\033[0m";
    constexpr const char st_bold[] = "\033[1m";
    constexpr const char st_bold_reset[] = "\033[21m";
    constexpr const char bg_reset[] = "\033[49m";
    constexpr const char bg_black[] = "\033[40m";
    std::vector<char*> argvbuf;
    csubstr basename = csubstr{exename, strlen(exename)}.basename();
    auto show_example = [&](const char *desc, std::vector<std::string> args) -> std::ostream& {
        // create argc/argv
        argvbuf.resize(args.size());
        for(size_t i = 0; i < args.size(); ++i) argvbuf[i] = &args[i][0];
        int argc = (int) args.size();
        char **argv = argvbuf.data();
        // print the description
        std::cout << bg_black << fg_dark_gray << desc << fg_reset << bg_reset << ":\n";
        // print the command
        std::cout << bg_black << st_bold << fg_white << "$ " << basename << ' ';
        for(auto &a : args) std::cout << a << ' ';
        std::cout << fg_reset << st_bold_reset << bg_reset << '\n';
        // print the result
        std::cout << makeconf(&argc, &argv);
        return std::cout;
    };
    std::cout << "\n";
    std::cout << R"(
--------
EXAMPLES
--------

Given these default settings:
)" << default_settings << R"(
... you can do any of the following:

)"; show_example("# run with defaults, do not change anything",
                 {}) << R"(

)"; show_example("# change seq node values",
                 {"-cn", "foo[1]=1.234e9"}) << R"(

)"; show_example("# change map node values",
                 {"-cn", "bar.bar2=newvalue"}) << R"(

)"; show_example("# change values repeatedly. Later values prevail",
                 {"-cn", "foo[1]=1.234e9", "-cn", "foo[1]=2.468e9"}) << R"(

)"; show_example("# append elements to a seq",
                 {"-cn", "foo=[these,will,be,appended]"}) << R"(

)"; show_example("# append elements to a map",
                 {"-cn", "bar='{these: will, be: appended}'"}) << R"(

)"; show_example("# remove all elements in a seq, replace with different elements",
                 {"-cn", "'foo=~'", "-cn", "foo=[all,new]"}) << R"(

)"; show_example("# remove all elements in a map, replace with different elements",
                 {"-cn", "'bar=~'", "-cn", "bar='{all: new}'"}) << R"(

)"; show_example("# replace a seq with a different type, eg val",
                 {"-cn", "foo=newfoo"}) << R"(

)"; show_example("# replace a map with a different type, eg val",
                 {"-cn", "bar=newbar"}) << R"(

)"; show_example("# add new nodes, eg seq",
                 {"-cn", "coffee=[morning,lunch,afternoon]"}) << R"(

)"; show_example("# add new nodes, eg map",
                 {"-cn", "wine='{green: summer, red: winter, champagne: year-round}'"}) << R"(

)"; show_example("# add new nested nodes to a seq",
                 {"-cn", "foo[3]=[a,b,c]"}) << R"(

)"; show_example("# add new nested nodes to a map",
                 {"-cn", "bar.possibly=[d,e,f]"}) << R"(

)"; show_example("# In seqs, target node indices do not need to be contiguous.\n"
                 "# This will add a new seq nested in foo, and\n"
                 "# foo[4] will also be created with a null",
                 {"-cn", "foo[5]=[d,e,f]"}) << R"(

)"; show_example("# NOTE:\n"
                 "# All of -cn/-cf/-cd have an implied target node, which\n"
                 "# defaults to the tree's root node. So take care if\n"
                 "# omitting the target node; that will replace the whole\n"
                 "# root node with the given value",
                 {"-cn", "eraseall"}) << R"(

)"; show_example("# call setfoo()",
                 {"-sf"}) << R"(

)"; show_example("# call setfoo3(). The following arg is mandatory",
                 {"-sf3", "123"}) << R"(
)"; show_example("# this is equivalent to the previous example",
                 {"-cn", "foo[3]=123"}) << R"(

)"; show_example("# call setbar2()",
                 {"-sb2", "'the value'"}) << R"(
)"; show_example("# this is equivalent to the previous example",
                 {"-cn", "'bar.bar2=the value'"}) << R"(

)"; show_example("# call setbar2(), with omitted arg",
                 {"-sb2"}) << R"(
)"; show_example("# this is equivalent to the previous example",
                 {"-cn", "'bar.bar2=~'"}) << R"(

# Notice above that the tilde `~` (which in YAML is understood as the null
# value) is escaped with quotes when it is part of an argument. This is
# needed to prevent the shell from replacing `~` with the user's home
# dir before the executable is called.

)";
}

C4_SUPPRESS_WARNING_GCC_POP
C4_SUPPRESS_WARNING_MSVC_POP
