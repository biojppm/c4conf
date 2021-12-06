#ifndef C4_CONF_HPP_
#define C4_CONF_HPP_

#include <c4/yml/yml.hpp>
#include <c4/fs/fs.hpp>
#include <type_traits>

namespace c4 {
namespace conf {

struct OptArg;

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


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

enum class Opt {
    none,
    set_node,
    load_file,
    load_dir,
};

struct OptSpec
{
    csubstr optshort;
    csubstr optlong;
    csubstr dummyname;
    csubstr help;
    Opt     action;
    bool matches(const char *arg) const { return matches(to_csubstr(arg)); }
    bool matches(csubstr a) const { return a == optshort || a == optlong; }
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

template<class OStream>
void print_help(OStream &os,
                OptSpec const *specs, size_t num_specs,
                csubstr section_title={}, size_t linewidth=70)
{
    auto print = [&os](csubstr value, size_t width=0) {
        os.write(value.str, value.len);
        if(!width) width = value.len;
        for(size_t i = value.len; i < width; ++i) os << ' ';
        return width;
    };
    auto printdummy = [&os](csubstr dummyname){
        size_t len = 0;
        if(!dummyname.empty())
        {
            os << ' ';
            os.write(dummyname.str, dummyname.len);
            len = 1 + dummyname.len;
        }
        return len;
    };
    if(section_title.not_empty())
    {
        for(size_t i = 0; i < section_title.len; ++i)
            os << '-';
        os << '\n';
        os << section_title << '\n';
        for(size_t i = 0; i < section_title.len; ++i)
            os << '-';
        os << '\n';
    }
    constexpr const size_t break_pos = 22;
    for(OptSpec const* spec = specs; spec < specs + num_specs; ++spec)
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

} // namespace conf
} // namespace c4

#endif // C4_CONF_HPP_
