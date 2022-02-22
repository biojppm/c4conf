# c4conf
[![MIT Licensed](https://img.shields.io/badge/License-MIT-green.svg)](https://github.com/biojppm/c4conf/blob/master/LICENSE.txt)
[![release](https://img.shields.io/github/v/release/biojppm/c4conf?color=g&include_prereleases&label=release%20&sort=semver)](https://github.com/biojppm/c4conf/releases)

[![test](https://github.com/biojppm/c4conf/workflows/test/badge.svg?branch=master)](https://github.com/biojppm/c4conf/actions)
[![Coveralls](https://coveralls.io/repos/github/biojppm/c4conf/badge.svg?branch=master)](https://coveralls.io/github/biojppm/c4conf)
[![Codecov](https://codecov.io/gh/biojppm/c4conf/branch/master/graph/badge.svg?branch=master)](https://codecov.io/gh/biojppm/c4conf)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/biojppm/c4conf.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/biojppm/c4conf/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/biojppm/c4conf.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/biojppm/c4conf/context:cpp)

c4conf is a C++ library offering use of a YAML tree as a program's configuration, using [rapidyaml](https://github.com/biojppm/rapidyaml). It leverages YAML's rich data grammar to simplify loading and successively overriding configurations:
  - from explicit YAML configurations (eg, `mapnode.seqnode[1]=[seq,of,values]`)
  - from YAML files, optionally targetting a nested node
  - from directories (ie, walk through YAML files in the directory), also with optional target node.

c4conf can be used with regular function calls from your code, or through fully customizable command line arguments, provided with parsing facilities.

After c4conf finishes with the configuration tree, you can visit the tree and read its values using any of the deserialization mechanisms available in [rapidyaml](https://github.com/biojppm/rapidyaml). See the [rapidyaml documentation here](https://rapidyaml.docsforge.com/), in particular the [rapidyaml quickstart](https://rapidyaml.docsforge.com/master/getting-started/#quick-start).

c4conf follows the same design principles of [rapidyaml](https://github.com/biojppm/rapidyaml) (low latency, low number of allocations, full control of allocations and error behaviors, no dependency on the STL). It is written in C++11, and is extensively tested in the same compilers/platforms where [rapidyaml](https://github.com/biojppm/rapidyaml) is tested.

### Quickstart
(See the complete [quickstart code here](samples/quickstart.cpp), should be enough to get going). This example has a default config tree, which is changed based on the command line arguments and then simply printed to stdout:
```c++
#include <c4/conf/conf.hpp>
#include <iostream>

using namespace c4::conf;

// these are settings used to fill the default config tree of this example:
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
void setfoo(Tree &tree, csubstr);
void setbar(Tree &tree, csubstr);
void setfoo3(Tree &tree, csubstr foo3val);
void setbar2(Tree &tree, csubstr bar2val);
void show_help(const char *exename);

// create the specs for the command line options to be handled by
// c4conf. These options will be used to transform the config tree:
constexpr const ConfigActionSpec conf_specs[] = {
    spec_for<ConfigAction::set_node >("-cn" , "--conf-node"  ), // override a node (scalars, seqs or vals)
    spec_for<ConfigAction::load_file>("-cf" , "--conf-file"  ), // override from a file, optionally into a nested node
    spec_for<ConfigAction::load_dir >("-cd" , "--conf-dir"   ), // override from all files in a directory, optionally into a nested node
    spec_for(&setfoo ,                "-sf" , "--set-foo"     , {}           , "call setfoo()"),
    spec_for(&setbar ,                "-sb" , "--set-bar"     , {}           , "call setbar()"),
    spec_for(&setfoo3,                "-sf3", "--set-foo3-val", "<foo3val>"  , "call setfoo3() with a required arg)"),
    spec_for(&setbar2,                "-sb2", "--set-bar2-val", "[<bar2val>]", "call setbar2() with an optional arg)"),
};

// Load settings, and override them with any command-line arguments.
// The arguments registered above are filtered out of the input, and
// any other arguments will remain.
c4::yml::Tree makeconf(int *argc, char ***argv)
{
    // This is our config tree; fill it with the defaults.
    c4::yml::Tree tree = c4::yml::parse("(defaults)", default_settings);
    // Parse the input args, filtering out the config options
    // registered above, and gathering them into the returned
    // container. Any options not listed in conf_specs are ignored and
    // will remain in (argc, argv). Note that this overload creating a
    // vector of ParsedOpt is chosen for brevity; you can use a
    // lower-level overload writing into a memory span.
    auto configs = parse_opts<std::vector<ParsedOpt>>(argc, argv, conf_specs, std::size(conf_specs));
    // After successfully parsing, you should also do validation, but
    // we skip that step in this sample.
    //
    // Now apply the config options onto the defaults tree.  All
    // options are handled in the order given by the user.
    c4::conf::Workspace workspace(&tree);
    workspace.apply_opts(configs);
    return tree;
}

int main(int argc, char *argv[])
{
    // This is our resulting config tree:
    c4::yml::Tree cfg = makeconf(&argc, &argv);
    // That's it. All your settings are now loaded into cfg.
    // Any c4conf arguments are filtered out of argc
    // and argv. If there are further arguments to be parsed by the
    // application, this is the occasion to do it. In this example, we
    // only have --help, and raise an error on any other argument. To
    // be clear, --help could be handled by parse_opts(), but we
    // choose to deal with it here instead, to show that c4conf does
    // not take over the options of your program.
    for(const char *arg = argv + 1; arg < argv + argc; ++arg)
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
}
```
c4conf also provides facilities to print formatted help for the arguments registered to it:
```c++
void show_help(const char *exename)
{
    auto dump = [](csubstr s){ std::cout << s; };
    print_help(dump, conf_specs, std::size(conf_specs), "conf options");
}
```

### Usage examples

Now here are some examples with this executable:

```console
# getting help
$ quickstart --help
------------
conf options
------------
  -cn [<targetpath>=]<validyaml>, --conf-node [<targetpath>=]<validyaml>
                      Load explicit YAML code into a target config node.
                      <targetpath> is optional, and defaults to the root
                      level; ie, when <targetpath> is omitted, then the
                      YAML tree resulting from parsing <validyaml> is merged
                      starting at the config tree's root node. Otherwise
                      the tree from <validyaml> is merged starting at the
                      config tree's node at <targetpath>. 
  -cf [<targetpath>=]<filename>, --conf-file [<targetpath>=]<filename>
                      Load a YAML file and merge into a target config node.
                      <targetpath> is optional, and defaults to the root
                      level; ie, when <targetpath> is omitted, then the
                      YAML tree resulting from parsing <validyaml> is merged
                      starting at the config tree's root node. Otherwise
                      the tree from <validyaml> is merged starting at the
                      config tree's node at <targetpath>. 
  -cd [<targetpath>=]<directory>, --conf-dir [<targetpath>=]<directory>
                      Consecutively load all files in a directory as YAML
                      into a target config node. All files are visited
                      even if their extension is neither of .yml or .yaml.
                      Files are visited in alphabetical order. <targetpath>
                      is optional, and defaults to the root level; ie,
                      when <targetpath> is omitted, then the YAML tree
                      resulting from parsing <validyaml> is merged starting
                      at the config tree's root node. Otherwise the tree
                      from <validyaml> is merged starting at the config
                      tree's node at <targetpath>. 
  -sf, --set-foo      call setfoo() 
  -sb, --set-bar      call setbar() 
  -sf3 <foo3val>, --set-foo3-val <foo3val>
                      call setfoo3() with a required arg 
  -sb2 [<bar2val>], --set-bar2-val [<bar2val>]
                      call setbar2() with an optional arg


# run with defaults, do not change anything
$ quickstart
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


# change seq node values:
$ quickstart -cn foo[1]=1.234e9 
foo:
  - foo0
  - 1.234e9
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# change map node values:
$ quickstart -cn bar.bar2=newvalue 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: newvalue
baz: definitely


# change values repeatedly. Later values prevail:
$ quickstart -cn foo[1]=1.234e9 -cn foo[1]=2.468e9 
foo:
  - foo0
  - 2.468e9
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# append elements to a seq:
$ quickstart -cn foo=[these,will,be,appended] 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
  - these
  - will
  - be
  - appended
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# append elements to a map:
$ quickstart -cn bar='{these: will, be: appended}' 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
  these: will
  be: appended
baz: definitely


# remove all elements in a seq, replace with different elements:
$ quickstart -cn 'foo=~' -cn foo=[all,new] 
foo:
  - all
  - new
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# remove all elements in a map, replace with different elements:
$ quickstart -cn 'bar=~' -cn bar='{all: new}' 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  all: new
baz: definitely


# replace a seq with a different type, eg val:
$ quickstart -cn foo=newfoo 
foo: newfoo
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# replace a map with a different type, eg val:
$ quickstart -cn bar=newbar 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar: newbar
baz: definitely


# add new nodes, eg seq:
$ quickstart -cn coffee=[morning,lunch,afternoon] 
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
coffee:
  - morning
  - lunch
  - afternoon


# add new nodes, eg map:
$ quickstart -cn wine='{green: summer, red: winter, champagne: year-round}' 
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
wine:
  green: summer
  red: winter
  champagne: 'year-round'


# add new nested nodes to a seq:
$ quickstart -cn foo[3]=[a,b,c] 
foo:
  - foo0
  - foo1
  - foo2
  - - a
    - b
    - c
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# add new nested nodes to a map:
$ quickstart -cn bar.possibly=[d,e,f] 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
  possibly:
    - d
    - e
    - f
baz: definitely


# In seqs, target node indices do not need to be contiguous.
# This will add a new seq nested in foo, and
# foo[4] will also be created with a null:
$ quickstart -cn foo[5]=[d,e,f] 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
  - 
  - - d
    - e
    - f
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# NOTE:
# All of -cn/-cf/-cd have an implied target node, which
# defaults to the tree's root node. So take care if
# omitting the target node; that will replace the whole
# root node with the given value:
$ quickstart -cn eraseall 
eraseall


# call setfoo():
$ quickstart -sf 
foo:
  - foo0
  - foo1
  - 'foo2, actually footoo'
  - - foo3elm0
    - foo3elm1
    - foo3elm2
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# call setfoo3(). The following arg is mandatory:
$ quickstart -sf3 123 
foo:
  - foo0
  - foo1
  - foo2
  - 123
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely

# this is equivalent to the previous example:
$ quickstart -cn foo[3]=123 
foo:
  - foo0
  - foo1
  - foo2
  - 123
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# call setbar2():
$ quickstart -sb2 'the value' 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: the value
baz: definitely

# this is equivalent to the previous example:
$ quickstart -cn 'bar.bar2=the value' 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: the value
baz: definitely


# call setbar2(), with omitted arg:
$ quickstart -sb2 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: 
baz: definitely

# this is equivalent to the previous example:
$ quickstart -cn 'bar.bar2=~' 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: ~
baz: definitely
```

Notice above that tilde `~` (which in YAML is understood as the null value) is escaped with surrounding quotes when it is part of an argument. This is needed to prevent the shell from replacing `~` with the user's home dir before the executable is called.


# License

Permissively licensed with the [MIT License](./LICENSE.txt).
