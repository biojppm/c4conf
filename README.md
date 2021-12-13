# c4conf

c4conf is a C++ library enabling use of a YAML tree as a program's configuration, using [rapidyaml](https://github.com/biojppm/rapidyaml). It leverages YAML's rich data grammer to simplify loading and successively overriding configurations:
  - from explicit YAML configurations (eg, `root.mapnode.seqnode[1]=[seq,of,values]`)
  - from YAML files, with optional target node, defaulting to the root node
  - from directories (ie, walk through YAML files in the directory), also with optional target node
This can be done both by regular function calls from your code, or through a thin abstraction layer on top of these offering full customizable command line argument parsing with convenient entry points.

c4conf follows the same design principles of [rapidyaml](https://github.com/biojppm/rapidyaml) (low latency, full control of allocations and error behaviors, no dependency on the STL), and is extensively tested in the same compilers/platforms where [rapidyaml](https://github.com/biojppm/rapidyaml) is tested.


### Sample code
A read through [the sample code](samples/quickstart.cpp) should be enough to get going. This executable has a default config tree, which is changed based on the command line arguments and then simply printed to stdout. Calling it with `--help`:

```console
$ c4conf-quickstart --help
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
```

### Usage examples

And now, some examples with this executable. Given these default settings:

```yaml
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
```

... you can do any of the following:

```console
# change seq node values:
$ exe -cn foo[1]=1.234e9 
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
$ exe -cn bar.bar2=newvalue 
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
$ exe -cn foo[1]=1.234e9 -cn foo[1]=2.468e9 
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
$ exe -cn foo=[these,will,be,appended] 
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
$ exe -cn bar='{these: will, be: appended}' 
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
$ exe -cn 'foo=~' -cn foo=[all,new] 
foo:
  - all
  - new
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# remove all elements in a map, replace with different elements:
$ exe -cn 'bar=~' -cn bar='{all: new}' 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  all: new
baz: definitely


# replace a seq with a different type, eg val:
$ exe -cn foo=newfoo 
foo: newfoo
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely


# replace a map with a different type, eg val:
$ exe -cn bar=newbar 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar: newbar
baz: definitely


# add new nodes, eg seq:
$ exe -cn coffee=[morning,lunch,afternoon] 
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
$ exe -cn wine='{green: summer, red: winter, champagne: year-round}' 
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
$ exe -cn foo[3]=[a,b,c] 
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
$ exe -cn bar.possibly=[d,e,f] 
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
$ exe -cn foo[5]=[d,e,f] 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
  - ~
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
$ exe -cn eraseall 
eraseall


# call setfoo():
$ exe -sf 
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
$ exe -sf3 123 
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
$ exe -cn foo[3]=123 
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
$ exe -sb2 'the value' 
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: "'the value'"
baz: definitely

# this is equivalent to the previous example:
$ exe -cn 'bar.bar2=the value' 
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
$ exe -sb2 
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

# this is equivalent to the previous example:
$ exe -cn 'bar.bar2=~' 
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

Notice above that tilde `~` (which in YAML is understood as the null value) is escaped with quotes when it is part of an argument. This is needed to prevent the shell from replacing `~` with the user's home dir before the executable is called.


# License

Permissively licensed with the [MIT License](./LICENSE.txt).
