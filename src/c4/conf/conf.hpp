#ifndef C4_CONF_HPP_
#define C4_CONF_HPP_

#include <c4/yml/yml.hpp>

namespace c4 {
namespace conf {


enum class Opt {
    set_node,
    load_file,
    load_dir,
};

struct OptSpec
{
    csubstr optshort;
    csubstr optlong;
    csubstr help;
    size_t  num_expected_args;
    Opt     action;
};

struct OptArg
{
    Opt     action;
    csubstr target;
    csubstr payload;
};


enum : size_t { argerror = ((size_t)-1) };

/** returns the number of arguments for @p opt_args or argerror in case
 * of error while parsing the arguments. */
size_t parse_opts(int *argc, char ***argv,
                  OptSpec const* specs, size_t num_specs,
                  OptArg *opt_args, size_t num_opt_args);


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct Workspace
{
    yml::Tree   m_wsbuf; //!< workspace buffer
    yml::Tree * m_ws;    //!< workspace (working tree)
    yml::Tree * m_output;
    bool        m_load_started;
    csubstr     m_arena_when_load_started;

    /** when @p workspace is null, default to the tree from this
     * workspace */
    Workspace(yml::Tree *output, yml::Tree *workspace=nullptr);

    void prepare_add_dir(const char *dirname);
    void prepare_add_dir(csubstr tree_path, const char *filename);
    void prepare_add_file(const char *filename);
    void prepare_add_file(csubstr tree_path, const char *filename);
    void prepare_add_conf(csubstr tree_path_eq_conf_yml);
    void prepare_add_conf(csubstr tree_path, csubstr conf_yml);

    void add_dir(const char *dirname);
    void add_dir(csubstr tree_path, const char *dirname);
    void add_file(const char *filename);
    void add_file(csubstr tree_path, const char *filename);
    void add_conf(csubstr tree_path_eq_conf_yml);
    void add_conf(csubstr tree_path, csubstr conf_yml);

    void apply_opts(OptArg const* args, size_t num_args);

private:

    void _load_started();
    substr _alloc_arena(size_t sz) const;

    void _parse_yml(csubstr filename, substr yml);
    void _parse_yml(csubstr filename, csubstr yml);
    template<class CharType> void _add_conf(csubstr filename, csubstr dst_path, basic_substring<CharType> yml);
};


} // namespace conf
} // namespace c4

#endif // C4_CONF_HPP_
