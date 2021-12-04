# c4conf

Load and override YAML configurations:
  - from explicit YAML configurations (eg, `root.mapnode.seqnode[1]=newvalue`)
  - from YAML files, with optional target node, defaulting to the root node
  - from directories (ie, walk through YAML files in the directory), also with optional target node

See [the sample code](samples/quickstart.cpp).

# License

Permissively licensed with the [MIT License](./LICENSE.txt).
