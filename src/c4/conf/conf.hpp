#ifndef C4_CONF_HPP_
#define C4_CONF_HPP_

#include <c4/yml/yml.hpp>
#include <c4/fs/fs.hpp>
#include <type_traits>

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

template<class OptArgContainer>
bool parse_opts(int *argc, char ***argv,
                OptSpec const* specs, size_t num_specs,
                OptArgContainer *opt_args)
{
    static_assert(std::is_same<typename OptArgContainer::value_type, OptArg>::value, "must be container of OptArg");
    size_t ret = parse_opts(argc, argv, specs, num_specs, opt_args->data(), opt_args->size());
    if(ret == argerror)
        return false;
    if(ret > opt_args->size())
    {
        opt_args->resize(ret);
        ret = parse_opts(argc, argv, specs, num_specs, opt_args->data(), opt_args->size());
        C4_CHECK(ret == opt_args->size());
    }
    return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** The main structure to create the configuration. */
struct Workspace
{
    /** @p output the output directory
     * @p workspace @p workspace is null, default to the tree from this
     * workspace */
    Workspace(yml::Tree *output, yml::Tree *workspace=nullptr);
    ~Workspace();

    void apply_opts(OptArg const* args, size_t num_args);

    template<class OptArgContainer>
    void apply_opts(OptArgContainer const& opt_args)
    {
        static_assert(std::is_same<typename OptArgContainer::value_type, OptArg>::value, "must be container of OptArg");
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


} // namespace conf
} // namespace c4

#endif // C4_CONF_HPP_
