#ifndef C4_CONF_HPP_
#define C4_CONF_HPP_

#include <c4/yml/yml.hpp>
#include <c4/fs/fs.hpp>
#include <c4/format.hpp>

namespace c4 {
namespace conf {

/** When loading multiple files, an object of this class may be
 * provided as a workspace to allow reuse. */
struct Workspace
{
    yml::Tree   m_tree_ws;
    yml::Tree * m_tree;

    Workspace(yml::Tree *t=nullptr)
        : m_tree_ws()
        , m_tree(t ? t : &m_tree_ws)
    {
    }

    bool load_file(csubstr s, yml::Tree *output)
    {
        size_t pos = output->arena_pos();
        // filesystem functions use libc functions which cannot
        // deal with substrings so create a cstr
        output->copy_to_arena(s);
        output->copy_to_arena("\0");
        const char* file = output->arena().str + pos;
        if( ! fs::path_exists(file)) return false;
        // copy the file contents into the tree arena
        size_t filesz = fs::file_get_contents(file, nullptr, 0);
        substr file_contents = output->alloc_arena(filesz);
        file = output->arena().str + pos; // WATCHOUT the arena may have been reallocated
        fs::file_get_contents(file, file_contents.str, file_contents.len);
        // now parse the yaml content into the tree
        m_tree->clear();
        yml::parse(s, file_contents, m_tree);
        return true;
    }

    template <class It>
    void prepare(It begin, It end, yml::Tree *output) const
    {
        // we have no guarantees that the filenames
        // are zero-terminated, as (regrettably) required 
        // by filesystem functions.
        size_t fns = 0; // max filename size
        for(It it = begin; it != end; ++it)
        {
            csubstr filename = to_csubstr(*it);
            fns = filename.len > fns ? filename.len : fns;
        }
        output->reserve_arena(fns + 1);
        substr arena = output->m_arena;
        C4_ASSERT(output->m_arena.len >= fns + 1);

        size_t count = 0;
        for( ; begin != end; ++begin)
        {
            csubstr filename = to_csubstr(*begin);
            count += filename.len + 1;
            c4::cat(arena, filename, '\0');
            count += fs::file_size(arena.str);
        }
        output->reserve_arena(count);
    }
};


//-----------------------------------------------------------------------------

template <class It>
inline bool load_range(It begin, It end, yml::Tree *tree, Workspace *ws=nullptr)
{
    if(ws)
    {
        ws->prepare(begin, end, tree);
        bool ret = false;
        for( ; begin != end; ++begin)
        {
            ret |= load(c4::to_csubstr(*begin), tree, ws);
        }
        return ret;
    }
    else
    {
        Workspace tmp;
        tmp.prepare(begin, end, tree);
        bool ret = false;
        for( ; begin != end; ++begin)
        {
            ret |= load(c4::to_csubstr(*begin), tree, &tmp);
        }
        return ret;
    }
}

template <class Container>
inline bool load(Container const& cont, yml::Tree *tree, Workspace *ws=nullptr)
{
    return load_range(cont.begin(), cont.end(), tree, ws);
}


//-----------------------------------------------------------------------------

inline bool load(csubstr path, yml::Tree *tree, Workspace *ws)
{
    C4_ASSERT(ws != nullptr);
    if( ! ws->load_file(path, tree)) return false;
    size_t root = tree->root_id();
    tree->merge_with(ws->m_tree, ws->m_tree->root_id(), root);
    return true;
}


//-----------------------------------------------------------------------------

inline bool load(csubstr path, yml::Tree *tree)
{
    Workspace ws;
    return load(path, tree, &ws);
}


//-----------------------------------------------------------------------------

inline yml::Tree load(csubstr path, Workspace *ws)
{
    yml::Tree t;
    load(path, &t, ws);
    return t;
}


//-----------------------------------------------------------------------------

inline yml::Tree load(csubstr path)
{
    yml::Tree t;
    Workspace ws;
    load(path, &t, &ws);
    return t;
}


} // namespace conf
} // namespace c4

#endif // C4_CONF_HPP_
