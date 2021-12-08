#ifndef C4_CONF_HPP_
#define C4_CONF_HPP_

#include "c4/language.hpp"
#include <c4/yml/yml.hpp>
#include <c4/fs/fs.hpp>
#include <type_traits>

namespace c4 {
namespace conf {

using substr = c4::substr;
using csubstr = c4::csubstr;
using Tree = c4::yml::Tree;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct ParsedOpt;

/** The main structure to create the configuration. */
struct Workspace
{
    /** @p output the output directory
     * @p workspace @p workspace is null, default to the tree from this
     * workspace */
    Workspace(yml::Tree *output, yml::Tree *workspace=nullptr);
    ~Workspace();

    void apply_opts(ParsedOpt const* args, size_t num_args);

    template<class OptArgContainer>
    void apply_opts(OptArgContainer const& opt_args)
    {
        static_assert(std::is_same<typename OptArgContainer::value_type, ParsedOpt>::value, "must be container of OptArg");
        apply_opts(opt_args.data(), opt_args.size());
    }

    // all the prepare methods need to be called before its
    // corresponding add method

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

public:

    yml::Tree   m_wsbuf; //!< workspace buffer
    yml::Tree * m_ws;    //!< workspace (working tree)
    yml::Tree * m_output;
    bool        m_load_started;
    csubstr     m_arena_when_load_started;
    // these are only needed for directories:
    c4::fs::maybe_buf<char> m_dir_scratch = {};
    c4::fs::EntryList       m_dir_entry_list = {};

private:

    void _load_started();
    substr _alloc_arena(size_t sz) const;
    void _reserve_arena(size_t sz) const;

    void _parse_yml(csubstr filename, substr yml);
    void _parse_yml(csubstr filename, csubstr yml);
    template<class CharType> void _add_conf(csubstr filename, csubstr dst_path, basic_substring<CharType> yml);

    template<class T>
    void _ensure(c4::fs::maybe_buf<T> *mb)
    {
        if(!mb->valid())
        {
            _release(mb);
            mb->buf = (T*) m_output->m_alloc.allocate(sizeof(T) * mb->required_size, mb->buf);
            mb->size = mb->required_size;
        }
    }
    template<class T>
    void _release(c4::fs::maybe_buf<T> *mb)
    {
        if(mb->buf)
            m_output->m_alloc.free(mb->buf, sizeof(T) * mb->size);
        mb->buf = nullptr;
        mb->size = 0;
    }

    static void _remdoc(yml::Tree *t);
    static void _askeyx(yml::Tree *t, csubstr key);
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @name command line option specification */
/** @{ */

/** Actions for configuration through command line options. */
enum class ConfigAction
{
    /** Load explicit YAML into a target config node.
     * <targetpath> is optional, and defaults to the root level;
     * ie, when <targetpath> is omitted, then the YAML tree
     * resulting from parsing <validyaml> is merged starting at
     * the config tree's root node.
     * Otherwise the tree from <validyaml> is merged starting at
     * the config tree's node at <targetpath>. */
    set_node,
    /** Load a YAML file and merge into a target config node.
     * <targetpath> is optional, and defaults to the root level;
     * ie, when <targetpath> is omitted, then the YAML tree
     * resulting from parsing <validyaml> is merged starting at
     * the config tree's root node.
     * Otherwise the tree from <validyaml> is merged starting at
     * the config tree's node at <targetpath>. */
    load_file,
    /** Consecutively load all files in a directory as YAML into a target config node.
     * All files are visited even if their extension is neither of .yml or .yaml.
     * Files are visited in alphabetical order.
     * <targetpath> is optional, and defaults to the root level;
     * ie, when <targetpath> is omitted, then the YAML tree
     * resulting from parsing <validyaml> is merged starting at
     * the config tree's root node.
     * Otherwise the tree from <validyaml> is merged starting at
     * the config tree's node at <targetpath>. */
    load_dir,
    /** Perform a custom action. */
    callback,
};

/** The function pointer type to be used with the callback action */
using pfn_callback = void (*)(Tree&, csubstr arg);

/** An action specification */
struct ConfigActionSpec
{
    ConfigAction action;
    //! the function to use for callback actions. Can be null otherwise.
    pfn_callback callback;
    //! the short-form command line option, eg "-c" or "-cn"
    csubstr optshort;
    //! the long-form command line option, eg "--conf-node"
    csubstr optlong;
    /** a representation of the argument to be accepted by the option.
     * Leave empty to signify that no further argument is expected.
     * Enclose the whole string in square brackets to signify that a
     * further argument is optional. */
    csubstr dummyname;
    //! describe what the command line option does
    csubstr help;

    bool matches(const char *arg) const { return matches(to_csubstr(arg)); }
    bool matches(csubstr a) const { return a == optshort || a == optlong; }
    bool expects_arg() const { return !dummyname.empty(); }
    bool accepts_optional_arg() const { return expects_arg() && dummyname.begins_with('[') && dummyname.ends_with(']'); };
};


template<ConfigAction action> constexpr ConfigActionSpec spec_for(csubstr optshort, csubstr optlong={});
/** A helper to create the set_node action specification */
template<> inline constexpr ConfigActionSpec spec_for<ConfigAction::set_node>(csubstr optshort, csubstr optlong)
{
    return {
        ConfigAction::set_node,
        {},
        optshort,
        optlong,
        // argument
        "[<targetpath>=]<validyaml>",
        // help
        "Load explicit YAML code into a target config node. "
        "<targetpath> is optional, and defaults to the root level; "
        "ie, when <targetpath> is omitted, then the YAML tree "
        "resulting from parsing <validyaml> is merged starting at "
        "the config tree's root node. "
        "Otherwise the tree from <validyaml> is merged starting at "
        "the config tree's node at <targetpath>.",
    };
}
/** A helper to create the load_file action specification */
template<> inline constexpr ConfigActionSpec spec_for<ConfigAction::load_file>(csubstr optshort, csubstr optlong)
{
    return {
        ConfigAction::load_file,
        {},
        optshort,
        optlong,
        // argument
        "[<targetpath>=]<filename>",
        // help
        "Load a YAML file and merge into a target config node. "
        "<targetpath> is optional, and defaults to the root level; "
        "ie, when <targetpath> is omitted, then the YAML tree "
        "resulting from parsing <validyaml> is merged starting at "
        "the config tree's root node. "
        "Otherwise the tree from <validyaml> is merged starting at "
        "the config tree's node at <targetpath>.",
    };
}
/** A helper to create the load_dir action specification */
template<> inline constexpr ConfigActionSpec spec_for<ConfigAction::load_dir>(csubstr optshort, csubstr optlong)
{
    return {
        ConfigAction::load_dir,
        {},
        optshort,
        optlong,
        // argument
        "[<targetpath>=]<directory>",
        // help
        "Consecutively load all files in a directory as YAML into a target config node. "
        "All files are visited even if their extension is neither of .yml or .yaml. "
        "Files are visited in alphabetical order. "
        "<targetpath> is optional, and defaults to the root level; "
        "ie, when <targetpath> is omitted, then the YAML tree "
        "resulting from parsing <validyaml> is merged starting at "
        "the config tree's root node. "
        "Otherwise the tree from <validyaml> is merged starting at "
        "the config tree's node at <targetpath>.",
    };
}

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @name command line option parsing */
/** @{ */

/** A configure option parsed from command line arguments */
struct ParsedOpt
{
    ConfigAction action;   //!< the specified action
    csubstr target;        //!< the path to the target node
    csubstr payload;       //!< the argument
    pfn_callback callback; //!< the function for the callback action
};

enum : size_t {
    /** a value signifying that there was an error parsing the command line options. */
    argerror = ((size_t)-1)
};


/** Parse command line options into the given buffer. This will
 * extract any configuration arguments matching any of the
 * @p num_specs specifications given in @p specs. The
 * configuration arguments are written into @p parsed_opts, up to
 * the size passed in @p parsed_opts_size. All other remaining
 * arguments kept in @p argc. @p argc and @p argv are adjusted to
 * refer only to these remaining arguments.
 *
 * @return If there was an error while parsing the command line
 * options, return argerror. Otherwise, return the size of the buffer
 * needed to accomodate all the command line options. */
size_t parse_opts(int *argc, char ***argv,
                  ConfigActionSpec const* specs, size_t num_specs,
                  ParsedOpt *parsed_opts, size_t parsed_opts_size);


/** Parse command line options into the given container. Works as (1),
 * except it receives an output container, which will be resized as
 * needed to accomodate the passed arguments.
 *
 * @return false if there was an error parsing the arguments */
template<class ParsedOptContainer>
bool parse_opts(int *argc, char ***argv,
                ConfigActionSpec const* specs, size_t num_specs,
                ParsedOptContainer *parsed_opts)
{
    static_assert(std::is_same<typename ParsedOptContainer::value_type, ParsedOpt>::value, "must be container of ParsedOpt");
    size_t ret = parse_opts(argc, argv, specs, num_specs, parsed_opts->data(), parsed_opts->size());
    if(ret == argerror)
        return false;
    if(ret > parsed_opts->size())
    {
        parsed_opts->resize(ret);
        ret = parse_opts(argc, argv, specs, num_specs, parsed_opts->data(), parsed_opts->size());
        C4_CHECK(ret == parsed_opts->size());
    }
    return true;
}

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @name Facilities for printing help */
/** @{ */

/** print help for configuration command line options.
 * @p dump is a function accepting a csubstr as its single argument, which should print it. */
template<class DumpFn>
C4_NO_INLINE void print_help(DumpFn &&dump,
                             ConfigActionSpec const *specs, size_t num_specs,
                             csubstr section_title={}, size_t linewidth=70)
{
    auto print = [&dump](csubstr value, size_t width=0) {
        dump(value);
        if(!width)
            width = value.len;
        for(size_t i = value.len; i < width; ++i)
            dump(" ");
        return width;
    };
    auto printdummy = [&dump](csubstr dummyname){
        size_t len = 0;
        if(!dummyname.empty())
        {
            dump(" ");
            dump(dummyname);
            len = 1 + dummyname.len;
        }
        return len;
    };
    if(section_title.not_empty())
    {
        for(size_t i = 0; i < section_title.len; ++i)
            dump("-");
        dump("\n");
        dump(section_title);
        dump("\n");
        for(size_t i = 0; i < section_title.len; ++i)
            dump("-");
        dump("\n");
    }
    constexpr const size_t break_pos = 22;
    for(ConfigActionSpec const* spec = specs; spec < specs + num_specs; ++spec)
    {
        size_t pos = 0;
        if(!spec->optshort.empty())
        {
            pos += print("  ");
            pos += print(spec->optshort);
            pos += printdummy(spec->dummyname);
        }
        if(!spec->optlong.empty())
        {
            if(!spec->optshort.empty())
                pos += print(", ");
            else
                pos += print("  ");
            pos += print(spec->optlong);
            pos += printdummy(spec->dummyname);
        }
        if(pos < break_pos - 2)
        {
            pos += print(" ", break_pos - pos);
        }
        else
        {
            print("\n");
            pos = print(" ", break_pos);
        }
        for(csubstr word : spec->help.split(' '))
        {
            pos += print(word);
            if(pos < linewidth)
            {
                pos += print(" ");
            }
            else
            {
                print("\n");
                pos = print(" ", break_pos);
            }
        }
        pos += print("\n");
    }
}

/** @} */

} // namespace conf
} // namespace c4

#endif // C4_CONF_HPP_
